/*
 * security/ccsecurity/internal.h
 *
 * Copyright (C) 2005-2012  NTT DATA CORPORATION
 *
 * Version: 1.8.3+   2012/05/05
 */

#ifndef _SECURITY_CCSECURITY_INTERNAL_H
#define _SECURITY_CCSECURITY_INTERNAL_H

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/utime.h>
#include <linux/file.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 38)
#include <linux/smp_lock.h>
#endif
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/poll.h>
#include <linux/binfmts.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/net.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/un.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
#include <linux/fs.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)
#include <linux/namei.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
#include <linux/fs_struct.h>
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
#include <linux/namespace.h>
#endif
#include <linux/proc_fs.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) || defined(RHEL_MAJOR)
#include <linux/hash.h>
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18) || (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33) && defined(CONFIG_SYSCTL_SYSCALL))
#include <linux/sysctl.h>
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 6)
#include <linux/kthread.h>
#endif
#include <stdarg.h>
#include <asm/uaccess.h>
#include <net/sock.h>
#include <net/af_unix.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/udp.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
#define sk_family family
#define sk_protocol protocol
#define sk_type type
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)

/* Structure for holding "struct vfsmount *" and "struct dentry *". */
struct path {
	struct vfsmount *mnt;
	struct dentry *dentry;
};

#endif

#ifndef __printf
#define __printf(a,b) __attribute__((format(printf,a,b)))
#endif
#ifndef __packed
#define __packed __attribute__((__packed__))
#endif
#ifndef bool
#define bool _Bool
#endif
#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

#ifndef __user
#define __user
#endif

#ifndef current_uid
#define current_uid()   (current->uid)
#endif
#ifndef current_gid
#define current_gid()   (current->gid)
#endif
#ifndef current_euid
#define current_euid()  (current->euid)
#endif
#ifndef current_egid
#define current_egid()  (current->egid)
#endif
#ifndef current_suid
#define current_suid()  (current->suid)
#endif
#ifndef current_sgid
#define current_sgid()  (current->sgid)
#endif
#ifndef current_fsuid
#define current_fsuid() (current->fsuid)
#endif
#ifndef current_fsgid
#define current_fsgid() (current->fsgid)
#endif

#ifndef DEFINE_SPINLOCK
#define DEFINE_SPINLOCK(x) spinlock_t x = SPIN_LOCK_UNLOCKED
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#define mutex semaphore
#define mutex_init(mutex) init_MUTEX(mutex)
#define mutex_unlock(mutex) up(mutex)
#define mutex_lock(mutex) down(mutex)
#define mutex_lock_interruptible(mutex) down_interruptible(mutex)
#define mutex_trylock(mutex) (!down_trylock(mutex))
#define DEFINE_MUTEX(mutexname) DECLARE_MUTEX(mutexname)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 15)
#define MS_UNBINDABLE	(1<<17)	/* change to unbindable */
#define MS_PRIVATE	(1<<18)	/* change to private */
#define MS_SLAVE	(1<<19)	/* change to slave */
#define MS_SHARED	(1<<20)	/* change to shared */
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({				\
			const typeof(((type *)0)->member) *__mptr = (ptr); \
			(type *)((char *)__mptr - offsetof(type, member)); })
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#define smp_read_barrier_depends smp_rmb
#endif

#ifndef ACCESS_ONCE
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))
#endif

#ifndef rcu_dereference
#define rcu_dereference(p)     ({					\
			typeof(p) _________p1 = ACCESS_ONCE(p);		\
			smp_read_barrier_depends(); /* see RCU */	\
			(_________p1);					\
		})
#endif

#ifndef rcu_assign_pointer
#define rcu_assign_pointer(p, v)			\
	({						\
		if (!__builtin_constant_p(v) ||		\
		    ((v) != NULL))			\
			smp_wmb(); /* see RCU */	\
		(p) = (v);				\
	})
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)
#define f_vfsmnt f_path.mnt
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 14)

/**
 * kzalloc() - Allocate memory. The memory is set to zero.
 *
 * @size:  Size to allocate.
 * @flags: GFP flags.
 *
 * Returns pointer to allocated memory on success, NULL otherwise.
 *
 * This is for compatibility with older kernels.
 *
 * Since several distributions backported kzalloc(), I define it as a macro
 * rather than an inlined function in order to avoid multiple definition error.
 */
#define kzalloc(size, flags) ({					\
			void *ret = kmalloc((size), (flags));	\
			if (ret)				\
				memset(ret, 0, (size));		\
			ret; })

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)

/**
 * path_put - Drop reference on "struct path".
 *
 * @path: Pointer to "struct path".
 *
 * Returns nothing.
 *
 * This is for compatibility with older kernels.
 */
static inline void path_put(struct path *path)
{
	dput(path->dentry);
	mntput(path->mnt);
}

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)

/**
 * __list_add_rcu - Insert a new entry between two known consecutive entries.
 *
 * @new:  Pointer to "struct list_head".
 * @prev: Pointer to "struct list_head".
 * @next: Pointer to "struct list_head".
 *
 * Returns nothing.
 *
 * This is for compatibility with older kernels.
 */
static inline void __list_add_rcu(struct list_head *new,
				  struct list_head *prev,
				  struct list_head *next)
{
	new->next = next;
	new->prev = prev;
	rcu_assign_pointer(prev->next, new);
	next->prev = new;
}

/**
 * list_add_tail_rcu - Add a new entry to rcu-protected list.
 *
 * @new:  Pointer to "struct list_head".
 * @head: Pointer to "struct list_head".
 *
 * Returns nothing.
 *
 * This is for compatibility with older kernels.
 */
static inline void list_add_tail_rcu(struct list_head *new,
				     struct list_head *head)
{
	__list_add_rcu(new, head->prev, head);
}

/**
 * list_add_rcu - Add a new entry to rcu-protected list.
 *
 * @new:  Pointer to "struct list_head".
 * @head: Pointer to "struct list_head".
 *
 * Returns nothing.
 *
 * This is for compatibility with older kernels.
 */
static inline void list_add_rcu(struct list_head *new, struct list_head *head)
{
	__list_add_rcu(new, head, head->next);
}

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 38)

/**
 * __list_del_entry - Deletes entry from list without re-initialization.
 *
 * @entry: Pointer to "struct list_head".
 *
 * Returns nothing.
 *
 * This is for compatibility with older kernels.
 */
static inline void __list_del_entry(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

#endif

#ifndef list_for_each_entry_safe

/**
 * list_for_each_entry_safe - Iterate over list of given type safe against removal of list entry.
 *
 * @pos:    The "type *" to use as a loop cursor.
 * @n:      Another "type *" to use as temporary storage.
 * @head:   Pointer to "struct list_head".
 * @member: The name of the list_struct within the struct.
 *
 * This is for compatibility with older kernels.
 */
#define list_for_each_entry_safe(pos, n, head, member)                  \
	for (pos = list_entry((head)->next, typeof(*pos), member),      \
		     n = list_entry(pos->member.next, typeof(*pos), member); \
	     &pos->member != (head);					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

#endif

#ifndef srcu_dereference

/**
 * srcu_dereference - Fetch SRCU-protected pointer with checking.
 *
 * @p:  The pointer to read, prior to dereferencing.
 * @ss: Pointer to "struct srcu_struct".
 *
 * Returns @p.
 *
 * This is for compatibility with older kernels.
 */
#define srcu_dereference(p, ss) rcu_dereference(p)

#endif

#ifndef list_for_each_entry_srcu

/**
 * list_for_each_entry_srcu - Iterate over rcu list of given type.
 *
 * @pos:    The type * to use as a loop cursor.
 * @head:   The head for your list.
 * @member: The name of the list_struct within the struct.
 * @ss:     Pointer to "struct srcu_struct".
 *
 * As of 2.6.36, this macro is not provided because only TOMOYO wants it.
 */
#define list_for_each_entry_srcu(pos, head, member, ss)		      \
	for (pos = list_entry(srcu_dereference((head)->next, ss),     \
			      typeof(*pos), member);		      \
	     prefetch(pos->member.next), &pos->member != (head);      \
	     pos = list_entry(srcu_dereference(pos->member.next, ss), \
			      typeof(*pos), member))

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 30) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9))

#ifndef ssleep

/**
 * ssleep - Sleep for specified seconds.
 *
 * @secs: Seconds to sleep.
 *
 * Returns nothing.
 *
 * This is for compatibility with older kernels.
 *
 * Since several distributions backported ssleep(), I define it as a macro
 * rather than an inlined function in order to avoid multiple definition error.
 */
#define ssleep(secs) {						\
		set_current_state(TASK_UNINTERRUPTIBLE);	\
		schedule_timeout((HZ * secs) + 1);		\
	}

#endif

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0)

/**
 * from_kuid - Convert kuid_t to uid_t.
 *
 * @ns:  Unused.
 * @uid: kuid_t value.
 *
 * Returns uid seen from init's user namespace.
 */
#define from_kuid(ns, uid) (uid)

/**
 * from_kgid - Convert kgid_t to gid_t.
 *
 * @ns:  Unused.
 * @gid: kgid_t value.
 *
 * Returns gid seen from init's user namespace.
 */
#define from_kgid(ns, gid) (gid)

/**
 * uid_eq - Check whether the uids are equals or not.
 *
 * @left: Uid seen from current user namespace.
 * @right: Uid seen from current user namespace.
 *
 * Returns true if uid is root in init's user namespace, false otherwise.
 */
#define uid_eq(left, right) ((left) == (right))
#define GLOBAL_ROOT_UID 0

#endif

/*
 * TOMOYO specific part start.
 */

#include <linux/ccsecurity.h>

/* Enumeration definition for internal use. */

/* Index numbers for Access Controls. */
enum ccs_acl_entry_type_index {
	CCS_TYPE_PATH_ACL,
	CCS_TYPE_PATH2_ACL,
	CCS_TYPE_PATH_NUMBER_ACL,
	CCS_TYPE_MKDEV_ACL,
	CCS_TYPE_MOUNT_ACL,
#ifdef CONFIG_CCSECURITY_MISC
	CCS_TYPE_ENV_ACL,
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	CCS_TYPE_CAPABILITY_ACL,
#endif
#ifdef CONFIG_CCSECURITY_NETWORK
	CCS_TYPE_INET_ACL,
	CCS_TYPE_UNIX_ACL,
#endif
#ifdef CONFIG_CCSECURITY_IPC
	CCS_TYPE_SIGNAL_ACL,
#endif
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	CCS_TYPE_AUTO_EXECUTE_HANDLER,
	CCS_TYPE_DENIED_EXECUTE_HANDLER,
#endif
#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION
	CCS_TYPE_AUTO_TASK_ACL,
	CCS_TYPE_MANUAL_TASK_ACL,
#endif
};

/* Index numbers for "struct ccs_condition". */
enum ccs_conditions_index {
	CCS_TASK_UID,             /* current_uid()   */
	CCS_TASK_EUID,            /* current_euid()  */
	CCS_TASK_SUID,            /* current_suid()  */
	CCS_TASK_FSUID,           /* current_fsuid() */
	CCS_TASK_GID,             /* current_gid()   */
	CCS_TASK_EGID,            /* current_egid()  */
	CCS_TASK_SGID,            /* current_sgid()  */
	CCS_TASK_FSGID,           /* current_fsgid() */
	CCS_TASK_PID,             /* sys_getpid()   */
	CCS_TASK_PPID,            /* sys_getppid()  */
	CCS_EXEC_ARGC,            /* "struct linux_binprm *"->argc */
	CCS_EXEC_ENVC,            /* "struct linux_binprm *"->envc */
	CCS_TYPE_IS_SOCKET,       /* S_IFSOCK */
	CCS_TYPE_IS_SYMLINK,      /* S_IFLNK */
	CCS_TYPE_IS_FILE,         /* S_IFREG */
	CCS_TYPE_IS_BLOCK_DEV,    /* S_IFBLK */
	CCS_TYPE_IS_DIRECTORY,    /* S_IFDIR */
	CCS_TYPE_IS_CHAR_DEV,     /* S_IFCHR */
	CCS_TYPE_IS_FIFO,         /* S_IFIFO */
	CCS_MODE_SETUID,          /* S_ISUID */
	CCS_MODE_SETGID,          /* S_ISGID */
	CCS_MODE_STICKY,          /* S_ISVTX */
	CCS_MODE_OWNER_READ,      /* S_IRUSR */
	CCS_MODE_OWNER_WRITE,     /* S_IWUSR */
	CCS_MODE_OWNER_EXECUTE,   /* S_IXUSR */
	CCS_MODE_GROUP_READ,      /* S_IRGRP */
	CCS_MODE_GROUP_WRITE,     /* S_IWGRP */
	CCS_MODE_GROUP_EXECUTE,   /* S_IXGRP */
	CCS_MODE_OTHERS_READ,     /* S_IROTH */
	CCS_MODE_OTHERS_WRITE,    /* S_IWOTH */
	CCS_MODE_OTHERS_EXECUTE,  /* S_IXOTH */
	CCS_TASK_TYPE,            /* ((u8) task->ccs_flags) &
				     CCS_TASK_IS_EXECUTE_HANDLER */
	CCS_TASK_EXECUTE_HANDLER, /* CCS_TASK_IS_EXECUTE_HANDLER */
	CCS_EXEC_REALPATH,
	CCS_SYMLINK_TARGET,
	CCS_PATH1_UID,
	CCS_PATH1_GID,
	CCS_PATH1_INO,
	CCS_PATH1_MAJOR,
	CCS_PATH1_MINOR,
	CCS_PATH1_PERM,
	CCS_PATH1_TYPE,
	CCS_PATH1_DEV_MAJOR,
	CCS_PATH1_DEV_MINOR,
	CCS_PATH2_UID,
	CCS_PATH2_GID,
	CCS_PATH2_INO,
	CCS_PATH2_MAJOR,
	CCS_PATH2_MINOR,
	CCS_PATH2_PERM,
	CCS_PATH2_TYPE,
	CCS_PATH2_DEV_MAJOR,
	CCS_PATH2_DEV_MINOR,
	CCS_PATH1_PARENT_UID,
	CCS_PATH1_PARENT_GID,
	CCS_PATH1_PARENT_INO,
	CCS_PATH1_PARENT_PERM,
	CCS_PATH2_PARENT_UID,
	CCS_PATH2_PARENT_GID,
	CCS_PATH2_PARENT_INO,
	CCS_PATH2_PARENT_PERM,
	CCS_MAX_CONDITION_KEYWORD,
	CCS_NUMBER_UNION,
	CCS_NAME_UNION,
	CCS_ARGV_ENTRY,
	CCS_ENVP_ENTRY,
};

/* Index numbers for domain's attributes. */
enum ccs_domain_info_flags_index {
	/* Quota warnning flag.   */
	CCS_DIF_QUOTA_WARNED,
	/*
	 * This domain was unable to create a new domain at
	 * ccs_find_next_domain() because the name of the domain to be created
	 * was too long or it could not allocate memory.
	 * More than one process continued execve() without domain transition.
	 */
	CCS_DIF_TRANSITION_FAILED,
	CCS_MAX_DOMAIN_INFO_FLAGS
};

/* Index numbers for audit type. */
enum ccs_grant_log {
	/* Follow profile's configuration. */
	CCS_GRANTLOG_AUTO,
	/* Do not generate grant log. */
	CCS_GRANTLOG_NO,
	/* Generate grant_log. */
	CCS_GRANTLOG_YES,
};

/* Index numbers for group entries. */
enum ccs_group_id {
	CCS_PATH_GROUP,
	CCS_NUMBER_GROUP,
#ifdef CONFIG_CCSECURITY_NETWORK
	CCS_ADDRESS_GROUP,
#endif
	CCS_MAX_GROUP
};

/* Index numbers for category of functionality. */
enum ccs_mac_category_index {
	CCS_MAC_CATEGORY_FILE,
#ifdef CONFIG_CCSECURITY_NETWORK
	CCS_MAC_CATEGORY_NETWORK,
#endif
#ifdef CONFIG_CCSECURITY_MISC
	CCS_MAC_CATEGORY_MISC,
#endif
#ifdef CONFIG_CCSECURITY_IPC
	CCS_MAC_CATEGORY_IPC,
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	CCS_MAC_CATEGORY_CAPABILITY,
#endif
	CCS_MAX_MAC_CATEGORY_INDEX
};

/* Index numbers for functionality. */
enum ccs_mac_index {
	CCS_MAC_FILE_EXECUTE,
	CCS_MAC_FILE_OPEN,
	CCS_MAC_FILE_CREATE,
	CCS_MAC_FILE_UNLINK,
#ifdef CONFIG_CCSECURITY_FILE_GETATTR
	CCS_MAC_FILE_GETATTR,
#endif
	CCS_MAC_FILE_MKDIR,
	CCS_MAC_FILE_RMDIR,
	CCS_MAC_FILE_MKFIFO,
	CCS_MAC_FILE_MKSOCK,
	CCS_MAC_FILE_TRUNCATE,
	CCS_MAC_FILE_SYMLINK,
	CCS_MAC_FILE_MKBLOCK,
	CCS_MAC_FILE_MKCHAR,
	CCS_MAC_FILE_LINK,
	CCS_MAC_FILE_RENAME,
	CCS_MAC_FILE_CHMOD,
	CCS_MAC_FILE_CHOWN,
	CCS_MAC_FILE_CHGRP,
	CCS_MAC_FILE_IOCTL,
	CCS_MAC_FILE_CHROOT,
	CCS_MAC_FILE_MOUNT,
	CCS_MAC_FILE_UMOUNT,
	CCS_MAC_FILE_PIVOT_ROOT,
#ifdef CONFIG_CCSECURITY_NETWORK
	CCS_MAC_NETWORK_INET_STREAM_BIND,
	CCS_MAC_NETWORK_INET_STREAM_LISTEN,
	CCS_MAC_NETWORK_INET_STREAM_CONNECT,
	CCS_MAC_NETWORK_INET_STREAM_ACCEPT,
	CCS_MAC_NETWORK_INET_DGRAM_BIND,
	CCS_MAC_NETWORK_INET_DGRAM_SEND,
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	CCS_MAC_NETWORK_INET_DGRAM_RECV,
#endif
	CCS_MAC_NETWORK_INET_RAW_BIND,
	CCS_MAC_NETWORK_INET_RAW_SEND,
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	CCS_MAC_NETWORK_INET_RAW_RECV,
#endif
	CCS_MAC_NETWORK_UNIX_STREAM_BIND,
	CCS_MAC_NETWORK_UNIX_STREAM_LISTEN,
	CCS_MAC_NETWORK_UNIX_STREAM_CONNECT,
	CCS_MAC_NETWORK_UNIX_STREAM_ACCEPT,
	CCS_MAC_NETWORK_UNIX_DGRAM_BIND,
	CCS_MAC_NETWORK_UNIX_DGRAM_SEND,
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	CCS_MAC_NETWORK_UNIX_DGRAM_RECV,
#endif
	CCS_MAC_NETWORK_UNIX_SEQPACKET_BIND,
	CCS_MAC_NETWORK_UNIX_SEQPACKET_LISTEN,
	CCS_MAC_NETWORK_UNIX_SEQPACKET_CONNECT,
	CCS_MAC_NETWORK_UNIX_SEQPACKET_ACCEPT,
#endif
#ifdef CONFIG_CCSECURITY_MISC
	CCS_MAC_ENVIRON,
#endif
#ifdef CONFIG_CCSECURITY_IPC
	CCS_MAC_SIGNAL,
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	CCS_MAC_CAPABILITY_USE_ROUTE_SOCKET,
	CCS_MAC_CAPABILITY_USE_PACKET_SOCKET,
	CCS_MAC_CAPABILITY_SYS_REBOOT,
	CCS_MAC_CAPABILITY_SYS_VHANGUP,
	CCS_MAC_CAPABILITY_SYS_SETTIME,
	CCS_MAC_CAPABILITY_SYS_NICE,
	CCS_MAC_CAPABILITY_SYS_SETHOSTNAME,
	CCS_MAC_CAPABILITY_USE_KERNEL_MODULE,
	CCS_MAC_CAPABILITY_SYS_KEXEC_LOAD,
	CCS_MAC_CAPABILITY_SYS_PTRACE,
#endif
	CCS_MAX_MAC_INDEX
};

/* Index numbers for /proc/ccs/stat interface. */
enum ccs_memory_stat_type {
	CCS_MEMORY_POLICY,
	CCS_MEMORY_AUDIT,
	CCS_MEMORY_QUERY,
	CCS_MAX_MEMORY_STAT
};

/* Index numbers for access controls with one pathname and three numbers. */
enum ccs_mkdev_acl_index {
	CCS_TYPE_MKBLOCK,
	CCS_TYPE_MKCHAR,
	CCS_MAX_MKDEV_OPERATION
};

/* Index numbers for operation mode. */
enum ccs_mode_value {
	CCS_CONFIG_DISABLED,
	CCS_CONFIG_LEARNING,
	CCS_CONFIG_PERMISSIVE,
	CCS_CONFIG_ENFORCING,
	CCS_CONFIG_MAX_MODE,
	CCS_CONFIG_WANT_REJECT_LOG =  64,
	CCS_CONFIG_WANT_GRANT_LOG  = 128,
	CCS_CONFIG_USE_DEFAULT     = 255,
};

/* Index numbers for socket operations. */
enum ccs_network_acl_index {
	CCS_NETWORK_BIND,    /* bind() operation. */
	CCS_NETWORK_LISTEN,  /* listen() operation. */
	CCS_NETWORK_CONNECT, /* connect() operation. */
	CCS_NETWORK_ACCEPT,  /* accept() operation. */
	CCS_NETWORK_SEND,    /* send() operation. */
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	CCS_NETWORK_RECV,    /* recv() operation. */
#endif
	CCS_MAX_NETWORK_OPERATION
};

/* Index numbers for access controls with two pathnames. */
enum ccs_path2_acl_index {
	CCS_TYPE_LINK,
	CCS_TYPE_RENAME,
	CCS_TYPE_PIVOT_ROOT,
	CCS_MAX_PATH2_OPERATION
};

/* Index numbers for access controls with one pathname. */
enum ccs_path_acl_index {
	CCS_TYPE_EXECUTE,
	CCS_TYPE_READ,
	CCS_TYPE_WRITE,
	CCS_TYPE_APPEND,
	CCS_TYPE_UNLINK,
#ifdef CONFIG_CCSECURITY_FILE_GETATTR
	CCS_TYPE_GETATTR,
#endif
	CCS_TYPE_RMDIR,
	CCS_TYPE_TRUNCATE,
	CCS_TYPE_SYMLINK,
	CCS_TYPE_CHROOT,
	CCS_TYPE_UMOUNT,
	CCS_MAX_PATH_OPERATION
};

/* Index numbers for access controls with one pathname and one number. */
enum ccs_path_number_acl_index {
	CCS_TYPE_CREATE,
	CCS_TYPE_MKDIR,
	CCS_TYPE_MKFIFO,
	CCS_TYPE_MKSOCK,
	CCS_TYPE_IOCTL,
	CCS_TYPE_CHMOD,
	CCS_TYPE_CHOWN,
	CCS_TYPE_CHGRP,
	CCS_MAX_PATH_NUMBER_OPERATION
};

/* Index numbers for stat(). */
enum ccs_path_stat_index {
	/* Do not change this order. */
	CCS_PATH1,
	CCS_PATH1_PARENT,
	CCS_PATH2,
	CCS_PATH2_PARENT,
	CCS_MAX_PATH_STAT
};

/* Index numbers for entry type. */
enum ccs_policy_id {
#ifdef CONFIG_CCSECURITY_PORTRESERVE
	CCS_ID_RESERVEDPORT,
#endif
	CCS_ID_GROUP,
#ifdef CONFIG_CCSECURITY_NETWORK
	CCS_ID_ADDRESS_GROUP,
#endif
	CCS_ID_PATH_GROUP,
	CCS_ID_NUMBER_GROUP,
	CCS_ID_AGGREGATOR,
	CCS_ID_TRANSITION_CONTROL,
	CCS_ID_MANAGER,
	CCS_ID_CONDITION,
	CCS_ID_NAME,
	CCS_ID_ACL,
	CCS_ID_DOMAIN,
	CCS_MAX_POLICY
};

/* Index numbers for /proc/ccs/stat interface. */
enum ccs_policy_stat_type {
	/* Do not change this order. */
	CCS_STAT_POLICY_UPDATES,
	CCS_STAT_POLICY_LEARNING,   /* == CCS_CONFIG_LEARNING */
	CCS_STAT_POLICY_PERMISSIVE, /* == CCS_CONFIG_PERMISSIVE */
	CCS_STAT_POLICY_ENFORCING,  /* == CCS_CONFIG_ENFORCING */
	CCS_MAX_POLICY_STAT
};

/* Index numbers for profile's PREFERENCE values. */
enum ccs_pref_index {
	CCS_PREF_MAX_AUDIT_LOG,
	CCS_PREF_MAX_LEARNING_ENTRY,
	CCS_PREF_ENFORCING_PENALTY,
	CCS_MAX_PREF
};

/* Index numbers for /proc/ccs/ interfaces. */
enum ccs_proc_interface_index {
	CCS_DOMAIN_POLICY,
	CCS_EXCEPTION_POLICY,
	CCS_PROCESS_STATUS,
	CCS_STAT,
	CCS_AUDIT,
	CCS_VERSION,
	CCS_PROFILE,
	CCS_QUERY,
	CCS_MANAGER,
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	CCS_EXECUTE_HANDLER,
#endif
};

/* Index numbers for special mount operations. */
enum ccs_special_mount {
	CCS_MOUNT_BIND,            /* mount --bind /source /dest   */
	CCS_MOUNT_MOVE,            /* mount --move /old /new       */
	CCS_MOUNT_REMOUNT,         /* mount -o remount /dir        */
	CCS_MOUNT_MAKE_UNBINDABLE, /* mount --make-unbindable /dir */
	CCS_MOUNT_MAKE_PRIVATE,    /* mount --make-private /dir    */
	CCS_MOUNT_MAKE_SLAVE,      /* mount --make-slave /dir      */
	CCS_MOUNT_MAKE_SHARED,     /* mount --make-shared /dir     */
	CCS_MAX_SPECIAL_MOUNT
};

/* Index numbers for domain transition control keywords. */
enum ccs_transition_type {
	/* Do not change this order, */
	CCS_TRANSITION_CONTROL_NO_RESET,
	CCS_TRANSITION_CONTROL_RESET,
	CCS_TRANSITION_CONTROL_NO_INITIALIZE,
	CCS_TRANSITION_CONTROL_INITIALIZE,
	CCS_TRANSITION_CONTROL_NO_KEEP,
	CCS_TRANSITION_CONTROL_KEEP,
	CCS_MAX_TRANSITION_TYPE
};

/* Index numbers for type of numeric values. */
enum ccs_value_type {
	CCS_VALUE_TYPE_INVALID,
	CCS_VALUE_TYPE_DECIMAL,
	CCS_VALUE_TYPE_OCTAL,
	CCS_VALUE_TYPE_HEXADECIMAL,
};

/* Constants definition for internal use. */

/*
 * TOMOYO uses this hash only when appending a string into the string table.
 * Frequency of appending strings is very low. So we don't need large (e.g.
 * 64k) hash size. 256 will be sufficient.
 */
#define CCS_HASH_BITS 8
#define CCS_MAX_HASH (1u << CCS_HASH_BITS)

/*
 * TOMOYO checks only SOCK_STREAM, SOCK_DGRAM, SOCK_RAW, SOCK_SEQPACKET.
 * Therefore, we don't need SOCK_MAX.
 */
#define CCS_SOCK_MAX 6

/* Size of temporary buffer for execve() operation. */
#define CCS_EXEC_TMPSIZE     4096

/* Garbage collector is trying to kfree() this element. */
#define CCS_GC_IN_PROGRESS -1

/* Profile number is an integer between 0 and 255. */
#define CCS_MAX_PROFILES 256

/* Group number is an integer between 0 and 255. */
#define CCS_MAX_ACL_GROUPS 256

/* Current thread is doing open(O_RDONLY | O_TRUNC) ? */
#define CCS_OPEN_FOR_READ_TRUNCATE        1
/* Current thread is doing open(3) ? */
#define CCS_OPEN_FOR_IOCTL_ONLY           2
/* Current thread is doing do_execve() ? */
#define CCS_TASK_IS_IN_EXECVE             4
/* Current thread is running as an execute handler program? */
#define CCS_TASK_IS_EXECUTE_HANDLER       8
/* Current thread is allowed to modify policy via /proc/ccs/ interface? */
#define CCS_TASK_IS_MANAGER              16

/*
 * Retry this request. Returned by ccs_supervisor() if policy violation has
 * occurred in enforcing mode and the userspace daemon decided to retry.
 *
 * We must choose a positive value in order to distinguish "granted" (which is
 * 0) and "rejected" (which is a negative value) and "retry".
 */
#define CCS_RETRY_REQUEST 1

/* Ignore gfp flags which are not supported. */
#ifndef __GFP_HIGHIO
#define __GFP_HIGHIO 0
#endif
#ifndef __GFP_NOWARN
#define __GFP_NOWARN 0
#endif
#ifndef __GFP_NORETRY
#define __GFP_NORETRY 0
#endif
#ifndef __GFP_NOMEMALLOC
#define __GFP_NOMEMALLOC 0
#endif

/* The gfp flags used by TOMOYO. */
#define CCS_GFP_FLAGS (__GFP_WAIT | __GFP_IO | __GFP_HIGHIO | __GFP_NOWARN | \
		       __GFP_NORETRY | __GFP_NOMEMALLOC)

/* Size of read buffer for /proc/ccs/ interface. */
#define CCS_MAX_IO_READ_QUEUE 64

/* Structure definition for internal use. */

/* Common header for holding ACL entries. */
struct ccs_acl_head {
	struct list_head list;
	s8 is_deleted; /* true or false or CCS_GC_IN_PROGRESS */
} __packed;

/* Common header for shared entries. */
struct ccs_shared_acl_head {
	struct list_head list;
	atomic_t users;
} __packed;

/* Common header for individual entries. */
struct ccs_acl_info {
	struct list_head list;
	struct ccs_condition *cond; /* Maybe NULL. */
	s8 is_deleted; /* true or false or CCS_GC_IN_PROGRESS */
	u8 type; /* One of values in "enum ccs_acl_entry_type_index". */
	u16 perm;
} __packed;

/* Structure for holding a word. */
struct ccs_name_union {
	/* Either @filename or @group is NULL. */
	const struct ccs_path_info *filename;
	struct ccs_group *group;
};

/* Structure for holding a number. */
struct ccs_number_union {
	unsigned long values[2];
	struct ccs_group *group; /* Maybe NULL. */
	/* One of values in "enum ccs_value_type". */
	u8 value_type[2];
};

/* Structure for holding an IP address. */
struct ccs_ipaddr_union {
	struct in6_addr ip[2]; /* Big endian. */
	struct ccs_group *group; /* Pointer to address group. */
	bool is_ipv6; /* Valid only if @group == NULL. */
};

/* Structure for "path_group"/"number_group"/"address_group" directive. */
struct ccs_group {
	struct ccs_shared_acl_head head;
	/* Name of group (without leading '@'). */
	const struct ccs_path_info *group_name;
	/*
	 * List of "struct ccs_path_group" or "struct ccs_number_group" or
	 * "struct ccs_address_group".
	 */
	struct list_head member_list;
};

/* Structure for "path_group" directive. */
struct ccs_path_group {
	struct ccs_acl_head head;
	const struct ccs_path_info *member_name;
};

/* Structure for "number_group" directive. */
struct ccs_number_group {
	struct ccs_acl_head head;
	struct ccs_number_union number;
};

/* Structure for "address_group" directive. */
struct ccs_address_group {
	struct ccs_acl_head head;
	/* Structure for holding an IP address. */
	struct ccs_ipaddr_union address;
};

/* Subset of "struct stat". Used by conditional ACL and audit logs. */
struct ccs_mini_stat {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
	kuid_t uid;
	kgid_t gid;
#else
	uid_t uid;
	gid_t gid;
#endif
	ino_t ino;
	umode_t mode;
	dev_t dev;
	dev_t rdev;
};

/* Structure for dumping argv[] and envp[] of "struct linux_binprm". */
struct ccs_page_dump {
	struct page *page;    /* Previously dumped page. */
	char *data;           /* Contents of "page". Size is PAGE_SIZE. */
};

/* Structure for attribute checks in addition to pathname checks. */
struct ccs_obj_info {
	/* True if ccs_get_attributes() was already called, false otherwise. */
	bool validate_done;
	/* True if @stat[] is valid. */
	bool stat_valid[CCS_MAX_PATH_STAT];
	/* First pathname. Initialized with { NULL, NULL } if no path. */
	struct path path1;
	/* Second pathname. Initialized with { NULL, NULL } if no path. */
	struct path path2;
	/*
	 * Information on @path1, @path1's parent directory, @path2, @path2's
	 * parent directory.
	 */
	struct ccs_mini_stat stat[CCS_MAX_PATH_STAT];
	/*
	 * Content of symbolic link to be created. NULL for operations other
	 * than symlink().
	 */
	struct ccs_path_info *symlink_target;
};

/* Structure for entries which follows "struct ccs_condition". */
struct ccs_condition_element {
	/*
	 * Left hand operand. A "struct ccs_argv" for CCS_ARGV_ENTRY, a
	 * "struct ccs_envp" for CCS_ENVP_ENTRY is attached to the tail
	 * of the array of this struct.
	 */
	u8 left;
	/*
	 * Right hand operand. A "struct ccs_number_union" for
	 * CCS_NUMBER_UNION, a "struct ccs_name_union" for CCS_NAME_UNION is
	 * attached to the tail of the array of this struct.
	 */
	u8 right;
	/* Equation operator. True if equals or overlaps, false otherwise. */
	bool equals;
};

/* Structure for optional arguments. */
struct ccs_condition {
	struct ccs_shared_acl_head head;
	u32 size; /* Memory size allocated for this entry. */
	u16 condc; /* Number of conditions in this struct. */
	u16 numbers_count; /* Number of "struct ccs_number_union values". */
	u16 names_count; /* Number of "struct ccs_name_union names". */
	u16 argc; /* Number of "struct ccs_argv". */
	u16 envc; /* Number of "struct ccs_envp". */
	u8 grant_log; /* One of values in "enum ccs_grant_log". */
	bool exec_transit; /* True if transit is for "file execute". */
	const struct ccs_path_info *transit; /* Maybe NULL. */
	/*
	 * struct ccs_condition_element condition[condc];
	 * struct ccs_number_union values[numbers_count];
	 * struct ccs_name_union names[names_count];
	 * struct ccs_argv argv[argc];
	 * struct ccs_envp envp[envc];
	 */
};

struct ccs_execve;
struct ccs_policy_namespace;

/* Structure for request info. */
struct ccs_request_info {
	/*
	 * For holding parameters specific to operations which deal files.
	 * NULL if not dealing files.
	 */
	struct ccs_obj_info *obj;
	/*
	 * For holding parameters specific to execve() request.
	 * NULL if not dealing do_execve().
	 */
	struct ccs_execve *ee;
	/*
	 * For holding parameters.
	 * Pointers in this union are not NULL except path->matched_path.
	 */
	union {
		struct {
			const struct ccs_path_info *filename;
			/*
			 * For using wildcards at ccs_find_next_domain().
			 *
			 * The matched_acl cannot be used because it may refer
			 * a "struct ccs_path_acl" with ->is_group == true.
			 * We want to use exact "struct ccs_path_info" rather
			 * than "struct ccs_path_acl".
			 */
			const struct ccs_path_info *matched_path;
			/* One of values in "enum ccs_path_acl_index". */
			u8 operation;
		} path;
		struct {
			const struct ccs_path_info *filename1;
			const struct ccs_path_info *filename2;
			/* One of values in "enum ccs_path2_acl_index". */
			u8 operation;
		} path2;
		struct {
			const struct ccs_path_info *filename;
			unsigned int mode;
			unsigned int major;
			unsigned int minor;
			/* One of values in "enum ccs_mkdev_acl_index". */
			u8 operation;
		} mkdev;
		struct {
			const struct ccs_path_info *filename;
			unsigned long number;
			/*
			 * One of values in "enum ccs_path_number_acl_index".
			 */
			u8 operation;
		} path_number;
#ifdef CONFIG_CCSECURITY_NETWORK
		struct {
			const u32 *address; /* Big endian. */
			u16 port; /* Host endian. */
			/* One of values smaller than CCS_SOCK_MAX. */
			u8 protocol;
			/* One of values in "enum ccs_network_acl_index". */
			u8 operation;
			bool is_ipv6;
		} inet_network;
		struct {
			const struct ccs_path_info *address;
			/* One of values smaller than CCS_SOCK_MAX. */
			u8 protocol;
			/* One of values in "enum ccs_network_acl_index". */
			u8 operation;
		} unix_network;
#endif
#ifdef CONFIG_CCSECURITY_MISC
		struct {
			const struct ccs_path_info *name;
		} environ;
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
		struct {
			/* One of values in "enum ccs_capability_acl_index". */
			u8 operation;
		} capability;
#endif
#ifdef CONFIG_CCSECURITY_IPC
		struct {
			const char *dest_pattern;
			int sig;
		} signal;
#endif
		struct {
			const struct ccs_path_info *type;
			const struct ccs_path_info *dir;
			const struct ccs_path_info *dev;
			unsigned long flags;
			int need_dev;
		} mount;
#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION
		struct {
			const struct ccs_path_info *domainname;
		} task;
#endif
	} param;
	/*
	 * For updating current->ccs_domain_info at ccs_update_task_domain().
	 * Initialized to NULL at ccs_init_request_info().
	 * Matching "struct ccs_acl_info" is copied if access request was
	 * granted. Re-initialized to NULL at ccs_update_task_domain().
	 */
	struct ccs_acl_info *matched_acl;
	u8 param_type; /* One of values in "enum ccs_acl_entry_type_index". */
	bool granted; /* True if granted, false otherwise. */
	/* True if current thread should not be carried sleep penalty. */
	bool dont_sleep_on_enforce_error;
	/*
	 * For counting number of retries made for this request.
	 * This counter is incremented whenever ccs_supervisor() returned
	 * CCS_RETRY_REQUEST.
	 */
	u8 retry;
	/*
	 * For holding profile number used for this request.
	 * One of values between 0 and CCS_MAX_PROFILES - 1.
	 */
	u8 profile;
	/*
	 * For holding operation mode used for this request.
	 * One of CCS_CONFIG_DISABLED, CCS_CONFIG_LEARNING,
	 * CCS_CONFIG_PERMISSIVE, CCS_CONFIG_ENFORCING.
	 */
	u8 mode;
	/*
	 * For holding operation index used for this request.
	 * Used by ccs_init_request_info() / ccs_get_mode() /
	 * ccs_write_log(). One of values in "enum ccs_mac_index".
	 */
	u8 type;
};

/* Structure for holding a token. */
struct ccs_path_info {
	const char *name;
	u32 hash;          /* = full_name_hash(name, strlen(name)) */
	u16 total_len;     /* = strlen(name)                       */
	u16 const_len;     /* = ccs_const_part_length(name)        */
	bool is_dir;       /* = ccs_strendswith(name, "/")         */
	bool is_patterned; /* = const_len < total_len              */
};

/* Structure for execve() operation. */
struct ccs_execve {
	struct ccs_request_info r;
	struct ccs_obj_info obj;
	struct linux_binprm *bprm;
	struct ccs_domain_info *previous_domain;
	const struct ccs_path_info *transition;
	/* For execute_handler */
	const struct ccs_path_info *handler;
	char *handler_path; /* = kstrdup(handler->name, CCS_GFP_FLAGS) */
	/* For dumping argv[] and envp[]. */
	struct ccs_page_dump dump;
	/* For temporary use. */
	char *tmp; /* Size is CCS_EXEC_TMPSIZE bytes */
};

/* Structure for domain information. */
struct ccs_domain_info {
	struct list_head list;
	struct list_head acl_info_list;
	/* Name of this domain. Never NULL.          */
	const struct ccs_path_info *domainname;
	/* Namespace for this domain. Never NULL. */
	struct ccs_policy_namespace *ns;
	u8 profile;        /* Profile number to use. */
	u8 group;          /* Group number to use.   */
	bool is_deleted;   /* Delete flag.           */
	bool flags[CCS_MAX_DOMAIN_INFO_FLAGS];
};

/*
 * Structure for "reset_domain"/"no_reset_domain"/"initialize_domain"/
 * "no_initialize_domain"/"keep_domain"/"no_keep_domain" keyword.
 */
struct ccs_transition_control {
	struct ccs_acl_head head;
	u8 type; /* One of values in "enum ccs_transition_type" */
	bool is_last_name; /* True if the domainname is ccs_last_word(). */
	const struct ccs_path_info *domainname; /* Maybe NULL */
	const struct ccs_path_info *program;    /* Maybe NULL */
};

/* Structure for "aggregator" keyword. */
struct ccs_aggregator {
	struct ccs_acl_head head;
	const struct ccs_path_info *original_name;
	const struct ccs_path_info *aggregated_name;
};

/* Structure for "deny_autobind" keyword. */
struct ccs_reserved {
	struct ccs_acl_head head;
	struct ccs_number_union port;
};

/* Structure for policy manager. */
struct ccs_manager {
	struct ccs_acl_head head;
	/* A path to program or a domainname. */
	const struct ccs_path_info *manager;
};

/* Structure for argv[]. */
struct ccs_argv {
	unsigned long index;
	const struct ccs_path_info *value;
	bool is_not;
};

/* Structure for envp[]. */
struct ccs_envp {
	const struct ccs_path_info *name;
	const struct ccs_path_info *value;
	bool is_not;
};

/*
 * Structure for "task auto_execute_handler" and "task denied_execute_handler"
 * directive.
 *
 * If "task auto_execute_handler" directive exists and the current process is
 * not an execute handler, all execve() requests are replaced by execve()
 * requests of a program specified by "task auto_execute_handler" directive.
 * If the current process is an execute handler, "task auto_execute_handler"
 * and "task denied_execute_handler" directives are ignored.
 * The program specified by "task execute_handler" validates execve()
 * parameters and executes the original execve() requests if appropriate.
 *
 * "task denied_execute_handler" directive is used only when execve() request
 * was rejected in enforcing mode (i.e. CONFIG::file::execute={ mode=enforcing
 * }). The program specified by "task denied_execute_handler" does whatever it
 * wants to do (e.g. silently terminate, change firewall settings, redirect the
 * user to honey pot etc.).
 */
struct ccs_handler_acl {
	struct ccs_acl_info head;       /* type = CCS_TYPE_*_EXECUTE_HANDLER */
	const struct ccs_path_info *handler; /* Pointer to single pathname.  */
};

/*
 * Structure for "task auto_domain_transition" and
 * "task manual_domain_transition" directive.
 */
struct ccs_task_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_*_TASK_ACL */
	/* Pointer to domainname. */
	const struct ccs_path_info *domainname;
};

/*
 * Structure for "file execute", "file read", "file write", "file append",
 * "file unlink", "file getattr", "file rmdir", "file truncate",
 * "file symlink", "file chroot" and "file unmount" directive.
 */
struct ccs_path_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_PATH_ACL */
	struct ccs_name_union name;
};

/*
 * Structure for "file rename", "file link" and "file pivot_root" directive.
 */
struct ccs_path2_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_PATH2_ACL */
	struct ccs_name_union name1;
	struct ccs_name_union name2;
};

/*
 * Structure for "file create", "file mkdir", "file mkfifo", "file mksock",
 * "file ioctl", "file chmod", "file chown" and "file chgrp" directive.
 */
struct ccs_path_number_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_PATH_NUMBER_ACL */
	struct ccs_name_union name;
	struct ccs_number_union number;
};

/* Structure for "file mkblock" and "file mkchar" directive. */
struct ccs_mkdev_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_MKDEV_ACL */
	struct ccs_name_union name;
	struct ccs_number_union mode;
	struct ccs_number_union major;
	struct ccs_number_union minor;
};

/* Structure for "file mount" directive. */
struct ccs_mount_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_MOUNT_ACL */
	struct ccs_name_union dev_name;
	struct ccs_name_union dir_name;
	struct ccs_name_union fs_type;
	struct ccs_number_union flags;
};

/* Structure for "misc env" directive in domain policy. */
struct ccs_env_acl {
	struct ccs_acl_info head;        /* type = CCS_TYPE_ENV_ACL  */
	const struct ccs_path_info *env; /* environment variable */
};

/* Structure for "capability" directive. */
struct ccs_capability_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_CAPABILITY_ACL */
	u8 operation; /* One of values in "enum ccs_capability_acl_index". */
};

/* Structure for "ipc signal" directive. */
struct ccs_signal_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_SIGNAL_ACL */
	struct ccs_number_union sig;
	/* Pointer to destination pattern. */
	const struct ccs_path_info *domainname;
};

/* Structure for "network inet" directive. */
struct ccs_inet_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_INET_ACL */
	u8 protocol;
	struct ccs_ipaddr_union address;
	struct ccs_number_union port;
};

/* Structure for "network unix" directive. */
struct ccs_unix_acl {
	struct ccs_acl_info head; /* type = CCS_TYPE_UNIX_ACL */
	u8 protocol;
	struct ccs_name_union name;
};

/* Structure for holding string data. */
struct ccs_name {
	struct ccs_shared_acl_head head;
	int size; /* Memory size allocated for this entry. */
	struct ccs_path_info entry;
};

/* Structure for holding a line from /proc/ccs/ interface. */
struct ccs_acl_param {
	char *data; /* Unprocessed data. */
	struct list_head *list; /* List to add or remove. */
	struct ccs_policy_namespace *ns; /* Namespace to use. */
	bool is_delete; /* True if it is a delete request. */
	union ccs_acl_union {
		struct ccs_acl_info acl_info;
		struct ccs_handler_acl handler_acl;
		struct ccs_task_acl task_acl;
		struct ccs_path_acl path_acl;
		struct ccs_path2_acl path2_acl;
		struct ccs_path_number_acl path_number_acl;
		struct ccs_mkdev_acl mkdev_acl;
		struct ccs_mount_acl mount_acl;
		struct ccs_env_acl env_acl;
		struct ccs_capability_acl capability_acl;
		struct ccs_signal_acl signal_acl;
		struct ccs_inet_acl inet_acl;
		struct ccs_unix_acl unix_acl;
		/**/
		struct ccs_acl_head acl_head;
		struct ccs_transition_control transition_control;
		struct ccs_aggregator aggregator;
		struct ccs_reserved reserved;
		struct ccs_manager manager;
		struct ccs_path_group path_group;
		struct ccs_number_group number_group;
		struct ccs_address_group address_group;
	} e;
};

/* Structure for reading/writing policy via /proc/ccs/ interfaces. */
struct ccs_io_buffer {
	/* Exclusive lock for this structure.   */
	struct mutex io_sem;
	char __user *read_user_buf;
	size_t read_user_buf_avail;
	struct {
		struct list_head *ns;
		struct list_head *domain;
		struct list_head *group;
		struct list_head *acl;
		size_t avail;
		unsigned int step;
		unsigned int query_index;
		u16 index;
		u16 cond_index;
		u8 acl_group_index;
		u8 cond_step;
		u8 bit;
		u8 w_pos;
		bool eof;
		bool print_this_domain_only;
		bool print_transition_related_only;
		bool print_cond_part;
		const char *w[CCS_MAX_IO_READ_QUEUE];
	} r;
	struct {
		struct ccs_policy_namespace *ns;
		struct ccs_domain_info *domain;
		size_t avail;
		bool is_delete;
	} w;
	/* Buffer for reading.                  */
	char *read_buf;
	/* Size of read buffer.                 */
	size_t readbuf_size;
	/* Buffer for writing.                  */
	char *write_buf;
	/* Size of write buffer.                */
	size_t writebuf_size;
	/* Type of interface. */
	enum ccs_proc_interface_index type;
	/* Users counter protected by ccs_io_buffer_list_lock. */
	u8 users;
	/* List for telling GC not to kfree() elements. */
	struct list_head list;
};

/* Structure for /proc/ccs/profile interface. */
struct ccs_profile {
	const struct ccs_path_info *comment;
	u8 default_config;
	u8 config[CCS_MAX_MAC_INDEX + CCS_MAX_MAC_CATEGORY_INDEX];
	unsigned int pref[CCS_MAX_PREF];
};

/* Structure for representing YYYY/MM/DD hh/mm/ss. */
struct ccs_time {
	u16 year;
	u8 month;
	u8 day;
	u8 hour;
	u8 min;
	u8 sec;
};

/* Structure for policy namespace. */
struct ccs_policy_namespace {
	/* Profile table. Memory is allocated as needed. */
	struct ccs_profile *profile_ptr[CCS_MAX_PROFILES];
	/* List of "struct ccs_group". */
	struct list_head group_list[CCS_MAX_GROUP];
	/* List of policy. */
	struct list_head policy_list[CCS_MAX_POLICY];
	/* The global ACL referred by "use_group" keyword. */
	struct list_head acl_group[CCS_MAX_ACL_GROUPS];
	/* List for connecting to ccs_namespace_list list. */
	struct list_head namespace_list;
	/* Profile version. Currently only 20100903 is defined. */
	unsigned int profile_version;
	/* Name of this namespace (e.g. "<kernel>", "</usr/sbin/httpd>" ). */
	const char *name;
};

/* Prototype definition for "struct ccsecurity_operations". */

void __init ccs_permission_init(void);
void __init ccs_mm_init(void);

/* Prototype definition for internal use. */

bool ccs_dump_page(struct linux_binprm *bprm, unsigned long pos,
		   struct ccs_page_dump *dump);
bool ccs_memory_ok(const void *ptr, const unsigned int size);
char *ccs_encode(const char *str);
char *ccs_encode2(const char *str, int str_len);
char *ccs_realpath(struct path *path);
const char *ccs_get_exe(void);
const struct ccs_path_info *ccs_get_name(const char *name);
int ccs_audit_log(struct ccs_request_info *r);
int ccs_check_acl(struct ccs_request_info *r);
int ccs_init_request_info(struct ccs_request_info *r, const u8 index);
struct ccs_domain_info *ccs_assign_domain(const char *domainname,
					  const bool transit);
u8 ccs_get_config(const u8 profile, const u8 index);
void *ccs_commit_ok(void *data, const unsigned int size);
void ccs_del_acl(struct list_head *element);
void ccs_del_condition(struct list_head *element);
void ccs_fill_path_info(struct ccs_path_info *ptr);
void ccs_get_attributes(struct ccs_obj_info *obj);
void ccs_notify_gc(struct ccs_io_buffer *head, const bool is_register);
void ccs_transition_failed(const char *domainname);
void ccs_warn_oom(const char *function);
void ccs_write_log(struct ccs_request_info *r, const char *fmt, ...)
	__printf(2, 3);

/* Variable definition for internal use. */

extern bool ccs_policy_loaded;
extern const char * const ccs_dif[CCS_MAX_DOMAIN_INFO_FLAGS];
extern const u8 ccs_c2mac[CCS_MAX_CAPABILITY_INDEX];
extern const u8 ccs_pn2mac[CCS_MAX_PATH_NUMBER_OPERATION];
extern const u8 ccs_pnnn2mac[CCS_MAX_MKDEV_OPERATION];
extern const u8 ccs_pp2mac[CCS_MAX_PATH2_OPERATION];
extern struct ccs_domain_info ccs_kernel_domain;
extern struct list_head ccs_condition_list;
extern struct list_head ccs_domain_list;
extern struct list_head ccs_name_list[CCS_MAX_HASH];
extern struct list_head ccs_namespace_list;
extern struct mutex ccs_policy_lock;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
extern struct srcu_struct ccs_ss;
#endif
extern unsigned int ccs_memory_quota[CCS_MAX_MEMORY_STAT];
extern unsigned int ccs_memory_used[CCS_MAX_MEMORY_STAT];

/* Inlined functions for internal use. */

/**
 * ccs_pathcmp - strcmp() for "struct ccs_path_info" structure.
 *
 * @a: Pointer to "struct ccs_path_info".
 * @b: Pointer to "struct ccs_path_info".
 *
 * Returns true if @a != @b, false otherwise.
 */
static inline bool ccs_pathcmp(const struct ccs_path_info *a,
			       const struct ccs_path_info *b)
{
	return a->hash != b->hash || strcmp(a->name, b->name);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)

/**
 * ccs_read_lock - Take lock for protecting policy.
 *
 * Returns index number for ccs_read_unlock().
 */
static inline int ccs_read_lock(void)
{
	return srcu_read_lock(&ccs_ss);
}

/**
 * ccs_read_unlock - Release lock for protecting policy.
 *
 * @idx: Index number returned by ccs_read_lock().
 *
 * Returns nothing.
 */
static inline void ccs_read_unlock(const int idx)
{
	srcu_read_unlock(&ccs_ss, idx);
}

#else

int ccs_lock(void);
void ccs_unlock(const int idx);

/**
 * ccs_read_lock - Take lock for protecting policy.
 *
 * Returns index number for ccs_read_unlock().
 */
static inline int ccs_read_lock(void)
{
	return ccs_lock();
}

/**
 * ccs_read_unlock - Release lock for protecting policy.
 *
 * @idx: Index number returned by ccs_read_lock().
 *
 * Returns nothing.
 */
static inline void ccs_read_unlock(const int idx)
{
	ccs_unlock(idx);
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)

/**
 * ccs_tasklist_lock - Take lock for reading list of "struct task_struct".
 *
 * Returns nothing.
 */
static inline void ccs_tasklist_lock(void)
{
	rcu_read_lock();
}

/**
 * ccs_tasklist_unlock - Release lock for reading list of "struct task_struct".
 *
 * Returns nothing.
 */
static inline void ccs_tasklist_unlock(void)
{
	rcu_read_unlock();
}

#else

/**
 * ccs_tasklist_lock - Take lock for reading list of "struct task_struct".
 *
 * Returns nothing.
 */
static inline void ccs_tasklist_lock(void)
{
	read_lock(&tasklist_lock);
}

/**
 * ccs_tasklist_unlock - Release lock for reading list of "struct task_struct".
 *
 * Returns nothing.
 */
static inline void ccs_tasklist_unlock(void)
{
	read_unlock(&tasklist_lock);
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)

/**
 * ccs_sys_getppid - Copy of getppid().
 *
 * Returns parent process's PID.
 *
 * Alpha does not have getppid() defined. To be able to build this module on
 * Alpha, I have to copy getppid() from kernel/timer.c.
 */
static inline pid_t ccs_sys_getppid(void)
{
	pid_t pid;
	rcu_read_lock();
	pid = task_tgid_vnr(rcu_dereference(current->real_parent));
	rcu_read_unlock();
	return pid;
}

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 0)

/**
 * ccs_sys_getppid - Copy of getppid().
 *
 * Returns parent process's PID.
 *
 * This function was rewritten to use RCU in 2.6.16.34. However, distributors
 * which use earlier kernels (e.g. 2.6.8/2.6.9) did not backport the bugfix.
 * Therefore, I'm using code for 2.6.16.34 for earlier kernels.
 */
static inline pid_t ccs_sys_getppid(void)
{
	pid_t pid;
	rcu_read_lock();
#if (defined(RHEL_MAJOR) && RHEL_MAJOR == 5) || (defined(AX_MAJOR) && AX_MAJOR == 3)
	pid = rcu_dereference(current->parent)->tgid;
#elif defined(CONFIG_UTRACE)
	/*
	 * RHEL 5.0 kernel does not have RHEL_MAJOR/RHEL_MINOR defined.
	 * Assume RHEL 5.0 if CONFIG_UTRACE is defined.
	 */
	pid = rcu_dereference(current->parent)->tgid;
#else
	pid = rcu_dereference(current->real_parent)->tgid;
#endif
	rcu_read_unlock();
	return pid;
}

#else

/**
 * ccs_sys_getppid - Copy of getppid().
 *
 * Returns parent process's PID.
 *
 * I can't use code for 2.6.16.34 for 2.4 kernels because 2.4 kernels does not
 * have RCU. Therefore, I'm using pessimistic lock (i.e. tasklist_lock
 * spinlock).
 */
static inline pid_t ccs_sys_getppid(void)
{
	pid_t pid;
	read_lock(&tasklist_lock);
#ifdef TASK_DEAD
	pid = current->group_leader->real_parent->tgid;
#else
	pid = current->p_opptr->pid;
#endif
	read_unlock(&tasklist_lock);
	return pid;
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)

/**
 * ccs_sys_getpid - Copy of getpid().
 *
 * Returns current thread's PID.
 *
 * Alpha does not have getpid() defined. To be able to build this module on
 * Alpha, I have to copy getpid() from kernel/timer.c.
 */
static inline pid_t ccs_sys_getpid(void)
{
	return task_tgid_vnr(current);
}

#else

/**
 * ccs_sys_getpid - Copy of getpid().
 *
 * Returns current thread's PID.
 */
static inline pid_t ccs_sys_getpid(void)
{
	return current->tgid;
}

#endif

/**
 * ccs_get_mode - Get mode for specified functionality.
 *
 * @profile: Profile number.
 * @index:   Functionality number.
 *
 * Returns mode.
 */
static inline u8 ccs_get_mode(const u8 profile, const u8 index)
{
	return ccs_get_config(profile, index) & (CCS_CONFIG_MAX_MODE - 1);
}

#if defined(CONFIG_SLOB)

/**
 * ccs_round2 - Round up to power of 2 for calculating memory usage.
 *
 * @size: Size to be rounded up.
 *
 * Returns @size.
 *
 * Since SLOB does not round up, this function simply returns @size.
 */
static inline int ccs_round2(size_t size)
{
	return size;
}

#else

/**
 * ccs_round2 - Round up to power of 2 for calculating memory usage.
 *
 * @size: Size to be rounded up.
 *
 * Returns rounded size.
 *
 * Strictly speaking, SLAB may be able to allocate (e.g.) 96 bytes instead of
 * (e.g.) 128 bytes.
 */
static inline int ccs_round2(size_t size)
{
#if PAGE_SIZE == 4096
	size_t bsize = 32;
#else
	size_t bsize = 64;
#endif
	if (!size)
		return 0;
	while (size > bsize)
		bsize <<= 1;
	return bsize;
}

#endif

/**
 * ccs_put_condition - Drop reference on "struct ccs_condition".
 *
 * @cond: Pointer to "struct ccs_condition". Maybe NULL.
 *
 * Returns nothing.
 */
static inline void ccs_put_condition(struct ccs_condition *cond)
{
	if (cond)
		atomic_dec(&cond->head.users);
}

/**
 * ccs_put_group - Drop reference on "struct ccs_group".
 *
 * @group: Pointer to "struct ccs_group". Maybe NULL.
 *
 * Returns nothing.
 */
static inline void ccs_put_group(struct ccs_group *group)
{
	if (group)
		atomic_dec(&group->head.users);
}

/**
 * ccs_put_name - Drop reference on "struct ccs_name".
 *
 * @name: Pointer to "struct ccs_path_info". Maybe NULL.
 *
 * Returns nothing.
 */
static inline void ccs_put_name(const struct ccs_path_info *name)
{
	if (name)
		atomic_dec(&container_of(name, struct ccs_name, entry)->
			   head.users);
}

/* For importing variables and functions. */
extern const struct ccsecurity_exports ccsecurity_exports;

#ifdef CONFIG_CCSECURITY_USE_EXTERNAL_TASK_SECURITY

/*
 * Structure for holding "struct ccs_domain_info *" and "struct ccs_execve *"
 * and "u32 ccs_flags" for each "struct task_struct".
 *
 * "struct ccs_domain_info *" and "u32 ccs_flags" for each "struct task_struct"
 * are maintained outside that "struct task_struct". Therefore, ccs_security
 * != task_struct . This keeps KABI for distributor's prebuilt kernels but
 * entails slow access.
 *
 * Memory for this structure is allocated when current thread tries to access
 * it. Therefore, if memory allocation failed, current thread will be killed by
 * SIGKILL. Note that if current->pid == 1, sending SIGKILL won't work.
 */
struct ccs_security {
	struct list_head list;
	const struct task_struct *task;
	struct ccs_domain_info *ccs_domain_info;
	u32 ccs_flags;
	struct rcu_head rcu;
};

#define CCS_TASK_SECURITY_HASH_BITS 12
#define CCS_MAX_TASK_SECURITY_HASH (1u << CCS_TASK_SECURITY_HASH_BITS)
extern struct list_head ccs_task_security_list[CCS_MAX_TASK_SECURITY_HASH];

struct ccs_security *ccs_find_task_security(const struct task_struct *task);

/**
 * ccs_current_security - Get "struct ccs_security" for current thread.
 *
 * Returns pointer to "struct ccs_security" for current thread.
 */
static inline struct ccs_security *ccs_current_security(void)
{
	return ccs_find_task_security(current);
}

/**
 * ccs_task_domain - Get "struct ccs_domain_info" for specified thread.
 *
 * @task: Pointer to "struct task_struct".
 *
 * Returns pointer to "struct ccs_security" for specified thread.
 */
static inline struct ccs_domain_info *ccs_task_domain(struct task_struct *task)
{
	struct ccs_domain_info *domain;
	rcu_read_lock();
	domain = ccs_find_task_security(task)->ccs_domain_info;
	rcu_read_unlock();
	return domain;
}

/**
 * ccs_current_domain - Get "struct ccs_domain_info" for current thread.
 *
 * Returns pointer to "struct ccs_domain_info" for current thread.
 */
static inline struct ccs_domain_info *ccs_current_domain(void)
{
	return ccs_find_task_security(current)->ccs_domain_info;
}

/**
 * ccs_task_flags - Get flags for specified thread.
 *
 * @task: Pointer to "struct task_struct".
 *
 * Returns flags for specified thread.
 */
static inline u32 ccs_task_flags(struct task_struct *task)
{
	u32 ccs_flags;
	rcu_read_lock();
	ccs_flags = ccs_find_task_security(task)->ccs_flags;
	rcu_read_unlock();
	return ccs_flags;
}

/**
 * ccs_current_flags - Get flags for current thread.
 *
 * Returns flags for current thread.
 */
static inline u32 ccs_current_flags(void)
{
	return ccs_find_task_security(current)->ccs_flags;
}

#else

/*
 * "struct ccs_domain_info *" and "u32 ccs_flags" for each "struct task_struct"
 * are maintained inside that "struct task_struct". Therefore, ccs_security ==
 * task_struct . This allows fast access but breaks KABI checks for
 * distributor's prebuilt kernels due to changes in "struct task_struct".
 */
#define ccs_security task_struct

/**
 * ccs_find_task_security - Find "struct ccs_security" for given task.
 *
 * @task: Pointer to "struct task_struct".
 *
 * Returns pointer to "struct ccs_security".
 */
static inline struct ccs_security *ccs_find_task_security(struct task_struct *
							  task)
{
	return task;
}

/**
 * ccs_current_security - Get "struct ccs_security" for current thread.
 *
 * Returns pointer to "struct ccs_security" for current thread.
 */
static inline struct ccs_security *ccs_current_security(void)
{
	return ccs_find_task_security(current);
}

/**
 * ccs_task_domain - Get "struct ccs_domain_info" for specified thread.
 *
 * @task: Pointer to "struct task_struct".
 *
 * Returns pointer to "struct ccs_security" for specified thread.
 */
static inline struct ccs_domain_info *ccs_task_domain(struct task_struct *task)
{
	struct ccs_domain_info *domain = task->ccs_domain_info;
	return domain ? domain : &ccs_kernel_domain;
}

/**
 * ccs_current_domain - Get "struct ccs_domain_info" for current thread.
 *
 * Returns pointer to "struct ccs_domain_info" for current thread.
 *
 * If current thread does not belong to a domain (which is true for initial
 * init_task in order to hide ccs_kernel_domain from this module),
 * current thread enters into ccs_kernel_domain.
 */
static inline struct ccs_domain_info *ccs_current_domain(void)
{
	struct task_struct *task = current;
	if (!task->ccs_domain_info)
		task->ccs_domain_info = &ccs_kernel_domain;
	return task->ccs_domain_info;
}

/**
 * ccs_task_flags - Get flags for specified thread.
 *
 * @task: Pointer to "struct task_struct".
 *
 * Returns flags for specified thread.
 */
static inline u32 ccs_task_flags(struct task_struct *task)
{
	return ccs_find_task_security(task)->ccs_flags;
}

/**
 * ccs_current_flags - Get flags for current thread.
 *
 * Returns flags for current thread.
 */
static inline u32 ccs_current_flags(void)
{
	return ccs_find_task_security(current)->ccs_flags;
}

#endif

/**
 * ccs_current_namespace - Get "struct ccs_policy_namespace" for current thread.
 *
 * Returns pointer to "struct ccs_policy_namespace" for current thread.
 */
static inline struct ccs_policy_namespace *ccs_current_namespace(void)
{
	return ccs_current_domain()->ns;
}

#endif
