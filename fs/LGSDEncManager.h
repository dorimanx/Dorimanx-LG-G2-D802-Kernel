//FEATURE_SDCARD_MEDIAEXN_SYSTEMCALL_ENCRYPTION

#include <keys/user-type.h>
#include <keys/encrypted-type.h>
#include <linux/fs.h>
#include <linux/fs_stack.h>
#include <linux/namei.h>
#include <linux/scatterlist.h>
#include <linux/hash.h>
#include <linux/nsproxy.h>
#include <linux/backing-dev.h>
#include <linux/ecryptfs.h>

#define FEATURE_SDCARD_MEDIAEXN_SYSTEMCALL_ENCRYPTION
#define MAX_MEDIA_EXT_LENGTH 330

extern int getMediaProperty(void);
extern int ecryptfs_mediaFileSearch(const unsigned char *filename);
extern char *ecryptfs_Extfilename(const unsigned char *filename);

