/*
 * max14688.c - MAX14688 - Headset Jack Dectection IC with Accessory Power
 *
 * Copyright (C) 2013 Maxim Integrated
 * Jayden Cha <jayden.cha@maximintegrated.com>
 *
 * Copyright and License statement to be determined with Customer.
 * GNU Public License version 2 requires software code to be
 * publically open source if the code is to be statically linked with
 * the Linux kernel binary object.
 */

//#define DEBUG
//#define VERBOSE_DEBUG

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <mach/board_lge.h>
#include <linux/switch.h>

/* for Device Tree */
#include <linux/io.h>
#include <linux/of.h>

#include <linux/delay.h>
#include <linux/input.h>
#include <linux/platform_data/hds_max14688.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#include <linux/qpnp/qpnp-adc.h>

#define DRIVER_DESC    "MAX14688 I2C Driver"
#define DRIVER_NAME    MAX14688_NAME
#define DRIVER_VERSION "1.01"
#define DRIVER_AUTHOR  "Jayden Cha <jayden.cha@maximintegrated.com>"

#define MAX14688_ADVANCED_JACK_DET 1

#undef  Z
#define Z(_minZ, _maxZ) \
        { .min_Z = _minZ, .max_Z = _maxZ }

#define JACK_MATCH(_name, _mic, _left, _has_btn, _mode0, _mode1, _switch_name, _switch_state, _evt_type, _evt_code1, _evt_code2) \
        {\
            .name           = _name,\
            .mic            = _mic,\
            .left           = _left,\
            .switch_name    = _switch_name,\
            .switch_state   = _switch_state,\
            .evt_type       = _evt_type,\
            .evt_code1      = _evt_code1,\
            .evt_code2      = _evt_code2,\
            .has_button     = _has_btn,\
            .mode0          = MAX14688_MODE_##_mode0,\
            .mode1          = MAX14688_MODE_##_mode1,\
        }

#define BUTTON_MATCH(_name, _mic, _left, _evt_type, _evt_code) \
        {\
            .name     = _name,\
            .mic      = _mic,\
            .left     = _left,\
            .evt_type = _evt_type,\
            .evt_code = _evt_code,\
        }

#undef  log_fmt
#define log_fmt(format) \
        DRIVER_NAME ": " format
#undef  log_err
#define log_err(format, ...) \
        printk(KERN_ERR log_fmt(format), ##__VA_ARGS__)
#undef  log_warn
#define log_warn(format, ...) \
        printk(KERN_WARNING log_fmt(format), ##__VA_ARGS__)
#undef  log_info
#define log_info(format, ...) \
        if (likely(max14688_log_level >= 0)) {\
            printk(KERN_INFO log_fmt(format), ##__VA_ARGS__);\
        }
#undef  log_dbg
#define log_dbg(format, ...) \
        if (likely(max14688_log_level >= 1)) {\
            printk(KERN_DEFAULT log_fmt(format), ##__VA_ARGS__);\
        }
#undef  log_vdbg
#define log_vdbg(format, ...) \
        if (likely(max14688_log_level >= 2)) {\
            printk(KERN_DEFAULT log_fmt(format), ##__VA_ARGS__);\
        }

#define MAX14688_OVERWRITE_JACK_INSERTION
#define MAX14688_WORK_DELAY                    0

/* Time constants [usec] */
#define MAX14688_JIG_POWER_DETECTION_DEBOUNCE   12000
#define MAX14688_MIC_SWITCH_TURN_ON_TIME            1
#define MAX14688_MIC_SWITCH_TURN_OFF_TIME           1
#define MAX14688_MIC_LOW_POWER_MODE_ON_TIME       120
#define MAX14688_MIC_LOW_POWER_MODE_PERIOD       8000
#define MAX14688_MODE_DETECTION_TIME            16000
#define MAX14688_DETIN_DEBOUNCE_TIME           300000
#define MAX14688_SEND_END_DEBOUNCE_TIME         30000
#define MAX14688_IDETIN_RISE_TIME               50000
#define MAX14688_IDETIN_FALL_TIME               50000
#define MAX14688_IDETIN_ON_TIME                100000
#define MAX14688_JIG_INJURY_TIME               100000
#define MAX14688_POLLMODE_INTERVAL \
        (MAX14688_DETIN_DEBOUNCE_TIME / 2)

#undef  __CONST_FFS
#define __CONST_FFS(_x) \
        ((_x) & 0x0F ? ((_x) & 0x03 ? ((_x) & 0x01 ? 0 : 1) :\
                                      ((_x) & 0x04 ? 2 : 0)) :\
        ((_x) & 0x30 ? ((_x) & 0x10 ? 4 : 5) :\
                       ((_x) & 0x40 ? 6 : 7)))

#undef  FFS
#define FFS(_x) \
        ((_x) ? (__builtin_constant_p(_x) ? __CONST_FFS(_x) : __ffs(_x)) : 0)

#undef  BIT_RSVD
#define BIT_RSVD  0

#undef  BITS
#define BITS(_end, _start) \
        ((BIT(_end) - BIT(_start)) + BIT(_end))

#undef  __BITS_GET
#define __BITS_GET(_word, _mask, _shift) \
        (((_word) & (_mask)) >> (_shift))

#undef  BITS_GET
#define BITS_GET(_word, _bit) \
        __BITS_GET(_word, _bit, FFS(_bit))

#undef  __BITS_SET
#define __BITS_SET(_word, _mask, _shift, _val) \
        (((_word) & ~(_mask)) | (((_val) << (_shift)) & (_mask)))

#undef  BITS_SET
#define BITS_SET(_word, _bit, _val) \
        __BITS_SET(_word, _bit, FFS(_bit), _val)

#undef  BITS_MATCH
#define BITS_MATCH(_word, _bit) \
        (((_word) & (_bit)) == (_bit))

/* Register map */
#define MAX14688_REG_ADDR_INVALID   0xFF
#define MAX14688_REG_ADDR_BASE      0x00
#define MAX14688_REG_ADDR(_offset)  (MAX14688_REG_ADDR_BASE + (_offset))

#define DEVICEID                    MAX14688_REG_ADDR(0x00)
#define DEVICEID_CHIPID             BITS(7,4)
#define DEVICEID_CHIPREV            BITS(3,0)
#define ADCCONVERSION               MAX14688_REG_ADDR(0x01)
#define ADCCONVERSION_ADCVAL        BITS(7,0)
#define ADCSTATUS                   MAX14688_REG_ADDR(0x02)
#define ADCSTATUS_EOC               BIT (0)
#define STATUS                      MAX14688_REG_ADDR(0x03)
#define STATUS_DET                  BIT (4)
#define STATUS_SWD                  BIT (3)
#define STATUS_INT                  BIT (2)
#define STATUS_MICIN                BIT (1)
#define STATUS_DETIN                BIT (0)
#define MISC1                       MAX14688_REG_ADDR(0x04)
#define MISC1_FUO                   BITS(7,0)
#define MISC2                       MAX14688_REG_ADDR(0x05)
#define MISC2_FUO                   BITS(7,0)
#define INTERRUPT                   MAX14688_REG_ADDR(0x06)
#define MASK                        MAX14688_REG_ADDR(0x07)
#define PINCONTROL1                 MAX14688_REG_ADDR(0x08)
#define PINCONTROL1_MANUALINT       BIT (7)
#define PINCONTROL1_MANUALMICSW     BIT (6)
#define PINCONTROL1_FORCEINT        BIT (5)
#define PINCONTROL1_FORCEMICSW      BIT (4)
#define PINCONTROL1_MODE1           BITS(3,2) /* Accessory Power control */
#define PINCONTROL1_MODE0           BITS(1,0) /* Microphone Bias control */
#define PINCONTROL2                 MAX14688_REG_ADDR(0x09)
#define PINCONTROL2_INTAUTO         BIT (2)
#define PINCONTROL2_MICOUTDELAY     BIT (0)   /* Microphone Bias Ouput Delay control */
#define ADCCONTROL                  MAX14688_REG_ADDR(0x0A)
#define ADCCONTROL_MANUALADC        BIT (5)
#define ADCCONTROL_FORCEADC         BIT (4)
#define ADCCONTROL_ADCCTL           BITS(1,0)
#define ADC_VAL_MAX                 BITS(5,0)

/* Interrupt corresponding bit */
#define IRQ_EOC                     BIT (5)
#define IRQ_DET                     BIT (4)
#define IRQ_SWD                     BIT (3)
#define IRQ_INT                     BIT (2)
#define IRQ_MICIN                   BIT (1)
#define IRQ_DETIN                   BIT (0)
#define IRQ_ALL                     BITS(5,0)

#undef  DONTCARE
#define DONTCARE  Z(INT_MAX, INT_MIN)
#undef  ANY
#define ANY       Z(      1, INT_MAX)
#undef  GROUNDED
#define GROUNDED  Z(INT_MIN,       0)
#define NONE      0

#define I2C_SUSPEND_WORKAROUND 1

#define SWITCH_NAME "h2w"
#define SWITCH_NAME_ADVANCED "h2w_advanced"
#define SWITCH_NAME_AUX "h2w_aux"

enum {
    NO_DEVICE   = 0,
    LGE_HEADSET = (1 << 0),
    LGE_HEADSET_NO_MIC = (1 << 1),
};

static struct max14688_jack_match max14688_jack_matches[] = {
    //         name      mic                  left         has     mode0  mode1  switch                switch              event  event
    //                   impedence            impedence    button                name                  state               type   code
    JACK_MATCH("3P",     Z(0,        50000),  Z(0,   4),   false,  LOW,   LOW,   SWITCH_NAME,          LGE_HEADSET_NO_MIC, EV_SW, SW_HEADPHONE_INSERT, NONE),
    JACK_MATCH("4P",     Z(120000, 2600000),  Z(0,   4),   true,   HIGH,  LOW,   SWITCH_NAME,          LGE_HEADSET,        EV_SW, SW_HEADPHONE_INSERT, SW_MICROPHONE_INSERT),
#if MAX14688_ADVANCED_JACK_DET
    JACK_MATCH("3P/ADV", Z(0,        50000),  Z(4,  11),   false,  LOW,   LOW,   SWITCH_NAME_ADVANCED, LGE_HEADSET_NO_MIC, EV_SW, SW_ADVANCED_HEADPHONE_INSERT, NONE),
    JACK_MATCH("4P/ADV", Z(120000, 2600000),  Z(4,  11),   true,   HIGH,  LOW,   SWITCH_NAME_ADVANCED, LGE_HEADSET,        EV_SW, SW_ADVANCED_HEADPHONE_INSERT, SW_MICROPHONE_INSERT),
    JACK_MATCH("ACC/AUX",Z(0,        50000),  Z(11, 64),   false,  LOW,   LOW,   SWITCH_NAME_AUX,      LGE_HEADSET_NO_MIC, EV_SW, SW_AUX_ACCESSORY_INSERT, NONE),
    JACK_MATCH("ACC/AUX",Z(120000, 2600000),  Z(11, 64),   true,   HIGH,  LOW,   SWITCH_NAME_AUX,      LGE_HEADSET,        EV_SW, SW_AUX_ACCESSORY_INSERT, SW_MICROPHONE_INSERT),
#else
#endif
};

static struct max14688_button_match max14688_button_matches[] = {
    //           name      mic                left         event   event
    //                     impedence          impedence    type    code
    BUTTON_MATCH("MEDIA",  Z(0,      150000), DONTCARE,    EV_KEY, KEY_MEDIA),
    BUTTON_MATCH("VOLUP",  Z(150000, 400000), DONTCARE,    EV_KEY, KEY_VOLUMEUP),
    BUTTON_MATCH("VOLDN",  Z(400000, 600000), DONTCARE,    EV_KEY, KEY_VOLUMEDOWN),
};

static int max14688_log_level = 1;

struct max14688 {
    struct switch_dev       sdev;
    struct mutex            lock;
    struct device          *dev;
    struct kobject         *kobj;
    struct attribute_group *attr_grp;
    struct input_dev       *input_dev;
    unsigned int            gpio_detect;
    unsigned int            gpio_int;
    unsigned int            gpio_mic_en;
    unsigned int            gpio_mic_en_2;
    u8                      status;
    int                     irq;
    u8                      irq_unmask;
    u8                      irq_saved;
    spinlock_t              irq_lock;
    struct delayed_work     irq_work;
    struct delayed_work     det_work;
#if I2C_SUSPEND_WORKAROUND
    struct                  delayed_work check_suspended_work;
    int                     suspended;
#endif
    int                     matched_jack;   /* invalid if negative */
    int                     matched_button; /* invalid if negative */

    /* from platform data */
    struct max14688_jack_match    *jack_matches;
    int                            num_of_jack_matches;
    struct max14688_button_match  *button_matches;
    int                            num_of_button_matches;
    bool (*detect_jack) (struct device *dev);
    int (*read_mic_impedence) (struct device *dev);
    int (*read_left_impedence) (struct device *dev);
    void (*report_jack) (struct device *dev,
    struct max14688_jack_match *match, int value);
    void (*report_button) (struct device *dev,
    struct max14688_button_match *match, int value);
};

#define __lock(_me)    mutex_lock(&(_me)->lock)
#define __unlock(_me)  mutex_unlock(&(_me)->lock)

#define __present_valid_jack(_me) \
        ((_me)->matched_jack >= 0 &&\
         (_me)->matched_jack < (_me)->num_of_jack_matches)
#define __current_jack(_me) \
        (&(_me)->jack_matches[(_me)->matched_jack])
#define __current_jack_name(_me) \
        (__current_jack(_me)->name)
#define __current_jack_has_button(_me) \
        (__current_jack(_me)->has_button)
#define __current_jack_mode0(_me) \
        (__current_jack(_me)->mode0)
#define __current_jack_mode1(_me) \
        (__current_jack(_me)->mode1)

#define __present_valid_button(_me) \
        ((_me)->matched_button >= 0 &&\
         (_me)->matched_button < (_me)->num_of_button_matches)
#define __current_button(_me) \
        (&(_me)->button_matches[(_me)->matched_button])
#define __current_button_name(_me) \
        (__current_button(_me)->name)

  #define __msleep(msec) msleep_interruptible((unsigned int)(msec))
//#define __msleep(msec) msleep((unsigned int)(msec))
//#define __msleep(msec) mdelay((unsigned int)(msec))

/* Reading from Sequential Registers */
static __inline int max14688_i2c_seq_read (struct max14688 *me,
    u8 addr, u8 *dst, u16 len)
{
    struct i2c_client *client = to_i2c_client(me->dev);
    struct i2c_adapter *adap = client->adapter;
    struct i2c_msg msg[2];
    int rc;

    msg[0].addr   = client->addr;
    msg[0].flags  = client->flags & I2C_M_TEN;
    msg[0].len    = 1;
    msg[0].buf    = (char*)(&addr);

    msg[1].addr   = client->addr;
    msg[1].flags  = client->flags & I2C_M_TEN;
    msg[1].flags |= I2C_M_RD;
    msg[1].len    = len;
    msg[1].buf    = (char*)dst;

    rc = i2c_transfer(adap, msg, 2);

    /* If everything went ok (i.e. 2 msg transmitted), return 0,
       else error code. */
    return (rc == 2) ? 0 : rc;
}

/* Writing to Sequential Registers */
static __inline int max14688_i2c_seq_write (struct max14688 *me,
    u8 addr, const u8 *src, u16 len)
{
    struct i2c_client *client = to_i2c_client(me->dev);
    struct i2c_adapter *adap = client->adapter;
    struct i2c_msg msg[1];
    u8 buf[len + 1];
    int rc;

    buf[0] = addr;
    memcpy(&buf[1], src, len);

    msg[0].addr  = client->addr;
    msg[0].flags = client->flags & I2C_M_TEN;
    msg[0].len   = len + 1;
    msg[0].buf   = (char*)buf;

    rc = i2c_transfer(adap, msg, 1);

    /* If everything went ok (i.e. 1 msg transmitted), return 0,
       else error code. */
    return (rc == 1) ? 0 : rc;
}

#define max14688_bulk_read(_me, _addr, _buf_ptr, _buf_len) \
        max14688_i2c_seq_read(_me, _addr, _buf_ptr, _buf_len)
#define max14688_bulk_write(_me, _addr, _buf_ptr, _buf_len) \
        max14688_i2c_seq_write(_me, _addr, _buf_ptr, _buf_len)
#define max14688_read(_me, _addr, _val_ptr) \
        max14688_bulk_read(_me, _addr, _val_ptr, 1)
#define max14688_write(_me, _addr, _val) \
        ({\
            u8 __buf = _val;\
            max14688_bulk_write(_me, _addr, &__buf, 1);\
        })

static __always_inline void max14688_enable_irq (struct max14688 *me,
    u8 irq_bits)
{
    int rc;
    u8 irq_current = 0;

    if (unlikely(me->irq < 0)) {
        return;
    }

    if (unlikely((me->irq_unmask & irq_bits) == irq_bits)) {
        /* already unmasked */
        return;
    }

    rc = max14688_read(me, INTERRUPT, &irq_current);
    if (unlikely(rc)) {
    log_err("INTERRUPT read error [%d]\n", rc);
    }

    log_dbg("%s[irq_current = %d]\n", __func__, irq_current);
    me->irq_saved |= irq_current;
    log_dbg("INTERRUPT CURR %02X SAVED %02X EN %02X\n", irq_current,
    me->irq_saved, me->irq_unmask);

    if (unlikely(!me->irq_unmask)) {
        enable_irq((unsigned int)me->irq);
        if (likely(device_may_wakeup(me->dev))) {
            enable_irq_wake(me->irq);
        }
    }

    log_dbg("%s[me->irq_unmask = %d]\n", __func__, me->irq_unmask);

    /* set enabled flag */
    me->irq_unmask |= irq_bits;

    rc = max14688_write(me, MASK, me->irq_unmask & IRQ_ALL);
    if (unlikely(rc)) {
        log_err("MASK write error [%d]n", rc);
    }
}

static __always_inline void max14688_disable_irq (struct max14688 *me,
    u8 irq_bits)
{
    int rc;

    if (unlikely(me->irq < 0)) {
        return;
    }

    if (unlikely((me->irq_unmask & irq_bits) == 0)) {
        /* already masked */
        return;
    }

    /* clear enabled flag */
    me->irq_unmask &= ~irq_bits;

    if (unlikely(!me->irq_unmask)) {
        if (likely(device_may_wakeup(me->dev))) {
            disable_irq_wake(me->irq);
        }
        disable_irq(me->irq);
    }

    rc = max14688_write(me, MASK, me->irq_unmask & IRQ_ALL);
    if (unlikely(rc)) {
        log_err("MASK write error [%d]n", rc);
    }
}

static int max14688_read_device_id (struct max14688 *me, u8 *id, u8 *rev)
{
    u8 deviceid = 0;
    int rc;

    rc = max14688_read(me, DEVICEID, &deviceid);
    if (unlikely(rc)) {
        log_err("DEVICEID read error [%d]\n", rc);
        goto out;
    }

    if (likely(id)) {
        *id = BITS_GET(deviceid, DEVICEID_CHIPID);
    }

    if (likely(rev)) {
        *rev = BITS_GET(deviceid, DEVICEID_CHIPREV);
    }

out:
    return rc;
}

static int max14688_update_status (struct max14688 *me)
{
    u8 status;
    int rc;

    status = 0;
    rc = max14688_read(me, STATUS, &status);
    if (unlikely(rc)) {
        log_err("STATUS read error [%d]\n", rc);
        goto out;
    }

    log_dbg("%s[status = %d]\n", __func__, status);

    me->status = status;

out:
    return rc;
}

#define max14688_get_status(_me, _bit) \
        BITS_GET((_me)->status, _bit)

static int max14688_read_mode0 (struct max14688 *me, int *mode0)
{
    u8 pincontrol1 = 0;
    int rc;

    rc = max14688_read(me, PINCONTROL1, &pincontrol1);
    if (unlikely(rc)) {
        log_err("DEVICEID read error [%d]\n", rc);
        goto out;
    }

    if (likely(mode0)) {
        *mode0 = BITS_GET(pincontrol1, PINCONTROL1_MODE0);
    }

out:
    return rc;
}

static int max14688_write_mode0 (struct max14688 *me, int mode0)
{
    u8 pincontrol1 = 0;
    int rc;

    rc = max14688_read(me, PINCONTROL1, &pincontrol1);
    if (unlikely(rc)) {
        log_err("PINCONTROL1 read error [%d]\n", rc);
        goto out;
    }

    pincontrol1 = BITS_SET(pincontrol1, PINCONTROL1_MODE0, (u8)mode0);

    log_dbg("%s[pincontrol1 = %d]\n", __func__, pincontrol1);
    rc = max14688_write(me, PINCONTROL1, pincontrol1);
    if (unlikely(rc)) {
        log_err("PINCONTROL1 write error [%d]\n", rc);
    }

out:
    return rc;
}

static int max14688_read_mode1 (struct max14688 *me, int *mode1)
{
    u8 pincontrol1 = 0;
    int rc;

    rc = max14688_read(me, PINCONTROL1, &pincontrol1);
    if (unlikely(rc)) {
        log_err("PINCONTROL1 read error [%d]\n", rc);
        goto out;
    }

    if (likely(mode1)) {
        *mode1 = BITS_GET(pincontrol1, PINCONTROL1_MODE1);
    }

out:
    return rc;
}

static int max14688_write_mode1 (struct max14688 *me, int mode1)
{
    u8 pincontrol1 = 0;
    int rc;

    rc = max14688_read(me, PINCONTROL1, &pincontrol1);
    if (unlikely(rc)) {
        log_err("PINCONTROL1 read error [%d]\n", rc);
        goto out;
    }

    pincontrol1 = BITS_SET(pincontrol1, PINCONTROL1_MODE1, (u8)mode1);

    log_dbg("%s[pincontrol1 = %d]\n", __func__, pincontrol1);
    rc = max14688_write(me, PINCONTROL1, pincontrol1);
    if (unlikely(rc)) {
        log_err("PINCONTROL1 write error [%d]\n", rc);
    }

out:
    return rc;
}

#define __is_key_valid(_key) \
        ((_key)->min_Z <= (_key)->max_Z)
#define __is_within_range_of_key(_key, _Z) \
        (((_key)->min_Z <= (_Z)) && ((_key)->max_Z > (_Z)))
#define __match_key(_key, _Z) \
        (!__is_key_valid(_key) || __is_within_range_of_key(_key, _Z))

static void max14688_lookup_jack (struct max14688 *me, int micZ, int leftZ)
{
    int i;

    for (i = 0; i < me->num_of_jack_matches; i++) {
        if (unlikely(__match_key(&me->jack_matches[i].mic,  micZ ) &&
                     __match_key(&me->jack_matches[i].left, leftZ))) {
            me->matched_jack = i;
            log_dbg("%s[me->matched_jack = %d\n", __func__, me->matched_jack);
            return;
        }
    }
    me->matched_jack = -1; /* not found */
}

static void max14688_lookup_button (struct max14688 *me, int micZ)
{
    int i;

    for (i = 0; i < me->num_of_button_matches; i++) {
        if (unlikely(__match_key(&me->button_matches[i].mic, micZ))) {
            me->matched_button = i;
            log_dbg("%s[me->matched_button = %d\n", __func__, me->matched_jack);
            return;
        }
    }

    me->matched_button = -1; /* not found */
}

#define JACK_IN_VALUE      MAX14688_JACK_IN_VALUE
#define JACK_OUT_VALUE     MAX14688_JACK_OUT_VALUE
#define BUTTON_DOWN_VALUE  MAX14688_BUTTON_DOWN_VALUE
#define BUTTON_UP_VALUE    MAX14688_BUTTON_UP_VALUE

static void max14688_det_work (struct work_struct *work)
{
    struct max14688 *me = container_of(work, struct max14688, det_work.work);
    int micZ, leftZ;

    __lock(me);

    if (unlikely(!me->detect_jack(me->dev))) {
        log_warn("no jack in detection work\n");
        goto out;
    }

    max14688_update_status(me);

    /* Read MIC and L-line impedences */
    micZ  = me->read_mic_impedence(me->dev);
    leftZ = me->read_left_impedence(me->dev);

    log_dbg("%s[micZ = %d, leftZ = %d\n", __func__, micZ, leftZ);

    /* Look up jack matching impedence ranges */
    max14688_lookup_jack(me, micZ, leftZ);
    
    if (unlikely(!__present_valid_jack(me))) {
        goto no_match_found;
    }

    max14688_write_mode0(me, __current_jack_mode0(me));
    max14688_write_mode1(me, __current_jack_mode1(me));

    if (__current_jack_has_button(me)) {
        max14688_enable_irq(me, IRQ_SWD);
    } else {
        max14688_disable_irq(me, IRQ_SWD);
    }

    log_dbg("jack %s inserted\n", __current_jack_name(me));
    me->report_jack(me->dev, __current_jack(me), JACK_IN_VALUE);
    goto out;

no_match_found:
    /* Handle exception */
    log_err("unknown jack - mic %d, left %d\n", micZ, leftZ);

out:
    __unlock(me);
    return;
}

static void max14688_irq_button_released (struct max14688 *me)
{
    if (unlikely(!__present_valid_button(me))) {
        goto out;
    }

    log_dbg("button %s released\n", __current_button_name(me));
    me->report_button(me->dev, __current_button(me), BUTTON_UP_VALUE);

    /* release current button */
    me->matched_button = -1;

out:
    return;
}

static void max14688_irq_button_pressed (struct max14688 *me)
{
    int micZ;

    /* Make sure no button down */
    max14688_irq_button_released(me);

    /* Read MIC impedences */
    micZ = me->read_mic_impedence(me->dev);

    /* Look up button matching impedence ranges */
    max14688_lookup_button(me, micZ);

    log_dbg("%s[micZ = %d]\n", __func__, micZ);

    if (unlikely(!__present_valid_button(me))) {
        log_warn("unknown button - mic %d\n", micZ);
        goto out;
    }

    log_dbg("button %s pressed\n", __current_button_name(me));
    me->report_button(me->dev, __current_button(me), BUTTON_DOWN_VALUE);

out:
    return;
}

static void max14688_irq_jack_removed (struct max14688 *me)
{
    cancel_delayed_work_sync(&me->det_work);

    max14688_disable_irq(me, IRQ_SWD);

    max14688_write_mode0(me, MAX14688_MODE_LOW);
    max14688_write_mode1(me, MAX14688_MODE_LOW);

    /* Make sure no button down */
    max14688_irq_button_released(me);

    if (unlikely(!__present_valid_jack(me))) {
        goto out;
    }

    log_dbg("jack %s removed\n", __current_jack_name(me));
    me->report_jack(me->dev, __current_jack(me), JACK_OUT_VALUE);

    /* release current jack */
    me->matched_jack = -1;

out:
    return;
}

static void max14688_irq_jack_inserted (struct max14688 *me)
{
    unsigned long det_work_delay =
        usecs_to_jiffies(MAX14688_IDETIN_RISE_TIME) +
        usecs_to_jiffies(MAX14688_IDETIN_FALL_TIME) +
        usecs_to_jiffies(MAX14688_IDETIN_ON_TIME) +
        usecs_to_jiffies(MAX14688_JIG_INJURY_TIME);

    /* Check INT bit and STATUS_INT bit for avoiding JIG mode */
    if (unlikely(max14688_get_status(me, STATUS_INT) && max14688_get_status(me, STATUS_MICIN))) {
        log_info("JIG power detected\n");

        max14688_disable_irq(me, IRQ_SWD);

        me->matched_jack   = -1;
        me->matched_button = -1;
        goto out;
    }

    if (unlikely(__present_valid_jack(me))) {
        log_warn("new jack detected during insertion of jack %s\n",
            __current_jack_name(me));
#ifdef MAX14688_OVERWRITE_JACK_INSERTION
        max14688_irq_jack_removed(me);
#else
        goto out;
#endif
    }

    if (unlikely(delayed_work_pending(&me->det_work))) {
        log_warn("detection in progress\n");
        goto out;
    }

    log_dbg("%s\n", __func__);
    max14688_write_mode0(me, MAX14688_MODE_HIGH);
    max14688_write_mode1(me, MAX14688_MODE_LOW);

    schedule_delayed_work(&me->det_work, det_work_delay);

out:
    return;
}

static void max14688_irq_work (struct work_struct *work)
{
    struct max14688 *me = container_of(work, struct max14688, irq_work.work);
    bool jack_detected, button_pressed;
    u8 irq_bits;

    log_dbg("%s\n", __func__);

    __lock(me);

    /* re-new status */
    max14688_update_status(me);

    spin_lock(&me->irq_lock);
    log_dbg("%s[me->irq_saved = %d, me->irq_unmask = %d\n", __func__, me->irq_saved, me->irq_unmask);
    irq_bits = me->irq_saved & me->irq_unmask;
    me->irq_saved = 0;
    spin_unlock(&me->irq_lock);

    jack_detected  = me->detect_jack(me->dev);
    button_pressed = (max14688_get_status(me, STATUS_SWD) != 0);

    log_dbg("%s[jack_detected = %d]\n", __func__, jack_detected);
    if (likely(irq_bits & IRQ_DET)) {
        /* jack insert/remove irq */
        if (jack_detected) {
            max14688_irq_jack_inserted(me);
        } else {
            max14688_irq_jack_removed(me);
        }
    }

    if (unlikely(!jack_detected)) {
        goto out;
    }

    if (likely(irq_bits & IRQ_SWD)) {
        /* button press/release irq */
        if (button_pressed) {
            max14688_irq_button_pressed(me);
        } else {
            max14688_irq_button_released(me);
        }
    }

out:
    if (unlikely(me->irq < 0)) {
        schedule_delayed_work(&me->irq_work, usecs_to_jiffies(MAX14688_POLLMODE_INTERVAL));
    }
    __unlock(me);
    return;
}

static irqreturn_t max14688_isr (int irq, void *data)
{
    struct max14688 *me = (struct max14688 *) data;
    u8 irq_current = 0;
    int rc;

    log_dbg("%s\n", __func__);

    rc = max14688_read(me, INTERRUPT, &irq_current);
    if (unlikely(rc)) {
        log_err("INTERRUPT read error [%d]\n", rc);
        goto out;
    }

    me->irq_saved |= irq_current;
    log_dbg("INTERRUPT CURR %02X SAVED %02X EN %02X\n", irq_current,
        me->irq_saved, me->irq_unmask);

    if (likely(!delayed_work_pending(&me->irq_work))) {
#if I2C_SUSPEND_WORKAROUND
        schedule_delayed_work(&me->check_suspended_work, MAX14688_WORK_DELAY);
#else
        schedule_delayed_work(&me->irq_work, MAX14688_WORK_DELAY);
#endif
    }

out:
    return IRQ_HANDLED;
}

static ssize_t max14688_log_level_show (struct device *dev,
    struct device_attribute *devattr, char *buf)
{
    struct max14688 *me = dev_get_drvdata(dev);
    int rc;

    __lock(me);

    rc = (int)snprintf(buf, PAGE_SIZE, "%d\n", max14688_log_level);

    __unlock(me);
    return (ssize_t)rc;
}

static ssize_t max14688_log_level_store (struct device *dev,
    struct device_attribute *devattr, const char *buf, size_t count)
{
    struct max14688 *me = dev_get_drvdata(dev);

    __lock(me);

    max14688_log_level = (int)simple_strtol(buf, NULL, 10);

    __unlock(me);
    return (ssize_t)count;
}

static DEVICE_ATTR(log_level, S_IRUGO|S_IWUSR, max14688_log_level_show,
    max14688_log_level_store);

static ssize_t max14688_device_id_show (struct device *dev,
    struct device_attribute *devattr, char *buf)
{
    struct max14688 *me = dev_get_drvdata(dev);
    u8 chip_id, chip_rev;
    int rc;

    __lock(me);

    chip_id  = 0;
    chip_rev = 0;

    rc = max14688_read_device_id(me, &chip_id, &chip_rev);
    if (unlikely(rc)) {
        goto out;
    }

    rc = (int)snprintf(buf, PAGE_SIZE, "ID %02Xh Rev. %02Xh\n",
        chip_id, chip_rev);

out:
    __unlock(me);
    return (ssize_t)rc;
}

static DEVICE_ATTR(device_id, S_IRUGO, max14688_device_id_show, NULL);

static ssize_t max14688_status_show (struct device *dev,
    struct device_attribute *devattr, char *buf)
{
    struct max14688 *me = dev_get_drvdata(dev);
    u8 status = 0;
    int rc;

    __lock(me);

    rc = max14688_read(me, STATUS, &status);
    if (unlikely(rc)) {
        log_err("STATUS read error [%d]\n", rc);
        goto out;
    }

    rc = (int)snprintf(buf, PAGE_SIZE, "%02Xh\n", status);

out:
    __unlock(me);
    return (ssize_t)rc;
}

static ssize_t max14688_status_store (struct device *dev,
    struct device_attribute *devattr, const char *buf, size_t count)
{
    struct max14688 *me = dev_get_drvdata(dev);

    __lock(me);

    max14688_update_status(me);

    __unlock(me);
    return (ssize_t)count;
}

static DEVICE_ATTR(status, S_IRUGO|S_IWUSR, max14688_status_show,
    max14688_status_store);

static ssize_t max14688_adc_result_show (struct device *dev,
    struct device_attribute *devattr, char *buf)
{
    struct max14688 *me = dev_get_drvdata(dev);
    u8 adcconversion = 0;
    int rc;

    __lock(me);

    rc = max14688_read(me, ADCCONVERSION, &adcconversion);
    if (unlikely(rc)) {
        log_err("ADCCONVERSION read error [%d]\n", rc);
        goto out;
    }

    rc = (int)snprintf(buf, PAGE_SIZE, "%u\n", adcconversion);

out:
    __unlock(me);
    return (ssize_t)rc;
}

static DEVICE_ATTR(adc_result, S_IRUGO, max14688_adc_result_show, NULL);

static ssize_t max14688_adc_refresh_store (struct device *dev,
    struct device_attribute *devattr, const char *buf, size_t count)
{
    struct max14688 *me = dev_get_drvdata(dev);
    unsigned long timeout;
    unsigned int delay;
    u8 adcstatus, adccontrol_save, adccontrol;
    int rc;

    __lock(me);

    adccontrol = 0;
    rc = max14688_read(me, ADCCONTROL, &adccontrol);
    if (unlikely(rc)) {
        log_err("ADCCONTROL read error [%d]\n", rc);
        goto out;
    }

    /* Save original ADCCONTROL value */
    adccontrol_save = adccontrol;

    delay = (unsigned int)simple_strtoul(buf, NULL, 10);
    if (likely(delay > 0)) {
        __msleep(delay);
    }

    adccontrol |= ADCCONTROL_FORCEADC;
    adccontrol |= ADCCONTROL_MANUALADC;

    rc = max14688_write(me, ADCCONTROL, adccontrol);
    if (unlikely(rc)) {
        log_err("ADCCONTROL write error [%d]\n", rc);
        goto out;
    }

    timeout = jiffies +
        usecs_to_jiffies(MAX14688_IDETIN_RISE_TIME) +
        usecs_to_jiffies(MAX14688_IDETIN_FALL_TIME) +
        usecs_to_jiffies(MAX14688_IDETIN_ON_TIME);

    do {
        if (unlikely(time_after(jiffies, timeout))) {
            log_err("AD conversion timed out\n");
            goto out;
        }

        __msleep(1);

        adcstatus = 0;
        max14688_read(me, ADCSTATUS, &adcstatus);

    } while (likely(!BITS_GET(adcstatus, ADCSTATUS_EOC)));

    /* Restore ADCCONTROL value and make sure FORCEADC bit cleared */
    adccontrol_save &= ~ADCCONTROL_FORCEADC;
    rc = max14688_write(me, ADCCONTROL, adccontrol_save);
    if (unlikely(rc)) {
        log_err("ADCCONTROL write error [%d]\n", rc);
        goto out;
    }

out:
    __unlock(me);
    return (ssize_t)count;
}

static DEVICE_ATTR(adc_refresh, S_IWUSR, NULL, max14688_adc_refresh_store);

static ssize_t max14688_monitor_show (struct device *dev,
    struct device_attribute *devattr, char *buf)
{
    struct max14688 *me = dev_get_drvdata(dev);
    unsigned long flags;
    u8 irq_unmask, irq_saved, reg_val;
    int rc = 0;

    __lock(me);

    spin_lock_irqsave(&me->irq_lock, flags);
    irq_unmask = me->irq_unmask;
    irq_saved  = me->irq_saved;
    spin_unlock_irqrestore(&me->irq_lock, flags);

    /**************************************************************************/
    reg_val = 0;
    max14688_read(me, ADCCONVERSION, &reg_val);

    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "Latest ADC conversion   %u (code %02Xh)\n", reg_val, reg_val);

    /**************************************************************************/
    reg_val = 0;
    max14688_read(me, ADCSTATUS, &reg_val);

    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "ADCSTATUS register      %02Xh\n", reg_val);

    /**************************************************************************/
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "Saved Status            %02Xh\n", me->status);
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  Headset is %s\n",
        BITS_GET(me->status, STATUS_DET)? "not detected" : "detected");
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  Swtich is %s\n",
        BITS_GET(me->status, STATUS_SWD)? "pressed" : "not pressed");
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  INT output is %s\n",
        BITS_GET(me->status, STATUS_INT)? "driven high" : "not driven");
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  MICIN switch is %s\n",
        BITS_GET(me->status, STATUS_MICIN)? "closed" : "open");
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  DETIN is %s\n",
        BITS_GET(me->status, STATUS_DETIN)? "detected" : "not detected");

    /**************************************************************************/
    reg_val = 0;
    max14688_read(me, MASK, &reg_val);

    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "MASK register           %02Xh\n", reg_val);
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  EOC %u DET %u SWD %u INT %u MICIN %u DETIN %u\n",
        !!(reg_val & IRQ_EOC),
        !!(reg_val & IRQ_DET),
        !!(reg_val & IRQ_SWD),
        !!(reg_val & IRQ_INT),
        !!(reg_val & IRQ_MICIN),
        !!(reg_val & IRQ_DETIN));

    /**************************************************************************/
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "Saved interrupt flags   %02Xh\n", irq_saved);
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  EOC %u DET %u SWD %u INT %u MICIN %u DETIN %u\n",
        !!(irq_saved & IRQ_EOC),
        !!(irq_saved & IRQ_DET),
        !!(irq_saved & IRQ_SWD),
        !!(irq_saved & IRQ_INT),
        !!(irq_saved & IRQ_MICIN),
        !!(irq_saved & IRQ_DETIN));

    /**************************************************************************/
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "Enabled interrupt flags %02Xh\n", irq_unmask);
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  EOC %u DET %u SWD %u INT %u MICIN %u DETIN %u\n",
        !!(irq_unmask & IRQ_EOC),
        !!(irq_unmask & IRQ_DET),
        !!(irq_unmask & IRQ_SWD),
        !!(irq_unmask & IRQ_INT),
        !!(irq_unmask & IRQ_MICIN),
        !!(irq_unmask & IRQ_DETIN));

    /**************************************************************************/
    reg_val = 0;
    max14688_read(me, PINCONTROL1, &reg_val);

    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "Pin control 1           %02Xh\n", reg_val);
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  INT follows %s %s\n",
        BITS_GET(reg_val, PINCONTROL1_MANUALINT)?
            "Force INT bit" : "the chip-internal decision",
        BITS_GET(reg_val, PINCONTROL1_MANUALINT)?
            BITS_GET(reg_val, PINCONTROL1_FORCEINT)?
                "INT is low" : "INT is high" :
            "");
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  MIC_SW follows %s %s\n",
        BITS_GET(reg_val, PINCONTROL1_MANUALMICSW)?
            "Force MIC_SW bit" : "the chip-internal decision",
        BITS_GET(reg_val, PINCONTROL1_MANUALINT)?
            BITS_GET(reg_val, PINCONTROL1_FORCEINT)?
                "MIC_SW is open" : "MIC_SW is closed" :
            "");
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  MODE1 (Accessory Power) is %s\n",
        BITS_GET(reg_val, PINCONTROL1_MODE1) == 0b00? "low" :
        BITS_GET(reg_val, PINCONTROL1_MODE1) == 0b11? "high" :
        "in high Z");
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  MODE0 (Microphone Bias) is %s\n",
        BITS_GET(reg_val, PINCONTROL1_MODE0) == 0b00? "low" :
        BITS_GET(reg_val, PINCONTROL1_MODE0) == 0b11? "high" :
        "in high Z");

    reg_val = 0;
    max14688_read(me, PINCONTROL2, &reg_val);

    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "Pin control 2           %02Xh\n", reg_val);
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  INT %s\n",
        BITS_GET(reg_val, PINCONTROL2_INTAUTO)?
            "is forced high during Z detection regardless of MODE0/1" :
            "follows the chip-internal decision");
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  MIC bias output %s\n",
        BITS_GET(reg_val, PINCONTROL2_MICOUTDELAY)?
            "delayed until Z detection after DET going low" :
            "follows the MODE0/1 after DET going low");

    /**************************************************************************/
    reg_val = 0;
    max14688_read(me, ADCCONTROL, &reg_val);

    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "ADC control             %02Xh\n", reg_val);
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  Manual control of ADC is %s\n",
        (u8)BITS_GET(reg_val, ADCCONTROL_MANUALADC)? "on" : "off");
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  Force ADC start bit is %u\n",
        (u8)BITS_GET(reg_val, ADCCONTROL_FORCEADC));
    rc += (int)snprintf(buf+rc, PAGE_SIZE,
        "  ADC control mode is %u\n",
        (u8)BITS_GET(reg_val, ADCCONTROL_ADCCTL));

    __unlock(me);
    return (ssize_t)rc;
}

static DEVICE_ATTR(monitor, S_IRUGO, max14688_monitor_show, NULL);

#define DEFINE_REGBITS_DEV_ATTR(_name) \
static ssize_t max14688_##_name##_show (struct device *dev,\
    struct device_attribute *devattr, char *buf)\
{\
    struct max14688 *me = dev_get_drvdata(dev);\
    int val = 0, rc;\
    __lock(me);\
    rc = max14688_read_##_name(me, &val);\
    if (unlikely(rc)) {\
        goto out;\
    }\
    rc = (int)snprintf(buf, PAGE_SIZE, "%d\n", val);\
out:\
    __unlock(me);\
    return (ssize_t)rc;\
}\
static ssize_t max14688_##_name##_store (struct device *dev,\
    struct device_attribute *devattr, const char *buf, size_t count)\
{\
    struct max14688 *me = dev_get_drvdata(dev);\
    int val, rc;\
    __lock(me);\
    val = (int)simple_strtol(buf, NULL, 16);\
    rc = max14688_write_##_name(me, val);\
    if (unlikely(rc)) {\
        goto out;\
    }\
out:\
    __unlock(me);\
    return (ssize_t)count;\
}\
static DEVICE_ATTR(regbits_##_name, S_IRUGO|S_IWUSR, max14688_##_name##_show,\
    max14688_##_name##_store)

DEFINE_REGBITS_DEV_ATTR(mode0);
DEFINE_REGBITS_DEV_ATTR(mode1);

static struct attribute* max14688_attr[] = {
    &dev_attr_log_level.attr,
    &dev_attr_device_id.attr,
    &dev_attr_status.attr,
    &dev_attr_adc_result.attr,
    &dev_attr_adc_refresh.attr,
    &dev_attr_monitor.attr,

    #define REGBITS_DEV_ATTR(_name) \
            (&dev_attr_regbits_##_name.attr)
    REGBITS_DEV_ATTR(mode0),
    REGBITS_DEV_ATTR(mode1),

    NULL,
};

static struct attribute_group max14688_attr_group = {
    .attrs = max14688_attr,
};

static __always_inline void max14688_destroy (struct max14688 *me)
{
    if (likely(me->irq >= 0)) {
        devm_free_irq(me->dev, me->irq, me);
    }

    me->sdev.name = SWITCH_NAME;
    switch_dev_unregister(&me->sdev);

    me->sdev.name = SWITCH_NAME_ADVANCED;
    switch_dev_unregister(&me->sdev);

    me->sdev.name = SWITCH_NAME_AUX;
    switch_dev_unregister(&me->sdev);

    cancel_delayed_work_sync(&me->irq_work);
    cancel_delayed_work_sync(&me->det_work);

    if (likely(me->attr_grp)) {
        sysfs_remove_group(me->kobj, me->attr_grp);
    }

    if (likely(me->input_dev)) {
        input_unregister_device(me->input_dev);
        input_free_device(me->input_dev);
    }

    mutex_destroy(&me->lock);

    devm_kfree(me->dev, me);
}

static bool max14688_detect_jack (struct device *dev)
{
    struct max14688 *me = dev_get_drvdata(dev);
    int rc;

    rc = max14688_update_status(me);
    if (unlikely(rc)) {
        return false;
    }

    return (max14688_get_status(me, STATUS_DET) == 0);
}

static int max14688_read_mic_impedence (struct device *dev)
{
    struct qpnp_vadc_result result;
    int acc_read_value = 0;

    int rc;

    /*                                                    */
    rc = qpnp_vadc_read_lge(P_MUX6_1_1,&result);
    if (rc < 0) {
	   if (rc == -ETIMEDOUT) {
		   pr_err("[DEBUG] button_pressed : adc read timeout \n");
	   } else {
		   pr_err("button_pressed: adc read error - %d\n", rc);
	   }
    }

    acc_read_value = (int)result.physical;
    log_dbg("%s[adc value =  %d]\n", __func__, acc_read_value);
   
    return acc_read_value;
}

static int max14688_read_left_impedence (struct device *dev)
{
    struct max14688 *me = dev_get_drvdata(dev);
    u8 adcconversion = 0;
    u8 adcstatus = 0;

    int rc;

    rc = max14688_read(me, ADCCONVERSION, &adcconversion);
    if (unlikely(rc)) {
        log_err("ADCCONVERSION read error [%d]\n", rc);
        goto out;
    }

    log_dbg("%s[adc value = %d]\n", __func__, adcconversion);

    /* Greater than 2.69k(ohm) resistor is connected, read the EOC bit in the ADCSTATUS address */
    max14688_read(me, ADCSTATUS, &adcstatus);
    if(!(BITS_GET(adcstatus, ADCSTATUS_EOC))) {
        log_dbg("%s[ADC_VAL_MAX = %d]\n", __func__, (int)ADC_VAL_MAX);
        rc = (int)ADC_VAL_MAX;
    } else {
        rc = (int)adcconversion;
    }
out:
    return rc;
}

static void max14688_report_jack (struct device *dev,
    struct max14688_jack_match *match, int value)
{
    struct max14688 *me = dev_get_drvdata(dev);
    struct input_dev *input_dev = me->input_dev;

    if (match->evt_type < EV_MAX && value) {
        input_event(input_dev, match->evt_type, match->evt_code1, value);

        me->sdev.name = match->switch_name;

        switch_set_state(&me->sdev, match->switch_state);

        if (likely(match->evt_code2)){
              input_event(input_dev, match->evt_type, match->evt_code2, value);
        }
        input_sync(input_dev);
    } else if (match->evt_type < EV_MAX && !value) {
        input_event(input_dev, match->evt_type, match->evt_code1, value);

        me->sdev.name = match->switch_name;
        switch_set_state(&me->sdev, NO_DEVICE);

        if (likely(match->evt_code2)){
              input_event(input_dev, match->evt_type, match->evt_code2, value);
        }
        input_sync(input_dev);
    }
}
static void max14688_report_button (struct device *dev,
    struct max14688_button_match *match, int value)
{
    struct max14688 *me = dev_get_drvdata(dev);
    struct input_dev *input_dev = me->input_dev;

    if (likely(match->evt_type < EV_MAX)) {
        input_event(input_dev, match->evt_type, match->evt_code, value);
        input_sync(input_dev);
    }
}

static const struct i2c_device_id max14688_i2c_ids[] = {
    { DRIVER_NAME, 0 },
    { /* end of array */ }
};
MODULE_DEVICE_TABLE(i2c, max14688_i2c_ids);

#ifdef CONFIG_OF
static const struct of_device_id max14688_device_ids[] = {
    { .compatible = "maxim,"DRIVER_NAME, },
    { /* end of array */ }
};
MODULE_DEVICE_TABLE(of, max14688_device_ids);
#endif /* CONFIG_OF */

#if I2C_SUSPEND_WORKAROUND
static void max14688_check_suspended_worker(struct work_struct *work)
{
    struct max14688 *me = container_of(work, struct max14688, check_suspended_work.work);

    log_vdbg("%s : me->suspended - %d\n", __func__, me->suspended);

    if (me->suspended)
    {
        log_vdbg("max14688 suspended. try i2c operation after 100ms.\n");
        schedule_delayed_work(&me->check_suspended_work, msecs_to_jiffies(100));
    }
    else
    {
        log_vdbg("max14688 resume. do max14688_irq.\n");
        schedule_delayed_work(&me->irq_work, 0);
    }
}
#endif

static void max14688_parse_dt(struct device *dev, struct max14688_platform_data *pdata)
{
    struct device_node *np = dev->of_node;

    pdata->gpio_detect = of_get_named_gpio_flags(np, "max14688,gpio_detect", 0, NULL);
    pdata->gpio_int = of_get_named_gpio_flags(np, "max14688,gpio_int", 0, NULL);
    
    if (lge_get_board_revno() <= HW_REV_A){
    pdata->gpio_mic_en = of_get_named_gpio_flags(np, "max14688,gpio_mic_en", 0, NULL);
    pdata->gpio_mic_en_2 = of_get_named_gpio_flags(np, "max14688,gpio_mic_en_2", 0, NULL);
    }
    pdata->switch_name           = SWITCH_NAME;
    pdata->switch_name_advanced  = SWITCH_NAME_ADVANCED;
    pdata->switch_name_aux       = SWITCH_NAME_AUX;
    pdata->jack_matches          = max14688_jack_matches;
    pdata->num_of_jack_matches   = ARRAY_SIZE(max14688_jack_matches);
    pdata->button_matches        = max14688_button_matches;
    pdata->num_of_button_matches = ARRAY_SIZE(max14688_button_matches);
    pdata->detect_jack           = max14688_detect_jack;
    pdata->read_mic_impedence    = max14688_read_mic_impedence;
    pdata->read_left_impedence   = max14688_read_left_impedence;
    pdata->report_jack           = max14688_report_jack;
    pdata->report_button         = max14688_report_button;
}

static __devinit int max14688_probe (struct i2c_client *client,
    const struct i2c_device_id *id)
{
    struct max14688_platform_data* pdata = client->dev.platform_data;
    struct max14688 *me;
    u8 chip_id, chip_rev;
    int i, rc;
    u8 pincontrol2 = 0;

    log_dbg(MAX14688_NAME" attached\n");

    me = kzalloc(sizeof(struct max14688), GFP_KERNEL);

    if ( me == NULL) {
	     log_err("Failed to allloate headset per device info\n");
            return -ENOMEM;
    }
 
    if(client->dev.of_node){
	     pdata = devm_kzalloc(&client->dev, sizeof(struct max14688_platform_data), GFP_KERNEL);
		if (unlikely(!pdata)) {
			log_err("out of memory (%uB requested)\n", sizeof(struct max14688_platform_data));
			return -ENOMEM;
		}
		client->dev.platform_data = pdata;

		max14688_parse_dt(&client->dev, pdata);
    } else {
		pdata = devm_kzalloc(&client->dev, sizeof(struct max14688_platform_data), GFP_KERNEL);
		if (unlikely(!pdata)) {
			log_err("out of memory (%uB requested)\n", sizeof(struct max14688_platform_data));
			return -ENOMEM;
              }
		else
			pdata = client->dev.platform_data;
    }
 
    i2c_set_clientdata(client, me);

    spin_lock_init(&me->irq_lock);
    mutex_init(&me->lock);
    me->dev  = &client->dev;
    me->kobj = &client->dev.kobj;
    me->irq  = -1;

    me->gpio_int = pdata->gpio_int;
    me->gpio_detect = pdata->gpio_detect;

    INIT_DELAYED_WORK(&me->irq_work, max14688_irq_work);
    INIT_DELAYED_WORK(&me->det_work, max14688_det_work);
#if I2C_SUSPEND_WORKAROUND
    INIT_DELAYED_WORK(&me->check_suspended_work, max14688_check_suspended_worker);
#endif

    rc = gpio_request(me->gpio_detect, MAX14688_NAME"-detect");
    if (unlikely(rc)) {
        return rc;
    }

    rc = gpio_direction_input(me->gpio_detect);
    if (rc < 0) {
	 log_err("Failed to configure gpio%d (me->gpio_detect) gpio_direction_input\n", me->gpio_detect);
	 gpio_free(me->gpio_detect);
	 return rc;
    }
    if (lge_get_board_revno() <= HW_REV_A){

        me->gpio_mic_en = pdata->gpio_mic_en;
        me->gpio_mic_en_2 = pdata->gpio_mic_en_2;

        rc = gpio_request(me->gpio_mic_en, MAX14688_NAME"-mic_en");
        if (unlikely(rc)) {
            log_err("Failed to configure gpio%d (me->gpio_mic_en) request\n", me->gpio_mic_en);
            return rc;
        }

        rc = gpio_direction_input(me->gpio_mic_en);
        if (rc < 0) {
            log_err("Failed to configure gpio%d (me->gpio_mic_en) gpio_direction_input\n", me->gpio_mic_en);
		    gpio_free(me->gpio_mic_en);
            return rc;
        }

        rc = gpio_request(me->gpio_mic_en_2, MAX14688_NAME"-mic_en_2");
        if (unlikely(rc)) {
            log_err("Failed to configure gpio%d (me->gpio_mic_en) request\n", me->gpio_mic_en);
            return rc;
        }
		
        rc = gpio_direction_input(me->gpio_mic_en_2);
        if (rc < 0) {
            log_err("Failed to configure gpio%d (me->gpio_mic_en_2) gpio_direction_input\n", me->gpio_mic_en_2);
            gpio_free(me->gpio_mic_en_2);
            return rc;
        }
    }
    rc = gpio_request(me->gpio_int, MAX14688_NAME"-irq");
    if (unlikely(rc)) {
		return rc;
    }
		
    rc = gpio_direction_input(me->gpio_int);
    if (rc < 0) {
    log_err("Failed to configure gpio%d (me->gpio_int) gpio_direction_input\n", me->gpio_int);
    gpio_free(me->gpio_int);
    return rc;
    }
	
    me->irq = gpio_to_irq(me->gpio_int);

    /* Save jack lookup table given via platform data */
    me->jack_matches        = pdata->jack_matches;
    me->num_of_jack_matches = pdata->num_of_jack_matches;

    /* Save button lookup table given via platform data */
    me->button_matches        = pdata->button_matches;
    me->num_of_button_matches = pdata->num_of_button_matches;

    me->matched_jack   = -1;
    me->matched_button = -1;
	
    /* Platform-specific Calls */
    me->detect_jack = pdata->detect_jack;
    me->read_mic_impedence = pdata->read_mic_impedence;
    me->read_left_impedence = pdata->read_left_impedence;
    me->report_jack = pdata->report_jack;
    me->report_button = pdata->report_button;

    /* Disable & Clear all interrupts */
    max14688_write(me, MASK, 0x00);
    max14688_read(me, INTERRUPT, &me->irq_saved);

    /* INT AUTO disable(INT follows the state diagram and flow chart) */
    max14688_read(me, PINCONTROL2, &pincontrol2);
    max14688_write(me, PINCONTROL2, ~PINCONTROL2_INTAUTO & pincontrol2);
    max14688_read(me, PINCONTROL2, &pincontrol2);

    log_dbg("%s[pincontrol2 = %d]\n", __func__, pincontrol2);

    /* Default MODE setting */
    max14688_write_mode0(me, MAX14688_MODE_LOW);
    max14688_write_mode1(me, MAX14688_MODE_LOW);

    me->irq_saved = 0;
    me->irq_unmask = 0;

    log_dbg("%s[me->irq_saved = %d]\n", __func__, me->irq_saved);

    /* Register input_dev */
    me->input_dev = input_allocate_device();
    if (unlikely(!me->input_dev)) {
        log_err("failed to allocate memory for new input device\n");
        rc = -ENOMEM;
        goto abort;
    }
    /* initialize switch device */
    me->sdev.name             = pdata->switch_name;

    rc = switch_dev_register(&me->sdev);
	
    if (rc < 0) {
	 log_err("Failed to register switch device\n");
	 switch_dev_unregister(&me->sdev);
        goto abort;
    }

    me->sdev.name             = pdata->switch_name_advanced;

    rc = switch_dev_register(&me->sdev);

    if (rc < 0) {
	 log_err("Failed to register switch device\n");
	 switch_dev_unregister(&me->sdev);
        goto abort;
    }

    me->sdev.name             = pdata->switch_name_aux;

    rc = switch_dev_register(&me->sdev);

    if (rc < 0) {
	 log_err("Failed to register switch device\n");
	 switch_dev_unregister(&me->sdev);
        goto abort;
    }

    me->input_dev->name       = DRIVER_NAME;
    me->input_dev->phys       = DRIVER_NAME"/input0";
    me->input_dev->dev.parent = me->dev;

    for (i = 0; i < me->num_of_jack_matches; i++) {
        if (likely(me->jack_matches[i].evt_type < EV_MAX)) {
            input_set_capability(me->input_dev,
                me->jack_matches[i].evt_type, me->jack_matches[i].evt_code1);
        if (likely(me->jack_matches[i].evt_code2))
            input_set_capability(me->input_dev,
                me->jack_matches[i].evt_type, me->jack_matches[i].evt_code2);
        }
    }

    for (i = 0; i < me->num_of_button_matches; i++) {
        if (likely(me->button_matches[i].evt_type < EV_MAX)) {
            input_set_capability(me->input_dev,
                me->button_matches[i].evt_type, me->button_matches[i].evt_code);
        }
    }

    rc = input_register_device(me->input_dev);
    if (unlikely(rc)) {
        log_err("failed to register input device [%d]\n", rc);
        input_free_device(me->input_dev);
        me->input_dev = NULL;
        goto abort;
    }

    /* Create max14688 sysfs attributes */
    me->attr_grp = &max14688_attr_group;
    rc = sysfs_create_group(me->kobj, me->attr_grp);
    if (unlikely(rc)) {
        log_err("failed to create attribute group [%d]\n", rc);
        me->attr_grp = NULL;
        goto abort;
    }

    /* Get MAX14688 IRQ */
    if (unlikely(me->irq < 0)) {
        log_warn( "interrupt disabled\n");
    } else {
        /* Request system IRQ for MAX14688 */
        rc = request_threaded_irq(me->irq, NULL, max14688_isr,
            IRQF_ONESHOT | IRQF_TRIGGER_FALLING, DRIVER_NAME, me);
        if (unlikely(rc < 0)) {
            log_err("failed to request IRQ(%u) [%d]\n", me->irq, rc);
            me->irq = -1;
            goto abort;
        }
        disable_irq((unsigned int)me->irq);
    }

    max14688_enable_irq(me, IRQ_DET);

    /* Complete initialization */

    pr_info(DRIVER_DESC" "DRIVER_VERSION" Installed\n");

    chip_id  = 0;
    chip_rev = 0;
    max14688_read_device_id(me, &chip_id, &chip_rev);
    log_info("chip id %02X rev %02X\n", chip_id, chip_rev);

    if (me->detect_jack(me->dev)) {
        max14688_irq_jack_inserted(me);
    }

    return 0;

abort:
    i2c_set_clientdata(client, NULL);
    max14688_destroy(me);
    return rc;
}

static __devexit int max14688_remove (struct i2c_client *client)
{
    struct max14688 *me = i2c_get_clientdata(client);

    i2c_set_clientdata(client, NULL);
    max14688_destroy(me);

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max14688_suspend (struct device *dev)
{
    struct max14688 *me = dev_get_drvdata(dev);

    log_vdbg("%s\n", __func__);
    __lock(me);

    if (likely(!device_may_wakeup(dev))) {
        max14688_disable_irq(me, IRQ_ALL);
        cancel_delayed_work_sync(&me->irq_work);
        cancel_delayed_work_sync(&me->det_work);
    }

#if I2C_SUSPEND_WORKAROUND
    me->suspended = 1;
#endif
    __unlock(me);
    return 0;
}

static int max14688_resume (struct device *dev)
{
    struct max14688 *me = dev_get_drvdata(dev);

    log_vdbg("%s\n", __func__);
    __lock(me);

    max14688_enable_irq(me, IRQ_DET);
    schedule_delayed_work(&me->irq_work, MAX14688_WORK_DELAY);

#if I2C_SUSPEND_WORKAROUND
    me->suspended = 0;
#endif
    __unlock(me);
    return 0;
}
#endif /* CONFIG_PM_SLEEP */

//static SIMPLE_DEV_PM_OPS(max14688_pm, max14688_suspend, max14688_resume);

const struct dev_pm_ops max14688_pm = {
    .suspend               = max14688_suspend,
    .resume                = max14688_resume,
};

static struct i2c_driver max14688_i2c_driver = {
    .driver.name           = DRIVER_NAME,
    .driver.owner          = THIS_MODULE,
    .driver.pm             = &max14688_pm,
#ifdef CONFIG_OF
    .driver.of_match_table = of_match_ptr(max14688_device_ids),
#endif /* CONFIG_OF */
    .probe                 = max14688_probe,
    .remove                = __devexit_p(max14688_remove),
    .id_table              = max14688_i2c_ids,
};

module_i2c_driver(max14688_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_VERSION(DRIVER_VERSION);

/******************************************************************************/

struct device *max14688_device (struct input_dev *input_dev)
{
    return input_dev->dev.parent;
}
EXPORT_SYMBOL(max14688_device);

struct input_dev *max14688_input_device (struct device *dev)
{
    struct max14688 *me = dev_get_drvdata(dev);

    return me->input_dev;
}
EXPORT_SYMBOL(max14688_input_device);

