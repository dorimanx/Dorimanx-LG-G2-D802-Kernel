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

#define APPI_LGE_SOUND_NORMALIZER_MODULE_ID            0x1000a010
#define APPI_LGE_SOUND_NORMALIZER_DEVICE_SPEAKER       0x1000a011
#define APPI_LGE_SOUND_NORMALIZER_ENABLES              0x1000a012
#define APPI_LGE_SOUND_NORMALIZER_MAKEUP_GAIN          0x1000a013
#define APPI_LGE_SOUND_NORMALIZER_PREFILTER            0x1000a014
#define APPI_LGE_SOUND_NORMALIZER_LIMITER_THRESHOLD    0x1000a015
#define APPI_LGE_SOUND_NORMALIZER_LIMITER_SLOPE        0x1000a016
#define APPI_LGE_SOUND_NORMALIZER_COMPRESSOR_THRESHOLD 0x1000a017
#define APPI_LGE_SOUND_NORMALIZER_COMPRESSOR_SLOPE     0x1000a018
#define APPI_LGE_SOUND_NORMALIZER_ON_OFF               0x1000a019


struct asm_lgesoundnormalizer_param_devicespeaker {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  DeviceSpeaker;
} __packed;

struct asm_lgesoundnormalizer_param_enable {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  enable_flag;
} __packed;

struct asm_lgesoundnormalizer_param_makeupgain {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  MakeupGain;
} __packed;

struct asm_lgesoundnormalizer_param_prefilter {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  PreFilter;
} __packed;

struct asm_lgesoundnormalizer_param_limiterthreshold {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  LimiterThreshold;
} __packed;
struct asm_lgesoundnormalizer_param_limiterslope {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  LimiterSlope;
} __packed;
struct asm_lgesoundnormalizer_param_compressorthreshold {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  CompressorThreshold;
} __packed;
struct asm_lgesoundnormalizer_param_compressorslope {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  CompressorSlope;
} __packed;
struct asm_lgesoundnormalizer_param_onoff {
	struct apr_hdr  hdr;
	struct asm_stream_cmd_set_pp_params_v2 param;
	struct asm_stream_param_data_v2 data;
	int32_t                  OnOff;
} __packed;



int q6asm_set_lgesoundnormalizer_enable(struct audio_client *ac, int enable);
int q6asm_set_lgesoundnormalizer_devicespeaker(struct audio_client *ac, int devicespeaker);
int q6asm_set_lgesoundnormalizer_makeupgain(struct audio_client *ac, int makeupgain);
int q6asm_set_lgesoundnormalizer_prefilter(struct audio_client *ac, int prefilter);
int q6asm_set_lgesoundnormalizer_limiterthreshold(struct audio_client *ac, int limiterthreshold);
int q6asm_set_lgesoundnormalizer_limiterslope(struct audio_client *ac, int limiterslope);
int q6asm_set_lgesoundnormalizer_compressorthreshold(struct audio_client *ac, int compressorthreshold);
int q6asm_set_lgesoundnormalizer_compressorslope(struct audio_client *ac, int compressorslope);
int q6asm_set_lgesoundnormalizer_onoff(struct audio_client *ac, int onoff);
