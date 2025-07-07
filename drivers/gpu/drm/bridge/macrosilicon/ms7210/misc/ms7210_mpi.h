/******************************************************************************
* @file    ms7210_mpi.h
* @author  
* @version V1.0.0
* @date    15-Nov-2014
* @brief   MacroSilicon Programming Interface.
*
* Copyright (c) 2009-2014, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#ifndef __MACROSILICON_MS7210_PROGRAMMING_INTERFACE_H__
#define __MACROSILICON_MS7210_PROGRAMMING_INTERFACE_H__

#include "ms7210_config.h"


#ifdef __cplusplus
extern "C" {
#endif

#define ms7210_chip_reset mculib_chip_reset

//
#define Delay_ms mculib_delay_ms
#define Delay_us mculib_delay_us

#if MS7210_USE_I2CBUS

#define HAL_SetChipAddr ms7210_HAL_SetChipAddr
#define HAL_ReadByte ms7210_HAL_ReadByte
#define HAL_WriteByte ms7210_HAL_WriteByte
#define HAL_ReadWord ms7210_HAL_ReadWord
#define HAL_WriteWord ms7210_HAL_WriteWord
#define HAL_ModBits ms7210_HAL_ModBits
#define HAL_SetBits ms7210_HAL_SetBits
#define HAL_ClrBits ms7210_HAL_ClrBits
#define HAL_ToggleBits ms7210_HAL_ToggleBits
#define HAL_ReadDWord ms7210_HAL_ReadDWord
#define HAL_WriteDWord ms7210_HAL_WriteDWord
#define HAL_ReadBytes ms7210_HAL_ReadBytes
#define HAL_WriteBytes ms7210_HAL_WriteBytes
#define HAL_ReadRange ms7210_HAL_ReadRange
#define HAL_WriteRange ms7210_HAL_WriteRange
#define HAL_ModBits_Ex ms7210_HAL_ModBits_Ex
#define HAL_SetBits_Ex ms7210_HAL_SetBits_Ex
#define HAL_ClrBits_Ex ms7210_HAL_ClrBits_Ex
#define HAL_ToggleBits_Ex ms7210_HAL_ToggleBits_Ex
#define HAL_ReadDWord_Ex ms7210_HAL_ReadDWord_Ex
#define HAL_WriteDWord_Ex ms7210_HAL_WriteDWord_Ex
#define HAL_ReadRange_Ex ms7210_HAL_ReadRange_Ex
#define HAL_WriteRange_Ex ms7210_HAL_WriteRange_Ex


void ms7210_HAL_set_i2c_adapter(struct i2c_client *_i2c_main);

/***************************************************************
*  Function name: HAL_SetChipAddr
*  Description:   change I2C slave u8_address 
*  Input parameters: uint8_t u8_address: chip slave u8_address
*                    
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_SetChipAddr(uint8_t u8_address);
#endif

/***************************************************************
*  Function name: HAL_ReadByte
*  Description: read back 8 bits register value with 16 bits specified index
*  Input parameters: uint16_t u16_index: 16 bits register index
*  Output parameters: None
*  Returned value: uint8_t type register value
***************************************************************/
uint8_t HAL_ReadByte(uint16_t u16_index);


/***************************************************************
*  Function name: HAL_WriteByte
*  Description: write 8 bits register value to 16 bits specified index
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint8_t  u8_value: 8 bits rgister value
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_WriteByte(uint16_t u16_index, uint8_t u8_value);


/***************************************************************
*  Function name: HAL_ReadWord
*  Description: equal to "HAL_ReadByte(u16_index) + 
*                        (uint16_t)(HAL_ReadByte(u16_index + 1) << 8)"
*  Input parameters: uint16_t u16_index: 16 bits register index
*  Output parameters: None
*  Returned value: uint16_t type register value
***************************************************************/
uint16_t HAL_ReadWord(uint16_t u16_index);


/***************************************************************
*  Function name:  HAL_WriteWord
*  Description: equal to "HAL_WriteByte(u16_index, (uint8_t)u16_value);
                          HAL_WriteByte(u16_index + 1, (uint8_t)(u16_value >> 8))"
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint16_t u16_value: 16 bits rgister value
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_WriteWord(uint16_t u16_index, uint16_t u16_value);


/***************************************************************
*  Function name: HAL_ModBits
*  Description: modify register value with bit mask (high active) for 16 bits index and 8 bits value
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint8_t u8_mask: 8 bits mask 
*                                   can be set by bit mask macro or compsite of macros
*                    uint8_t u8_value: 8 bits value, value with masked bits will be ignored
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_ModBits(uint16_t u16_index, uint8_t u8_mask, uint8_t u8_value);


/***************************************************************
*  Function name: HAL_SetBits
*  Description: set bits with bit mask (high active) for 16 bits index and 8 bits value
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint8_t u8_mask: 8 bits mask 
*                                      can be set by bit mask macro or compsite of macros
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_SetBits(uint16_t u16_index, uint8_t u8_mask);

/***************************************************************
*  Function name:  HAL_ClrBits
*  Description: clear bits with bit mask (high active) for 16 bits index and 8 bits value
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint8_t u8_mask: 8 bits mask 
*                                      can be set by bit mask macro or compsite of macros
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_ClrBits(uint16_t u16_index, uint8_t u8_mask);

/***************************************************************
*  Function name:  HAL_ToggleBits
*  Description: toggle bits with bit mask (high active) for 16 bits index and 8 bits value
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint8_t u8_mask: 8 bits mask 
*                                      can be set by bit mask macro or compsite of macros
*                            bool b_set, if true set to 1 else 0
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_ToggleBits(uint16_t u16_index, uint8_t u8_mask, bool b_set);

//
/***************************************************************
*  Function name:  HAL_ReadDWord
*  Description: read 32 bits value 
*  Input parameters: uint16_t u16_index: 16 bits register index
*  Output parameters: None
*  Returned value: 32 bits value
***************************************************************/
uint32_t HAL_ReadDWord(uint16_t u16_index);

/***************************************************************
*  Function name:  HAL_WriteDWord
*  Description: write 32 bits value 
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint32_t u32_value: 32 bits value
*  Output parameters: None
*  Returned value: 32 bits value
***************************************************************/
void HAL_WriteDWord(uint16_t u16_index, uint32_t u32_value);

//I2C read/write with length
/***************************************************************
*  Function name:  HAL_ReadBytes
*  Description: burst mode read 
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint16_t u16_length: 16 bits length
*                    uint8_t *p_u8_value: data buffer to read
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_ReadBytes(uint16_t u16_index, uint16_t u16_length, uint8_t *p_u8_value);

/***************************************************************
*  Function name:  HAL_WriteBytes
*  Description: burst mode write 
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint16_t u16_length: 16 bits length
*                    uint8_t *p_u8_value: data buffer to write
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_WriteBytes(uint16_t u16_index, uint16_t u16_length, uint8_t *p_u8_value);

#if MS7210_EXT_APIS
/***************************************************************
*  Function name: uint32_t HAL_ReadRange(uint16_t u16_index, uint8_t u8_bitpos, uint8_t u8_length)
*  Description:   read back 32 bits register value by specified index with start bit and length
*                 in case of 16 bits index and 8 bits value
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint8_t u8_bitpos: start bit of fisrt index register, should be less than 8
*                    uint8_t u8_length: register length , should less than 33
*  Output parameters: None
*  Returned value: uint32_t type register value
***************************************************************/
uint32_t HAL_ReadRange(uint16_t u16_index, uint8_t u8_bitpos, uint8_t u8_length);


/***************************************************************
*  Function name: void HAL_WriteRange(uint16_t u16_index, uint8_t u8_bitpos, uint8_t u8_length, uint32_t u32_value)
*  Description:   write 32 bits register value to specified index with start bit and length
*                 in case of 16 bits index and 8 bits value
*  Input parameters: uint16_t u16_index: 16 bits register index
*                    uint8_t u8_bitpos: start bit of fisrt index register, should be less than 8
*                    uint8_t u8_length: register length , should less than 33
*                    uint32_t u32_value: 32 bits register value
*  Output parameters: None
*  Returned value: None
***************************************************************/
void HAL_WriteRange(uint16_t u16_index, uint8_t u8_bitpos, uint8_t u8_length, uint32_t u32_value);
#endif


//extend APIs is for HD RX register access only
void HAL_ModBits_Ex(uint16_t u16_index, uint32_t u32_mask, uint32_t u32_value);
void HAL_SetBits_Ex(uint16_t u16_index, uint32_t u32_mask);
void HAL_ClrBits_Ex(uint16_t u16_index, uint32_t u32_mask);
void HAL_ToggleBits_Ex(uint16_t u16_index, uint32_t u32_mask, bool b_set);
uint32_t HAL_ReadDWord_Ex(uint16_t u16_index);
void HAL_WriteDWord_Ex(uint16_t u16_index, uint32_t u32_value);
uint32_t HAL_ReadRange_Ex(uint16_t u16_index, uint8_t u8_bitpos, uint8_t u8_length);
void HAL_WriteRange_Ex(uint16_t u16_index, uint8_t u8_bitpos, uint8_t u8_length, uint32_t u32_value);


//
#if MS7210_DEBUG_LOG

#ifndef _PLATFORM_WINDOWS_
#define mculib_log printf
#endif

#define MS7210_PRINTF       mculib_log
#define MS7210_LOG(X)       mculib_uart_log((uint8_t*)X)
#define MS7210_LOG1(X, Y)   mculib_uart_log1((uint8_t*)X, Y)
#define MS7210_LOG2(X, Y)   mculib_uart_log2((uint8_t*)X, Y)

#else
#define MS7210_PRINTF
#define MS7210_LOG(X)
#define MS7210_LOG1(X, Y)
#define MS7210_LOG2(X, Y)
#endif

#ifdef __cplusplus
}
#endif

#endif  // __MACROSILICON_MS7210_PROGRAMMING_INTERFACE_H__
