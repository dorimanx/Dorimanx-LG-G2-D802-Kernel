/*
 * MAX14688 - Headset Jack Dectection IC with Accessory Power
 *
 * Copyright (C) 2013 Maxim Integrated
 * Jayden Cha <jayden.cha@maximintegrated.com>
 *
 * Copyright and License statement to be determined with Customer.
 * GNU Public License version 2 requires software code to be
 * publically open source if the code is to be statically linked with
 * the Linux kernel binary object.
 */

#ifndef __MAX14688_H__
#define __MAX14688_H__

#define MAX14688_NAME                    "max14688"
#define MAX14688_I2C_ADDR                0x15

/* Mode Control */
#define MAX14688_MODE_LOW                0b00
#define MAX14688_MODE_HIGH_Z             0b01
//      MAX14688_MODE_HIGH_Z             0b10
#define MAX14688_MODE_HIGH               0b11

/* Switch events */
#define SW_ADVANCED_HEADPHONE_INSERT	0x11
#define SW_AUX_ACCESSORY_INSERT		0x12

/* Lookup Keys */
struct max14688_match_key {
    /* impedence range */
    int min_Z, max_Z; /* make (min > max) to be invalid, ignored */
};

/* Jack Detection Table (for DET interrupt) */
struct max14688_jack_match {
    char                      *name;

    /* look-up keys */
    struct max14688_match_key  mic;              /* MIC impedence range */
    struct max14688_match_key  left;             /* L-line impedence range */

    /* output: reporting switch state events */
    char                       *switch_name;
    unsigned int               switch_state;

    /* output: reporting input events */
    unsigned int               evt_type, evt_code1, evt_code2;

    /* device settings */
    bool                       has_button;       /* set to enable SWD */
    u8                         mode0, mode1;     /* mode outputs */
};

/* Button Detection Table (for SWD interrupt) */
struct max14688_button_match {
    char                      *name;

    /* look-up keys */
    struct max14688_match_key  mic;              /* MIC impedence range */
    struct max14688_match_key  left;             /* L-line impedence range */

    /* output: reporting input events */
    unsigned int               evt_type, evt_code;
};

/* ADCCTL Selections
 *   0b00  Z detection & ADC always off
 *   0b01  Z detection forced on as long as "01" is kept and
 *         ADC enabled continuously
 *   0b10  A single ADC is executed in the middle of the flat current
 *         being 100ms long
 *   0b11  A single ADC is executed in the middle of the flat current
 *         being 200ms long
 */
#define MAX14688_ADCCTL_OFF              0b00
#define MAX14688_ADCCTL_CONTINUOUS       0b01
#define MAX14688_ADCCTL_SINGLE_OF_100MS  0b10
#define MAX14688_ADCCTL_SINGLE_OF_200MS  0b11

/* Value Definition for Reporting Input Event */
#define MAX14688_JACK_IN_VALUE           1
#define MAX14688_JACK_OUT_VALUE          0
#define MAX14688_BUTTON_DOWN_VALUE       1
#define MAX14688_BUTTON_UP_VALUE         0

/* Platform Data */
struct max14688_platform_data {
    const char             *switch_name;
    const char             *switch_name_advanced;
    const char             *switch_name_aux;
    unsigned int           gpio_detect;
    unsigned int           gpio_int;
    unsigned int           gpio_mic_en;
    unsigned int           gpio_mic_en_2;

    /* Look-up tables */
    struct max14688_jack_match    *jack_matches;
    int                            num_of_jack_matches;
    struct max14688_button_match  *button_matches;
    int                            num_of_button_matches;

    /* Replace-able input functions */
    bool (*detect_jack) (struct device *dev);
    int (*read_mic_impedence) (struct device *dev);
    int (*read_left_impedence) (struct device *dev);

    /* Replace-able output functions */
    void (*report_jack) (struct device *dev,
        struct max14688_jack_match *match, int value);
    void (*report_button) (struct device *dev,
        struct max14688_button_match *match, int value);
};

/***
 *** External Services
 ***/

struct input_dev;
extern struct device *max14688_device (struct input_dev *input_dev);

extern struct input_dev *max14688_input_device (struct device *dev);

#endif /* !__MAX14688_H__ */
