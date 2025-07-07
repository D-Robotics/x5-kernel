/**
******************************************************************************
* @file    ms7210_comm.h
* @author  
* @version V1.0.0
* @date    24-Nov-2020
* @brief   This file contains all header files
* @history
*
* Copyright (c) 2009-2014, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#ifndef __MACROSILICON_MS7210_COMMON_H__
#define __MACROSILICON_MS7210_COMMON_H__


#if defined (_PLATFORM_WINDOWS_)
#ifndef MS7210_DRV_EXTERN
    #define MS7210_DRV_EXTERN extern
#endif

#ifndef MS7210_DRV_API
    #define MS7210_DRV_API  __declspec(dllexport)
#endif

#ifndef MS7210_MW_EXTERN
    #define MS7210_MW_EXTERN extern
#endif

#ifndef MS7210_MW_API
    #define MS7210_MW_API  __declspec(dllexport)
#endif

#else //

#ifndef MS7210_DRV_EXTERN
    #define MS7210_DRV_EXTERN extern
#endif

#ifndef MS7210_DRV_API
    #define MS7210_DRV_API  
#endif

#ifndef MS7210_MW_EXTERN
    #define MS7210_MW_EXTERN extern
#endif

#ifndef MS7210_MW_API
    #define MS7210_MW_API 
#endif

#endif //end

//
#if defined (_PLATFORM_ARM_)
#include <stdio.h>
#include <math.h>
#include <string.h>

#elif defined (__STD_GCC__)
#include <stdio.h>
#include <math.h>
#include <string.h>

#elif defined (_PLATFORM_WINDOWS_)
#include <stdio.h>  
#include <math.h>
#include <string.h>
#include <windows.h>

#elif defined (__KEIL_C__)
#include <intrins.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <absacc.h>

#elif defined (__CSMC__)
#include <string.h>
#include <stdio.h>

#elif defined (_IAR_)
#include <string.h>
#include <stdio.h>

#endif


//sdk common header file include
#include "ms7210_typedef.h"

#include <linux/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void mculib_set_i2c_adapter(struct i2c_client *_i2c_main);

//below extern function need be implemented by user 

/***************************************************************
*  Function name:   mculib_chip_reset
*  Description:     ms1850 hardware short reset to chip
*  Entry:           [in]None
* 
*  Returned value:  None
*  Remark:
***************************************************************/
/*extern VOID mculib_chip_reset(VOID);*/


/***************************************************************
*  Function name:   mculib_delay_ms
*  Description:     delay ms
*  Entry:           [in]u8_ms, uint ms
* 
*  Returned value:  None
*  Remark:
***************************************************************/
extern VOID mculib_delay_ms(UINT8 u8_ms);


/***************************************************************
*  Function name:   mculib_delay_us
*  Description:     delay us
*  Entry:           [in]u8_us, uint us
* 
*  Returned value:  None
*  Remark:
***************************************************************/
extern VOID mculib_delay_us(UINT8 u8_us);


/***************************************************************
*  Function name:   mculib_i2c_read_16bidx8bval
*  Description:     read back 8 bits register value with 16 bits specified index
*  Entry:           [in]u8_address: 8 bits I2C slave address
*                   [in]u16_index:  16 bits register index
*
*  Returned value: UINT8 type register value
*  Remark:
***************************************************************/
extern UINT8 mculib_i2c_read_16bidx8bval(UINT8 u8_address, UINT16 u16_index);


/***************************************************************
*  Function name:   mculib_i2c_write_16bidx8bval
*  Description:     write 8 bits register value to 16 bits specified index
*  Entry:           [in]u8_address: 8 bits I2C slave address
*                   [in]u16_index:  16 bits register index
*                   [in]u8_value:   8 bits register value
*
*  Returned value: if write success return TRUE, else return FALSE
*  Remark:
***************************************************************/
extern BOOL mculib_i2c_write_16bidx8bval(UINT8 u8_address, UINT16 u16_index, UINT8 u8_value);

//I2C burst read/write mode 
/***************************************************************
*  Function name:   mculib_i2c_burstread_16bidx8bval
*  Description:     8 bits i2c burst read for 16 bits specified index
*  Entry:           [in]u8_address: 8 bits I2C slave address
*                   [in]u16_index:  16 bits register index
*                   [in]u16_length: 16 bits length to read
*                   [in]pu8_value:  read buffer
*
*  Returned value: if write success return TRUE, else return FALSE
*  Remark:
***************************************************************/
extern VOID  mculib_i2c_burstread_16bidx8bval(UINT8 u8_address, UINT16 u16_index, UINT16 u16_length, UINT8 *pu8_value);

/***************************************************************
*  Function name:   mculib_i2c_burstwrite_16bidx8bval
*  Description:     8 bits i2c burst write for 16 bits specified index
*  Entry:           [in]u8_address: 8 bits I2C slave address
*                   [in]u16_index:  16 bits register index
*                   [in]u16_length: 16 bits length to write
*                   [in]pu8_value:  data buffer to write
*
*  Returned value: if write success return TRUE, else return FALSE
*  Remark:
***************************************************************/
extern VOID  mculib_i2c_burstwrite_16bidx8bval(UINT8 u8_address, UINT16 u16_index, UINT16 u16_length, UINT8 *pu8_value);

/***************************************************************
*  Function name:   mculib_i2c_read_8bidx8bval
*  Description:     read back 8 bits register value with 8 bits specified index
*  Entry:           [in]u8_address: 8 bits I2C slave address
*                   [in]u8_index:  8 bits register index
*
*  Returned value: UINT8 type register value
*  Remark:
***************************************************************/
extern UINT8 mculib_i2c_read_8bidx8bval(UINT8 u8_address, UINT8 u8_index);

/***************************************************************
*  Function name:   mculib_i2c_write_8bidx8bval
*  Description:     write 8 bits register value to 8 bits specified index
*  Entry:           [in]u8_address: 8 bits I2C slave address
*                   [in]u16_index:  8 bits register index
*                   [in]u8_value:   8 bits register value
*
*  Returned value: if write success return TRUE, else return FALSE
*  Remark:
***************************************************************/
extern BOOL mculib_i2c_write_8bidx8bval(UINT8 u8_address, UINT8 u8_index, UINT8 u8_value);

/***************************************************************
*  Function name:   mculib_i2c_burstread_8bidx8bval
*  Description:     8 bits i2c burst read for 8 bits specified index
*  Entry:           [in]u8_address: 8 bits I2C slave address
*                   [in]u16_index:  8 bits register index
*                   [in]u16_length: 8 bits length to read
*                   [in]pu8_value:  read buffer
*
*  Returned value: None
*  Remark:
***************************************************************/
extern VOID  mculib_i2c_burstread_8bidx8bval(UINT8 u8_address, UINT8 u8_index, UINT8 u8_length, UINT8 *pu8_value);
extern VOID  mculib_ddc_i2c_readbytes(UINT8 u8_address, UINT8 u8_index, UINT8 u8_length, UINT8 *pu8_value);

/***************************************************************
*  Function name:   mculib_i2c_set_speed
*  Description:     set i2c speed
*  Entry:           [in]u8speed: 0 - 20Kbps, 1 - 100Kbps
*
*  Returned value: None
*  Remark:
***************************************************************/
extern VOID mculib_i2c_set_speed(UINT8 u8speed);

/***************************************************************
*  Function name:   mculib_i2c_write_blank
*  Description:     
*  Entry:           [in]u8_address
*                   [in]u8_index
*
*  Returned value: BOOL
*  Remark:
***************************************************************/
extern BOOL mculib_i2c_write_blank(UINT8 u8_address, UINT8 u8_index);

/***************************************************************
*  Function name:   mculib_i2c_burstread_8bidx8bval_ext
*  Description:     
*  Entry:           [in]u8_address
*                   [in]u8_index
*                   [in]u8_length
*
*  Returned value: None
*  Remark:
***************************************************************/
extern VOID mculib_i2c_burstread_8bidx8bval_ext(UINT8 u8_address, UINT8 u8_index, UINT8 u8_length);


//end of extern function


//
#define Delay_ms mculib_delay_ms
#define Delay_us mculib_delay_us

#ifdef __cplusplus
}
#endif


#endif //__MACROSILICON_MS7210_COMMON_H__
