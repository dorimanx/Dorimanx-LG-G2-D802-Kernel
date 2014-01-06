#ifndef __BROADCAST_TCC3530_H__
#define __BROADCAST_TCC3530_H__

int tcc353x_power_on(void);
int tcc353x_power_off(void);
int tcc353x_is_power_on(void);
int tcc353x_select_antenna(unsigned int sel);

#endif /*__BROADCAST_TCC3530_H__*/
