/*
 *	Copyright (c) AuthenTec, Inc. 2011-2012
 *	All Rights Reserved
 *
 *	This software is open source; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This General Public License does NOT permit incorporating this software
 *	into proprietary programs.  If you are unable to comply with the GPL, a
 *	commercial license for this software may be purchased from AuthenTec at
 *	http://www.authentec.com/Products/EmbeddedSecurity/SecurityToolkits.aspx
 *
 *	This program is distributed in WITHOUT ANY WARRANTY; without even the
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *	See the GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *	http://www.gnu.org/copyleft/gpl.html
 */
/******************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/crypto.h>
#include <linux/vmalloc.h>
#include <crypto/scatterwalk.h>

#include "kfips.h"

#define assert(C)  if (C) ; else        \
	{ pr_err("ASSERT(%s:%d)\n", __FILE__, __LINE__); }

/* Module parameters no g_ prefix as it would look ugly externally. */

/* Uid of the user that the /proc/ device is given to. */
static int uid = 1000;

/* Per-crypto-alg-transform context, which contains the key material. */
struct kfips_transform_context {
	int keylen;
	uint32_t key[(AES_MAX_KEY_SIZE * 2) / sizeof(uint32_t)];
};

/* Per-request context, which contains the in-kernel list pointers
 * (for g_pending or g_sent) as well as pointer to the SHM handle. */
struct kfips_request_context {
	/* DO NOT MOVE this! assumption that typecasts can be done
	 * exists in few places. */
	struct list_head list;

	/* Request-related flags (See KFIPS_FLAGS_*). */
	uint8_t flags;

	/* Available if in g_sent, but not if in g_pending */
	kfips_msg_t *msg;

	/* Available always */
	struct ablkcipher_request *req;
};

static struct proc_dir_entry *g_proc_entry;
static pid_t g_pid = 0;

static kfips_queue_t *g_queue;

/* This lock is used to protect access to g_queue, and to the g_sent
 * and g_pending. */
static DEFINE_SPINLOCK(g_lock);
static LIST_HEAD(g_pending);
static LIST_HEAD(g_sent);

/* Wait queue for blocking file operations */
static DECLARE_WAIT_QUEUE_HEAD(g_file_wq);

/* Get real allocated size in multiple of PAGE_SIZE, as e.g. mmap
 * rounds to it.
 */
static size_t kfips_real_size(size_t size)
{
	size_t real_size = size;
	if (real_size % PAGE_SIZE)
		real_size += PAGE_SIZE - real_size % PAGE_SIZE;
	return real_size;
}

/* Get number of pages within scatterlist that are needed to store
 * nbytes bytes of data. It is interesting question how we should
 * behave in error cases; hopefully sg_copy functions do not overwrite
 * memory if running out of lists! */
static int sg_count(struct scatterlist *sg, size_t nbytes)
{
	int n = 0;

	while (sg && nbytes > 0) {
		n++;
		nbytes -= sg->length;
		sg = scatterwalk_sg_next(sg);
	}
	if (nbytes > 0 && !sg) {
		pr_err("too short input to sg_count!");
	}

	return n;
}

/* Flush a request to the memory structure, and potentially wake up
 * the reading user-land application. The call should be done with the
 * spinlock held. If the queue is full, NULL is returned, otherwise
 * the place within kfips_queue.
 */
static bool flush_request_locked(struct ablkcipher_request *req)
{
	struct kfips_transform_context *ctx =
	    crypto_ablkcipher_ctx(crypto_ablkcipher_reqtfm(req));
	struct kfips_request_context *rctx = ablkcipher_request_ctx(req);
	kfips_msg_t *msg;

	if (!(msg = kfips_queue_insreq_begin(g_queue)))
		return false;
	rctx->msg = msg;
	memcpy(msg->key, ctx->key, ctx->keylen);
	memcpy(msg->iv, req->info, AES_BLOCK_SIZE);
	msg->pointer = req;
	msg->keylen = ctx->keylen;
	msg->flags = rctx->flags;
	sg_copy_to_buffer(req->src, sg_count(req->dst, req->nbytes),
			  msg->buf, req->nbytes);
	msg->len = req->nbytes;
	kfips_queue_insreq_end(g_queue, msg);
	wake_up_interruptible(&g_file_wq);
	return true;
}

/* Copy all dirty/done requests' content back to the ring, and clear
 * the dirty/done bits. In general, dirty requests are ones that have
 * been touched by the userland, but haven't yet received completion
 * message from the userland. The assumption is that when this is
 * called, there is no pending processing in the userland, nor
 * anything inbound on the file descriptor => everything dirty is
 * tainted, as userland would operate again on the same data (or worse
 * yet, corrupted data).
 */
static void recopy_dirty_locked(void)
{
	struct kfips_request_context *rctx;
	struct ablkcipher_request *req;
	kfips_msg_t *msg;
	struct list_head *l;

	list_for_each(l, &g_sent) {
		rctx = (struct kfips_request_context *) l;
		msg = rctx->msg;
		assert(msg);
		if (msg->request_valid) {
			req = msg->pointer;
			if (req) {
				msg->response_valid = 0;
				/* Do the same scatter-gather copy
				 * operation as within
				 * flush_request_locked. */
				sg_copy_to_buffer(req->src,
						  sg_count(req->dst,
							   req->nbytes),
						  msg->buf, req->nbytes);
				msg->len = req->nbytes;
			}
		}
	}
}


static int file_open(struct inode *inode, struct file *filp);
static int file_release(struct inode *inode, struct file *filp);
static int file_mmap(struct file *, struct vm_area_struct *vma);
static long file_ioctl(struct file *, unsigned int cmd, unsigned long arg);
static ssize_t file_read(struct file *, char *b, size_t c, loff_t * pos);
static ssize_t file_write(struct file *, const char *b, size_t c,
			  loff_t * pos);

static struct file_operations file_fops = {
	.open = file_open,
	.release = file_release,
	.unlocked_ioctl = file_ioctl,
	.read = file_read,
	.write = file_write,
	.mmap = file_mmap,
	.owner = THIS_MODULE,
};

static int file_open(struct inode *inode, struct file *filp)
{
	if (g_pid) {
		pr_err("Pid %d already attached.\n", g_pid);
		return -EPERM;
	}
	g_pid = current->pid;
	pr_info("Process %d connected\n", g_pid);
	return 0;
}

static int file_release(struct inode *inode, struct file *filp)
{
	pr_info("Process %d disconnected\n", g_pid);
	g_pid = 0;
	/* At this point, we have to refresh the contents of the
	   ring where applicable. */
	spin_lock_bh(&g_lock);
	recopy_dirty_locked();
	spin_unlock_bh(&g_lock);
	return 0;
}

static int file_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	size_t length = vma->vm_end - vma->vm_start;
	unsigned long start = vma->vm_start;
	char *vmalloc_area_ptr = (char *) g_queue;
	unsigned long pfn;
	size_t rsize = kfips_real_size(sizeof(kfips_queue_t));

	if (length > rsize) {
		pr_err
		    ("mmap - Desired size %d > local size %d [qsize %d]\n",
		     (int) length, (int) rsize, (int) sizeof(kfips_queue_t));
		return -EIO;
	}
	if (length % PAGE_SIZE) {
		pr_err
		    ("mmap - non-event # of pages required (length %d)\n",
		     (int) length);
		return -EIO;
	}
	/* As one call remaps only ~page we iterate and map each page
	 * individually. */
	pfn = vmalloc_to_pfn(vmalloc_area_ptr);
	while (length > 0) {
		pfn = vmalloc_to_pfn(vmalloc_area_ptr);
		if ((ret = remap_pfn_range(vma, start, pfn,
					   PAGE_SIZE, PAGE_SHARED)) < 0) {
			pr_err
			    ("remap_pfn_range failed: %d [size:%d / %d]\n",
			     ret, (int) length, (int) rsize);
			return ret;
		}
		start += PAGE_SIZE;
		vmalloc_area_ptr += PAGE_SIZE;
		length -= PAGE_SIZE;
	}
	return 0;
}

static long file_ioctl(struct file *filp, unsigned int cmd,
		       unsigned long arg)
{
	return 0;
}

static ssize_t file_read(struct file *filp, char *buf, size_t count,
			 loff_t * pos)
{
	bool want_sleep;
	/* We produce an artificial EOF at ~second interval, even if
	 * nothing received in the g_file_wq, and faster, if we do
	 * have something. */
	spin_lock_bh(&g_lock);
	want_sleep = list_empty(&g_sent);
	spin_unlock_bh(&g_lock);
	if (want_sleep)
		interruptible_sleep_on_timeout(&g_file_wq, HZ);
	return 0;
}

static ssize_t file_write(struct file *filp, const char *buf, size_t count,
			  loff_t * pos)
{
	struct ablkcipher_request *req;
	struct ablkcipher_request *qreq;
	kfips_msg_t *msg;
	struct kfips_request_context *rctx, *qrctx;

	if (!count) {
		pr_debug("Got 0 to file_write\n");
		return 0;
	}

	if (count != 4) {
		pr_err("Invalid write size (expected 4)\n");
		return -EINVAL;
	}

	/* Assumption is that data comes in the order it was given to
	 * the userland.
	 */
	pr_debug("Processing responses\n");
	spin_lock_bh(&g_lock);

	while ((msg = kfips_queue_remrsp_begin(g_queue))) {

		if (list_empty(&g_sent)) {
			pr_err("g_sent empty yet write from userland.\n");
			kfips_queue_remrsp_end(g_queue, msg);
			spin_unlock_bh(&g_lock);
			return -EINVAL;
		}
		/* Make sure the things come in order - the code
		 * makes that assumption fairly strictly right now. */
		req = msg->pointer;
		rctx = ablkcipher_request_ctx(req);

		qrctx = (struct kfips_request_context *) g_sent.next;
		list_del(&qrctx->list);
		qreq = qrctx->req;

		/* Mark that we're done with this particular message. */
		msg->flags = 0;

		if (qreq != req) {
			/* Also the dequeue request should be the oldest. */
			pr_err("Queue screwed up");
			msg->pointer = NULL;
			kfips_queue_remrsp_end(g_queue, msg);
			spin_unlock_bh(&g_lock);
			qreq->base.complete(&qreq->base, -EINVAL);
		} else {
			assert(rctx && req->dst);
			if (rctx && req->dst) {
				sg_copy_from_buffer(req->dst,
						    sg_count(req->dst,
							     req->nbytes),
						    msg->buf, req->nbytes);
			}
			kfips_queue_remrsp_end(g_queue, msg);
			spin_unlock_bh(&g_lock);
			if (req)
				req->base.complete(&req->base, 0);
		}
		spin_lock_bh(&g_lock);
	}


	/* Finally, check if there is something in the g_sent
	 * that needs to be flushed now that we have again space.  To
	 * prevent congestion, we just fire off the first one (so that
	 * if there is multiple parties doing more-than-we-can-handle
	 * I/O, we wind up with roughly ~fair usage, each party gets
	 * to utilize us for one block)
	 */
	req = NULL;
	if (!list_empty(&g_pending)) {
		rctx = (struct kfips_request_context *) g_pending.next;
		req = rctx->req;
		assert(req);
		/* If we manage to send it, have put it to
		 * g_sent. Otherwise keep it in g_pending to wait for
		 * more entries to complete for more space. */
		if (flush_request_locked(req)) {
			list_del(&rctx->list);
			list_add_tail(&rctx->list, &g_sent);
		} else {
			req = NULL;
		}
	}
	spin_unlock_bh(&g_lock);
	/* We have someone to wake (to start sending us more stuff) */
	/* => send -EINPROGRESS completion */
	if (req) {
		pr_debug("Resuming after EBUSY - sending -EINPROGRESS\n");
		req->base.complete(&req->base, -EINPROGRESS);
	}
	return 0;
}

static int kfips_aes_qcrypt(struct ablkcipher_request *req, uint8_t flags)
{
	struct kfips_request_context *rctx = ablkcipher_request_ctx(req);
	int rc;
	int rflags = req->base.flags;

	/* For all AES modes, we require a minimum amount of data */
	if (req->nbytes < AES_BLOCK_SIZE) {
		pr_err("request size %d less than AES block size\n",
		       (int)req->nbytes);
		return -EINVAL;
	}
	if (req->nbytes > sizeof g_queue->msg[0].buf) {
		pr_err
		    ("request size %d greater than maximum supported\n",
		     (int)req->nbytes);
		return -ENOMEM;
	}

	if (req->nbytes % AES_BLOCK_SIZE) {
		pr_err("request size is not multiple of AES block size\n");
		return -EINVAL;
	}
	if (!(rflags & CRYPTO_TFM_REQ_MAY_SLEEP)) {
		pr_err("non-sleeping request for kfips");
		return -EINVAL;
	}

	if (!req->base.complete) {
		pr_err("no completion callback?!?");
		return -EINVAL;
	}

	/* Store the initial flags in the request context, just in
	 * case they're needed later on (likely if we don't fit in the
	 * ring)  */
	rctx->flags = flags;

	/* And backpointer to the request itself. */
	rctx->req = req;

	/* Lock the queue, attempt send the request. */
	spin_lock_bh(&g_lock);
	if (flush_request_locked(req)) {
		/* Put the request in the sent queue. */
		list_add_tail(&rctx->list, &g_sent);
		rc = -EINPROGRESS;
	} else {
		/* Put the request in the pending queue. */
		list_add_tail(&rctx->list, &g_pending);
		rc = -EBUSY;
	}

	spin_unlock_bh(&g_lock);

	assert(rc == -EINPROGRESS || rc == -EBUSY);

	return rc;
}

static int kfips_aes_setkey(struct crypto_ablkcipher *tfm, const u8 * key,
			    unsigned int keylen)
{
	struct ablkcipher_alg *alg = crypto_ablkcipher_alg(tfm);
	struct kfips_transform_context *ctx = crypto_ablkcipher_ctx(tfm);

	if (alg->min_keysize == AES_MIN_KEY_SIZE) {
		if (keylen != AES_KEYSIZE_128 &&
		    keylen != AES_KEYSIZE_192 &&
		    keylen != AES_KEYSIZE_256)
			return -EINVAL;
	} else if (alg->min_keysize == AES_MIN_KEY_SIZE * 2) {
		if (keylen != AES_KEYSIZE_128 * 2 &&
		    keylen != AES_KEYSIZE_192 * 2 &&
		    keylen != AES_KEYSIZE_256 * 2)
			return -EINVAL;
	} else {
		return -EINVAL;
	}
	memcpy(ctx->key, key, keylen);
	ctx->keylen = keylen;
	return 0;
}

static int kfips_aes_ecb_encrypt(struct ablkcipher_request *req)
{
	return kfips_aes_qcrypt(req, KFIPS_FLAGS_ENCRYPT | KFIPS_FLAGS_ECB);
}

static int kfips_aes_ecb_decrypt(struct ablkcipher_request *req)
{
	return kfips_aes_qcrypt(req, KFIPS_FLAGS_DECRYPT | KFIPS_FLAGS_ECB);
}

static int kfips_aes_cbc_encrypt(struct ablkcipher_request *req)
{
	return kfips_aes_qcrypt(req, KFIPS_FLAGS_ENCRYPT | KFIPS_FLAGS_CBC);
}

static int kfips_aes_cbc_decrypt(struct ablkcipher_request *req)
{
	return kfips_aes_qcrypt(req, KFIPS_FLAGS_DECRYPT | KFIPS_FLAGS_CBC);
}

static int kfips_aes_xts_encrypt(struct ablkcipher_request *req)
{
	return kfips_aes_qcrypt(req, KFIPS_FLAGS_ENCRYPT | KFIPS_FLAGS_XTS);
}

static int kfips_aes_xts_decrypt(struct ablkcipher_request *req)
{
	return kfips_aes_qcrypt(req, KFIPS_FLAGS_DECRYPT | KFIPS_FLAGS_XTS);
}

static int kfips_aes_cra_init(struct crypto_tfm *tfm)
{
	tfm->crt_ablkcipher.reqsize = sizeof(struct kfips_request_context);
	return 0;
}

static void kfips_aes_cra_exit(struct crypto_tfm *tfm)
{
}

static struct crypto_alg algs[] = {
	{
		.cra_name = "ecb(fipsaes)",
		.cra_driver_name = "ecb-fipsaes",
		.cra_priority = 500,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_ctxsize = sizeof(struct kfips_transform_context),
		.cra_alignmask = 0,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = kfips_aes_cra_init,
		.cra_exit = kfips_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = 0,
			.setkey = kfips_aes_setkey,
			.encrypt = kfips_aes_ecb_encrypt,
			.decrypt = kfips_aes_ecb_decrypt,
		}
	}, {
		.cra_name = "cbc(fipsaes)",
		.cra_driver_name = "cbc-fipsaes",
		.cra_priority = 500,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_ctxsize = sizeof(struct kfips_transform_context),
		.cra_alignmask = 0,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = kfips_aes_cra_init,
		.cra_exit = kfips_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = AES_MIN_KEY_SIZE,
			.max_keysize = AES_MAX_KEY_SIZE,
			.ivsize = AES_BLOCK_SIZE,
			.setkey = kfips_aes_setkey,
			.encrypt = kfips_aes_cbc_encrypt,
			.decrypt = kfips_aes_cbc_decrypt,
		}
	}, {
		.cra_name = "xts(fipsaes)",
		.cra_driver_name = "xts-fipsaes",
		.cra_priority = 500,
		.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER | CRYPTO_ALG_ASYNC,
		.cra_blocksize = AES_BLOCK_SIZE,
		.cra_ctxsize = sizeof(struct kfips_transform_context),
		.cra_alignmask = 0,
		.cra_type = &crypto_ablkcipher_type,
		.cra_module = THIS_MODULE,
		.cra_init = kfips_aes_cra_init,
		.cra_exit = kfips_aes_cra_exit,
		.cra_u.ablkcipher = {
			.min_keysize = AES_MIN_KEY_SIZE * 2,
			.max_keysize = AES_MAX_KEY_SIZE * 2,
			.ivsize = AES_BLOCK_SIZE,
			.setkey = kfips_aes_setkey,
			.encrypt = kfips_aes_xts_encrypt,
			.decrypt = kfips_aes_xts_decrypt,
		}
	}
};

typedef enum {
	KFIPS_STATE_NOTLOADED,
	KFIPS_STATE_CRYPTO_REGISTERED,
	KFIPS_STATE_VMALLOC_USER_DONE,
	KFIPS_STATE_LOADED
} kfips_load_state;

static void kfips_aes_mod_unload(kfips_load_state mstate)
{
	int i;

	pr_alert("Unloading kfips from state %d\n", mstate);

	switch (mstate) {
	case KFIPS_STATE_LOADED:
		remove_proc_entry(KFIPS_PROC_NAME, NULL);
	case KFIPS_STATE_VMALLOC_USER_DONE:
		for (i = 0;
		     i < sizeof(kfips_queue_t); i += PAGE_SIZE) {
			ClearPageReserved(vmalloc_to_page
					  ((void
					    *) (((unsigned long) g_queue)
						+ i)));
		}
		vfree((void *)g_queue);
	case KFIPS_STATE_CRYPTO_REGISTERED:
		for (i = 0; i < sizeof algs / sizeof algs[0]; i++)
			crypto_unregister_alg(&algs[i]);
	case KFIPS_STATE_NOTLOADED:
		break;
	default:
		pr_err("Invalid module state!\n");
		break;
	}
}

static int __init kfips_aes_mod_init(void)
{
	int rc;
	int i;
	kfips_load_state mstate = KFIPS_STATE_NOTLOADED;
	size_t real_size;

	pr_err("Loading kfips\n");

	for (i = 0; i < sizeof algs / sizeof algs[0]; i++) {
		INIT_LIST_HEAD(&algs[i].cra_list);
		if ((rc = crypto_register_alg(&algs[i])) != 0) {
			pr_alert("Error registering %s \n", algs[i].cra_name);
			while (i > 0)
				crypto_unregister_alg(&algs[--i]);
			return rc;
		}
	}

	mstate = KFIPS_STATE_CRYPTO_REGISTERED;
	real_size = kfips_real_size(sizeof(kfips_queue_t));

	/* vmalloc_user will zero the memory */
	if ((g_queue = vmalloc_user(real_size)) == NULL) {
		pr_err("Error vmalloc %s \n", algs[0].cra_driver_name);
		kfips_aes_mod_unload(mstate);
		return -ENOMEM;
	}
	memset((void *)g_queue, 0, sizeof *g_queue);
	for (i = 0; i < real_size; i += PAGE_SIZE) {
		SetPageReserved(vmalloc_to_page
				((void *) (((unsigned long) g_queue) +
					   i)));
	}
	mstate = KFIPS_STATE_VMALLOC_USER_DONE;

	g_proc_entry = create_proc_entry(KFIPS_PROC_NAME,
					 S_IWUSR | S_IRUSR, NULL);
	if (!g_proc_entry) {
		pr_err("Unable to register proc entry %s\n",
		       KFIPS_PROC_NAME);
		kfips_aes_mod_unload(mstate);
		return -EINVAL;
	}
	if (uid >= 0)
		g_proc_entry->uid = uid;
	g_proc_entry->proc_fops = &file_fops;

	return 0;
}

static void __exit kfips_aes_mod_exit(void)
{
	kfips_aes_mod_unload(KFIPS_STATE_LOADED);
}

module_init(kfips_aes_mod_init);
module_exit(kfips_aes_mod_exit);

MODULE_DESCRIPTION("AuthenTec FIPS AES-XTS/AES-CBC Driver.");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_AUTHOR("AuthenTec, Inc.");

module_param(uid, int, 0);
MODULE_PARM_DESC(uid, "User id for the userland device access "
		 "(default root)");
