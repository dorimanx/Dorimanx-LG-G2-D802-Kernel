#ifndef __BROADCAST_t3a00_H__
#define __BROADCAST_t3a00_H__


void tdmb_t3a00_set_userstop(int mode);
int tdmb_t3a00_mdelay(int32 ms);
void tdmb_t3a00_must_mdelay(int32 ms);
int tdmb_t3a00_power_on(void);
int tdmb_t3a00_power_off(void);
int tdmb_t3a00_select_antenna(unsigned int sel);

#if 0
int tdmb_t3a00_i2c_write_burst(uint16 waddr, uint8* wdata, int length);
int tdmb_t3a00_i2c_read_burst(uint16 raddr, uint8* rdata, int length);
int tdmb_t3a00_i2c_write16(unsigned short reg, unsigned short val);
int tdmb_t3a00_i2c_read16(uint16 reg, uint16 *ret);
#endif

void tdmb_t3a00_interrupt_free(void);
void tdmb_t3a00_interrupt_lock(void);
int tdmb_t3a00_spi_write_read(uint8* tx_data, int tx_length, uint8 *rx_data, int rx_length);
#endif /* __BROADCAST_t3a00_H__ */
