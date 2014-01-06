// ---------------------------------------------------------------------------
// Analogix Confidential Strictly Private
//
//
// ---------------------------------------------------------------------------
// >>>>>>>>>>>>>>>>>>>>>>>>> COPYRIGHT NOTICE <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// ---------------------------------------------------------------------------
// Copyright 2004-2010 (c) Analogix 
//
//Analogix owns the sole copyright to this software. Under international
// copyright laws you (1) may not make a copy of this software except for
// the purposes of maintaining a single archive copy, (2) may not derive
// works herefrom, (3) may not distribute this work to others. These rights
// are provided for information clarification, other restrictions of rights
// may apply as well.
//
// This is an unpublished work.
// ---------------------------------------------------------------------------
// >>>>>>>>>>>>>>>>>>>>>>>>>>>> WARRANTEE <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
// ---------------------------------------------------------------------------
// Analogix  MAKES NO WARRANTY OF ANY KIND WITH REGARD TO THE USE OF
// THIS SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
// PURPOSE.
// ---------------------------------------------------------------------------
#define _SP_TX_DRV_C_

#include "sp_tx_drv.h"
#include "sp_tx_reg.h"
#include "anx7805.h"

#define POWER_DOWN 0
#define POWER_ON 1

keil_51_code struct MIPI_Video_Format mipi_video_timing_table[] = { 
 //////////////////////pixel_clk--Htotal---H active--Vtotal--V active-HFP---HSW--HBP--VFP--VSW--VBP-----////////

 //{0, "1400x1050@60",		 108,			 1688,	 1400,	   1066,	 1050,		  88,	  50,		 150,	  9,  4,   3},
 {0, "720x480@60",		 27,		 858,	 720,	  525,	   480, 	  16,	  60,		 62,  10,  6,   29},
 {1, "1280X720P@60",	   74,		   1650,	  1280,    750, 	  720,		 110,	 40,	   220, 	5,	5,	20},
 {2, "1680X1050@60",	   146,	  2240, 	1680,	  1089, 	1050,	   104,   176,	   280, 	 3,  6 , 30},
 {3, "1920x1080p@60",	  148,	  2200, 	 1920,	   1125,	 1080,		88, 	44, 	 148,	   4, 2, 39},
 {4, "1920x1080p@24",	  59, 	 2200,		1920,	  1125, 	1080,	   88,	   44,		148,	  4, 2, 39},
 {5, "1920x1080p@30",	  74,	  2200, 	 1920,	   1125,	 1080,		88, 	44, 	 148,	   4, 2, 39},
 {6, "2560x1600@60",	 268,	  2720,  2560,	   1646,	 1600,		48, 	32, 	 80,  2,  6 , 38},
 //{7, "640x480@60",			25.17,			 816,		640,	   525, 	  480,		  32,	  96,	   48,	  10,  2,  33},
 {7, "640x480@60",			25,			 800,		640,	   525, 	  480,		  16,	  96,	   48,	  10,  2,  33},
 //{5, "640x480@60",		10.06,			 334,		320,	   502, 	  480,		  3,	 3, 	 8,   7,  8,  7},
 {8, "640x480@75",			 31,	  840,	 640,		   500, 	  480,		  16,	  64,		120,  1,  3,  16},
 {9, "1920*1200@60",	 154,			 2080,	 1920,	   1235,	 1200,		  40,	  80,		  40,	  3,  6,  26},	  
 {10, "800x600@60", 	 38,  976,	 800,		645,	   600, 	  32,	  96,		  48,	  10,  2,  33},
 //{11, "320x480@60",		 10.06, 		 334,		320,	   502, 	  480,		  3,	 3, 	 8,   7,  8,  7},
 {11, "320x480@60", 	 10, 		 352,		320,	   493, 	  480,		  8,	 8, 	 16,  6,  2,  5},
 {12, "1024x768@60",	 65,		 1344,		 1024,		   806, 	  768,		  24,	  136,		160,  3,  2,  33},
 {13, "1920x1200@60",	 154,		 2080,		 1920,		   1235,	  1200, 	  48,	  32,	   80,	  3,  2,  30},
 {14, "1280X1024@60",	 108,		 1688,		 1280,		   1066,	  1024, 	  48,	  112,	   248,   1,  2,  39}


};
keil_51_code struct Bist_Video_Format video_timing_table[] = { 
//number,video_type[32],pclk,h_total,h_active, v_total,v_active,h_front,h_sync,h_BP, v_FP, v_sync,v_BP, h_polarity, v_polarity, interlaced, repeat_times, frame_rate, bpp, video_mode;

    {0, "1400x1050@60",	  	108,	        1688,	1400,     1066,     1050,	     88,     50,	    150,	 9,  4,  3,   1,  1,  0, 1,	60, 1, 0},
    {1, "1280X720P@60",       75,          1650,      1280,    750,       720,       110,    40,       220,     5,  5,  20,   0,0,  0, 1,    60, 1, 1},
    {2, "1680X1050@60",       146,    2240,     1680,     1089,     1050,      104,   176,     280,      3,  6 , 30,  0,  1, 0, 1,   60, 1, 0},
    {3, "1920x1080p@60",     148,      2200,      1920,     1125,     1080,      88,     44,      148,      4, 5, 36, 0,0,  0, 1,    60, 1, 1},  
    {4, "2560x1600@60",		268,  	 2720,	2560,     1646,     1600,      48,     32,      80,	 2,  6 , 38,  1,  1, 0, 1, 	60, 1, 0},
    {5, "640x480@60",	       25,	        840,       640,	      500,       480,	     16,     64,      120,	 1,  3,  16,  1,  1,  0, 1,	75, 1, 0},
    {6, "640x480@75",	      	31,	 840,	640,	      500,       480,	     16,     64,       120,	 1,  3,  16,  1,  1,	0, 1,	75, 1, 0},
    {7, "1920*1200@60",		154,	        2080,	1920,     1235,     1200,	     40,     80,	     40,	 3,  6,  26,  1,  1, 0, 1,	60, 1, 0},    
    {8, "800x600@75",	  	49,	 1056,	800,       628,       600,	     40,     128,	     88,	 3,  6,  29,  1,  1, 0, 1,	75, 1, 0}, 
    {9, "800x480@60",           25,          848,	800,      493,        480,       14,     8,         26,       5,  1,   1,	  1,	1, 0, 1,	60, 1,0}
};
#if(REDUCE_REPEAT_PRINT_INFO)
void loop_print_msg(BYTE msg_id) 
{
	BYTE i = 0, no_msg = 0;
	if(maybe_repeat_print_info_flag == REPEAT_PRINT_INFO_CLEAR) {
		debug_puts("Repeat print info clear");
		for(i = 0; i < LOOP_PRINT_MSG_MAX; i++) 
			repeat_printf_info_count[i] = 0;
	}	
	switch(msg_id) {
		case 0x00:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_puts("Stream clock not found!");
			break;
		case 0x01:
			break;
		case 0x02:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_puts("video stream not valid!");
			break;
		case 0x03:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_puts("Stream clock not stable!");
			break;
		case 0x04:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_printf("colorspace: %.2x, Embedded_Sync: %.2x, DE_reDenerate: %.2x,YC_MUX: %.2x ",
					(WORD)SP_TX_Video_Input.bColorSpace,
					(WORD)SP_TX_Video_Input.bLVTTL_HW_Interface.sEmbedded_Sync.Embedded_Sync,
					(WORD)SP_TX_Video_Input.bLVTTL_HW_Interface.DE_reDenerate,
					(WORD)SP_TX_Video_Input.bLVTTL_HW_Interface.sYC_MUX.YC_MUX
					);
			break;
		case 0x05:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_printf("****Over bandwidth**** \n");
			break;
		case 0x06:
			break;	
		case 0x07:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_puts("PLL not lock!");
			break;	
		case 0x08:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_puts("LINK_TRAINING_ERROR! \n");
			break;
		case 0x09:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_puts("loop HDCP_END \n");
			break;	
		case 0x0A:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_puts("Video enable \n");
			break;	
		case 0x0B:
			if(repeat_printf_info_count[msg_id] == 0) 
				debug_puts("Video  disable \n");
			break;	
		default:
			no_msg = 1;
			break;			
	}
	if(no_msg == 0)
		repeat_printf_info_count[msg_id]++;
	
	maybe_repeat_print_info_flag = REPEAT_PRINT_INFO_START;
}
void confirm_loop_print_msg(void)
{
	if(maybe_repeat_print_info_flag > REPEAT_PRINT_INFO_INIT
	   && maybe_repeat_print_info_flag < REPEAT_PRINT_INFO_NUM)
		maybe_repeat_print_info_flag++;
}
#endif

void SP_TX_Initialization(struct VideoFormat* pInputFormat)
{
    BYTE c;
	/*
	 //software reset	 
	 SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, &c);
	 SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c | SP_TX_RST_SW_RST);
	 SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c & ~SP_TX_RST_SW_RST);
	*/
	 SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_EXTRA_ADDR_REG, 0x50);//EDID address for AUX access
	 SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CTRL, 0x02);	//disable HDCP polling mode.
	 SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_DEBUG_REG, 0x30);//enable M value read out


        /*added for B0 to enable enable c-wire polling-ANX.Fei-20110831*/
	 SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c);

	 SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, (c|0x82));//disable polling HPD, force hotplug for HDCP, enable polling
	 //SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, (c|0x80));//disable polling HPD, force hotplug for HDCP, disable polling, for HDCP blocked bug
	 //SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DP_POLLING_CTRL_REG, 0x01);

         /*added for B0 to change the c-wire termination from 100ohm to 50 ohm for polling error iisue-ANX.Fei-20110916*/
	 SP_TX_Read_Reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, &c);
	 SP_TX_Write_Reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, (c|0x30));//change the c-wire termination from 100ohm to 50 ohm

        SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x37, 0x26);//set 400mv3.5db value according to mehran CTS report-ANX.Fei-20111009
        SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x47, 0x10);//set 400mv3.5db value according to mehran CTS report-ANX.Fei-20111009

     SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	 SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c | 0x03);//set KSV valid


	 SP_TX_Write_Reg(SP_TX_PORT2_ADDR, ANALOG_DEBUG_REG2, 0x06);

	 //SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	 //SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c& (~0x08));//set signle AUX output

	 SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, 0x00);//mask all int
	 SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK2, 0x00);//mask all int
	 SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK3, 0x00);//mask all int
	 SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK4, 0x00);//mask all int

      //PHY parameter for cts
     //change for 200--400mv, 300-600mv, 400-800nv
     //400mv (actual 200mv)
      //Swing
      SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x30, 0x16);//0db
      SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x36, 0x1b);//3.5db
      SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x39, 0x22);//6db
      SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x3b, 0x23);//9db
      
      //Pre-emphasis
      //SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x40, 0x00);//0db
      SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x46, 0x09);//3.5db
      SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x49, 0x16);//6db
      SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x4b, 0x1F);//9db

     
     //600mv (actual 300mv)
     //Swing
     SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x31, 0x26);//0db
     SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x37, 0x28);//3.5db
     SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x3A, 0x2F);//6db

     //Pre-emphasis
     //SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x41, 0x00);//0db
     SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x47, 0x10);//3.5db
     SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x4A, 0x1F);//6db


     //800mv (actual 400mv)
     //Swing
     SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x34, 0x36);//0db
     SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x38, 0x3c);//3.5db
     //emp     
     //SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x44, 0x00);//0db
     SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x48, 0x10);//3.5db

     //1200mv (actual 600mv)
     //Swing
     SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x35, 0x3F);//0db
     //Pre-emphasis     
     //SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x45, 0x00);//0db

        /*added for B0 version-ANX.Fei-20110831-Begin*/
	 SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_INT_MASK, 0xb4);//0xb0 unmask IRQ request Int & c-wire polling error int
        /*added for B0 version-ANX.Fei-20110831-Begin*/

        //force termination open for clock lane
        SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, 0x02, &c);
        SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x02, (c&0x3f));

        SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, 0x2f, &c);
        SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x2f, (c|0xc0));

     	 //M value select, select clock with downspreading
	 SP_TX_API_M_GEN_CLK_Select(1);

	 if(pInputFormat->Interface == LVTTL_RGB)
	 {
		//set clock edge
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, (c & 0xfc) | 0x03);

		//set Input BPC mode & color space
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c &= 0x8c;
		c = c |(pInputFormat->bColordepth << 4);
		c |= pInputFormat->bColorSpace;
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);
	 }


	 //SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DP_POLLING_PERIOD, 0x01);

	  SP_TX_Write_Reg(SP_TX_PORT2_ADDR, 0x0F, 0x10);//0db
	debug_puts("SP_TX_Initialization\n");

}
void SP_TX_Power_Enable(SP_TX_POWER_BLOCK sp_tx_pd_block, BYTE power)
{
	BYTE need_return = 0, c, power_type;
    
    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG , &c);
	switch(sp_tx_pd_block) {
		case SP_TX_PWR_REG://power down register
			power_type = SP_POWERD_REGISTER_REG;
			break;
		case SP_TX_PWR_HDCP://power down IO
			power_type = SP_POWERD_HDCP_REG;
			break;
		case SP_TX_PWR_AUDIO://power down audio
			power_type = SP_POWERD_AUDIO_REG;
			break;
		case SP_TX_PWR_VIDEO://power down video
			power_type = SP_POWERD_VIDEO_REG;
			break;
		case SP_TX_PWR_LINK://power down link
			power_type = SP_POWERD_LINK_REG;
			break;
		case SP_TX_PWR_TOTAL://power down total.
			power_type = SP_POWERD_TOTAL_REG;
			break;
		default:
			need_return = 1;
			break;;
	}
	if(need_return == 0) {
		if( (power == SP_TX_POWER_ON) && (c & power_type) == power_type)
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG, (c & (~power_type)));
		else if((power == SP_TX_POWER_DOWN) && ((c & power_type) == 0)) 
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_POWERD_CTRL_REG, (c |power_type));
	}
	debug_puts("SP_TX_Power_Enable\n");

}

void system_power_ctrl(BYTE ON)
{
    BYTE c1,c2,c3;

	if(ON == 0) {
		SP_CTRL_Set_System_State(SP_TX_WAIT_SLIMPORT_PLUGIN);
		vbus_power_ctrl(0);
		SP_TX_Power_Enable(SP_TX_PWR_REG, POWER_DOWN);
		SP_TX_Power_Enable(SP_TX_PWR_TOTAL,POWER_DOWN);
		SP_TX_Hardware_PowerDown();
		sp_tx_pd_mode = 1;
	}else {
		sp_tx_pd_mode = 0; 
		SP_TX_Hardware_PowerOn();
		SP_TX_Power_Enable(SP_TX_PWR_REG, POWER_ON);
		SP_TX_Power_Enable(SP_TX_PWR_TOTAL, POWER_ON);
		SP_TX_Initialization(&SP_TX_Video_Input);
		vbus_power_ctrl(1);

		c1 = 0;
		c2 = 0;
		c3 = 0;
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c1);
	    	debug_printf("0x70  = %.2x\n",(WORD)c1);
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c1| 0x01);
	    	debug_puts("write 0x70 ok \n");

		  SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDL_REG , &c1);
		    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDH_REG , &c2);
		    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_DEV_REV_REG , &c3);
		    if ((c1==0x05) && (c2==0x78)&&(c3==0xca))
		    {
		        debug_puts("ANX7805 Reversion CA");
		    }
		    else
		    {
		    	debug_printf("dev IDL = %.2x, deb IDH = %.2x, REV= %.2x\n",(WORD)c1,(WORD)c2,(WORD)c3);
		    }
	}	
}

void sp_tx_enable_video_input(BYTE enable)
{
	BYTE c;
	if (enable) {
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c = (c & 0xf7) | 0x80;
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, 0xf5);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1, 0x0a);
		#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x0A);
		#else
		debug_puts("Video Enabled!\n");	
		#endif

	} else {
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c &= ~0x80;
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c);
		#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x0B);
		#else
		debug_puts("Video disable! \n");	
		#endif
	}
}
#ifdef __KEIL51_ENV__

void SP_TX_BIST_Format_Config(WORD sp_tx_bist_select_number)
{
    WORD sp_tx_bist_data;
    BYTE c,c1;
    WORD wTemp,wTemp1,wTemp2;
    BYTE bInterlace;


       debug_puts("config bist vid timing");
	if(!Force_Video_Resolution)
	{
		//use prefered timing if EDID read success, otherwise output failsafe mode.
		if((sp_tx_edid_err_code==0) && (!edid_pclk_out_of_range))
		{
			//Interlace or Progressive mode
			//temp = (SP_TX_EDID_PREFERRED[17]&0x80)>>7;
			c = SP_TX_EDID_PREFERRED[17] & 0x80;
			SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c1);
			if(c == 0)//progress
			{
				SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c1 &(~ SP_TX_VID_CTRL10_I_SCAN)));
				bInterlace = 0;
			}
			else//interlace
			{
				SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c1 | SP_TX_VID_CTRL10_I_SCAN));
				bInterlace = 1;
			}

			//Vsync Polarity set
			//temp = (SP_TX_EDID_PREFERRED[17]&0x04)>>2;	
			SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
			if(SP_TX_EDID_PREFERRED[17]&0x04)
			{
				SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_VSYNC_POL));
			}
			else
			{
				SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_VSYNC_POL)));
			}

			//Hsync Polarity set
			//temp = (SP_TX_EDID_PREFERRED[17]&0x20)>>1;	
			SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
			if(SP_TX_EDID_PREFERRED[17]&0x20)//h sync polarity should be bit 1 2010/07/06
			{
				SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_HSYNC_POL));
			}
			else
			{
				SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_HSYNC_POL)));
			}

			//H active length set
			wTemp = SP_TX_EDID_PREFERRED[4];
			wTemp = (wTemp << 4) & 0x0f00;
			sp_tx_bist_data = wTemp + SP_TX_EDID_PREFERRED[2];
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELL_REG, (sp_tx_bist_data&0x00FF));
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELH_REG, (sp_tx_bist_data>>8));

			//H total length = hactive+hblank
			wTemp = SP_TX_EDID_PREFERRED[4];
			wTemp = (wTemp<< 8) & 0x0f00;
			wTemp= wTemp + SP_TX_EDID_PREFERRED[3];	
			sp_tx_bist_data = sp_tx_bist_data + wTemp;
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELL_REG, (sp_tx_bist_data&0x00FF));
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELH_REG, (sp_tx_bist_data >> 8));


			//H front porch width set
			wTemp = SP_TX_EDID_PREFERRED[11];
			wTemp = (wTemp << 2) & 0x0300;
			wTemp = wTemp + SP_TX_EDID_PREFERRED[8];
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHL_REG, (wTemp&0xF00FF));
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHH_REG, (wTemp>>8));

			//H sync width set
			wTemp = SP_TX_EDID_PREFERRED[11];
			wTemp = (wTemp << 4) & 0x0300;
			wTemp = wTemp + SP_TX_EDID_PREFERRED[9];
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, (wTemp&0xF00FF));
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, (wTemp>>8));

			//H back porch = H blank - H Front porch - H sync width
			//Hblank
			wTemp = SP_TX_EDID_PREFERRED[4];
			wTemp = (wTemp<< 8) & 0x0f00;
			wTemp= wTemp + SP_TX_EDID_PREFERRED[3];

			//H Front porch
			wTemp1 = SP_TX_EDID_PREFERRED[11];
			wTemp1 = (wTemp1 << 2) & 0x0300;
			wTemp1 = wTemp1 + SP_TX_EDID_PREFERRED[8];

			//Hsync width
			sp_tx_bist_data = SP_TX_EDID_PREFERRED[11];
			sp_tx_bist_data = (sp_tx_bist_data << 4) & 0x0300;
			sp_tx_bist_data = sp_tx_bist_data + SP_TX_EDID_PREFERRED[9];

			//H Back porch
			wTemp2 = wTemp - wTemp1 - sp_tx_bist_data;
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHL_REG, (wTemp2&0x00ff));
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHH_REG, (wTemp2 >> 8));

			//V active length set
			wTemp = SP_TX_EDID_PREFERRED[7];
			wTemp = (wTemp << 4) & 0x0f00;
			sp_tx_bist_data = wTemp + SP_TX_EDID_PREFERRED[5];
			//for interlaced signal
			if(bInterlace ==1)
			sp_tx_bist_data = sp_tx_bist_data*2;
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEL_REG, (sp_tx_bist_data&0x00ff));
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEH_REG, (sp_tx_bist_data >> 8));

			//V total length set
			wTemp = SP_TX_EDID_PREFERRED[7];
			wTemp = (wTemp << 8) & 0x0f00;
			wTemp = wTemp + SP_TX_EDID_PREFERRED[6];
			//vactive+vblank
			sp_tx_bist_data = sp_tx_bist_data + wTemp;
			//for interlaced signal
			if(bInterlace ==1)
			sp_tx_bist_data = sp_tx_bist_data*2+1;
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEL_REG, (sp_tx_bist_data&0x00ff));
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEH_REG, (sp_tx_bist_data >> 8));

			//V front porch width set
			wTemp = SP_TX_EDID_PREFERRED[11];
			wTemp = (wTemp << 2) & 0x0030;
			wTemp = wTemp + (SP_TX_EDID_PREFERRED[10] >> 4);
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VF_PORCH_REG, wTemp);

			//V sync width set

			wTemp = SP_TX_EDID_PREFERRED[11];
			wTemp = (wTemp << 4) & 0x0030;
			wTemp = wTemp + (SP_TX_EDID_PREFERRED[10] & 0x0f);
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG, wTemp);


			//V back porch = V blank - V Front porch - V sync width
			//V blank
			wTemp = SP_TX_EDID_PREFERRED[7];
			wTemp = (wTemp << 8) & 0x0f00;
			wTemp = wTemp + SP_TX_EDID_PREFERRED[6];

			//V front porch
			wTemp1 = SP_TX_EDID_PREFERRED[11];
			wTemp1 = (wTemp1 << 2) & 0x0030;
			wTemp1 = wTemp1 + (SP_TX_EDID_PREFERRED[10] >> 4);

			//V sync width
			wTemp2 = SP_TX_EDID_PREFERRED[11];
			wTemp2 = (wTemp2 << 4) & 0x0030;
			wTemp2 = wTemp2 + (SP_TX_EDID_PREFERRED[10] & 0x0f);
			sp_tx_bist_data = wTemp - wTemp1 - wTemp2;
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VB_PORCH_REG, sp_tx_bist_data);

		}
		else
		{

			SP_TX_BIST_Format_Resolution(5);

			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, 0x00);//18bpp for fail safe mode
			debug_puts("safe mode  = 640*480p@60hz_18bpp");
		}
	} 
	else
	SP_TX_BIST_Format_Resolution(sp_tx_bist_select_number);
	
	
    //BIST color bar width set--set to each bar is 32 pixel width
    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, (c &(~SP_TX_VID_CTRL4_BIST_WIDTH)));

    if(sp_tx_lane_count == 1)
    {
    	//set to gray bar
    	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
    	c&= 0xfc;
    	c|= 0x01;
    	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, c);
    }
    
    //Enable video BIST
    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, (c | SP_TX_VID_CTRL4_BIST));

}
void SP_TX_BIST_Format_Resolution(unsigned int video_id)
{
    WORD sp_tx_bist_data;
    BYTE c;

    sp_tx_bist_data = video_timing_table[video_id].is_interlaced;
    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
    if(sp_tx_bist_data == 0)
    {
        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_I_SCAN)));
        debug_puts("Bist video is progressive.");
    }
    else
    {
        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_I_SCAN));
        debug_puts("Bist video is interlace.");
    }

    //Vsync Polarity set
    sp_tx_bist_data = video_timing_table[video_id].v_sync_polarity;
    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
    if(sp_tx_bist_data == 1)
    {
        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_VSYNC_POL));
        debug_puts("Bist video VSYNC polarity: low is active.");
    }
    else
    {
        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_VSYNC_POL)));
        debug_puts("Bist video VSYNC polarity: high is active.");
    }

    //Hsync Polarity set
    sp_tx_bist_data = video_timing_table[video_id].h_sync_polarity;
    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
    if(sp_tx_bist_data == 1)
    {
        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c | SP_TX_VID_CTRL10_HSYNC_POL));
        debug_puts("Bist video HSYNC polarity: low is active.");
    }
    else
    {
        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, (c &(~ SP_TX_VID_CTRL10_HSYNC_POL)));
        debug_puts("Bist video HSYNC polarity: high is active.");
    }    

    //H total length set
    sp_tx_bist_data = video_timing_table[video_id].h_total_length;

    //sp_tx_bist_data = video_timing_table[video_id].h_total_length;
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELL_REG, sp_tx_bist_data);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELH_REG, (sp_tx_bist_data >> 8));

    //H active length set
    sp_tx_bist_data = video_timing_table[video_id].h_active_length;
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELL_REG, sp_tx_bist_data);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELH_REG, (sp_tx_bist_data >> 8));

    //H front porth width set
    sp_tx_bist_data = video_timing_table[video_id].h_front_porch;
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHL_REG, sp_tx_bist_data);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHH_REG, (sp_tx_bist_data >> 8));

    //H sync width set
    sp_tx_bist_data = video_timing_table[video_id].h_sync_width;
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, sp_tx_bist_data);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, (sp_tx_bist_data >> 8));

    //H back porth width set
    sp_tx_bist_data = (video_timing_table[video_id].h_back_porch);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHL_REG, sp_tx_bist_data);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHH_REG, (sp_tx_bist_data >> 8));

    //V total length set
    sp_tx_bist_data = video_timing_table[video_id].v_total_length;
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEL_REG, sp_tx_bist_data);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEH_REG, (sp_tx_bist_data >> 8));

    //V active length set
    sp_tx_bist_data = video_timing_table[video_id].v_active_length;
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEL_REG, sp_tx_bist_data);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEH_REG, (sp_tx_bist_data >> 8));


    //V front porth width set
    sp_tx_bist_data = video_timing_table[video_id].v_front_porch;
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VF_PORCH_REG, sp_tx_bist_data);

    //V sync width set
    sp_tx_bist_data = video_timing_table[video_id].v_sync_width;
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG, sp_tx_bist_data);

    //V back porth width set
    sp_tx_bist_data = video_timing_table[video_id].v_back_porch;
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VB_PORCH_REG, sp_tx_bist_data);
}

void SP_TX_Config_BIST_Video (BYTE cBistIndex,struct VideoFormat* pInputFormat)
{

   BYTE c;
       //power down MIPI,enable lvttl input
	SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
	c |= 0x10;
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, c);

	//SP_TX_Clean_HDCP();
	SP_CTRL_Clean_HDCP();
	SP_TX_Power_Enable(SP_TX_PWR_VIDEO, POWER_ON);

	debug_puts("Configure video format in BIST mode");

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, c);
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, &c);
	if(!(c & SP_TX_SYS_CTRL1_DET_STA))
	{
		debug_puts("Stream clock not found!");
		return;
	}
	
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, c);
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	if(c & SP_TX_SYS_CTRL2_CHA_STA)
	{
		debug_puts("Stream clock not stable!");
		return;
	}

	//set Input BPC mode & color space
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c &= 0x8c;
	if(cBistIndex == 4)//bist fail safe mode, set to 18 bpp for CTS test
		c&=0x8f;
	else
		c = c |(pInputFormat->bColordepth << 4);

		c |= pInputFormat->bColorSpace;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);


	SP_TX_BIST_Format_Config(cBistIndex);

	//enable video input
	sp_tx_enable_video_input(1);
	delay_ms(50);
	
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, c);
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
	if(!(c & SP_TX_SYS_CTRL3_STRM_VALID))
	{
		debug_puts("video stream not valid!");
		return;
	}
       SP_TX_Config_Packets(AVI_PACKETS);
	   
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, c|0x0e);//Unmask video clock&format change &PLL int

	//if(Force_AUD)//force to configure audio regadless of EDID info.
	//sp_tx_ds_edid_hdmi = 1;

	if(sp_tx_ds_edid_hdmi)
	{
            SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
	}
	else
       {       	
            SP_TX_Power_Enable(SP_TX_PWR_AUDIO, POWER_DOWN);//power down audio when DVI
            SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
        }
	
}
void SP_CTRL_BIST_Clk_MN_Gen(WORD sp_tx_bist_select_number)
{

    switch (sp_tx_bist_select_number) 
    {
        case 0:    //Freq = 108MHz, M=216, N=2
			SP_CTRL_nbc12429_setting(108);
			pclk = 54;
			break;
			
     	case 1: //Freq = 75, M=146, N=1
			SP_CTRL_nbc12429_setting(75);
			pclk = 37;
			break;
	case 2:    //Freq = 146MHz, M=308, N=2
			SP_CTRL_nbc12429_setting(146);
			pclk = 73;
			break;

        case 3:    //Freq = 154.128MHz, M=308, N=2
			SP_CTRL_nbc12429_setting(148);
			pclk = 74;
			break;
			
        case 4: //Freq = 268, M=268, N=0
			SP_CTRL_nbc12429_setting(268);
			pclk = 134;
			break;
			
	 case 5: //Freq = 25, M=100, N=2
			SP_CTRL_nbc12429_setting(25);
			pclk = 12;
			break;
			
        case 6: //Freq = 31.5, M=268, N=0
			SP_CTRL_nbc12429_setting(31);
			pclk = 16;
                     break;

        case 7: //Freq = 154, M=268, N=0
			SP_CTRL_nbc12429_setting(154);
			pclk = 77;
                     break;

        case 8: //Freq = 49.5, M=268, N=0
			SP_CTRL_nbc12429_setting(49);
			pclk = 25;
                     break;
	 case 9: //Freq = 25, M=268, N=0
			SP_CTRL_nbc12429_setting(25);
			pclk = 12;
                     break;
			
        default:
                    break;
         
    }

}

void SP_CTRL_BIST_CLK_Genarator(WORD sp_tx_bist_select_number)
{

   WORD wTemp;	
   int M,N;
   int wPixelClk;//,wPixelClk1;
   
   if(!Force_Video_Resolution)
   {
	   if(sp_tx_edid_err_code == 0)//commented for QDI test
	   {
		   //select the correct clock according to EDID
		   //Get pixel clock
		   wTemp = SP_TX_EDID_PREFERRED[1];
		   wTemp = wTemp << 8;
		   wPixelClk = wTemp + SP_TX_EDID_PREFERRED[0];
		   debug_printf("Pixel clock is 10000 * %u\n",	wPixelClk);
		   //debug_printf("config clk\n");
   
		   if((wPixelClk > 27000)||(wPixelClk < 2500))//25M-256M clk
		   {

			edid_pclk_out_of_range = 1;
			SP_CTRL_nbc12429_setting(25);
			pclk = 12;
			debug_puts("clk out of range, set to safe clock 25MHz SDR\n");

		   }
		   else
		   {
			M = wPixelClk/100;
			SP_CTRL_nbc12429_setting(M);
			pclk = M/2;
			debug_printf("clock M =0x%.2x, N = 0x%.2x\n",(WORD)M,(WORD)N);

		   }
	   }
	   else
	   {
		SP_CTRL_nbc12429_setting(25);
		pclk = 12;
		debug_puts("EDID read error, set to safe clk 25Mhz SDR\n");
	   }
   }
   else
	   SP_CTRL_BIST_Clk_MN_Gen(sp_tx_bist_select_number);

 
}


void SP_CTRL_nbc12429_setting(int frequency)
{
	int m_setting;
	//BYTE x,y;
	debug_printf("set pclk: %d\n",frequency);
	
	if(/* frequency>=25 &&*/ frequency<=50)
	{
		// N = 8
		 MC12429_N0 = 1;
		 MC12429_N1 = 1;		 
		 m_setting = frequency << 3;
         /*
                if((sp_tx_lane_count!=0x01)&&(SP_TX_Video_Input.bColordepth != COLOR_12_BIT))
                {         
                    m_setting = frequency << 2;
                }*/
		 
		 MC12429_M0 = (m_setting & 0x001);
		 MC12429_M1 = (m_setting & 0x002);
		 MC12429_M2 = (m_setting & 0x004);
		 MC12429_M3 = (m_setting & 0x008);
		 MC12429_M4 = (m_setting & 0x010);
		 MC12429_M5 = (m_setting & 0x020);
		 MC12429_M6 = (m_setting & 0x040);
		 MC12429_M7 = (m_setting & 0x080);
		 MC12429_M8 = (m_setting & 0x100);
		 
	}
	else if(frequency>50 && frequency<=110)
	{
		// N = 4
		MC12429_N0 = 0;
		MC12429_N1 = 1;
           /*
		if((sp_tx_lane_count!=1)&&(SP_TX_Video_Input.bColordepth != COLOR_12_BIT))
		{	   
		   MC12429_N0 = 1;
		   MC12429_N1 = 1;
		}*/
	  
		 m_setting = frequency << 2;
		 MC12429_M0 = (m_setting & 0x001);
		 MC12429_M1 = (m_setting & 0x002);
		 MC12429_M2 = (m_setting & 0x004);
		 MC12429_M3 = (m_setting & 0x008);
		 MC12429_M4 = (m_setting & 0x010);
		 MC12429_M5 = (m_setting & 0x020);
		 MC12429_M6 = (m_setting & 0x040);
		 MC12429_M7 = (m_setting & 0x080);
		 MC12429_M8 = (m_setting & 0x100);
	}
	else if(frequency>110 && frequency<=200)
	{
		// N = 2
		MC12429_N0 = 1;
		MC12429_N1 = 0;
	   /*
		if((sp_tx_lane_count!=1)&&(SP_TX_Video_Input.bColordepth != COLOR_12_BIT))
		{	   
		   MC12429_N0 = 0;
		   MC12429_N1 = 1;
		}*/
		
		 m_setting = frequency << 1;
		 
		 MC12429_M0 = (m_setting & 0x001);
		 MC12429_M1 = (m_setting & 0x002);
		 MC12429_M2 = (m_setting & 0x004);
		 MC12429_M3 = (m_setting & 0x008);
		 MC12429_M4 = (m_setting & 0x010);
		 MC12429_M5 = (m_setting & 0x020);
		 MC12429_M6 = (m_setting & 0x040);
		 MC12429_M7 = (m_setting & 0x080);
		 MC12429_M8 = (m_setting & 0x100);
	}
	else if(frequency>200 && frequency<=400)
	{
		// N = 1
		MC12429_N0 = 0;
		MC12429_N1 = 0;
		/*
		if((sp_tx_lane_count!=1)&&(SP_TX_Video_Input.bColordepth != COLOR_12_BIT))
		{	   
		   MC12429_N0 = 1;
		   MC12429_N1 = 0;
		}  */
		 m_setting = frequency ;
		 
		 MC12429_M0 = (m_setting & 0x001);
		 MC12429_M1 = (m_setting & 0x002);
		 MC12429_M2 = (m_setting & 0x004);
		 MC12429_M3 = (m_setting & 0x008);
		 MC12429_M4 = (m_setting & 0x010);
		 MC12429_M5 = (m_setting & 0x020);
		 MC12429_M6 = (m_setting & 0x040);
		 MC12429_M7 = (m_setting & 0x080);
		 MC12429_M8 = (m_setting & 0x100);
	}
	else
	debug_puts("Wrong value given!");
}
#endif
BYTE get_bandwidth_and_pclk(void)
{
	BYTE c,c1;
	WORD wPacketLenth;

	SP_TX_Power_Enable(SP_TX_PWR_VIDEO, POWER_ON);
	if(SP_TX_Video_Input.Interface == LVTTL_RGB)
	{
		if(SP_TX_Config_Video_LVTTL(&SP_TX_Video_Input))
			return 1;
		else
		#if(REDUCE_REPEAT_PRINT_INFO)
			loop_print_msg(0x04);
		#endif					
	}
	else if(SP_TX_Video_Input.Interface == MIPI_DSI)
	{

            pclk = mipi_video_timing_table[bMIPIFormatIndex].MIPI_pixel_frequency;
            if(SP_TX_BW_LC_Sel(&SP_TX_Video_Input))
        	{
        	    debug_puts("****Over bandwidth****");
                   return 1;
        	}
                    //set bandwidth
            SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, sp_tx_bw);
		if(SP_TX_Config_Video_MIPI())
			return 1;
	}

	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, 0xf5);//unmask video clock change&format change int
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1, 0x0a);//Clear video format and clock change-20111206-ANX.Fei

	SP_TX_Video_Mute(1);

	//enable video input
	sp_tx_enable_video_input(1);
	
	delay_ms(50);
	if(SP_TX_Video_Input.Interface == LVTTL_RGB)
	{
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, c);
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
		if(!(c & SP_TX_SYS_CTRL3_STRM_VALID))
		{
			#if(REDUCE_REPEAT_PRINT_INFO)
			loop_print_msg(0x02);
			#else
			debug_puts("video stream not valid!");
			#endif
			return 1;
		}
		//debug_puts("video stream valid!");
            //Get  transmit lane count&link bw
            SP_TX_Get_Link_BW(&c);
            sp_tx_bw = (SP_LINK_BW)c;
            //Calculate the pixel clock
            SP_TX_PCLK_Calc(sp_tx_bw);
	}
	else //MIPI DSI
	{
		//check video packet to determin if video stream valid
		SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_LONG_PACKET_LENTH_LOW, &c);
		SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_LONG_PACKET_LENTH_HIGH, &c1);

		wPacketLenth = (mipi_video_timing_table[bMIPIFormatIndex].MIPI_HActive)*3;

		if(((wPacketLenth&0x00ff)!=c)||(((wPacketLenth&0xff00)>>8)!=c1))
		{
			#if(REDUCE_REPEAT_PRINT_INFO)
			loop_print_msg(0x02);
			#else
			debug_puts("video stream not valid!");
			#endif

			return 1;
		}
		else
		//debug_puts("mipi video stream valid!");
       		pclk = mipi_video_timing_table[bMIPIFormatIndex].MIPI_pixel_frequency;
	}
	//Optimize the LT to get minimum power consumption
	if(SP_TX_BW_LC_Sel(&SP_TX_Video_Input))
	{
	#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x05);
	#else
	    debug_puts("****Over bandwidth****");
	#endif
		   return 1;
	}			
	debug_printf("get->pclk: %.2x, The optimized BW =%.2x, Lane cnt=%.2x \n",
			(WORD)pclk, (WORD)sp_tx_bw, (WORD)sp_tx_lane_count);
	return 0;
			
}

void SP_TX_DE_reGenerate (WORD video_id)
{
    BYTE c;

	//interlace scan mode configuration
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG,
	    (c & 0xfb) | (video_timing_table[video_id].is_interlaced<< 2));

	//V sync polarity
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, 
	    (c & 0xfd) | (video_timing_table[video_id].v_sync_polarity<< 1) );

	//H sync polarity
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, 
	    (c & 0xfe) | (video_timing_table[video_id].h_sync_polarity) );

	//active line
	c = video_timing_table[video_id].v_active_length& 0xff;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEL_REG, c);
	c = video_timing_table[video_id].v_active_length >> 8;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEH_REG, c);

	//V sync width
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG, 
	    video_timing_table[video_id].v_sync_width);

	//V sync back porch.
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VB_PORCH_REG, 
	    video_timing_table[video_id].v_back_porch);

	//total pixel in each frame
	c = video_timing_table[video_id].h_total_length& 0xff;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELL_REG, c);
	c = video_timing_table[video_id].h_total_length>> 8;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELH_REG, c);

	//active pixel in each frame.
	c = video_timing_table[video_id].h_active_length& 0xff;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELL_REG, c);
	c = video_timing_table[video_id].h_active_length >> 8;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELH_REG, c);

	//pixel number in H period
	c = video_timing_table[video_id].h_sync_width& 0xff;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, c);
	c = video_timing_table[video_id].h_sync_width >> 8;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, c);

	//pixel number in frame horizontal back porch
	c = video_timing_table[video_id].h_back_porch& 0xff;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHL_REG, c);
	c = video_timing_table[video_id].h_back_porch >> 8;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHH_REG, c);

	//enable DE mode.
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c | SP_TX_VID_CTRL1_DE_GEN);
}

void SP_TX_Embedded_Sync(struct VideoFormat* pInputFormat, WORD video_id)
{
    BYTE c;

	//set embeded sync flag check 
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, 
	    (c & ~SP_TX_VID_CTRL4_EX_E_SYNC) | 
	    pInputFormat->bLVTTL_HW_Interface.sEmbedded_Sync.Extend_Embedded_Sync_flag << 6);

	// SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL3_REG, &c);
	// SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL3_REG,(c & 0xfb | 0x02));

	//set Embedded sync repeat mode.
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, 
	    (c & 0xcf) | (video_timing_table[video_id].pix_repeat_times<< 4) );

	//V sync polarity
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, 
	    (c & 0xfd) | (video_timing_table[video_id].v_sync_polarity<< 1) );

	//H sync polarity
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL10_REG, 
	    (c & 0xfe) | (video_timing_table[video_id].h_sync_polarity) );

	//V  front porch.
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VF_PORCH_REG, 
	    video_timing_table[video_id].v_front_porch);

	//V sync width
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG, 
	    video_timing_table[video_id].v_sync_width);

	//H front porch
	c = video_timing_table[video_id].h_front_porch& 0xff;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHL_REG, c);
	c = video_timing_table[video_id].h_front_porch >> 8;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHH_REG, c);

	//H sync width
	c = video_timing_table[video_id].h_sync_width& 0xff;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, c);
	c = video_timing_table[video_id].h_sync_width >> 8;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, c);

	//Enable Embedded sync 
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, c | SP_TX_VID_CTRL4_E_SYNC_EN);

}

BYTE SP_TX_Config_Video_LVTTL (struct VideoFormat* pInputFormat)
{
    BYTE c;//,i;
    //power down MIPI,enable lvttl input
	SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
	c |= 0x10;
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, c);

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, c);
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL1_REG, &c);
	if(!(c & SP_TX_SYS_CTRL1_DET_STA))
	{
	#if(REDUCE_REPEAT_PRINT_INFO)
		loop_print_msg(0x00);
	#else
		debug_puts("Stream clock not found!");
	#endif
		return 1;
	}

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, c);
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
	if(c & SP_TX_SYS_CTRL2_CHA_STA)
	{
		#if(REDUCE_REPEAT_PRINT_INFO)
			loop_print_msg(0x03);
		#else
			debug_puts("Stream clock not stable!");
		#endif
		return 1;
	}

    	if(pInputFormat->bColorSpace==COLOR_YCBCR_444)
	{
		//debug_puts("ColorSpace YCbCr444");
              SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
              SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0xfc)|0x02));
		//SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL5_REG, 0x90);//enable Y2R conversion based on BT709
		//SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL6_REG, 0x40);//enable video porcess
	}
	else if(pInputFormat->bColorSpace==COLOR_YCBCR_422)
	{
            //debug_puts("ColorSpace YCbCr422");
            SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0xfc)|0x01));


           // SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL5_REG, 0x90);//enable Y2R conversion based on BT709
            //SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL6_REG, 0x42);//enable video porcess and upsample
	}
	else
	{
           // debug_puts("ColorSpace RGB");
            SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, (c&0xfc));
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL5_REG, 0x00);
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL6_REG, 0x00);
	}
    
    
	if(pInputFormat->bLVTTL_HW_Interface.sEmbedded_Sync.Embedded_Sync)
	{
		//debug_puts("Embedded_Sync");
		SP_TX_Embedded_Sync(pInputFormat,1);//set 720p as the default
	}
	else
	{
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL4_REG, c & ~SP_TX_VID_CTRL4_E_SYNC_EN);
	}

	if(pInputFormat->bLVTTL_HW_Interface.DE_reDenerate)
	{
		//debug_puts("DE_reDenerate\n");
		SP_TX_DE_reGenerate(1);//set 720p as the default
	}
	else
	{
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c & ~SP_TX_VID_CTRL1_DE_GEN);
	}

	if(pInputFormat->bLVTTL_HW_Interface.sYC_MUX.YC_MUX)
	{
		//debug_puts("YC_MUX\n");
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, (c & 0xef) | SP_TX_VID_CTRL1_DEMUX);

		if(pInputFormat->bLVTTL_HW_Interface.sYC_MUX.YC_BIT_SEL)
		{
			SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, (c & 0xfb) |SP_TX_VID_CTRL1_YCBIT_SEL );
		}
		
	}
	else
	{
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c & ~SP_TX_VID_CTRL1_DEMUX);
	}

	if(pInputFormat->bLVTTL_HW_Interface.DDR_Mode)
	{
		//debug_puts("DDR_mode\n");
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, (c & 0xfb) | SP_TX_VID_CTRL1_IN_BIT);
	}
	else
	{
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, c & ~SP_TX_VID_CTRL1_IN_BIT);
	}

      //Force output to CEA range(16-235)
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL9_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL9_REG, c|0x80);

	SP_TX_LVTTL_Bit_Mapping(pInputFormat);
	return 0;

}
void SP_TX_LVTTL_Bit_Mapping(struct VideoFormat* pInputFormat)//the default mode is 12bit ddr
{
    
	BYTE c;
	if(pInputFormat->bLVTTL_HW_Interface.DDR_Mode)
        {
            switch(pInputFormat->bColordepth)
            {
                case COLOR_8_BIT://correspond with ANX8770 24bit DDR mode 
                    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
                    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0x8f)|0x10));//set input video 8-bit
                    for(c=0; c<12; c++)
                    {	 
                        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, 0x40 + c, 0x06 + c);
                    }
                    for(c=0; c<12; c++)
                    {	 
                        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, 0x4d + c, 0x1e + c);
                    }
                    break;
                case COLOR_10_BIT://correspond with ANX8770 30bit DDR mode 
                    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
                    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0x8f)|0x20));//set input video 10-bit
                    for(c=0; c<15; c++)
                    {	 
                        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, 0x40 + c, 0x03 + c);
                    }
                    for(c=0; c<15; c++)
                    {	 
                        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, 0x4f + c, 0x1b + c);
                    }
                    break;
                case COLOR_12_BIT://correspond with ANX8770 36bit DDR mode 
                    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
                    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0x8f)|0x30));//set input video 12-bit
                    for(c=0; c<18; c++)
                    {	 
                        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, 0x40 + c, 0x00 + c);
                    }
                    for(c=0; c<18; c++)
                    {	 
                        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, 0x52 + c, 0x18 + c);
                    }
                    break;	
                default:
                    break;
            }
        }
       else
        {
            switch(pInputFormat->bColordepth)
            {
                case COLOR_8_BIT://8bit SDR mode
                    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
                    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, ((c&0x8f)|0x10));//set input video 8-bit
                    for(c=0; c<24; c++)
                    {	 
                        SP_TX_Write_Reg(SP_TX_PORT2_ADDR, 0x40 + c, 0x00 + c);
                    }
                    break;
                default:
                        break;
            }
        }
	   
	#if(AUTO_TEST_CTS)
	if (sp_tx_test_edid ){
		/*set color depth to 18-bit for link cts*/
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c = (c & 0x8f);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);
		sp_tx_test_edid = 0;
		debug_puts("***color space is set to 18bit***");
	}
	#endif
	
		delay_ms(10);
}

BYTE  SP_TX_Config_Video_MIPI (void)
{
        BYTE c;//,i;
    
        if(!bMIPI_Configured)
        {
        	//config video format
        	SP_TX_Config_MIPI_Video_Format();

			//force blanking vsync
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, 0xB4, &c);
            SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0xb4, (c|0x02));
        	
            //power down MIPI,
            SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
            c |= 0x10;
            SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, c);
            
            //set power up counter, never power down high speed data path during blanking
            SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG2, 0x40);
            //SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG3, 0xC4);

			//set LP high reference voltage to 800mv
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x2A, 0x0b);

			//SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x1c, 0x10);
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x1c, 0x31);
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x1b, 0xbb);

			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x19, 0x3e);

            SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x08, 0x08);
			


	//set mipi data lane count

			SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, &c);
			if((MIPI_LANE_SEL_0)&&(MIPI_LANE_SEL_1))
			{
				debug_puts("####1 lane");
				//set lane count
				c&= 0xF9;
			}
			else if((MIPI_LANE_SEL_0)&&(!MIPI_LANE_SEL_1))
			{
				debug_puts("####2 lanes");
				//set lane count
				c&= 0xF9;
				c|= 0x02;// two lanes	
			}
			else if((!MIPI_LANE_SEL_0)&&(MIPI_LANE_SEL_1))
			{
				debug_puts("####3 lanes");
				//set lane count
				c&= 0xF9;
				c|= 0x04;// three lanes 
			}
			else
			{
				debug_puts("####4 lanes");
				//set lane count
				c&= 0xF9;
				c|= 0x06;// four lanes	
			}
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, c);//Set 4 lanes, link clock 270M

    
            //power on MIPI, //enable MIPI input
            SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
            c &= 0xEF;
            SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, c);
    
            //control reset_n_ls_clk_comb to reset mipi
            SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, &c);
            c&=0xF7;
            SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, c);
            delay_ms(1);
            c|=0x08;
            SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_MISC_CTRL, c);

			//reset low power mode
			SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG2, &c);
			c|=0x01;
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG2, c);
			delay_ms(1);
			c&=0xfe;
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_TIMING_REG2, c);
    
            debug_puts("MIPI configured!");
    
            SP_TX_MIPI_CONFIG_Flag_Set(1);
        
    
        }
        else
            debug_puts("MIPI interface enabled");
        
    
        SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_PROTOCOL_STATE, &c);
        SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, MIPI_PROTOCOL_STATE, c);
        SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_PROTOCOL_STATE, &c);
        if(!(c & 0X40))
        {
            debug_puts("Stream clock not found!");
            debug_printf("0x70:0x80=%.2x\n",(WORD)c);
            delay_ms(100);
            return 1;
        }
#ifdef MIPI_DEBUG
        else
        {
            debug_puts("#######Stream clock found!");
            debug_printf("0x70:0x80=%.2x\n",(WORD)c);
        }
#endif
            
    
        SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
        SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, c);
        SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL2_REG, &c);
        if(c & SP_TX_SYS_CTRL2_CHA_STA)
        {
            debug_puts("Stream clock not stable!");
            debug_printf("0x70:0x04=%.2x\n",(WORD)c);
            delay_ms(100);
    
            return 1;
        }
#ifdef MIPI_DEBUG
        else
        {
            debug_puts("#######Stream clock stable!");
            debug_printf("0x70:0x04=%.2x\n",(WORD)c);
        }
#endif
    



      return 0;

}

void SP_TX_EnhaceMode_Set(void)
{
    BYTE c;    
	SP_TX_AUX_DPCDRead_Bytes(0x00,0x00,DPCD_MAX_LANE_COUNT,1,&c);
	if(c & 0x80)
	{

		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, c | SP_TX_SYS_CTRL4_ENHANCED);

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x01,DPCD_LANE_COUNT_SET,1,&c);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x01,DPCD_LANE_COUNT_SET, c | 0x80);

		debug_puts("Enhance mode enabled");
	}
	else
	{

		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL4_REG, c & (~SP_TX_SYS_CTRL4_ENHANCED));

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x01,DPCD_LANE_COUNT_SET,1,&c);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x01,DPCD_LANE_COUNT_SET, c & (~0x80));

		debug_puts("Enhance mode disabled");
	}
}


void SP_TX_Clean_HDCP(void)
{
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, 0x00);//disable HW HDCP
    //set re-auth
    SP_TX_HDCP_ReAuth();
}

void hdcp_encryption_enable(BYTE enable)
{
	BYTE c;	
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	if(enable == 0)
		c &= ~SP_TX_HDCP_CONTROL_0_HDCP_ENC_EN;
	else
		c |= SP_TX_HDCP_CONTROL_0_HDCP_ENC_EN;
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c & ~SP_TX_HDCP_CONTROL_0_HDCP_ENC_EN);
}

void SP_TX_HW_HDCP_Enable(void)
{
	BYTE c;
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	c&=0xf3;
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c);
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	c|=0x0f;//enable HDCP and encryption
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c);
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	debug_printf("SP_TX_HDCP_CTRL0_REG = %.2x\n", (unsigned int)c);

	/*SP_TX_WAIT_R0_TIME*/
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0x40, 0xb0);
	/*SP_TX_WAIT_KSVR_TIME*/
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0x42, 0xc8);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK2, 0xfc);//unmask auth change&done int

	debug_puts("Hardware HDCP is enabled.\n");
}
/*
void SP_TX_HW_HDCP_Disable(void)
{
    BYTE c;
    SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c & ~SP_TX_HDCP_CONTROL_0_HARD_AUTH_EN);
}
*/
void SP_TX_PCLK_Calc(SP_LINK_BW hbr_rbr)
{
    long int str_clk;
    BYTE c;
    switch(hbr_rbr)
    {
      case BW_54G:
	  	str_clk = 540;
		break;
      /*case BW_45G:
	  	str_clk = 450;
		break;*/
      case BW_27G:
	  	str_clk = 270;
		break;
      case BW_162G:
	  	str_clk = 162;
		break;
	default:
		str_clk = 540;
		break;
	  
    }


    SP_TX_Read_Reg(SP_TX_PORT0_ADDR,M_VID_2, &c);
    M_val = c * 0x10000;
    SP_TX_Read_Reg(SP_TX_PORT0_ADDR,M_VID_1, &c);
    M_val = M_val + c * 0x100;
    SP_TX_Read_Reg(SP_TX_PORT0_ADDR,M_VID_0, &c);
    M_val = M_val + c;

    SP_TX_Read_Reg(SP_TX_PORT0_ADDR,N_VID_2, &c);
    N_val = c * 0x10000;
    SP_TX_Read_Reg(SP_TX_PORT0_ADDR,N_VID_1, &c);
    N_val = N_val + c * 0x100;
    SP_TX_Read_Reg(SP_TX_PORT0_ADDR,N_VID_0, &c);
    N_val = N_val + c;

    str_clk = str_clk * M_val;
    pclk = str_clk ;
    pclk = pclk / N_val;
}


void SP_TX_Show_Infomation(void)
{
	BYTE c,c1;
	WORD h_res,h_act,v_res,v_act;
	WORD h_fp,h_sw,h_bp,v_fp,v_sw,v_bp;
	unsigned long fresh_rate;

	debug_puts("\n*************************SP Video Information*************************\n");
	if(BIST_EN)
		debug_printf("   SP TX mode = BIST mode\n");
	else
		debug_printf("   SP TX mode = Normal mode");



	SP_TX_Read_Reg(SP_TX_PORT0_ADDR,SP_TX_LANE_COUNT_SET_REG, &c);
	if(c==0x00)
		debug_printf("   LC = 1");


    SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_ANALOG_PWD_CTRL1, &c);
    if(c&0x10)
    {
        SP_TX_Read_Reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG, &c);
        if(c==0x06)
        {
            debug_printf("   BW = 1.62G");
            SP_TX_PCLK_Calc(BW_162G);//str_clk = 162;
        }
        else if(c==0x0a)
        {
            debug_printf("   BW = 2.7G");
            SP_TX_PCLK_Calc(BW_27G);//str_clk = 270;
        }
        else if(c==0x14)
        {
            debug_printf("   BW = 5.4G");
            SP_TX_PCLK_Calc(BW_54G);//str_clk = 540;
        }
    
    }
    else
    {
        SP_TX_Read_Reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG, &c);
        if(c==0x06)
        {
            debug_printf("   BW = 1.62G");
        }
        else if(c==0x0a)
        {
            debug_printf("   BW = 2.7G");
        }
        else if(c==0x14)
        {
            debug_printf("   BW = 5.4G");
        }
    
    }
	
	

	if(SSC_EN)
		debug_puts("   SSC On");
	else
		debug_puts("   SSC Off");

	debug_printf("   M = %lu, N = %lu, PCLK = %.2x MHz\n",M_val,N_val,(WORD)pclk);

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_H,&c1);

	v_res = c1;
	v_res = v_res << 8;
	v_res = v_res + c;


	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_H,&c1);

	v_act = c1;
	v_act = v_act << 8;
	v_act = v_act + c;


	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_H,&c1);

	h_res = c1;
	h_res = h_res << 8;
	h_res = h_res + c;


	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_H,&c1);

	h_act = c1;
	h_act = h_act << 8;
	h_act = h_act + c;

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_H_F_PORCH_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_H_F_PORCH_STA_H,&c1);
	
	h_fp = c1;
	h_fp = h_fp << 8;
	h_fp = h_fp + c;

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_H_SYNC_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_H_SYNC_STA_H,&c1);
	
	h_sw = c1;
	h_sw = h_sw << 8;
	h_sw = h_sw + c;

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_H_B_PORCH_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_H_B_PORCH_STA_H,&c1);
	
	h_bp = c1;
	h_bp = h_bp << 8;
	h_bp = h_bp + c;

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_V_F_PORCH_STA,&c);
	v_fp = c;

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_V_SYNC_STA,&c);
	v_sw = c;

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_V_B_PORCH_STA,&c);
	v_bp = c;
	
	debug_printf("   Total resolution is %d * %d \n", h_res, v_res);
	
	debug_printf("   HF=%d, HSW=%d, HBP=%d\n", h_fp, h_sw, h_bp);
	debug_printf("   VF=%d, VSW=%d, VBP=%d\n", v_fp, v_sw, v_bp);
	debug_printf("   Active resolution is %d * %d ", h_act, v_act);
	

/*
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINE_STA_H,&c1);

	v_res = c1;
	v_res = v_res << 8;
	v_res = v_res + c;


	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_H,&c1);

	v_act = c1;
	v_act = v_act << 8;
	v_act = v_act + c;


	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXEL_STA_H,&c1);

	h_res = c1;
	h_res = h_res << 8;
	h_res = h_res + c;


	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_L,&c);
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_H,&c1);

	h_act = c1;
	h_act = h_act << 8;
	h_act = h_act + c;

	debug_printf("   Total resolution is %d * %d \n", h_res, v_res);
	debug_printf("   Active resolution is %d * %d ", h_act, v_act);
*/
	{
	fresh_rate = pclk * 1000;
	fresh_rate = fresh_rate / h_res;
	fresh_rate = fresh_rate * 1000;
	fresh_rate = fresh_rate / v_res;
	//debug_printf(" @ %.2fHz\n", fresh_rate);
	}

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL,&c);
	if((c & 0x06) == 0x00)
	debug_printf("   ColorSpace: RGB,");
	else if((c & 0x06) == 0x02)
	debug_printf("   ColorSpace: YCbCr422,");
	else if((c & 0x06) == 0x04)
	debug_printf("   ColorSpace: YCbCr444,");


	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL,&c);
	if((c & 0xe0) == 0x00)
	debug_puts("  6 BPC");
	else if((c & 0xe0) == 0x20)
	debug_puts("  8 BPC");
	else if((c & 0xe0) == 0x40)
	debug_puts("  10 BPC");
	else if((c & 0xe0) == 0x60)
	debug_puts("  12 BPC");

#ifdef ANX7730
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x05, 0x23, 1, ByteBuf);
	if((ByteBuf[0]&0x0f)!=0x02)
		{
			debug_printf("   ANX7730 BB current FW Ver %.2x \n", (WORD)(ByteBuf[0]&0x0f));
			debug_puts("   It can be updated to the latest version 02 with the command:\update7730 ");
		}
	else
		debug_puts("   ANX7730 BB current FW is the latest version 02.");
		
#endif

	debug_puts("\n********************************************************************\n");

}

void SP_TX_AUX_WR (BYTE offset)
{
	BYTE c,cnt;
	cnt = 0;
    //load offset to fifo
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, offset);
    //set I2C write com 0x04 mot = 1
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
	//enable aux
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
    SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
    while(c&0x01)
    {
       delay_ms(10);
        cnt ++;
        //debug_printf("cntwr = %.2x\n",(WORD)cnt);
        if(cnt == 10)
        {
            debug_puts("write break");
            //SP_TX_RST_AUX();
            cnt = 0;
            bEDIDBreak=1;
            break;
        }
        SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
    }

} 

void SP_TX_AUX_RD (BYTE len_cmd)
{
	BYTE c,cnt;
	cnt = 0;
	
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, len_cmd);
	//enable aux
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while(c & 0x01)
	{
		delay_ms(10);
		cnt ++;
        //debug_printf("cntrd = %.2x\n",(WORD)cnt);
		if(cnt == 10)
		{
			debug_puts("read break");
			SP_TX_RST_AUX();
                     bEDIDBreak=1;
			break;
		}
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	}

}



void SP_TX_Insert_Err(void)
{
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_DEBUG_REG, 0x02);
	debug_puts("Insert err\n");
}

BYTE SP_TX_Chip_Located(void)
{
    BYTE c1,c2,c3;
    SP_TX_Hardware_PowerOn();
    
    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDL_REG , &c1);
    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_DEV_IDH_REG , &c2);
    SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_DEV_REV_REG , &c3);
    if ((c1==0x05) && (c2==0x78)&&(c3==0xca))
    {
        debug_puts("ANX7805 Reversion CA\n");
        return 1;
    }
    else
    {
    	debug_printf("dev IDL = %.2x, deb IDH = %.2x, REV= %.2x\n",(WORD)c1,(WORD)c2,(WORD)c3);
        return 0;
    }
}


void SP_TX_Hardware_PowerOn(void) 
{
	if(!hardware_power_ctl(1))
		debug_puts("hardware poweron func is not found \n");
	else
		debug_puts("Chip is power on\n");
}

void SP_TX_Hardware_PowerDown(void) 
{
	if(!hardware_power_ctl(0))
		debug_puts("hardware poweroff func is not found \n");
	else
		debug_printf("Chip is power down\n");
	
}
void vbus_power_ctrl(BYTE ON)
{
	BYTE c, i;
	if(ON == 0) {
		SP_TX_Read_Reg (SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, (c& (~V33_SWITCH_ON))); 
	    //Power down  5V detect and short portect circuit
		SP_TX_Read_Reg (SP_TX_PORT2_ADDR, PLL_FILTER_CTRL6, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL6, c|0x30); 
		debug_puts("3.3V output disabled");
	}
	else {
		for (i = 0; i < 5; i++) {
		    //Power up  5V detect and short portect circuit
		    	sp_write_reg_mask(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL6, ~0x30, 0x00);
		    // Enable power 3.3v out
			SP_TX_Read_Reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, &c);
			c &= ~V33_SWITCH_ON;
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, c);
			c |= V33_SWITCH_ON;
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, PLL_FILTER_CTRL1, c);
			
			delay_ms(100);
			SP_TX_Read_Reg (SP_TX_PORT2_ADDR, PLL_FILTER_CTRL6, &c);
			if (!(c & 0xc0)) {
				debug_puts("3.3V output enabled\n");
				break;
			}else{
				debug_puts("VBUS power can not be supplied\n");
			}
		}

	}
	
	
}


/*
void SP_TX_Hardware_Reset(void) 
{
    SP_TX_HW_RESET= 0;
    delay_ms(20);
    SP_TX_HW_RESET = 1;
    delay_ms(10);
}
*/

void SP_TX_CONFIG_SSC(SP_LINK_BW linkbw) 
{
	BYTE c;

	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, 0x00); 			// disable SSC first
	//SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, 0x00);		//disable speed first

	
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00,DPCD_MAX_DOWNSPREAD,1,&c);
          

#ifndef SSC_1
	//debug_puts("############### Config SSC 0.4% ####################");
	if(linkbw == BW_54G) 
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0xc0);	              // set value according to mehran CTS report -ANX.Fei-20111009
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);	              
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x75);			// ctrl_th 
	
	}
	else if(linkbw == BW_27G) 
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0x5f);			//  set value according to mehran CTS report -ANX.Fei-20111009
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);	
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x75);			// ctrl_th 
	}
	else 
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0x9e);         	//  set value according to mehran CTS report -ANX.Fei-20111009
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);	
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x6d);			// ctrl_th 
	}
#else
      //debug_puts("############### Config SSC 1% ####################");
	if(linkbw == BW_54G) 
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0xdd);	              // ssc d  1%, f0/8 mode
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x01);	              
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x76);			// ctrl_th 
	}
	else if(linkbw == BW_27G) 
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0xef);			// ssc d  1%, f0/4 mode
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x00);	
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x76);			// ctrl_th 
	}
	else 
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL1, 0x8e);			// ssc d 0.4%, f0/4 mode
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, 0x01);	
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL3, 0x6d);			// ctrl_th 
	}
#endif

	//Enable SSC
	SP_TX_SPREAD_Enable(1);


}


void SP_TX_Config_Audio_BIST(struct AudioFormat *bAudioFormat) 
{
       BYTE c;
	   
	debug_puts("############## Config BIST audio #####################\n");
	// config audio bist status 
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status1, 0x00 ); // configure channel status1
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status2, 0x00); // configure channel status2
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status3, 0x00); // configure channel status3
	c = bAudioFormat->bAudio_Fs;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status4, c ); // configure channel status4
	c = bAudioFormat->bAudio_word_len;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status5, c ); // configure channel status5             

	// Disable SPDIF input
	SP_TX_Read_Reg (SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, (c & 0x7e )|0x01); 

	// disable  I2S input
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL, 0x00 ); 
	
	// enable AUDIO bist
	SP_TX_Read_Reg (SP_TX_PORT2_ADDR, AUDIO_BIST_CTRL, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, AUDIO_BIST_CTRL, (c | 0xf1) );   // max sin amp 
}

void SP_TX_Config_Audio_SPDIF(void) 
{
       BYTE c;

       debug_puts("############## Config SPDIF audio #####################\n");

	// disable  I2S input
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL, 0x00 ); 
	   
	SP_TX_Read_Reg (SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, (c | SPDIF_AUDIO_CTRL0_SPDIF_IN) ); // enable SPDIF input

	delay_ms(2);

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_STATUS0, &c);

	if ( ( c & SPDIF_AUDIO_STATUS0_CLK_DET ) != 0 ) 
		debug_puts("SPDIF Clock is detected!\n");
	else 
		debug_puts("ERR:SPDIF Clock is Not detected!\n");

	if ( ( c & SPDIF_AUDIO_STATUS0_AUD_DET ) != 0 ) 
		debug_puts("SPDIF Audio is detected!\n");
	else 
		debug_puts("ERR:SPDIF Audio is Not detected!\n");
}

void SP_TX_Config_Audio_I2S(struct AudioFormat *bAudioFormat) 
{
     BYTE c;

	 // Disable SPDIF input
	SP_TX_Read_Reg (SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, ((c & 0x7e )|0x01)); 
	
       debug_puts("############## Config I2S audio #####################\n");
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL,&c); // enable I2S input
       c = (c&~0xff) | (bAudioFormat->bI2S_FORMAT.SHIFT_CTRL<<3) |bAudioFormat->bI2S_FORMAT.DIR_CTRL <<2 
	   	| (bAudioFormat->bI2S_FORMAT.WS_POL <<1) | (bAudioFormat->bI2S_FORMAT.JUST_CTRL);
	 switch(bAudioFormat->bI2S_FORMAT.Channel_Num)
	 {
		case I2S_CH_2: 
			c = c|0x10;
			break;
		case I2S_CH_4:
			c = c|0x30;
			break;
		case I2S_CH_6: 
			c = c|0x70;
			break;
		case I2S_CH_8:
			c = c|0xf0;
			break;
		default: 
			c = c|0x10;
			break;
	 }
       SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL,c); // enable I2S input

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL,&c); // select I2S clock as audio reference clock-2011.9.9-ANX.Fei
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL,c|0x06); 

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_FMT,&c); // configure I2S format
       c =  (c&~0xe5)| (bAudioFormat->bI2S_FORMAT.EXT_VUCP <<2) | (bAudioFormat->bI2S_FORMAT.AUDIO_LAYOUT) 
	   	| (bAudioFormat->bI2S_FORMAT.Channel_Num << 5);
       SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_FMT,c); // configure I2S format


	c = bAudioFormat->bI2S_FORMAT.Channel_status1;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status1, c ); // configure I2S channel status1
	c = bAudioFormat->bI2S_FORMAT.Channel_status2;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status2, c ); // configure I2S channel status2
	c = bAudioFormat->bI2S_FORMAT.Channel_status3;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status3, c ); // configure I2S channel status3
	c = bAudioFormat->bI2S_FORMAT.Channel_status4;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status4, c ); // configure I2S channel status4
	c = bAudioFormat->bI2S_FORMAT.Channel_status5;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CH_Status5, c ); // configure I2S channel status5

}
void SP_TX_Config_Audio_Slimbus(struct AudioFormat *bAudioFormat)
{
 //TO DO
	//bAudioFormat = bAudioFormat;

	BYTE c;

       debug_puts("############## Config Slimbus #####################\n");


	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_DEV0_REG0, 0x04 ); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_DEV0_REG1, 0x08 ); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_DEV0_REG2, 0xbb ); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_DEV0_REG3, 0xbb); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_DEV0_REG4, 0xaa ); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_DEV0_REG5, 0xaa ); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_INTERFACE_DEV_REG0, 0x04 ); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_INTERFACE_DEV_REG1, 0x09 ); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_INTERFACE_DEV_REG2, 0xbb); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_INTERFACE_DEV_REG3, 0xbb ); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_INTERFACE_DEV_REG4, 0xaa); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_ENUMERATION_ADD_INTERFACE_DEV_REG5, 0xaa ); 

	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_INITIAL_M_AUD_0, 0x0a ); 
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_INITIAL_M_AUD_1, 0xab ); 

   
	SP_TX_Read_Reg (MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG0, &c);
	if(c & 0x80)
	{
		SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG0, c&0x7f); 

	}

	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, 0x0 ); 


	SP_TX_Read_Reg (MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG2, &c);
	 switch(bAudioFormat->bI2S_FORMAT.Channel_Num)
	 {
		case I2S_CH_2: 
			c = c|0x08;
			break;
		case I2S_CH_4:
			c = c|0x1c;
			break;
		case I2S_CH_6: 
			c = c|0x2c;
			break;
		case I2S_CH_8:
			c = c|0x3c;
			break;
		default: 
			c = c|0x08;
			break;
	 }
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG2, c); 
	
	//liujinggang add 20120511 end


	SP_TX_Read_Reg (MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG0, &c);
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG0, c | 0x20); 
	
	SP_TX_Read_Reg (MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG1, &c);
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG1, c | 0x08); 

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_FMT,&c); 
	 switch(bAudioFormat->bI2S_FORMAT.Channel_Num)
	 {
		case I2S_CH_2: 
			c = c|0x20;
			break;
		case I2S_CH_4:
			c = c|0x61;
			break;
		case I2S_CH_6: 
			c = c|0xa1;
			break;
		case I2S_CH_8:
			c = c|0xe1;
			break;
		default: 
			c = c|0x20;
			break;
	 }
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_FMT,c);
 
}


void SP_TX_Enable_Audio_Output(BYTE bEnable)
{
	BYTE c;

	SP_TX_Read_Reg (SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL, &c);
	if(bEnable)
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL, ( c| SP_TX_AUD_CTRL_AUD_EN ) ); // enable SP audio
		
		SP_TX_InfoFrameUpdate(&SP_TX_AudioInfoFrmae);
	}
	else
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUD_CTRL, (c &(~SP_TX_AUD_CTRL_AUD_EN))); // Disable SP audio

		
		SP_TX_Read_Reg (SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, ( c&(~SP_TX_PKT_AUD_EN )) ); // Disable the audio info-frame
	}
    

}

void SP_TX_Disable_Audio_Input(void)
{
	BYTE c;

    	SP_TX_Read_Reg (SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, &c);
       SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0, (c &(~SPDIF_AUDIO_CTRL0_SPDIF_IN))); // Disable SPDIF

       // disable  I2S input
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_I2S_CTRL, 0x00 ); 	

}


void SP_TX_AudioInfoFrameSetup(struct AudioFormat *bAudioFormat)
{
	
	SP_TX_AudioInfoFrmae.type = 0x84;
	SP_TX_AudioInfoFrmae.version = 0x01;
	SP_TX_AudioInfoFrmae.length = 0x0A;

	if(bAudioFormat ->bAudioType ==AUDIO_I2S)
	{
		switch(bAudioFormat->bI2S_FORMAT.Channel_Num)
		{
			case I2S_CH_2: 
				SP_TX_AudioInfoFrmae.pb_byte[0]=0x00;//coding type ,refer to stream header, audio channel count,two channel
				SP_TX_AudioInfoFrmae.pb_byte[3]=0x00;//for multi channel LPCM
				break;
			case I2S_CH_4:
				SP_TX_AudioInfoFrmae.pb_byte[0]=0x03;
				SP_TX_AudioInfoFrmae.pb_byte[3]=0x08;
				break;
			case I2S_CH_6: 
				SP_TX_AudioInfoFrmae.pb_byte[0]=0x05;
				SP_TX_AudioInfoFrmae.pb_byte[3]=0x0b;
				break;
			case I2S_CH_8:
				SP_TX_AudioInfoFrmae.pb_byte[0]=0x07;
				SP_TX_AudioInfoFrmae.pb_byte[3]=0x13;
				break;
			default: 
				break;
		}
	
	}else
	{

		SP_TX_AudioInfoFrmae.pb_byte[0]=0x00;//coding type ,refer to stream header, audio channel count,two channel
		SP_TX_AudioInfoFrmae.pb_byte[3]=0x00;//for multi channel LPCM
	}


	//SP_TX_AudioInfoFrmae.pb_byte[0]=0x00;//coding type ,refer to stream header, audio channel count,two channel
	SP_TX_AudioInfoFrmae.pb_byte[1]=0x00;//refer to stream header
	SP_TX_AudioInfoFrmae.pb_byte[2]=0x00;
	//SP_TX_AudioInfoFrmae.pb_byte[3]=0x00;//for multi channel LPCM
	SP_TX_AudioInfoFrmae.pb_byte[4]=0x00;//for multi channel LPCM
	SP_TX_AudioInfoFrmae.pb_byte[5]=0x00;//reserved to 0
	SP_TX_AudioInfoFrmae.pb_byte[6]=0x00;//reserved to 0
	SP_TX_AudioInfoFrmae.pb_byte[7]=0x00;//reserved to 0
	SP_TX_AudioInfoFrmae.pb_byte[8]=0x00;//reserved to 0
	SP_TX_AudioInfoFrmae.pb_byte[9]=0x00;//reserved to 0
}

void SP_TX_InfoFrameUpdate(struct AudiInfoframe* pAudioInfoFrame)
{
	BYTE c;

	c = pAudioInfoFrame->type;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_TYPE, c); // Audio infoframe

	
	c = pAudioInfoFrame->version;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_VER,	c);

	c = pAudioInfoFrame->length;
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_LEN,	c);

	c = pAudioInfoFrame->pb_byte[0];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB1,c);
	
	c = pAudioInfoFrame->pb_byte[1];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB2,c);

	c = pAudioInfoFrame->pb_byte[2];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB3,c);

	c = pAudioInfoFrame->pb_byte[3];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB4,c);

	c = pAudioInfoFrame->pb_byte[4];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB5,c);

	c = pAudioInfoFrame->pb_byte[5];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB6,c);

	c = pAudioInfoFrame->pb_byte[6];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB7,c);

	c = pAudioInfoFrame->pb_byte[7];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB8,c);

	c = pAudioInfoFrame->pb_byte[8];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB9,c);

	c = pAudioInfoFrame->pb_byte[9];
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AUD_DB10,c);
	

	SP_TX_Read_Reg (SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, ( c | SP_TX_PKT_AUD_UP ) ); // update the audio info-frame

		
	SP_TX_Read_Reg (SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, ( c | SP_TX_PKT_AUD_EN ) ); // enable the audio info-frame
}
	

void SP_TX_Config_Audio(struct AudioFormat *bAudio) 
{
	BYTE c;
	debug_puts("############## Config audio #####################");

       SP_TX_Power_Enable(SP_TX_PWR_AUDIO, POWER_ON);
       
       switch(bAudio->bAudioType)
       {
       case AUDIO_I2S:
	   	SP_TX_Config_Audio_I2S(bAudio);
		break;
	case AUDIO_SPDIF:
	   	SP_TX_Config_Audio_SPDIF();
		break;
	case AUDIO_BIST:
		SP_TX_Config_Audio_BIST(bAudio);
		break;
	case AUDIO_SLIMBUS:
		SP_TX_Config_Audio_Slimbus(bAudio);
		break;
	default:
		debug_puts("ERR:Illegal audio format.\n");
		break;
       }

	// write audio info-frame
	SP_TX_AudioInfoFrameSetup(bAudio);
	SP_TX_Enable_Audio_Output(1);//enable audio
	
       SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_MASK1, c|0x04);//Unmask audio clock change int
       SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);

}



void SP_TX_RST_AUX(void)
{
	BYTE c,c1;

	//debug_puts("reset aux");

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c1);
	c = c1;
	c1&=0xdd;//clear HPD polling and Transmitter polling
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, c1); //disable  polling  before reset AUX-ANX.Fei-2011.9.19 
	
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, &c1);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c1|SP_TX_AUX_RST);
	delay_ms(1);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c1& (~SP_TX_AUX_RST));

	//set original polling enable
	//SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c1);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, c); //enable  polling  after reset AUX-ANX.Fei-2011.9.19 
}


BYTE SP_TX_AUX_DPCDRead_Bytes(BYTE addrh, BYTE addrm, BYTE addrl,BYTE cCount,BYTE * pBuf)
{
	BYTE c,i;
	BYTE bOK;
	//BYTE c1;

	//clr buffer
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, 0x80);

	//set read cmd and count
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, ((cCount-1) <<4)|0x09);


	//set aux address15:0
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, addrl);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, addrm);

	//set address19:16 and enable aux
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, (c & 0xf0) | addrh);


	//Enable Aux
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c | 0x01);


	delay_ms(2);

	bOK = SP_TX_Wait_AUX_Finished();

	if(!bOK)
    	{
#ifdef AUX_DBG
        	debug_puts("aux read failed");
#endif
		if(SP_TX_HDCP_AUTHENTICATION != get_system_state())
		SP_TX_RST_AUX();
        	return AUX_ERR;
    	}

	for(i =0;i<cCount;i++)
	{
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG+i, &c);

		*(pBuf+i) = c;

		if(i >= MAX_BUF_CNT)
			break;
	}

	return AUX_OK;
	
}


BYTE SP_TX_AUX_DPCDWrite_Bytes(BYTE addrh, BYTE addrm, BYTE addrl,BYTE cCount,BYTE * pBuf)
{
	BYTE c,i;
	BYTE bOK;
	
	//clr buffer
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, 0x80);

	//set write cmd and count;
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, ((cCount-1) <<4)|0x08);

	//set aux address15:0
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, addrl);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, addrm);

	//set address19:16
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, (c & 0xf0) | addrh);


	//write data to buffer
	for(i =0;i<cCount;i++)
	{
		c = *pBuf;
		pBuf++;
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG+i, c);

		if(i >= MAX_BUF_CNT)
			break;
	}

	//Enable Aux
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, c | 0x01);

	bOK = SP_TX_Wait_AUX_Finished();

	if(bOK)
		return AUX_OK;
	else
	{
#ifdef AUX_DBG
        	debug_puts("aux write failed");
#endif
		//SP_TX_RST_AUX();
		return AUX_ERR;
	}

}

BYTE SP_TX_AUX_DPCDWrite_Byte(BYTE addrh, BYTE addrm, BYTE addrl, BYTE data1)
{
	return SP_TX_AUX_DPCDWrite_Bytes(addrh, addrm, addrl, 1, &data1);
}



BYTE SP_TX_Wait_AUX_Finished(void)
{
	BYTE c;
	BYTE cCnt;
	cCnt = 0;
	
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_STATUS, &c);
	while(c & 0x10)
	{
		cCnt++;
               
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_STATUS, &c);

		if(cCnt>100)
                 {
 #ifdef AUX_DBG
                   debug_puts("AUX Operaton does not finished, and tome out.");
#endif
                   break; 
                 }
			
	}

    if(c&0x0F)
    {
  #ifdef AUX_DBG
        debug_printf("aux operation failed %.2x\n",(WORD)c);
  #endif
        return 0;
    }
    else
        return 1; //succeed

	//SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_STATUS, &c);
	//if(c&0x0f !=0)
	//debug_printf("**AUX Access error code = %.2x***\n",(WORD)c);
	
}


/*
void SP_TX_SW_Reset(void)
{
	BYTE c;

	//software reset    
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c | SP_TX_RST_SW_RST);
	delay_ms(10);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL_REG, c & ~SP_TX_RST_SW_RST);
}

*/


void SP_TX_SPREAD_Enable(BYTE bEnable)
{
	BYTE c;

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, &c);
	
	if(bEnable)
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, c | SPREAD_AMP);// enable SSC
		//SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, (c &(~0x04)));// powerdown SSC

		//reset SSC
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, &c);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c | SP_TX_RST_SSC);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_RST_CTRL2_REG, c & (~SP_TX_RST_SSC));
                    
		//enable the DPCD SSC
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x01, DPCD_DOWNSPREAD_CTRL,1,&c);
              SP_TX_AUX_DPCDWrite_Byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, (c | 0x10));

	}
	else
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SSC_CTRL_REG1, (c & (~SPREAD_AMP)));// disable SSC
              //SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_DOWN_SPREADING_CTRL2, c|0x04);// powerdown SSC
		//disable the DPCD SSC
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x01, DPCD_DOWNSPREAD_CTRL,1,&c);
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x01, DPCD_DOWNSPREAD_CTRL, (c & 0xef));
	}
		
}



void SP_TX_Get_Int_status(INTStatus IntIndex, BYTE *cStatus)
{
	BYTE c;

	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1 + IntIndex, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_COMMON_INT_STATUS1 + IntIndex, c);

	*cStatus = c;
}

/*
void SP_TX_Get_HPD_status( BYTE *cStatus)
{
	BYTE c;
	
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);

	*cStatus = c;
}
*/

BYTE SP_TX_Get_PLL_Lock_Status(void)
{
	BYTE c;
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_DEBUG_REG1, &c);
	return (c & SP_TX_DEBUG_PLL_LOCK) == SP_TX_DEBUG_PLL_LOCK ? 1 : 0;
}




void SP_TX_HDCP_ReAuth(void)
{	
	BYTE c;
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, &c);
	c |= 0x20;
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c);
	c &= ~(0x20);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_CONTROL_0_REG, c);
}


void SP_TX_Lanes_PWR_Ctrl(ANALOG_PWD_BLOCK eBlock, BYTE powerdown)
{
	BYTE c;

	switch(eBlock)
	{
			
		case CH0_BLOCK:
			if(powerdown)
			{
				SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
				c|=SP_TX_ANALOG_POWER_DOWN_CH0_PD;
				SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);				
			}
			else
			{
				SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
				c&=~SP_TX_ANALOG_POWER_DOWN_CH0_PD;
				SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);
			}

			break;
			
		case CH1_BLOCK:
			if(powerdown)
			{
				SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
				c|=SP_TX_ANALOG_POWER_DOWN_CH1_PD;
				SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);				
			}
			else
			{
				SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, &c);
				c&=~SP_TX_ANALOG_POWER_DOWN_CH1_PD;
				SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_ANALOG_POWER_DOWN_REG, c);
			}

			break;
			
		default:
			break;
	}
}

/*

void SP_TX_Get_Rx_LaneCount(BYTE bMax,BYTE *cLaneCnt)
{
	if(bMax)
	    SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00,DPCD_MAX_LANE_COUNT,1,cLaneCnt);
	else
	    SP_TX_AUX_DPCDRead_Bytes(0x00, 0x01,DPCD_LANE_COUNT_SET,1,cLaneCnt);
}


void SP_TX_Set_Rx_laneCount(BYTE cLaneCnt)
{
	SP_TX_AUX_DPCDWrite_Byte(0x00, 0x00, DPCD_LANE_COUNT_SET, cLaneCnt);
}
*/

void SP_TX_Get_Rx_BW(BYTE bMax,BYTE *cBw)
{
	if(bMax)
	   SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00,DPCD_MAX_LINK_RATE,1,cBw);
	else
	    SP_TX_AUX_DPCDRead_Bytes(0x00, 0x01,DPCD_LINK_BW_SET,1,cBw);
}

/*
void SP_TX_Set_Rx_BW(BYTE cBw)
{
	SP_TX_AUX_DPCDWrite_Byte(0x00, 0x01, DPCD_LINK_BW_SET, cBw);
}
*/



void SP_TX_Get_Link_BW(BYTE *bwtype)
{
	BYTE c;

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c);

	*bwtype = c;
}

/*
void SP_TX_Get_Lane_Count(BYTE *count)
{
	BYTE c;

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_LANE_COUNT_SET_REG, &c);

	*count = c;

}
*/

void SP_TX_EDID_Read_Initial(void)
{
	BYTE c;

	//Set I2C address	
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, 0x50);
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_15_8_REG, 0);
    SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, &c);
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_19_16_REG, c & 0xf0);
}

BYTE SP_TX_AUX_EDIDRead_Byte(BYTE offset)
{
    BYTE c,i,edid[16],data_cnt,cnt,vsdbdata[4],VSDBaddr;
    BYTE bReturn=0;
    //debug_printf("***************************offset = %.2x\n", (unsigned int)offset);
    VSDBaddr= 0;
     vsdbdata[0] = 0;
    cnt = 0;
	   
    SP_TX_AUX_WR(offset);//offset 
    
    if((offset == 0x00) || (offset == 0x80))
	checksum = 0;
	
       SP_TX_AUX_RD(0xf5);//set I2C read com 0x05 mot = 1 and read 16 bytes
       
	data_cnt = 0;
	while(data_cnt < 16)
	{
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, &c);
		c = c & 0x1f;
		//debug_printf("cnt_d = %.2x\n",(WORD)c);
		if(c != 0)
		{
		    for( i = 0; i < c; i ++)
		    {
		        SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG + i, &edid[i + data_cnt]);
			 //debug_printf("edid[%.2x] = %.2x\n",(WORD)(i + offset),(WORD)edid[i + data_cnt]);
		        checksum = checksum + edid[i + data_cnt];
		    }
		}
		else
		{
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x01);
			//enable aux
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
			while(c & 0x01)
        	      {
	        		delay_ms(2);
	        		cnt ++;
	        		if(cnt == 10)
	        		{
	        			debug_puts("read break");
	        			SP_TX_RST_AUX();
	                           bEDIDBreak=1;
	        			    bReturn = 0x01;
	        		}
	                    SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
        	      }
             	     //debug_puts("cnt_d = 0, break");
	             sp_tx_edid_err_code = 0xff;
	             bReturn = 0x02;// for fixing bug leading to dead lock in loop "while(data_cnt < 16)"
	             return bReturn;
		}
		data_cnt = data_cnt + c;
		if(data_cnt < 16)// 080610. solution for handle case ACK + M byte
		{
			
			//SP_TX_AUX_WR(offset);
			SP_TX_RST_AUX();
                     delay_ms(10);

			c = 0x05 | ((0x0f - data_cnt) << 4);//Read MOT = 1
			SP_TX_AUX_RD(c);
			//debug_puts("M < 16");
		}
	}

       SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x01);
	//enable aux
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only to stop EDID reading
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while(c & 0x01)
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	//debug_printf("***************************offset %.2x reading completed\n", (unsigned int)offset);


	if(EDID_Print_Enable)
	{
		for(i=0;i<16;i++) 
		{
			if((i&0x0f)==0)
				debug_printf("\n edid: [%.2x]  %.2x  ", (unsigned int)offset, (unsigned int)edid[i]);
			else 
				debug_printf("%.2x  ", (unsigned int)edid[i]);

			if((i&0x0f)==0x0f)
				debug_printf("\n");
		}

	}

#if 0
	if(offset < 0x80) {
		for(i=0;i<16;i++)//record all 128 data in extsion block.
			bEDID_firstblock[offset+i]=edid[i];
	}else if(offset >= 0x80) {
		for(i=0;i<16;i++)//record all 128 data in extsion block.
			bEDID_extblock[offset-0x80+i]=edid[i];
	}

#else

    if(offset == 0x00)
    {
        if((edid[0] == 0) && (edid[7] == 0) && (edid[1] == 0xff) && (edid[2] == 0xff) && (edid[3] == 0xff)
            && (edid[4] == 0xff) && (edid[5] == 0xff) && (edid[6] == 0xff))
            debug_puts("Good EDID header!");
        else
        {
            debug_puts("Bad EDID header!");
            sp_tx_edid_err_code = 0x01;
        }
            
    }

    else if(offset == 0x30)
    {
        for(i = 0; i < 10; i ++ )
            SP_TX_EDID_PREFERRED[i] = edid[i + 6];//edid[0x36]~edid[0x3f]
    }

    else if(offset == 0x40)
    {
        for(i = 0; i < 8; i ++ )
            SP_TX_EDID_PREFERRED[10 + i] = edid[i];//edid[0x40]~edid[0x47]
    }

    else if(offset == 0x70)
    {
       checksum = checksum&0xff;
        checksum = checksum - edid[15];
        checksum = ~checksum + 1;
        if(checksum != edid[15])
        {
            debug_puts("Bad EDID check sum1!");
            sp_tx_edid_err_code = 0x02;
            checksum = edid[15];
        }
	 else
	 	debug_puts("Good EDID check sum1!");
    }
/*
    else if(offset == 0xf0)
    {
        checksum = checksum - edid[15];
        checksum = ~checksum + 1;
        if(checksum != edid[15])
        {
            debug_puts("Bad EDID check sum2!");
            sp_tx_edid_err_code = 0x02;
        }
	 else
	 	debug_puts("Good EDID check sum2!");
    }*/
    else if( (offset >= 0x80)&&(sp_tx_ds_edid_hdmi==0))
    {
       if(offset ==0x80)
       {
	       if(edid[0] !=0x02)
		   return 0x03;
       }
        for(i=0;i<16;i++)//record all 128 data in extsion block.
            EDIDExtBlock[offset-0x80+i]=edid[i];
/*
		for(i=0;i<16;i++) 
		{
			if((i&0x0f)==0)
				debug_printf("\n edid: [%.2x]  %.2x  ", (unsigned int)offset, (unsigned int)EDIDExtBlock[offset-0x80+i]);
			else 
				debug_printf("%.2x  ", (unsigned int)edid[i]);

			if((i&0x0f)==0x0f)
				debug_printf("\n");
		}*/
        
        if(offset ==0x80)
           DTDbeginAddr = edid[2];

        if(offset == 0xf0)
         {
		checksum = checksum - edid[15];
		checksum = ~checksum + 1;
		if(checksum != edid[15])
		{
			debug_puts("Bad EDID check sum2!");
			sp_tx_edid_err_code = 0x02;
		}
		else
			debug_puts("Good EDID check sum2!");
	 
            for(VSDBaddr = 0x04;VSDBaddr <DTDbeginAddr;)
            {
                //debug_printf("####VSDBaddr = %.2x\n",(WORD)(VSDBaddr));

                vsdbdata[0] = EDIDExtBlock[VSDBaddr];
                vsdbdata[1] = EDIDExtBlock[VSDBaddr+1];
                vsdbdata[2] = EDIDExtBlock[VSDBaddr+2];
                vsdbdata[3] = EDIDExtBlock[VSDBaddr+3];

               // debug_printf("vsdbdata= %.2x,%.2x,%.2x,%.2x\n",(WORD)vsdbdata[0],(WORD)vsdbdata[1],(WORD)vsdbdata[2],(WORD)vsdbdata[3]);
                if((vsdbdata[0]&0xe0)==0x60)
                {


			if((vsdbdata[0]&0x1f) > 0x08)
			{
				if((EDIDExtBlock[VSDBaddr+8]&0xc0)	== 0x80)
				{
					if((EDIDExtBlock[VSDBaddr+11]&0x80)	== 0x80)
					sp_tx_ds_edid_3d_present = 1;
					debug_puts("Downstream monitor supports 3D");
						
				}
				else if((EDIDExtBlock[VSDBaddr+8]&0xc0)	== 0x40)
				{
					if((EDIDExtBlock[VSDBaddr+11]&0x80)	== 0x80)
					sp_tx_ds_edid_3d_present = 1;
					debug_puts("Downstream monitor supports 3D");
						
				}
				else if((EDIDExtBlock[VSDBaddr+8]&0xc0)	== 0xc0)
				{
					if((EDIDExtBlock[VSDBaddr+13]&0x80)	== 0x80)
					sp_tx_ds_edid_3d_present = 1;
					debug_puts("Downstream monitor supports 3D");
						
				}
    				else if((EDIDExtBlock[VSDBaddr+8]&0xc0)	== 0x00)
				{     
					if((EDIDExtBlock[VSDBaddr+9]&0x80)	== 0x80)
					sp_tx_ds_edid_3d_present = 1;
					debug_puts("Downstream monitor supports 3D");
						
				}
				else
				{
					sp_tx_ds_edid_3d_present = 0;
					debug_puts("Downstream monitor does not support 3D");
				}
			
			
			}
							
                    if((vsdbdata[1]==0x03)&&(vsdbdata[2]==0x0c)&&(vsdbdata[3]==0x00))
                    {
                        sp_tx_ds_edid_hdmi = 1;
                        return 0;
                    }
                    else
                    {
                        sp_tx_ds_edid_hdmi = 0;
                        return 0x03;
                    }

                }	
                else
                {
                    sp_tx_ds_edid_hdmi = 0;
                    VSDBaddr = VSDBaddr+(vsdbdata[0]&0x1f);
                    VSDBaddr = VSDBaddr + 0x01;
                }
        
                if(VSDBaddr > DTDbeginAddr)
                    return 0x03;

            } 
         }
          
    }      
#endif
return bReturn;
}



void SP_TX_Parse_Segments_EDID(BYTE segment, BYTE offset)
{
	BYTE c,cnt;
	int i;

	//set I2C write com 0x04 mot = 1
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);

	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, 0x30);	

	// adress_only
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	
	//while(c & 0x01)
	//	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	SP_TX_Wait_AUX_Finished();
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, &c);

	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, segment);

	//set I2C write com 0x04 mot = 1
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
	//enable aux
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
	cnt = 0;
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

	while(c&0x01)
	{
		delay_ms(10);
		cnt ++;
		//debug_printf("cntwr = %.2x\n",(WORD)cnt);
		if(cnt == 10)
		{
			debug_puts("write break");
			SP_TX_RST_AUX();
			cnt = 0;
			bEDIDBreak=1;
		      	return;// bReturn;
		}
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	
	}

	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_ADDR_7_0_REG, 0x50);//set EDID addr 0xa0	
	// adress_only
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only

	SP_TX_AUX_WR(offset);//offset   
	//adress_only
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only

	SP_TX_AUX_RD(0xf5);//set I2C read com 0x05 mot = 1 and read 16 bytes    
       cnt = 0;
	for(i=0; i<16; i++)
	{
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, &c);
		while((c & 0x1f) == 0)
		{
			delay_ms(2);
			cnt ++;
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_COUNT_REG, &c);
			if(cnt == 10)
	        	{
				debug_puts("read break");
				SP_TX_RST_AUX();
				bEDIDBreak=1;
				return;
	        	}
		}


		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG+i, &c);
		//debug_printf("edid[0x%.2x] = 0x%.2x\n",(WORD)(offset+i),(WORD)c);
	} 

	///*
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x01);
	//enable aux
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x03);//set address only to stop EDID reading
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	while(c & 0x01)
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);

}





BYTE SP_TX_Get_EDID_Block(void)
{
	BYTE c;
       SP_TX_AUX_WR(0x00);
       SP_TX_AUX_RD(0x01);
       SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, &c);
       //debug_printf("[a0:00] = %.2x\n", (WORD)c);

       SP_TX_AUX_WR(0x7e);
       SP_TX_AUX_RD(0x01);
       SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_BUF_DATA_0_REG, &c);

	debug_printf("EDID Block = %d\n",(int)(c+1));
	return c;
    }




void SP_TX_AddrOnly_Set(BYTE bSet)
{
	BYTE c;

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, &c);
	if(bSet)
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, (c|SP_TX_ADDR_ONLY_BIT));
	}
	else
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, (c&~SP_TX_ADDR_ONLY_BIT));
	}	
}

/*
void SP_TX_Scramble_Enable(BYTE bEnabled)
{
	BYTE c;

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, &c);
	if(bEnabled)//enable scramble
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, (c&~SP_TX_SCRAMBLE_DISABLE));
	}
	else
	{
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, (c|SP_TX_SCRAMBLE_DISABLE));
	}

}
*/


void SP_TX_API_M_GEN_CLK_Select(BYTE bSpreading)
{
    BYTE c;

    SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_M_CALCU_CTRL, &c);    
    if(bSpreading)
    {
            //M value select, select clock with downspreading
            SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_M_CALCU_CTRL, (c | M_GEN_CLK_SEL));    		      
    }
    else
    {
            //M value select, initialed as clock without downspreading
            SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_M_CALCU_CTRL, c&(~M_GEN_CLK_SEL));
    }
}
#if(ENABLE_3D)
/*
	Function: sp_tx_send_3d_vsi_packet_to_7730
	Parameter:  
		video_fromat 
			0x00 framepacking
			0x01 field alternative
			0x02 line alternative
			0x06 top-and-bottom
*/
void sp_tx_send_3d_vsi_packet_to_7730(BYTE video_format) {

	BYTE i;
	for(i = 0; i < MPEG_PACKET_SIZE; i++) {
		SP_TX_Packet_MPEG.MPEG_data[i] = 0;
	}
	SP_TX_Packet_MPEG.MPEG_data[0] = 0x03;
	SP_TX_Packet_MPEG.MPEG_data[1] = 0x0C;
	SP_TX_Packet_MPEG.MPEG_data[2] = 0x00;
	SP_TX_Packet_MPEG.MPEG_data[3] = 0x40;
	SP_TX_Packet_MPEG.MPEG_data[4] =(BYTE) (video_format << 4 );
	
	SP_TX_Config_Packets(VSI_PACKETS);


	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_VSC_DB1, 0x04);

	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_3D_VSC_CTRL, 0xFF, INFO_FRAME_VSC_EN);
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, ~0x01, 0x00);
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, 0xFF, 0x10);
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, 0xFF, 0x01);
	
	
}
#endif

void SP_TX_Config_Packets(PACKETS_TYPE bType)
{
	BYTE c,c1;
	WORD h_act,v_act;

	switch(bType)
	{
		case AVI_PACKETS:
			//clear packet enable
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c&(~SP_TX_PKT_AVI_EN));
 
			//get input color space
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_VID_CTRL, &c);

			SP_TX_Packet_AVI.AVI_data[0] = SP_TX_Packet_AVI.AVI_data[0] & 0x9f;
			SP_TX_Packet_AVI.AVI_data[0] = SP_TX_Packet_AVI.AVI_data[0] | (c <<4);

                    //debug_printf("AVI 0 =%x\n", (WORD)SP_TX_Packet_AVI.AVI_data[0]);


			//set timing ID

			SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_L,&c);
			SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINE_STA_H,&c1);

			v_act = c1;
			v_act = v_act << 8;
			v_act = v_act + c;


			SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_L,&c);
			SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXEL_STA_H,&c1);

			h_act = c1;
			h_act = h_act << 8;
			h_act = h_act + c;

			if(((v_act > 470)&&(v_act < 490))&&((h_act > 710)&&(h_act < 730)))
				SP_TX_Packet_AVI.AVI_data[3] = 0x03; 
			else if (((v_act > 710)&&(v_act < 730))&&((h_act > 1270)&&(h_act < 1290)))
				SP_TX_Packet_AVI.AVI_data[3] = 0x04; 
			else if (((v_act > 1070)&&(v_act < 1090))&&((h_act > 1910)&&(h_act < 1930)))
				SP_TX_Packet_AVI.AVI_data[3] = 0x10; 
			else
				SP_TX_Packet_AVI.AVI_data[3] = 0x0; 

			//Set aspect ratio
			SP_TX_Packet_AVI.AVI_data[1] = SP_TX_Packet_AVI.AVI_data[1] & 0xcf;
			SP_TX_Packet_AVI.AVI_data[1] = SP_TX_Packet_AVI.AVI_data[1] |0x20;

			//Set  limited range
			//SP_TX_Packet_AVI.AVI_data[2] = SP_TX_Packet_AVI.AVI_data[2] & 0xf3;
			//SP_TX_Packet_AVI.AVI_data[2] = SP_TX_Packet_AVI.AVI_data[2] |0x04;


			SP_TX_Load_Packet(AVI_PACKETS);

			//send packet update
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_AVI_UD);

			//enable packet
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_AVI_EN);			
			
			break;
			
		case SPD_PACKETS:
			//clear packet enable
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c&(~SP_TX_PKT_SPD_EN));

			SP_TX_Load_Packet(SPD_PACKETS);

			//send packet update
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_SPD_UD);

			//enable packet
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_SPD_EN);	

			break;
		#if(ENABLE_3D)
			case VSI_PACKETS:
			//clear packet enable
			sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, (~SP_TX_PKT_MPEG_EN), 0x00);
			SP_TX_Load_Packet(VSI_PACKETS);
			//send packet update
			sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, 0xFF, SP_TX_PKT_MPEG_UD);
			//enable packet
			sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, 0xFF, SP_TX_PKT_MPEG_EN);

			break;
		#endif
		case MPEG_PACKETS:
			//clear packet enable
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c&(~SP_TX_PKT_MPEG_EN));

			SP_TX_Load_Packet(MPEG_PACKETS);

			//send packet update
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_MPEG_UD);

			//enable packet
			SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, &c);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_PKT_EN_REG, c | SP_TX_PKT_MPEG_EN);	

			break;

		default:
			break;
	}
	
}

void SP_TX_Load_Packet (PACKETS_TYPE type)
{
    BYTE i;
    
    switch(type)
    {
        case AVI_PACKETS:
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AVI_TYPE , 0x82);
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AVI_VER , 0x02);
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AVI_LEN , 0x0d);
			
            for(i=0;i<13;i++)
            {
                SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_AVI_DB0 + i, SP_TX_Packet_AVI.AVI_data[i]);                
            }
            break;

        case SPD_PACKETS:
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_SPD_TYPE , 0x83);
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_SPD_VER , 0x01);
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_SPD_LEN , 0x19);
            for(i=0;i<25;i++)
            {
                SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_SPD_DATA1 + i, SP_TX_Packet_SPD.SPD_data[i]);
            }
            break;
	#if(ENABLE_3D)
	case VSI_PACKETS:
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_TYPE, 0x81);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_VER, 0x01);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_LEN, 0x05);

		for (i = 0; i < 8; i++) {
			SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_DATA0 + i,
					SP_TX_Packet_MPEG.MPEG_data[i]);
		}
		break;
	#endif

        case MPEG_PACKETS:
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_TYPE , 0x85);
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_VER , 0x01);
            SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_LEN , 0x0a);
            for(i=0;i<10;i++)
            {
                SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_MPEG_DATA1 + i, SP_TX_Packet_MPEG.MPEG_data[i]);
            }
            break;
			
	default:
	    break;
    }
}
void SP_TX_AVI_Setup(void)
{
	SP_TX_Packet_AVI.AVI_data[0]=0x10;// Active video, color space RGB
	SP_TX_Packet_AVI.AVI_data[1]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[2]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[3]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[4]=0x00;//repeat 0
	SP_TX_Packet_AVI.AVI_data[5]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[6]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[7]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[8]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[9]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[10]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[11]=0x00;//reserved to 0
	SP_TX_Packet_AVI.AVI_data[12]=0x00;//reserved to 0

}

BYTE SP_TX_BW_LC_Sel(struct VideoFormat* pInputFormat)
{
    BYTE over_bw;
    int pixel_clk;

    over_bw = 0;
    pixel_clk = pclk;

    if(pInputFormat->bColordepth != COLOR_8_BIT)
	return 1;


       //debug_printf("pclk = %d\n",(WORD)pixel_clk);
	SP_TX_AUX_DPCDRead_Bytes(0x00,0x00,0x01,1,ByteBuf);
	sp_tx_bw = ByteBuf[0];
	//sp_tx_lane_count = ByteBuf[1] & 0x0f;

    
	if(pixel_clk <= 54)
	{
		sp_tx_bw = BW_162G;
		sp_tx_lane_count = 0x01;
	}
	else if((54 < pixel_clk) && (pixel_clk <= 90))
	{
		if(sp_tx_bw >= BW_27G)
		{
			sp_tx_bw = BW_27G;
			sp_tx_lane_count = 0x01;
		}
		else
		{
			over_bw = 1;
		}
	}
	else if((90 < pixel_clk) && (pixel_clk <= 180))
	{
		if(sp_tx_bw >= BW_54G)
		{
			sp_tx_bw = BW_54G;
			sp_tx_lane_count = 0x01;
		}
		else 
		{
			over_bw = 1;
		}
	}
	else
	{
		over_bw = 1;
	}
	/*
    if(over_bw)
        debug_puts("over bw!\n");
     else
	debug_printf("The optimized BW =%.2x, Lane cnt=%.2x\n",(WORD)sp_tx_bw,(WORD)sp_tx_lane_count);
	*/
     return over_bw;

}

#if(AUTO_TEST_CTS)
unsigned int sp_tx_link_err_check(void)
{
	unsigned int errl = 0, errh = 0;
	BYTE bytebuf[2];
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x10, 2, bytebuf);
	delay_ms(5);
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x10, 2, bytebuf);
	errh = bytebuf[1];

	if (errh & 0x80) {
		errl = bytebuf[0];
		errh = (errh & 0x7f) << 8;
		errl = errh + errl;
	}

	debug_printf(" Err of Lane = %d\n", errl);
	return errl;
}
#endif
BYTE sp_tx_lt_pre_config(void)
{
	BYTE legel_bw,legel_lc,c; 
	legel_bw = legel_lc = 1;

	SP_TX_Get_Rx_BW(1,&c);
	switch(c) {
		case 0x06:
			sp_tx_bw=BW_162G;
			break;
		case 0x0a:
			sp_tx_bw=BW_27G;
			break;
		case 0x14:
			sp_tx_bw=BW_54G;
			break;
		default:
			sp_tx_bw=BW_54G;
			break;
	}
	
	// if(sp_tx_lane_count>2)//ANX7805 supports 2 lanes max.
	SP_TX_Lanes_PWR_Ctrl(CH1_BLOCK, 1);
	
	#if(AUTO_TEST_CTS)
	if(sp_tx_test_lt) {
		sp_tx_test_lt = 0;
		sp_tx_bw = sp_tx_test_bw;
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, &c);
		c = (c & 0x8f);
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL2_REG, c);
	} else {
		struct VideoFormat InputFormattemp;
		InputFormattemp.bColordepth = COLOR_8_BIT;
		if (SP_TX_BW_LC_Sel(&InputFormattemp)) {
			#if(REDUCE_REPEAT_PRINT_INFO)
				loop_print_msg(0x05);
			#else
			    debug_puts("****Over bandwidth****");
			#endif
			return 1;
		}
		else
			debug_printf("pclk: %.2x, The optimized BW =%.2x, Lane cnt=%.2x \n",
				(WORD)pclk, (WORD)sp_tx_bw, (WORD)sp_tx_lane_count);
	}
	#endif

	/*Diable video before link training to enable idle pattern*/
	SP_TX_Enable_Audio_Output(0);	//Disable audio  output
	SP_TX_Disable_Audio_Input();  //Disable audio input
	sp_tx_enable_video_input(0);//Disable video input 
	
	if(SSC_EN)
		SP_TX_CONFIG_SSC(sp_tx_bw);
	else
		SP_TX_SPREAD_Enable(0);

	//set bandwidth
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, sp_tx_bw);
	//set lane conut
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_LANE_COUNT_SET_REG, sp_tx_lane_count);

	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_ANALOG_PD_REG, 0xFF, CH0_PD);
	delay_ms(2);
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_ANALOG_PD_REG, ~CH0_PD, 0x00);

	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PLL_CTRL_REG, 0xFF, PLL_RST);
	delay_ms(2);
	sp_write_reg_mask(SP_TX_PORT0_ADDR, SP_TX_PLL_CTRL_REG, ~PLL_RST, 0x00);
	return 0;
}

BYTE SP_TX_HW_Link_Training (void)
{
    
    BYTE c, return_value = 1;
	switch(sp_tx_link_training_state) {
		
		case LINK_TRAINING_INIT:
			sp_tx_link_training_state = LINK_TRAINING_PRE_CONFIG;
			break;	
		case LINK_TRAINING_PRE_CONFIG:
			if(sp_tx_lt_pre_config() == 0)
			sp_tx_link_training_state = LINK_TRAINING_START;
			break;

		case LINK_TRAINING_START:
			if(!SP_TX_Get_PLL_Lock_Status())
			{
				#if(REDUCE_REPEAT_PRINT_INFO)
					loop_print_msg(0x07);
				#else
					debug_puts("PLL not lock!");
				#endif
				break;
			}
			debug_puts("Hardware link training");
			SP_TX_EnhaceMode_Set();
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x06, 0x00, 0x01, &c);
			c |= 0x01;
			SP_TX_AUX_DPCDWrite_Bytes(0x00,0x06,0x00, 0x01, &c); //Set sink to D0 mode

			//SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_LANE0_SET_REG, 0x09);//link training from 400mv3.5db for ANX7730 B0-ANX.Fei-20111011 // by wsl 

			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_TRAINING_CTRL_REG, SP_TX_LINK_TRAINING_CTRL_EN);
			sp_tx_link_training_state = LINK_TRAINING_WAITTING_FINISH;

			break;

		case LINK_TRAINING_WAITTING_FINISH:	
			/*here : waitting interrupt to change training state.*/			
			break;		
		case LINK_TRAINING_ERROR:
			#if(REDUCE_REPEAT_PRINT_INFO)
				loop_print_msg(0x08);
			#else
				debug_puts("LINK_TRAINING_ERROR! \r\n");
			#endif
			sp_tx_link_training_state = LINK_TRAINING_INIT;
			break;
		case LINK_TRAINING_FINISH:
			 SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,0x02, 1, ByteBuf);
			//debug_printf(" ##DPCD 0x00202 = 0x%.2x##\n",(WORD)ByteBuf[0]);
			if(ByteBuf[0] != 0x07)
				sp_tx_link_training_state = LINK_TRAINING_ERROR;
			else {
				#if(AUTO_TEST_CTS)
				/* if there is link error, adjust pre-emphsis to check error again.
					If there is no error,keep the setting, otherwise use 400mv0db */
					if(!sp_tx_test_lt) {					
				   		c = 0x01;//DRVIE_CURRENT_LEVEL1;
						SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0xA3, c);

						if (sp_tx_link_err_check()) {
							c = 0x08 | 0x01;
							SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0xA3, c);

							if (sp_tx_link_err_check())
								SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0xA3, 0x01);
						}

						SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c);
						if (c != sp_tx_bw) {
							/*here can not replace using 
							sp_tx_link_training_state = LINK_TRAINING_ERROR; */
							 SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
							break;
						}
					}
				#endif
				return_value = 0;
				SP_TX_MIPI_CONFIG_Flag_Set(0);
				sp_tx_link_training_state = LINK_TRAINING_END;

				SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO_OUTPUT);

			}
			break;
		default:
		case LINK_TRAINING_END:	
			//when link training end, the state machine is keep here.
			break;
	}
	return return_value;

}
void SP_TX_Video_Mute(BYTE enable)
{
        BYTE c;
	if(enable)
	{
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c |=SP_TX_VID_CTRL1_VID_MUTE;
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG , c);
	}
	else
	{
		SP_TX_Read_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG, &c);
		c &=~SP_TX_VID_CTRL1_VID_MUTE;
		SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VID_CTRL1_REG , c);
	}
		

}


void SP_TX_Config_MIPI_Video_Format()
{
	unsigned long  M_vid;
	//long float lTemp;
	unsigned long lBW;
	unsigned long  l_M_Vid;

	BYTE bIndex;
	WORD MIPI_Format_data;
	
	BYTE c,c1,c2;
	lBW = 0;
	//clear force stream valid flag
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, c&0xfc);
	

	//Get BW
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c);
	if(c==0x06)
	{
		debug_puts("1.62G");
		lBW = 162;
	}
	else if(c==0x0a)
	{
		debug_puts("2.7G");
		lBW = 270;
	}
	else if(c==0x14)
	{
		debug_puts("5.4G");
		lBW = 540;
	}
	else {
		lBW = 540;
		debug_puts("invalid BW");
	}
	bIndex = MIPI_Format_Index_Get();
	
	M_vid = (unsigned long)((mipi_video_timing_table[bIndex].MIPI_pixel_frequency)*100);

	c = (unsigned char)M_vid;
	c1 = (unsigned char)(M_vid>>8);
	c2 = (unsigned char)(M_vid>>16);
	
	//debug_printf("m_vid h = %x,m_vid m = %x, mvid l= %x \n",(WORD)c2,(WORD)c1,(WORD)c);

	
	M_vid = M_vid*32768;

	c = (unsigned char)M_vid;
	c1 = (unsigned char)(M_vid>>8);
	c2 = (unsigned char)(M_vid>>16);
	
	//debug_printf("m_vid h = %x,m_vid m = %x, mvid l= %x \n",(WORD)c2,(WORD)c1,(WORD)c);
	
	M_vid=M_vid/(lBW*100);
	
	//debug_printf("m_vid = %x \n", M_vid);

	c = (unsigned char)M_vid;
	c1 = (unsigned char)(M_vid>>8);
	c2 = (unsigned char)(M_vid>>16);
	
	//debug_printf("m_vid h = %x,m_vid m = %x, mvid l= %x \n",(WORD)c2,(WORD)c1,(WORD)c);
	
	//M_vid = ((mipi_video_timing_table[bIndex].MIPI_pixel_frequency)*32768)/lBW;

	//debug_printf("m_vid = %x \n", M_vid);

	l_M_Vid =(unsigned long)M_vid;

	//debug_printf("m_vid1 = %x \n", l_M_Vid);

	//Set M_vid
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x20, c);
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x21, c1);
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x22, c2);

	/*
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x20, 0x75);
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x21, 0x27);
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x22, 0x0);
*/

	//Vtotal
	//MIPI_Format_data = (mipi_video_timing_table[bIndex].VTOTAL);
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_VTOTAL);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEL_REG, MIPI_Format_data);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_LINEH_REG, (MIPI_Format_data >> 8));

	//V active
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_VActive);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEL_REG, MIPI_Format_data);
    SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_LINEH_REG, (MIPI_Format_data >> 8));

	//V Front porch
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_V_Front_Porch);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VF_PORCH_REG, MIPI_Format_data);

	//V Sync width
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_V_Sync_Width);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VSYNC_CFG_REG, MIPI_Format_data);

	//V Back porch
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_V_Back_Porch);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_VB_PORCH_REG, MIPI_Format_data);

	//H total
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_HTOTAL);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELL_REG, MIPI_Format_data);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_TOTAL_PIXELH_REG, (MIPI_Format_data >> 8));

	//H active
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_HActive);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELL_REG, MIPI_Format_data);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_ACT_PIXELH_REG, (MIPI_Format_data >> 8));

	//H Front porch
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_H_Front_Porch);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHL_REG, MIPI_Format_data);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HF_PORCHH_REG, (MIPI_Format_data >> 8));

	//H Sync width
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_H_Sync_Width);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGL_REG, MIPI_Format_data);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HSYNC_CFGH_REG, (MIPI_Format_data >> 8));

	//H Back porch
	MIPI_Format_data = (mipi_video_timing_table[bIndex].MIPI_H_Back_Porch);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHL_REG, MIPI_Format_data);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SP_TX_HB_PORCHH_REG, (MIPI_Format_data >> 8));


	//force stream valid for MIPI
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, &c);
	SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_SYS_CTRL3_REG, c|0x03);

	//force video format select from register
	SP_TX_Read_Reg(SP_TX_PORT2_ADDR, 0x011, &c);
	SP_TX_Write_Reg(SP_TX_PORT2_ADDR, 0x11, c|0x10);


}

void MIPI_Format_Index_Set(BYTE bFormatIndex)
{
	bMIPIFormatIndex = bFormatIndex;
	//debug_printf("Set MIPI video format index to %d\n",(WORD)bMIPIFormatIndex);
}

BYTE MIPI_Format_Index_Get(void)
{
	debug_printf("MIPI video format index is %d\n",(WORD)bMIPIFormatIndex);
	return bMIPIFormatIndex;
}

void SP_TX_MIPI_CONFIG_Flag_Set(BYTE bConfigured)
{
		bMIPI_Configured = bConfigured;
}
/*
BYTE MIPI_CheckSum_Status_OK(void)
{
	BYTE c;
	
	SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, MIPI_PROTOCOL_STATE, &c);
	//debug_printf("protocol state = %.2x\n",(WORD)c);
	if(c&MIPI_CHECKSUM_ERR)
		return 0;
	else
		return 1;
}
*/




void SP_CTRL_Variable_Init(void)
{	
#if(REDUCE_REPEAT_PRINT_INFO)
	BYTE i = 0;
#endif
	sp_tx_edid_err_code = 0; 
	edid_pclk_out_of_range = 0; 
	sp_tx_ds_edid_hdmi = 0;
	sp_tx_ds_edid_3d_present = 0;

	sp_tx_pd_mode = 1;//initial power state is power down.


	sp_tx_link_training_state = LINK_TRAINING_INIT;
	hdcp_process_state = HDCP_PROCESS_INIT;
		
	bEDIDBreak = 0;
	//VSDBaddr = 0x84;
	EDID_Print_Enable = 0;
	//Bist format index initial
	bBIST_FORMAT_INDEX = 1;//default 720P
	Force_Video_Resolution = 1; // set bist resolution acoording to the timing table

	sp_tx_lane_count = 0x01;
	sp_tx_bw= 0;
	pclk = 0;

	sp_tx_rx_anx7730 = 1;//default the Rx is ANX7730
#if(AUTO_TEST_CTS)
	sp_tx_test_lt = 0;
	sp_tx_test_bw = 0;
	sp_tx_test_edid = 0;
#endif
#if(REDUCE_REPEAT_PRINT_INFO)
	maybe_repeat_print_info_flag = 0;
	for(i = 0; i < LOOP_PRINT_MSG_MAX; i++)
		repeat_printf_info_count[i] = 0;
#endif

      //CEC support index initial
	CEC_abort_message_received = 0;
	CEC_get_physical_adress_message_received = 0;
	CEC_logic_addr = 0x00;
	CEC_loop_number = 0;
	CEC_resent_flag = 0;
      
	MIPI_Format_Index_Set(1);

}


void SP_CTRL_Set_LVTTL_Interface(BYTE eBedSync, BYTE rDE, BYTE sYCMUX, BYTE sDDR,BYTE lEdge )
{
    /*
	eBedSync:    1_Embeded SYNC, 0_Separate SYNC
	rDE:             1_Regenerate DE, 0_Separate DE
	sYCMUX:       1_YCMUX, 0_Not YCMUX
	sDDR:           1_DDR,  0_SDR   
	lEdge:           1_negedge, 0_posedge  
    */
    
    SP_TX_Video_Input.bLVTTL_HW_Interface.sEmbedded_Sync.Embedded_Sync = eBedSync;
    SP_TX_Video_Input.bLVTTL_HW_Interface.sEmbedded_Sync.Extend_Embedded_Sync_flag = 0;
    SP_TX_Video_Input.bLVTTL_HW_Interface.DE_reDenerate = rDE;
    SP_TX_Video_Input.bLVTTL_HW_Interface.sYC_MUX.YC_MUX = sYCMUX;
    SP_TX_Video_Input.bLVTTL_HW_Interface.sYC_MUX.YC_BIT_SEL = 1;
    SP_TX_Video_Input.bLVTTL_HW_Interface.DDR_Mode = sDDR;
    SP_TX_Video_Input.bLVTTL_HW_Interface.Clock_EDGE = lEdge;
}


void SP_CTRL_InputSet(VideoInterface Interface,ColorSpace bColorSpace, ColorDepth cBpc)
{
	SP_TX_Video_Input.Interface = Interface;

	SP_TX_Video_Input.bColordepth = cBpc;
	SP_TX_Video_Input.bColorSpace = bColorSpace;

}

void SP_CTRL_AUDIO_FORMAT_Set(AudioType cAudio_Type,AudioFs cAudio_Fs,AudioWdLen cAudio_Word_Len)
{
        SP_TX_Audio_Input.bAudioType = cAudio_Type;
        SP_TX_Audio_Input.bAudio_Fs = cAudio_Fs;
        SP_TX_Audio_Input.bAudio_word_len = cAudio_Word_Len;
}


void SP_CTRL_I2S_CONFIG_Set(I2SChNum cCh_Num, I2SLayOut cI2S_Layout)
{
	SP_TX_Audio_Input.bI2S_FORMAT.AUDIO_LAYOUT  = cI2S_Layout;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_Num     = cCh_Num;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status1 = 0x00;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status2 = 0x00;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status3 = 0x00;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status4 = SP_TX_Audio_Input.bAudio_Fs;
	SP_TX_Audio_Input.bI2S_FORMAT.Channel_status5 = SP_TX_Audio_Input.bAudio_word_len;
	SP_TX_Audio_Input.bI2S_FORMAT.SHIFT_CTRL = 0;
	SP_TX_Audio_Input.bI2S_FORMAT.DIR_CTRL = 0;
	SP_TX_Audio_Input.bI2S_FORMAT.WS_POL = 0;
	SP_TX_Audio_Input.bI2S_FORMAT.JUST_CTRL = 0;
	SP_TX_Audio_Input.bI2S_FORMAT.EXT_VUCP = 0;
}

SP_TX_System_State get_system_state(void)
{
	return sp_tx_system_state;
}
/*
* FUNCTION: change_system_state_clean
* 		clear up pre system state machine register status.
*/
void change_system_state_clean(SP_TX_System_State cur_state)
{
	if(sp_tx_system_state >= SP_TX_HDCP_AUTHENTICATION
	   && cur_state <= SP_TX_HDCP_AUTHENTICATION) {
		SP_TX_Video_Mute(1);
		hdcp_encryption_enable(0);
	   	SP_CTRL_Clean_HDCP(); 
	}
	else if(sp_tx_system_state > SP_TX_CONFIG_VIDEO_INPUT
		&& cur_state <= SP_TX_CONFIG_VIDEO_INPUT) {
		sp_tx_enable_video_input(0);
             	SP_TX_Disable_Audio_Input();
             	SP_TX_Enable_Audio_Output(0);
	}
}
void SP_CTRL_Set_System_State(SP_TX_System_State ss) 
{
	debug_puts("SP_TX To System State: ");
	change_system_state_clean(ss);
    switch (ss) 
    {
        case SP_TX_INITIAL:
            sp_tx_system_state = SP_TX_INITIAL;
            debug_puts("SP_TX_INITIAL");
            break;
        case SP_TX_WAIT_SLIMPORT_PLUGIN: 
            sp_tx_system_state = SP_TX_WAIT_SLIMPORT_PLUGIN;
            debug_puts("SP_TX_WAIT_SLIMPORT_PLUGIN");
            break;
        case SP_TX_PARSE_EDID:
            sp_tx_system_state = SP_TX_PARSE_EDID;
            debug_puts("SP_TX_READ_PARSE_EDID");
            break;
        case SP_TX_CONFIG_VIDEO_INPUT:
            sp_tx_system_state = SP_TX_CONFIG_VIDEO_INPUT;
            debug_puts("SP_TX_CONFIG_VIDEO_INPUT");
            break;
	 case SP_TX_CONFIG_AUDIO:
            sp_tx_system_state = SP_TX_CONFIG_AUDIO;
            debug_puts("SP_TX_CONFIG_AUDIO");
            break;
        case SP_TX_LINK_TRAINING:
            sp_tx_system_state = SP_TX_LINK_TRAINING;
	    	sp_tx_link_training_state = LINK_TRAINING_INIT;
            debug_puts("SP_TX_LINK_TRAINING");
            break;
	 case SP_TX_CONFIG_VIDEO_OUTPUT:
            sp_tx_system_state = SP_TX_CONFIG_VIDEO_OUTPUT;
	 	break;
        case SP_TX_HDCP_AUTHENTICATION:
		hdcp_process_state = HDCP_PROCESS_INIT;
            sp_tx_system_state = SP_TX_HDCP_AUTHENTICATION;
            debug_puts("SP_TX_HDCP_AUTHENTICATION");
            break;
        case SP_TX_PLAY_BACK:
            sp_tx_system_state = SP_TX_PLAY_BACK;
            debug_puts("SP_TX_PLAY_BACK");
	     #if(UPDATE_ANX7730_SW)
 	     SP_TX_Write_Reg(SP_TX_PORT2_ADDR, SPDIF_AUDIO_CTRL0 , 0x85);	
	    #endif
            break;
        default:
            debug_puts("state error!\n");
            break;
    }

	//per_system_state = sp_tx_system_state;

}

//check downstream cable stauts ok-20110906-ANX.Fei
BYTE SP_CTRL_Check_Cable_Status(void)
{
    BYTE c;
	SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x18,1,&c);
	if((c&0x28)==0x28)
		return 1; //RX OK

	return 0;

}


static void sp_tx_send_message(enum SP_TX_SEND_MSG message)
{
	BYTE c;

	switch (message) {
	case MSG_OCM_EN:
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x05, 0x25, 0x5a);
		break;

	case MSG_INPUT_HDMI:
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x05, 0x26, 0x01);
		break;

	case MSG_INPUT_DVI:
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x05, 0x26, 0x00);
		break;

	case MSG_CLEAR_IRQ:
		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x04, 0x10, 1, &c);
		c |= 0x01;
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x04, 0x10, c);
		break;
	}

}

static BYTE sp_tx_get_cable_type(void)
{
	BYTE SINK_OUI[8] = { 0 };
	BYTE ds_port_preset = 0;
	BYTE ds_port_recoginze = 0;
	BYTE temp_value;
	int i,j;


	for (i = 0; i < 5; i++) {
		if(AUX_ERR == SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00, 0x05, 1, &ds_port_preset)) {
			/*time delay for VGA dongle mcu startup*/
			delay_ms(500);
			debug_puts(" AUX access error");
			SP_TX_RST_AUX();
			continue;
		}
		
		for( j =0; j < 0x0c; j++) {
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00, j, 1, &temp_value);
			//pr_info(" DPCD 0x000%.2x = 0x%.2x\n", (uint)i, (uint)bytebuf[0]);
		}

		switch (ds_port_preset & 0x07) {
		case 0x00:
			sp_tx_rx_anx7730 = 0;
			ds_port_recoginze = 1;
			debug_puts("Downstream is DP dongle.\n");
			break;
		case 0x03:
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x04, 0x00, 8, SINK_OUI);
			if ((SINK_OUI[0] == 0x00) && (SINK_OUI[1] == 0x22)
			    && (SINK_OUI[2] == 0xb9) && (SINK_OUI[3] == 0x61)
			    && (SINK_OUI[4] == 0x39) && (SINK_OUI[5] == 0x38)
			    && (SINK_OUI[6] == 0x33)) {
				debug_puts("Downstream is VGA dongle.\n");
				sp_tx_rx_anx7730 = 0;
			} else {
				sp_tx_rx_anx7730 = 0;
				debug_puts("Downstream is general DP2HDMI converter.\n");
			}
			ds_port_recoginze = 1;
			break;
		case 0x05:
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x04, 0x00, 8, SINK_OUI);
			if ((SINK_OUI[0] == 0xb9) && (SINK_OUI[1] == 0x22)
			    && (SINK_OUI[2] == 0x00) && (SINK_OUI[3] == 0x00)
			    && (SINK_OUI[4] == 0x00) && (SINK_OUI[5] == 0x00)
			    && (SINK_OUI[6] == 0x00)) {
				debug_puts("Downstream is HDMI dongle.\n");
				sp_tx_send_message(MSG_OCM_EN);
				sp_tx_rx_anx7730 = 1;
			} else {
				sp_tx_rx_anx7730 = 0;
				debug_puts("Downstream is general DP2VGA converter.\n");
			}
			ds_port_recoginze = 1;
			break;
		default:
			delay_ms(1000);
			debug_puts("Downstream can not recognized.\n");
			sp_tx_rx_anx7730 = 0;
			ds_port_recoginze = 0;
			break;

		}

	}

	return ds_port_recoginze;
}

BYTE sp_tx_get_hdmi_connection(void)
{
	BYTE c;	
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x05, 0x18, 1, &c);
	if ((c & 0x41) == 0x41) {
		SP_TX_AUX_DPCDWrite_Byte(0x00, 0x05, 0xf3, 0x70);
		return 1;
	} else
		return 0;
}

BYTE sp_tx_get_vga_connection(void)
{
	BYTE c;
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, DPCD_SINK_COUNT, 1, &c);
	return ((c & 0x01) == 0x01) ? 1:0;
}
BYTE is_cable_detected(void)
{
	BYTE temp1,return_value = 0;
	temp1 = (get_cable_detect_status() != 0) ? 1 : 0;
	if(temp1 == 1) {
		delay_ms(50);
		temp1 = 0;
		temp1 = (get_cable_detect_status() != 0) ? 1 : 0;
		if(temp1 == 1)
			return_value = 1;			
	}
	return return_value;
}
void SP_CTRL_Slimport_Plug_Process(void)
{
	if(is_cable_detected() == 1)
	{
	debug_printf("detected cable: %.2x \n",(WORD)is_cable_detected());
	printk(KERN_INFO "7805 : detected cable: %.2x \n",(WORD)is_cable_detected());
            if(sp_tx_pd_mode)
	     {
			system_power_ctrl(1);
			delay_ms(500);
			if(sp_tx_get_cable_type() == 0)
			{
				debug_puts("AUX ERR");
				system_power_ctrl(0);
				return;
			} 
                	#if(UPDATE_ANX7730_SW)
			SP_TX_RST_AUX();
			if(update_anx7730_sw() != 0) {
				system_power_ctrl(0);
				return;
			}			
			#endif

                }				  
                if(sp_tx_rx_anx7730)
                {
				if(sp_tx_get_hdmi_connection())
					SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
                }else{
				if(sp_tx_get_vga_connection()) {
					sp_tx_send_message(MSG_CLEAR_IRQ); 
					SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
				}
                }
	}		
	else if(sp_tx_pd_mode == 0)
	{
		printk(KERN_INFO "7805 : cable not detected");
		debug_puts("cable not detected");
		system_power_ctrl(0);
	}

}

void SP_CTRL_Video_Changed_Int_Handler (BYTE changed_source)
{
    //debug_puts("[INT]: SP_CTRL_Video_Changed_Int_Handler");
    if(sp_tx_system_state >= SP_TX_CONFIG_AUDIO)
    {
        switch(changed_source) 
        {
            case 0:
                debug_puts("Video:_______________Video clock changed!");
		
		  SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO_INPUT);
                break;
            case 1:
               //debug_puts("Video:_______________Video format changed!");
                //SP_TX_Disable_Video_Input();
                // SP_TX_Disable_Audio_Input();
                // SP_TX_Enable_Audio_Output(0);
                //SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO);
                break;
            default:
                break;
        } 
    }
}

void SP_CTRL_PLL_Changed_Int_Handler(void)
{
  // debug_puts("[INT]: SP_CTRL_PLL_Changed_Int_Handler");
    if (sp_tx_system_state > SP_TX_PARSE_EDID)
    {
	  if(!SP_TX_Get_PLL_Lock_Status())
        {
            debug_puts("PLL:_______________PLL not lock!");
            //SP_CTRL_Clean_HDCP();
            SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
        }
    }
}
void SP_CTRL_AudioClk_Change_Int_Handler(void)
{
   // debug_puts("[INT]: SP_CTRL_AudioClk_Change_Int_Handler");
    if (sp_tx_system_state >= SP_TX_CONFIG_AUDIO)
    {

            debug_puts("Audio:_______________Audio clock changed!");
            SP_TX_Disable_Audio_Input();
	     SP_TX_Enable_Audio_Output(0);
            //SP_CTRL_Clean_HDCP();
            SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
    }
}

void  SP_CTRL_Auth_Done_Int_Handler(void) 
{
	BYTE temp_value[2] = {0};
	static BYTE auth_fail_counter = 0;
	if (sp_tx_system_state < SP_TX_HDCP_AUTHENTICATION)
		return;
		
 	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_HDCP_STATUS, temp_value);

	if (temp_value[0] & SP_TX_HDCP_AUTH_PASS) {
		SP_TX_AUX_DPCDRead_Bytes(0x06, 0x80, 0x2A, 2, temp_value);
		if (temp_value[1] & 0x08) {
			hdcp_process_state = HDCP_PROCESS_INIT;
			debug_puts("Re-authentication!\n");
		} else {
			debug_puts("Authentication pass in Auth_Done\n");
			hdcp_process_state = HDCP_FINISH;
			auth_fail_counter = 0;
		}
	} else {
		debug_puts("Authentication failed in AUTH_done\n");
		auth_fail_counter++;
		if(auth_fail_counter >= SP_TX_HDCP_FAIL_THRESHOLD) {
			auth_fail_counter = 0;
			hdcp_process_state = HDCP_FAILE;
		} else {
			hdcp_process_state = HDCP_PROCESS_INIT;
			debug_puts("Re-authentication!\n");
		}
	}
}

#if 0
void SP_CTRL_Auth_Change_Int_Handler(void) 
{
	BYTE c;
      // debug_puts("[INT]: SP_CTRL_Auth_Change_Int_Handler");
	SP_TX_Get_HDCP_status(&c);
	if(c & SP_TX_HDCP_AUTH_PASS) 
	{
		sp_tx_hdcp_auth_pass = 1;
		debug_puts("Authentication pass in Auth_Change");
	} 
	else 
	{
		debug_puts("Authentication failed in Auth_change");
		sp_tx_hdcp_auth_pass = 0;
		SP_TX_Video_Mute(1);
		SP_TX_HDCP_Encryption_Disable();
		if(sp_tx_system_state > SP_TX_CONFIG_VIDEO)
		{
			SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
			SP_CTRL_Clean_HDCP();
		}
	}
}
#endif

/*added for B0 version-ANX.Fei-20110901-Start*/

// hardware linktraining finish interrupt handle process
void SP_CTRL_LT_DONE_Int_Handler(void)
{
	BYTE c;
	//debug_puts("[INT]: SP_CTRL_LT_DONE_Int_Handler");
	if((sp_tx_link_training_state >= LINK_TRAINING_FINISH))
		return;

	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_TRAINING_CTRL_REG, &c);
	if(c & 0x70)
	{
		c = (c & 0x70) >> 4;
		debug_printf("HW LT failed in interrupt, ERR code = %.2x\n",(WORD)c);
		 SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
		 delay_ms(50);
		 
	}else{
		sp_tx_link_training_state = LINK_TRAINING_FINISH;
		SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_LANE0_SET_REG, &c);
		debug_printf("HW LT succeed in interrupt ,LANE0_SET = %.2x\n",(WORD)c);
	}
}

//for mipi interrupt process
void SP_CTRL_MIPI_Htotal_Chg_Int_Handler(void)
{
#if 1
	if(SP_TX_Video_Input.Interface   == MIPI_DSI)
	{
		debug_puts("mipi htotal changed");
		//SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO);
	}
#endif
}

void SP_CTRL_MIPI_Packet_Len_Chg_Int_Handler(void)
{
#if 1
	if(SP_TX_Video_Input.Interface   == MIPI_DSI)
	{
		debug_puts("mipi packet length changed");

		//SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO);
	}
#endif
}
 
void SP_CTRL_MIPI_Lane_clk_Chg_Int_Handler(void)
{
#if 1
	if(SP_TX_Video_Input.Interface   == MIPI_DSI)
	debug_puts("mipi lane clk changed");
#endif
}
 void SP_CTRL_LINK_CHANGE_Int_Handler(void)
 {

     BYTE lane0_1_status,sl_cr,al;
  
		//debug_puts("[INT]: SP_CTRL_LINK_CHANGE_Int_Handler");
		if(sp_tx_system_state < SP_TX_CONFIG_VIDEO_OUTPUT)//(sp_tx_system_state < SP_TX_LINK_TRAINING )
			return;
      
           
            SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE_ALIGN_STATUS_UPDATED,1,ByteBuf);
			al = ByteBuf[0];

		  
            SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE0_1_STATUS,1,ByteBuf);
			lane0_1_status = ByteBuf[0];

            
           // debug_printf("al = %x, lane0_1 = %x\n",(WORD)al,(WORD)lane0_1_status);

                if(((lane0_1_status & 0x01) == 0) || ((lane0_1_status & 0x04) == 0))
                sl_cr = 0;
                else 
                sl_cr = 1;


            if(((al & 0x01) == 0) || (sl_cr == 0) )//align not done, CR not done
            {
                if((al & 0x01)==0)
                    debug_printf("Lane align not done\n");
                if(sl_cr == 0)
                    debug_printf("Lane clock recovery not done\n");

                //re-link training only happen when link traing have done		  
                if((sp_tx_system_state > SP_TX_LINK_TRAINING )
			&&sp_tx_link_training_state > LINK_TRAINING_FINISH)
                {
                    SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
                    debug_puts("IRQ:____________re-LT request!");
                }
            }
        

 }


// c-wire polling error interrupt handle process
void SP_CTRL_POLLING_ERR_Int_Handler(void)
{
    BYTE c;
    int i;

    //debug_puts("[INT]: SP_CTRL_POLLING_ERR_Int_Handler\n");
    if((sp_tx_system_state < SP_TX_WAIT_SLIMPORT_PLUGIN)||sp_tx_pd_mode)
        return;

    for(i=0;i<5;i++)
    {
        SP_TX_AUX_DPCDRead_Bytes(0x00,0x00,0x00,1,&c);
	 if(c==0x11)
	      return; 
        delay_ms(100);//delay 1s for ANX9832 dongle
    }
    
     if(sp_tx_pd_mode ==0)
    {
        //debug_printf("read dpcd 0x00000=%.2x\n",(WORD)c);	
        debug_puts("Cwire polling is corrupted,power down ANX7805.\n");
        system_power_ctrl(0);
    }
}

void SP_CTRL_SLIMBUS_AUDIO_STABLE_Int_Handler(void)
{
    BYTE c;
    debug_printf("SP_CTRL_SLIMBUS_AUDIO_STABLE_Int_Handler\n");

	SP_TX_Read_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG1, &c);
	SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, SLIMBUS_MISC_CONTROL_REG1, c | 0x04);

}


#if(AUTO_TEST_CTS)
void sp_tx_phy_auto_test(void)
{

	BYTE bSwing,bEmp;//for automated phy test
	BYTE c1,c;
	BYTE bytebuf[10];

	SP_TX_AUX_DPCDRead_Bytes(0x0, 0x02, 0x19, 1, bytebuf);
	switch(bytebuf[0]) //DPCD 0x219 TEST_LINK_RATE
	{
		case 0x06: 
			
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, 0x06);
			debug_puts("test BW= 1.62Gbps\n");
			break;
		case 0x0a:
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG,0x0a);
			debug_puts("test BW= 2.7Gbps\n");
			break;
		case 0x14:
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR,SP_TX_LINK_BW_SET_REG,0x014);
			debug_puts("test BW= 5.4Gbps\n");
			break;
		}

	SP_TX_AUX_DPCDRead_Bytes(0x0,0x02, 0x48, 1, bytebuf);
	switch(bytebuf[0]) //DPCD 0x248 PHY_TEST_PATTERN
	{
		case 0:
			debug_puts("No test pattern selected\n");
			break;
		case 1:
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x04);
			debug_puts("D10.2 Pattern\n");
			break;
		case 2:
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x08);
			debug_puts("Symbol Error Measurement Count\n");
			break;
		case 3:
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x0c);
			debug_puts("PRBS7 Pattern\n");
			break;
		case 4:
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x50, 0xa, bytebuf);
			
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x80, bytebuf[0]);	
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x81, bytebuf[1]);	
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x82, bytebuf[2]);	
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x83, bytebuf[3]);	
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x84, bytebuf[4]);	
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x85, bytebuf[5]);	
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x86, bytebuf[6]);	
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x87, bytebuf[7]);	
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x88, bytebuf[8]);	
			SP_TX_Write_Reg(MIPI_RX_PORT1_ADDR, 0x89, bytebuf[9]);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x30);//disable scramble?
			debug_puts("80bit custom pattern transmitted\n");
			break;
		case 5:
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0xA9, 0xFC);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0xAA, 0x00);
			SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_TRAINING_PTN_SET_REG, 0x14);
			debug_puts("HBR2 Compliance Eye Pattern\n");
			break;
		}
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x00, 0x03, 1, bytebuf);
	switch(bytebuf[0]&0x01)
	{
		case 0:
				SP_TX_SPREAD_Enable(0);
				debug_puts("SSC OFF\n");
				break;
		case 1:
				SP_TX_Read_Reg(SP_TX_PORT0_ADDR, SP_TX_LINK_BW_SET_REG, &c1);
				sp_tx_bw = c1;
				SP_TX_CONFIG_SSC(sp_tx_bw);
				debug_puts("SSC ON\n");
				break;
	}

	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x06,1,bytebuf);
	c1 = bytebuf[0]&0x03;//get swing adjust request
	SP_TX_Read_Reg(SP_TX_PORT0_ADDR, 0xA2, &c);
	if((c == 0x0c)&&((bytebuf[0]&0x0f)==0x02)){//PRBS7 pattern, eye diagram, swing2/emp 0
		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0xA3, 0x0a);
		debug_puts("eye request,lane0,400/6db\n");
	}else if (c == 0x14) {//cep pattern

		SP_TX_Write_Reg(SP_TX_PORT0_ADDR, 0xA3, 0x0a);
		debug_puts("cep pattern,lane0,600/3.5db\n");
	} else {

		switch(c1)		  
		{
			case 0x00:
				SP_TX_Read_Reg(0x70, 0xA3, &bSwing);
				SP_TX_Write_Reg(0x70, 0xA3, (bSwing&~0x03)|0x00);
				debug_puts("lane0,Swing200mv\n");
				break;
			case 0x01: 
				SP_TX_Read_Reg(0x70, 0xA3, &bSwing);
				SP_TX_Write_Reg(0x70, 0xA3, (bSwing&~0x03)|0x01);
				debug_puts("lane0,Swing400mv\n");
				break;
			case 0x02:
				SP_TX_Read_Reg(0x70, 0xA3, &bSwing);
				SP_TX_Write_Reg(0x70, 0xA3, (bSwing&~0x03)|0x02);
				debug_puts("lane0,Swing600mv\n");
				break;
			case 0x03:
				SP_TX_Read_Reg(0x70, 0xA3, &bSwing);
				SP_TX_Write_Reg(0x70, 0xA3, (bSwing&~0x03)|0x03);
				debug_puts("lane0,Swing800mv\n");
				break;
			default:
				break;
		}

		c1 = ( bytebuf[0]&0x0c);//get emphasis adjust request
		c1=c1>>2;
		switch(c1)		 
		{
			case 0x00:
				SP_TX_Read_Reg(0x70, 0xA3, &bEmp);
				SP_TX_Write_Reg(0x70, 0xA3, (bEmp&~0x18)|0x00);
				debug_puts("lane0,emp 0db\n");
				break;
			case 0x01: 
				SP_TX_Read_Reg(0x70, 0xA3, &bEmp);
				SP_TX_Write_Reg(0x70, 0xA3, (bEmp&~0x18)|0x08);
				debug_puts("lane0,emp 3.5db\n");
				break;
			case 0x02:
				SP_TX_Read_Reg(0x70, 0xA3, &bEmp);
				SP_TX_Write_Reg(0x70, 0xA3, (bEmp&~0x18)|0x10);
				debug_puts("lane0,emp 6db\n");
				break;
		
			default:
				break;
		}
	}

}
#endif
/*added for B0 version-ANX.Fei-20110901-End*/
void SP_CTRL_IRQ_ISP(void)
{
    BYTE c,lane0_1_status,sl_cr,al,need_return = 0, temp_value;
    BYTE IRQ_Vector,Int_vector1,Int_vector2;

    //SP_TX_RST_AUX();

	SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_DEVICE_SERVICE_IRQ_VECTOR,1,ByteBuf);
	IRQ_Vector = ByteBuf[0];
	SP_TX_AUX_DPCDWrite_Bytes(0x00, 0x02, DPCD_DEVICE_SERVICE_IRQ_VECTOR,1, ByteBuf);//write clear IRQ


	if(IRQ_Vector & 0x04)//HDCP IRQ
	{
		if(hdcp_process_state == HDCP_FINISH)
		{
			SP_TX_AUX_DPCDRead_Bytes(0x06,0x80,0x29,1,&c);
			//debug_printf("Bstatus = %.2x\n", (WORD)c1);	
			if(c & 0x04)
			{
				if(sp_tx_system_state > SP_TX_HDCP_AUTHENTICATION) 
				{
					SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
					debug_puts("IRQ:____________HDCP Sync lost!");
				}
			}
		}
	}

	if((IRQ_Vector & 0x40)&&(sp_tx_rx_anx7730))//specific int
	{

		// debug_puts("Rx specific interrupt IRQ!\n");

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,DPCD_SPECIFIC_INTERRUPT_1,1,&Int_vector1);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x05,DPCD_SPECIFIC_INTERRUPT_1,Int_vector1);
		//debug_printf("DPCD00510 = 0x%.2x!\n",(WORD)Int_vector1);

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,DPCD_SPECIFIC_INTERRUPT_2,1,&Int_vector2);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x05,DPCD_SPECIFIC_INTERRUPT_2,Int_vector2);
		//debug_printf("DPCD00511 = 0x%.2x!\n",(WORD)Int_vector2);

		temp_value = 0x01;
		do {
			switch( Int_vector1 & temp_value) {
			default:
				break;
			case 0x01:
			//check downstream HDMI hotplug status plugin
			SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x18,1,&c) ;
			if((c&0x01)==0x01)
			debug_puts("Downstream HDMI is pluged!\n");
			break;
			case 0x02:
			//check downstream HDMI hotplug status unplug
			SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x18,1,&c);
			if((c&0x01)!=0x01)
			{
				debug_puts("Downstream HDMI is unpluged!\n");
				if((sp_tx_system_state > SP_TX_WAIT_SLIMPORT_PLUGIN) 
					&& (!sp_tx_pd_mode))
				{
					system_power_ctrl(0);
					need_return = 1;
				}
			}
			break;
			case 0x04:
				if(sp_tx_system_state < SP_TX_CONFIG_AUDIO) 
					break;
				
				debug_puts("Rx specific  IRQ: Link is down!\n");
				SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE_ALIGN_STATUS_UPDATED,1,ByteBuf);
				al = ByteBuf[0];

				SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE0_1_STATUS,1,ByteBuf);
				lane0_1_status = ByteBuf[0];

				if(((lane0_1_status & 0x01) == 0) || ((lane0_1_status & 0x04) == 0))
				sl_cr = 0;
				else 
				sl_cr = 1;

				if(((al & 0x01) == 0) || (sl_cr == 0) )//align not done, CR not done
				{
					if((al & 0x01)==0)
						debug_printf("Lane align not done\n");
					if(sl_cr == 0)
						debug_printf("Lane clock recovery not done\n");

					//re-link training only happen when link traing have done		  
					if((sp_tx_system_state > SP_TX_LINK_TRAINING )
					&& (sp_tx_link_training_state > LINK_TRAINING_PRE_CONFIG) )
					{
						SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
						debug_puts("IRQ:____________re-LT request!");
					}
				}
				break;
			case 0x08:
				debug_puts("Downstream HDCP is done!\n");
				if((Int_vector1&0x10) !=0x10)
					debug_puts("Downstream HDCP is passed!\n");
				else
				{
					if(sp_tx_system_state > SP_TX_CONFIG_VIDEO_OUTPUT )
					{
						SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
						debug_puts("Re-authentication due to downstream HDCP failure!");
					}
				}
				break;
			case 0x10:
				debug_puts("Downstream HDCP is fail! \n");
				break;
			case 0x20:
				debug_puts(" Downstream HDCP link integrity check fail!");
				//add for hdcp fail
				if(sp_tx_system_state > SP_TX_HDCP_AUTHENTICATION) 
				{
					SP_CTRL_Set_System_State(SP_TX_HDCP_AUTHENTICATION);
					debug_puts("IRQ:____________HDCP Sync lost!\n");
				}
				break;
			case 0x40:
				debug_puts("Receive CEC command from upstream done!\n");
				break;
			case 0x80:
				debug_puts("CEC command transfer to downstream done!\n");
				break;
			}
			if(need_return == 1)
				return;			
			temp_value = (temp_value << 1);

		}while(temp_value != 0);

		/*check downstream HDMI Rx sense status -20110906-ANX.Fei*/
		if((Int_vector2&0x04)==0x04)
		{
			SP_TX_AUX_DPCDRead_Bytes(0x00,0x05,0x18,1,&c);
			if((c&0x40)==0x40)
			{
				debug_puts("Downstream HDMI termination is detected!\n");
			}
		}			

	} 	
	else  if((IRQ_Vector & 0x40)&&(!sp_tx_rx_anx7730))//specific int 	
	{
		//check sink count for general monitor
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,0x00,1,&c);
		if( (c & 0x01) == 0x00) {
			if((sp_tx_system_state > SP_TX_WAIT_SLIMPORT_PLUGIN )
				&& (sp_tx_pd_mode == 0))
			{
				SP_TX_Power_Enable(SP_TX_PWR_TOTAL, POWER_DOWN);
				SP_TX_Power_Enable(SP_TX_PWR_REG, POWER_DOWN);
				SP_TX_Hardware_PowerDown();
				SP_CTRL_Set_System_State(SP_TX_WAIT_SLIMPORT_PLUGIN);
				sp_tx_pd_mode = 1;
			}
		}

		//Indicate the Rx to clear the specific irq
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x04,0x10,1,&c);
		SP_TX_AUX_DPCDWrite_Byte(0x00,0x04,0x10,(c|0x01));

		SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE_ALIGN_STATUS_UPDATED,1,ByteBuf);
		al = ByteBuf[0];
		SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,DPCD_LANE0_1_STATUS,1,ByteBuf);
		lane0_1_status = ByteBuf[0];

		if(((lane0_1_status & 0x01) == 0) || ((lane0_1_status & 0x04) == 0))
			sl_cr = 0;
		else 
			sl_cr = 1;

		if(((al & 0x01) == 0) || (sl_cr == 0) )//align not done, CR not done
		{
			if((al & 0x01)==0)
				debug_printf("Lane align not done\n");
			if(sl_cr == 0)
				debug_printf("Lane clock recovery not done\n");

			//re-link training only happen when link traing have done		  
			if((sp_tx_system_state > SP_TX_LINK_TRAINING )
				&&sp_tx_link_training_state > LINK_TRAINING_PRE_CONFIG)
			{
				SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
				debug_puts("IRQ:____________re-LT request!");
			}
		}

	}
	#if(AUTO_TEST_CTS)
	/* AUTOMATED TEST IRQ */
	if (IRQ_Vector & 0x02) {
		BYTE bytebuf[1]={0};
 	       BYTE test_vector = 0;

		SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x18, 1, bytebuf);
		test_vector = bytebuf[0];
		if(test_vector & 0x01)//test link training
		{
			sp_tx_test_lt = 1;
	
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x19,1,bytebuf);
			sp_tx_test_bw = bytebuf[0];
			debug_printf(" test_bw = %.2x\n", (unsigned int)sp_tx_test_bw);

			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x60,1,bytebuf);
			bytebuf[0] = bytebuf[0] | TEST_ACK;
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x60,1, bytebuf);

			debug_puts("Set TEST_ACK!\n");
			if (sp_tx_system_state >= SP_TX_LINK_TRAINING)
			SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
			debug_puts("IRQ:test-LT request!\n");


		}


		if(test_vector & 0x04)//test edid
		{
			if (sp_tx_system_state > SP_TX_PARSE_EDID)
			SP_CTRL_Set_System_State(SP_TX_PARSE_EDID);
			sp_tx_test_edid = 1;
			debug_printf("Test EDID Requested!\n");

		}         

		if(test_vector & 0x08)//phy test pattern
		{
			
			sp_tx_phy_auto_test();
			
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x60,1,bytebuf);
			bytebuf[0] = bytebuf[0] | 0x01;
			SP_TX_AUX_DPCDRead_Bytes(0x00, 0x02, 0x60, 1,bytebuf);
/*
			sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60,1,bytebuf);
			while((bytebuf[0] & 0x03) == 0){
				bytebuf[0] = bytebuf[0] | 0x01;
				sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60, 1, bytebuf);
				sp_tx_aux_dpcdread_bytes(0x00, 0x02, 0x60,1,bytebuf);
			}*/
		}
		            
	}
	#endif
}

// downstream DPCD IRQ request interrupt handle process
void SP_CTRL_SINK_IRQ_Int_Handler(void)
{
    //debug_puts("[INT]: SP_CTRL_SINK_IRQ_Int_Handler\n");
    SP_CTRL_IRQ_ISP();
}

void SP_CTRL_Clean_HDCP(void)
{
      // debug_puts("HDCP Clean!");
	SP_TX_Clean_HDCP();
}

void SP_CTRL_EDID_Read(void)//add adress only cmd before every I2C over AUX access.-fei
{
    BYTE i,j,test_vector,edid_block = 0,segment = 0,offset = 0;

    SP_TX_EDID_Read_Initial();

    checksum = 0;
    sp_tx_ds_edid_hdmi =0;
	
    //VSDBaddr = 0x84;
    bEDIDBreak = 0;
    //Set the address only bit
    SP_TX_AddrOnly_Set(1);
    //set I2C write com 0x04 mot = 1
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG, 0x04);
    //enable aux
    SP_TX_Write_Reg(SP_TX_PORT0_ADDR, SP_TX_AUX_CTRL_REG2, 0x01);
    SP_TX_Wait_AUX_Finished();
       
    edid_block = SP_TX_Get_EDID_Block();
   	if(edid_block < 2)
	{

		edid_block = 8 * (edid_block + 1);

    for(i = 0; i < edid_block; i ++)
    {
	if(!bEDIDBreak)
	SP_TX_AUX_EDIDRead_Byte(i * 16);
	delay_ms(10);
    }

		//clear the address only bit
		SP_TX_AddrOnly_Set(0);

	}else
	{

		for(i = 0; i < 16; i ++)
		{
			if(!bEDIDBreak)
			SP_TX_AUX_EDIDRead_Byte(i * 16);
		}

             //clear the address only bit
             SP_TX_AddrOnly_Set(0);
	     if(!bEDIDBreak)
	     	{
		     	edid_block = (edid_block + 1);
			for(i=0; i<((edid_block-1)/2); i++)//for the extern 256bytes EDID block
			{
				//debug_puts("EXT 256 EDID block");		
				segment = i + 1;		
				for(j = 0; j<16; j++)
				{
				 if(!bEDIDBreak)
				SP_TX_Parse_Segments_EDID(segment, offset);
				//delay_ms(1);
				offset = offset + 0x10;
				}
			}

			if(edid_block%2)//if block is odd, for the left 128BYTEs EDID block
			{
				//debug_puts("Last block");
				segment = segment + 1;
				for(j = 0; j<8; j++)
				{
				        if(!bEDIDBreak)
					SP_TX_Parse_Segments_EDID(segment, offset);
					//delay_ms(1);
					offset = offset + 0x10;
				}
			}	
	     	
	     	}
		
	      
	}


  //clear the address only bit
   SP_TX_AddrOnly_Set(0);
    

    SP_TX_RST_AUX();

    if(sp_tx_ds_edid_hdmi)
    SP_TX_AUX_DPCDWrite_Byte(0x00,0x05,0x26, 0x01);//inform ANX7730 the downstream is HDMI
    else
    SP_TX_AUX_DPCDWrite_Byte(0x00,0x05,0x26, 0x00);//inform ANX7730 the downstream is not HDMI
		

    SP_TX_AUX_DPCDRead_Bytes(0x00,0x02,0x18,1,ByteBuf);
  

    test_vector = ByteBuf[0];

    if(test_vector & 0x04)//test edid
    {
       // debug_printf("check sum = %.2x\n", (WORD)checksum);

        {
            SP_TX_AUX_DPCDWrite_Byte(0x00,0x02,0x61,checksum);   
            SP_TX_AUX_DPCDWrite_Byte(0x00,0x02,0x60,0x04);
        }
        debug_puts("Test read EDID finished");
    }
}

void SP_CTRL_EDID_Process(void)
{
	BYTE i,c;
	bEDIDBreak = 0;
	
	//read DPCD 00000-0000b
       for(i = 0; i <= 0x0b; i ++)
       	SP_TX_AUX_DPCDRead_Bytes(0x00,0x00,i,1,&c);
          
	SP_CTRL_EDID_Read();
	SP_TX_RST_AUX();
	if(bEDIDBreak)
		debug_puts("ERR:EDID corruption!\n");
	SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO_INPUT);
}

#if(ANX7730_VIDEO_STB)
BYTE sp_tx_get_ds_video_status(void)
{
	BYTE c;
	SP_TX_AUX_DPCDRead_Bytes(0x00, 0x05, 0x27, 1, &c);
	//debug_printf("0x00527 = %.2x.\n", (unsigned int)c);
	return ((c & 0x01) != 0 ? 1 : 0);
}
#endif
void SP_CTRL_HDCP_Process(void)
{
	BYTE c;
	static BYTE ds_vid_stb_cntr = 0;
	//debug_printf("HDCP Process state: %x \r\n", (unsigned int)hdcp_process_state);
	switch(hdcp_process_state) {
	case HDCP_PROCESS_INIT:		
		hdcp_process_state = HDCP_CAPABLE_CHECK;
		break;
	case HDCP_CAPABLE_CHECK:
		SP_TX_AUX_DPCDRead_Bytes(0x06, 0x80, 0x28,1,&c);
		if((c & 0x01) == 0)
			hdcp_process_state = HDCP_NOT_SUPPORT;
		else
			hdcp_process_state = HDCP_WAITTING_VID_STB;
		break;
	case HDCP_WAITTING_VID_STB:
		#if(ANX7730_VIDEO_STB)
		/*In case ANX7730 can not get ready video*/
		if(sp_tx_rx_anx7730) {
			//debug_printf("video stb : count%.2x \n",(WORD)ds_vid_stb_cntr);
			if (!sp_tx_get_ds_video_status()) {
				if (ds_vid_stb_cntr >= SP_TX_DS_VID_STB_TH) {
					system_power_ctrl(0);
					ds_vid_stb_cntr = 0;
				} else {
					ds_vid_stb_cntr++;
					delay_ms(100);
				}
				break;	
			} else {
				ds_vid_stb_cntr = 0;
				hdcp_process_state = HDCP_HW_ENABLE;
			}
		}
		#endif		
		break;
	case HDCP_HW_ENABLE:
		SP_TX_Power_Enable(SP_TX_PWR_HDCP, POWER_ON);// Poer on HDCP link clock domain logic for B0 version-20110913-ANX.Fei
		delay_ms(50);
		SP_TX_HW_HDCP_Enable();
		hdcp_process_state = HDCP_WAITTING_FINISH;
		break;
	case HDCP_WAITTING_FINISH:
		break;
	case HDCP_FINISH:		
		hdcp_encryption_enable(1);
		SP_TX_Video_Mute(0);
		debug_puts("@@@@@@@@@@@@@@@@@@@@@@@hdcp_auth_pass@@@@@@@@@@@@@@@@@@@@\n");
		SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);
		SP_TX_Show_Infomation();
		       break;
	case HDCP_FAILE:
		hdcp_encryption_enable(0);
		SP_TX_Video_Mute(1);
	    	debug_puts("***************************hdcp_auth_failed*********************************\n");
		SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);
		SP_TX_Show_Infomation();
		break;
	default:
	case HDCP_NOT_SUPPORT:
		debug_puts("Sink is not capable HDCP");
	        SP_TX_Video_Mute(1);//when Rx does not support HDCP, force to send blue screen
	        SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);
		break;
	}
}


void SP_CTRL_PlayBack_Process(void)
{	
     //BYTE c;

#if 0
	//for MIPI video change
	if(SP_TX_Video_Input.Interface  == MIPI_DSI)
	{
		//polling checksum error
		if(!MIPI_CheckSum_Status_OK())
		{
			debug_puts("mipi checksum error!");
			SP_TX_MIPI_CONFIG_Flag_Set(0);
			SP_CTRL_Set_System_State(SP_TX_CONFIG_VIDEO);
		}
		//else
			//debug_puts("mipi checksum ok!");
	}
#endif

}
void SP_CTRL_Int_Process(void)
{
	BYTE c1,c2,c3,c4,c5;//,b;

	if(sp_tx_pd_mode )//When chip is power down, do not care the int.-ANX.Fei-20111020
		return;

	if(sp_tx_rx_anx7730)
	{
		if(!SP_CTRL_Check_Cable_Status())//wait for downstream cable stauts ok-20110906-ANX.Fei
		{
			//RX not ready, check DPCD polling is still available 
			SP_CTRL_POLLING_ERR_Int_Handler();
			return;	    	
		}
	}

	SP_TX_Get_Int_status(COMMON_INT_1,&c1);
	SP_TX_Get_Int_status(COMMON_INT_2,&c2);
	SP_TX_Get_Int_status(COMMON_INT_3,&c3);
	SP_TX_Get_Int_status(COMMON_INT_4,&c4);
	SP_TX_Get_Int_status(SP_INT_STATUS,&c5);



	if(c1 & SP_COMMON_INT1_VIDEO_CLOCK_CHG)//video clock change
		SP_CTRL_Video_Changed_Int_Handler(0);

	if(sp_tx_pd_mode )
		return;

	if(c1 & SP_COMMON_INT1_PLL_LOCK_CHG)//pll lock change
		SP_CTRL_PLL_Changed_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	if(c1 & SP_COMMON_INT1_PLL_LOCK_CHG)//audio clock change
		SP_CTRL_AudioClk_Change_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	if(c2 & SP_COMMON_INT2_AUTHCHG)//auth change
	;//SP_CTRL_Auth_Change_Int_Handler();

	if(c2 & SP_COMMON_INT2_AUTHDONE)//auth done
		SP_CTRL_Auth_Done_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	//for mipi interrupt
	if(c2 & SP_COMMON_INT2_MIPI_HTOTAL_CHG)//mipi htotal change
		SP_CTRL_MIPI_Htotal_Chg_Int_Handler();
	if(sp_tx_pd_mode )
		return;

	if(c3 & SP_COMMON_INT3_MIPI_PACKET_LEN_CHG)//
		SP_CTRL_MIPI_Packet_Len_Chg_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	if(c3 & SP_COMMON_INT3_MIPI_LANE_CLK_CHG)//
		SP_CTRL_MIPI_Lane_clk_Chg_Int_Handler();

	if(sp_tx_pd_mode )
		return;

	/*added for B0 version-ANX.Fei-20110831-Begin*/
	if(c5 & SP_TX_INT_DPCD_IRQ_REQUEST)//IRQ int
		SP_CTRL_SINK_IRQ_Int_Handler();
	if(sp_tx_pd_mode )
		return;

	if(c5 & SP_TX_INT_STATUS1_POLLING_ERR)//c-wire polling error
		SP_CTRL_POLLING_ERR_Int_Handler();
	if(sp_tx_pd_mode )
		return;

	if(c5 & SP_TX_INT_STATUS1_TRAINING_Finish)//link training finish int
		SP_CTRL_LT_DONE_Int_Handler();
	if(sp_tx_pd_mode )
		return;

	if(c5 & SP_TX_INT_STATUS1_LINK_CHANGE)//link is lost  int
		SP_CTRL_LINK_CHANGE_Int_Handler();

//	if(c4 & SP_COMMON_INT4_SLIMBUS_AUDIO_STABLE)//slimbus audio stable int
	if (1)	
		SP_CTRL_SLIMBUS_AUDIO_STABLE_Int_Handler();

	
	/*added for B0 version-ANX.Fei-20110831-End*/
}

BYTE SP_CTRL_Chip_Detect(void)
{
	return SP_TX_Chip_Located();
}


void SP_CTRL_Chip_Initial(void)
{
	SP_CTRL_Variable_Init();

	//set video Input format
	if(MIPI_EN) {
		SP_CTRL_InputSet(MIPI_DSI,COLOR_RGB, COLOR_8_BIT);
		debug_puts("MIPI interface selected!\n");
	}
	else {
		SP_CTRL_InputSet(LVTTL_RGB,COLOR_RGB, COLOR_8_BIT);
		SP_CTRL_Set_LVTTL_Interface(SeparateSync,SeparateDE,UnYCMUX,SDR,Negedge);//separate SYNC,DE, not YCMUX, SDR,negedge
		debug_puts("LVTTL interface selected!");
	}
	//set audio input format
	#if(UPDATE_ANX7730_SW)
	if(0)
	#else
	if(!BIST_EN)
	#endif
	{
		if((!AUD_IN_SEL_1)&&(!AUD_IN_SEL_2))//00:SPDIF input as default
		{
			debug_puts("Set audio input to SPDIF\n");
			SP_CTRL_AUDIO_FORMAT_Set(AUDIO_SPDIF,AUDIO_FS_48K,AUDIO_W_LEN_20_24MAX);
		}
		else if((!AUD_IN_SEL_1)&&(AUD_IN_SEL_2)) //01: I2S input
		{
			debug_puts("Set audio input to I2S\n");
			SP_CTRL_AUDIO_FORMAT_Set(AUDIO_I2S,AUDIO_FS_48K,AUDIO_W_LEN_20_24MAX);
			SP_CTRL_I2S_CONFIG_Set(I2S_CH_2, I2S_LAYOUT_0);	
		}
		else if((AUD_IN_SEL_1)&&(!AUD_IN_SEL_2))	//10:Slimbus input
		{
			debug_puts("Set audio input to Slimbus\n");
			SP_CTRL_AUDIO_FORMAT_Set(AUDIO_SLIMBUS,AUDIO_FS_48K,AUDIO_W_LEN_20_24MAX);
			SP_CTRL_I2S_CONFIG_Set(I2S_CH_2, I2S_LAYOUT_0);
		}
		else
			debug_puts("invalid audio input");
	}
	else //set bist audio Input format
	{
		debug_puts("bist audio input");
		SP_CTRL_AUDIO_FORMAT_Set(AUDIO_BIST,AUDIO_FS_48K,AUDIO_W_LEN_20_24MAX);
	}
	SP_CTRL_Set_System_State(SP_TX_WAIT_SLIMPORT_PLUGIN);
	
	vbus_power_ctrl(0); // Disable the power supply for ANX7730
	SP_TX_Hardware_PowerDown();//Power down ANX7805 totally

}

void slimport_config_video_input(void)
{
#ifdef __KEIL51_ENV__
	if(BIST_EN) {
		bBIST_FORMAT_INDEX_backup = bBIST_FORMAT_INDEX;
		SP_CTRL_BIST_CLK_Genarator(bBIST_FORMAT_INDEX);//Generate Bist format clock
		SP_TX_Config_BIST_Video(bBIST_FORMAT_INDEX, &SP_TX_Video_Input);
	} else 
#endif
	{
		if(get_bandwidth_and_pclk() == 0) {
		sp_tx_link_training_state = LINK_TRAINING_INIT;
		SP_CTRL_Set_System_State(SP_TX_LINK_TRAINING);
		}

	}
}
void slimport_config_video_output(void)
{
	SP_TX_Video_Mute(1);
	//enable video input
	sp_tx_enable_video_input(1);			
	delay_ms(50);	
	SP_TX_AVI_Setup();//initial AVI infoframe packet
       SP_TX_Config_Packets(AVI_PACKETS);
       // 3d packed config
	if(EN_3D) {
		#if(ENABLE_3D)
		 debug_puts("send 3D packet \r\n");
		 sp_tx_send_3d_vsi_packet_to_7730(0x06);
		#endif
	}
	SP_CTRL_Set_System_State(SP_TX_CONFIG_AUDIO);
}

void slimport_hdcp_authentication(BYTE enable)
{
	if(enable)
	{
		SP_CTRL_HDCP_Process();
	}
	else
	{
		SP_TX_Power_Enable(SP_TX_PWR_HDCP, POWER_DOWN);// Poer down HDCP link clock domain logic for B0 version-20110913-ANX.Fei
		SP_TX_Show_Infomation();
                 SP_TX_Video_Mute(0);
		SP_CTRL_Set_System_State(SP_TX_PLAY_BACK);
	}
}

void SP_CTRL_TimerProcess (void) 
{
	printk(KERN_INFO "\n7805 : %s, system_state : %d\n", __func__, sp_tx_system_state);
	switch(sp_tx_system_state) {
		case SP_TX_INITIAL:
			break;		
		case SP_TX_WAIT_SLIMPORT_PLUGIN:
			printk(KERN_INFO "7805 : SP_TX_WAIT_SLIMPORT_PLUGIN\n");
			SP_CTRL_Slimport_Plug_Process();
			break;
		case SP_TX_PARSE_EDID:
			SP_CTRL_EDID_Process();
			break;
		case SP_TX_CONFIG_VIDEO_INPUT:
			slimport_config_video_input();
			break;
		case SP_TX_LINK_TRAINING:
			if(SP_TX_HW_Link_Training())
				return;
			break;
		case SP_TX_CONFIG_VIDEO_OUTPUT:
			slimport_config_video_output();			
			break;
		case SP_TX_CONFIG_AUDIO:
			SP_TX_Config_Audio(&SP_TX_Audio_Input);
			break;
		case SP_TX_HDCP_AUTHENTICATION:
			slimport_hdcp_authentication((HDCP_EN == 0 ? 0 : 1));
			break;
		case SP_TX_PLAY_BACK:
			SP_CTRL_PlayBack_Process();
			break;
		default:
			break;
	}
}

void SP_CTRL_Main_Procss(void)
{
#if(REDUCE_REPEAT_PRINT_INFO)
	confirm_loop_print_msg();
#endif
    SP_CTRL_TimerProcess();

#ifndef EXT_INT
    SP_CTRL_Int_Process();
#else

    if(ext_int_index)
     {
     	ext_int_index = 0;
	SP_CTRL_Int_Process();
     }
#endif
	
}

#undef _SP_TX_DRV_C_

