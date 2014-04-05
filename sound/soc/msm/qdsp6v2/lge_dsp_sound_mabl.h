/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#define APPI_LGE_SOUND_MABL_MODULE_ID            0x1000b010
#define APPI_LGE_SOUND_MABL_DEVICE_SPEAKER       0x1000b011
#define APPI_LGE_SOUND_MABL_MONO_AUDIO_ENABLE    0x1000b012
#define APPI_LGE_SOUND_MABL_LR_BALANCE_CONTROL   0x1000b013


struct asm_lgesoundmabl_param_devicespeaker {
    struct apr_hdr  hdr;
    struct asm_stream_cmd_set_pp_params_v2 param;
    struct asm_stream_param_data_v2 data;
    int32_t                  DeviceSpeaker;
} __packed;

struct asm_lgesoundmabl_param_monoenable {
    struct apr_hdr  hdr;
    struct asm_stream_cmd_set_pp_params_v2 param;
    struct asm_stream_param_data_v2 data;
    int32_t                  MonoEnable;
} __packed;

struct asm_lgesoundmabl_param_lrbalancecontrol {
    struct apr_hdr  hdr;
    struct asm_stream_cmd_set_pp_params_v2 param;
    struct asm_stream_param_data_v2 data;
    int32_t                  LrBalanceControl;
} __packed;



int q6asm_set_lgesoundmabl_devicespeaker(struct audio_client *ac, int devicespeaker);
int q6asm_set_lgesoundmabl_monoenable(struct audio_client *ac, int monoenable);
int q6asm_set_lgesoundmabl_lrbalancecontrol(struct audio_client *ac, int lrbalancecontrol);
