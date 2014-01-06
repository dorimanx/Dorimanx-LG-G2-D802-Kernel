/*
 *  felica_test.h
 *
 */


#ifndef __FELICA_TEST_H__
#define __FELICA_TEST_H__

#ifdef __cplusplus
extern "C" {
#endif
/*
 *  INCLUDE FILES FOR MODULE
 *
 */
#include <linux/list.h>

#include "felica_common.h"
#include "felica.h"
#include "felica_pon.h"
#include "felica_cen.h"
#include "felica_rfs.h"
#include "felica_rws.h"

/*
 *  DEFINE
 */

/* FUNCTION FEATURE */
//#define FELICA_FN_DEVICE_TEST

/*
 *  ENUM
 */
enum{
  FFI = 0,
  FFO,
  PFO,
  CFI,
  RFI,
  RWFI
};

enum{
  FN_DEVICE_FT_DISABLE_OPEN_UART = 0,
  FN_DEVICE_FT_DISABLE_CLOSE_UART,
  FN_DEVICE_FT_DISABLE_AVAILABLE_UART,
  FN_DEVICE_FT_DISABLE_READ_UART,
  FN_DEVICE_FT_DISABLE_WRITE_UART
};

enum{
  FN_DEVICE_FT_DISABLE_OPEN_PON = 0,
  FN_DEVICE_FT_DISABLE_CLOSE_PON,
  FN_DEVICE_FT_DISABLE_WRITE_PON
};

enum{
  FN_DEVICE_FT_DISABLE_OPEN_CEN = 0,
  FN_DEVICE_FT_DISABLE_CLOSE_CEN,
  FN_DEVICE_FT_DISABLE_READ_CEN,
  FN_DEVICE_FT_DISABLE_WRITE_CEN
};

enum{
  FN_DEVICE_FT_DISABLE_OPEN_RFS = 0,
  FN_DEVICE_FT_DISABLE_CLOSE_RFS,
  FN_DEVICE_FT_DISABLE_READ_RFS,
  FN_DEVICE_FT_DISABLE_WRITE_RFS
};

enum{
  FN_DEVICE_FT_DISABLE_OPEN_RWS = 0,
  FN_DEVICE_FT_DISABLE_CLOSE_RWS,
  FN_DEVICE_FT_DISABLE_READ_RWS,
  FN_DEVICE_FT_DISABLE_WRITE_RWS
};

/*
 *  DEFINE
 */
#define FN_DEVICE_TEST_ON  1
#define FN_DEVICE_TEST_OFF  0

//UART
extern int result_open_uart;
extern int result_close_uart;
extern int result_available_uart;
extern int result_read_uart;
extern int result_write_uart;
//PON
extern int result_open_pon;
extern int result_close_pon;
extern int result_write_pon;
//RWS
extern int result_open_rws;
extern int result_close_rws;
extern int result_read_rws;
//RFS
extern int result_open_rfs;
extern int result_close_rfs;
extern int result_read_rfs;

//CEN
extern int result_open_cen;
extern int result_close_cen;
extern int result_read_cen;

/*
 *  EXTERNAL FUNCTION PROTOTYPE
 */
void disable_open_uart(bool val);
void disable_close_uart(bool val);
void disable_available_uart(bool val);
void disable_read_uart(bool val);
void disable_write_uart(bool val);

void disable_open_pon(bool val);
void disable_close_pon(bool val);
void disable_write_pon(bool val);

void disable_open_cen(bool val);
void disable_close_cen(bool val);
void disable_read_cen(bool val);

void disable_open_rfs(bool val);
void disable_close_rfs(bool val);
void disable_read_rfs(bool val);

void disable_open_rws(bool val);
void disable_close_rws(bool val);
void disable_read_rws(bool val);

#ifdef __cplusplus
}
#endif

#endif // __FELICA_TEST_H__
