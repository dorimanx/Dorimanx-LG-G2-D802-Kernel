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

#ifndef _CRYPTO_KFIPS_H
#define _CRYPTO_KFIPS_H

#define KFIPS_PROC_NAME "driver/kfips"
#define KFIPS_PROC_PATH "/proc/" KFIPS_PROC_NAME

#define KFIPS_BUFFER_SIZE 4096

#define KFIPS_AES_MAX_KEY_SIZE 32
#define KFIPS_AES_BLOCK_SIZE 16

#ifdef __KERNEL__
#include <crypto/aes.h>
#include <linux/device-mapper.h>
#if KFIPS_AES_MAX_KEY_SIZE != AES_MAX_KEY_SIZE
#error bad KFIPS_AES_MAX_KEY_SIZE
#endif
#if KFIPS_AES_BLOCK_SIZE != AES_BLOCK_SIZE
#error bad KFIPS_AES_BLOCK_SIZE
#endif
#endif

#if defined(__KERNEL__)
#define KFIPS_WRITE_BARRIER() wmb()
#define KFIPS_READ_BARRIER() rmb()
#elif defined(__arm__)
#define KFIPS_WRITE_BARRIER() asm volatile("dsb" : : : "memory")
#define KFIPS_READ_BARRIER() asm volatile("dsb" : : : "memory")
#elif defined(__i386__) || defined(__amd64__)
#define KFIPS_WRITE_BARRIER() asm volatile("" : : : "memory")
#define KFIPS_READ_BARRIER() asm volatile("" : : : "memory")
#else
#error unsupported CPU architecture
#endif

/* individual operation structure */
typedef struct {
	/* kernel-userland synchronization fields */
	uint32_t request_valid;
	uint32_t response_valid;
	/* two keys for XTS, one for ECB and CBC */
	uint8_t key[2 * KFIPS_AES_MAX_KEY_SIZE];
	/* initialization vector */
	uint8_t iv[KFIPS_AES_BLOCK_SIZE];
	/* data to encrypt/decrypt */
	uint8_t buf[KFIPS_BUFFER_SIZE];
	/* length of data */
	int len;
	/* operation flags */
	uint32_t flags;
#define KFIPS_FLAGS_DECRYPT 0x01
#define KFIPS_FLAGS_ENCRYPT 0x02
#define KFIPS_FLAGS_ECB 0x04
#define KFIPS_FLAGS_CBC 0x08
#define KFIPS_FLAGS_XTS 0x10
	/* key length in bytes */
	uint32_t keylen;
	/* context pointer */
	void *pointer;
} kfips_msg_t;

/* operation ring. */
#define KFIPS_RING_INDEX_BITS 3
#define KFIPS_RING_ENTRIES (1 << KFIPS_RING_INDEX_BITS)
#define KFIPS_RING_INDEX_MASK (KFIPS_RING_ENTRIES - 1)
typedef struct {
	/* operation buffers */
	kfips_msg_t msg[1 << KFIPS_RING_INDEX_BITS];
	/* request insert index */
	int reqins;
	/* response remove index */
	int rsprem;
	/* request processing index */
	int next;
} kfips_queue_t;

#ifdef __KERNEL__
static inline kfips_msg_t *kfips_queue_insreq_begin(kfips_queue_t *q)
{
	kfips_msg_t *m;

	m = &q->msg[q->reqins];
	if (m->request_valid)
		return NULL;
	KFIPS_READ_BARRIER();
	if (m->response_valid)
		return NULL;
	KFIPS_READ_BARRIER();
	return m;
}

static inline void kfips_queue_insreq_end(kfips_queue_t *q,
					  kfips_msg_t *m)
{
	KFIPS_WRITE_BARRIER();
	m->request_valid = 1;
	q->reqins++;
	q->reqins &= KFIPS_RING_INDEX_MASK;
}

static inline kfips_msg_t *kfips_queue_remrsp_begin(kfips_queue_t *q)
{
	kfips_msg_t *m;

	m = &q->msg[q->rsprem];
	if (!m->response_valid)
		return NULL;
	KFIPS_READ_BARRIER();
	return m;
}

static inline void kfips_queue_remrsp_end(kfips_queue_t *q,
					  kfips_msg_t *m)
{
	KFIPS_WRITE_BARRIER();
	m->response_valid = 0;
	q->rsprem++;
	q->rsprem &= KFIPS_RING_INDEX_MASK;
}
#else
static inline kfips_msg_t *kfips_queue_getreq(kfips_queue_t *q)
{
	kfips_msg_t *m;

	m = &q->msg[q->next];
	if (!m->request_valid)
		return NULL;
	KFIPS_READ_BARRIER();
	return m;
}

static inline void kfips_queue_putrsp(kfips_queue_t *q, kfips_msg_t *m)
{
	KFIPS_WRITE_BARRIER();
	m->response_valid = 1;
	KFIPS_WRITE_BARRIER();
	m->request_valid = 0;
	q->next++;
	q->next &= KFIPS_RING_INDEX_MASK;
}
#endif

#endif /* _CRYPTO_KFIPS_H */
