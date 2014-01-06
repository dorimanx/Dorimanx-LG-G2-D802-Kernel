
/*             
  
                                        
                                             
  
                             
 */
#include <linux/semaphore.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mount.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <linux/pid.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#define FILE_PATHLEN	200
#define FILE_NAMELEN	100
#define PROC_NAMELEN	16
#define PROF_BUF_SIZE	2000
#define PROF_TIMEOUT	60
//-------------------------------------------------------------------------------
#define PROF_NOT	0
#define PROF_INIT	1
#define PROF_RUN	2
#define PROF_OPT	3
#define PROF_DONE	4

//#define DEBUG
#ifdef DEBUG
#define _DBG(fmt, args...) printk(KERN_DEBUG "%s: " fmt "\n", __FUNCTION__, ##args)
#else
#define _DBG(fmt, args...) do { } while(0);
#endif

#define ALIGNPAGECACHE(x) (( (x) >> PAGE_CACHE_SHIFT) << PAGE_CACHE_SHIFT)
#define E_ALIGNPAGECACHE(x) \
	((((x) - 1 + PAGE_CACHE_SIZE) >> PAGE_CACHE_SHIFT) << PAGE_CACHE_SHIFT)

struct sreadahead_profdata {
	char procname[PROC_NAMELEN];
	unsigned char name[FILE_PATHLEN+FILE_NAMELEN];
	long long len; // same as pos[][1] - pos[][0]
	long long pos[2]; // 0: start position 1: end position
};

struct sreadahead_prof {
	struct sreadahead_profdata *data;
	int state;
	int file_cnt;
	int read_cnt;
	struct mutex ulock;
	struct timer_list timer;
	struct work_struct free_work;
};

int sreadahead_prof(struct file *filp, size_t len, loff_t pos);
/*              */
