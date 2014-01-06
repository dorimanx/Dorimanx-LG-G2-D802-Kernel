/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fci_types.h
 
 Description : 
 
 History : 
 ----------------------------------------------------------------------
 2009/08/31 	jason		initial
*******************************************************************************/

#ifndef __FCI_TYPES_H__
#define __FCI_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HANDLE
typedef void 		*HANDLE;
#endif

typedef char fci_s8;
typedef short int fci_s16;
typedef int fci_s32;
typedef unsigned char fci_u8;
typedef unsigned short fci_u16;
typedef unsigned int fci_u32;


#ifndef TRUE
#define TRUE		1
#endif

#ifndef FALSE
#define	FALSE		0
#endif

#ifndef NULL
#define NULL		0
#endif

#define BBM_OK		0
#define BBM_NOK 	1

#define BBM_E_FAIL              0x00000001
#define BBM_E_HOSTIF_SELECT     0x00000002
#define BBM_E_HOSTIF_INIT       0x00000003
#define BBM_E_BB_REG_WRITE      0x00000100
#define BBM_E_BB_REG_READ       0x00000101
#define BBM_E_TN_REG_WRITE      0x00000200
#define BBM_E_TN_REG_READ       0x00000201
#define BBM_E_TN_CTRL_SELECT    0x00000202
#define BBM_E_TN_CTRL_INIT      0x00000203
#define BBM_E_TN_SELECT         0x00000204
#define BBM_E_TN_INIT           0x00000205
#define BBM_E_TN_RSSI           0x00000206
#define BBM_E_TN_SET_FREQ       0x00000207
#define BBM_E_MUX_SYNC          0x00010000
#define BBM_E_MUX_DATA_MASK     0x00010001
#define BBM_E_MUX_SUBCHANNEL    0x00010002
#define BBM_E_MUX_INDICATOR     0x00010003

#ifdef __cplusplus
}
#endif

#endif /* __FCI_TYPES_H__ */
