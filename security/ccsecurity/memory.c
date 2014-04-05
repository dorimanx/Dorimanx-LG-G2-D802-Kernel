/*
 * security/ccsecurity/memory.c
 *
 * Copyright (C) 2005-2012  NTT DATA CORPORATION
 *
 * Version: 1.8.3+   2012/05/05
 */

#include "internal.h"

/***** SECTION1: Constants definition *****/

/***** SECTION2: Structure definition *****/

/***** SECTION3: Prototype definition section *****/

bool ccs_memory_ok(const void *ptr, const unsigned int size);
const struct ccs_path_info *ccs_get_name(const char *name);
#ifdef CONFIG_CCSECURITY_USE_EXTERNAL_TASK_SECURITY
struct ccs_security *ccs_find_task_security(const struct task_struct *task);
#endif
void *ccs_commit_ok(void *data, const unsigned int size);
void __init ccs_mm_init(void);
void ccs_warn_oom(const char *function);

#ifdef CONFIG_CCSECURITY_USE_EXTERNAL_TASK_SECURITY
static int __ccs_alloc_task_security(const struct task_struct *task);
static void __ccs_free_task_security(const struct task_struct *task);
static void ccs_add_task_security(struct ccs_security *ptr,
				  struct list_head *list);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 8)
static void ccs_rcu_free(struct rcu_head *rcu);
#else
static void ccs_rcu_free(void *arg);
#endif
#endif

/***** SECTION4: Standalone functions section *****/

/***** SECTION5: Variables definition section *****/

/* Memoy currently used by policy/audit log/query. */
unsigned int ccs_memory_used[CCS_MAX_MEMORY_STAT];

/* Memory quota for "policy"/"audit log"/"query". */
unsigned int ccs_memory_quota[CCS_MAX_MEMORY_STAT];

/* The list for "struct ccs_name". */
struct list_head ccs_name_list[CCS_MAX_HASH];

#ifdef CONFIG_CCSECURITY_USE_EXTERNAL_TASK_SECURITY

/* Dummy security context for avoiding NULL pointer dereference. */
static struct ccs_security ccs_oom_security = {
	.ccs_domain_info = &ccs_kernel_domain
};

/* Dummy security context for avoiding NULL pointer dereference. */
static struct ccs_security ccs_default_security = {
	.ccs_domain_info = &ccs_kernel_domain
};

/* List of "struct ccs_security". */
struct list_head ccs_task_security_list[CCS_MAX_TASK_SECURITY_HASH];
/* Lock for protecting ccs_task_security_list[]. */
static DEFINE_SPINLOCK(ccs_task_security_list_lock);

#endif

/***** SECTION6: Dependent functions section *****/

/**
 * ccs_warn_oom - Print out of memory warning message.
 *
 * @function: Function's name.
 *
 * Returns nothing.
 */
void ccs_warn_oom(const char *function)
{
	/* Reduce error messages. */
	static pid_t ccs_last_pid;
	const pid_t pid = current->pid;
	if (ccs_last_pid != pid) {
		printk(KERN_WARNING "ERROR: Out of memory at %s.\n",
		       function);
		ccs_last_pid = pid;
	}
	if (!ccs_policy_loaded)
		panic("MAC Initialization failed.\n");
}

/**
 * ccs_memory_ok - Check memory quota.
 *
 * @ptr:  Pointer to allocated memory. Maybe NULL.
 * @size: Size in byte. Not used if @ptr is NULL.
 *
 * Returns true if @ptr is not NULL and quota not exceeded, false otherwise.
 *
 * Caller holds ccs_policy_lock mutex.
 */
bool ccs_memory_ok(const void *ptr, const unsigned int size)
{
	if (ptr) {
		const size_t s = ccs_round2(size);
		ccs_memory_used[CCS_MEMORY_POLICY] += s;
		if (!ccs_memory_quota[CCS_MEMORY_POLICY] ||
		    ccs_memory_used[CCS_MEMORY_POLICY] <=
		    ccs_memory_quota[CCS_MEMORY_POLICY])
			return true;
		ccs_memory_used[CCS_MEMORY_POLICY] -= s;
	}
	ccs_warn_oom(__func__);
	return false;
}

/**
 * ccs_commit_ok - Allocate memory and check memory quota.
 *
 * @data: Data to copy from.
 * @size: Size in byte.
 *
 * Returns pointer to allocated memory on success, NULL otherwise.
 * @data is zero-cleared on success.
 *
 * Caller holds ccs_policy_lock mutex.
 */
void *ccs_commit_ok(void *data, const unsigned int size)
{
	void *ptr = kmalloc(size, CCS_GFP_FLAGS);
	if (ccs_memory_ok(ptr, size)) {
		memmove(ptr, data, size);
		memset(data, 0, size);
		return ptr;
	}
	kfree(ptr);
	return NULL;
}

/**
 * ccs_get_name - Allocate memory for string data.
 *
 * @name: The string to store into the permernent memory.
 *
 * Returns pointer to "struct ccs_path_info" on success, NULL otherwise.
 */
const struct ccs_path_info *ccs_get_name(const char *name)
{
	struct ccs_name *ptr;
	unsigned int hash;
	int len;
	int allocated_len;
	struct list_head *head;

	if (!name)
		return NULL;
	len = strlen(name) + 1;
	hash = full_name_hash((const unsigned char *) name, len - 1);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) || defined(RHEL_MAJOR)
	head = &ccs_name_list[hash_long(hash, CCS_HASH_BITS)];
#else
	head = &ccs_name_list[hash % CCS_MAX_HASH];
#endif
	if (mutex_lock_interruptible(&ccs_policy_lock))
		return NULL;
	list_for_each_entry(ptr, head, head.list) {
		if (hash != ptr->entry.hash || strcmp(name, ptr->entry.name) ||
		    atomic_read(&ptr->head.users) == CCS_GC_IN_PROGRESS)
			continue;
		atomic_inc(&ptr->head.users);
		goto out;
	}
	allocated_len = sizeof(*ptr) + len;
	ptr = kzalloc(allocated_len, CCS_GFP_FLAGS);
	if (ccs_memory_ok(ptr, allocated_len)) {
		ptr->entry.name = ((char *) ptr) + sizeof(*ptr);
		memmove((char *) ptr->entry.name, name, len);
		atomic_set(&ptr->head.users, 1);
		ccs_fill_path_info(&ptr->entry);
		ptr->size = allocated_len;
		list_add_tail(&ptr->head.list, head);
	} else {
		kfree(ptr);
		ptr = NULL;
	}
out:
	mutex_unlock(&ccs_policy_lock);
	return ptr ? &ptr->entry : NULL;
}

#ifdef CONFIG_CCSECURITY_USE_EXTERNAL_TASK_SECURITY

/**
 * ccs_add_task_security - Add "struct ccs_security" to list.
 *
 * @ptr:  Pointer to "struct ccs_security".
 * @list: Pointer to "struct list_head".
 *
 * Returns nothing.
 */
static void ccs_add_task_security(struct ccs_security *ptr,
				  struct list_head *list)
{
	unsigned long flags;
	spin_lock_irqsave(&ccs_task_security_list_lock, flags);
	list_add_rcu(&ptr->list, list);
	spin_unlock_irqrestore(&ccs_task_security_list_lock, flags);
}

/**
 * __ccs_alloc_task_security - Allocate memory for new tasks.
 *
 * @task: Pointer to "struct task_struct".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int __ccs_alloc_task_security(const struct task_struct *task)
{
	struct ccs_security *old_security = ccs_current_security();
	struct ccs_security *new_security = kzalloc(sizeof(*new_security),
						    GFP_KERNEL);
	struct list_head *list = &ccs_task_security_list
		[hash_ptr((void *) task, CCS_TASK_SECURITY_HASH_BITS)];
	if (!new_security)
		return -ENOMEM;
	new_security->task = task;
	new_security->ccs_domain_info = old_security->ccs_domain_info;
	new_security->ccs_flags = old_security->ccs_flags;
	ccs_add_task_security(new_security, list);
	return 0;
}

/**
 * ccs_find_task_security - Find "struct ccs_security" for given task.
 *
 * @task: Pointer to "struct task_struct".
 *
 * Returns pointer to "struct ccs_security" on success, &ccs_oom_security on
 * out of memory, &ccs_default_security otherwise.
 *
 * If @task is current thread and "struct ccs_security" for current thread was
 * not found, I try to allocate it. But if allocation failed, current thread
 * will be killed by SIGKILL. Note that if current->pid == 1, sending SIGKILL
 * won't work.
 */
struct ccs_security *ccs_find_task_security(const struct task_struct *task)
{
	struct ccs_security *ptr;
	struct list_head *list = &ccs_task_security_list
		[hash_ptr((void *) task, CCS_TASK_SECURITY_HASH_BITS)];
	/* Make sure INIT_LIST_HEAD() in ccs_mm_init() takes effect. */
	while (!list->next);
	rcu_read_lock();
	list_for_each_entry_rcu(ptr, list, list) {
		if (ptr->task != task)
			continue;
		rcu_read_unlock();
		return ptr;
	}
	rcu_read_unlock();
	if (task != current)
		return &ccs_default_security;
	/* Use GFP_ATOMIC because caller may have called rcu_read_lock(). */
	ptr = kzalloc(sizeof(*ptr), GFP_ATOMIC);
	if (!ptr) {
		printk(KERN_WARNING "Unable to allocate memory for pid=%u\n",
		       task->pid);
		send_sig(SIGKILL, current, 0);
		return &ccs_oom_security;
	}
	*ptr = ccs_default_security;
	ptr->task = task;
	ccs_add_task_security(ptr, list);
	return ptr;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 8)

/**
 * ccs_rcu_free - RCU callback for releasing "struct ccs_security".
 *
 * @rcu: Pointer to "struct rcu_head".
 *
 * Returns nothing.
 */
static void ccs_rcu_free(struct rcu_head *rcu)
{
	struct ccs_security *ptr = container_of(rcu, typeof(*ptr), rcu);
	kfree(ptr);
}

#else

/**
 * ccs_rcu_free - RCU callback for releasing "struct ccs_security".
 *
 * @arg: Pointer to "void".
 *
 * Returns nothing.
 */
static void ccs_rcu_free(void *arg)
{
	struct ccs_security *ptr = arg;
	kfree(ptr);
}

#endif

/**
 * __ccs_free_task_security - Release memory associated with "struct task_struct".
 *
 * @task: Pointer to "struct task_struct".
 *
 * Returns nothing.
 */
static void __ccs_free_task_security(const struct task_struct *task)
{
	unsigned long flags;
	struct ccs_security *ptr = ccs_find_task_security(task);
	if (ptr == &ccs_default_security || ptr == &ccs_oom_security)
		return;
	spin_lock_irqsave(&ccs_task_security_list_lock, flags);
	list_del_rcu(&ptr->list);
	spin_unlock_irqrestore(&ccs_task_security_list_lock, flags);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 8)
	call_rcu(&ptr->rcu, ccs_rcu_free);
#else
	call_rcu(&ptr->rcu, ccs_rcu_free, ptr);
#endif
}

#endif

/**
 * ccs_mm_init - Initialize mm related code.
 *
 * Returns nothing.
 */
void __init ccs_mm_init(void)
{
	int idx;
	for (idx = 0; idx < CCS_MAX_HASH; idx++)
		INIT_LIST_HEAD(&ccs_name_list[idx]);
#ifdef CONFIG_CCSECURITY_USE_EXTERNAL_TASK_SECURITY
	for (idx = 0; idx < CCS_MAX_TASK_SECURITY_HASH; idx++)
		INIT_LIST_HEAD(&ccs_task_security_list[idx]);
#endif
	smp_wmb(); /* Avoid out of order execution. */
#ifdef CONFIG_CCSECURITY_USE_EXTERNAL_TASK_SECURITY
	ccsecurity_ops.alloc_task_security = __ccs_alloc_task_security;
	ccsecurity_ops.free_task_security = __ccs_free_task_security;
#endif
	ccs_kernel_domain.domainname = ccs_get_name("<kernel>");
	list_add_tail_rcu(&ccs_kernel_domain.list, &ccs_domain_list);
}
