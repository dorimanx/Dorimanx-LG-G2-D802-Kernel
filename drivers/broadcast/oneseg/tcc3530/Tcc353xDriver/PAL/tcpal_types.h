#ifndef __TCPAL_TYPES_H__
#define __TCPAL_TYPES_H__

#ifdef __cplusplus
extern    "C"
{
#endif

#ifdef __cplusplus
#ifdef NULL
#undef NULL
#endif
#define NULL 0
#define TCBB_FUNC extern "C"
#else
#ifdef NULL
#undef NULL
#endif
#define NULL (void*)0
#define TCBB_FUNC
#endif

typedef unsigned char I08U;	/* 1 byte */
typedef signed char I08S;	/* 1 byte */
typedef unsigned short I16U;	/* 2 bytes */
typedef signed short I16S;	/* 2 bytes */
typedef unsigned long I32U;	/* 4 bytes */
typedef signed long I32S;	/* 4 bytes */
typedef signed long long I64S;	/* 8 bytes */
typedef unsigned long long I64U;	/* 8 bytes */

typedef I64U TcpalTime_t;
typedef I32U TcpalSemaphore_t;
typedef I32U TcpalHandle_t;

#ifdef __cplusplus
	};
#endif

#endif
