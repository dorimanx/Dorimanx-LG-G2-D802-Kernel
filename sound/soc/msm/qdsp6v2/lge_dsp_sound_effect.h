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


#define APPI_LGE_SOUNDEFFECT_MODULE_ID              0x10009010
#define APPI_LGE_SOUNDEFFECT_ENABLES                0x10009011
#define APPI_LGE_SOUNDEFFECT_EFFECTMODE_PARAMS      0x10009012
#define APPI_LGE_SOUNDEFFECT_OUTPUTDEVICE_PARAMS    0x10009013
#define APPI_LGE_SOUNDEFFECT_MEDITYPE_PARAMS        0x10009014
#define APPI_LGE_SOUNDEFFECT_GEQ_PARAMS             0x10009015


struct asm_lgesoundeffect_param_enable {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  enable_flag;
} __packed;


struct asm_lgesoundeffect_param_modetype {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  ModeType;
} __packed;

struct asm_lgesoundeffect_param_outputdevicetype {  
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  OutputDeviceType;
} __packed;

struct asm_lgesoundeffect_param_mediatype {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  MediaType;
} __packed;

struct asm_lgesoundeffect_param_geq {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  BandNum;
	int32_t                  BandGain;
} __packed;



int q6asm_set_lgesoundeffect_enable(struct audio_client *ac, int enable);
int q6asm_set_lgesoundeffect_modetype(struct audio_client *ac, int modetype);
int q6asm_set_lgesoundeffect_outputdevicetype(struct audio_client *ac, int outputdevicetype);
int q6asm_set_lgesoundeffect_mediatype(struct audio_client *ac, int mediatype);
int q6asm_set_lgesoundeffect_geq(struct audio_client *ac, int geq_band, int geq_gain);




