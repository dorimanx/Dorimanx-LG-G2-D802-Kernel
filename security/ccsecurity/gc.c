/*
 * security/ccsecurity/gc.c
 *
 * Copyright (C) 2005-2012  NTT DATA CORPORATION
 *
 * Version: 1.8.3+   2012/05/05
 */

#include "internal.h"

/***** SECTION1: Constants definition *****/

/* For compatibility with older kernels. */
#ifndef for_each_process
#define for_each_process for_each_task
#endif

/* The list for "struct ccs_io_buffer". */
static LIST_HEAD(ccs_io_buffer_list);
/* Lock for protecting ccs_io_buffer_list. */
static DEFINE_SPINLOCK(ccs_io_buffer_list_lock);

/***** SECTION2: Structure definition *****/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)

/*
 * Lock for syscall users.
 *
 * This lock is used for protecting single SRCU section for 2.6.18 and
 * earlier kernels because they don't have SRCU support.
 */
struct ccs_lock_struct {
	int counter_idx; /* Currently active index (0 or 1). */
	int counter[2];  /* Current users. Protected by ccs_counter_lock. */
};

#endif

/***** SECTION3: Prototype definition section *****/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
int ccs_lock(void);
#endif
void ccs_del_acl(struct list_head *element);
void ccs_del_condition(struct list_head *element);
void ccs_notify_gc(struct ccs_io_buffer *head, const bool is_register);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
void ccs_unlock(const int idx);
#endif

static bool ccs_domain_used_by_task(struct ccs_domain_info *domain);
static bool ccs_name_used_by_io_buffer(const char *string, const size_t size);
static bool ccs_struct_used_by_io_buffer(const struct list_head *element);
static int ccs_gc_thread(void *unused);
static void ccs_collect_acl(struct list_head *list);
static void ccs_collect_entry(void);
static void ccs_collect_member(const enum ccs_policy_id id,
			       struct list_head *member_list);
static void ccs_memory_free(const void *ptr, const enum ccs_policy_id type);
static void ccs_put_name_union(struct ccs_name_union *ptr);
static void ccs_put_number_union(struct ccs_number_union *ptr);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static void ccs_synchronize_counter(void);
#endif
static void ccs_try_to_gc(const enum ccs_policy_id type,
			  struct list_head *element);

/***** SECTION4: Standalone functions section *****/

/***** SECTION5: Variables definition section *****/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)

/*
 * Lock for syscall users.
 *
 * This lock is held for only protecting single SRCU section.
 */
struct srcu_struct ccs_ss;

#else

static struct ccs_lock_struct ccs_counter;
/* Lock for protecting ccs_counter. */
static DEFINE_SPINLOCK(ccs_counter_lock);

#endif

/***** SECTION6: Dependent functions section *****/

/**
 * ccs_memory_free - Free memory for elements.
 *
 * @ptr:  Pointer to allocated memory.
 * @type: One of values in "enum ccs_policy_id".
 *
 * Returns nothing.
 *
 * Caller holds ccs_policy_lock mutex.
 */
static void ccs_memory_free(const void *ptr, const enum ccs_policy_id type)
{
	/* Size of an element. */
	static const u8 e[CCS_MAX_POLICY] = {
#ifdef CONFIG_CCSECURITY_PORTRESERVE
		[CCS_ID_RESERVEDPORT] = sizeof(struct ccs_reserved),
#endif
		[CCS_ID_GROUP] = sizeof(struct ccs_group),
#ifdef CONFIG_CCSECURITY_NETWORK
		[CCS_ID_ADDRESS_GROUP] = sizeof(struct ccs_address_group),
#endif
		[CCS_ID_PATH_GROUP] = sizeof(struct ccs_path_group),
		[CCS_ID_NUMBER_GROUP] = sizeof(struct ccs_number_group),
		[CCS_ID_AGGREGATOR] = sizeof(struct ccs_aggregator),
		[CCS_ID_TRANSITION_CONTROL]
		= sizeof(struct ccs_transition_control),
		[CCS_ID_MANAGER] = sizeof(struct ccs_manager),
		/* [CCS_ID_CONDITION] = "struct ccs_condition"->size, */
		/* [CCS_ID_NAME] = "struct ccs_name"->size, */
		/* [CCS_ID_ACL] = a["struct ccs_acl_info"->type], */
		[CCS_ID_DOMAIN] = sizeof(struct ccs_domain_info),
	};
	/* Size of a domain ACL element. */
	static const u8 a[] = {
		[CCS_TYPE_PATH_ACL] = sizeof(struct ccs_path_acl),
		[CCS_TYPE_PATH2_ACL] = sizeof(struct ccs_path2_acl),
		[CCS_TYPE_PATH_NUMBER_ACL]
		= sizeof(struct ccs_path_number_acl),
		[CCS_TYPE_MKDEV_ACL] = sizeof(struct ccs_mkdev_acl),
		[CCS_TYPE_MOUNT_ACL] = sizeof(struct ccs_mount_acl),
#ifdef CONFIG_CCSECURITY_NETWORK
		[CCS_TYPE_INET_ACL] = sizeof(struct ccs_inet_acl),
		[CCS_TYPE_UNIX_ACL] = sizeof(struct ccs_unix_acl),
#endif
#ifdef CONFIG_CCSECURITY_MISC
		[CCS_TYPE_ENV_ACL] = sizeof(struct ccs_env_acl),
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
		[CCS_TYPE_CAPABILITY_ACL] = sizeof(struct ccs_capability_acl),
#endif
#ifdef CONFIG_CCSECURITY_IPC
		[CCS_TYPE_SIGNAL_ACL] = sizeof(struct ccs_signal_acl),
#endif
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
		[CCS_TYPE_AUTO_EXECUTE_HANDLER]
		= sizeof(struct ccs_handler_acl),
		[CCS_TYPE_DENIED_EXECUTE_HANDLER]
		= sizeof(struct ccs_handler_acl),
#endif
#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION
		[CCS_TYPE_AUTO_TASK_ACL] = sizeof(struct ccs_task_acl),
		[CCS_TYPE_MANUAL_TASK_ACL] = sizeof(struct ccs_task_acl),
#endif
	};
	size_t size;
	if (type == CCS_ID_ACL)
		size = a[container_of(ptr, typeof(struct ccs_acl_info),
				      list)->type];
	else if (type == CCS_ID_NAME)
		size = container_of(ptr, typeof(struct ccs_name),
				    head.list)->size;
	else if (type == CCS_ID_CONDITION)
		size = container_of(ptr, typeof(struct ccs_condition),
				    head.list)->size;
	else
		size = e[type];
	ccs_memory_used[CCS_MEMORY_POLICY] -= ccs_round2(size);
	kfree(ptr);
}

/**
 * ccs_put_name_union - Drop reference on "struct ccs_name_union".
 *
 * @ptr: Pointer to "struct ccs_name_union".
 *
 * Returns nothing.
 */
static void ccs_put_name_union(struct ccs_name_union *ptr)
{
	ccs_put_group(ptr->group);
	ccs_put_name(ptr->filename);
}

/**
 * ccs_put_number_union - Drop reference on "struct ccs_number_union".
 *
 * @ptr: Pointer to "struct ccs_number_union".
 *
 * Returns nothing.
 */
static void ccs_put_number_union(struct ccs_number_union *ptr)
{
	ccs_put_group(ptr->group);
}

/**
 * ccs_struct_used_by_io_buffer - Check whether the list element is used by /proc/ccs/ users or not.
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns true if @element is used by /proc/ccs/ users, false otherwise.
 */
static bool ccs_struct_used_by_io_buffer(const struct list_head *element)
{
	struct ccs_io_buffer *head;
	bool in_use = false;
	spin_lock(&ccs_io_buffer_list_lock);
	list_for_each_entry(head, &ccs_io_buffer_list, list) {
		head->users++;
		spin_unlock(&ccs_io_buffer_list_lock);
		mutex_lock(&head->io_sem);
		if (head->r.domain == element || head->r.group == element ||
		    head->r.acl == element || &head->w.domain->list == element)
			in_use = true;
		mutex_unlock(&head->io_sem);
		spin_lock(&ccs_io_buffer_list_lock);
		head->users--;
		if (in_use)
			break;
	}
	spin_unlock(&ccs_io_buffer_list_lock);
	return in_use;
}

/**
 * ccs_name_used_by_io_buffer - Check whether the string is used by /proc/ccs/ users or not.
 *
 * @string: String to check.
 * @size:   Memory allocated for @string .
 *
 * Returns true if @string is used by /proc/ccs/ users, false otherwise.
 */
static bool ccs_name_used_by_io_buffer(const char *string, const size_t size)
{
	struct ccs_io_buffer *head;
	bool in_use = false;
	spin_lock(&ccs_io_buffer_list_lock);
	list_for_each_entry(head, &ccs_io_buffer_list, list) {
		int i;
		head->users++;
		spin_unlock(&ccs_io_buffer_list_lock);
		mutex_lock(&head->io_sem);
		for (i = 0; i < CCS_MAX_IO_READ_QUEUE; i++) {
			const char *w = head->r.w[i];
			if (w < string || w > string + size)
				continue;
			in_use = true;
			break;
		}
		mutex_unlock(&head->io_sem);
		spin_lock(&ccs_io_buffer_list_lock);
		head->users--;
		if (in_use)
			break;
	}
	spin_unlock(&ccs_io_buffer_list_lock);
	return in_use;
}

/**
 * ccs_del_transition_control - Delete members in "struct ccs_transition_control".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static inline void ccs_del_transition_control(struct list_head *element)
{
	struct ccs_transition_control *ptr =
		container_of(element, typeof(*ptr), head.list);
	ccs_put_name(ptr->domainname);
	ccs_put_name(ptr->program);
}

/**
 * ccs_del_aggregator - Delete members in "struct ccs_aggregator".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static inline void ccs_del_aggregator(struct list_head *element)
{
	struct ccs_aggregator *ptr =
		container_of(element, typeof(*ptr), head.list);
	ccs_put_name(ptr->original_name);
	ccs_put_name(ptr->aggregated_name);
}

/**
 * ccs_del_manager - Delete members in "struct ccs_manager".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static inline void ccs_del_manager(struct list_head *element)
{
	struct ccs_manager *ptr =
		container_of(element, typeof(*ptr), head.list);
	ccs_put_name(ptr->manager);
}

/**
 * ccs_domain_used_by_task - Check whether the given pointer is referenced by a task.
 *
 * @domain: Pointer to "struct ccs_domain_info".
 *
 * Returns true if @domain is in use, false otherwise.
 */
static bool ccs_domain_used_by_task(struct ccs_domain_info *domain)
{
	bool in_use = false;
	/*
	 * Don't delete this domain if somebody is doing execve().
	 *
	 * Since ccs_finish_execve() first reverts ccs_domain_info and then
	 * updates ccs_flags, we need smp_rmb() to make sure that GC first
	 * checks ccs_flags and then checks ccs_domain_info.
	 */
#ifdef CONFIG_CCSECURITY_USE_EXTERNAL_TASK_SECURITY
	int idx;
	rcu_read_lock();
	for (idx = 0; idx < CCS_MAX_TASK_SECURITY_HASH; idx++) {
		struct ccs_security *ptr;
		struct list_head *list = &ccs_task_security_list[idx];
		list_for_each_entry_rcu(ptr, list, list) {
			if (!(ptr->ccs_flags & CCS_TASK_IS_IN_EXECVE)) {
				smp_rmb(); /* Avoid out of order execution. */
				if (ptr->ccs_domain_info != domain)
					continue;
			}
			in_use = true;
			goto out;
		}
	}
out:
	rcu_read_unlock();
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	struct task_struct *g;
	struct task_struct *t;
	ccs_tasklist_lock();
	do_each_thread(g, t) {
		if (!(t->ccs_flags & CCS_TASK_IS_IN_EXECVE)) {
			smp_rmb(); /* Avoid out of order execution. */
			if (t->ccs_domain_info != domain)
				continue;
		}
		in_use = true;
		goto out;
	} while_each_thread(g, t);
out:
	ccs_tasklist_unlock();
#else
	struct task_struct *p;
	ccs_tasklist_lock();
	for_each_process(p) {
		if (!(p->ccs_flags & CCS_TASK_IS_IN_EXECVE)) {
			smp_rmb(); /* Avoid out of order execution. */
			if (p->ccs_domain_info != domain)
				continue;
		}
		in_use = true;
		break;
	}
	ccs_tasklist_unlock();
#endif
	return in_use;
}

/**
 * ccs_del_acl - Delete members in "struct ccs_acl_info".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
void ccs_del_acl(struct list_head *element)
{
	struct ccs_acl_info *acl = container_of(element, typeof(*acl), list);
	ccs_put_condition(acl->cond);
	switch (acl->type) {
	case CCS_TYPE_PATH_ACL:
		{
			struct ccs_path_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_name_union(&entry->name);
		}
		break;
	case CCS_TYPE_PATH2_ACL:
		{
			struct ccs_path2_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_name_union(&entry->name1);
			ccs_put_name_union(&entry->name2);
		}
		break;
	case CCS_TYPE_PATH_NUMBER_ACL:
		{
			struct ccs_path_number_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_name_union(&entry->name);
			ccs_put_number_union(&entry->number);
		}
		break;
	case CCS_TYPE_MKDEV_ACL:
		{
			struct ccs_mkdev_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_name_union(&entry->name);
			ccs_put_number_union(&entry->mode);
			ccs_put_number_union(&entry->major);
			ccs_put_number_union(&entry->minor);
		}
		break;
	case CCS_TYPE_MOUNT_ACL:
		{
			struct ccs_mount_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_name_union(&entry->dev_name);
			ccs_put_name_union(&entry->dir_name);
			ccs_put_name_union(&entry->fs_type);
			ccs_put_number_union(&entry->flags);
		}
		break;
#ifdef CONFIG_CCSECURITY_NETWORK
	case CCS_TYPE_INET_ACL:
		{
			struct ccs_inet_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_group(entry->address.group);
			ccs_put_number_union(&entry->port);
		}
		break;
	case CCS_TYPE_UNIX_ACL:
		{
			struct ccs_unix_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_name_union(&entry->name);
		}
		break;
#endif
#ifdef CONFIG_CCSECURITY_MISC
	case CCS_TYPE_ENV_ACL:
		{
			struct ccs_env_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_name(entry->env);
		}
		break;
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	case CCS_TYPE_CAPABILITY_ACL:
		{
			/* Nothing to do. */
		}
		break;
#endif
#ifdef CONFIG_CCSECURITY_IPC
	case CCS_TYPE_SIGNAL_ACL:
		{
			struct ccs_signal_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_number_union(&entry->sig);
			ccs_put_name(entry->domainname);
		}
		break;
#endif
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	case CCS_TYPE_AUTO_EXECUTE_HANDLER:
	case CCS_TYPE_DENIED_EXECUTE_HANDLER:
		{
			struct ccs_handler_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_name(entry->handler);
		}
		break;
#endif
#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION
	case CCS_TYPE_AUTO_TASK_ACL:
	case CCS_TYPE_MANUAL_TASK_ACL:
		{
			struct ccs_task_acl *entry =
				container_of(acl, typeof(*entry), head);
			ccs_put_name(entry->domainname);
		}
		break;
#endif
	}
}

/**
 * ccs_del_domain - Delete members in "struct ccs_domain_info".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 *
 * Caller holds ccs_policy_lock mutex.
 */
static inline void ccs_del_domain(struct list_head *element)
{
	struct ccs_domain_info *domain =
		container_of(element, typeof(*domain), list);
	struct ccs_acl_info *acl;
	struct ccs_acl_info *tmp;
	/*
	 * Since this domain is referenced from neither "struct ccs_io_buffer"
	 * nor "struct task_struct", we can delete elements without checking
	 * for is_deleted flag.
	 */
	list_for_each_entry_safe(acl, tmp, &domain->acl_info_list, list) {
		ccs_del_acl(&acl->list);
		ccs_memory_free(acl, CCS_ID_ACL);
	}
	ccs_put_name(domain->domainname);
}

/**
 * ccs_del_path_group - Delete members in "struct ccs_path_group".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static inline void ccs_del_path_group(struct list_head *element)
{
	struct ccs_path_group *member =
		container_of(element, typeof(*member), head.list);
	ccs_put_name(member->member_name);
}

/**
 * ccs_del_group - Delete "struct ccs_group".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static inline void ccs_del_group(struct list_head *element)
{
	struct ccs_group *group =
		container_of(element, typeof(*group), head.list);
	ccs_put_name(group->group_name);
}

/**
 * ccs_del_address_group - Delete members in "struct ccs_address_group".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static inline void ccs_del_address_group(struct list_head *element)
{
	/* Nothing to do. */
}

/**
 * ccs_del_number_group - Delete members in "struct ccs_number_group".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static inline void ccs_del_number_group(struct list_head *element)
{
	/* Nothing to do. */
}

/**
 * ccs_del_reservedport - Delete members in "struct ccs_reserved".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static inline void ccs_del_reservedport(struct list_head *element)
{
	/* Nothing to do. */
}

/**
 * ccs_del_condition - Delete members in "struct ccs_condition".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
void ccs_del_condition(struct list_head *element)
{
	struct ccs_condition *cond = container_of(element, typeof(*cond),
						  head.list);
	const u16 condc = cond->condc;
	const u16 numbers_count = cond->numbers_count;
	const u16 names_count = cond->names_count;
	const u16 argc = cond->argc;
	const u16 envc = cond->envc;
	unsigned int i;
	const struct ccs_condition_element *condp
		= (const struct ccs_condition_element *) (cond + 1);
	struct ccs_number_union *numbers_p
		= (struct ccs_number_union *) (condp + condc);
	struct ccs_name_union *names_p
		= (struct ccs_name_union *) (numbers_p + numbers_count);
	const struct ccs_argv *argv
		= (const struct ccs_argv *) (names_p + names_count);
	const struct ccs_envp *envp
		= (const struct ccs_envp *) (argv + argc);
	for (i = 0; i < numbers_count; i++)
		ccs_put_number_union(numbers_p++);
	for (i = 0; i < names_count; i++)
		ccs_put_name_union(names_p++);
	for (i = 0; i < argc; argv++, i++)
		ccs_put_name(argv->value);
	for (i = 0; i < envc; envp++, i++) {
		ccs_put_name(envp->name);
		ccs_put_name(envp->value);
	}
	ccs_put_name(cond->transit);
}

/**
 * ccs_del_name - Delete members in "struct ccs_name".
 *
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static inline void ccs_del_name(struct list_head *element)
{
	/* Nothing to do. */
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)

/**
 * ccs_lock - Alternative for srcu_read_lock().
 *
 * Returns index number which has to be passed to ccs_unlock().
 */
int ccs_lock(void)
{
	int idx;
	spin_lock(&ccs_counter_lock);
	idx = ccs_counter.counter_idx;
	ccs_counter.counter[idx]++;
	spin_unlock(&ccs_counter_lock);
	return idx;
}

/**
 * ccs_unlock - Alternative for srcu_read_unlock().
 *
 * @idx: Index number returned by ccs_lock().
 *
 * Returns nothing.
 */
void ccs_unlock(const int idx)
{
	spin_lock(&ccs_counter_lock);
	ccs_counter.counter[idx]--;
	spin_unlock(&ccs_counter_lock);
}

/**
 * ccs_synchronize_counter - Alternative for synchronize_srcu().
 *
 * Returns nothing.
 */
static void ccs_synchronize_counter(void)
{
	int idx;
	int v;
	/*
	 * Change currently active counter's index. Make it visible to other
	 * threads by doing it with ccs_counter_lock held.
	 * This function is called by garbage collector thread, and the garbage
	 * collector thread is exclusive. Therefore, it is guaranteed that
	 * SRCU grace period has expired when returning from this function.
	 */
	spin_lock(&ccs_counter_lock);
	idx = ccs_counter.counter_idx;
	ccs_counter.counter_idx ^= 1;
	v = ccs_counter.counter[idx];
	spin_unlock(&ccs_counter_lock);
	/* Wait for previously active counter to become 0. */
	while (v) {
		ssleep(1);
		spin_lock(&ccs_counter_lock);
		v = ccs_counter.counter[idx];
		spin_unlock(&ccs_counter_lock);
	}
}

#endif

/**
 * ccs_try_to_gc - Try to kfree() an entry.
 *
 * @type:    One of values in "enum ccs_policy_id".
 * @element: Pointer to "struct list_head".
 *
 * Returns nothing.
 *
 * Caller holds ccs_policy_lock mutex.
 */
static void ccs_try_to_gc(const enum ccs_policy_id type,
			  struct list_head *element)
{
	/*
	 * __list_del_entry() guarantees that the list element became no longer
	 * reachable from the list which the element was originally on (e.g.
	 * ccs_domain_list). Also, synchronize_srcu() guarantees that the list
	 * element became no longer referenced by syscall users.
	 */
	__list_del_entry(element);
	mutex_unlock(&ccs_policy_lock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
	synchronize_srcu(&ccs_ss);
#else
	ccs_synchronize_counter();
#endif
	/*
	 * However, there are two users which may still be using the list
	 * element. We need to defer until both users forget this element.
	 *
	 * Don't kfree() until "struct ccs_io_buffer"->r.{domain,group,acl} and
	 * "struct ccs_io_buffer"->w.domain forget this element.
	 */
	if (ccs_struct_used_by_io_buffer(element))
		goto reinject;
	switch (type) {
	case CCS_ID_TRANSITION_CONTROL:
		ccs_del_transition_control(element);
		break;
	case CCS_ID_MANAGER:
		ccs_del_manager(element);
		break;
	case CCS_ID_AGGREGATOR:
		ccs_del_aggregator(element);
		break;
	case CCS_ID_GROUP:
		ccs_del_group(element);
		break;
	case CCS_ID_PATH_GROUP:
		ccs_del_path_group(element);
		break;
#ifdef CONFIG_CCSECURITY_NETWORK
	case CCS_ID_ADDRESS_GROUP:
		ccs_del_address_group(element);
		break;
#endif
	case CCS_ID_NUMBER_GROUP:
		ccs_del_number_group(element);
		break;
#ifdef CONFIG_CCSECURITY_PORTRESERVE
	case CCS_ID_RESERVEDPORT:
		ccs_del_reservedport(element);
		break;
#endif
	case CCS_ID_CONDITION:
		ccs_del_condition(element);
		break;
	case CCS_ID_NAME:
		/*
		 * Don't kfree() until all "struct ccs_io_buffer"->r.w[] forget
		 * this element.
		 */
		if (ccs_name_used_by_io_buffer
		    (container_of(element, typeof(struct ccs_name),
				  head.list)->entry.name,
		     container_of(element, typeof(struct ccs_name),
				  head.list)->size))
			goto reinject;
		ccs_del_name(element);
		break;
	case CCS_ID_ACL:
		ccs_del_acl(element);
		break;
	case CCS_ID_DOMAIN:
		/*
		 * Don't kfree() until all "struct task_struct" forget this
		 * element.
		 */
		if (ccs_domain_used_by_task
		    (container_of(element, typeof(struct ccs_domain_info),
				  list)))
			goto reinject;
		break;
	case CCS_MAX_POLICY:
		break;
	}
	mutex_lock(&ccs_policy_lock);
	if (type == CCS_ID_DOMAIN)
		ccs_del_domain(element);
	ccs_memory_free(element, type);
	return;
reinject:
	/*
	 * We can safely reinject this element here bacause
	 * (1) Appending list elements and removing list elements are protected
	 *     by ccs_policy_lock mutex.
	 * (2) Only this function removes list elements and this function is
	 *     exclusively executed by ccs_gc_mutex mutex.
	 * are true.
	 */
	mutex_lock(&ccs_policy_lock);
	list_add_rcu(element, element->prev);
}

/**
 * ccs_collect_member - Delete elements with "struct ccs_acl_head".
 *
 * @id:          One of values in "enum ccs_policy_id".
 * @member_list: Pointer to "struct list_head".
 *
 * Returns nothing.
 *
 * Caller holds ccs_policy_lock mutex.
 */
static void ccs_collect_member(const enum ccs_policy_id id,
			       struct list_head *member_list)
{
	struct ccs_acl_head *member;
	struct ccs_acl_head *tmp;
	list_for_each_entry_safe(member, tmp, member_list, list) {
		if (!member->is_deleted)
			continue;
		member->is_deleted = CCS_GC_IN_PROGRESS;
		ccs_try_to_gc(id, &member->list);
	}
}

/**
 * ccs_collect_acl - Delete elements in "struct ccs_domain_info".
 *
 * @list: Pointer to "struct list_head".
 *
 * Returns nothing.
 *
 * Caller holds ccs_policy_lock mutex.
 */
static void ccs_collect_acl(struct list_head *list)
{
	struct ccs_acl_info *acl;
	struct ccs_acl_info *tmp;
	list_for_each_entry_safe(acl, tmp, list, list) {
		if (!acl->is_deleted)
			continue;
		acl->is_deleted = CCS_GC_IN_PROGRESS;
		ccs_try_to_gc(CCS_ID_ACL, &acl->list);
	}
}

/**
 * ccs_collect_entry - Try to kfree() deleted elements.
 *
 * Returns nothing.
 */
static void ccs_collect_entry(void)
{
	int i;
	enum ccs_policy_id id;
	struct ccs_policy_namespace *ns;
	mutex_lock(&ccs_policy_lock);
	{
		struct ccs_domain_info *domain;
		struct ccs_domain_info *tmp;
		list_for_each_entry_safe(domain, tmp, &ccs_domain_list, list) {
			ccs_collect_acl(&domain->acl_info_list);
			if (!domain->is_deleted ||
			    ccs_domain_used_by_task(domain))
				continue;
			ccs_try_to_gc(CCS_ID_DOMAIN, &domain->list);
		}
	}
	list_for_each_entry(ns, &ccs_namespace_list, namespace_list) {
		for (id = 0; id < CCS_MAX_POLICY; id++)
			ccs_collect_member(id, &ns->policy_list[id]);
		for (i = 0; i < CCS_MAX_ACL_GROUPS; i++)
			ccs_collect_acl(&ns->acl_group[i]);
	}
	{
		struct ccs_shared_acl_head *ptr;
		struct ccs_shared_acl_head *tmp;
		list_for_each_entry_safe(ptr, tmp, &ccs_condition_list, list) {
			if (atomic_read(&ptr->users) > 0)
				continue;
			atomic_set(&ptr->users, CCS_GC_IN_PROGRESS);
			ccs_try_to_gc(CCS_ID_CONDITION, &ptr->list);
		}
	}
	list_for_each_entry(ns, &ccs_namespace_list, namespace_list) {
		for (i = 0; i < CCS_MAX_GROUP; i++) {
			struct list_head *list = &ns->group_list[i];
			struct ccs_group *group;
			struct ccs_group *tmp;
			switch (i) {
			case 0:
				id = CCS_ID_PATH_GROUP;
				break;
			case 1:
				id = CCS_ID_NUMBER_GROUP;
				break;
			default:
#ifdef CONFIG_CCSECURITY_NETWORK
				id = CCS_ID_ADDRESS_GROUP;
#else
				continue;
#endif
				break;
			}
			list_for_each_entry_safe(group, tmp, list, head.list) {
				ccs_collect_member(id, &group->member_list);
				if (!list_empty(&group->member_list) ||
				    atomic_read(&group->head.users) > 0)
					continue;
				atomic_set(&group->head.users,
					   CCS_GC_IN_PROGRESS);
				ccs_try_to_gc(CCS_ID_GROUP, &group->head.list);
			}
		}
	}
	for (i = 0; i < CCS_MAX_HASH; i++) {
		struct list_head *list = &ccs_name_list[i];
		struct ccs_shared_acl_head *ptr;
		struct ccs_shared_acl_head *tmp;
		list_for_each_entry_safe(ptr, tmp, list, list) {
			if (atomic_read(&ptr->users) > 0)
				continue;
			atomic_set(&ptr->users, CCS_GC_IN_PROGRESS);
			ccs_try_to_gc(CCS_ID_NAME, &ptr->list);
		}
	}
	mutex_unlock(&ccs_policy_lock);
}

/**
 * ccs_gc_thread - Garbage collector thread function.
 *
 * @unused: Unused.
 *
 * Returns 0.
 */
static int ccs_gc_thread(void *unused)
{
	/* Garbage collector thread is exclusive. */
	static DEFINE_MUTEX(ccs_gc_mutex);
	if (!mutex_trylock(&ccs_gc_mutex))
		goto out;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 6)
	/* daemonize() not needed. */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)
	daemonize("GC for CCS");
#else
	daemonize();
	reparent_to_init();
#if defined(TASK_DEAD)
	{
		struct task_struct *task = current;
		spin_lock_irq(&task->sighand->siglock);
		siginitsetinv(&task->blocked, 0);
		recalc_sigpending();
		spin_unlock_irq(&task->sighand->siglock);
	}
#else
	{
		struct task_struct *task = current;
		spin_lock_irq(&task->sigmask_lock);
		siginitsetinv(&task->blocked, 0);
		recalc_sigpending(task);
		spin_unlock_irq(&task->sigmask_lock);
	}
#endif
	snprintf(current->comm, sizeof(current->comm) - 1, "GC for CCS");
#endif
	ccs_collect_entry();
	{
		struct ccs_io_buffer *head;
		struct ccs_io_buffer *tmp;
		spin_lock(&ccs_io_buffer_list_lock);
		list_for_each_entry_safe(head, tmp, &ccs_io_buffer_list,
					 list) {
			if (head->users)
				continue;
			list_del(&head->list);
			kfree(head->read_buf);
			kfree(head->write_buf);
			kfree(head);
		}
		spin_unlock(&ccs_io_buffer_list_lock);
	}
	mutex_unlock(&ccs_gc_mutex);
out:
	/* This acts as do_exit(0). */
	return 0;
}

/**
 * ccs_notify_gc - Register/unregister /proc/ccs/ users.
 *
 * @head:        Pointer to "struct ccs_io_buffer".
 * @is_register: True if register, false if unregister.
 *
 * Returns nothing.
 */
void ccs_notify_gc(struct ccs_io_buffer *head, const bool is_register)
{
	bool is_write = false;
	spin_lock(&ccs_io_buffer_list_lock);
	if (is_register) {
		head->users = 1;
		list_add(&head->list, &ccs_io_buffer_list);
	} else {
		is_write = head->write_buf != NULL;
		if (!--head->users) {
			list_del(&head->list);
			kfree(head->read_buf);
			kfree(head->write_buf);
			kfree(head);
		}
	}
	spin_unlock(&ccs_io_buffer_list_lock);
	if (is_write) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 6)
		struct task_struct *task = kthread_create(ccs_gc_thread, NULL,
							  "GC for CCS");
		if (!IS_ERR(task))
			wake_up_process(task);
#else
		kernel_thread(ccs_gc_thread, NULL, 0);
#endif
	}
}
