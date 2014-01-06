
/*             
  
                                        
                                             
  
                             
 */
#include "mount.h"
#include "ext4/ext4.h"
#include "sreadahead_prof.h"

static struct sreadahead_prof prof_buf;

//--------------------------------------------------------------
// functions - work queue
//--------------------------------------------------------------
static void prof_buf_free_work(struct work_struct *data)
{
	_DBG("mem of prof_buf is freed by vfree()");
	mutex_lock(&prof_buf.ulock);
	prof_buf.state = PROF_NOT;
	vfree(prof_buf.data);
	prof_buf.data = NULL;
	mutex_unlock(&prof_buf.ulock);
}

//--------------------------------------------------------------
// functions - timer
//--------------------------------------------------------------
static void prof_timer_handler(unsigned long arg)
{
	_DBG("profiling state - %d\n", prof_buf.state);
	schedule_work(&prof_buf.free_work);
}

//--------------------------------------------------------------
// functions - initialization of debugfs
//--------------------------------------------------------------

static ssize_t sreadahead_dbgfs_read(
		struct file *file,
		char __user *buff,
		size_t buff_count,
		loff_t *ppos)
{
	struct sreadahead_profdata data;

	mutex_lock(&prof_buf.ulock);
	if (prof_buf.read_cnt >= prof_buf.file_cnt) {
		vfree(prof_buf.data);
		prof_buf.data = NULL;
		mutex_unlock(&prof_buf.ulock);
		return 0;
	}

	data = prof_buf.data[prof_buf.read_cnt++];
	mutex_unlock(&prof_buf.ulock);

	_DBG("%s:%lld:%lld#%s -- read_cnt:%d",
		data.name, data.pos[0], data.len, data.procname, prof_buf.read_cnt);

	if (copy_to_user(buff, &data, sizeof(struct sreadahead_profdata)))
		return 0;

	(*ppos) = 0;
	return 1;
}

static ssize_t sreadaheadflag_dbgfs_read(
		struct file *file,
		char __user *buff,
		size_t buff_count,
		loff_t *ppos)
{
	if (copy_to_user(buff, &prof_buf.state, sizeof(int)))
		return 0;
	(*ppos) = 0;
	return sizeof(int);
}

static ssize_t sreadaheadflag_dbgfs_write(
		struct file *file,
		const char __user *buff,
		size_t count,
		loff_t *ppos)
{
	if (copy_from_user(&prof_buf.state, buff, sizeof(int)))
		return 0;

	if (prof_buf.state == PROF_INIT) {
		_DBG("PROF_INT & add timer");
		prof_buf.timer.expires = get_jiffies_64() + (PROF_TIMEOUT * HZ);
		add_timer(&prof_buf.timer);
	} else if (prof_buf.state == PROF_DONE) {
		_DBG("PROF_DONE by user daemon(boot_completed) & del timer");
		del_timer(&prof_buf.timer);
	}

	(*ppos) = 0;
	return sizeof(int);
}

static const struct file_operations sreadaheadflag_dbgfs_fops = {
	.read = sreadaheadflag_dbgfs_read,
	.write = sreadaheadflag_dbgfs_write,
};

static const struct file_operations sreadahead_dbgfs_fops = {
	.read = sreadahead_dbgfs_read,
};

static int __init sreadahead_init(void)
{
	struct dentry *dbgfs_dir;

	/* state init */
	prof_buf.state = PROF_NOT;

	/* lock init */
	mutex_init(&prof_buf.ulock);

	/* timer init */
	init_timer(&prof_buf.timer);
	prof_buf.timer.function = prof_timer_handler;

	/* work struct init */
	INIT_WORK(&prof_buf.free_work, prof_buf_free_work);

	/* debugfs init for sreadahead */
	dbgfs_dir = debugfs_create_dir("sreadahead", NULL);
	if (!dbgfs_dir)
		return (-1);
	debugfs_create_file("profilingdata",
			0444, dbgfs_dir, NULL,
			&sreadahead_dbgfs_fops);
	debugfs_create_file("profilingflag",
			0644, dbgfs_dir, NULL,
			&sreadaheadflag_dbgfs_fops);
	return 0;
}

__initcall(sreadahead_init);

//--------------------------------------------------------------
// functions - sreadahead profiling
//--------------------------------------------------------------

static int get_absolute_path(unsigned char* buf, int buflen, struct file *filp)
{
	unsigned char tmpstr[FILE_PATHLEN+FILE_NAMELEN];
	struct dentry* tmpdentry = 0;
	struct mount* tmpmnt;
	struct mount* tmpoldmnt;
	tmpmnt = real_mount(filp->f_vfsmnt);

	tmpdentry = filp->f_path.dentry->d_parent;
	do {
		tmpoldmnt = tmpmnt;
		while (!IS_ROOT(tmpdentry)) {
			strcpy(tmpstr, buf);
			// byungchul.park@lge.com
			// make codes robust
			strncpy(buf, tmpdentry->d_name.name, buflen - 1);
			buf[buflen - 1] = '\0';
			if (strlen(buf) + strlen("/") > buflen -1)
				return -1;
			else
				strcat(buf, "/");

			if (strlen(buf) + strlen(tmpstr) > buflen -1)
				return -1;
			else
				strcat(buf, tmpstr);

			tmpdentry = tmpdentry->d_parent;
		}
		tmpdentry = tmpmnt->mnt_mountpoint;
		tmpmnt = tmpmnt->mnt_parent;
	} while (tmpmnt != tmpoldmnt);
	strcpy(tmpstr, buf);
	strcpy(buf, "/");
	// byungchul.park@lge.com
	// make codes robust
	if (strlen(buf) + strlen(tmpstr) > buflen -1)
		return -1;
	strcat(buf, tmpstr);

	return 0;
}

static int sreadahead_prof_RUN(struct file *filp, size_t len, loff_t pos)
{
	int i;
	struct sreadahead_profdata data;
	memset(&data, 0x00, sizeof(struct sreadahead_profdata));
	data.len = (long long)len;
	data.pos[0] = pos;
	data.pos[1] = 0;
	data.procname[0] = '\0';
	get_task_comm(data.procname, current);

	if (get_absolute_path(data.name, FILE_PATHLEN + FILE_NAMELEN, filp) < 0) return -1;
	strcat(data.name, filp->f_path.dentry->d_name.name);

	mutex_lock(&prof_buf.ulock);

	if (prof_buf.data == NULL) /* vfree called */
		return -1;

	for(i = 0; i < prof_buf.file_cnt; ++i){
		if (strncmp(prof_buf.data[i].name, data.name, FILE_PATHLEN + FILE_NAMELEN) == 0){
			break;
		}
	}
	// add a new entry
	if (i == prof_buf.file_cnt){
		strncpy(prof_buf.data[i].procname, data.procname, PROC_NAMELEN);
		prof_buf.data[i].procname[PROC_NAMELEN - 1] = '\0';
		strncpy(prof_buf.data[i].name, data.name, FILE_PATHLEN + FILE_NAMELEN);
		prof_buf.data[i].name[FILE_PATHLEN + FILE_NAMELEN - 1] = '\0';
		prof_buf.data[i].pos[0] = prof_buf.data[i].pos[1] = ALIGNPAGECACHE(data.pos[0]);
		prof_buf.data[i].pos[1] += E_ALIGNPAGECACHE((long long)data.len);
		prof_buf.data[i].len = prof_buf.data[i].pos[1] - prof_buf.data[i].pos[0];
		prof_buf.file_cnt++;

		_DBG("New Entry - %s:%lld:%lld#%s -- cnt:%d",
				prof_buf.data[i].name,
				prof_buf.data[i].pos[0],
				prof_buf.data[i].len,
				prof_buf.data[i].procname,
				prof_buf.file_cnt);
	}

	if (prof_buf.file_cnt >= PROF_BUF_SIZE) {
		_DBG("PROF_DONE by kernel(file_cnt) & del timer");
		prof_buf.state = PROF_DONE;
		del_timer(&prof_buf.timer);
	}

	mutex_unlock(&prof_buf.ulock);

	return 0;
}

static int sreadahead_profdata_init(void)
{
	mutex_lock(&prof_buf.ulock);
	if(prof_buf.state != PROF_INIT) {
		mutex_unlock(&prof_buf.ulock);
		return 0;

	}

	prof_buf.data = (struct sreadahead_profdata*)vmalloc(sizeof(struct sreadahead_profdata) * PROF_BUF_SIZE);

	if (prof_buf.data == NULL)
		return -1;

	memset(prof_buf.data, 0x00, sizeof(struct sreadahead_profdata) * PROF_BUF_SIZE);
	prof_buf.state = PROF_RUN;

	mutex_unlock(&prof_buf.ulock);
	return 0;
}

int sreadahead_prof(struct file *filp, size_t len, loff_t pos)
{
	if (prof_buf.state == PROF_NOT || prof_buf.state == PROF_DONE)
		return 0;
	if (prof_buf.state == PROF_INIT)
		sreadahead_profdata_init();
	if (prof_buf.state == PROF_RUN) {
		if (filp->f_op == &ext4_file_operations)
			sreadahead_prof_RUN(filp, len, pos);
	}
	return 0;
}
/*              */
