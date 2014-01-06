/*--------------------------------------------------------------------------*/
/*    FileName    : Tcc353x_register_control.h                              */
/*    Description : Register control                                        */
/*--------------------------------------------------------------------------*/
/*                                                                          */
/*   TCC Version : 1.0.0                                                    */
/*   Copyright (c) Telechips, Inc.                                          */
/*   ALL RIGHTS RESERVED                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

#ifndef __TCC353X_REGISTER_CONTROL_H__
#define __TCC353X_REGISTER_CONTROL_H__

#include "tcc353x_common.h"
#include "tcc353x_core.h"

#define _LOCK_      0
#define _UNLOCK_    1

#define Bit31       0x80000000
#define Bit30       0x40000000
#define Bit29       0x20000000
#define Bit28       0x10000000
#define Bit27       0x08000000
#define Bit26       0x04000000
#define Bit25       0x02000000
#define Bit24       0x01000000
#define Bit23       0x00800000
#define Bit22       0x00400000
#define Bit21       0x00200000
#define Bit20       0x00100000
#define Bit19       0x00080000
#define Bit18       0x00040000
#define Bit17       0x00020000
#define Bit16       0x00010000
#define Bit15       0x00008000
#define Bit14       0x00004000
#define Bit13       0x00002000
#define Bit12       0x00001000
#define Bit11       0x00000800
#define Bit10       0x00000400
#define Bit9        0x00000200
#define Bit8        0x00000100
#define Bit7        0x00000080
#define Bit6        0x00000040
#define Bit5        0x00000020
#define Bit4        0x00000010
#define Bit3        0x00000008
#define Bit2        0x00000004
#define Bit1        0x00000002
#define Bit0        0x00000001
#define BitNONE     0x00000000
#define BitFF       0x000000FF

/*--------------------------------------------------------------------------*/
/* System Control Register                                                  */

#define TC3XREG_SYS_EN                  0x00
#define TC3XREG_SYS_EN_OPCLK            Bit4
#define TC3XREG_SYS_EN_PWSAVE           Bit3
#define TC3XREG_SYS_EN_RF               Bit2
#define TC3XREG_SYS_EN_DSP              Bit1
#define TC3XREG_SYS_EN_EP               Bit0

#define TC3XREG_SYS_RESET               0x01
#define TC3XREG_SYS_RESET_PWSAVE        Bit3
#define TC3XREG_SYS_RESET_RF            Bit2
#define TC3XREG_SYS_RESET_DSP           Bit1
#define TC3XREG_SYS_RESET_EP            Bit0

#define TC3XREG_IRQ_MODE                0x02
#define TC3XREG_IRQ_MODE_PAD_ENABLE     Bit2
#define TC3XREG_IRQ_MODE_EDGE           Bit1
#define TC3XREG_IRQ_MODE_LEVEL          BitNONE
#define TC3XREG_IRQ_MODE_RISING         Bit0
#define TC3XREG_IRQ_MODE_FALLING        BitNONE

#define TC3XREG_IRQ_EN                  0x03
#define TC3XREG_IRQ_EN_FIFODINIT        Bit7
#define TC3XREG_IRQ_EN_FIFOCINIT        Bit6
#define TC3XREG_IRQ_EN_FIFOBINIT        Bit5
#define TC3XREG_IRQ_EN_FIFOAINIT        Bit4
#define TC3XREG_IRQ_EN_DATAINT          Bit3
#define TC3XREG_IRQ_EN_QUE_OVER         Bit1
#define TC3XREG_IRQ_EN_FIFO_OVER        Bit0

#define TC3XREG_IRQ_STAT_CLR            0x04
#define TC3XREG_IRQ_STAT_FIFODINIT      Bit7
#define TC3XREG_IRQ_STAT_FIFOCINIT      Bit6
#define TC3XREG_IRQ_STAT_FIFOBINIT      Bit5
#define TC3XREG_IRQ_STAT_FIFOAINIT      Bit4
#define TC3XREG_IRQ_STAT_DATAINT        Bit3
#define TC3XREG_IRQ_STAT_OPINT          Bit2
#define TC3XREG_IRQ_STAT_QUE_OVER       Bit1
#define TC3XREG_IRQ_STAT_FIFO_OVER      Bit0
#define TC3XREG_IRQ_STATCLR_ALL         BitFF

#define TC3XREG_IRQ_ERROR               0x05
#define TC3XREG_IRQ_ERROR_FIFOD         Bit7
#define TC3XREG_IRQ_ERROR_FIFOC         Bit6
#define TC3XREG_IRQ_ERROR_FIFOB         Bit5
#define TC3XREG_IRQ_ERROR_FIFOA         Bit4
#define TC3XREG_IRQ_ERROR_DATA          Bit3
#define TC3XREG_IRQ_ERROR_OPINT         Bit2
#define TC3XREG_IRQ_ERROR_I2C           Bit1
#define TC3XREG_IRQ_ERROR_MAILBOX       Bit0

#define TC3XREG_PLL_6                   0x06
#define TC3XREG_PLL_7                   0x07
#define TC3XREG_PLL_8                   0x08
#define TC3XREG_PLL_9                   0x09

#define TC3XREG_PROGRAMID               0x0b
#define TC3XREG_CHIPID                  0x0c
#define TC3XREG_INIT_REMAP              0x0d
#define TC3XREG_INIT_PC8                0x0e
#define TC3XREG_INIT_PC0                0x0f

/*--------------------------------------------------------------------------*/
/* GPIO Control Register                                                    */

#define TC3XREG_GPIO_ALT                0x10
#define TC3XREG_GPIO_DR                 0x11
#define TC3XREG_GPIO_LR                 0x12
#define TC3XREG_GPIO_DRV                0x13
#define TC3XREG_GPIO_PE                 0x14
#define TC3XREG_GPIOS_DRV               0x15
#define TC3XREG_IO_MISC                 0x16
#define TC3XREG_IOCFG_MUX               0x17

/*--------------------------------------------------------------------------*/
/* POWER SAVE Control Register                                              */

#define TC3XREG_PSAVE_CFG0              0x18
#define TC3XREG_PSAVE_CFG1              0x19

/*--------------------------------------------------------------------------*/
/* Stream Data Register                                                     */

#define TC3XREG_STREAM_CFG0             0x1b
#define TC3XREG_STREAM_CFG1             0x1c
#define TC3XREG_STREAM_CFG2             0x1d

#define TC3XREG_STREAM_CFG3             0x1e
#define TC3XREG_STREAM_DATA_ENABLE      Bit7
#define TC3XREG_STREAM_DATA_FIFO_INIT   Bit4
#define TC3XREG_STREAM_DATA_FIFO_EN     Bit1

#define TC3XREG_STREAM_CFG4             0x1f

/*--------------------------------------------------------------------------*/
/* Command DMA Register                                                     */

#define TC3XREG_CMDDMA_CTRL                 0x20
#define TC3XREG_CMDDMA_DMAEN                Bit7
#define TC3XREG_CMDDMA_CIRCULARMODE         Bit5
#define TC3XREG_CMDDMA_CRC32EN              Bit4
#define TC3XREG_CMDDMA_ADDRFIX              Bit3
#define TC3XREG_CMDDMA_WRITEMODE            Bit2
#define TC3XREG_CMDDMA_READMODE             BitNONE
#define TC3XREG_CMDDMA_BYTEMSB              Bit1
#define TC3XREG_CMDDMA_BITMSB               Bit0

#define TC3XREG_CMDDMA_SADDR_24             0x21
#define TC3XREG_CMDDMA_SADDR_16             0x22
#define TC3XREG_CMDDMA_SADDR_8              0x23
#define TC3XREG_CMDDMA_SADDR_0              0x24

#define TC3XREG_CMDDMA_SIZE8                0x27
#define TC3XREG_CMDDMA_SIZE0                0x28

#define TC3XREG_CMDDMA_STARTCTRL            0x29
#define TC3XREG_CMDDMA_START_AUTOCLR        Bit7
#define TC3XREG_CMDDMA_CRC32INIT_AUTOCLR    Bit1
#define TC3XREG_CMDDMA_INIT_AUTOCLR         Bit0

#define TC3XREG_CMDDMA_DATA_WIND            0x2a
#define TC3XREG_TGTBUFF_CIR_MODE            0x2b
#define TC3XREG_TGTBUFF_DEFAULT             BitNONE
#define TC3XREG_TGTBUFF_CIR_MODE_C          (Bit0|Bit1)
#define TC3XREG_TGTBUFF_CIR_MODE_B          Bit1
#define TC3XREG_TGTBUFF_CIR_MODE_A          Bit0

#define TC3XREG_CMDDMA_CRC24                0x2c
#define TC3XREG_CMDDMA_CRC16                0x2d
#define TC3XREG_CMDDMA_CRC8                 0x2e
#define TC3XREG_CMDDMA_CRC0                 0x2f

/*--------------------------------------------------------------------------*/
/* PERIperal for stream data Register                                       */

#define TC3XREG_PERI_CTRL                   0x30
#define TC3XREG_PERI_EN                     Bit7
#define TC3XREG_PERI_SEL_SPI                Bit4
#define TC3XREG_PERI_SEL_TS                 Bit5
#define TC3XREG_PERI_SEL_HPI                (Bit4|Bit5)
#define TC3XREG_PERI_SEL_OTHER              BitNONE
#define TC3XREG_PERI_INIT_AUTOCLR           Bit1
#define TC3XREG_PERI_HEADERON               Bit0

#define TC3XREG_PERI_MODE0                  0x31
#define TC3XREG_PERI_SPI_TISSP              Bit7
#define TC3XREG_PERI_SPI_MOTOROLA_SSP       BitNONE
#define TC3XREG_PERI_SPI_SLAVE              Bit6
#define TC3XREG_PERI_SPI_MASTER             BitNONE
#define TC3XREG_PERI_SPI_FASTON             Bit5
#define TC3XREG_PERI_SPI_SIZE8BIT           Bit0
#define TC3XREG_PERI_SPI_SIZE16BIT          Bit1
#define TC3XREG_PERI_SPI_SIZE32BIT          BitNONE
#define TC3XREG_PERI_TS_MSM_SLAVE           Bit1
#define TC3XREG_PERI_TS_NORMAL_SLAVE        BitNONE
#define TC3XREG_PERI_TS_STS                 BitNONE
#define TC3XREG_PERI_TS_PTS                 Bit6
#define TC3XREG_PERI_TS_FASTON              Bit5
#define TC3XREG_PERI_TS_TSCLKMASKON         Bit4
#define TC3XREG_PERI_HPI_INTEL              Bit7
#define TC3XREG_PERI_HPI_MOTOROLA           BitNONE
#define TC3XREG_PERI_HPI_BYTEMSB            Bit6
#define TC3XREG_PERI_HPI_BITMSB             Bit5

#define TC3XREG_PERI_MODE1                  0x32
#define TC3XREG_PERI_SPI_CLKINIT_LOW        BitNONE
#define TC3XREG_PERI_SPI_CLKINIT_HIGH       Bit7
#define TC3XREG_PERI_SPI_CLKPOL_POS         BitNONE
#define TC3XREG_PERI_SPI_CLKPOL_NEG         Bit6
#define TC3XREG_PERI_SPI_BYTEMSB1           Bit5
#define TC3XREG_PERI_SPI_BITMSB1            Bit4
#define TC3XREG_PERI_TS_CLKPHASE_POS        BitNONE
#define TC3XREG_PERI_TS_CLKPHASE_NEG        Bit7
#define TC3XREG_PERI_TS_SYNC_ACTIVEHIGH     BitNONE
#define TC3XREG_PERI_TS_SYNC_ACTIVELOW      Bit6
#define TC3XREG_PERI_TS_ENPOL_ACTIVEHIGH    BitNONE
#define TC3XREG_PERI_TS_ENPOL_ACTIVELOW     Bit5
#define TC3XREG_PERI_TS_STREAM_WAIT_ON      Bit4

#define TC3XREG_PERI_MODE2                  0x33
#define TC3XREG_PERI_TS_BYTEMSB             Bit7
#define TC3XREG_PERI_TS_BITMSB              Bit6

#define TC3XREG_PERI_MODE3                  0x34
#define TC3XREG_PERI_TS_ERR_POL             Bit1
#define TC3XREG_PERI_TS_ERR_SIG_ON          Bit0
#define TC3XREG_PERI_TS_ERR_SIG_OFF         BitNONE

/*--------------------------------------------------------------------------*/
/* MAILBOX Register                                                         */

#define TC3XREG_MAIL_CTRL                   0x3c
#define TC3XREG_MAIL_INIT                   Bit6
#define TC3XREG_MAIL_HOSTMAILPOST           Bit5
#define TC3XREG_MAIL_OPACCEPTEN             Bit4
#define TC3XREG_MAIL_FIFO_R                 0x3d
#define TC3XREG_MAIL_FIFO_W                 0x3e
#define TC3XREG_MAIL_FIFO_WIND              0x3f

/*--------------------------------------------------------------------------*/
/* I2C Master/Slave Register                                                */

#define TC3XREG_I2CMST_CTRL                 0x40
#define TC3XREG_I2CMST_INIT                 Bit7
#define TC3XREG_I2CMST_EN                   Bit2
#define TC3XREG_I2CMST_NORMALMODE           BitNONE
#define TC3XREG_I2CMST_AUTOMODE             Bit1
#define TC3XREG_I2CMST_SCL_CMOS             BitNONE
#define TC3XREG_I2CMST_SCL_OPENDRAIN        Bit0

#define TC3XREG_I2CMST_FILTER               0x41
#define TC3XREG_I2CMST_PRER_H               0x42
#define TC3XREG_I2CMST_PRER_L               0x43

#define TC3XREG_I2CMST_DEV_ADDR             0x44
#define TC3XREG_I2CMST_DEV_ADDR_WRITE       BitNONE
#define TC3XREG_I2CMST_DEV_ADDR_READ        Bit0

#define TC3XREG_I2CMST_TGT_ADDR             0x45
#define TC3XREG_I2CMST_TXRX                 0x46

#define TC3XREG_I2CMST_AUTOCFG              0x47
#define TC3XREG_I2CMST_AUTOCFG_CMDQUE_ADD   Bit7
#define TC3XREG_I2CMST_AUTOCFG_CMDQUE_INIT  Bit6
#define TC3XREG_I2CMST_AUTOCFG_CMDQUE_CNT4  Bit4
#define TC3XREG_I2CMST_AUTOCFG_CMDQUE_CNT3  Bit3
#define TC3XREG_I2CMST_AUTOCFG_CMDQUE_CNT2  Bit2
#define TC3XREG_I2CMST_AUTOCFG_CMDQUE_CNT1  Bit1
#define TC3XREG_I2CMST_AUTOCFG_CMDQUE_CNT0  Bit0

#define TC3XREG_I2CMST_START                0x48
#define TC3XREG_I2CMST_AUTOSTART            Bit7
#define TC3XREG_I2CMST_NORMALSTART          Bit4
#define TC3XREG_I2CMST_NORMALSTOP           Bit3
#define TC3XREG_I2CMST_NORMALREAD           Bit2
#define TC3XREG_I2CMST_NORMALWRITE          Bit1
#define TC3XREG_I2CMST_NORMALACK            Bit0

#define TC3XREG_I2CMST_STAT0                0x49
#define TC3XREG_I2CMST_STAT0_NORMAL_ACKIN   Bit7
#define TC3XREG_I2CMST_STAT0_NORMAL_DONE    Bit6
#define TC3XREG_I2CMST_STAT0_ARBIT_LOST     Bit5
#define TC3XREG_I2CMST_STAT0_SDA            Bit1
#define TC3XREG_I2CMST_STAT0_SCL            Bit0

#define TC3XREG_I2CMST_STAT1                0x4A
#define TC3XREG_I2CMST_STAT1_AUTOERR        Bit7
#define TC3XREG_I2CMST_STAT1_AUTODONE       Bit6
#define TC3XREG_I2CMST_STAT1_ARBIT_LOST     Bit5
#define TC3XREG_I2CMST_STAT1_RXRFIFOCNT4    Bit4
#define TC3XREG_I2CMST_STAT1_RXRFIFOCNT3    Bit3
#define TC3XREG_I2CMST_STAT1_RXRFIFOCNT2    Bit2
#define TC3XREG_I2CMST_STAT1_RXRFIFOCNT1    Bit1
#define TC3XREG_I2CMST_STAT1_RXRFIFOCNT0    Bit0

/*--------------------------------------------------------------------------*/
/* STREAM Mixer Register                                                    */

#define TC3XREG_STREAMMIX_CFG0              0x4D

/*--------------------------------------------------------------------------*/
/* OUTPUT Buffer Management Register                                        */

#define TC3XREG_OBUFF_CFG                   0x4e
#define TC3XREG_OBUFF_CFG_BUFF_D_EN         Bit7
#define TC3XREG_OBUFF_CFG_BUFF_C_EN         Bit6
#define TC3XREG_OBUFF_CFG_BUFF_B_EN         Bit5
#define TC3XREG_OBUFF_CFG_BUFF_A_EN         Bit4
#define TC3XREG_OBUFF_CFG_BUFF_D_CIRCULAR   BitNONE
#define TC3XREG_OBUFF_CFG_BUFF_C_CIRCULAR   BitNONE
#define TC3XREG_OBUFF_CFG_BUFF_B_CIRCULAR   BitNONE
#define TC3XREG_OBUFF_CFG_BUFF_A_CIRCULAR   BitNONE
#define TC3XREG_OBUFF_CFG_BUFF_D_FIFO       Bit3
#define TC3XREG_OBUFF_CFG_BUFF_C_FIFO       Bit2
#define TC3XREG_OBUFF_CFG_BUFF_B_FIFO       Bit1
#define TC3XREG_OBUFF_CFG_BUFF_A_FIFO       Bit0

#define TC3XREG_OBUFF_INIT                  0x4f
#define TC3XREG_OBUFF_DBUFF_STATLATCH       Bit7
#define TC3XREG_OBUFF_CBUFF_STATLATCH       Bit6
#define TC3XREG_OBUFF_BBUFF_STATLATCH       Bit5
#define TC3XREG_OBUFF_ABUFF_STATLATCH       Bit4
#define TC3XREG_OBUFF_BUFF_D_INIT           Bit3
#define TC3XREG_OBUFF_BUFF_C_INIT           Bit2
#define TC3XREG_OBUFF_BUFF_B_INIT           Bit1
#define TC3XREG_OBUFF_BUFF_A_INIT           Bit0

#define TC3XREG_OBUFF_A_SADDR0              0x50
#define TC3XREG_OBUFF_A_SADDR1              0x51
#define TC3XREG_OBUFF_A_EADDR0              0x52
#define TC3XREG_OBUFF_A_EADDR1              0x53

#define TC3XREG_OBUFF_A_FIFO_THR0           0x54
#define TC3XREG_OBUFF_A_FIFO_THR1           0x55

#define TC3XREG_OBUFF_A_CIRBUFF_DATA_ADDR0  0x54
#define TC3XREG_OBUFF_A_CIRBUFF_DATA_ADDR1  0x55

#define TC3XREG_OBUFF_A_FIFO_STAT0          0x56
#define TC3XREG_OBUFF_A_FIFO_STAT1          0x57

#define TC3XREG_OBUFF_A_CIRBUFF_DATA_SIZE0  0x56
#define TC3XREG_OBUFF_A_CIRBUFF_DATA_SIZE1  0x57

#define TC3XREG_OBUFF_B_SADDR0              0x58
#define TC3XREG_OBUFF_B_SADDR1              0x59
#define TC3XREG_OBUFF_B_EADDR0              0x5a
#define TC3XREG_OBUFF_B_EADDR1              0x5b

#define TC3XREG_OBUFF_B_FIFO_THR0           0x5c
#define TC3XREG_OBUFF_B_FIFO_THR1           0x5d

#define TC3XREG_OBUFF_B_CIRBUFF_DATA_ADDR0  0x5c
#define TC3XREG_OBUFF_B_CIRBUFF_DATA_ADDR1  0x5d

#define TC3XREG_OBUFF_B_FIFO_STAT0          0x5e
#define TC3XREG_OBUFF_B_FIFO_STAT1          0x5f

#define TC3XREG_OBUFF_B_CIRBUFF_DATA_SIZE0  0x5e
#define TC3XREG_OBUFF_B_CIRBUFF_DATA_SIZE1  0x5f

#define TC3XREG_OBUFF_C_SADDR0              0x60
#define TC3XREG_OBUFF_C_SADDR1              0x61
#define TC3XREG_OBUFF_C_EADDR0              0x62
#define TC3XREG_OBUFF_C_EADDR1              0x63

#define TC3XREG_OBUFF_C_FIFO_THR0           0x64
#define TC3XREG_OBUFF_C_FIFO_THR1           0x65

#define TC3XREG_OBUFF_C_CIRBUFF_DATA_ADDR0  0x64
#define TC3XREG_OBUFF_C_CIRBUFF_DATA_ADDR1  0x65

#define TC3XREG_OBUFF_C_FIFO_STAT0          0x66
#define TC3XREG_OBUFF_C_FIFO_STAT1          0x67

#define TC3XREG_OBUFF_C_CIRBUFF_DATA_SIZE0  0x66
#define TC3XREG_OBUFF_C_CIRBUFF_DATA_SIZE1  0x67

#define TC3XREG_OBUFF_D_SADDR0              0x68
#define TC3XREG_OBUFF_D_SADDR1              0x69
#define TC3XREG_OBUFF_D_EADDR0              0x6a
#define TC3XREG_OBUFF_D_EADDR1              0x6b

#define TC3XREG_OBUFF_D_FIFO_THR0           0x6c
#define TC3XREG_OBUFF_D_FIFO_THR1           0x6d

#define TC3XREG_OBUFF_D_CIRBUFF_DATA_ADDR0  0x6c
#define TC3XREG_OBUFF_D_CIRBUFF_DATA_ADDR1  0x6d

#define TC3XREG_OBUFF_D_FIFO_STAT0          0x6e
#define TC3XREG_OBUFF_D_FIFO_STAT1          0x6f

#define TC3XREG_OBUFF_D_CIRBUFF_DATA_SIZE0  0x6e
#define TC3XREG_OBUFF_D_CIRBUFF_DATA_SIZE1  0x6f

/*--------------------------------------------------------------------------*/
/* RF /SDRAM/ OPCFG Management Register                                     */

#define TC3XREG_MISC_CFG0                   0x70

#define TC3XREG_MISC_CFG1                   0x71
#define TC3XREG_MISC_ACTION                 Bit0

#define TC3XREG_MISC_CFG2                   0x72
#define TC3XREG_MISC_CFG3                   0x73
#define TC3XREG_MISC_CFG4                   0x74
#define TC3XREG_MISC_CFG5                   0x75
#define TC3XREG_MISC_CFG6                   0x76
#define TC3XREG_OP_DEBUG0                   0x78
#define TC3XREG_OP_DEBUG1                   0x79
#define TC3XREG_OP_DEBUG2                   0x7a
#define TC3XREG_OP_LDO_CONFIG               0x7b
#define TC3XREG_OP_XTAL_BIAS                0x7c
#define TC3XREG_OP_XTAL_BIAS_KEY            0x7d
#define TC3XREG_OP_STATUS0                  0x7e
#define TC3XREG_OP_STATUS1                  0x7f

/*--------------------------------------------------------------------------*/
/* OPCFG Register                                                           */

#define TC3XREG_OP_CFG00                    0x00
#define TC3XREG_OP_CFG01                    0x01
#define TC3XREG_OP_CFG02                    0x02
#define TC3XREG_OP_CFG03                    0x03
#define TC3XREG_OP_CFG04                    0x04
#define TC3XREG_OP_CFG05                    0x05
#define TC3XREG_OP_CFG06                    0x06
#define TC3XREG_OP_CFG07                    0x07
#define TC3XREG_OP_CFG08                    0x08
#define TC3XREG_OP_CFG09                    0x09
#define TC3XREG_OP_CFG10                    0x0A
#define TC3XREG_OP_CFG11                    0x0B
#define TC3XREG_OP_CFG12                    0x0C
#define TC3XREG_OP_CFG13                    0x0D
#define TC3XREG_OP_CFG14                    0x0E
#define TC3XREG_OP_CFG15                    0x0F

#define TC3XREG_OP_PID00                    0x10
#define TC3XREG_OP_PID02                    0x11
#define TC3XREG_OP_PID04                    0x12
#define TC3XREG_OP_PID06                    0x13
#define TC3XREG_OP_PID08                    0x14
#define TC3XREG_OP_PID10                    0x15
#define TC3XREG_OP_PID12                    0x16
#define TC3XREG_OP_PID14                    0x17
#define TC3XREG_OP_PID16                    0x18
#define TC3XREG_OP_PID18                    0x19
#define TC3XREG_OP_PID20                    0x1a
#define TC3XREG_OP_PID22                    0x1b
#define TC3XREG_OP_PID24                    0x1c
#define TC3XREG_OP_PID26                    0x1d
#define TC3XREG_OP_PID28                    0x1e
#define TC3XREG_OP_PID30                    0x1f

#define TC3XREG_OP_CAS_PID0100              0x20
#define TC3XREG_OP_CAS_PID0302              0x21

#define TC3XREG_OP_CAS_PID0O_ODD_KEY_L      0x22
#define TC3XREG_OP_CAS_PID0O_ODD_KEY_H      0x23

#define TC3XREG_OP_CAS_PID0O_EVEN_KEY_L     0x24
#define TC3XREG_OP_CAS_PID0O_EVEN_KEY_H     0x25

#define TC3XREG_OP_CAS_PID01_ODD_KEY_L      0x26
#define TC3XREG_OP_CAS_PID01_ODD_KEY_H      0x27

#define TC3XREG_OP_CAS_PID01_EVEN_KEY_L     0x28
#define TC3XREG_OP_CAS_PID01_EVEN_KEY_H     0x29

#define TC3XREG_OP_CAS_PID02_ODD_KEY_L      0x2a
#define TC3XREG_OP_CAS_PID02_ODD_KEY_H      0x2b

#define TC3XREG_OP_CAS_PID02_EVEN_KEY_L     0x2c
#define TC3XREG_OP_CAS_PID02_EVEN_KEY_H     0x2d

#define TC3XREG_OP_CAS_PID03_ODD_KEY_L      0x2e
#define TC3XREG_OP_CAS_PID03_ODD_KEY_H      0x2f

#define TC3XREG_OP_CAS_PID03_EVEN_KEY_L     0x30
#define TC3XREG_OP_CAS_PID03_EVEN_KEY_H     0x31

#define TC3XREG_OP_CAS_SYSTEM_KEY0          0x32
#define TC3XREG_OP_CAS_IV0                  0x3a
#define TC3XREG_OP_FILTER_CFG               0x3C


/*--------------------------------------------------------------------------*/
/* Functions                                                                */

I32S Tcc353xSetRegManual(Tcc353xHandle_t * _handle, I08U _addr,
			 I08U * _data, I32S _size);
I32S Tcc353xGetRegManual(Tcc353xHandle_t * _handle, I08U _addr, I32S _size,
			 I08U * _data);
I32S Tcc353xSetRegSysEnable(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegSysReset(Tcc353xHandle_t * _handle, I08U _value, 
			   I08U _unlock);
I32S Tcc353xSetRegIrqMode(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegIrqEnable(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegIrqClear(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xGetRegIrqStatus(Tcc353xHandle_t * _handle, I08U * _data);
I32S Tcc353xSetRegIrqErrorClear(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xGetRegIrqError(Tcc353xHandle_t * _handle, I08U * _data);
I32S Tcc353xSetRegPll6(Tcc353xHandle_t * _handle, I08U _value, I08U _unlock);
I32S Tcc353xSetRegPll7(Tcc353xHandle_t * _handle, I08U _value, I08U _unlock);
I32S Tcc353xSetRegPll8(Tcc353xHandle_t * _handle, I08U _value, I08U _unlock);
I32S Tcc353xSetRegPll9(Tcc353xHandle_t * _handle, I08U _value, I08U _unlock);
I32S Tcc353xSetRegRemap(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegRemapPc(Tcc353xHandle_t * _handle, I08U * _data,
			  I32S _size);
I32S Tcc353xGetRegProgramId(Tcc353xHandle_t * _handle, I08U * _data);
I32S Tcc353xGetRegChipId(Tcc353xHandle_t * _handle, I08U * _data);
I32S Tcc353xSetRegGpioAlt(Tcc353xHandle_t * _handle, I08U _data);
I32S Tcc353xSetRegGpioDR(Tcc353xHandle_t * _handle, I08U _data);
I32S Tcc353xGetRegGpioLR(Tcc353xHandle_t * _handle, I08U * _data);
I32S Tcc353xSetRegGpioLR(Tcc353xHandle_t * _handle, I08U _data);
I32S Tcc353xGetRegGpioDR(Tcc353xHandle_t * _handle, I08U * _data);
I32S Tcc353xSetRegGpioDRV(Tcc353xHandle_t * _handle, I08U _data);
I32S Tcc353xSetRegGpioPE(Tcc353xHandle_t * _handle, I08U _data);
I32S Tcc353xSetRegGpiosDRV(Tcc353xHandle_t * _handle, I08U _data);
I32S Tcc353xSetRegIoCfgMux(Tcc353xHandle_t * _handle, I08U _data);
I32S Tcc353xSetRegIoMISC(Tcc353xHandle_t * _handle, I08U _data);
I32S Tcc353xSetRegStreamConfig0(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegStreamConfig1(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegStreamConfig2(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegStreamConfig3(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegStreamConfig(Tcc353xHandle_t * _handle, I08U * _data);
I32S Tcc353xGetRegStreamData(Tcc353xHandle_t * _handle, I08U * _data,
			     I32S _size);
I32S Tcc353xSetRegDataWindow(Tcc353xHandle_t * _handle, I08U * _data,
			     I32S _size, I08U _unlock);
I32S Tcc353xGetRegDataWindow(Tcc353xHandle_t * _handle, I08U * _data,
			     I32S _size, I08U _unlock);
I32S Tcc353xSetRegDmaControl(Tcc353xHandle_t * _handle, I08U _value,
			     I08U _unlock);
I32S Tcc353xSetRegDmaSourceAddress(Tcc353xHandle_t * _handle, I08U * _data,
				   I08U _unlock);
I32S Tcc353xSetRegDmaSize(Tcc353xHandle_t * _handle, I08U * _data,
			  I08U _unlock);
I32S Tcc353xSetRegDmaStartControl(Tcc353xHandle_t * _handle, I08U _value,
				  I08U _unlock);
I32S Tcc353xGetRegDmaCrc32(Tcc353xHandle_t * _handle, I08U * _data);
I32S Tcc353xSetRegPeripheralConfig0(Tcc353xHandle_t * _handle,
				    I08U _value);
I32S Tcc353xSetRegPeripheralConfig(Tcc353xHandle_t * _handle,
				   I08U * _data);
I32S Tcc353xSetRegMailboxControl(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xGetRegMailboxFifoReadStatus(Tcc353xHandle_t * _handle,
					I08U * _data);
I32S Tcc353xGetRegMailboxFifoWriteStatus(Tcc353xHandle_t * _handle,
					 I08U * _data);
I32S Tcc353xSetRegMailboxFifoWindow(Tcc353xHandle_t * _handle,
				    I08U * _data, I32S _size);
I32S Tcc353xGetRegMailboxFifoWindow(Tcc353xHandle_t * _handle,
				    I08U * _data, I32S _size);
I32S Tcc353xSetRegOutBufferConfig(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegOutBufferInit(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegOutBufferStartAddressA(Tcc353xHandle_t * _handle,
					 I08U * _data);
I32S Tcc353xSetRegOutBufferEndAddressA(Tcc353xHandle_t * _handle,
				       I08U * _data);
I32S Tcc353xSetRegOutBufferStartAddressB(Tcc353xHandle_t * _handle,
					 I08U * _data);
I32S Tcc353xSetRegOutBufferEndAddressB(Tcc353xHandle_t * _handle,
				       I08U * _data);
I32S Tcc353xSetRegOutBufferStartAddressC(Tcc353xHandle_t * _handle,
					 I08U * _data);
I32S Tcc353xSetRegOutBufferEndAddressC(Tcc353xHandle_t * _handle,
				       I08U * _data);
I32S Tcc353xSetRegOutBufferStartAddressD(Tcc353xHandle_t * _handle,
					 I08U * _data);
I32S Tcc353xSetRegOutBufferEndAddressD(Tcc353xHandle_t * _handle,
				       I08U * _data);
I32S Tcc353xSetRegOutBufferDFifoThr(Tcc353xHandle_t * _handle,
				    I08U * _data);
I32S Tcc353xSetRegOutBufferAFifoThr(Tcc353xHandle_t * _handle,
				    I08U * _data);
I32S Tcc353xSetRegOutBufferBFifoThr(Tcc353xHandle_t * _handle,
				    I08U * _data);
I32S Tcc353xGetRegFifoAStatus(Tcc353xHandle_t * _handle, I08U * _data);
I32S Tcc353xSetRegLdoConfig(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegXtalBias(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xSetRegXtalBiasKey(Tcc353xHandle_t * _handle, I08U _value);
I32S Tcc353xGetRegOPStatus(Tcc353xHandle_t * _handle, I08U * _data, 
			   I32U _dataSize, I08U _unlock);
I32S Tcc353xSetRegMiscConfig(Tcc353xHandle_t * _handle, I08U _value,
			     I08U _unlock);
I32S Tcc353xSetRegMiscAction(Tcc353xHandle_t * _handle, I08U _value,
			     I08U _unlock);
I32S Tcc353xSetRegMiscAddress(Tcc353xHandle_t * _handle, I08U _value,
			      I08U _unlock);
I32S Tcc353xSetRegMiscData(Tcc353xHandle_t * _handle, I08U * _data,
			   I08U _unlock);
I32S Tcc353xGetRegMiscData(Tcc353xHandle_t * _handle, I08U * _data,
			   I08U _unlock);
I32S Tcc353xGetRegOpDebug(Tcc353xHandle_t * _handle, I08U * _data,
			  I08U _unlock);

#endif
