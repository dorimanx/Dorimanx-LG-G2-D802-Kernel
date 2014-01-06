/*                                                                            
                                        
 
                                        
 
           
                                                                       
                                          
                                                                              */
#ifndef	__TUNERBB_DRV_FC8050_H__
#define	__TUNERBB_DRV_FC8050_H__
/*
** Include Header File
*/

#include "../../broadcast_tdmb_typedef.h"
#include "../../broadcast_tdmb_drv_ifdef.h"

/*============================================================
**    1.   DEFINITIONS
*============================================================*/

#define TDMB_UPLOAD_MODE_SPI

#if defined(TDMB_UPLOAD_MODE_TSIF)
//#define STREAM_TS_UPLOAD
#elif defined(TDMB_UPLOAD_MODE_EBI)
//#define STREAM_SLAVE_PARALLEL_UPLOAD
#elif defined(TDMB_UPLOAD_MODE_SPI)
#define STREAM_SPI_UPLOAD
#endif

	//#define STREAM_MASTER_SERIAL_UPLOAD
	//#define STREAM_MASTER_PARALLEL_UPLOAD
	//#define STREAM_SLAVE_SERIAL_UPLOAD
	//#define STREAM_SPI_UPLOAD

#if defined(STREAM_TS_UPLOAD)
#define TSIF_EN_ACTIVE_HIGH 
#endif
	
#if defined(STREAM_SLAVE_PARALLEL_UPLOAD)
#define	DMB_IRQ_TYPE 	       				GPIO_INT_36  /* DMB_IRQ_N */
#define 	DMB_RESET_TYPE					GPIO_INT_39  /* DMB_RESET_N  */
#define	DMB_EN_TYPE					GPIO_INT_41  /* DMB_EN */
#define	DMB_CS_TYPE					GPIO_INT_36  /*DMB_CS_N */
#define 	TDMB_RFBB_BASE_ADDR		 	EBI2_GP3_BASE
#define	DMB_PATTER_EN					GPIO_INT_86 /* PATTERN_TDMB_ANT_EN  = GPIO_OUT(86,4) */
#define 	DMB_EAR_ANT_EN 					GPIO_INT_87 /* EAR_TDMB_ANT_EN		  = GPIO_OUT(87,4) */
#endif

#define	TDMB_RFBB_DEV_ADDR                    0x80   /*1000 0000 (7bit addr +r/w_n) */
#define	TDMB_RFBB_RW_RETRY                    3

typedef enum
{
	TDMB_BB_DATA_TS,
	TDMB_BB_DATA_DAB,
	TDMB_BB_DATA_PACK,
	TDMB_BB_DATA_FIC,
	TDMB_BB_DATA_FIDC
} TDMB_BB_DATA_TYPE;

typedef struct
{
	uint16	reserved;
	uint8	subch_id;
	uint16	size;
	uint8	data_type:7;
	uint8	ack_bit:1;
} TDMB_BB_HEADER_TYPE;


/*============================================================
**    2.   External Variables
*============================================================*/

/*============================================================
**    3.   External Functions
*============================================================*/

/*============================================================
**    4.   Local constant variables
*============================================================*/

/*============================================================
**    5.   Local Typedef
*============================================================*/
typedef enum	fc8050_service_type
{
	FC8050_DAB = 1,
	FC8050_DMB = 2,
	FC8050_VISUAL =3,
	FC8050_DATA,
	FC8050_ENSQUERY = 6,	/*           */
	FC8050_BLT_TEST = 9,
	FC8050_SERVICE_MAX
} fc8050_service_type;

/*============================================================
**    6.   Global Variables
*============================================================*/

/*============================================================
**    7.   Static Variables
*============================================================*/

/*============================================================
**    8.Function Prototype
*============================================================*/

extern	int8 tunerbb_drv_fc8050_power_on(void);
extern	int8 tunerbb_drv_fc8050_power_off(void);
extern	int8 tunerbb_drv_fc8050_select_antenna(unsigned int sel);
extern	int8 tunerbb_drv_fc8050_init(void);
extern	int8 tunerbb_drv_fc8050_stop(void);
extern	int8 tunerbb_drv_fc8050_get_ber(struct broadcast_tdmb_sig_info *dmb_bb_info);
extern  int8 tunerbb_drv_fc8050_get_msc_ber(uint32 *msc_ber);
extern	int8 tunerbb_drv_fc8050_set_channel(int32 freq_num, uint8 subch_id, uint8 op_mode);
extern	int8 tunerbb_drv_fc8050_re_syncdetector(uint8 op_mode);
extern	int8 tunerbb_drv_fc8050_re_sync(void);
extern	int8 tunerbb_drv_fc8050_get_fic(uint8* buffer, uint32* buffer_size);
extern	int8 tunerbb_drv_fc8050_control_fic(uint8 enable);
extern	int8 tunerbb_drv_fc8050_read_data(uint8* buffer, uint32* buffer_size);
extern	int8 tunerbb_drv_fc8050_multi_set_channel(int32 freq_num, uint8 subch_cnt, uint8 subch_id[ ], uint8 op_mode[ ]);
extern	int8 tunerbb_drv_fc8050_process_multi_data(uint8 subch_cnt,uint8* input_buf, uint32 input_size, uint32* read_size);
extern 	int8 tunerbb_drv_fc8050_get_multi_data(uint8 subch_cnt, uint8* buf_ptr, uint32 buf_size);
extern	int8 tunerbb_drv_fc8050_start_tii(void);
extern	int8 tunerbb_drv_fc8050_check_tii(uint8 * pmain_tii,uint8 * psub_tii);
extern	int8 tunerbb_drv_fc8050_reset_ch(void);
extern void tunerbb_drv_fc8050_set_userstop(int mode);
extern int tunerbb_drv_fc8050_is_on(void);
#endif	/* __TUNERBB_DRV_FC8050_H__ */

