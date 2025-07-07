/**
******************************************************************************
* @file    ms7210_mpi.c
* @author  
* @version V1.0.0
* @date    15-Nov-2014
* @brief   MacroSilicon Programming Interface source file
* @history    
*
* Copyright (c) 2009-2014, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#include "ms7210_comm.h"
#include "ms7210_mpi.h"

static UINT8 s_u8chipAddr = MS7210_I2C_ADDR;

VOID ms7210_HAL_SetChipAddr(UINT8 u8_address)
{
    s_u8chipAddr = u8_address;
}

void ms7210_HAL_set_i2c_adapter(struct i2c_client *_i2c_main)
{
	mculib_set_i2c_adapter(_i2c_main);
}

UINT8 ms7210_HAL_ReadByte(UINT16 u16_index)
{
    return mculib_i2c_read_16bidx8bval(s_u8chipAddr, u16_index);
}

VOID ms7210_HAL_WriteByte(UINT16 u16_index, UINT8 u8_value)
{
    mculib_i2c_write_16bidx8bval(s_u8chipAddr, u16_index, u8_value);
}

UINT16 ms7210_HAL_ReadWord(UINT16 u16_index)
{
    return (HAL_ReadByte(u16_index) + ((UINT16)HAL_ReadByte(u16_index + 1) << 8));
}

VOID ms7210_HAL_WriteWord(UINT16 u16_index, UINT16 u16_value)
{
    HAL_WriteByte(u16_index, (UINT8)u16_value);
    HAL_WriteByte(u16_index + 1, (UINT8)(u16_value >> 8));
}

VOID ms7210_HAL_ModBits(UINT16 u16_index, UINT8 u8_mask, UINT8 u8_value)
{
    UINT8 tmp;
    tmp  = HAL_ReadByte(u16_index);
    tmp &= ~u8_mask;
    tmp |= (u8_value & u8_mask);
    HAL_WriteByte(u16_index, tmp);
}

VOID ms7210_HAL_SetBits(UINT16 u16_index, UINT8 u8_mask)
{
    HAL_ModBits(u16_index, u8_mask, u8_mask);
}

VOID ms7210_HAL_ClrBits(UINT16 u16_index, UINT8 u8_mask)
{
    HAL_ModBits(u16_index, u8_mask, 0x00);
}

VOID ms7210_HAL_ToggleBits(UINT16 u16_index, UINT8 u8_mask, BOOL b_set)
{
    (b_set) ? HAL_SetBits(u16_index, u8_mask) : HAL_ClrBits(u16_index, u8_mask);
}

UINT32 ms7210_HAL_ReadDWord(UINT16 u16_index)
{
    return (HAL_ReadWord(u16_index) + ((UINT32)HAL_ReadWord(u16_index + 2) << 16));
}

VOID ms7210_HAL_WriteDWord(UINT16 u16_index, UINT32 u32_value)
{
    HAL_WriteWord(u16_index, (UINT16)u32_value);
    HAL_WriteWord(u16_index + 2, (UINT16)(u32_value >> 16));
}


VOID ms7210_HAL_ReadBytes(UINT16 u16_index, UINT16 u16_length, UINT8 *p_u8_value)
{
    mculib_i2c_burstread_16bidx8bval(s_u8chipAddr, u16_index, u16_length, p_u8_value);
}

VOID ms7210_HAL_WriteBytes(UINT16 u16_index, UINT16 u16_length, UINT8 *p_u8_value)
{
    mculib_i2c_burstwrite_16bidx8bval(s_u8chipAddr, u16_index, u16_length, p_u8_value);
}


#if MS7210_EXT_APIS
UINT32 ms7210_HAL_ReadRange(UINT16 u16_index, UINT8 u8_bitpos, UINT8 u8_length)
{
    UINT32 u32_data = 0;    
    UINT32 u32_mask = 0xffffffff >> (32 - u8_length);
    
    u32_mask <<= u8_bitpos;
    
    u32_data = HAL_ReadDWord(u16_index);
    u32_data &= u32_mask;
    u32_data >>= u8_bitpos;    
    
    return u32_data;
}

VOID ms7210_HAL_WriteRange(UINT16 u16_index, UINT8 u8_bitpos, UINT8 u8_length, UINT32 u32_value)
{
    UINT32 u32_data = HAL_ReadDWord(u16_index);    
    UINT32 u32_val = u32_value << u8_bitpos;
    UINT32 u32_mask = 0xffffffff >> (32 - u8_length);
    
    u32_mask <<= u8_bitpos;
    
    u32_data &= ~u32_mask;  
    u32_val  &= u32_mask; 
    u32_data |= u32_val;
    
    HAL_WriteDWord(u16_index, u32_data);    
}

UINT32 ms7210_HAL_ReadRange_Ex(UINT16 u16_index, UINT8 u8_bitpos, UINT8 u8_length)
{
    UINT32 u32_data = 0;    
    UINT32 u32_mask = 0xffffffff >> (32 - u8_length);
    
    u32_mask <<= u8_bitpos;
    
    u32_data = HAL_ReadDWord_Ex(u16_index);
    u32_data &= u32_mask;
    u32_data >>= u8_bitpos;    
    
    return u32_data;
}

VOID ms7210_HAL_WriteRange_Ex(UINT16 u16_index, UINT8 u8_bitpos, UINT8 u8_length, UINT32 u32_value)
{
    UINT32 u32_data = HAL_ReadDWord_Ex(u16_index);    
    UINT32 u32_val = u32_value << u8_bitpos;
    UINT32 u32_mask = 0xffffffff >> (32 - u8_length);
    
    u32_mask <<= u8_bitpos;
    
    u32_data &= ~u32_mask;  
    u32_val  &= u32_mask; 
    u32_data |= u32_val;
    
    HAL_WriteDWord_Ex(u16_index, u32_data);    
}

#endif

//extend APIs is for HD RX register access only
VOID ms7210_HAL_ModBits_Ex(UINT16 u16_index, UINT32 u32_mask, UINT32 u32_value)
{
    UINT32 tmp;
    tmp  = HAL_ReadDWord_Ex(u16_index);
    tmp &= ~u32_mask;
    tmp |= (u32_value & u32_mask);
    HAL_WriteDWord_Ex(u16_index, tmp);
}

VOID ms7210_HAL_SetBits_Ex(UINT16 u16_index, UINT32 u32_mask)
{
    HAL_ModBits_Ex(u16_index, u32_mask, u32_mask);
}

VOID ms7210_HAL_ClrBits_Ex(UINT16 u16_index, UINT32 u32_mask)
{
    HAL_ModBits_Ex(u16_index, u32_mask, 0x00);
}

VOID ms7210_HAL_ToggleBits_Ex(UINT16 u16_index, UINT32 u32_mask, BOOL b_set)
{
    (b_set) ? HAL_SetBits_Ex(u16_index, u32_mask) : HAL_ClrBits_Ex(u16_index, u32_mask);
}

UINT32 ms7210_HAL_ReadDWord_Ex(UINT16 u16_index)
{
    UINT32 temp;
    UINT8 buff[4];
    HAL_ReadBytes(u16_index, 4, buff);
    temp = buff[3];
    temp = temp << 8 | buff[2];
    temp = temp << 8 | buff[1];
    temp = temp << 8 | buff[0];
    return temp;
}

VOID ms7210_HAL_WriteDWord_Ex(UINT16 u16_index, UINT32 u32_value)
{
    UINT8 buff[4];
    buff[0] = (UINT8)(u32_value);
    buff[1] = (UINT8)(u32_value >> 8);
    buff[2] = (UINT8)(u32_value >> 16);
    buff[3] = (UINT8)(u32_value >> 24);
    HAL_WriteBytes(u16_index, 4, buff);
}
