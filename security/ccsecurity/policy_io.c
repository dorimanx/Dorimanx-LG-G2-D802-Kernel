/*
 * security/ccsecurity/policy_io.c
 *
 * Copyright (C) 2005-2012  NTT DATA CORPORATION
 *
 * Version: 1.8.3+   2012/05/05
 */

#include "internal.h"

/***** SECTION1: Constants definition *****/

/* Define this to enable debug mode. */
/* #define DEBUG_CONDITION */

#ifdef DEBUG_CONDITION
#define dprintk printk
#else
#define dprintk(...) do { } while (0)
#endif

/* Mapping table from "enum ccs_mac_index" to "enum ccs_mac_category_index". */
static const u8 ccs_index2category[CCS_MAX_MAC_INDEX] = {
	/* CONFIG::file group */
	[CCS_MAC_FILE_EXECUTE]    = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_OPEN]       = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_CREATE]     = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_UNLINK]     = CCS_MAC_CATEGORY_FILE,
#ifdef CONFIG_CCSECURITY_FILE_GETATTR
	[CCS_MAC_FILE_GETATTR]    = CCS_MAC_CATEGORY_FILE,
#endif
	[CCS_MAC_FILE_MKDIR]      = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_RMDIR]      = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_MKFIFO]     = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_MKSOCK]     = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_TRUNCATE]   = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_SYMLINK]    = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_MKBLOCK]    = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_MKCHAR]     = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_LINK]       = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_RENAME]     = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_CHMOD]      = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_CHOWN]      = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_CHGRP]      = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_IOCTL]      = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_CHROOT]     = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_MOUNT]      = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_UMOUNT]     = CCS_MAC_CATEGORY_FILE,
	[CCS_MAC_FILE_PIVOT_ROOT] = CCS_MAC_CATEGORY_FILE,
#ifdef CONFIG_CCSECURITY_MISC
	/* CONFIG::misc group */
	[CCS_MAC_ENVIRON]         = CCS_MAC_CATEGORY_MISC,
#endif
#ifdef CONFIG_CCSECURITY_NETWORK
	/* CONFIG::network group */
	[CCS_MAC_NETWORK_INET_STREAM_BIND]       = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_INET_STREAM_LISTEN]     = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_INET_STREAM_CONNECT]    = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_INET_STREAM_ACCEPT]     = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_INET_DGRAM_BIND]        = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_INET_DGRAM_SEND]        = CCS_MAC_CATEGORY_NETWORK,
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	[CCS_MAC_NETWORK_INET_DGRAM_RECV]        = CCS_MAC_CATEGORY_NETWORK,
#endif
	[CCS_MAC_NETWORK_INET_RAW_BIND]          = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_INET_RAW_SEND]          = CCS_MAC_CATEGORY_NETWORK,
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	[CCS_MAC_NETWORK_INET_RAW_RECV]          = CCS_MAC_CATEGORY_NETWORK,
#endif
	[CCS_MAC_NETWORK_UNIX_STREAM_BIND]       = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_UNIX_STREAM_LISTEN]     = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_UNIX_STREAM_CONNECT]    = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_UNIX_STREAM_ACCEPT]     = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_UNIX_DGRAM_BIND]        = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_UNIX_DGRAM_SEND]        = CCS_MAC_CATEGORY_NETWORK,
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	[CCS_MAC_NETWORK_UNIX_DGRAM_RECV]        = CCS_MAC_CATEGORY_NETWORK,
#endif
	[CCS_MAC_NETWORK_UNIX_SEQPACKET_BIND]    = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_UNIX_SEQPACKET_LISTEN]  = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_UNIX_SEQPACKET_CONNECT] = CCS_MAC_CATEGORY_NETWORK,
	[CCS_MAC_NETWORK_UNIX_SEQPACKET_ACCEPT]  = CCS_MAC_CATEGORY_NETWORK,
#endif
#ifdef CONFIG_CCSECURITY_IPC
	/* CONFIG::ipc group */
	[CCS_MAC_SIGNAL]          = CCS_MAC_CATEGORY_IPC,
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	/* CONFIG::capability group */
	[CCS_MAC_CAPABILITY_USE_ROUTE_SOCKET]  = CCS_MAC_CATEGORY_CAPABILITY,
	[CCS_MAC_CAPABILITY_USE_PACKET_SOCKET] = CCS_MAC_CATEGORY_CAPABILITY,
	[CCS_MAC_CAPABILITY_SYS_REBOOT]        = CCS_MAC_CATEGORY_CAPABILITY,
	[CCS_MAC_CAPABILITY_SYS_VHANGUP]       = CCS_MAC_CATEGORY_CAPABILITY,
	[CCS_MAC_CAPABILITY_SYS_SETTIME]       = CCS_MAC_CATEGORY_CAPABILITY,
	[CCS_MAC_CAPABILITY_SYS_NICE]          = CCS_MAC_CATEGORY_CAPABILITY,
	[CCS_MAC_CAPABILITY_SYS_SETHOSTNAME]   = CCS_MAC_CATEGORY_CAPABILITY,
	[CCS_MAC_CAPABILITY_USE_KERNEL_MODULE] = CCS_MAC_CATEGORY_CAPABILITY,
	[CCS_MAC_CAPABILITY_SYS_KEXEC_LOAD]    = CCS_MAC_CATEGORY_CAPABILITY,
	[CCS_MAC_CAPABILITY_SYS_PTRACE]        = CCS_MAC_CATEGORY_CAPABILITY,
#endif
};

/* String table for operation mode. */
static const char * const ccs_mode[CCS_CONFIG_MAX_MODE] = {
	[CCS_CONFIG_DISABLED]   = "disabled",
	[CCS_CONFIG_LEARNING]   = "learning",
	[CCS_CONFIG_PERMISSIVE] = "permissive",
	[CCS_CONFIG_ENFORCING]  = "enforcing"
};

/* String table for /proc/ccs/profile interface. */
static const char * const ccs_mac_keywords[CCS_MAX_MAC_INDEX
					   + CCS_MAX_MAC_CATEGORY_INDEX] = {
	/* CONFIG::file group */
	[CCS_MAC_FILE_EXECUTE]    = "execute",
	[CCS_MAC_FILE_OPEN]       = "open",
	[CCS_MAC_FILE_CREATE]     = "create",
	[CCS_MAC_FILE_UNLINK]     = "unlink",
#ifdef CONFIG_CCSECURITY_FILE_GETATTR
	[CCS_MAC_FILE_GETATTR]    = "getattr",
#endif
	[CCS_MAC_FILE_MKDIR]      = "mkdir",
	[CCS_MAC_FILE_RMDIR]      = "rmdir",
	[CCS_MAC_FILE_MKFIFO]     = "mkfifo",
	[CCS_MAC_FILE_MKSOCK]     = "mksock",
	[CCS_MAC_FILE_TRUNCATE]   = "truncate",
	[CCS_MAC_FILE_SYMLINK]    = "symlink",
	[CCS_MAC_FILE_MKBLOCK]    = "mkblock",
	[CCS_MAC_FILE_MKCHAR]     = "mkchar",
	[CCS_MAC_FILE_LINK]       = "link",
	[CCS_MAC_FILE_RENAME]     = "rename",
	[CCS_MAC_FILE_CHMOD]      = "chmod",
	[CCS_MAC_FILE_CHOWN]      = "chown",
	[CCS_MAC_FILE_CHGRP]      = "chgrp",
	[CCS_MAC_FILE_IOCTL]      = "ioctl",
	[CCS_MAC_FILE_CHROOT]     = "chroot",
	[CCS_MAC_FILE_MOUNT]      = "mount",
	[CCS_MAC_FILE_UMOUNT]     = "unmount",
	[CCS_MAC_FILE_PIVOT_ROOT] = "pivot_root",
#ifdef CONFIG_CCSECURITY_MISC
	/* CONFIG::misc group */
	[CCS_MAC_ENVIRON] = "env",
#endif
#ifdef CONFIG_CCSECURITY_NETWORK
	/* CONFIG::network group */
	[CCS_MAC_NETWORK_INET_STREAM_BIND]       = "inet_stream_bind",
	[CCS_MAC_NETWORK_INET_STREAM_LISTEN]     = "inet_stream_listen",
	[CCS_MAC_NETWORK_INET_STREAM_CONNECT]    = "inet_stream_connect",
	[CCS_MAC_NETWORK_INET_STREAM_ACCEPT]     = "inet_stream_accept",
	[CCS_MAC_NETWORK_INET_DGRAM_BIND]        = "inet_dgram_bind",
	[CCS_MAC_NETWORK_INET_DGRAM_SEND]        = "inet_dgram_send",
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	[CCS_MAC_NETWORK_INET_DGRAM_RECV]        = "inet_dgram_recv",
#endif
	[CCS_MAC_NETWORK_INET_RAW_BIND]          = "inet_raw_bind",
	[CCS_MAC_NETWORK_INET_RAW_SEND]          = "inet_raw_send",
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	[CCS_MAC_NETWORK_INET_RAW_RECV]          = "inet_raw_recv",
#endif
	[CCS_MAC_NETWORK_UNIX_STREAM_BIND]       = "unix_stream_bind",
	[CCS_MAC_NETWORK_UNIX_STREAM_LISTEN]     = "unix_stream_listen",
	[CCS_MAC_NETWORK_UNIX_STREAM_CONNECT]    = "unix_stream_connect",
	[CCS_MAC_NETWORK_UNIX_STREAM_ACCEPT]     = "unix_stream_accept",
	[CCS_MAC_NETWORK_UNIX_DGRAM_BIND]        = "unix_dgram_bind",
	[CCS_MAC_NETWORK_UNIX_DGRAM_SEND]        = "unix_dgram_send",
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	[CCS_MAC_NETWORK_UNIX_DGRAM_RECV]        = "unix_dgram_recv",
#endif
	[CCS_MAC_NETWORK_UNIX_SEQPACKET_BIND]    = "unix_seqpacket_bind",
	[CCS_MAC_NETWORK_UNIX_SEQPACKET_LISTEN]  = "unix_seqpacket_listen",
	[CCS_MAC_NETWORK_UNIX_SEQPACKET_CONNECT] = "unix_seqpacket_connect",
	[CCS_MAC_NETWORK_UNIX_SEQPACKET_ACCEPT]  = "unix_seqpacket_accept",
#endif
#ifdef CONFIG_CCSECURITY_IPC
	/* CONFIG::ipc group */
	[CCS_MAC_SIGNAL] = "signal",
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	/* CONFIG::capability group */
	[CCS_MAC_CAPABILITY_USE_ROUTE_SOCKET]  = "use_route",
	[CCS_MAC_CAPABILITY_USE_PACKET_SOCKET] = "use_packet",
	[CCS_MAC_CAPABILITY_SYS_REBOOT]        = "SYS_REBOOT",
	[CCS_MAC_CAPABILITY_SYS_VHANGUP]       = "SYS_VHANGUP",
	[CCS_MAC_CAPABILITY_SYS_SETTIME]       = "SYS_TIME",
	[CCS_MAC_CAPABILITY_SYS_NICE]          = "SYS_NICE",
	[CCS_MAC_CAPABILITY_SYS_SETHOSTNAME]   = "SYS_SETHOSTNAME",
	[CCS_MAC_CAPABILITY_USE_KERNEL_MODULE] = "use_kernel_module",
	[CCS_MAC_CAPABILITY_SYS_KEXEC_LOAD]    = "SYS_KEXEC_LOAD",
	[CCS_MAC_CAPABILITY_SYS_PTRACE]        = "SYS_PTRACE",
#endif
	/* CONFIG group */
	[CCS_MAX_MAC_INDEX + CCS_MAC_CATEGORY_FILE]       = "file",
#ifdef CONFIG_CCSECURITY_NETWORK
	[CCS_MAX_MAC_INDEX + CCS_MAC_CATEGORY_NETWORK]    = "network",
#endif
#ifdef CONFIG_CCSECURITY_MISC
	[CCS_MAX_MAC_INDEX + CCS_MAC_CATEGORY_MISC]       = "misc",
#endif
#ifdef CONFIG_CCSECURITY_IPC
	[CCS_MAX_MAC_INDEX + CCS_MAC_CATEGORY_IPC]        = "ipc",
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	[CCS_MAX_MAC_INDEX + CCS_MAC_CATEGORY_CAPABILITY] = "capability",
#endif
};

/* String table for path operation. */
static const char * const ccs_path_keyword[CCS_MAX_PATH_OPERATION] = {
	[CCS_TYPE_EXECUTE]    = "execute",
	[CCS_TYPE_READ]       = "read",
	[CCS_TYPE_WRITE]      = "write",
	[CCS_TYPE_APPEND]     = "append",
	[CCS_TYPE_UNLINK]     = "unlink",
#ifdef CONFIG_CCSECURITY_FILE_GETATTR
	[CCS_TYPE_GETATTR]    = "getattr",
#endif
	[CCS_TYPE_RMDIR]      = "rmdir",
	[CCS_TYPE_TRUNCATE]   = "truncate",
	[CCS_TYPE_SYMLINK]    = "symlink",
	[CCS_TYPE_CHROOT]     = "chroot",
	[CCS_TYPE_UMOUNT]     = "unmount",
};

#ifdef CONFIG_CCSECURITY_NETWORK

/* String table for socket's operation. */
static const char * const ccs_socket_keyword[CCS_MAX_NETWORK_OPERATION] = {
	[CCS_NETWORK_BIND]    = "bind",
	[CCS_NETWORK_LISTEN]  = "listen",
	[CCS_NETWORK_CONNECT] = "connect",
	[CCS_NETWORK_ACCEPT]  = "accept",
	[CCS_NETWORK_SEND]    = "send",
#ifdef CONFIG_CCSECURITY_NETWORK_RECVMSG
	[CCS_NETWORK_RECV]    = "recv",
#endif
};

/* String table for socket's protocols. */
static const char * const ccs_proto_keyword[CCS_SOCK_MAX] = {
	[SOCK_STREAM]    = "stream",
	[SOCK_DGRAM]     = "dgram",
	[SOCK_RAW]       = "raw",
	[SOCK_SEQPACKET] = "seqpacket",
	[0] = " ", /* Dummy for avoiding NULL pointer dereference. */
	[4] = " ", /* Dummy for avoiding NULL pointer dereference. */
};

#endif

/* String table for categories. */
static const char * const ccs_category_keywords[CCS_MAX_MAC_CATEGORY_INDEX] = {
	[CCS_MAC_CATEGORY_FILE]       = "file",
#ifdef CONFIG_CCSECURITY_NETWORK
	[CCS_MAC_CATEGORY_NETWORK]    = "network",
#endif
#ifdef CONFIG_CCSECURITY_MISC
	[CCS_MAC_CATEGORY_MISC]       = "misc",
#endif
#ifdef CONFIG_CCSECURITY_IPC
	[CCS_MAC_CATEGORY_IPC]        = "ipc",
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	[CCS_MAC_CATEGORY_CAPABILITY] = "capability",
#endif
};

/* String table for conditions. */
static const char * const ccs_condition_keyword[CCS_MAX_CONDITION_KEYWORD] = {
	[CCS_TASK_UID]             = "task.uid",
	[CCS_TASK_EUID]            = "task.euid",
	[CCS_TASK_SUID]            = "task.suid",
	[CCS_TASK_FSUID]           = "task.fsuid",
	[CCS_TASK_GID]             = "task.gid",
	[CCS_TASK_EGID]            = "task.egid",
	[CCS_TASK_SGID]            = "task.sgid",
	[CCS_TASK_FSGID]           = "task.fsgid",
	[CCS_TASK_PID]             = "task.pid",
	[CCS_TASK_PPID]            = "task.ppid",
	[CCS_EXEC_ARGC]            = "exec.argc",
	[CCS_EXEC_ENVC]            = "exec.envc",
	[CCS_TYPE_IS_SOCKET]       = "socket",
	[CCS_TYPE_IS_SYMLINK]      = "symlink",
	[CCS_TYPE_IS_FILE]         = "file",
	[CCS_TYPE_IS_BLOCK_DEV]    = "block",
	[CCS_TYPE_IS_DIRECTORY]    = "directory",
	[CCS_TYPE_IS_CHAR_DEV]     = "char",
	[CCS_TYPE_IS_FIFO]         = "fifo",
	[CCS_MODE_SETUID]          = "setuid",
	[CCS_MODE_SETGID]          = "setgid",
	[CCS_MODE_STICKY]          = "sticky",
	[CCS_MODE_OWNER_READ]      = "owner_read",
	[CCS_MODE_OWNER_WRITE]     = "owner_write",
	[CCS_MODE_OWNER_EXECUTE]   = "owner_execute",
	[CCS_MODE_GROUP_READ]      = "group_read",
	[CCS_MODE_GROUP_WRITE]     = "group_write",
	[CCS_MODE_GROUP_EXECUTE]   = "group_execute",
	[CCS_MODE_OTHERS_READ]     = "others_read",
	[CCS_MODE_OTHERS_WRITE]    = "others_write",
	[CCS_MODE_OTHERS_EXECUTE]  = "others_execute",
	[CCS_TASK_TYPE]            = "task.type",
	[CCS_TASK_EXECUTE_HANDLER] = "execute_handler",
	[CCS_EXEC_REALPATH]        = "exec.realpath",
	[CCS_SYMLINK_TARGET]       = "symlink.target",
	[CCS_PATH1_UID]            = "path1.uid",
	[CCS_PATH1_GID]            = "path1.gid",
	[CCS_PATH1_INO]            = "path1.ino",
	[CCS_PATH1_MAJOR]          = "path1.major",
	[CCS_PATH1_MINOR]          = "path1.minor",
	[CCS_PATH1_PERM]           = "path1.perm",
	[CCS_PATH1_TYPE]           = "path1.type",
	[CCS_PATH1_DEV_MAJOR]      = "path1.dev_major",
	[CCS_PATH1_DEV_MINOR]      = "path1.dev_minor",
	[CCS_PATH2_UID]            = "path2.uid",
	[CCS_PATH2_GID]            = "path2.gid",
	[CCS_PATH2_INO]            = "path2.ino",
	[CCS_PATH2_MAJOR]          = "path2.major",
	[CCS_PATH2_MINOR]          = "path2.minor",
	[CCS_PATH2_PERM]           = "path2.perm",
	[CCS_PATH2_TYPE]           = "path2.type",
	[CCS_PATH2_DEV_MAJOR]      = "path2.dev_major",
	[CCS_PATH2_DEV_MINOR]      = "path2.dev_minor",
	[CCS_PATH1_PARENT_UID]     = "path1.parent.uid",
	[CCS_PATH1_PARENT_GID]     = "path1.parent.gid",
	[CCS_PATH1_PARENT_INO]     = "path1.parent.ino",
	[CCS_PATH1_PARENT_PERM]    = "path1.parent.perm",
	[CCS_PATH2_PARENT_UID]     = "path2.parent.uid",
	[CCS_PATH2_PARENT_GID]     = "path2.parent.gid",
	[CCS_PATH2_PARENT_INO]     = "path2.parent.ino",
	[CCS_PATH2_PARENT_PERM]    = "path2.parent.perm",
};

/* String table for PREFERENCE keyword. */
static const char * const ccs_pref_keywords[CCS_MAX_PREF] = {
	[CCS_PREF_MAX_AUDIT_LOG]      = "max_audit_log",
	[CCS_PREF_MAX_LEARNING_ENTRY] = "max_learning_entry",
	[CCS_PREF_ENFORCING_PENALTY]  = "enforcing_penalty",
};

/* String table for domain flags. */
const char * const ccs_dif[CCS_MAX_DOMAIN_INFO_FLAGS] = {
	[CCS_DIF_QUOTA_WARNED]      = "quota_exceeded\n",
	[CCS_DIF_TRANSITION_FAILED] = "transition_failed\n",
};

/* String table for domain transition control keywords. */
static const char * const ccs_transition_type[CCS_MAX_TRANSITION_TYPE] = {
	[CCS_TRANSITION_CONTROL_NO_RESET]      = "no_reset_domain ",
	[CCS_TRANSITION_CONTROL_RESET]         = "reset_domain ",
	[CCS_TRANSITION_CONTROL_NO_INITIALIZE] = "no_initialize_domain ",
	[CCS_TRANSITION_CONTROL_INITIALIZE]    = "initialize_domain ",
	[CCS_TRANSITION_CONTROL_NO_KEEP]       = "no_keep_domain ",
	[CCS_TRANSITION_CONTROL_KEEP]          = "keep_domain ",
};

/* String table for grouping keywords. */
static const char * const ccs_group_name[CCS_MAX_GROUP] = {
	[CCS_PATH_GROUP]    = "path_group ",
	[CCS_NUMBER_GROUP]  = "number_group ",
#ifdef CONFIG_CCSECURITY_NETWORK
	[CCS_ADDRESS_GROUP] = "address_group ",
#endif
};

/* String table for /proc/ccs/stat interface. */
static const char * const ccs_policy_headers[CCS_MAX_POLICY_STAT] = {
	[CCS_STAT_POLICY_UPDATES]    = "update:",
	[CCS_STAT_POLICY_LEARNING]   = "violation in learning mode:",
	[CCS_STAT_POLICY_PERMISSIVE] = "violation in permissive mode:",
	[CCS_STAT_POLICY_ENFORCING]  = "violation in enforcing mode:",
};

/* String table for /proc/ccs/stat interface. */
static const char * const ccs_memory_headers[CCS_MAX_MEMORY_STAT] = {
	[CCS_MEMORY_POLICY]     = "policy:",
	[CCS_MEMORY_AUDIT]      = "audit log:",
	[CCS_MEMORY_QUERY]      = "query message:",
};

/***** SECTION2: Structure definition *****/

struct iattr;

/* Structure for query. */
struct ccs_query {
	struct list_head list;
	struct ccs_domain_info *domain;
	char *query;
	size_t query_len;
	unsigned int serial;
	u8 timer;
	u8 answer;
	u8 retry;
};

/* Structure for audit log. */
struct ccs_log {
	struct list_head list;
	char *log;
	int size;
};

/***** SECTION3: Prototype definition section *****/

int ccs_audit_log(struct ccs_request_info *r);
struct ccs_domain_info *ccs_assign_domain(const char *domainname,
					  const bool transit);
u8 ccs_get_config(const u8 profile, const u8 index);
void ccs_transition_failed(const char *domainname);
void ccs_write_log(struct ccs_request_info *r, const char *fmt, ...);

static bool ccs_correct_domain(const unsigned char *domainname);
static bool ccs_correct_path(const char *filename);
static bool ccs_correct_word(const char *string);
static bool ccs_correct_word2(const char *string, size_t len);
static bool ccs_domain_def(const unsigned char *buffer);
static bool ccs_domain_quota_ok(struct ccs_request_info *r);
static bool ccs_flush(struct ccs_io_buffer *head);
static bool ccs_get_audit(const struct ccs_request_info *r);
static bool ccs_has_more_namespace(struct ccs_io_buffer *head);
static bool ccs_manager(void);
static bool ccs_namespace_jump(const char *domainname);
static bool ccs_parse_argv(char *left, char *right, struct ccs_argv *argv);
static bool ccs_parse_envp(char *left, char *right, struct ccs_envp *envp);
static bool ccs_parse_name_union(struct ccs_acl_param *param,
				 struct ccs_name_union *ptr);
static bool ccs_parse_name_union_quoted(struct ccs_acl_param *param,
					struct ccs_name_union *ptr);
static bool ccs_parse_number_union(struct ccs_acl_param *param,
				   struct ccs_number_union *ptr);
static bool ccs_permstr(const char *string, const char *keyword);
static bool ccs_print_condition(struct ccs_io_buffer *head,
				const struct ccs_condition *cond);
static bool ccs_print_entry(struct ccs_io_buffer *head,
			    const struct ccs_acl_info *acl);
static bool ccs_print_group(struct ccs_io_buffer *head,
			    const struct ccs_group *group);
static bool ccs_read_acl(struct ccs_io_buffer *head, struct list_head *list);
static bool ccs_read_group(struct ccs_io_buffer *head, const int idx);
static bool ccs_read_policy(struct ccs_io_buffer *head, const int idx);
static bool ccs_same_condition(const struct ccs_condition *a,
			       const struct ccs_condition *b);
static bool ccs_select_domain(struct ccs_io_buffer *head, const char *data);
static bool ccs_set_lf(struct ccs_io_buffer *head);
static bool ccs_str_starts(char **src, const char *find);
static char *ccs_get_transit_preference(struct ccs_acl_param *param,
					struct ccs_condition *e);
static char *ccs_init_log(struct ccs_request_info *r, int len, const char *fmt,
			  va_list args);
static char *ccs_print_bprm(struct linux_binprm *bprm,
			    struct ccs_page_dump *dump);
static char *ccs_print_header(struct ccs_request_info *r);
static char *ccs_read_token(struct ccs_acl_param *param);
static const char *ccs_yesno(const unsigned int value);
static const struct ccs_path_info *ccs_get_domainname
(struct ccs_acl_param *param);
static const struct ccs_path_info *ccs_get_dqword(char *start);
static int __init ccs_init_module(void);
static int ccs_delete_domain(char *domainname);
static int ccs_open(struct inode *inode, struct file *file);
static int ccs_parse_policy(struct ccs_io_buffer *head, char *line);
static int ccs_release(struct inode *inode, struct file *file);
static int ccs_set_mode(char *name, const char *value,
			struct ccs_profile *profile);
static int ccs_supervisor(struct ccs_request_info *r, const char *fmt, ...)
	__printf(2, 3);
static int ccs_truncate(char *str);
static int ccs_update_acl(const int size, struct ccs_acl_param *param);
static int ccs_update_manager_entry(const char *manager, const bool is_delete);
static int ccs_update_policy(const int size, struct ccs_acl_param *param);
static int ccs_write_acl(struct ccs_policy_namespace *ns,
			 struct list_head *list, char *data,
			 const bool is_delete);
static int ccs_write_aggregator(struct ccs_acl_param *param);
static int ccs_write_answer(struct ccs_io_buffer *head);
static int ccs_write_domain(struct ccs_io_buffer *head);
static int ccs_write_exception(struct ccs_io_buffer *head);
static int ccs_write_file(struct ccs_acl_param *param);
static int ccs_write_group(struct ccs_acl_param *param, const u8 type);
static int ccs_write_manager(struct ccs_io_buffer *head);
static int ccs_write_pid(struct ccs_io_buffer *head);
static int ccs_write_profile(struct ccs_io_buffer *head);
static int ccs_write_stat(struct ccs_io_buffer *head);
static int ccs_write_task(struct ccs_acl_param *param);
static int ccs_write_transition_control(struct ccs_acl_param *param,
					const u8 type);
static s8 ccs_find_yesno(const char *string, const char *find);
static ssize_t ccs_read(struct file *file, char __user *buf, size_t count,
			loff_t *ppos);
static ssize_t ccs_read_self(struct file *file, char __user *buf, size_t count,
			     loff_t *ppos);
static ssize_t ccs_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos);
static struct ccs_condition *ccs_commit_condition(struct ccs_condition *entry);
static struct ccs_condition *ccs_get_condition(struct ccs_acl_param *param);
static struct ccs_domain_info *ccs_find_domain(const char *domainname);
static struct ccs_domain_info *ccs_find_domain_by_qid(unsigned int serial);
static struct ccs_group *ccs_get_group(struct ccs_acl_param *param,
				       const u8 idx);
static struct ccs_policy_namespace *ccs_assign_namespace
(const char *domainname);
static struct ccs_policy_namespace *ccs_find_namespace(const char *name,
						       const unsigned int len);
static struct ccs_profile *ccs_assign_profile(struct ccs_policy_namespace *ns,
					      const unsigned int profile);
static struct ccs_profile *ccs_profile(const u8 profile);
static u8 ccs_condition_type(const char *word);
static u8 ccs_make_byte(const u8 c1, const u8 c2, const u8 c3);
static u8 ccs_parse_ulong(unsigned long *result, char **str);
static unsigned int ccs_poll(struct file *file, poll_table *wait);
static void __init ccs_create_entry(const char *name, const umode_t mode,
				    struct proc_dir_entry *parent,
				    const u8 key);
static void __init ccs_load_builtin_policy(void);
static void __init ccs_policy_io_init(void);
static void __init ccs_proc_init(void);
static void ccs_add_entry(char *header);
static void ccs_addprintf(char *buffer, int len, const char *fmt, ...)
	__printf(3, 4);
static void ccs_addprintf(char *buffer, int len, const char *fmt, ...);
static void ccs_check_profile(void);
static void ccs_convert_time(time_t time, struct ccs_time *stamp);
static void ccs_init_policy_namespace(struct ccs_policy_namespace *ns);
static void ccs_io_printf(struct ccs_io_buffer *head, const char *fmt, ...)
	__printf(2, 3);
static void ccs_normalize_line(unsigned char *buffer);
static void ccs_print_config(struct ccs_io_buffer *head, const u8 config);
static void ccs_print_name_union(struct ccs_io_buffer *head,
				 const struct ccs_name_union *ptr);
static void ccs_print_name_union_quoted(struct ccs_io_buffer *head,
					const struct ccs_name_union *ptr);
static void ccs_print_namespace(struct ccs_io_buffer *head);
static void ccs_print_number_union(struct ccs_io_buffer *head,
				   const struct ccs_number_union *ptr);
static void ccs_print_number_union_nospace(struct ccs_io_buffer *head,
					   const struct ccs_number_union *ptr);
static void ccs_read_domain(struct ccs_io_buffer *head);
static void ccs_read_exception(struct ccs_io_buffer *head);
static void ccs_read_log(struct ccs_io_buffer *head);
static void ccs_read_manager(struct ccs_io_buffer *head);
static void ccs_read_pid(struct ccs_io_buffer *head);
static void ccs_read_profile(struct ccs_io_buffer *head);
static void ccs_read_query(struct ccs_io_buffer *head);
static void ccs_read_stat(struct ccs_io_buffer *head);
static void ccs_read_version(struct ccs_io_buffer *head);
static void ccs_set_group(struct ccs_io_buffer *head, const char *category);
static void ccs_set_namespace_cursor(struct ccs_io_buffer *head);
static void ccs_set_slash(struct ccs_io_buffer *head);
static void ccs_set_space(struct ccs_io_buffer *head);
static void ccs_set_string(struct ccs_io_buffer *head, const char *string);
static void ccs_set_uint(unsigned int *i, const char *string,
			 const char *find);
static void ccs_update_stat(const u8 index);
static void ccs_update_task_domain(struct ccs_request_info *r);
static void ccs_write_log2(struct ccs_request_info *r, int len,
			   const char *fmt, va_list args);

#ifdef CONFIG_CCSECURITY_PORTRESERVE
static bool __ccs_lport_reserved(const u16 port);
static int ccs_write_reserved_port(struct ccs_acl_param *param);
#endif

#ifdef CONFIG_CCSECURITY_NETWORK
static bool ccs_parse_ipaddr_union(struct ccs_acl_param *param,
				   struct ccs_ipaddr_union *ptr);
static int ccs_print_ipv4(char *buffer, const unsigned int buffer_len,
			  const u32 *ip);
static int ccs_print_ipv6(char *buffer, const unsigned int buffer_len,
			  const struct in6_addr *ip);
static int ccs_write_inet_network(struct ccs_acl_param *param);
static int ccs_write_unix_network(struct ccs_acl_param *param);
static void ccs_print_ip(char *buf, const unsigned int size,
			 const struct ccs_ipaddr_union *ptr);
#endif

#ifdef CONFIG_CCSECURITY_CAPABILITY
static int ccs_write_capability(struct ccs_acl_param *param);
#endif

#ifdef CONFIG_CCSECURITY_MISC
static int ccs_write_misc(struct ccs_acl_param *param);
#endif

#ifdef CONFIG_CCSECURITY_IPC
static int ccs_write_ipc(struct ccs_acl_param *param);
#endif

#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION
static ssize_t ccs_write_self(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos);
#endif

/***** SECTION4: Standalone functions section *****/

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)

/**
 * fatal_signal_pending - Check whether SIGKILL is pending or not.
 *
 * @p: Pointer to "struct task_struct".
 *
 * Returns true if SIGKILL is pending on @p, false otherwise.
 *
 * This is for compatibility with older kernels.
 */
#define fatal_signal_pending(p) (signal_pending(p) &&			\
				 sigismember(&p->pending.signal, SIGKILL))

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)

/**
 * __wait_event_interruptible_timeout - Sleep until a condition gets true or a timeout elapses.
 *
 * @wq:        The waitqueue to wait on.
 * @condition: A C expression for the event to wait for.
 * @ret:       Timeout, in jiffies.
 *
 * Returns 0 if the @timeout elapsed, -ERESTARTSYS if it was interrupted by a
 * signal, and the remaining jiffies otherwise if the condition evaluated to
 * true before the timeout elapsed.
 *
 * This is for compatibility with older kernels.
 */
#define __wait_event_interruptible_timeout(wq, condition, ret)		\
do {									\
	wait_queue_t __wait;						\
	init_waitqueue_entry(&__wait, current);				\
									\
	add_wait_queue(&wq, &__wait);					\
	for (;;) {							\
		set_current_state(TASK_INTERRUPTIBLE);			\
		if (condition)						\
			break;						\
		if (!signal_pending(current)) {				\
			ret = schedule_timeout(ret);			\
			if (!ret)					\
				break;					\
			continue;					\
		}							\
		ret = -ERESTARTSYS;					\
		break;							\
	}								\
	current->state = TASK_RUNNING;					\
	remove_wait_queue(&wq, &__wait);				\
} while (0)

/**
 * wait_event_interruptible_timeout - Sleep until a condition gets true or a timeout elapses.
 *
 * @wq:        The waitqueue to wait on.
 * @condition: A C expression for the event to wait for.
 * @timeout:   Timeout, in jiffies.
 *
 * Returns 0 if the @timeout elapsed, -ERESTARTSYS if it was interrupted by a
 * signal, and the remaining jiffies otherwise if the condition evaluated to
 * true before the timeout elapsed.
 *
 * This is for compatibility with older kernels.
 */
#define wait_event_interruptible_timeout(wq, condition, timeout)	\
({									\
	long __ret = timeout;						\
	if (!(condition))						\
		__wait_event_interruptible_timeout(wq, condition, __ret); \
	__ret;								\
})

#endif

/**
 * ccs_convert_time - Convert time_t to YYYY/MM/DD hh/mm/ss.
 *
 * @time:  Seconds since 1970/01/01 00:00:00.
 * @stamp: Pointer to "struct ccs_time".
 *
 * Returns nothing.
 *
 * This function does not handle Y2038 problem.
 */
static void ccs_convert_time(time_t time, struct ccs_time *stamp)
{
	static const u16 ccs_eom[2][12] = {
		{ 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
		{ 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
	};
	u16 y;
	u8 m;
	bool r;
	stamp->sec = time % 60;
	time /= 60;
	stamp->min = time % 60;
	time /= 60;
	stamp->hour = time % 24;
	time /= 24;
	for (y = 1970; ; y++) {
		const unsigned short days = (y & 3) ? 365 : 366;
		if (time < days)
			break;
		time -= days;
	}
	r = (y & 3) == 0;
	for (m = 0; m < 11 && time >= ccs_eom[r][m]; m++);
	if (m)
		time -= ccs_eom[r][m - 1];
	stamp->year = y;
	stamp->month = ++m;
	stamp->day = ++time;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 23)
#if !defined(RHEL_VERSION) || RHEL_VERSION != 3

/**
 * PDE - Get "struct proc_dir_entry".
 *
 * @inode: Pointer to "struct inode".
 *
 * Returns pointer to "struct proc_dir_entry".
 *
 * This is for compatibility with older kernels.
 */
static inline struct proc_dir_entry *PDE(const struct inode *inode)
{
	return (struct proc_dir_entry *) inode->u.generic_ip;
}

#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)

/**
 * proc_notify_change - Update inode's attributes and reflect to the dentry.
 *
 * @dentry: Pointer to "struct dentry".
 * @iattr:  Pointer to "struct iattr".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * The 2.4 kernels don't allow chmod()/chown() for files in /proc,
 * while the 2.6 kernels allow.
 * To permit management of /proc/ccs/ interface by non-root user,
 * I modified to allow chmod()/chown() of /proc/ccs/ interface like 2.6 kernels
 * by adding "struct inode_operations"->setattr hook.
 */
static int proc_notify_change(struct dentry *dentry, struct iattr *iattr)
{
	struct inode *inode = dentry->d_inode;
	struct proc_dir_entry *de = PDE(inode);
	int error;

	error = inode_change_ok(inode, iattr);
	if (error)
		goto out;

	error = inode_setattr(inode, iattr);
	if (error)
		goto out;

	de->uid = inode->i_uid;
	de->gid = inode->i_gid;
	de->mode = inode->i_mode;
out:
	return error;
}

#endif

#ifdef CONFIG_CCSECURITY_NETWORK

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) && defined(CONFIG_NET)
#define ccs_in4_pton in4_pton
#define ccs_in6_pton in6_pton
#else
/*
 * Routines for parsing IPv4 or IPv6 address.
 * These are copied from lib/hexdump.c net/core/utils.c .
 */
#include <linux/ctype.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static int hex_to_bin(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	ch = tolower(ch);
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	return -1;
}
#endif

#define IN6PTON_XDIGIT		0x00010000
#define IN6PTON_DIGIT		0x00020000
#define IN6PTON_COLON_MASK	0x00700000
#define IN6PTON_COLON_1		0x00100000	/* single : requested */
#define IN6PTON_COLON_2		0x00200000	/* second : requested */
#define IN6PTON_COLON_1_2	0x00400000	/* :: requested */
#define IN6PTON_DOT		0x00800000	/* . */
#define IN6PTON_DELIM		0x10000000
#define IN6PTON_NULL		0x20000000	/* first/tail */
#define IN6PTON_UNKNOWN		0x40000000

static inline int xdigit2bin(char c, int delim)
{
	int val;

	if (c == delim || c == '\0')
		return IN6PTON_DELIM;
	if (c == ':')
		return IN6PTON_COLON_MASK;
	if (c == '.')
		return IN6PTON_DOT;

	val = hex_to_bin(c);
	if (val >= 0)
		return val | IN6PTON_XDIGIT | (val < 10 ? IN6PTON_DIGIT : 0);

	if (delim == -1)
		return IN6PTON_DELIM;
	return IN6PTON_UNKNOWN;
}

static int ccs_in4_pton(const char *src, int srclen, u8 *dst, int delim,
			const char **end)
{
	const char *s;
	u8 *d;
	u8 dbuf[4];
	int ret = 0;
	int i;
	int w = 0;

	if (srclen < 0)
		srclen = strlen(src);
	s = src;
	d = dbuf;
	i = 0;
	while (1) {
		int c;
		c = xdigit2bin(srclen > 0 ? *s : '\0', delim);
		if (!(c & (IN6PTON_DIGIT | IN6PTON_DOT | IN6PTON_DELIM |
			   IN6PTON_COLON_MASK)))
			goto out;
		if (c & (IN6PTON_DOT | IN6PTON_DELIM | IN6PTON_COLON_MASK)) {
			if (w == 0)
				goto out;
			*d++ = w & 0xff;
			w = 0;
			i++;
			if (c & (IN6PTON_DELIM | IN6PTON_COLON_MASK)) {
				if (i != 4)
					goto out;
				break;
			}
			goto cont;
		}
		w = (w * 10) + c;
		if ((w & 0xffff) > 255)
			goto out;
cont:
		if (i >= 4)
			goto out;
		s++;
		srclen--;
	}
	ret = 1;
	memcpy(dst, dbuf, sizeof(dbuf));
out:
	if (end)
		*end = s;
	return ret;
}

static int ccs_in6_pton(const char *src, int srclen, u8 *dst, int delim,
			const char **end)
{
	const char *s, *tok = NULL;
	u8 *d, *dc = NULL;
	u8 dbuf[16];
	int ret = 0;
	int i;
	int state = IN6PTON_COLON_1_2 | IN6PTON_XDIGIT | IN6PTON_NULL;
	int w = 0;

	memset(dbuf, 0, sizeof(dbuf));

	s = src;
	d = dbuf;
	if (srclen < 0)
		srclen = strlen(src);

	while (1) {
		int c;

		c = xdigit2bin(srclen > 0 ? *s : '\0', delim);
		if (!(c & state))
			goto out;
		if (c & (IN6PTON_DELIM | IN6PTON_COLON_MASK)) {
			/* process one 16-bit word */
			if (!(state & IN6PTON_NULL)) {
				*d++ = (w >> 8) & 0xff;
				*d++ = w & 0xff;
			}
			w = 0;
			if (c & IN6PTON_DELIM) {
				/* We've processed last word */
				break;
			}
			/*
			 * COLON_1 => XDIGIT
			 * COLON_2 => XDIGIT|DELIM
			 * COLON_1_2 => COLON_2
			 */
			switch (state & IN6PTON_COLON_MASK) {
			case IN6PTON_COLON_2:
				dc = d;
				state = IN6PTON_XDIGIT | IN6PTON_DELIM;
				if (dc - dbuf >= sizeof(dbuf))
					state |= IN6PTON_NULL;
				break;
			case IN6PTON_COLON_1|IN6PTON_COLON_1_2:
				state = IN6PTON_XDIGIT | IN6PTON_COLON_2;
				break;
			case IN6PTON_COLON_1:
				state = IN6PTON_XDIGIT;
				break;
			case IN6PTON_COLON_1_2:
				state = IN6PTON_COLON_2;
				break;
			default:
				state = 0;
			}
			tok = s + 1;
			goto cont;
		}

		if (c & IN6PTON_DOT) {
			ret = ccs_in4_pton(tok ? tok : s, srclen +
					   (int)(s - tok), d, delim, &s);
			if (ret > 0) {
				d += 4;
				break;
			}
			goto out;
		}

		w = (w << 4) | (0xff & c);
		state = IN6PTON_COLON_1 | IN6PTON_DELIM;
		if (!(w & 0xf000))
			state |= IN6PTON_XDIGIT;
		if (!dc && d + 2 < dbuf + sizeof(dbuf)) {
			state |= IN6PTON_COLON_1_2;
			state &= ~IN6PTON_DELIM;
		}
		if (d + 2 >= dbuf + sizeof(dbuf))
			state &= ~(IN6PTON_COLON_1|IN6PTON_COLON_1_2);
cont:
		if ((dc && d + 4 < dbuf + sizeof(dbuf)) ||
		    d + 4 == dbuf + sizeof(dbuf))
			state |= IN6PTON_DOT;
		if (d >= dbuf + sizeof(dbuf))
			state &= ~(IN6PTON_XDIGIT|IN6PTON_COLON_MASK);
		s++;
		srclen--;
	}

	i = 15; d--;

	if (dc) {
		while (d >= dc)
			dst[i--] = *d--;
		while (i >= dc - dbuf)
			dst[i--] = 0;
		while (i >= 0)
			dst[i--] = *d--;
	} else
		memcpy(dst, dbuf, sizeof(dbuf));

	ret = 1;
out:
	if (end)
		*end = s;
	return ret;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)

/*
 * Routines for printing IPv4 or IPv6 address.
 * These are copied from include/linux/kernel.h include/net/ipv6.h
 * include/net/addrconf.h lib/hexdump.c lib/vsprintf.c and simplified.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
#if !defined(RHEL_MAJOR) || RHEL_MAJOR != 5 || !defined(RHEL_MINOR) || RHEL_MINOR < 9
static const char hex_asc[] = "0123456789abcdef";
#define hex_asc_lo(x)   hex_asc[((x) & 0x0f)]
#define hex_asc_hi(x)   hex_asc[((x) & 0xf0) >> 4]

static inline char *pack_hex_byte(char *buf, u8 byte)
{
	*buf++ = hex_asc_hi(byte);
	*buf++ = hex_asc_lo(byte);
	return buf;
}
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static inline int ipv6_addr_v4mapped(const struct in6_addr *a)
{
	return (a->s6_addr32[0] | a->s6_addr32[1] |
		(a->s6_addr32[2] ^ htonl(0x0000ffff))) == 0;
}
#endif

static inline int ipv6_addr_is_isatap(const struct in6_addr *addr)
{
	return (addr->s6_addr32[2] | htonl(0x02000000)) == htonl(0x02005EFE);
}

static char *ip4_string(char *p, const u8 *addr)
{
	/*
	 * Since this function is called outside vsnprintf(), I can use
	 * sprintf() here.
	 */
	return p +
		sprintf(p, "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
}

static char *ip6_compressed_string(char *p, const char *addr)
{
	int i, j, range;
	unsigned char zerolength[8];
	int longest = 1;
	int colonpos = -1;
	u16 word;
	u8 hi, lo;
	bool needcolon = false;
	bool useIPv4;
	struct in6_addr in6;

	memcpy(&in6, addr, sizeof(struct in6_addr));

	useIPv4 = ipv6_addr_v4mapped(&in6) || ipv6_addr_is_isatap(&in6);

	memset(zerolength, 0, sizeof(zerolength));

	if (useIPv4)
		range = 6;
	else
		range = 8;

	/* find position of longest 0 run */
	for (i = 0; i < range; i++) {
		for (j = i; j < range; j++) {
			if (in6.s6_addr16[j] != 0)
				break;
			zerolength[i]++;
		}
	}
	for (i = 0; i < range; i++) {
		if (zerolength[i] > longest) {
			longest = zerolength[i];
			colonpos = i;
		}
	}
	if (longest == 1)		/* don't compress a single 0 */
		colonpos = -1;

	/* emit address */
	for (i = 0; i < range; i++) {
		if (i == colonpos) {
			if (needcolon || i == 0)
				*p++ = ':';
			*p++ = ':';
			needcolon = false;
			i += longest - 1;
			continue;
		}
		if (needcolon) {
			*p++ = ':';
			needcolon = false;
		}
		/* hex u16 without leading 0s */
		word = ntohs(in6.s6_addr16[i]);
		hi = word >> 8;
		lo = word & 0xff;
		if (hi) {
			if (hi > 0x0f)
				p = pack_hex_byte(p, hi);
			else
				*p++ = hex_asc_lo(hi);
			p = pack_hex_byte(p, lo);
		} else if (lo > 0x0f)
			p = pack_hex_byte(p, lo);
		else
			*p++ = hex_asc_lo(lo);
		needcolon = true;
	}

	if (useIPv4) {
		if (needcolon)
			*p++ = ':';
		p = ip4_string(p, &in6.s6_addr[12]);
	}
	*p = '\0';

	return p;
}
#endif

/**
 * ccs_print_ipv4 - Print an IPv4 address.
 *
 * @buffer:     Buffer to write to.
 * @buffer_len: Size of @buffer.
 * @ip:         Pointer to "u32 in network byte order".
 *
 * Returns written length.
 */
static int ccs_print_ipv4(char *buffer, const unsigned int buffer_len,
			  const u32 *ip)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	return snprintf(buffer, buffer_len, "%pI4", ip);
#else
	char addr[sizeof("255.255.255.255")];
	ip4_string(addr, (const u8 *) ip);
	return snprintf(buffer, buffer_len, "%s", addr);
#endif
}

/**
 * ccs_print_ipv6 - Print an IPv6 address.
 *
 * @buffer:     Buffer to write to.
 * @buffer_len: Size of @buffer.
 * @ip:         Pointer to "struct in6_addr".
 *
 * Returns written length.
 */
static int ccs_print_ipv6(char *buffer, const unsigned int buffer_len,
			  const struct in6_addr *ip)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	return snprintf(buffer, buffer_len, "%pI6c", ip);
#else
	char addr[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255")];
	ip6_compressed_string(addr, (const u8 *) ip);
	return snprintf(buffer, buffer_len, "%s", addr);
#endif
}

/**
 * ccs_print_ip - Print an IP address.
 *
 * @buf:  Buffer to write to.
 * @size: Size of @buf.
 * @ptr:  Pointer to "struct ipaddr_union".
 *
 * Returns nothing.
 */
static void ccs_print_ip(char *buf, const unsigned int size,
			 const struct ccs_ipaddr_union *ptr)
{
	int len;
	if (ptr->is_ipv6)
		len = ccs_print_ipv6(buf, size, &ptr->ip[0]);
	else
		len = ccs_print_ipv4(buf, size, &ptr->ip[0].s6_addr32[0]);
	if (!memcmp(&ptr->ip[0], &ptr->ip[1], 16) || len >= size / 2)
		return;
	buf[len++] = '-';
	if (ptr->is_ipv6)
		ccs_print_ipv6(buf + len, size - len, &ptr->ip[1]);
	else
		ccs_print_ipv4(buf + len, size - len,
			       &ptr->ip[1].s6_addr32[0]);
}

#endif

/***** SECTION5: Variables definition section *****/

/* Permit policy management by non-root user? */
static bool ccs_manage_by_non_root;

/* Lock for protecting policy. */
DEFINE_MUTEX(ccs_policy_lock);

/* Has /sbin/init started? */
bool ccs_policy_loaded;

/* List of namespaces. */
LIST_HEAD(ccs_namespace_list);
/* True if namespace other than ccs_kernel_namespace is defined. */
static bool ccs_namespace_enabled;

/* Initial namespace.*/
static struct ccs_policy_namespace ccs_kernel_namespace;

/* List of "struct ccs_condition". */
LIST_HEAD(ccs_condition_list);

#ifdef CONFIG_CCSECURITY_PORTRESERVE
/* Bitmap for reserved local port numbers.*/
static u8 ccs_reserved_port_map[8192];
#endif

/* Wait queue for kernel -> userspace notification. */
static DECLARE_WAIT_QUEUE_HEAD(ccs_query_wait);
/* Wait queue for userspace -> kernel notification. */
static DECLARE_WAIT_QUEUE_HEAD(ccs_answer_wait);

/* The list for "struct ccs_query". */
static LIST_HEAD(ccs_query_list);

/* Lock for manipulating ccs_query_list. */
static DEFINE_SPINLOCK(ccs_query_list_lock);

/* Number of "struct file" referring /proc/ccs/query interface. */
static atomic_t ccs_query_observers = ATOMIC_INIT(0);

/* Wait queue for /proc/ccs/audit. */
static DECLARE_WAIT_QUEUE_HEAD(ccs_log_wait);

/* The list for "struct ccs_log". */
static LIST_HEAD(ccs_log);

/* Lock for "struct list_head ccs_log". */
static DEFINE_SPINLOCK(ccs_log_lock);

/* Length of "stuct list_head ccs_log". */
static unsigned int ccs_log_count;

/* Timestamp counter for last updated. */
static unsigned int ccs_stat_updated[CCS_MAX_POLICY_STAT];

/* Counter for number of updates. */
static unsigned int ccs_stat_modified[CCS_MAX_POLICY_STAT];

/* Operations for /proc/ccs/self_domain interface. */
static
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 17)
const
#endif
struct file_operations ccs_self_operations = {
#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION
	.write = ccs_write_self,
#endif
	.read  = ccs_read_self,
};

/* Operations for /proc/ccs/ interface. */
static
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 17)
const
#endif
struct file_operations ccs_operations = {
	.open    = ccs_open,
	.release = ccs_release,
	.poll    = ccs_poll,
	.read    = ccs_read,
	.write   = ccs_write,
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)

/* The inode operations for /proc/ccs/ directory. */
static struct inode_operations ccs_dir_inode_operations;

/* The inode operations for files under /proc/ccs/ directory. */
static struct inode_operations ccs_file_inode_operations;

#endif

/***** SECTION6: Dependent functions section *****/

/**
 * list_for_each_cookie - iterate over a list with cookie.
 *
 * @pos:  Pointer to "struct list_head".
 * @head: Pointer to "struct list_head".
 */
#define list_for_each_cookie(pos, head)					\
	for (pos = pos ? pos : srcu_dereference((head)->next, &ccs_ss); \
	     pos != (head); pos = srcu_dereference(pos->next, &ccs_ss))

/**
 * ccs_read_token - Read a word from a line.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns a word on success, "" otherwise.
 *
 * To allow the caller to skip NULL check, this function returns "" rather than
 * NULL if there is no more words to read.
 */
static char *ccs_read_token(struct ccs_acl_param *param)
{
	char *pos = param->data;
	char *del = strchr(pos, ' ');
	if (del)
		*del++ = '\0';
	else
		del = pos + strlen(pos);
	param->data = del;
	return pos;
}

/**
 * ccs_make_byte - Make byte value from three octal characters.
 *
 * @c1: The first character.
 * @c2: The second character.
 * @c3: The third character.
 *
 * Returns byte value.
 */
static u8 ccs_make_byte(const u8 c1, const u8 c2, const u8 c3)
{
	return ((c1 - '0') << 6) + ((c2 - '0') << 3) + (c3 - '0');
}

/**
 * ccs_correct_word2 - Check whether the given string follows the naming rules.
 *
 * @string: The byte sequence to check. Not '\0'-terminated.
 * @len:    Length of @string.
 *
 * Returns true if @string follows the naming rules, false otherwise.
 */
static bool ccs_correct_word2(const char *string, size_t len)
{
	const char *const start = string;
	bool in_repetition = false;
	unsigned char c;
	unsigned char d;
	unsigned char e;
	if (!len)
		goto out;
	while (len--) {
		c = *string++;
		if (c == '\\') {
			if (!len--)
				goto out;
			c = *string++;
			switch (c) {
			case '\\':  /* "\\" */
				continue;
			case '$':   /* "\$" */
			case '+':   /* "\+" */
			case '?':   /* "\?" */
			case '*':   /* "\*" */
			case '@':   /* "\@" */
			case 'x':   /* "\x" */
			case 'X':   /* "\X" */
			case 'a':   /* "\a" */
			case 'A':   /* "\A" */
			case '-':   /* "\-" */
				continue;
			case '{':   /* "/\{" */
				if (string - 3 < start || *(string - 3) != '/')
					break;
				in_repetition = true;
				continue;
			case '}':   /* "\}/" */
				if (*string != '/')
					break;
				if (!in_repetition)
					break;
				in_repetition = false;
				continue;
			case '0':   /* "\ooo" */
			case '1':
			case '2':
			case '3':
				if (!len-- || !len--)
					break;
				d = *string++;
				e = *string++;
				if (d < '0' || d > '7' || e < '0' || e > '7')
					break;
				c = ccs_make_byte(c, d, e);
				if (c <= ' ' || c >= 127)
					continue;
			}
			goto out;
		} else if (in_repetition && c == '/') {
			goto out;
		} else if (c <= ' ' || c >= 127) {
			goto out;
		}
	}
	if (in_repetition)
		goto out;
	return true;
out:
	return false;
}

/**
 * ccs_correct_word - Check whether the given string follows the naming rules.
 *
 * @string: The string to check.
 *
 * Returns true if @string follows the naming rules, false otherwise.
 */
static bool ccs_correct_word(const char *string)
{
	return ccs_correct_word2(string, strlen(string));
}

/**
 * ccs_get_group - Allocate memory for "struct ccs_path_group"/"struct ccs_number_group"/"struct ccs_address_group".
 *
 * @param: Pointer to "struct ccs_acl_param".
 * @idx:   Index number.
 *
 * Returns pointer to "struct ccs_group" on success, NULL otherwise.
 */
static struct ccs_group *ccs_get_group(struct ccs_acl_param *param,
				       const u8 idx)
{
	struct ccs_group e = { };
	struct ccs_group *group = NULL;
	struct list_head *list;
	const char *group_name = ccs_read_token(param);
	bool found = false;
	if (!ccs_correct_word(group_name) || idx >= CCS_MAX_GROUP)
		return NULL;
	e.group_name = ccs_get_name(group_name);
	if (!e.group_name)
		return NULL;
	if (mutex_lock_interruptible(&ccs_policy_lock))
		goto out;
	list = &param->ns->group_list[idx];
	list_for_each_entry(group, list, head.list) {
		if (e.group_name != group->group_name ||
		    atomic_read(&group->head.users) == CCS_GC_IN_PROGRESS)
			continue;
		atomic_inc(&group->head.users);
		found = true;
		break;
	}
	if (!found) {
		struct ccs_group *entry = ccs_commit_ok(&e, sizeof(e));
		if (entry) {
			INIT_LIST_HEAD(&entry->member_list);
			atomic_set(&entry->head.users, 1);
			list_add_tail_rcu(&entry->head.list, list);
			group = entry;
			found = true;
		}
	}
	mutex_unlock(&ccs_policy_lock);
out:
	ccs_put_name(e.group_name);
	return found ? group : NULL;
}

/**
 * ccs_parse_name_union - Parse a ccs_name_union.
 *
 * @param: Pointer to "struct ccs_acl_param".
 * @ptr:   Pointer to "struct ccs_name_union".
 *
 * Returns true on success, false otherwise.
 */
static bool ccs_parse_name_union(struct ccs_acl_param *param,
				 struct ccs_name_union *ptr)
{
	char *filename;
	if (param->data[0] == '@') {
		param->data++;
		ptr->group = ccs_get_group(param, CCS_PATH_GROUP);
		return ptr->group != NULL;
	}
	filename = ccs_read_token(param);
	if (!ccs_correct_word(filename))
		return false;
	ptr->filename = ccs_get_name(filename);
	return ptr->filename != NULL;
}

/**
 * ccs_parse_ulong - Parse an "unsigned long" value.
 *
 * @result: Pointer to "unsigned long".
 * @str:    Pointer to string to parse.
 *
 * Returns one of values in "enum ccs_value_type".
 *
 * The @src is updated to point the first character after the value
 * on success.
 */
static u8 ccs_parse_ulong(unsigned long *result, char **str)
{
	const char *cp = *str;
	char *ep;
	int base = 10;
	if (*cp == '0') {
		char c = *(cp + 1);
		if (c == 'x' || c == 'X') {
			base = 16;
			cp += 2;
		} else if (c >= '0' && c <= '7') {
			base = 8;
			cp++;
		}
	}
	*result = simple_strtoul(cp, &ep, base);
	if (cp == ep)
		return CCS_VALUE_TYPE_INVALID;
	*str = ep;
	switch (base) {
	case 16:
		return CCS_VALUE_TYPE_HEXADECIMAL;
	case 8:
		return CCS_VALUE_TYPE_OCTAL;
	default:
		return CCS_VALUE_TYPE_DECIMAL;
	}
}

/**
 * ccs_parse_number_union - Parse a ccs_number_union.
 *
 * @param: Pointer to "struct ccs_acl_param".
 * @ptr:   Pointer to "struct ccs_number_union".
 *
 * Returns true on success, false otherwise.
 */
static bool ccs_parse_number_union(struct ccs_acl_param *param,
				   struct ccs_number_union *ptr)
{
	char *data;
	u8 type;
	unsigned long v;
	memset(ptr, 0, sizeof(*ptr));
	if (param->data[0] == '@') {
		param->data++;
		ptr->group = ccs_get_group(param, CCS_NUMBER_GROUP);
		return ptr->group != NULL;
	}
	data = ccs_read_token(param);
	type = ccs_parse_ulong(&v, &data);
	if (type == CCS_VALUE_TYPE_INVALID)
		return false;
	ptr->values[0] = v;
	ptr->value_type[0] = type;
	if (!*data) {
		ptr->values[1] = v;
		ptr->value_type[1] = type;
		return true;
	}
	if (*data++ != '-')
		return false;
	type = ccs_parse_ulong(&v, &data);
	if (type == CCS_VALUE_TYPE_INVALID || *data || ptr->values[0] > v)
		return false;
	ptr->values[1] = v;
	ptr->value_type[1] = type;
	return true;
}

#ifdef CONFIG_CCSECURITY_NETWORK

/**
 * ccs_parse_ipaddr_union - Parse an IP address.
 *
 * @param: Pointer to "struct ccs_acl_param".
 * @ptr:   Pointer to "struct ccs_ipaddr_union".
 *
 * Returns true on success, false otherwise.
 */
static bool ccs_parse_ipaddr_union(struct ccs_acl_param *param,
				   struct ccs_ipaddr_union *ptr)
{
	u8 * const min = ptr->ip[0].in6_u.u6_addr8;
	u8 * const max = ptr->ip[1].in6_u.u6_addr8;
	char *address = ccs_read_token(param);
	const char *end;
	if (!strchr(address, ':') &&
	    ccs_in4_pton(address, -1, min, '-', &end) > 0) {
		ptr->is_ipv6 = false;
		if (!*end)
			ptr->ip[1].s6_addr32[0] = ptr->ip[0].s6_addr32[0];
		else if (*end++ != '-' ||
			 ccs_in4_pton(end, -1, max, '\0', &end) <= 0 || *end)
			return false;
		return true;
	}
	if (ccs_in6_pton(address, -1, min, '-', &end) > 0) {
		ptr->is_ipv6 = true;
		if (!*end)
			memmove(max, min, sizeof(u16) * 8);
		else if (*end++ != '-' ||
			 ccs_in6_pton(end, -1, max, '\0', &end) <= 0 || *end)
			return false;
		return true;
	}
	return false;
}

#endif

/**
 * ccs_get_dqword - ccs_get_name() for a quoted string.
 *
 * @start: String to save.
 *
 * Returns pointer to "struct ccs_path_info" on success, NULL otherwise.
 */
static const struct ccs_path_info *ccs_get_dqword(char *start)
{
	char *cp = start + strlen(start) - 1;
	if (cp == start || *start++ != '"' || *cp != '"')
		return NULL;
	*cp = '\0';
	if (*start && !ccs_correct_word(start))
		return NULL;
	return ccs_get_name(start);
}

/**
 * ccs_parse_name_union_quoted - Parse a quoted word.
 *
 * @param: Pointer to "struct ccs_acl_param".
 * @ptr:   Pointer to "struct ccs_name_union".
 *
 * Returns true on success, false otherwise.
 */
static bool ccs_parse_name_union_quoted(struct ccs_acl_param *param,
					struct ccs_name_union *ptr)
{
	char *filename = param->data;
	if (*filename == '@')
		return ccs_parse_name_union(param, ptr);
	ptr->filename = ccs_get_dqword(filename);
	return ptr->filename != NULL;
}

/**
 * ccs_parse_argv - Parse an argv[] condition part.
 *
 * @left:  Lefthand value.
 * @right: Righthand value.
 * @argv:  Pointer to "struct ccs_argv".
 *
 * Returns true on success, false otherwise.
 */
static bool ccs_parse_argv(char *left, char *right, struct ccs_argv *argv)
{
	if (ccs_parse_ulong(&argv->index, &left) != CCS_VALUE_TYPE_DECIMAL ||
	    *left++ != ']' || *left)
		return false;
	argv->value = ccs_get_dqword(right);
	return argv->value != NULL;
}

/**
 * ccs_parse_envp - Parse an envp[] condition part.
 *
 * @left:  Lefthand value.
 * @right: Righthand value.
 * @envp:  Pointer to "struct ccs_envp".
 *
 * Returns true on success, false otherwise.
 */
static bool ccs_parse_envp(char *left, char *right, struct ccs_envp *envp)
{
	const struct ccs_path_info *name;
	const struct ccs_path_info *value;
	char *cp = left + strlen(left) - 1;
	if (*cp-- != ']' || *cp != '"')
		goto out;
	*cp = '\0';
	if (!ccs_correct_word(left))
		goto out;
	name = ccs_get_name(left);
	if (!name)
		goto out;
	if (!strcmp(right, "NULL")) {
		value = NULL;
	} else {
		value = ccs_get_dqword(right);
		if (!value) {
			ccs_put_name(name);
			goto out;
		}
	}
	envp->name = name;
	envp->value = value;
	return true;
out:
	return false;
}

/**
 * ccs_same_condition - Check for duplicated "struct ccs_condition" entry.
 *
 * @a: Pointer to "struct ccs_condition".
 * @b: Pointer to "struct ccs_condition".
 *
 * Returns true if @a == @b, false otherwise.
 */
static bool ccs_same_condition(const struct ccs_condition *a,
			       const struct ccs_condition *b)
{
	return a->size == b->size && a->condc == b->condc &&
		a->numbers_count == b->numbers_count &&
		a->names_count == b->names_count &&
		a->argc == b->argc && a->envc == b->envc &&
		a->grant_log == b->grant_log &&
		a->exec_transit == b->exec_transit && a->transit == b->transit
		&& !memcmp(a + 1, b + 1, a->size - sizeof(*a));
}

/**
 * ccs_condition_type - Get condition type.
 *
 * @word: Keyword string.
 *
 * Returns one of values in "enum ccs_conditions_index" on success,
 * CCS_MAX_CONDITION_KEYWORD otherwise.
 */
static u8 ccs_condition_type(const char *word)
{
	u8 i;
	for (i = 0; i < CCS_MAX_CONDITION_KEYWORD; i++) {
		if (!strcmp(word, ccs_condition_keyword[i]))
			break;
	}
	return i;
}

/**
 * ccs_commit_condition - Commit "struct ccs_condition".
 *
 * @entry: Pointer to "struct ccs_condition".
 *
 * Returns pointer to "struct ccs_condition" on success, NULL otherwise.
 *
 * This function merges duplicated entries. This function returns NULL if
 * @entry is not duplicated but memory quota for policy has exceeded.
 */
static struct ccs_condition *ccs_commit_condition(struct ccs_condition *entry)
{
	struct ccs_condition *ptr;
	bool found = false;
	if (mutex_lock_interruptible(&ccs_policy_lock)) {
		dprintk(KERN_WARNING "%u: %s failed\n", __LINE__, __func__);
		ptr = NULL;
		found = true;
		goto out;
	}
	list_for_each_entry(ptr, &ccs_condition_list, head.list) {
		if (!ccs_same_condition(ptr, entry) ||
		    atomic_read(&ptr->head.users) == CCS_GC_IN_PROGRESS)
			continue;
		/* Same entry found. Share this entry. */
		atomic_inc(&ptr->head.users);
		found = true;
		break;
	}
	if (!found) {
		if (ccs_memory_ok(entry, entry->size)) {
			atomic_set(&entry->head.users, 1);
			list_add(&entry->head.list, &ccs_condition_list);
		} else {
			found = true;
			ptr = NULL;
		}
	}
	mutex_unlock(&ccs_policy_lock);
out:
	if (found) {
		ccs_del_condition(&entry->head.list);
		kfree(entry);
		entry = ptr;
	}
	return entry;
}

/**
 * ccs_correct_path - Check whether the given pathname follows the naming rules.
 *
 * @filename: The pathname to check.
 *
 * Returns true if @filename follows the naming rules, false otherwise.
 */
static bool ccs_correct_path(const char *filename)
{
	return *filename == '/' && ccs_correct_word(filename);
}

/**
 * ccs_domain_def - Check whether the given token can be a domainname.
 *
 * @buffer: The token to check.
 *
 * Returns true if @buffer possibly be a domainname, false otherwise.
 */
static bool ccs_domain_def(const unsigned char *buffer)
{
	const unsigned char *cp;
	int len;
	if (*buffer != '<')
		return false;
	cp = strchr(buffer, ' ');
	if (!cp)
		len = strlen(buffer);
	else
		len = cp - buffer;
	if (buffer[len - 1] != '>' || !ccs_correct_word2(buffer + 1, len - 2))
		return false;
	return true;
}

/**
 * ccs_correct_domain - Check whether the given domainname follows the naming rules.
 *
 * @domainname: The domainname to check.
 *
 * Returns true if @domainname follows the naming rules, false otherwise.
 */
static bool ccs_correct_domain(const unsigned char *domainname)
{
	if (!domainname || !ccs_domain_def(domainname))
		return false;
	domainname = strchr(domainname, ' ');
	if (!domainname++)
		return true;
	while (1) {
		const unsigned char *cp = strchr(domainname, ' ');
		if (!cp)
			break;
		if (*domainname != '/' ||
		    !ccs_correct_word2(domainname, cp - domainname))
			return false;
		domainname = cp + 1;
	}
	return ccs_correct_path(domainname);
}

/**
 * ccs_normalize_line - Format string.
 *
 * @buffer: The line to normalize.
 *
 * Returns nothing.
 *
 * Leading and trailing whitespaces are removed.
 * Multiple whitespaces are packed into single space.
 */
static void ccs_normalize_line(unsigned char *buffer)
{
	unsigned char *sp = buffer;
	unsigned char *dp = buffer;
	bool first = true;
	while (*sp && (*sp <= ' ' || *sp >= 127))
		sp++;
	while (*sp) {
		if (!first)
			*dp++ = ' ';
		first = false;
		while (*sp > ' ' && *sp < 127)
			*dp++ = *sp++;
		while (*sp && (*sp <= ' ' || *sp >= 127))
			sp++;
	}
	*dp = '\0';
}

/**
 * ccs_get_domainname - Read a domainname from a line.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns a domainname on success, NULL otherwise.
 */
static const struct ccs_path_info *ccs_get_domainname
(struct ccs_acl_param *param)
{
	char *start = param->data;
	char *pos = start;
	while (*pos) {
		if (*pos++ != ' ' || *pos++ == '/')
			continue;
		pos -= 2;
		*pos++ = '\0';
		break;
	}
	param->data = pos;
	if (ccs_correct_domain(start))
		return ccs_get_name(start);
	return NULL;
}

/**
 * ccs_get_transit_preference - Parse domain transition preference for execve().
 *
 * @param: Pointer to "struct ccs_acl_param".
 * @e:     Pointer to "struct ccs_condition".
 *
 * Returns the condition string part.
 */
static char *ccs_get_transit_preference(struct ccs_acl_param *param,
					struct ccs_condition *e)
{
	char * const pos = param->data;
	bool flag;
	if (*pos == '<') {
		e->transit = ccs_get_domainname(param);
		goto done;
	}
	{
		char *cp = strchr(pos, ' ');
		if (cp)
			*cp = '\0';
		flag = ccs_correct_path(pos) || !strcmp(pos, "keep") ||
			!strcmp(pos, "initialize") || !strcmp(pos, "reset") ||
			!strcmp(pos, "child") || !strcmp(pos, "parent");
		if (cp)
			*cp = ' ';
	}
	if (!flag)
		return pos;
	e->transit = ccs_get_name(ccs_read_token(param));
done:
	if (e->transit) {
		e->exec_transit = true;
		return param->data;
	}
	/*
	 * Return a bad read-only condition string that will let
	 * ccs_get_condition() return NULL.
	 */
	return "/";
}

/**
 * ccs_get_condition - Parse condition part.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns pointer to "struct ccs_condition" on success, NULL otherwise.
 */
struct ccs_condition *ccs_get_condition(struct ccs_acl_param *param)
{
	struct ccs_condition *entry = NULL;
	struct ccs_condition_element *condp = NULL;
	struct ccs_number_union *numbers_p = NULL;
	struct ccs_name_union *names_p = NULL;
	struct ccs_argv *argv = NULL;
	struct ccs_envp *envp = NULL;
	struct ccs_condition e = { };
	char * const start_of_string = ccs_get_transit_preference(param, &e);
	char * const end_of_string = start_of_string + strlen(start_of_string);
	char *pos;
rerun:
	pos = start_of_string;
	while (1) {
		u8 left = -1;
		u8 right = -1;
		char *left_word = pos;
		char *cp;
		char *right_word;
		bool is_not;
		if (!*left_word)
			break;
		/*
		 * Since left-hand condition does not allow use of "path_group"
		 * or "number_group" and environment variable's names do not
		 * accept '=', it is guaranteed that the original line consists
		 * of one or more repetition of $left$operator$right blocks
		 * where "$left is free from '=' and ' '" and "$operator is
		 * either '=' or '!='" and "$right is free from ' '".
		 * Therefore, we can reconstruct the original line at the end
		 * of dry run even if we overwrite $operator with '\0'.
		 */
		cp = strchr(pos, ' ');
		if (cp) {
			*cp = '\0'; /* Will restore later. */
			pos = cp + 1;
		} else {
			pos = "";
		}
		right_word = strchr(left_word, '=');
		if (!right_word || right_word == left_word)
			goto out;
		is_not = *(right_word - 1) == '!';
		if (is_not)
			*(right_word++ - 1) = '\0'; /* Will restore later. */
		else if (*(right_word + 1) != '=')
			*right_word++ = '\0'; /* Will restore later. */
		else
			goto out;
		dprintk(KERN_WARNING "%u: <%s>%s=<%s>\n", __LINE__, left_word,
			is_not ? "!" : "", right_word);
		if (!strcmp(left_word, "grant_log")) {
			if (entry) {
				if (is_not ||
				    entry->grant_log != CCS_GRANTLOG_AUTO)
					goto out;
				else if (!strcmp(right_word, "yes"))
					entry->grant_log = CCS_GRANTLOG_YES;
				else if (!strcmp(right_word, "no"))
					entry->grant_log = CCS_GRANTLOG_NO;
				else
					goto out;
			}
			continue;
		}
		if (!strcmp(left_word, "auto_domain_transition")) {
			if (entry) {
				if (is_not || entry->transit)
					goto out;
				entry->transit = ccs_get_dqword(right_word);
				if (!entry->transit ||
				    (entry->transit->name[0] != '/' &&
				     !ccs_domain_def(entry->transit->name)))
					goto out;
			}
			continue;
		}
		if (!strncmp(left_word, "exec.argv[", 10)) {
			if (!argv) {
				e.argc++;
				e.condc++;
			} else {
				e.argc--;
				e.condc--;
				left = CCS_ARGV_ENTRY;
				argv->is_not = is_not;
				if (!ccs_parse_argv(left_word + 10,
						    right_word, argv++))
					goto out;
			}
			goto store_value;
		}
		if (!strncmp(left_word, "exec.envp[\"", 11)) {
			if (!envp) {
				e.envc++;
				e.condc++;
			} else {
				e.envc--;
				e.condc--;
				left = CCS_ENVP_ENTRY;
				envp->is_not = is_not;
				if (!ccs_parse_envp(left_word + 11,
						    right_word, envp++))
					goto out;
			}
			goto store_value;
		}
		left = ccs_condition_type(left_word);
		dprintk(KERN_WARNING "%u: <%s> left=%u\n", __LINE__, left_word,
			left);
		if (left == CCS_MAX_CONDITION_KEYWORD) {
			if (!numbers_p) {
				e.numbers_count++;
			} else {
				e.numbers_count--;
				left = CCS_NUMBER_UNION;
				param->data = left_word;
				if (*left_word == '@' ||
				    !ccs_parse_number_union(param,
							    numbers_p++))
					goto out;
			}
		}
		if (!condp)
			e.condc++;
		else
			e.condc--;
		if (left == CCS_EXEC_REALPATH || left == CCS_SYMLINK_TARGET) {
			if (!names_p) {
				e.names_count++;
			} else {
				e.names_count--;
				right = CCS_NAME_UNION;
				param->data = right_word;
				if (!ccs_parse_name_union_quoted(param,
								 names_p++))
					goto out;
			}
			goto store_value;
		}
		right = ccs_condition_type(right_word);
		if (right == CCS_MAX_CONDITION_KEYWORD) {
			if (!numbers_p) {
				e.numbers_count++;
			} else {
				e.numbers_count--;
				right = CCS_NUMBER_UNION;
				param->data = right_word;
				if (!ccs_parse_number_union(param,
							    numbers_p++))
					goto out;
			}
		}
store_value:
		if (!condp) {
			dprintk(KERN_WARNING "%u: dry_run left=%u right=%u "
				"match=%u\n", __LINE__, left, right, !is_not);
			continue;
		}
		condp->left = left;
		condp->right = right;
		condp->equals = !is_not;
		dprintk(KERN_WARNING "%u: left=%u right=%u match=%u\n",
			__LINE__, condp->left, condp->right,
			condp->equals);
		condp++;
	}
	dprintk(KERN_INFO "%u: cond=%u numbers=%u names=%u ac=%u ec=%u\n",
		__LINE__, e.condc, e.numbers_count, e.names_count, e.argc,
		e.envc);
	if (entry) {
		BUG_ON(e.names_count | e.numbers_count | e.argc | e.envc |
		       e.condc);
		return ccs_commit_condition(entry);
	}
	e.size = sizeof(*entry)
		+ e.condc * sizeof(struct ccs_condition_element)
		+ e.numbers_count * sizeof(struct ccs_number_union)
		+ e.names_count * sizeof(struct ccs_name_union)
		+ e.argc * sizeof(struct ccs_argv)
		+ e.envc * sizeof(struct ccs_envp);
	entry = kzalloc(e.size, CCS_GFP_FLAGS);
	if (!entry)
		goto out2;
	*entry = e;
	e.transit = NULL;
	condp = (struct ccs_condition_element *) (entry + 1);
	numbers_p = (struct ccs_number_union *) (condp + e.condc);
	names_p = (struct ccs_name_union *) (numbers_p + e.numbers_count);
	argv = (struct ccs_argv *) (names_p + e.names_count);
	envp = (struct ccs_envp *) (argv + e.argc);
	{
		bool flag = false;
		for (pos = start_of_string; pos < end_of_string; pos++) {
			if (*pos)
				continue;
			if (flag) /* Restore " ". */
				*pos = ' ';
			else if (*(pos + 1) == '=') /* Restore "!=". */
				*pos = '!';
			else /* Restore "=". */
				*pos = '=';
			flag = !flag;
		}
	}
	goto rerun;
out:
	dprintk(KERN_WARNING "%u: %s failed\n", __LINE__, __func__);
	if (entry) {
		ccs_del_condition(&entry->head.list);
		kfree(entry);
	}
out2:
	ccs_put_name(e.transit);
	return NULL;
}

/**
 * ccs_yesno - Return "yes" or "no".
 *
 * @value: Bool value.
 *
 * Returns "yes" if @value is not 0, "no" otherwise.
 */
static const char *ccs_yesno(const unsigned int value)
{
	return value ? "yes" : "no";
}

/**
 * ccs_addprintf - strncat()-like-snprintf().
 *
 * @buffer: Buffer to write to. Must be '\0'-terminated.
 * @len:    Size of @buffer.
 * @fmt:    The printf()'s format string, followed by parameters.
 *
 * Returns nothing.
 */
static void ccs_addprintf(char *buffer, int len, const char *fmt, ...)
{
	va_list args;
	const int pos = strlen(buffer);
	va_start(args, fmt);
	vsnprintf(buffer + pos, len - pos - 1, fmt, args);
	va_end(args);
}

/**
 * ccs_flush - Flush queued string to userspace's buffer.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns true if all data was flushed, false otherwise.
 */
static bool ccs_flush(struct ccs_io_buffer *head)
{
	while (head->r.w_pos) {
		const char *w = head->r.w[0];
		size_t len = strlen(w);
		if (len) {
			if (len > head->read_user_buf_avail)
				len = head->read_user_buf_avail;
			if (!len)
				return false;
			if (copy_to_user(head->read_user_buf, w, len))
				return false;
			head->read_user_buf_avail -= len;
			head->read_user_buf += len;
			w += len;
		}
		head->r.w[0] = w;
		if (*w)
			return false;
		/* Add '\0' for audit logs and query. */
		if (head->type == CCS_AUDIT || head->type == CCS_QUERY) {
			if (!head->read_user_buf_avail ||
			    copy_to_user(head->read_user_buf, "", 1))
				return false;
			head->read_user_buf_avail--;
			head->read_user_buf++;
		}
		head->r.w_pos--;
		for (len = 0; len < head->r.w_pos; len++)
			head->r.w[len] = head->r.w[len + 1];
	}
	head->r.avail = 0;
	return true;
}

/**
 * ccs_set_string - Queue string to "struct ccs_io_buffer" structure.
 *
 * @head:   Pointer to "struct ccs_io_buffer".
 * @string: String to print.
 *
 * Returns nothing.
 *
 * Note that @string has to be kept valid until @head is kfree()d.
 * This means that char[] allocated on stack memory cannot be passed to
 * this function. Use ccs_io_printf() for char[] allocated on stack memory.
 */
static void ccs_set_string(struct ccs_io_buffer *head, const char *string)
{
	if (head->r.w_pos < CCS_MAX_IO_READ_QUEUE) {
		head->r.w[head->r.w_pos++] = string;
		ccs_flush(head);
	} else
		printk(KERN_WARNING "Too many words in a line.\n");
}

/**
 * ccs_io_printf - printf() to "struct ccs_io_buffer" structure.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @fmt:  The printf()'s format string, followed by parameters.
 *
 * Returns nothing.
 */
static void ccs_io_printf(struct ccs_io_buffer *head, const char *fmt, ...)
{
	va_list args;
	size_t len;
	size_t pos = head->r.avail;
	int size = head->readbuf_size - pos;
	if (size <= 0)
		return;
	va_start(args, fmt);
	len = vsnprintf(head->read_buf + pos, size, fmt, args) + 1;
	va_end(args);
	if (pos + len >= head->readbuf_size) {
		printk(KERN_WARNING "Too many words in a line.\n");
		return;
	}
	head->r.avail += len;
	ccs_set_string(head, head->read_buf + pos);
}

/**
 * ccs_set_space - Put a space to "struct ccs_io_buffer" structure.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 */
static void ccs_set_space(struct ccs_io_buffer *head)
{
	ccs_set_string(head, " ");
}

/**
 * ccs_set_lf - Put a line feed to "struct ccs_io_buffer" structure.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns true if all data was flushed, false otherwise.
 */
static bool ccs_set_lf(struct ccs_io_buffer *head)
{
	ccs_set_string(head, "\n");
	return !head->r.w_pos;
}

/**
 * ccs_set_slash - Put a shash to "struct ccs_io_buffer" structure.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 */
static void ccs_set_slash(struct ccs_io_buffer *head)
{
	ccs_set_string(head, "/");
}

/**
 * ccs_init_policy_namespace - Initialize namespace.
 *
 * @ns: Pointer to "struct ccs_policy_namespace".
 *
 * Returns nothing.
 */
static void ccs_init_policy_namespace(struct ccs_policy_namespace *ns)
{
	unsigned int idx;
	for (idx = 0; idx < CCS_MAX_ACL_GROUPS; idx++)
		INIT_LIST_HEAD(&ns->acl_group[idx]);
	for (idx = 0; idx < CCS_MAX_GROUP; idx++)
		INIT_LIST_HEAD(&ns->group_list[idx]);
	for (idx = 0; idx < CCS_MAX_POLICY; idx++)
		INIT_LIST_HEAD(&ns->policy_list[idx]);
	ns->profile_version = 20100903;
	ccs_namespace_enabled = !list_empty(&ccs_namespace_list);
	list_add_tail_rcu(&ns->namespace_list, &ccs_namespace_list);
}

/**
 * ccs_print_namespace - Print namespace header.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 */
static void ccs_print_namespace(struct ccs_io_buffer *head)
{
	if (!ccs_namespace_enabled)
		return;
	ccs_set_string(head,
		       container_of(head->r.ns, struct ccs_policy_namespace,
				    namespace_list)->name);
	ccs_set_space(head);
}

/**
 * ccs_assign_profile - Create a new profile.
 *
 * @ns:      Pointer to "struct ccs_policy_namespace".
 * @profile: Profile number to create.
 *
 * Returns pointer to "struct ccs_profile" on success, NULL otherwise.
 */
static struct ccs_profile *ccs_assign_profile(struct ccs_policy_namespace *ns,
					      const unsigned int profile)
{
	struct ccs_profile *ptr;
	struct ccs_profile *entry;
	if (profile >= CCS_MAX_PROFILES)
		return NULL;
	ptr = ns->profile_ptr[profile];
	if (ptr)
		return ptr;
	entry = kzalloc(sizeof(*entry), CCS_GFP_FLAGS);
	if (mutex_lock_interruptible(&ccs_policy_lock))
		goto out;
	ptr = ns->profile_ptr[profile];
	if (!ptr && ccs_memory_ok(entry, sizeof(*entry))) {
		ptr = entry;
		ptr->default_config = CCS_CONFIG_DISABLED |
			CCS_CONFIG_WANT_GRANT_LOG | CCS_CONFIG_WANT_REJECT_LOG;
		memset(ptr->config, CCS_CONFIG_USE_DEFAULT,
		       sizeof(ptr->config));
		ptr->pref[CCS_PREF_MAX_AUDIT_LOG] =
			CONFIG_CCSECURITY_MAX_AUDIT_LOG;
		ptr->pref[CCS_PREF_MAX_LEARNING_ENTRY] =
			CONFIG_CCSECURITY_MAX_ACCEPT_ENTRY;
		mb(); /* Avoid out-of-order execution. */
		ns->profile_ptr[profile] = ptr;
		entry = NULL;
	}
	mutex_unlock(&ccs_policy_lock);
out:
	kfree(entry);
	return ptr;
}

/**
 * ccs_check_profile - Check all profiles currently assigned to domains are defined.
 *
 * Returns nothing.
 */
static void ccs_check_profile(void)
{
	struct ccs_domain_info *domain;
	const int idx = ccs_read_lock();
	ccs_policy_loaded = true;
	printk(KERN_INFO "CCSecurity: 1.8.3+   2012/05/05\n");
	list_for_each_entry_srcu(domain, &ccs_domain_list, list, &ccs_ss) {
		const u8 profile = domain->profile;
		const struct ccs_policy_namespace *ns = domain->ns;
		if (ns->profile_version != 20100903)
			printk(KERN_ERR
			       "Profile version %u is not supported.\n",
			       ns->profile_version);
		else if (!ns->profile_ptr[profile])
			printk(KERN_ERR
			       "Profile %u (used by '%s') is not defined.\n",
			       profile, domain->domainname->name);
		else
			continue;
		printk(KERN_ERR
		       "Userland tools for TOMOYO 1.8 must be installed and "
		       "policy must be initialized.\n");
		printk(KERN_ERR "Please see http://tomoyo.sourceforge.jp/1.8/ "
		       "for more information.\n");
		panic("STOP!");
	}
	ccs_read_unlock(idx);
	printk(KERN_INFO "Mandatory Access Control activated.\n");
}

/**
 * ccs_profile - Find a profile.
 *
 * @profile: Profile number to find.
 *
 * Returns pointer to "struct ccs_profile".
 */
static struct ccs_profile *ccs_profile(const u8 profile)
{
	static struct ccs_profile ccs_null_profile;
	struct ccs_profile *ptr = ccs_current_namespace()->
		profile_ptr[profile];
	if (!ptr)
		ptr = &ccs_null_profile;
	return ptr;
}

/**
 * ccs_get_config - Get config for specified profile's specified functionality.
 *
 * @profile: Profile number.
 * @index:   Index number of functionality.
 *
 * Returns config.
 *
 * First, check for CONFIG::category::functionality.
 * If CONFIG::category::functionality is set to use default, then check
 * CONFIG::category. If CONFIG::category is set to use default, then use
 * CONFIG. CONFIG cannot be set to use default.
 */
u8 ccs_get_config(const u8 profile, const u8 index)
{
	u8 config;
	const struct ccs_profile *p;
	if (!ccs_policy_loaded)
		return CCS_CONFIG_DISABLED;
	p = ccs_profile(profile);
	config = p->config[index];
	if (config == CCS_CONFIG_USE_DEFAULT)
		config = p->config[ccs_index2category[index]
				   + CCS_MAX_MAC_INDEX];
	if (config == CCS_CONFIG_USE_DEFAULT)
		config = p->default_config;
	return config;
}

/**
 * ccs_find_yesno - Find values for specified keyword.
 *
 * @string: String to check.
 * @find:   Name of keyword.
 *
 * Returns 1 if "@find=yes" was found, 0 if "@find=no" was found, -1 otherwise.
 */
static s8 ccs_find_yesno(const char *string, const char *find)
{
	const char *cp = strstr(string, find);
	if (cp) {
		cp += strlen(find);
		if (!strncmp(cp, "=yes", 4))
			return 1;
		else if (!strncmp(cp, "=no", 3))
			return 0;
	}
	return -1;
}

/**
 * ccs_set_uint - Set value for specified preference.
 *
 * @i:      Pointer to "unsigned int".
 * @string: String to check.
 * @find:   Name of keyword.
 *
 * Returns nothing.
 */
static void ccs_set_uint(unsigned int *i, const char *string, const char *find)
{
	const char *cp = strstr(string, find);
	if (cp)
		sscanf(cp + strlen(find), "=%u", i);
}

/**
 * ccs_str_starts - Check whether the given string starts with the given keyword.
 *
 * @src:  Pointer to pointer to the string.
 * @find: Pointer to the keyword.
 *
 * Returns true if @src starts with @find, false otherwise.
 *
 * The @src is updated to point the first character after the @find
 * if @src starts with @find.
 */
static bool ccs_str_starts(char **src, const char *find)
{
	const int len = strlen(find);
	char *tmp = *src;
	if (strncmp(tmp, find, len))
		return false;
	tmp += len;
	*src = tmp;
	return true;
}

/**
 * ccs_print_group - Print group's name.
 *
 * @head:  Pointer to "struct ccs_io_buffer".
 * @group: Pointer to "struct ccsgroup". Maybe NULL.
 *
 * Returns true if @group is not NULL. false otherwise.
 */
static bool ccs_print_group(struct ccs_io_buffer *head,
			    const struct ccs_group *group)
{
	if (group) {
		ccs_set_string(head, "@");
		ccs_set_string(head, group->group_name->name);
		return true;
	}
	return false;
}

/**
 * ccs_set_mode - Set mode for specified profile.
 *
 * @name:    Name of functionality.
 * @value:   Mode for @name.
 * @profile: Pointer to "struct ccs_profile".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_set_mode(char *name, const char *value,
			struct ccs_profile *profile)
{
	u8 i;
	u8 config;
	if (!strcmp(name, "CONFIG")) {
		i = CCS_MAX_MAC_INDEX + CCS_MAX_MAC_CATEGORY_INDEX;
		config = profile->default_config;
	} else if (ccs_str_starts(&name, "CONFIG::")) {
		config = 0;
		for (i = 0; i < CCS_MAX_MAC_INDEX + CCS_MAX_MAC_CATEGORY_INDEX;
		     i++) {
			int len = 0;
			if (i < CCS_MAX_MAC_INDEX) {
				const u8 c = ccs_index2category[i];
				const char *category =
					ccs_category_keywords[c];
				len = strlen(category);
				if (strncmp(name, category, len) ||
				    name[len++] != ':' || name[len++] != ':')
					continue;
			}
			if (strcmp(name + len, ccs_mac_keywords[i]))
				continue;
			config = profile->config[i];
			break;
		}
		if (i == CCS_MAX_MAC_INDEX + CCS_MAX_MAC_CATEGORY_INDEX)
			return -EINVAL;
	} else {
		return -EINVAL;
	}
	if (strstr(value, "use_default")) {
		config = CCS_CONFIG_USE_DEFAULT;
	} else {
		u8 mode;
		for (mode = 0; mode < CCS_CONFIG_MAX_MODE; mode++)
			if (strstr(value, ccs_mode[mode]))
				/*
				 * Update lower 3 bits in order to distinguish
				 * 'config' from 'CCS_CONFIG_USE_DEAFULT'.
				 */
				config = (config & ~7) | mode;
		if (config != CCS_CONFIG_USE_DEFAULT) {
			switch (ccs_find_yesno(value, "grant_log")) {
			case 1:
				config |= CCS_CONFIG_WANT_GRANT_LOG;
				break;
			case 0:
				config &= ~CCS_CONFIG_WANT_GRANT_LOG;
				break;
			}
			switch (ccs_find_yesno(value, "reject_log")) {
			case 1:
				config |= CCS_CONFIG_WANT_REJECT_LOG;
				break;
			case 0:
				config &= ~CCS_CONFIG_WANT_REJECT_LOG;
				break;
			}
		}
	}
	if (i < CCS_MAX_MAC_INDEX + CCS_MAX_MAC_CATEGORY_INDEX)
		profile->config[i] = config;
	else if (config != CCS_CONFIG_USE_DEFAULT)
		profile->default_config = config;
	return 0;
}

/**
 * ccs_write_profile - Write profile table.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_write_profile(struct ccs_io_buffer *head)
{
	char *data = head->write_buf;
	unsigned int i;
	char *cp;
	struct ccs_profile *profile;
	if (sscanf(data, "PROFILE_VERSION=%u", &head->w.ns->profile_version)
	    == 1)
		return 0;
	i = simple_strtoul(data, &cp, 10);
	if (*cp != '-')
		return -EINVAL;
	data = cp + 1;
	profile = ccs_assign_profile(head->w.ns, i);
	if (!profile)
		return -EINVAL;
	cp = strchr(data, '=');
	if (!cp)
		return -EINVAL;
	*cp++ = '\0';
	if (!strcmp(data, "COMMENT")) {
		static DEFINE_SPINLOCK(lock);
		const struct ccs_path_info *new_comment = ccs_get_name(cp);
		const struct ccs_path_info *old_comment;
		if (!new_comment)
			return -ENOMEM;
		spin_lock(&lock);
		old_comment = profile->comment;
		profile->comment = new_comment;
		spin_unlock(&lock);
		ccs_put_name(old_comment);
		return 0;
	}
	if (!strcmp(data, "PREFERENCE")) {
		for (i = 0; i < CCS_MAX_PREF; i++)
			ccs_set_uint(&profile->pref[i], cp,
				     ccs_pref_keywords[i]);
		return 0;
	}
	return ccs_set_mode(data, cp, profile);
}

/**
 * ccs_print_config - Print mode for specified functionality.
 *
 * @head:   Pointer to "struct ccs_io_buffer".
 * @config: Mode for that functionality.
 *
 * Returns nothing.
 *
 * Caller prints functionality's name.
 */
static void ccs_print_config(struct ccs_io_buffer *head, const u8 config)
{
	ccs_io_printf(head, "={ mode=%s grant_log=%s reject_log=%s }\n",
		      ccs_mode[config & 3],
		      ccs_yesno(config & CCS_CONFIG_WANT_GRANT_LOG),
		      ccs_yesno(config & CCS_CONFIG_WANT_REJECT_LOG));
}

/**
 * ccs_read_profile - Read profile table.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 */
static void ccs_read_profile(struct ccs_io_buffer *head)
{
	u8 index;
	struct ccs_policy_namespace *ns = container_of(head->r.ns, typeof(*ns),
						       namespace_list);
	const struct ccs_profile *profile;
	if (head->r.eof)
		return;
next:
	index = head->r.index;
	profile = ns->profile_ptr[index];
	switch (head->r.step) {
	case 0:
		ccs_print_namespace(head);
		ccs_io_printf(head, "PROFILE_VERSION=%u\n",
			      ns->profile_version);
		head->r.step++;
		break;
	case 1:
		for ( ; head->r.index < CCS_MAX_PROFILES; head->r.index++)
			if (ns->profile_ptr[head->r.index])
				break;
		if (head->r.index == CCS_MAX_PROFILES) {
			head->r.eof = true;
			return;
		}
		head->r.step++;
		break;
	case 2:
		{
			u8 i;
			const struct ccs_path_info *comment = profile->comment;
			ccs_print_namespace(head);
			ccs_io_printf(head, "%u-COMMENT=", index);
			ccs_set_string(head, comment ? comment->name : "");
			ccs_set_lf(head);
			ccs_print_namespace(head);
			ccs_io_printf(head, "%u-PREFERENCE={ ", index);
			for (i = 0; i < CCS_MAX_PREF; i++)
				ccs_io_printf(head, "%s=%u ",
					      ccs_pref_keywords[i],
					      profile->pref[i]);
			ccs_set_string(head, "}\n");
			head->r.step++;
		}
		break;
	case 3:
		{
			ccs_print_namespace(head);
			ccs_io_printf(head, "%u-%s", index, "CONFIG");
			ccs_print_config(head, profile->default_config);
			head->r.bit = 0;
			head->r.step++;
		}
		break;
	case 4:
		for ( ; head->r.bit < CCS_MAX_MAC_INDEX
			      + CCS_MAX_MAC_CATEGORY_INDEX; head->r.bit++) {
			const u8 i = head->r.bit;
			const u8 config = profile->config[i];
			if (config == CCS_CONFIG_USE_DEFAULT)
				continue;
			ccs_print_namespace(head);
			if (i < CCS_MAX_MAC_INDEX)
				ccs_io_printf(head, "%u-CONFIG::%s::%s", index,
					      ccs_category_keywords
					      [ccs_index2category[i]],
					      ccs_mac_keywords[i]);
			else
				ccs_io_printf(head, "%u-CONFIG::%s", index,
					      ccs_mac_keywords[i]);
			ccs_print_config(head, config);
			head->r.bit++;
			break;
		}
		if (head->r.bit == CCS_MAX_MAC_INDEX
		    + CCS_MAX_MAC_CATEGORY_INDEX) {
			head->r.index++;
			head->r.step = 1;
		}
		break;
	}
	if (ccs_flush(head))
		goto next;
}

/**
 * ccs_update_policy - Update an entry for exception policy.
 *
 * @size:  Size of new entry in bytes.
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_update_policy(const int size, struct ccs_acl_param *param)
{
	struct ccs_acl_head *new_entry = &param->e.acl_head;
	int error = param->is_delete ? -ENOENT : -ENOMEM;
	struct ccs_acl_head *entry;
	struct list_head *list = param->list;
	BUG_ON(size < sizeof(*entry));
	if (mutex_lock_interruptible(&ccs_policy_lock))
		return -ENOMEM;
	list_for_each_entry_srcu(entry, list, list, &ccs_ss) {
		if (entry->is_deleted == CCS_GC_IN_PROGRESS)
			continue;
		if (memcmp(entry + 1, new_entry + 1, size - sizeof(*entry)))
			continue;
		entry->is_deleted = param->is_delete;
		error = 0;
		break;
	}
	if (error && !param->is_delete) {
		entry = ccs_commit_ok(new_entry, size);
		if (entry) {
			list_add_tail_rcu(&entry->list, list);
			error = 0;
		}
	}
	mutex_unlock(&ccs_policy_lock);
	return error;
}

/**
 * ccs_update_manager_entry - Add a manager entry.
 *
 * @manager:   The path to manager or the domainnamme.
 * @is_delete: True if it is a delete request.
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_update_manager_entry(const char *manager,
				    const bool is_delete)
{
	struct ccs_acl_param param = {
		/* .ns = &ccs_kernel_namespace, */
		.is_delete = is_delete,
		.list = &ccs_kernel_namespace.policy_list[CCS_ID_MANAGER],
	};
	struct ccs_manager *e = &param.e.manager;
	int error = is_delete ? -ENOENT : -ENOMEM;
	/* Forced zero clear for using memcmp() at ccs_update_policy(). */
	memset(&param.e, 0, sizeof(param.e));
	if (!ccs_correct_domain(manager) && !ccs_correct_word(manager))
		return -EINVAL;
	e->manager = ccs_get_name(manager);
	if (e->manager) {
		error = ccs_update_policy(sizeof(*e), &param);
		ccs_put_name(e->manager);
	}
	return error;
}

/**
 * ccs_write_manager - Write manager policy.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_write_manager(struct ccs_io_buffer *head)
{
	const char *data = head->write_buf;
	if (!strcmp(data, "manage_by_non_root")) {
		ccs_manage_by_non_root = !head->w.is_delete;
		return 0;
	}
	return ccs_update_manager_entry(data, head->w.is_delete);
}

/**
 * ccs_read_manager - Read manager policy.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 *
 * Caller holds ccs_read_lock().
 */
static void ccs_read_manager(struct ccs_io_buffer *head)
{
	if (head->r.eof)
		return;
	list_for_each_cookie(head->r.acl, &ccs_kernel_namespace.
			     policy_list[CCS_ID_MANAGER]) {
		struct ccs_manager *ptr =
			list_entry(head->r.acl, typeof(*ptr), head.list);
		if (ptr->head.is_deleted)
			continue;
		if (!ccs_flush(head))
			return;
		ccs_set_string(head, ptr->manager->name);
		ccs_set_lf(head);
	}
	head->r.eof = true;
}

/**
 * ccs_manager - Check whether the current process is a policy manager.
 *
 * Returns true if the current process is permitted to modify policy
 * via /proc/ccs/ interface.
 *
 * Caller holds ccs_read_lock().
 */
static bool ccs_manager(void)
{
	struct ccs_manager *ptr;
	struct ccs_path_info exe;
	struct ccs_security *task = ccs_current_security();
	const struct ccs_path_info *domainname
		= ccs_current_domain()->domainname;
	bool found = false;
	if (!ccs_policy_loaded)
		return true;
	if (task->ccs_flags & CCS_TASK_IS_MANAGER)
		return true;
	if (!ccs_manage_by_non_root &&
	    (!uid_eq(current_uid(), GLOBAL_ROOT_UID) ||
	     !uid_eq(current_euid(), GLOBAL_ROOT_UID)))
		return false;
	exe.name = ccs_get_exe();
	if (!exe.name)
		return false;
	ccs_fill_path_info(&exe);
	list_for_each_entry_srcu(ptr, &ccs_kernel_namespace.
				 policy_list[CCS_ID_MANAGER], head.list,
				 &ccs_ss) {
		if (ptr->head.is_deleted)
			continue;
		if (ccs_pathcmp(domainname, ptr->manager) &&
		    ccs_pathcmp(&exe, ptr->manager))
			continue;
		/* Set manager flag. */
		task->ccs_flags |= CCS_TASK_IS_MANAGER;
		found = true;
		break;
	}
	if (!found) { /* Reduce error messages. */
		static pid_t ccs_last_pid;
		const pid_t pid = current->pid;
		if (ccs_last_pid != pid) {
			printk(KERN_WARNING "%s ( %s ) is not permitted to "
			       "update policies.\n", domainname->name,
			       exe.name);
			ccs_last_pid = pid;
		}
	}
	kfree(exe.name);
	return found;
}

/**
 * ccs_find_domain - Find a domain by the given name.
 *
 * @domainname: The domainname to find.
 *
 * Returns pointer to "struct ccs_domain_info" if found, NULL otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static struct ccs_domain_info *ccs_find_domain(const char *domainname)
{
	struct ccs_domain_info *domain;
	struct ccs_path_info name;
	name.name = domainname;
	ccs_fill_path_info(&name);
	list_for_each_entry_srcu(domain, &ccs_domain_list, list, &ccs_ss) {
		if (!domain->is_deleted &&
		    !ccs_pathcmp(&name, domain->domainname))
			return domain;
	}
	return NULL;
}

/**
 * ccs_select_domain - Parse select command.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @data: String to parse.
 *
 * Returns true on success, false otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static bool ccs_select_domain(struct ccs_io_buffer *head, const char *data)
{
	unsigned int pid;
	struct ccs_domain_info *domain = NULL;
	bool global_pid = false;
	if (strncmp(data, "select ", 7))
		return false;
	data += 7;
	if (sscanf(data, "pid=%u", &pid) == 1 ||
	    (global_pid = true, sscanf(data, "global-pid=%u", &pid) == 1)) {
		struct task_struct *p;
		ccs_tasklist_lock();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
		if (global_pid)
			p = ccsecurity_exports.find_task_by_pid_ns(pid,
							       &init_pid_ns);
		else
			p = ccsecurity_exports.find_task_by_vpid(pid);
#else
		p = find_task_by_pid(pid);
#endif
		if (p)
			domain = ccs_task_domain(p);
		ccs_tasklist_unlock();
	} else if (!strncmp(data, "domain=", 7)) {
		if (*(data + 7) == '<')
			domain = ccs_find_domain(data + 7);
	} else if (sscanf(data, "Q=%u", &pid) == 1) {
		domain = ccs_find_domain_by_qid(pid);
	} else
		return false;
	head->w.domain = domain;
	/* Accessing read_buf is safe because head->io_sem is held. */
	if (!head->read_buf)
		return true; /* Do nothing if open(O_WRONLY). */
	memset(&head->r, 0, sizeof(head->r));
	head->r.print_this_domain_only = true;
	if (domain)
		head->r.domain = &domain->list;
	else
		head->r.eof = true;
	ccs_io_printf(head, "# select %s\n", data);
	if (domain && domain->is_deleted)
		ccs_set_string(head, "# This is a deleted domain.\n");
	return true;
}

/**
 * ccs_update_acl - Update "struct ccs_acl_info" entry.
 *
 * @size:  Size of new entry in bytes.
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_update_acl(const int size, struct ccs_acl_param *param)
{
	struct ccs_acl_info *new_entry = &param->e.acl_info;
	const bool is_delete = param->is_delete;
	int error = is_delete ? -ENOENT : -ENOMEM;
	struct ccs_acl_info *entry;
	struct list_head * const list = param->list;
	BUG_ON(size < sizeof(*entry));
	if (param->data[0]) {
		new_entry->cond = ccs_get_condition(param);
		if (!new_entry->cond)
			return -EINVAL;
		/*
		 * Domain transition preference is allowed for only
		 * "file execute"/"task auto_execute_handler"/
		 * "task denied_auto_execute_handler" entries.
		 */
		if (new_entry->cond->exec_transit &&
		    !(new_entry->type == CCS_TYPE_PATH_ACL &&
		      new_entry->perm == 1 << CCS_TYPE_EXECUTE)
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
		    && new_entry->type != CCS_TYPE_AUTO_EXECUTE_HANDLER &&
		    new_entry->type != CCS_TYPE_DENIED_EXECUTE_HANDLER
#endif
		    )
			return -EINVAL;
	}
	if (mutex_lock_interruptible(&ccs_policy_lock))
		return -ENOMEM;
	list_for_each_entry_srcu(entry, list, list, &ccs_ss) {
		if (entry->is_deleted == CCS_GC_IN_PROGRESS)
			continue;
		if (entry->type != new_entry->type ||
		    entry->cond != new_entry->cond ||
		    memcmp(entry + 1, new_entry + 1, size - sizeof(*entry)))
			continue;
		if (is_delete)
			entry->perm &= ~new_entry->perm;
		else
			entry->perm |= new_entry->perm;
		entry->is_deleted = !entry->perm;
		error = 0;
		break;
	}
	if (error && !is_delete) {
		entry = ccs_commit_ok(new_entry, size);
		if (entry) {
			list_add_tail_rcu(&entry->list, list);
			error = 0;
		}
	}
	mutex_unlock(&ccs_policy_lock);
	return error;
}

/**
 * ccs_permstr - Find permission keywords.
 *
 * @string: String representation for permissions in foo/bar/buz format.
 * @keyword: Keyword to find from @string/
 *
 * Returns ture if @keyword was found in @string, false otherwise.
 *
 * This function assumes that strncmp(w1, w2, strlen(w1)) != 0 if w1 != w2.
 */
static bool ccs_permstr(const char *string, const char *keyword)
{
	const char *cp = strstr(string, keyword);
	if (cp)
		return cp == string || *(cp - 1) == '/';
	return false;
}

/**
 * ccs_write_task - Update task related list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_write_task(struct ccs_acl_param *param)
{
	int error;
	const bool is_auto = ccs_str_starts(&param->data,
					    "auto_domain_transition ");
	if (!is_auto && !ccs_str_starts(&param->data,
					"manual_domain_transition ")) {
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
		struct ccs_handler_acl *e = &param->e.handler_acl;
		char *handler;
		if (ccs_str_starts(&param->data, "auto_execute_handler "))
			e->head.type = CCS_TYPE_AUTO_EXECUTE_HANDLER;
		else if (ccs_str_starts(&param->data,
					"denied_execute_handler "))
			e->head.type = CCS_TYPE_DENIED_EXECUTE_HANDLER;
		else
			return -EINVAL;
		handler = ccs_read_token(param);
		if (!ccs_correct_path(handler))
			return -EINVAL;
		e->handler = ccs_get_name(handler);
		if (!e->handler)
			return -ENOMEM;
		if (e->handler->is_patterned)
			return -EINVAL; /* No patterns allowed. */
		return ccs_update_acl(sizeof(*e), param);
#else
		error = -EINVAL;
#endif
	} else {
#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION
		struct ccs_task_acl *e = &param->e.task_acl;
		e->head.type = is_auto ?
			CCS_TYPE_AUTO_TASK_ACL : CCS_TYPE_MANUAL_TASK_ACL;
		e->domainname = ccs_get_domainname(param);
		if (!e->domainname)
			return -EINVAL;
		return ccs_update_acl(sizeof(*e), param);
#else
		error = -EINVAL;
#endif
	}
	return error;
}

#ifdef CONFIG_CCSECURITY_NETWORK

/**
 * ccs_write_inet_network - Write "struct ccs_inet_acl" list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_write_inet_network(struct ccs_acl_param *param)
{
	struct ccs_inet_acl *e = &param->e.inet_acl;
	u8 type;
	const char *protocol = ccs_read_token(param);
	const char *operation = ccs_read_token(param);
	e->head.type = CCS_TYPE_INET_ACL;
	for (type = 0; type < CCS_SOCK_MAX; type++)
		if (!strcmp(protocol, ccs_proto_keyword[type]))
			break;
	if (type == CCS_SOCK_MAX)
		return -EINVAL;
	e->protocol = type;
	e->head.perm = 0;
	for (type = 0; type < CCS_MAX_NETWORK_OPERATION; type++)
		if (ccs_permstr(operation, ccs_socket_keyword[type]))
			e->head.perm |= 1 << type;
	if (!e->head.perm)
		return -EINVAL;
	if (param->data[0] == '@') {
		param->data++;
		e->address.group = ccs_get_group(param, CCS_ADDRESS_GROUP);
		if (!e->address.group)
			return -ENOMEM;
	} else {
		if (!ccs_parse_ipaddr_union(param, &e->address))
			return -EINVAL;
	}
	if (!ccs_parse_number_union(param, &e->port) ||
	    e->port.values[1] > 65535)
		return -EINVAL;
	return ccs_update_acl(sizeof(*e), param);
}

/**
 * ccs_write_unix_network - Write "struct ccs_unix_acl" list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_write_unix_network(struct ccs_acl_param *param)
{
	struct ccs_unix_acl *e = &param->e.unix_acl;
	u8 type;
	const char *protocol = ccs_read_token(param);
	const char *operation = ccs_read_token(param);
	e->head.type = CCS_TYPE_UNIX_ACL;
	for (type = 0; type < CCS_SOCK_MAX; type++)
		if (!strcmp(protocol, ccs_proto_keyword[type]))
			break;
	if (type == CCS_SOCK_MAX)
		return -EINVAL;
	e->protocol = type;
	e->head.perm = 0;
	for (type = 0; type < CCS_MAX_NETWORK_OPERATION; type++)
		if (ccs_permstr(operation, ccs_socket_keyword[type]))
			e->head.perm |= 1 << type;
	if (!e->head.perm)
		return -EINVAL;
	if (!ccs_parse_name_union(param, &e->name))
		return -EINVAL;
	return ccs_update_acl(sizeof(*e), param);
}

#endif

/**
 * ccs_write_file - Update file related list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_write_file(struct ccs_acl_param *param)
{
	u16 perm = 0;
	u8 type;
	const char *operation = ccs_read_token(param);
	for (type = 0; type < CCS_MAX_PATH_OPERATION; type++)
		if (ccs_permstr(operation, ccs_path_keyword[type]))
			perm |= 1 << type;
	if (perm) {
		struct ccs_path_acl *e = &param->e.path_acl;
		e->head.type = CCS_TYPE_PATH_ACL;
		e->head.perm = perm;
		if (!ccs_parse_name_union(param, &e->name))
			return -EINVAL;
		return ccs_update_acl(sizeof(*e), param);
	}
	for (type = 0; type < CCS_MAX_PATH2_OPERATION; type++)
		if (ccs_permstr(operation, ccs_mac_keywords[ccs_pp2mac[type]]))
			perm |= 1 << type;
	if (perm) {
		struct ccs_path2_acl *e = &param->e.path2_acl;
		e->head.type = CCS_TYPE_PATH2_ACL;
		e->head.perm = perm;
		if (!ccs_parse_name_union(param, &e->name1) ||
		    !ccs_parse_name_union(param, &e->name2))
			return -EINVAL;
		return ccs_update_acl(sizeof(*e), param);
	}
	for (type = 0; type < CCS_MAX_PATH_NUMBER_OPERATION; type++)
		if (ccs_permstr(operation, ccs_mac_keywords[ccs_pn2mac[type]]))
			perm |= 1 << type;
	if (perm) {
		struct ccs_path_number_acl *e = &param->e.path_number_acl;
		e->head.type = CCS_TYPE_PATH_NUMBER_ACL;
		e->head.perm = perm;
		if (!ccs_parse_name_union(param, &e->name) ||
		    !ccs_parse_number_union(param, &e->number))
			return -EINVAL;
		return ccs_update_acl(sizeof(*e), param);
	}
	for (type = 0; type < CCS_MAX_MKDEV_OPERATION; type++)
		if (ccs_permstr(operation,
				ccs_mac_keywords[ccs_pnnn2mac[type]]))
			perm |= 1 << type;
	if (perm) {
		struct ccs_mkdev_acl *e = &param->e.mkdev_acl;
		e->head.type = CCS_TYPE_MKDEV_ACL;
		e->head.perm = perm;
		if (!ccs_parse_name_union(param, &e->name) ||
		    !ccs_parse_number_union(param, &e->mode) ||
		    !ccs_parse_number_union(param, &e->major) ||
		    !ccs_parse_number_union(param, &e->minor))
			return -EINVAL;
		return ccs_update_acl(sizeof(*e), param);
	}
	if (ccs_permstr(operation, ccs_mac_keywords[CCS_MAC_FILE_MOUNT])) {
		struct ccs_mount_acl *e = &param->e.mount_acl;
		e->head.type = CCS_TYPE_MOUNT_ACL;
		if (!ccs_parse_name_union(param, &e->dev_name) ||
		    !ccs_parse_name_union(param, &e->dir_name) ||
		    !ccs_parse_name_union(param, &e->fs_type) ||
		    !ccs_parse_number_union(param, &e->flags))
			return -EINVAL;
		return ccs_update_acl(sizeof(*e), param);
	}
	return -EINVAL;
}

#ifdef CONFIG_CCSECURITY_MISC

/**
 * ccs_write_misc - Update environment variable list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_write_misc(struct ccs_acl_param *param)
{
	if (ccs_str_starts(&param->data, "env ")) {
		struct ccs_env_acl *e = &param->e.env_acl;
		const char *data = ccs_read_token(param);
		e->head.type = CCS_TYPE_ENV_ACL;
		if (!ccs_correct_word(data) || strchr(data, '='))
			return -EINVAL;
		e->env = ccs_get_name(data);
		if (!e->env)
			return -ENOMEM;
		return ccs_update_acl(sizeof(*e), param);
	}
	return -EINVAL;
}

#endif

#ifdef CONFIG_CCSECURITY_IPC

/**
 * ccs_write_ipc - Update "struct ccs_signal_acl" list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_write_ipc(struct ccs_acl_param *param)
{
	struct ccs_signal_acl *e = &param->e.signal_acl;
	e->head.type = CCS_TYPE_SIGNAL_ACL;
	if (!ccs_parse_number_union(param, &e->sig))
		return -EINVAL;
	e->domainname = ccs_get_domainname(param);
	if (!e->domainname)
		return -EINVAL;
	return ccs_update_acl(sizeof(*e), param);
}

#endif

#ifdef CONFIG_CCSECURITY_CAPABILITY

/**
 * ccs_write_capability - Write "struct ccs_capability_acl" list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_write_capability(struct ccs_acl_param *param)
{
	struct ccs_capability_acl *e = &param->e.capability_acl;
	const char *operation = ccs_read_token(param);
	u8 type;
	e->head.type = CCS_TYPE_CAPABILITY_ACL;
	for (type = 0; type < CCS_MAX_CAPABILITY_INDEX; type++) {
		if (strcmp(operation, ccs_mac_keywords[ccs_c2mac[type]]))
			continue;
		e->operation = type;
		return ccs_update_acl(sizeof(*e), param);
	}
	return -EINVAL;
}

#endif

/**
 * ccs_write_acl - Write "struct ccs_acl_info" list.
 *
 * @ns:        Pointer to "struct ccs_policy_namespace".
 * @list:      Pointer to "struct list_head".
 * @data:      Policy to be interpreted.
 * @is_delete: True if it is a delete request.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_write_acl(struct ccs_policy_namespace *ns,
			 struct list_head *list, char *data,
			 const bool is_delete)
{
	struct ccs_acl_param param = {
		.ns = ns,
		.list = list,
		.data = data,
		.is_delete = is_delete,
	};
	static const struct {
		const char *keyword;
		int (*write) (struct ccs_acl_param *);
	} ccs_callback[] = {
		{ "file ", ccs_write_file },
#ifdef CONFIG_CCSECURITY_NETWORK
		{ "network inet ", ccs_write_inet_network },
		{ "network unix ", ccs_write_unix_network },
#endif
#ifdef CONFIG_CCSECURITY_MISC
		{ "misc ", ccs_write_misc },
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
		{ "capability ", ccs_write_capability },
#endif
#ifdef CONFIG_CCSECURITY_IPC
		{ "ipc signal ", ccs_write_ipc },
#endif
		{ "task ", ccs_write_task },
	};
	u8 i;
	/* Forced zero clear for using memcmp() at ccs_update_acl(). */
	memset(&param.e, 0, sizeof(param.e));
	param.e.acl_info.perm = 1;
	for (i = 0; i < ARRAY_SIZE(ccs_callback); i++) {
		int error;
		if (!ccs_str_starts(&param.data, ccs_callback[i].keyword))
			continue;
		error = ccs_callback[i].write(&param);
		ccs_del_acl(&param.e.acl_info.list);
		return error;
	}
	return -EINVAL;
}

/**
 * ccs_delete_domain - Delete a domain.
 *
 * @domainname: The name of domain.
 *
 * Returns 0.
 */
static int ccs_delete_domain(char *domainname)
{
	struct ccs_domain_info *domain;
	struct ccs_path_info name;
	name.name = domainname;
	ccs_fill_path_info(&name);
	if (mutex_lock_interruptible(&ccs_policy_lock))
		return 0;
	/* Is there an active domain? */
	list_for_each_entry_srcu(domain, &ccs_domain_list, list, &ccs_ss) {
		/* Never delete ccs_kernel_domain. */
		if (domain == &ccs_kernel_domain)
			continue;
		if (domain->is_deleted ||
		    ccs_pathcmp(domain->domainname, &name))
			continue;
		domain->is_deleted = true;
		break;
	}
	mutex_unlock(&ccs_policy_lock);
	return 0;
}

/**
 * ccs_write_domain - Write domain policy.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_write_domain(struct ccs_io_buffer *head)
{
	char *data = head->write_buf;
	struct ccs_policy_namespace *ns;
	struct ccs_domain_info *domain = head->w.domain;
	const bool is_delete = head->w.is_delete;
	const bool is_select = !is_delete && ccs_str_starts(&data, "select ");
	unsigned int profile;
	if (*data == '<') {
		domain = NULL;
		if (is_delete)
			ccs_delete_domain(data);
		else if (is_select)
			domain = ccs_find_domain(data);
		else
			domain = ccs_assign_domain(data, false);
		head->w.domain = domain;
		return 0;
	}
	if (!domain)
		return -EINVAL;
	ns = domain->ns;
	if (sscanf(data, "use_profile %u\n", &profile) == 1
	    && profile < CCS_MAX_PROFILES) {
		if (!ccs_policy_loaded || ns->profile_ptr[(u8) profile])
			if (!is_delete)
				domain->profile = (u8) profile;
		return 0;
	}
	if (sscanf(data, "use_group %u\n", &profile) == 1
	    && profile < CCS_MAX_ACL_GROUPS) {
		if (!is_delete)
			domain->group = (u8) profile;
		return 0;
	}
	for (profile = 0; profile < CCS_MAX_DOMAIN_INFO_FLAGS; profile++) {
		const char *cp = ccs_dif[profile];
		if (strncmp(data, cp, strlen(cp) - 1))
			continue;
		domain->flags[profile] = !is_delete;
		return 0;
	}
	return ccs_write_acl(ns, &domain->acl_info_list, data, is_delete);
}

/**
 * ccs_print_name_union - Print a ccs_name_union.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @ptr:  Pointer to "struct ccs_name_union".
 *
 * Returns nothing.
 */
static void ccs_print_name_union(struct ccs_io_buffer *head,
				 const struct ccs_name_union *ptr)
{
	ccs_set_space(head);
	if (!ccs_print_group(head, ptr->group))
		ccs_set_string(head, ptr->filename->name);
}

/**
 * ccs_print_name_union_quoted - Print a ccs_name_union with a quote.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @ptr:  Pointer to "struct ccs_name_union".
 *
 * Returns nothing.
 */
static void ccs_print_name_union_quoted(struct ccs_io_buffer *head,
					const struct ccs_name_union *ptr)
{
	if (!ccs_print_group(head, ptr->group)) {
		ccs_set_string(head, "\"");
		ccs_set_string(head, ptr->filename->name);
		ccs_set_string(head, "\"");
	}
}

/**
 * ccs_print_number_union_nospace - Print a ccs_number_union without a space.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @ptr:  Pointer to "struct ccs_number_union".
 *
 * Returns nothing.
 */
static void ccs_print_number_union_nospace(struct ccs_io_buffer *head,
					   const struct ccs_number_union *ptr)
{
	if (!ccs_print_group(head, ptr->group)) {
		int i;
		unsigned long min = ptr->values[0];
		const unsigned long max = ptr->values[1];
		u8 min_type = ptr->value_type[0];
		const u8 max_type = ptr->value_type[1];
		char buffer[128];
		buffer[0] = '\0';
		for (i = 0; i < 2; i++) {
			switch (min_type) {
			case CCS_VALUE_TYPE_HEXADECIMAL:
				ccs_addprintf(buffer, sizeof(buffer), "0x%lX",
					      min);
				break;
			case CCS_VALUE_TYPE_OCTAL:
				ccs_addprintf(buffer, sizeof(buffer), "0%lo",
					      min);
				break;
			default:
				ccs_addprintf(buffer, sizeof(buffer), "%lu",
					      min);
				break;
			}
			if (min == max && min_type == max_type)
				break;
			ccs_addprintf(buffer, sizeof(buffer), "-");
			min_type = max_type;
			min = max;
		}
		ccs_io_printf(head, "%s", buffer);
	}
}

/**
 * ccs_print_number_union - Print a ccs_number_union.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @ptr:  Pointer to "struct ccs_number_union".
 *
 * Returns nothing.
 */
static void ccs_print_number_union(struct ccs_io_buffer *head,
				   const struct ccs_number_union *ptr)
{
	ccs_set_space(head);
	ccs_print_number_union_nospace(head, ptr);
}

/**
 * ccs_print_condition - Print condition part.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @cond: Pointer to "struct ccs_condition".
 *
 * Returns true on success, false otherwise.
 */
static bool ccs_print_condition(struct ccs_io_buffer *head,
				const struct ccs_condition *cond)
{
	switch (head->r.cond_step) {
	case 0:
		head->r.cond_index = 0;
		head->r.cond_step++;
		if (cond->transit && cond->exec_transit) {
			ccs_set_space(head);
			ccs_set_string(head, cond->transit->name);
		}
		/* fall through */
	case 1:
		{
			const u16 condc = cond->condc;
			const struct ccs_condition_element *condp =
				(typeof(condp)) (cond + 1);
			const struct ccs_number_union *numbers_p =
				(typeof(numbers_p)) (condp + condc);
			const struct ccs_name_union *names_p =
				(typeof(names_p))
				(numbers_p + cond->numbers_count);
			const struct ccs_argv *argv =
				(typeof(argv)) (names_p + cond->names_count);
			const struct ccs_envp *envp =
				(typeof(envp)) (argv + cond->argc);
			u16 skip;
			for (skip = 0; skip < head->r.cond_index; skip++) {
				const u8 left = condp->left;
				const u8 right = condp->right;
				condp++;
				switch (left) {
				case CCS_ARGV_ENTRY:
					argv++;
					continue;
				case CCS_ENVP_ENTRY:
					envp++;
					continue;
				case CCS_NUMBER_UNION:
					numbers_p++;
					break;
				}
				switch (right) {
				case CCS_NAME_UNION:
					names_p++;
					break;
				case CCS_NUMBER_UNION:
					numbers_p++;
					break;
				}
			}
			while (head->r.cond_index < condc) {
				const u8 match = condp->equals;
				const u8 left = condp->left;
				const u8 right = condp->right;
				if (!ccs_flush(head))
					return false;
				condp++;
				head->r.cond_index++;
				ccs_set_space(head);
				switch (left) {
				case CCS_ARGV_ENTRY:
					ccs_io_printf(head,
						      "exec.argv[%lu]%s=\"",
						      argv->index,
						      argv->is_not ? "!" : "");
					ccs_set_string(head,
						       argv->value->name);
					ccs_set_string(head, "\"");
					argv++;
					continue;
				case CCS_ENVP_ENTRY:
					ccs_set_string(head, "exec.envp[\"");
					ccs_set_string(head, envp->name->name);
					ccs_io_printf(head, "\"]%s=",
						      envp->is_not ? "!" : "");
					if (envp->value) {
						ccs_set_string(head, "\"");
						ccs_set_string(head, envp->
							       value->name);
						ccs_set_string(head, "\"");
					} else {
						ccs_set_string(head, "NULL");
					}
					envp++;
					continue;
				case CCS_NUMBER_UNION:
					ccs_print_number_union_nospace
						(head, numbers_p++);
					break;
				default:
					ccs_set_string(head,
					       ccs_condition_keyword[left]);
					break;
				}
				ccs_set_string(head, match ? "=" : "!=");
				switch (right) {
				case CCS_NAME_UNION:
					ccs_print_name_union_quoted
						(head, names_p++);
					break;
				case CCS_NUMBER_UNION:
					ccs_print_number_union_nospace
						(head, numbers_p++);
					break;
				default:
					ccs_set_string(head,
					       ccs_condition_keyword[right]);
					break;
				}
			}
		}
		head->r.cond_step++;
		/* fall through */
	case 2:
		if (!ccs_flush(head))
			break;
		head->r.cond_step++;
		/* fall through */
	case 3:
		if (cond->grant_log != CCS_GRANTLOG_AUTO)
			ccs_io_printf(head, " grant_log=%s",
				      ccs_yesno(cond->grant_log ==
						CCS_GRANTLOG_YES));
		if (cond->transit && !cond->exec_transit) {
			const char *name = cond->transit->name;
			ccs_set_string(head, " auto_domain_transition=\"");
			ccs_set_string(head, name);
			ccs_set_string(head, "\"");
		}
		ccs_set_lf(head);
		return true;
	}
	return false;
}

/**
 * ccs_set_group - Print "acl_group " header keyword and category name.
 *
 * @head:     Pointer to "struct ccs_io_buffer".
 * @category: Category name.
 *
 * Returns nothing.
 */
static void ccs_set_group(struct ccs_io_buffer *head, const char *category)
{
	if (head->type == CCS_EXCEPTION_POLICY) {
		ccs_print_namespace(head);
		ccs_io_printf(head, "acl_group %u ", head->r.acl_group_index);
	}
	ccs_set_string(head, category);
}

/**
 * ccs_print_entry - Print an ACL entry.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @acl:  Pointer to an ACL entry.
 *
 * Returns true on success, false otherwise.
 */
static bool ccs_print_entry(struct ccs_io_buffer *head,
			    const struct ccs_acl_info *acl)
{
	const u8 acl_type = acl->type;
	const bool may_trigger_transition = acl->cond && acl->cond->transit;
	bool first = true;
	u8 bit;
	if (head->r.print_cond_part)
		goto print_cond_part;
	if (acl->is_deleted)
		return true;
	if (!ccs_flush(head))
		return false;
	else if (acl_type == CCS_TYPE_PATH_ACL) {
		struct ccs_path_acl *ptr
			= container_of(acl, typeof(*ptr), head);
		for (bit = 0; bit < CCS_MAX_PATH_OPERATION; bit++) {
			if (!(acl->perm & (1 << bit)))
				continue;
			if (head->r.print_transition_related_only &&
			    bit != CCS_TYPE_EXECUTE && !may_trigger_transition)
				continue;
			if (first) {
				ccs_set_group(head, "file ");
				first = false;
			} else {
				ccs_set_slash(head);
			}
			ccs_set_string(head, ccs_path_keyword[bit]);
		}
		if (first)
			return true;
		ccs_print_name_union(head, &ptr->name);
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	} else if (acl_type == CCS_TYPE_AUTO_EXECUTE_HANDLER ||
		   acl_type == CCS_TYPE_DENIED_EXECUTE_HANDLER) {
		struct ccs_handler_acl *ptr
			= container_of(acl, typeof(*ptr), head);
		ccs_set_group(head, "task ");
		ccs_set_string(head, acl_type == CCS_TYPE_AUTO_EXECUTE_HANDLER
			       ? "auto_execute_handler " :
			       "denied_execute_handler ");
		ccs_set_string(head, ptr->handler->name);
#endif
#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION
	} else if (acl_type == CCS_TYPE_AUTO_TASK_ACL ||
		   acl_type == CCS_TYPE_MANUAL_TASK_ACL) {
		struct ccs_task_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		ccs_set_group(head, "task ");
		ccs_set_string(head, acl_type == CCS_TYPE_AUTO_TASK_ACL ?
			       "auto_domain_transition " :
			       "manual_domain_transition ");
		ccs_set_string(head, ptr->domainname->name);
#endif
	} else if (head->r.print_transition_related_only &&
		   !may_trigger_transition) {
		return true;
	} else if (acl_type == CCS_TYPE_MKDEV_ACL) {
		struct ccs_mkdev_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		for (bit = 0; bit < CCS_MAX_MKDEV_OPERATION; bit++) {
			if (!(acl->perm & (1 << bit)))
				continue;
			if (first) {
				ccs_set_group(head, "file ");
				first = false;
			} else {
				ccs_set_slash(head);
			}
			ccs_set_string(head, ccs_mac_keywords
				       [ccs_pnnn2mac[bit]]);
		}
		if (first)
			return true;
		ccs_print_name_union(head, &ptr->name);
		ccs_print_number_union(head, &ptr->mode);
		ccs_print_number_union(head, &ptr->major);
		ccs_print_number_union(head, &ptr->minor);
	} else if (acl_type == CCS_TYPE_PATH2_ACL) {
		struct ccs_path2_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		for (bit = 0; bit < CCS_MAX_PATH2_OPERATION; bit++) {
			if (!(acl->perm & (1 << bit)))
				continue;
			if (first) {
				ccs_set_group(head, "file ");
				first = false;
			} else {
				ccs_set_slash(head);
			}
			ccs_set_string(head, ccs_mac_keywords
				       [ccs_pp2mac[bit]]);
		}
		if (first)
			return true;
		ccs_print_name_union(head, &ptr->name1);
		ccs_print_name_union(head, &ptr->name2);
	} else if (acl_type == CCS_TYPE_PATH_NUMBER_ACL) {
		struct ccs_path_number_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		for (bit = 0; bit < CCS_MAX_PATH_NUMBER_OPERATION; bit++) {
			if (!(acl->perm & (1 << bit)))
				continue;
			if (first) {
				ccs_set_group(head, "file ");
				first = false;
			} else {
				ccs_set_slash(head);
			}
			ccs_set_string(head, ccs_mac_keywords
				       [ccs_pn2mac[bit]]);
		}
		if (first)
			return true;
		ccs_print_name_union(head, &ptr->name);
		ccs_print_number_union(head, &ptr->number);
#ifdef CONFIG_CCSECURITY_MISC
	} else if (acl_type == CCS_TYPE_ENV_ACL) {
		struct ccs_env_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		ccs_set_group(head, "misc env ");
		ccs_set_string(head, ptr->env->name);
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	} else if (acl_type == CCS_TYPE_CAPABILITY_ACL) {
		struct ccs_capability_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		ccs_set_group(head, "capability ");
		ccs_set_string(head, ccs_mac_keywords
			       [ccs_c2mac[ptr->operation]]);
#endif
#ifdef CONFIG_CCSECURITY_NETWORK
	} else if (acl_type == CCS_TYPE_INET_ACL) {
		struct ccs_inet_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		for (bit = 0; bit < CCS_MAX_NETWORK_OPERATION; bit++) {
			if (!(acl->perm & (1 << bit)))
				continue;
			if (first) {
				ccs_set_group(head, "network inet ");
				ccs_set_string(head, ccs_proto_keyword
					       [ptr->protocol]);
				ccs_set_space(head);
				first = false;
			} else {
				ccs_set_slash(head);
			}
			ccs_set_string(head, ccs_socket_keyword[bit]);
		}
		if (first)
			return true;
		ccs_set_space(head);
		if (!ccs_print_group(head, ptr->address.group)) {
			char buf[128];
			ccs_print_ip(buf, sizeof(buf), &ptr->address);
			ccs_io_printf(head, "%s", buf);
		}
		ccs_print_number_union(head, &ptr->port);
	} else if (acl_type == CCS_TYPE_UNIX_ACL) {
		struct ccs_unix_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		for (bit = 0; bit < CCS_MAX_NETWORK_OPERATION; bit++) {
			if (!(acl->perm & (1 << bit)))
				continue;
			if (first) {
				ccs_set_group(head, "network unix ");
				ccs_set_string(head, ccs_proto_keyword
					       [ptr->protocol]);
				ccs_set_space(head);
				first = false;
			} else {
				ccs_set_slash(head);
			}
			ccs_set_string(head, ccs_socket_keyword[bit]);
		}
		if (first)
			return true;
		ccs_print_name_union(head, &ptr->name);
#endif
#ifdef CONFIG_CCSECURITY_IPC
	} else if (acl_type == CCS_TYPE_SIGNAL_ACL) {
		struct ccs_signal_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		ccs_set_group(head, "ipc signal ");
		ccs_print_number_union_nospace(head, &ptr->sig);
		ccs_set_space(head);
		ccs_set_string(head, ptr->domainname->name);
#endif
	} else if (acl_type == CCS_TYPE_MOUNT_ACL) {
		struct ccs_mount_acl *ptr =
			container_of(acl, typeof(*ptr), head);
		ccs_set_group(head, "file mount");
		ccs_print_name_union(head, &ptr->dev_name);
		ccs_print_name_union(head, &ptr->dir_name);
		ccs_print_name_union(head, &ptr->fs_type);
		ccs_print_number_union(head, &ptr->flags);
	}
	if (acl->cond) {
		head->r.print_cond_part = true;
		head->r.cond_step = 0;
		if (!ccs_flush(head))
			return false;
print_cond_part:
		if (!ccs_print_condition(head, acl->cond))
			return false;
		head->r.print_cond_part = false;
	} else {
		ccs_set_lf(head);
	}
	return true;
}

/**
 * ccs_read_acl - Read "struct ccs_acl_info" list.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @list: Pointer to "struct list_head".
 *
 * Returns true on success, false otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static bool ccs_read_acl(struct ccs_io_buffer *head, struct list_head *list)
{
	list_for_each_cookie(head->r.acl, list) {
		struct ccs_acl_info *ptr =
			list_entry(head->r.acl, typeof(*ptr), list);
		if (!ccs_print_entry(head, ptr))
			return false;
	}
	head->r.acl = NULL;
	return true;
}

/**
 * ccs_read_domain - Read domain policy.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 *
 * Caller holds ccs_read_lock().
 */
static void ccs_read_domain(struct ccs_io_buffer *head)
{
	if (head->r.eof)
		return;
	list_for_each_cookie(head->r.domain, &ccs_domain_list) {
		struct ccs_domain_info *domain =
			list_entry(head->r.domain, typeof(*domain), list);
		switch (head->r.step) {
			u8 i;
		case 0:
			if (domain->is_deleted &&
			    !head->r.print_this_domain_only)
				continue;
			/* Print domainname and flags. */
			ccs_set_string(head, domain->domainname->name);
			ccs_set_lf(head);
			ccs_io_printf(head, "use_profile %u\n",
				      domain->profile);
			ccs_io_printf(head, "use_group %u\n", domain->group);
			for (i = 0; i < CCS_MAX_DOMAIN_INFO_FLAGS; i++)
				if (domain->flags[i])
					ccs_set_string(head, ccs_dif[i]);
			head->r.step++;
			ccs_set_lf(head);
			/* fall through */
		case 1:
			if (!ccs_read_acl(head, &domain->acl_info_list))
				return;
			head->r.step++;
			if (!ccs_set_lf(head))
				return;
			/* fall through */
		case 2:
			head->r.step = 0;
			if (head->r.print_this_domain_only)
				goto done;
		}
	}
done:
	head->r.eof = true;
}

/**
 * ccs_write_pid - Specify PID to obtain domainname.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns 0.
 */
static int ccs_write_pid(struct ccs_io_buffer *head)
{
	head->r.eof = false;
	return 0;
}

/**
 * ccs_read_pid - Read information of a process.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns the domainname which the specified PID is in or
 * process information of the specified PID on success,
 * empty string otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static void ccs_read_pid(struct ccs_io_buffer *head)
{
	char *buf = head->write_buf;
	bool task_info = false;
	bool global_pid = false;
	unsigned int pid;
	struct task_struct *p;
	struct ccs_domain_info *domain = NULL;
	u32 ccs_flags = 0;
	/* Accessing write_buf is safe because head->io_sem is held. */
	if (!buf) {
		head->r.eof = true;
		return; /* Do nothing if open(O_RDONLY). */
	}
	if (head->r.w_pos || head->r.eof)
		return;
	head->r.eof = true;
	if (ccs_str_starts(&buf, "info "))
		task_info = true;
	if (ccs_str_starts(&buf, "global-pid "))
		global_pid = true;
	pid = (unsigned int) simple_strtoul(buf, NULL, 10);
	ccs_tasklist_lock();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	if (global_pid)
		p = ccsecurity_exports.find_task_by_pid_ns(pid, &init_pid_ns);
	else
		p = ccsecurity_exports.find_task_by_vpid(pid);
#else
	p = find_task_by_pid(pid);
#endif
	if (p) {
		domain = ccs_task_domain(p);
		ccs_flags = ccs_task_flags(p);
	}
	ccs_tasklist_unlock();
	if (!domain)
		return;
	if (!task_info) {
		ccs_io_printf(head, "%u %u ", pid, domain->profile);
		ccs_set_string(head, domain->domainname->name);
	} else {
		ccs_io_printf(head, "%u manager=%s execute_handler=%s ", pid,
			      ccs_yesno(ccs_flags &
					CCS_TASK_IS_MANAGER),
			      ccs_yesno(ccs_flags &
					CCS_TASK_IS_EXECUTE_HANDLER));
	}
}

/**
 * ccs_write_group - Write "struct ccs_path_group"/"struct ccs_number_group"/"struct ccs_address_group" list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 * @type:  Type of this group.
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_write_group(struct ccs_acl_param *param, const u8 type)
{
	struct ccs_group *group = ccs_get_group(param, type);
	int error = -EINVAL;
	if (!group)
		return -ENOMEM;
	param->list = &group->member_list;
	if (type == CCS_PATH_GROUP) {
		struct ccs_path_group *e = &param->e.path_group;
		e->member_name = ccs_get_name(ccs_read_token(param));
		if (!e->member_name) {
			error = -ENOMEM;
			goto out;
		}
		error = ccs_update_policy(sizeof(*e), param);
		ccs_put_name(e->member_name);
	} else if (type == CCS_NUMBER_GROUP) {
		struct ccs_number_group *e = &param->e.number_group;
		if (param->data[0] == '@' ||
		    !ccs_parse_number_union(param, &e->number))
			goto out;
		error = ccs_update_policy(sizeof(*e), param);
#ifdef CONFIG_CCSECURITY_NETWORK
	} else {
		struct ccs_address_group *e = &param->e.address_group;
		if (param->data[0] == '@' ||
		    !ccs_parse_ipaddr_union(param, &e->address))
			goto out;
		error = ccs_update_policy(sizeof(*e), param);
#endif
	}
out:
	ccs_put_group(group);
	return error;
}

#ifdef CONFIG_CCSECURITY_PORTRESERVE
/**
 * ccs_lport_reserved - Check whether local port is reserved or not.
 *
 * @port: Port number.
 *
 * Returns true if local port is reserved, false otherwise.
 */
static bool __ccs_lport_reserved(const u16 port)
{
	return ccs_reserved_port_map[port >> 3] & (1 << (port & 7))
		? true : false;
}

/**
 * ccs_write_reserved_port - Update "struct ccs_reserved" list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_write_reserved_port(struct ccs_acl_param *param)
{
	struct ccs_reserved *e = &param->e.reserved;
	struct ccs_policy_namespace *ns = param->ns;
	int error;
	u8 *tmp;
	if (param->data[0] == '@' ||
	    !ccs_parse_number_union(param, &e->port) ||
	    e->port.values[1] > 65535 || param->data[0])
		return -EINVAL;
	param->list = &ns->policy_list[CCS_ID_RESERVEDPORT];
	error = ccs_update_policy(sizeof(*e), param);
	if (error)
		return error;
	tmp = kzalloc(sizeof(ccs_reserved_port_map), CCS_GFP_FLAGS);
	if (!tmp)
		return -ENOMEM;
	list_for_each_entry_srcu(ns, &ccs_namespace_list, namespace_list,
				 &ccs_ss) {
		struct ccs_reserved *ptr;
		struct list_head *list = &ns->policy_list[CCS_ID_RESERVEDPORT];
		list_for_each_entry_srcu(ptr, list, head.list, &ccs_ss) {
			unsigned int port;
			if (ptr->head.is_deleted)
				continue;
			for (port = ptr->port.values[0];
			     port <= ptr->port.values[1]; port++)
				tmp[port >> 3] |= 1 << (port & 7);
		}
	}
	memmove(ccs_reserved_port_map, tmp, sizeof(ccs_reserved_port_map));
	kfree(tmp);
	/*
	 * Since this feature is no-op by default, we don't need to register
	 * this callback hook unless the first entry is added.
	 */
	ccsecurity_ops.lport_reserved = __ccs_lport_reserved;
	return 0;
}
#endif

/**
 * ccs_write_aggregator - Write "struct ccs_aggregator" list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_write_aggregator(struct ccs_acl_param *param)
{
	struct ccs_aggregator *e = &param->e.aggregator;
	int error = param->is_delete ? -ENOENT : -ENOMEM;
	const char *original_name = ccs_read_token(param);
	const char *aggregated_name = ccs_read_token(param);
	if (!ccs_correct_word(original_name) ||
	    !ccs_correct_path(aggregated_name))
		return -EINVAL;
	e->original_name = ccs_get_name(original_name);
	e->aggregated_name = ccs_get_name(aggregated_name);
	if (!e->original_name || !e->aggregated_name ||
	    e->aggregated_name->is_patterned) /* No patterns allowed. */
		goto out;
	param->list = &param->ns->policy_list[CCS_ID_AGGREGATOR];
	error = ccs_update_policy(sizeof(*e), param);
out:
	ccs_put_name(e->original_name);
	ccs_put_name(e->aggregated_name);
	return error;
}

/**
 * ccs_write_transition_control - Write "struct ccs_transition_control" list.
 *
 * @param: Pointer to "struct ccs_acl_param".
 * @type:  Type of this entry.
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_write_transition_control(struct ccs_acl_param *param,
					const u8 type)
{
	struct ccs_transition_control *e = &param->e.transition_control;
	int error = param->is_delete ? -ENOENT : -ENOMEM;
	char *program = param->data;
	char *domainname = strstr(program, " from ");
	e->type = type;
	if (domainname) {
		*domainname = '\0';
		domainname += 6;
	} else if (type == CCS_TRANSITION_CONTROL_NO_KEEP ||
		   type == CCS_TRANSITION_CONTROL_KEEP) {
		domainname = program;
		program = NULL;
	}
	if (program && strcmp(program, "any")) {
		if (!ccs_correct_path(program))
			return -EINVAL;
		e->program = ccs_get_name(program);
		if (!e->program)
			goto out;
	}
	if (domainname && strcmp(domainname, "any")) {
		if (!ccs_correct_domain(domainname)) {
			if (!ccs_correct_path(domainname))
				goto out;
			e->is_last_name = true;
		}
		e->domainname = ccs_get_name(domainname);
		if (!e->domainname)
			goto out;
	}
	param->list = &param->ns->policy_list[CCS_ID_TRANSITION_CONTROL];
	error = ccs_update_policy(sizeof(*e), param);
out:
	ccs_put_name(e->domainname);
	ccs_put_name(e->program);
	return error;
}

/**
 * ccs_write_exception - Write exception policy.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_write_exception(struct ccs_io_buffer *head)
{
	const bool is_delete = head->w.is_delete;
	struct ccs_acl_param param = {
		.ns = head->w.ns,
		.is_delete = is_delete,
		.data = head->write_buf,
	};
	u8 i;
	/* Forced zero clear for using memcmp() at ccs_update_policy(). */
	memset(&param.e, 0, sizeof(param.e));
	if (ccs_str_starts(&param.data, "aggregator "))
		return ccs_write_aggregator(&param);
#ifdef CONFIG_CCSECURITY_PORTRESERVE
	if (ccs_str_starts(&param.data, "deny_autobind "))
		return ccs_write_reserved_port(&param);
#endif
	for (i = 0; i < CCS_MAX_TRANSITION_TYPE; i++)
		if (ccs_str_starts(&param.data, ccs_transition_type[i]))
			return ccs_write_transition_control(&param, i);
	for (i = 0; i < CCS_MAX_GROUP; i++)
		if (ccs_str_starts(&param.data, ccs_group_name[i]))
			return ccs_write_group(&param, i);
	if (ccs_str_starts(&param.data, "acl_group ")) {
		unsigned int group;
		char *data;
		group = simple_strtoul(param.data, &data, 10);
		if (group < CCS_MAX_ACL_GROUPS && *data++ == ' ')
			return ccs_write_acl(head->w.ns,
					     &head->w.ns->acl_group[group],
					     data, is_delete);
	}
	return -EINVAL;
}

/**
 * ccs_read_group - Read "struct ccs_path_group"/"struct ccs_number_group"/"struct ccs_address_group" list.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @idx:  Index number.
 *
 * Returns true on success, false otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static bool ccs_read_group(struct ccs_io_buffer *head, const int idx)
{
	struct ccs_policy_namespace *ns = container_of(head->r.ns, typeof(*ns),
						       namespace_list);
	struct list_head *list = &ns->group_list[idx];
	list_for_each_cookie(head->r.group, list) {
		struct ccs_group *group =
			list_entry(head->r.group, typeof(*group), head.list);
		list_for_each_cookie(head->r.acl, &group->member_list) {
			struct ccs_acl_head *ptr =
				list_entry(head->r.acl, typeof(*ptr), list);
			if (ptr->is_deleted)
				continue;
			if (!ccs_flush(head))
				return false;
			ccs_print_namespace(head);
			ccs_set_string(head, ccs_group_name[idx]);
			ccs_set_string(head, group->group_name->name);
			if (idx == CCS_PATH_GROUP) {
				ccs_set_space(head);
				ccs_set_string(head, container_of
					       (ptr, struct ccs_path_group,
						head)->member_name->name);
			} else if (idx == CCS_NUMBER_GROUP) {
				ccs_print_number_union(head, &container_of
					       (ptr, struct ccs_number_group,
						head)->number);
#ifdef CONFIG_CCSECURITY_NETWORK
			} else if (idx == CCS_ADDRESS_GROUP) {
				char buffer[128];
				struct ccs_address_group *member =
					container_of(ptr, typeof(*member),
						     head);
				ccs_print_ip(buffer, sizeof(buffer),
					     &member->address);
				ccs_io_printf(head, " %s", buffer);
#endif
			}
			ccs_set_lf(head);
		}
		head->r.acl = NULL;
	}
	head->r.group = NULL;
	return true;
}

/**
 * ccs_read_policy - Read "struct ccs_..._entry" list.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 * @idx:  Index number.
 *
 * Returns true on success, false otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static bool ccs_read_policy(struct ccs_io_buffer *head, const int idx)
{
	struct ccs_policy_namespace *ns = container_of(head->r.ns, typeof(*ns),
						       namespace_list);
	struct list_head *list = &ns->policy_list[idx];
	list_for_each_cookie(head->r.acl, list) {
		struct ccs_acl_head *acl =
			container_of(head->r.acl, typeof(*acl), list);
		if (acl->is_deleted)
			continue;
		if (head->r.print_transition_related_only &&
		    idx != CCS_ID_TRANSITION_CONTROL)
			continue;
		if (!ccs_flush(head))
			return false;
		switch (idx) {
		case CCS_ID_TRANSITION_CONTROL:
			{
				struct ccs_transition_control *ptr =
					container_of(acl, typeof(*ptr), head);
				ccs_print_namespace(head);
				ccs_set_string(head,
					       ccs_transition_type[ptr->type]);
				ccs_set_string(head, ptr->program ?
					       ptr->program->name : "any");
				ccs_set_string(head, " from ");
				ccs_set_string(head, ptr->domainname ?
					       ptr->domainname->name : "any");
			}
			break;
		case CCS_ID_AGGREGATOR:
			{
				struct ccs_aggregator *ptr =
					container_of(acl, typeof(*ptr), head);
				ccs_print_namespace(head);
				ccs_set_string(head, "aggregator ");
				ccs_set_string(head, ptr->original_name->name);
				ccs_set_space(head);
				ccs_set_string(head,
					       ptr->aggregated_name->name);
			}
			break;
#ifdef CONFIG_CCSECURITY_PORTRESERVE
		case CCS_ID_RESERVEDPORT:
			{
				struct ccs_reserved *ptr =
					container_of(acl, typeof(*ptr), head);
				ccs_print_namespace(head);
				ccs_set_string(head, "deny_autobind ");
				ccs_print_number_union_nospace(head,
							       &ptr->port);
			}
			break;
#endif
		default:
			continue;
		}
		ccs_set_lf(head);
	}
	head->r.acl = NULL;
	return true;
}

/**
 * ccs_read_exception - Read exception policy.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 *
 * Caller holds ccs_read_lock().
 */
static void ccs_read_exception(struct ccs_io_buffer *head)
{
	struct ccs_policy_namespace *ns = container_of(head->r.ns, typeof(*ns),
						       namespace_list);
	if (head->r.eof)
		return;
	while (head->r.step < CCS_MAX_POLICY &&
	       ccs_read_policy(head, head->r.step))
		head->r.step++;
	if (head->r.step < CCS_MAX_POLICY)
		return;
	while (head->r.step < CCS_MAX_POLICY + CCS_MAX_GROUP &&
	       ccs_read_group(head, head->r.step - CCS_MAX_POLICY))
		head->r.step++;
	if (head->r.step < CCS_MAX_POLICY + CCS_MAX_GROUP)
		return;
	while (head->r.step < CCS_MAX_POLICY + CCS_MAX_GROUP
	       + CCS_MAX_ACL_GROUPS) {
		head->r.acl_group_index =
			head->r.step - CCS_MAX_POLICY - CCS_MAX_GROUP;
		if (!ccs_read_acl(head, &ns->acl_group
				  [head->r.acl_group_index]))
			return;
		head->r.step++;
	}
	head->r.eof = true;
}

/**
 * ccs_truncate - Truncate a line.
 *
 * @str: String to truncate.
 *
 * Returns length of truncated @str.
 */
static int ccs_truncate(char *str)
{
	char *start = str;
	while (*(unsigned char *) str > (unsigned char) ' ')
		str++;
	*str = '\0';
	return strlen(start) + 1;
}

/**
 * ccs_add_entry - Add an ACL to current thread's domain. Used by learning mode.
 *
 * @header: Lines containing ACL.
 *
 * Returns nothing.
 */
static void ccs_add_entry(char *header)
{
	char *buffer;
	char *realpath = NULL;
	char *argv0 = NULL;
	char *symlink = NULL;
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	char *handler;
#endif
	char *cp = strchr(header, '\n');
	int len;
	if (!cp)
		return;
	cp = strchr(cp + 1, '\n');
	if (!cp)
		return;
	*cp++ = '\0';
	len = strlen(cp) + 1;
	/* strstr() will return NULL if ordering is wrong. */
	if (*cp == 'f') {
		argv0 = strstr(header, " argv[]={ \"");
		if (argv0) {
			argv0 += 10;
			len += ccs_truncate(argv0) + 14;
		}
		realpath = strstr(header, " exec={ realpath=\"");
		if (realpath) {
			realpath += 8;
			len += ccs_truncate(realpath) + 6;
		}
		symlink = strstr(header, " symlink.target=\"");
		if (symlink)
			len += ccs_truncate(symlink + 1) + 1;
	}
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	handler = strstr(header, "type=execute_handler");
	if (handler)
		len += ccs_truncate(handler) + 6;
#endif
	buffer = kmalloc(len, CCS_GFP_FLAGS);
	if (!buffer)
		return;
	snprintf(buffer, len - 1, "%s", cp);
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	if (handler)
		ccs_addprintf(buffer, len, " task.%s", handler);
#endif
	if (realpath)
		ccs_addprintf(buffer, len, " exec.%s", realpath);
	if (argv0)
		ccs_addprintf(buffer, len, " exec.argv[0]=%s", argv0);
	if (symlink)
		ccs_addprintf(buffer, len, "%s", symlink);
	ccs_normalize_line(buffer);
	{
		struct ccs_domain_info *domain = ccs_current_domain();
		if (!ccs_write_acl(domain->ns, &domain->acl_info_list,
				   buffer, false))
			ccs_update_stat(CCS_STAT_POLICY_UPDATES);
	}
	kfree(buffer);
}

/**
 * ccs_domain_quota_ok - Check for domain's quota.
 *
 * @r: Pointer to "struct ccs_request_info".
 *
 * Returns true if the domain is not exceeded quota, false otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static bool ccs_domain_quota_ok(struct ccs_request_info *r)
{
	unsigned int count = 0;
	struct ccs_domain_info * const domain = ccs_current_domain();
	struct ccs_acl_info *ptr;
	if (r->mode != CCS_CONFIG_LEARNING)
		return false;
	if (!domain)
		return true;
	list_for_each_entry_srcu(ptr, &domain->acl_info_list, list, &ccs_ss) {
		u16 perm;
		u8 i;
		if (ptr->is_deleted)
			continue;
		switch (ptr->type) {
		case CCS_TYPE_PATH_ACL:
		case CCS_TYPE_PATH2_ACL:
		case CCS_TYPE_PATH_NUMBER_ACL:
		case CCS_TYPE_MKDEV_ACL:
#ifdef CONFIG_CCSECURITY_NETWORK
		case CCS_TYPE_INET_ACL:
		case CCS_TYPE_UNIX_ACL:
#endif
			perm = ptr->perm;
			break;
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
		case CCS_TYPE_AUTO_EXECUTE_HANDLER:
		case CCS_TYPE_DENIED_EXECUTE_HANDLER:
#endif
#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION
		case CCS_TYPE_AUTO_TASK_ACL:
		case CCS_TYPE_MANUAL_TASK_ACL:
#endif
			perm = 0;
			break;
		default:
			perm = 1;
		}
		for (i = 0; i < 16; i++)
			if (perm & (1 << i))
				count++;
	}
	if (count < ccs_profile(r->profile)->pref[CCS_PREF_MAX_LEARNING_ENTRY])
		return true;
	if (!domain->flags[CCS_DIF_QUOTA_WARNED]) {
		domain->flags[CCS_DIF_QUOTA_WARNED] = true;
		/* r->granted = false; */
		ccs_write_log(r, "%s", ccs_dif[CCS_DIF_QUOTA_WARNED]);
		printk(KERN_WARNING "WARNING: "
		       "Domain '%s' has too many ACLs to hold. "
		       "Stopped learning mode.\n", domain->domainname->name);
	}
	return false;
}

/**
 * ccs_supervisor - Ask for the supervisor's decision.
 *
 * @r:   Pointer to "struct ccs_request_info".
 * @fmt: The printf()'s format string, followed by parameters.
 *
 * Returns 0 if the supervisor decided to permit the access request which
 * violated the policy in enforcing mode, CCS_RETRY_REQUEST if the supervisor
 * decided to retry the access request which violated the policy in enforcing
 * mode, 0 if it is not in enforcing mode, -EPERM otherwise.
 */
static int ccs_supervisor(struct ccs_request_info *r, const char *fmt, ...)
{
	va_list args;
	int error;
	int len;
	static unsigned int ccs_serial;
	struct ccs_query entry = { };
	bool quota_exceeded = false;
	va_start(args, fmt);
	len = vsnprintf((char *) &len, 1, fmt, args) + 1;
	va_end(args);
	/* Write /proc/ccs/audit. */
	va_start(args, fmt);
	ccs_write_log2(r, len, fmt, args);
	va_end(args);
	/* Nothing more to do if granted. */
	if (r->granted)
		return 0;
	if (r->mode)
		ccs_update_stat(r->mode);
	switch (r->mode) {
		int i;
		struct ccs_profile *p;
	case CCS_CONFIG_ENFORCING:
		error = -EPERM;
		if (atomic_read(&ccs_query_observers))
			break;
		if (r->dont_sleep_on_enforce_error)
			goto out;
		p = ccs_profile(r->profile);
		/* Check enforcing_penalty parameter. */
		for (i = 0; i < p->pref[CCS_PREF_ENFORCING_PENALTY]; i++) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ / 10);
		}
		goto out;
	case CCS_CONFIG_LEARNING:
		error = 0;
		/* Check max_learning_entry parameter. */
		if (ccs_domain_quota_ok(r))
			break;
		/* fall through */
	default:
		return 0;
	}
	/* Get message. */
	va_start(args, fmt);
	entry.query = ccs_init_log(r, len, fmt, args);
	va_end(args);
	if (!entry.query)
		goto out;
	entry.query_len = strlen(entry.query) + 1;
	if (!error) {
		ccs_add_entry(entry.query);
		goto out;
	}
	len = ccs_round2(entry.query_len);
	entry.domain = ccs_current_domain();
	spin_lock(&ccs_query_list_lock);
	if (ccs_memory_quota[CCS_MEMORY_QUERY] &&
	    ccs_memory_used[CCS_MEMORY_QUERY] + len
	    >= ccs_memory_quota[CCS_MEMORY_QUERY]) {
		quota_exceeded = true;
	} else {
		entry.serial = ccs_serial++;
		entry.retry = r->retry;
		ccs_memory_used[CCS_MEMORY_QUERY] += len;
		list_add_tail(&entry.list, &ccs_query_list);
	}
	spin_unlock(&ccs_query_list_lock);
	if (quota_exceeded)
		goto out;
	/* Give 10 seconds for supervisor's opinion. */
	while (entry.timer < 10) {
		wake_up_all(&ccs_query_wait);
		if (wait_event_interruptible_timeout
		    (ccs_answer_wait, entry.answer ||
		     !atomic_read(&ccs_query_observers), HZ))
			break;
		else
			entry.timer++;
	}
	spin_lock(&ccs_query_list_lock);
	list_del(&entry.list);
	ccs_memory_used[CCS_MEMORY_QUERY] -= len;
	spin_unlock(&ccs_query_list_lock);
	switch (entry.answer) {
	case 3: /* Asked to retry by administrator. */
		error = CCS_RETRY_REQUEST;
		r->retry++;
		break;
	case 1:
		/* Granted by administrator. */
		error = 0;
		break;
	default:
		/* Timed out or rejected by administrator. */
		break;
	}
out:
	kfree(entry.query);
	return error;
}

/**
 * ccs_audit_log - Audit permission check log.
 *
 * @r: Pointer to "struct ccs_request_info".
 *
 * Returns return value of ccs_supervisor().
 */
int ccs_audit_log(struct ccs_request_info *r)
{
	switch (r->param_type) {
		u8 type;
		char buf[48];
#ifdef CONFIG_CCSECURITY_NETWORK
		const u32 *address;
#endif
	case CCS_TYPE_PATH_ACL:
		return ccs_supervisor(r, "file %s %s\n", ccs_path_keyword
				      [r->param.path.operation],
				      r->param.path.filename->name);
	case CCS_TYPE_PATH2_ACL:
		return ccs_supervisor(r, "file %s %s %s\n", ccs_mac_keywords
				      [ccs_pp2mac[r->param.path2.operation]],
				      r->param.path2.filename1->name,
				      r->param.path2.filename2->name);
	case CCS_TYPE_PATH_NUMBER_ACL:
		type = r->param.path_number.operation;
		switch (type) {
		case CCS_TYPE_CREATE:
		case CCS_TYPE_MKDIR:
		case CCS_TYPE_MKFIFO:
		case CCS_TYPE_MKSOCK:
		case CCS_TYPE_CHMOD:
			snprintf(buf, sizeof(buf), "0%lo",
				 r->param.path_number.number);
			break;
		case CCS_TYPE_IOCTL:
			snprintf(buf, sizeof(buf), "0x%lX",
				 r->param.path_number.number);
			break;
		default:
			snprintf(buf, sizeof(buf), "%lu",
				 r->param.path_number.number);
			break;
		}
		return ccs_supervisor(r, "file %s %s %s\n", ccs_mac_keywords
				      [ccs_pn2mac[type]],
				      r->param.path_number.filename->name,
				      buf);
	case CCS_TYPE_MKDEV_ACL:
		return ccs_supervisor(r, "file %s %s 0%o %u %u\n",
				      ccs_mac_keywords
				      [ccs_pnnn2mac[r->param.mkdev.operation]],
				      r->param.mkdev.filename->name,
				      r->param.mkdev.mode,
				      r->param.mkdev.major,
				      r->param.mkdev.minor);
	case CCS_TYPE_MOUNT_ACL:
		return ccs_supervisor(r, "file mount %s %s %s 0x%lX\n",
				      r->param.mount.dev->name,
				      r->param.mount.dir->name,
				      r->param.mount.type->name,
				      r->param.mount.flags);
#ifdef CONFIG_CCSECURITY_MISC
	case CCS_TYPE_ENV_ACL:
		return ccs_supervisor(r, "misc env %s\n",
				      r->param.environ.name->name);
#endif
#ifdef CONFIG_CCSECURITY_CAPABILITY
	case CCS_TYPE_CAPABILITY_ACL:
		return ccs_supervisor(r, "capability %s\n", ccs_mac_keywords
				      [ccs_c2mac[r->param.capability.
						 operation]]);
#endif
#ifdef CONFIG_CCSECURITY_NETWORK
	case CCS_TYPE_INET_ACL:
		address = r->param.inet_network.address;
		if (r->param.inet_network.is_ipv6)
			ccs_print_ipv6(buf, sizeof(buf),
				       (const struct in6_addr *) address);
		else
			ccs_print_ipv4(buf, sizeof(buf), address);
		return ccs_supervisor(r, "network inet %s %s %s %u\n",
				      ccs_proto_keyword[r->param.inet_network.
							protocol],
				      ccs_socket_keyword[r->param.inet_network.
							 operation],
				      buf, r->param.inet_network.port);
	case CCS_TYPE_UNIX_ACL:
		return ccs_supervisor(r, "network unix %s %s %s\n",
				      ccs_proto_keyword[r->param.
							unix_network.protocol],
				      ccs_socket_keyword[r->param.unix_network.
							 operation],
				      r->param.unix_network.address->name);
#endif
#ifdef CONFIG_CCSECURITY_IPC
	case CCS_TYPE_SIGNAL_ACL:
		return ccs_supervisor(r, "ipc signal %d %s\n",
				      r->param.signal.sig,
				      r->param.signal.dest_pattern);
#endif
	}
	return 0;
}

/**
 * ccs_find_domain_by_qid - Get domain by query id.
 *
 * @serial: Query ID assigned by ccs_supervisor().
 *
 * Returns pointer to "struct ccs_domain_info" if found, NULL otherwise.
 */
static struct ccs_domain_info *ccs_find_domain_by_qid(unsigned int serial)
{
	struct ccs_query *ptr;
	struct ccs_domain_info *domain = NULL;
	spin_lock(&ccs_query_list_lock);
	list_for_each_entry(ptr, &ccs_query_list, list) {
		if (ptr->serial != serial)
			continue;
		domain = ptr->domain;
		break;
	}
	spin_unlock(&ccs_query_list_lock);
	return domain;
}

/**
 * ccs_read_query - Read access requests which violated policy in enforcing mode.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 */
static void ccs_read_query(struct ccs_io_buffer *head)
{
	struct list_head *tmp;
	unsigned int pos = 0;
	size_t len = 0;
	char *buf;
	if (head->r.w_pos)
		return;
	kfree(head->read_buf);
	head->read_buf = NULL;
	spin_lock(&ccs_query_list_lock);
	list_for_each(tmp, &ccs_query_list) {
		struct ccs_query *ptr = list_entry(tmp, typeof(*ptr), list);
		if (pos++ != head->r.query_index)
			continue;
		len = ptr->query_len;
		break;
	}
	spin_unlock(&ccs_query_list_lock);
	if (!len) {
		head->r.query_index = 0;
		return;
	}
	buf = kzalloc(len + 32, CCS_GFP_FLAGS);
	if (!buf)
		return;
	pos = 0;
	spin_lock(&ccs_query_list_lock);
	list_for_each(tmp, &ccs_query_list) {
		struct ccs_query *ptr = list_entry(tmp, typeof(*ptr), list);
		if (pos++ != head->r.query_index)
			continue;
		/*
		 * Some query can be skipped because ccs_query_list
		 * can change, but I don't care.
		 */
		if (len == ptr->query_len)
			snprintf(buf, len + 31, "Q%u-%hu\n%s", ptr->serial,
				 ptr->retry, ptr->query);
		break;
	}
	spin_unlock(&ccs_query_list_lock);
	if (buf[0]) {
		head->read_buf = buf;
		head->r.w[head->r.w_pos++] = buf;
		head->r.query_index++;
	} else {
		kfree(buf);
	}
}

/**
 * ccs_write_answer - Write the supervisor's decision.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns 0 on success, -EINVAL otherwise.
 */
static int ccs_write_answer(struct ccs_io_buffer *head)
{
	char *data = head->write_buf;
	struct list_head *tmp;
	unsigned int serial;
	unsigned int answer;
	spin_lock(&ccs_query_list_lock);
	list_for_each(tmp, &ccs_query_list) {
		struct ccs_query *ptr = list_entry(tmp, typeof(*ptr), list);
		ptr->timer = 0;
	}
	spin_unlock(&ccs_query_list_lock);
	if (sscanf(data, "A%u=%u", &serial, &answer) != 2)
		return -EINVAL;
	spin_lock(&ccs_query_list_lock);
	list_for_each(tmp, &ccs_query_list) {
		struct ccs_query *ptr = list_entry(tmp, typeof(*ptr), list);
		if (ptr->serial != serial)
			continue;
		ptr->answer = (u8) answer;
		/* Remove from ccs_query_list. */
		if (ptr->answer) {
			list_del(&ptr->list);
			INIT_LIST_HEAD(&ptr->list);
		}
		break;
	}
	spin_unlock(&ccs_query_list_lock);
	wake_up_all(&ccs_answer_wait);
	return 0;
}

/**
 * ccs_read_version - Get version.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 */
static void ccs_read_version(struct ccs_io_buffer *head)
{
	if (head->r.eof)
		return;
	ccs_set_string(head, "1.8.3");
	head->r.eof = true;
}

/**
 * ccs_update_stat - Update statistic counters.
 *
 * @index: Index for policy type.
 *
 * Returns nothing.
 */
static void ccs_update_stat(const u8 index)
{
	struct timeval tv;
	do_gettimeofday(&tv);
	/*
	 * I don't use atomic operations because race condition is not fatal.
	 */
	ccs_stat_updated[index]++;
	ccs_stat_modified[index] = tv.tv_sec;
}

/**
 * ccs_read_stat - Read statistic data.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 */
static void ccs_read_stat(struct ccs_io_buffer *head)
{
	u8 i;
	unsigned int total = 0;
	if (head->r.eof)
		return;
	for (i = 0; i < CCS_MAX_POLICY_STAT; i++) {
		ccs_io_printf(head, "Policy %-30s %10u", ccs_policy_headers[i],
			      ccs_stat_updated[i]);
		if (ccs_stat_modified[i]) {
			struct ccs_time stamp;
			ccs_convert_time(ccs_stat_modified[i], &stamp);
			ccs_io_printf(head, " (Last: %04u/%02u/%02u "
				      "%02u:%02u:%02u)",
				      stamp.year, stamp.month, stamp.day,
				      stamp.hour, stamp.min, stamp.sec);
		}
		ccs_set_lf(head);
	}
	for (i = 0; i < CCS_MAX_MEMORY_STAT; i++) {
		unsigned int used = ccs_memory_used[i];
		total += used;
		ccs_io_printf(head, "Memory used by %-22s %10u",
			      ccs_memory_headers[i], used);
		used = ccs_memory_quota[i];
		if (used)
			ccs_io_printf(head, " (Quota: %10u)", used);
		ccs_set_lf(head);
	}
	ccs_io_printf(head, "Total memory used:                    %10u\n",
		      total);
	head->r.eof = true;
}

/**
 * ccs_write_stat - Set memory quota.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns 0.
 */
static int ccs_write_stat(struct ccs_io_buffer *head)
{
	char *data = head->write_buf;
	u8 i;
	if (ccs_str_starts(&data, "Memory used by "))
		for (i = 0; i < CCS_MAX_MEMORY_STAT; i++)
			if (ccs_str_starts(&data, ccs_memory_headers[i])) {
				if (*data == ' ')
					data++;
				ccs_memory_quota[i] =
					simple_strtoul(data, NULL, 10);
			}
	return 0;
}

/**
 * ccs_print_bprm - Print "struct linux_binprm" for auditing.
 *
 * @bprm: Pointer to "struct linux_binprm".
 * @dump: Pointer to "struct ccs_page_dump".
 *
 * Returns the contents of @bprm on success, NULL otherwise.
 *
 * This function uses kzalloc(), so caller must kfree() if this function
 * didn't return NULL.
 */
static char *ccs_print_bprm(struct linux_binprm *bprm,
			    struct ccs_page_dump *dump)
{
	static const int ccs_buffer_len = 4096 * 2;
	char *buffer = kzalloc(ccs_buffer_len, CCS_GFP_FLAGS);
	char *cp;
	char *last_start;
	int len;
	unsigned long pos = bprm->p;
	int offset = pos % PAGE_SIZE;
	int argv_count = bprm->argc;
	int envp_count = bprm->envc;
	bool truncated = false;
	if (!buffer)
		return NULL;
	len = snprintf(buffer, ccs_buffer_len - 1, "argv[]={ ");
	cp = buffer + len;
	if (!argv_count) {
		memmove(cp, "} envp[]={ ", 11);
		cp += 11;
	}
	last_start = cp;
	while (argv_count || envp_count) {
		if (!ccs_dump_page(bprm, pos, dump))
			goto out;
		pos += PAGE_SIZE - offset;
		/* Read. */
		while (offset < PAGE_SIZE) {
			const char *kaddr = dump->data;
			const unsigned char c = kaddr[offset++];
			if (cp == last_start)
				*cp++ = '"';
			if (cp >= buffer + ccs_buffer_len - 32) {
				/* Reserve some room for "..." string. */
				truncated = true;
			} else if (c == '\\') {
				*cp++ = '\\';
				*cp++ = '\\';
			} else if (c > ' ' && c < 127) {
				*cp++ = c;
			} else if (!c) {
				*cp++ = '"';
				*cp++ = ' ';
				last_start = cp;
			} else {
				*cp++ = '\\';
				*cp++ = (c >> 6) + '0';
				*cp++ = ((c >> 3) & 7) + '0';
				*cp++ = (c & 7) + '0';
			}
			if (c)
				continue;
			if (argv_count) {
				if (--argv_count == 0) {
					if (truncated) {
						cp = last_start;
						memmove(cp, "... ", 4);
						cp += 4;
					}
					memmove(cp, "} envp[]={ ", 11);
					cp += 11;
					last_start = cp;
					truncated = false;
				}
			} else if (envp_count) {
				if (--envp_count == 0) {
					if (truncated) {
						cp = last_start;
						memmove(cp, "... ", 4);
						cp += 4;
					}
				}
			}
			if (!argv_count && !envp_count)
				break;
		}
		offset = 0;
	}
	*cp++ = '}';
	*cp = '\0';
	return buffer;
out:
	snprintf(buffer, ccs_buffer_len - 1, "argv[]={ ... } envp[]= { ... }");
	return buffer;
}

/**
 * ccs_filetype - Get string representation of file type.
 *
 * @mode: Mode value for stat().
 *
 * Returns file type string.
 */
static inline const char *ccs_filetype(const umode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFREG:
	case 0:
		return ccs_condition_keyword[CCS_TYPE_IS_FILE];
	case S_IFDIR:
		return ccs_condition_keyword[CCS_TYPE_IS_DIRECTORY];
	case S_IFLNK:
		return ccs_condition_keyword[CCS_TYPE_IS_SYMLINK];
	case S_IFIFO:
		return ccs_condition_keyword[CCS_TYPE_IS_FIFO];
	case S_IFSOCK:
		return ccs_condition_keyword[CCS_TYPE_IS_SOCKET];
	case S_IFBLK:
		return ccs_condition_keyword[CCS_TYPE_IS_BLOCK_DEV];
	case S_IFCHR:
		return ccs_condition_keyword[CCS_TYPE_IS_CHAR_DEV];
	}
	return "unknown"; /* This should not happen. */
}

/**
 * ccs_print_header - Get header line of audit log.
 *
 * @r: Pointer to "struct ccs_request_info".
 *
 * Returns string representation.
 *
 * This function uses kmalloc(), so caller must kfree() if this function
 * didn't return NULL.
 */
static char *ccs_print_header(struct ccs_request_info *r)
{
	struct ccs_time stamp;
	struct ccs_obj_info *obj = r->obj;
	const u32 ccs_flags = ccs_current_flags();
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	const pid_t gpid = ccs_sys_getpid();
#else
	const pid_t gpid = task_pid_nr(current);
#endif
	static const int ccs_buffer_len = 4096;
	char *buffer = kmalloc(ccs_buffer_len, CCS_GFP_FLAGS);
	int pos;
	u8 i;
	if (!buffer)
		return NULL;
	{
		struct timeval tv;
		do_gettimeofday(&tv);
		ccs_convert_time(tv.tv_sec, &stamp);
	}
	pos = snprintf(buffer, ccs_buffer_len - 1,
		       "#%04u/%02u/%02u %02u:%02u:%02u# profile=%u mode=%s "
		       "granted=%s (global-pid=%u) task={ pid=%u ppid=%u "
		       "uid=%u gid=%u euid=%u egid=%u suid=%u sgid=%u "
		       "fsuid=%u fsgid=%u type%s=execute_handler }",
		       stamp.year, stamp.month, stamp.day, stamp.hour,
		       stamp.min, stamp.sec, r->profile, ccs_mode[r->mode],
		       ccs_yesno(r->granted), gpid, ccs_sys_getpid(),
		       ccs_sys_getppid(),
		       from_kuid(&init_user_ns, current_uid()),
		       from_kgid(&init_user_ns, current_gid()),
		       from_kuid(&init_user_ns, current_euid()),
		       from_kgid(&init_user_ns, current_egid()),
		       from_kuid(&init_user_ns, current_suid()),
		       from_kgid(&init_user_ns, current_sgid()),
		       from_kuid(&init_user_ns, current_fsuid()),
		       from_kgid(&init_user_ns, current_fsgid()),
		       ccs_flags & CCS_TASK_IS_EXECUTE_HANDLER ? "" : "!");
	if (!obj)
		goto no_obj_info;
	if (!obj->validate_done) {
		ccs_get_attributes(obj);
		obj->validate_done = true;
	}
	for (i = 0; i < CCS_MAX_PATH_STAT; i++) {
		struct ccs_mini_stat *stat;
		unsigned int dev;
		umode_t mode;
		if (!obj->stat_valid[i])
			continue;
		stat = &obj->stat[i];
		dev = stat->dev;
		mode = stat->mode;
		if (i & 1) {
			pos += snprintf(buffer + pos, ccs_buffer_len - 1 - pos,
					" path%u.parent={ uid=%u gid=%u "
					"ino=%lu perm=0%o }", (i >> 1) + 1,
					from_kuid(&init_user_ns, stat->uid),
					from_kgid(&init_user_ns, stat->gid),
					(unsigned long) stat->ino,
					stat->mode & S_IALLUGO);
			continue;
		}
		pos += snprintf(buffer + pos, ccs_buffer_len - 1 - pos,
				" path%u={ uid=%u gid=%u ino=%lu major=%u"
				" minor=%u perm=0%o type=%s", (i >> 1) + 1,
				from_kuid(&init_user_ns, stat->uid),
				from_kgid(&init_user_ns, stat->gid),
				(unsigned long) stat->ino, MAJOR(dev),
				MINOR(dev), mode & S_IALLUGO,
				ccs_filetype(mode));
		if (S_ISCHR(mode) || S_ISBLK(mode)) {
			dev = stat->rdev;
			pos += snprintf(buffer + pos, ccs_buffer_len - 1 - pos,
					" dev_major=%u dev_minor=%u",
					MAJOR(dev), MINOR(dev));
		}
		pos += snprintf(buffer + pos, ccs_buffer_len - 1 - pos, " }");
	}
no_obj_info:
	if (pos < ccs_buffer_len - 1)
		return buffer;
	kfree(buffer);
	return NULL;
}

/**
 * ccs_init_log - Allocate buffer for audit logs.
 *
 * @r:    Pointer to "struct ccs_request_info".
 * @len:  Buffer size needed for @fmt and @args.
 * @fmt:  The printf()'s format string.
 * @args: va_list structure for @fmt.
 *
 * Returns pointer to allocated memory.
 *
 * This function uses kzalloc(), so caller must kfree() if this function
 * didn't return NULL.
 */
static char *ccs_init_log(struct ccs_request_info *r, int len, const char *fmt,
			  va_list args)
{
	char *buf = NULL;
	char *bprm_info = NULL;
	char *realpath = NULL;
	const char *symlink = NULL;
	const char *header = NULL;
	int pos;
	const char *domainname = ccs_current_domain()->domainname->name;
	header = ccs_print_header(r);
	if (!header)
		return NULL;
	/* +10 is for '\n' etc. and '\0'. */
	len += strlen(domainname) + strlen(header) + 10;
	if (r->ee) {
		struct file *file = r->ee->bprm->file;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 20)
		struct path path = { file->f_vfsmnt, file->f_dentry };
		realpath = ccs_realpath(&path);
#else
		realpath = ccs_realpath(&file->f_path);
#endif
		bprm_info = ccs_print_bprm(r->ee->bprm, &r->ee->dump);
		if (!realpath || !bprm_info)
			goto out;
		/* +80 is for " exec={ realpath=\"%s\" argc=%d envc=%d %s }" */
		len += strlen(realpath) + 80 + strlen(bprm_info);
	} else if (r->obj && r->obj->symlink_target) {
		symlink = r->obj->symlink_target->name;
		/* +18 is for " symlink.target=\"%s\"" */
		len += 18 + strlen(symlink);
	}
	len = ccs_round2(len);
	buf = kzalloc(len, CCS_GFP_FLAGS);
	if (!buf)
		goto out;
	len--;
	pos = snprintf(buf, len, "%s", header);
	if (realpath) {
		struct linux_binprm *bprm = r->ee->bprm;
		pos += snprintf(buf + pos, len - pos,
				" exec={ realpath=\"%s\" argc=%d envc=%d %s }",
				realpath, bprm->argc, bprm->envc, bprm_info);
	} else if (symlink)
		pos += snprintf(buf + pos, len - pos, " symlink.target=\"%s\"",
				symlink);
	pos += snprintf(buf + pos, len - pos, "\n%s\n", domainname);
	vsnprintf(buf + pos, len - pos, fmt, args);
out:
	kfree(realpath);
	kfree(bprm_info);
	kfree(header);
	return buf;
}

/**
 * ccs_transition_failed - Print waning message and send signal when domain transition failed.
 *
 * @domainname: Name of domain to transit.
 *
 * Returns nothing.
 *
 * Note that if current->pid == 1, sending SIGKILL won't work.
 */
void ccs_transition_failed(const char *domainname)
{
	printk(KERN_WARNING
	       "ERROR: Unable to transit to '%s' domain.\n", domainname);
	force_sig(SIGKILL, current);
}

/**
 * ccs_update_task_domain - Update task's domain.
 *
 * @r: Pointer to "struct ccs_request_info".
 *
 * Returns nothing.
 *
 * The task will retry as hard as possible. But if domain transition failed,
 * the task will be killed by SIGKILL.
 */
static void ccs_update_task_domain(struct ccs_request_info *r)
{
	char *buf;
	const char *cp;
	const struct ccs_acl_info *acl = r->matched_acl;
	r->matched_acl = NULL;
	if (!acl || !acl->cond || !acl->cond->transit ||
	    acl->cond->exec_transit)
		return;
	while (1) {
		buf = kzalloc(CCS_EXEC_TMPSIZE, CCS_GFP_FLAGS);
		if (buf)
			break;
		ssleep(1);
		if (fatal_signal_pending(current))
			return;
	}
	cp = acl->cond->transit->name;
	if (*cp == '/')
		snprintf(buf, CCS_EXEC_TMPSIZE - 1, "%s %s",
			 ccs_current_domain()->domainname->name, cp);
	else
		strncpy(buf, cp, CCS_EXEC_TMPSIZE - 1);
	if (!ccs_assign_domain(buf, true))
		ccs_transition_failed(buf);
	kfree(buf);
}

/**
 * ccs_get_audit - Get audit mode.
 *
 * @r: Pointer to "struct ccs_request_info".
 *
 * Returns true if this request should be audited, false otherwise.
 */
static bool ccs_get_audit(const struct ccs_request_info *r)
{
	const struct ccs_acl_info *matched_acl = r->matched_acl;
	const u8 profile = r->profile;
	const u8 index = r->type;
	const bool is_granted = r->granted;
	u8 mode;
	struct ccs_profile *p;
	if (!ccs_policy_loaded)
		return false;
	p = ccs_profile(profile);
	if (ccs_log_count >= p->pref[CCS_PREF_MAX_AUDIT_LOG])
		return false;
	if (is_granted && matched_acl && matched_acl->cond &&
	    matched_acl->cond->grant_log != CCS_GRANTLOG_AUTO)
		return matched_acl->cond->grant_log == CCS_GRANTLOG_YES;
	mode = p->config[index];
	if (mode == CCS_CONFIG_USE_DEFAULT)
		mode = p->config
			[ccs_index2category[index] + CCS_MAX_MAC_INDEX];
	if (mode == CCS_CONFIG_USE_DEFAULT)
		mode = p->default_config;
	if (is_granted)
		return mode & CCS_CONFIG_WANT_GRANT_LOG;
	return mode & CCS_CONFIG_WANT_REJECT_LOG;
}

/**
 * ccs_write_log2 - Write an audit log.
 *
 * @r:    Pointer to "struct ccs_request_info".
 * @len:  Buffer size needed for @fmt and @args.
 * @fmt:  The printf()'s format string.
 * @args: va_list structure for @fmt.
 *
 * Returns nothing.
 */
static void ccs_write_log2(struct ccs_request_info *r, int len,
			   const char *fmt, va_list args)
{
	char *buf;
	struct ccs_log *entry;
	bool quota_exceeded = false;
	if (!ccs_get_audit(r))
		goto out;
	buf = ccs_init_log(r, len, fmt, args);
	if (!buf)
		goto out;
	entry = kzalloc(sizeof(*entry), CCS_GFP_FLAGS);
	if (!entry) {
		kfree(buf);
		goto out;
	}
	entry->log = buf;
	len = ccs_round2(strlen(buf) + 1);
	/*
	 * The entry->size is used for memory quota checks.
	 * Don't go beyond strlen(entry->log).
	 */
	entry->size = len + ccs_round2(sizeof(*entry));
	spin_lock(&ccs_log_lock);
	if (ccs_memory_quota[CCS_MEMORY_AUDIT] &&
	    ccs_memory_used[CCS_MEMORY_AUDIT] + entry->size >=
	    ccs_memory_quota[CCS_MEMORY_AUDIT]) {
		quota_exceeded = true;
	} else {
		ccs_memory_used[CCS_MEMORY_AUDIT] += entry->size;
		list_add_tail(&entry->list, &ccs_log);
		ccs_log_count++;
	}
	spin_unlock(&ccs_log_lock);
	if (quota_exceeded) {
		kfree(buf);
		kfree(entry);
		goto out;
	}
	wake_up(&ccs_log_wait);
out:
	ccs_update_task_domain(r);
}

/**
 * ccs_write_log - Write an audit log.
 *
 * @r:   Pointer to "struct ccs_request_info".
 * @fmt: The printf()'s format string, followed by parameters.
 *
 * Returns nothing.
 */
void ccs_write_log(struct ccs_request_info *r, const char *fmt, ...)
{
	va_list args;
	int len;
	va_start(args, fmt);
	len = vsnprintf((char *) &len, 1, fmt, args) + 1;
	va_end(args);
	va_start(args, fmt);
	ccs_write_log2(r, len, fmt, args);
	va_end(args);
}

/**
 * ccs_read_log - Read an audit log.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 */
static void ccs_read_log(struct ccs_io_buffer *head)
{
	struct ccs_log *ptr = NULL;
	if (head->r.w_pos)
		return;
	kfree(head->read_buf);
	head->read_buf = NULL;
	spin_lock(&ccs_log_lock);
	if (!list_empty(&ccs_log)) {
		ptr = list_entry(ccs_log.next, typeof(*ptr), list);
		list_del(&ptr->list);
		ccs_log_count--;
		ccs_memory_used[CCS_MEMORY_AUDIT] -= ptr->size;
	}
	spin_unlock(&ccs_log_lock);
	if (ptr) {
		head->read_buf = ptr->log;
		head->r.w[head->r.w_pos++] = head->read_buf;
		kfree(ptr);
	}
}

/**
 * ccs_set_namespace_cursor - Set namespace to read.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns nothing.
 */
static void ccs_set_namespace_cursor(struct ccs_io_buffer *head)
{
	struct list_head *ns;
	if (head->type != CCS_EXCEPTION_POLICY && head->type != CCS_PROFILE)
		return;
	/*
	 * If this is the first read, or reading previous namespace finished
	 * and has more namespaces to read, update the namespace cursor.
	 */
	ns = head->r.ns;
	if (!ns || (head->r.eof && ns->next != &ccs_namespace_list)) {
		/* Clearing is OK because ccs_flush() returned true. */
		memset(&head->r, 0, sizeof(head->r));
		head->r.ns = ns ? ns->next : ccs_namespace_list.next;
	}
}

/**
 * ccs_has_more_namespace - Check for unread namespaces.
 *
 * @head: Pointer to "struct ccs_io_buffer".
 *
 * Returns true if we have more entries to print, false otherwise.
 */
static bool ccs_has_more_namespace(struct ccs_io_buffer *head)
{
	return (head->type == CCS_EXCEPTION_POLICY ||
		head->type == CCS_PROFILE) && head->r.eof &&
		head->r.ns->next != &ccs_namespace_list;
}

/**
 * ccs_find_namespace - Find specified namespace.
 *
 * @name: Name of namespace to find.
 * @len:  Length of @name.
 *
 * Returns pointer to "struct ccs_policy_namespace" if found, NULL otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static struct ccs_policy_namespace *ccs_find_namespace(const char *name,
						       const unsigned int len)
{
	struct ccs_policy_namespace *ns;
	list_for_each_entry_srcu(ns, &ccs_namespace_list, namespace_list,
				 &ccs_ss) {
		if (strncmp(name, ns->name, len) ||
		    (name[len] && name[len] != ' '))
			continue;
		return ns;
	}
	return NULL;
}

/**
 * ccs_assign_namespace - Create a new namespace.
 *
 * @domainname: Name of namespace to create.
 *
 * Returns pointer to "struct ccs_policy_namespace" on success, NULL otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static struct ccs_policy_namespace *ccs_assign_namespace
(const char *domainname)
{
	struct ccs_policy_namespace *ptr;
	struct ccs_policy_namespace *entry;
	const char *cp = domainname;
	unsigned int len = 0;
	while (*cp && *cp++ != ' ')
		len++;
	ptr = ccs_find_namespace(domainname, len);
	if (ptr)
		return ptr;
	if (len >= CCS_EXEC_TMPSIZE - 10 || !ccs_domain_def(domainname))
		return NULL;
	entry = kzalloc(sizeof(*entry) + len + 1, CCS_GFP_FLAGS);
	if (!entry)
		return NULL;
	if (mutex_lock_interruptible(&ccs_policy_lock))
		goto out;
	ptr = ccs_find_namespace(domainname, len);
	if (!ptr && ccs_memory_ok(entry, sizeof(*entry) + len + 1)) {
		char *name = (char *) (entry + 1);
		ptr = entry;
		memmove(name, domainname, len);
		name[len] = '\0';
		entry->name = name;
		ccs_init_policy_namespace(entry);
		entry = NULL;
	}
	mutex_unlock(&ccs_policy_lock);
out:
	kfree(entry);
	return ptr;
}

/**
 * ccs_namespace_jump - Check for namespace jump.
 *
 * @domainname: Name of domain.
 *
 * Returns true if namespace differs, false otherwise.
 */
static bool ccs_namespace_jump(const char *domainname)
{
	const char *namespace = ccs_current_namespace()->name;
	const int len = strlen(namespace);
	return strncmp(domainname, namespace, len) ||
		(domainname[len] && domainname[len] != ' ');
}

/**
 * ccs_assign_domain - Create a domain or a namespace.
 *
 * @domainname: The name of domain.
 * @transit:    True if transit to domain found or created.
 *
 * Returns pointer to "struct ccs_domain_info" on success, NULL otherwise.
 *
 * Caller holds ccs_read_lock().
 */
struct ccs_domain_info *ccs_assign_domain(const char *domainname,
					  const bool transit)
{
	struct ccs_security *security = ccs_current_security();
	struct ccs_domain_info e = { };
	struct ccs_domain_info *entry = ccs_find_domain(domainname);
	bool created = false;
	if (entry) {
		if (transit) {
			/*
			 * Since namespace is created at runtime, profiles may
			 * not be created by the moment the process transits to
			 * that domain. Do not perform domain transition if
			 * profile for that domain is not yet created.
			 */
			if (ccs_policy_loaded &&
			    !entry->ns->profile_ptr[entry->profile])
				return NULL;
			security->ccs_domain_info = entry;
		}
		return entry;
	}
	/* Requested domain does not exist. */
	/* Don't create requested domain if domainname is invalid. */
	if (strlen(domainname) >= CCS_EXEC_TMPSIZE - 10 ||
	    !ccs_correct_domain(domainname))
		return NULL;
	/*
	 * Since definition of profiles and acl_groups may differ across
	 * namespaces, do not inherit "use_profile" and "use_group" settings
	 * by automatically creating requested domain upon domain transition.
	 */
	if (transit && ccs_namespace_jump(domainname))
		return NULL;
	e.ns = ccs_assign_namespace(domainname);
	if (!e.ns)
		return NULL;
	/*
	 * "use_profile" and "use_group" settings for automatically created
	 * domains are inherited from current domain. These are 0 for manually
	 * created domains.
	 */
	if (transit) {
		const struct ccs_domain_info *domain =
			security->ccs_domain_info;
		e.profile = domain->profile;
		e.group = domain->group;
	}
	e.domainname = ccs_get_name(domainname);
	if (!e.domainname)
		return NULL;
	if (mutex_lock_interruptible(&ccs_policy_lock))
		goto out;
	entry = ccs_find_domain(domainname);
	if (!entry) {
		entry = ccs_commit_ok(&e, sizeof(e));
		if (entry) {
			INIT_LIST_HEAD(&entry->acl_info_list);
			list_add_tail_rcu(&entry->list, &ccs_domain_list);
			created = true;
		}
	}
	mutex_unlock(&ccs_policy_lock);
out:
	ccs_put_name(e.domainname);
	if (entry && transit) {
		security->ccs_domain_info = entry;
		if (created) {
			struct ccs_request_info r;
			ccs_init_request_info(&r, CCS_MAC_FILE_EXECUTE);
			r.granted = false;
			ccs_write_log(&r, "use_profile %u\n", entry->profile);
			ccs_write_log(&r, "use_group %u\n", entry->group);
			ccs_update_stat(CCS_STAT_POLICY_UPDATES);
		}
	}
	return entry;
}

/**
 * ccs_parse_policy - Parse a policy line.
 *
 * @head: Poiter to "struct ccs_io_buffer".
 * @line: Line to parse.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds ccs_read_lock().
 */
static int ccs_parse_policy(struct ccs_io_buffer *head, char *line)
{
	/* Delete request? */
	head->w.is_delete = !strncmp(line, "delete ", 7);
	if (head->w.is_delete)
		memmove(line, line + 7, strlen(line + 7) + 1);
	/* Selecting namespace to update. */
	if (head->type == CCS_EXCEPTION_POLICY || head->type == CCS_PROFILE) {
		if (*line == '<') {
			char *cp = strchr(line, ' ');
			if (cp) {
				*cp++ = '\0';
				head->w.ns = ccs_assign_namespace(line);
				memmove(line, cp, strlen(cp) + 1);
			} else
				head->w.ns = NULL;
		} else
			head->w.ns = &ccs_kernel_namespace;
		/* Don't allow updating if namespace is invalid. */
		if (!head->w.ns)
			return -ENOENT;
	}
	/* Do the update. */
	switch (head->type) {
	case CCS_DOMAIN_POLICY:
		return ccs_write_domain(head);
	case CCS_EXCEPTION_POLICY:
		return ccs_write_exception(head);
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	case CCS_EXECUTE_HANDLER:
#endif
	case CCS_PROCESS_STATUS:
		return ccs_write_pid(head);
	case CCS_STAT:
		return ccs_write_stat(head);
	case CCS_PROFILE:
		return ccs_write_profile(head);
	case CCS_QUERY:
		return ccs_write_answer(head);
	case CCS_MANAGER:
		return ccs_write_manager(head);
	default:
		return -ENOSYS;
	}
}

/**
 * ccs_policy_io_init - Register hooks for policy I/O.
 *
 * Returns nothing.
 */
static void __init ccs_policy_io_init(void)
{
	ccsecurity_ops.check_profile = ccs_check_profile;
}

/**
 * ccs_load_builtin_policy - Load built-in policy.
 *
 * Returns nothing.
 */
static void __init ccs_load_builtin_policy(void)
{
	/*
	 * This include file is manually created and contains built-in policy
	 * named "ccs_builtin_profile", "ccs_builtin_exception_policy",
	 * "ccs_builtin_domain_policy", "ccs_builtin_manager",
	 * "ccs_builtin_stat" in the form of "static char [] __initdata".
	 */
#include "builtin-policy.h"
	u8 i;
	const int idx = ccs_read_lock();
	for (i = 0; i < 5; i++) {
		struct ccs_io_buffer head = { };
		char *start = "";
		switch (i) {
		case 0:
			start = ccs_builtin_profile;
			head.type = CCS_PROFILE;
			break;
		case 1:
			start = ccs_builtin_exception_policy;
			head.type = CCS_EXCEPTION_POLICY;
			break;
		case 2:
			start = ccs_builtin_domain_policy;
			head.type = CCS_DOMAIN_POLICY;
			break;
		case 3:
			start = ccs_builtin_manager;
			head.type = CCS_MANAGER;
			break;
		case 4:
			start = ccs_builtin_stat;
			head.type = CCS_STAT;
			break;
		}
		while (1) {
			char *end = strchr(start, '\n');
			if (!end)
				break;
			*end = '\0';
			ccs_normalize_line(start);
			head.write_buf = start;
			ccs_parse_policy(&head, start);
			start = end + 1;
		}
	}
	ccs_read_unlock(idx);
#ifdef CONFIG_CCSECURITY_OMIT_USERSPACE_LOADER
	ccs_check_profile();
#endif
}

/**
 * ccs_read_self - read() for /proc/ccs/self_domain interface.
 *
 * @file:  Pointer to "struct file".
 * @buf:   Domainname which current thread belongs to.
 * @count: Size of @buf.
 * @ppos:  Bytes read by now.
 *
 * Returns read size on success, negative value otherwise.
 */
static ssize_t ccs_read_self(struct file *file, char __user *buf, size_t count,
			     loff_t *ppos)
{
	const char *domain = ccs_current_domain()->domainname->name;
	loff_t len = strlen(domain);
	loff_t pos = *ppos;
	if (pos >= len || !count)
		return 0;
	len -= pos;
	if (count < len)
		len = count;
	if (copy_to_user(buf, domain + pos, len))
		return -EFAULT;
	*ppos += len;
	return len;
}

/**
 * ccs_open - open() for /proc/ccs/ interface.
 *
 * @inode: Pointer to "struct inode".
 * @file:  Pointer to "struct file".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int ccs_open(struct inode *inode, struct file *file)
{
	const u8 type = (unsigned long) PDE(inode)->data;
	struct ccs_io_buffer *head = kzalloc(sizeof(*head), CCS_GFP_FLAGS);
	if (!head)
		return -ENOMEM;
	if (ccs_policy_loaded && type != CCS_AUDIT) {
		kfree(head);
		return -EPERM;
	}
	mutex_init(&head->io_sem);
	head->type = type;
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	if (type == CCS_EXECUTE_HANDLER) {
		/* Allow execute_handler to read process's status. */
		if (!(ccs_current_flags() & CCS_TASK_IS_EXECUTE_HANDLER)) {
			kfree(head);
			return -EPERM;
		}
	}
#endif
	if ((file->f_mode & FMODE_READ) && type != CCS_AUDIT &&
	    type != CCS_QUERY) {
		/* Don't allocate read_buf for poll() access. */
		head->readbuf_size = 4096;
		head->read_buf = kzalloc(head->readbuf_size, CCS_GFP_FLAGS);
		if (!head->read_buf) {
			kfree(head);
			return -ENOMEM;
		}
	}
	if (file->f_mode & FMODE_WRITE) {
		head->writebuf_size = 4096;
		head->write_buf = kzalloc(head->writebuf_size, CCS_GFP_FLAGS);
		if (!head->write_buf) {
			kfree(head->read_buf);
			kfree(head);
			return -ENOMEM;
		}
	}
	/*
	 * If the file is /proc/ccs/query, increment the observer counter.
	 * The obserber counter is used by ccs_supervisor() to see if
	 * there is some process monitoring /proc/ccs/query.
	 */
	if (type == CCS_QUERY)
		atomic_inc(&ccs_query_observers);
	file->private_data = head;
	ccs_notify_gc(head, true);
	return 0;
}

/**
 * ccs_release - close() for /proc/ccs/ interface.
 *
 * @inode: Pointer to "struct inode".
 * @file:  Pointer to "struct file".
 *
 * Returns 0.
 */
static int ccs_release(struct inode *inode, struct file *file)
{
	struct ccs_io_buffer *head = file->private_data;
	/*
	 * If the file is /proc/ccs/query, decrement the observer counter.
	 */
	if (head->type == CCS_QUERY &&
	    atomic_dec_and_test(&ccs_query_observers))
		wake_up_all(&ccs_answer_wait);
	ccs_notify_gc(head, false);
	return 0;
}

/**
 * ccs_poll - poll() for /proc/ccs/ interface.
 *
 * @file: Pointer to "struct file".
 * @wait: Pointer to "poll_table". Maybe NULL.
 *
 * Returns POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM if ready to read/write,
 * POLLOUT | POLLWRNORM otherwise.
 */
static unsigned int ccs_poll(struct file *file, poll_table *wait)
{
	struct ccs_io_buffer *head = file->private_data;
	if (head->type == CCS_AUDIT) {
		if (!ccs_memory_used[CCS_MEMORY_AUDIT]) {
			poll_wait(file, &ccs_log_wait, wait);
			if (!ccs_memory_used[CCS_MEMORY_AUDIT])
				return POLLOUT | POLLWRNORM;
		}
	} else if (head->type == CCS_QUERY) {
		if (list_empty(&ccs_query_list)) {
			poll_wait(file, &ccs_query_wait, wait);
			if (list_empty(&ccs_query_list))
				return POLLOUT | POLLWRNORM;
		}
	}
	return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}

/**
 * ccs_read - read() for /proc/ccs/ interface.
 *
 * @file:  Pointer to "struct file".
 * @buf:   Pointer to buffer.
 * @count: Size of @buf.
 * @ppos:  Unused.
 *
 * Returns bytes read on success, negative value otherwise.
 */
static ssize_t ccs_read(struct file *file, char __user *buf, size_t count,
			loff_t *ppos)
{
	struct ccs_io_buffer *head = file->private_data;
	int len;
	int idx;
	if (mutex_lock_interruptible(&head->io_sem))
		return -EINTR;
	head->read_user_buf = buf;
	head->read_user_buf_avail = count;
	idx = ccs_read_lock();
	if (ccs_flush(head))
		/* Call the policy handler. */
		do {
			ccs_set_namespace_cursor(head);
			switch (head->type) {
			case CCS_DOMAIN_POLICY:
				ccs_read_domain(head);
				break;
			case CCS_EXCEPTION_POLICY:
				ccs_read_exception(head);
				break;
			case CCS_AUDIT:
				ccs_read_log(head);
				break;
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
			case CCS_EXECUTE_HANDLER:
#endif
			case CCS_PROCESS_STATUS:
				ccs_read_pid(head);
				break;
			case CCS_VERSION:
				ccs_read_version(head);
				break;
			case CCS_STAT:
				ccs_read_stat(head);
				break;
			case CCS_PROFILE:
				ccs_read_profile(head);
				break;
			case CCS_QUERY:
				ccs_read_query(head);
				break;
			case CCS_MANAGER:
				ccs_read_manager(head);
				break;
			}
		} while (ccs_flush(head) && ccs_has_more_namespace(head));
	ccs_read_unlock(idx);
	len = head->read_user_buf - buf;
	mutex_unlock(&head->io_sem);
	return len;
}

#ifdef CONFIG_CCSECURITY_TASK_DOMAIN_TRANSITION

/**
 * ccs_write_self - write() for /proc/ccs/self_domain interface.
 *
 * @file:  Pointer to "struct file".
 * @buf:   Domainname to transit to.
 * @count: Size of @buf.
 * @ppos:  Unused.
 *
 * Returns @count on success, negative value otherwise.
 *
 * If domain transition was permitted but the domain transition failed, this
 * function returns error rather than terminating current thread with SIGKILL.
 */
static ssize_t ccs_write_self(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	char *data;
	int error;
	if (!count || count >= CCS_EXEC_TMPSIZE - 10)
		return -ENOMEM;
	data = kzalloc(count + 1, CCS_GFP_FLAGS);
	if (!data)
		return -ENOMEM;
	if (copy_from_user(data, buf, count)) {
		error = -EFAULT;
		goto out;
	}
	ccs_normalize_line(data);
	if (ccs_correct_domain(data)) {
		const int idx = ccs_read_lock();
		struct ccs_path_info name;
		struct ccs_request_info r;
		name.name = data;
		ccs_fill_path_info(&name);
		/* Check "task manual_domain_transition" permission. */
		ccs_init_request_info(&r, CCS_MAC_FILE_EXECUTE);
		r.param_type = CCS_TYPE_MANUAL_TASK_ACL;
		r.param.task.domainname = &name;
		ccs_check_acl(&r);
		if (!r.granted)
			error = -EPERM;
		else
			error = ccs_assign_domain(data, true) ? 0 : -ENOENT;
		ccs_read_unlock(idx);
	} else
		error = -EINVAL;
out:
	kfree(data);
	return error ? error : count;
}

#endif

/**
 * ccs_write - write() for /proc/ccs/ interface.
 *
 * @file:  Pointer to "struct file".
 * @buf:   Pointer to buffer.
 * @count: Size of @buf.
 * @ppos:  Unused.
 *
 * Returns @count on success, negative value otherwise.
 */
static ssize_t ccs_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	struct ccs_io_buffer *head = file->private_data;
	int error = count;
	char *cp0 = head->write_buf;
	int idx;
	if (mutex_lock_interruptible(&head->io_sem))
		return -EINTR;
	head->read_user_buf_avail = 0;
	idx = ccs_read_lock();
	/* Read a line and dispatch it to the policy handler. */
	while (count) {
		char c;
		if (head->w.avail >= head->writebuf_size - 1) {
			const int len = head->writebuf_size * 2;
			char *cp = kzalloc(len, CCS_GFP_FLAGS);
			if (!cp) {
				error = -ENOMEM;
				break;
			}
			memmove(cp, cp0, head->w.avail);
			kfree(cp0);
			head->write_buf = cp;
			cp0 = cp;
			head->writebuf_size = len;
		}
		if (get_user(c, buf)) {
			error = -EFAULT;
			break;
		}
		buf++;
		count--;
		cp0[head->w.avail++] = c;
		if (c != '\n')
			continue;
		cp0[head->w.avail - 1] = '\0';
		head->w.avail = 0;
		ccs_normalize_line(cp0);
		if (!strcmp(cp0, "reset")) {
			head->w.ns = &ccs_kernel_namespace;
			head->w.domain = NULL;
			memset(&head->r, 0, sizeof(head->r));
			continue;
		}
		/* Don't allow updating policies by non manager programs. */
		switch (head->type) {
		case CCS_PROCESS_STATUS:
			/* This does not write anything. */
			break;
		case CCS_DOMAIN_POLICY:
			if (ccs_select_domain(head, cp0))
				continue;
			/* fall through */
		case CCS_EXCEPTION_POLICY:
			if (!strcmp(cp0, "select transition_only")) {
				head->r.print_transition_related_only = true;
				continue;
			}
			/* fall through */
		default:
			if (!ccs_manager()) {
				error = -EPERM;
				goto out;
			}
		}
		switch (ccs_parse_policy(head, cp0)) {
		case -EPERM:
			error = -EPERM;
			goto out;
		case 0:
			/* Update statistics. */
			switch (head->type) {
			case CCS_DOMAIN_POLICY:
			case CCS_EXCEPTION_POLICY:
			case CCS_STAT:
			case CCS_PROFILE:
			case CCS_MANAGER:
				ccs_update_stat(CCS_STAT_POLICY_UPDATES);
				break;
			default:
				break;
			}
			break;
		}
	}
out:
	ccs_read_unlock(idx);
	mutex_unlock(&head->io_sem);
	return error;
}

/**
 * ccs_create_entry - Create interface files under /proc/ccs/ directory.
 *
 * @name:   The name of the interface file.
 * @mode:   The permission of the interface file.
 * @parent: The parent directory.
 * @key:    Type of interface.
 *
 * Returns nothing.
 */
static void __init ccs_create_entry(const char *name, const umode_t mode,
				    struct proc_dir_entry *parent,
				    const u8 key)
{
	struct proc_dir_entry *entry = create_proc_entry(name, mode, parent);
	if (entry) {
		entry->proc_fops = &ccs_operations;
		entry->data = ((u8 *) NULL) + key;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
		if (entry->proc_iops)
			ccs_file_inode_operations = *entry->proc_iops;
		if (!ccs_file_inode_operations.setattr)
			ccs_file_inode_operations.setattr = proc_notify_change;
		entry->proc_iops = &ccs_file_inode_operations;
#endif
	}
}

/**
 * ccs_proc_init - Initialize /proc/ccs/ interface.
 *
 * Returns nothing.
 */
static void __init ccs_proc_init(void)
{
	struct proc_dir_entry *ccs_dir = proc_mkdir("ccs", NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
	if (ccs_dir->proc_iops)
		ccs_dir_inode_operations = *ccs_dir->proc_iops;
	if (!ccs_dir_inode_operations.setattr)
		ccs_dir_inode_operations.setattr = proc_notify_change;
	ccs_dir->proc_iops = &ccs_dir_inode_operations;
#endif
	ccs_create_entry("query",            0600, ccs_dir, CCS_QUERY);
	ccs_create_entry("domain_policy",    0600, ccs_dir, CCS_DOMAIN_POLICY);
	ccs_create_entry("exception_policy", 0600, ccs_dir,
			 CCS_EXCEPTION_POLICY);
	ccs_create_entry("audit",            0400, ccs_dir, CCS_AUDIT);
	ccs_create_entry(".process_status",  0600, ccs_dir,
			 CCS_PROCESS_STATUS);
	ccs_create_entry("stat",             0644, ccs_dir, CCS_STAT);
	ccs_create_entry("profile",          0600, ccs_dir, CCS_PROFILE);
	ccs_create_entry("manager",          0600, ccs_dir, CCS_MANAGER);
	ccs_create_entry("version",          0400, ccs_dir, CCS_VERSION);
#ifdef CONFIG_CCSECURITY_TASK_EXECUTE_HANDLER
	ccs_create_entry(".execute_handler", 0666, ccs_dir,
			 CCS_EXECUTE_HANDLER);
#endif
	{
		struct proc_dir_entry *e = create_proc_entry("self_domain",
							     0666, ccs_dir);
		if (e)
			e->proc_fops = &ccs_self_operations;
	}
}

/**
 * ccs_init_module - Initialize this module.
 *
 * Returns 0 on success, negative value otherwise.
 */
static int __init ccs_init_module(void)
{
	if (ccsecurity_ops.disabled)
		return -EINVAL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	MOD_INC_USE_COUNT;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
	if (init_srcu_struct(&ccs_ss))
		panic("Out of memory.");
#endif
	ccs_kernel_namespace.name = "<kernel>";
	ccs_init_policy_namespace(&ccs_kernel_namespace);
	ccs_kernel_domain.ns = &ccs_kernel_namespace;
	INIT_LIST_HEAD(&ccs_kernel_domain.acl_info_list);
	ccs_mm_init();
	ccs_policy_io_init();
	ccs_permission_init();
	ccs_proc_init();
	ccs_load_builtin_policy();
	return 0;
}

MODULE_LICENSE("GPL");
module_init(ccs_init_module);
