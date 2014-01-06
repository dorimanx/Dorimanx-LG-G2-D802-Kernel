/*
   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011 Synaptics, Inc.

   Permission is hereby granted, free of charge, to any person obtaining a copy of
   this software and associated documentation files (the "Software"), to deal in
   the Software without restriction, including without limitation the rights to use,
   copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
   Software, and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.

   +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/
#include <linux/i2c.h>
#include <linux/delay.h>	//msleep
#include <linux/file.h>		//for file access
#include <linux/syscalls.h> //for file access
#include <linux/uaccess.h>  //for file access


extern struct i2c_client* ds4_i2c_client;
extern int touch_i2c_read(struct i2c_client *client, u8 reg, int len, u8 *buf);
extern int touch_i2c_write(struct i2c_client *client, u8 reg, int len, u8 * buf);

void Read8BitRegisters(unsigned short regAddr, unsigned char *data, int length)
{
	// I2C read
	if(touch_i2c_read(ds4_i2c_client, regAddr, length, data) < 0) return;
}

void Write8BitRegisters(unsigned short regAddr, unsigned char *data, int length)
{
	// I2C write
	if(touch_i2c_write(ds4_i2c_client, regAddr, length, data) < 0) return;
}

void delayMS(int val)
{
	// Wait for val MS
	msleep(val);
}

void write_log(char *data)
{
	extern int f54_window_crack;
	extern int f54_window_crack_check_mode;

	int fd;
	char *fname = "/mnt/sdcard/synaptics_f54_log.txt";
	struct stat fstat;

	if(f54_window_crack||f54_window_crack_check_mode==0) {
		mm_segment_t old_fs = get_fs();
		set_fs(KERNEL_DS);

		fd = sys_open(fname, O_WRONLY|O_CREAT|O_APPEND, 0644);

		if(fd >= 0) {
			if(sys_newstat((char __user *) fname, (struct stat *)&fstat) < 0) {
				printk("[Touch] cannot read %s stat info\n", fname);
			} else {
				if(fstat.st_size > 5 * 1024 * 1024) {
					printk("[Touch] delete %s\n", fname);
					sys_unlink(fname);
					sys_close(fd);

					fd = sys_open(fname, O_WRONLY|O_CREAT|O_APPEND, 0644);
					if(fd >= 0) {
						sys_write(fd, data, strlen(data));
					}
				} else {
					sys_write(fd, data, strlen(data));
				}
				sys_close(fd);
			}
		}
		set_fs(old_fs);
	}
}

#if 0
void main(void)
/* Please be informed this main() function & related functions are an example for host implementation */
{
	PowerOnSensor();
	delayMS(400);

	F54Test();
	PowerOffSensor();
}
#endif
