#ifndef __MEDIA_INFO_H__
#define __MEDIA_INFO_H__

#ifndef MSM_MEDIA_ALIGN
#define MSM_MEDIA_ALIGN(__sz, __align) (((__sz) + (__align-1)) & (~(__align-1)))
#endif

enum color_fmts {
	/* Venus NV12:
	 * YUV 4:2:0 image with a plane of 8 bit Y samples followed
	 * by an interleaved U/V plane containing 8 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  ^           ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  Height      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |          Y_Scanlines
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  V           |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              V
	 * U V U V U V U V U V U V X X X X  ^
	 * U V U V U V U V U V U V X X X X  |
	 * U V U V U V U V U V U V X X X X  |
	 * U V U V U V U V U V U V X X X X  UV_Scanlines
	 * X X X X X X X X X X X X X X X X  |
	 * X X X X X X X X X X X X X X X X  V
	 * X X X X X X X X X X X X X X X X  --> Buffer size alignment
	 *
	 * Y_Stride : Width aligned to 128
	 * UV_Stride : Width aligned to 128
	 * Y_Scanlines: Height aligned to 32
	 * UV_Scanlines: Height/2 aligned to 16
	 * Total size = align((Y_Stride * Y_Scanlines
	 *          + UV_Stride * UV_Scanlines + 4096), 4096)
	 */
	COLOR_FMT_NV12,

	/* Venus NV21:
	 * YUV 4:2:0 image with a plane of 8 bit Y samples followed
	 * by an interleaved V/U plane containing 8 bit 2x2 subsampled
	 * colour difference samples.
	 *
	 * <-------- Y/UV_Stride -------->
	 * <------- Width ------->
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  ^           ^
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  Height      |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |          Y_Scanlines
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  |           |
	 * Y Y Y Y Y Y Y Y Y Y Y Y X X X X  V           |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              |
	 * X X X X X X X X X X X X X X X X              V
	 * V U V U V U V U V U V U X X X X  ^
	 * V U V U V U V U V U V U X X X X  |
	 * V U V U V U V U V U V U X X X X  |
	 * V U V U V U V U V U V U X X X X  UV_Scanlines
	 * X X X X X X X X X X X X X X X X  |
	 * X X X X X X X X X X X X X X X X  V
	 * X X X X X X X X X X X X X X X X  --> Padding & Buffer size alignment
	 *
	 * Y_Stride : Width aligned to 128
	 * UV_Stride : Width aligned to 128
	 * Y_Scanlines: Height aligned to 32
	 * UV_Scanlines: Height/2 aligned to 16
	 * Total size = align((Y_Stride * Y_Scanlines
	 *          + UV_Stride * UV_Scanlines + 4096), 4096)
	 */
	COLOR_FMT_NV21,
};

#define VENUS_Y_STRIDE(__color_fmt, __width) ({\
	unsigned int __alignment, __stride = 0; \
	if (__width) { \
		switch (__color_fmt) { \
		case COLOR_FMT_NV12: \
		__alignment = 128; \
		__stride = MSM_MEDIA_ALIGN(__width, __alignment);\
		break;\
		default:\
		break;\
		}\
	} \
	__stride;\
})

#define VENUS_UV_STRIDE(__color_fmt, __width) ({\
	unsigned int __alignment, __stride = 0; \
	if (__width) {\
		switch (__color_fmt) { \
		case COLOR_FMT_NV12: \
		__alignment = 128; \
		__stride = MSM_MEDIA_ALIGN(__width, __alignment); \
		break; \
		default: \
		break; \
		} \
	} \
	__stride; \
})

#define VENUS_Y_SCANLINES(__color_fmt, __height) ({ \
	unsigned int __alignment, __sclines = 0; \
	if (__height) {\
		switch (__color_fmt) { \
		case COLOR_FMT_NV12: \
		__alignment = 32; \
		__sclines = MSM_MEDIA_ALIGN(__height, __alignment); \
		break; \
		default: \
		break; \
		} \
	} \
	__sclines; \
})

#define VENUS_UV_SCANLINES(__color_fmt, __height) ({\
	unsigned int __alignment, __sclines = 0; \
	if (__height) {\
		switch (__color_fmt) { \
		case COLOR_FMT_NV12: \
			__alignment = 16; \
			__sclines = MSM_MEDIA_ALIGN(((__height + 1) >> 1), __alignment); \
			break; \
		default: \
			break; \
		} \
	} \
	__sclines; \
})

#define VENUS_BUFFER_SIZE( \
	__color_fmt, __width, __height) ({ \
	unsigned int __uv_alignment; \
	unsigned int __size = 0; \
	unsigned int __y_plane, __uv_plane, __y_stride, \
		__uv_stride, __y_sclines, __uv_sclines; \
	if (__width && __height) {\
		__y_stride = VENUS_Y_STRIDE(__color_fmt, __width); \
		__uv_stride = VENUS_UV_STRIDE(__color_fmt, __width); \
		__y_sclines = VENUS_Y_SCANLINES(__color_fmt, __height); \
		__uv_sclines = VENUS_UV_SCANLINES(__color_fmt, __height); \
		switch (__color_fmt) { \
		case COLOR_FMT_NV12: \
			__uv_alignment = 4096; \
			__y_plane = __y_stride * __y_sclines; \
			__uv_plane = __uv_stride * __uv_sclines + __uv_alignment; \
			__size = __y_plane + __uv_plane; \
			__size = MSM_MEDIA_ALIGN(__size, 4096); \
			break; \
		default: \
			break; \
		} \
	} \
	__size; \
})

#endif
