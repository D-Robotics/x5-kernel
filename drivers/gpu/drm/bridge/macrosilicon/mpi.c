/******************************************************************************
* @file    mpi.c
* @author  
* @version V1.0.0
* @date    11-Nov-2014
* @brief   MacroSilicon Programming Interface.
*
* Copyright (c) 2009-2014, MacroSilicon Technology Co.,Ltd.
******************************************************************************/
#include "ms7210_comm.h"
#include <linux/delay.h>
#include <linux/regmap.h>

static struct i2c_client *i2c_main = NULL;

VOID mculib_chip_reset(VOID)
{

}

void mculib_set_i2c_adapter(struct i2c_client *_i2c_main)
{
	if (!_i2c_main) {
		printk("[MS7210] Error: i2c client is NULL\n");
		return;
	}
	i2c_main = _i2c_main;
}

static inline unsigned int ms7210_swap_reg(unsigned int reg)
{
	return (reg >> 8) | (reg << 8);
}

BOOL mculib_chip_read_interrupt_pin(VOID)
{
	printk("mculib_chip_read_interrupt_pin: Not Support...\n");
    return TRUE;
}

VOID mculib_delay_ms(UINT8 u8_ms)
{
	mdelay(u8_ms);
}

VOID mculib_delay_us(UINT8 u8_us)
{
	udelay(u8_us);
}

//beflow APIs is for 16bits I2C slaver address for access ms933x register, must be implemented 
UINT8 mculib_i2c_read_16bidx8bval(UINT8 u8_address, UINT16 u16_index)
{
	u8 val;
	int ret;
	if (!i2c_main) {
		return 0;
	}

	u8 reg_addr_buf[2] = {u16_index & 0xFF, (u16_index >> 8) & 0xFF};
    struct i2c_msg msg[2] = {
        {
            .addr = i2c_main->addr,
            .flags = 0,
            .len = sizeof(reg_addr_buf),
            .buf = reg_addr_buf
        },
        {
            .addr = i2c_main->addr,
            .flags = I2C_M_RD,
            .len = 1,
            .buf = &val
        }
    };

    ret = i2c_transfer(i2c_main->adapter, msg, 2);
    if (ret < 0) {
        printk("I2C transfer error: %d\n", ret);
        return 0;
    } else if (ret != 2) {
        printk("Partial transfer: %d/2 messages\n", ret);
        return 0;
    }

#ifdef DEBUG
	printk("[MS7210] HDMI I2C read: addr[%04x] val[%02x]\n", u16_index, val);
#endif
	return (UINT8)val;
}

BOOL mculib_i2c_write_16bidx8bval(UINT8 u8_address, UINT16 u16_index, UINT8 u8_value)
{
	int ret;
	if (!i2c_main) {
		return FALSE;
	}

	u8 reg_addr_buf[3] = {u16_index & 0xFF, (u16_index >> 8) & 0xFF, u8_value};
    struct i2c_msg msg[1] = {
        {
            .addr = i2c_main->addr,
            .flags = 0,
            .len = sizeof(reg_addr_buf),
            .buf = reg_addr_buf
        },
    };

    ret = i2c_transfer(i2c_main->adapter, msg, 1);
    if (ret < 0) {
        printk("I2C transfer error: %d\n", ret);
        return 0;
    } else if (ret != 1) {
        printk("Partial transfer: %d/1 messages\n", ret);
        return 0;
    }

#ifdef DEBUG
	printk("[MS7210] HDMI I2C write: addr[%04x] val[%02x]\n", u16_index, u8_value);
#endif
    return TRUE;
}

VOID mculib_i2c_burstread_16bidx8bval(UINT8 u8_address, UINT16 u16_index, UINT16 u16_length, UINT8 *pu8_value)
{
    UINT16 i;

    for (i = 0; i < u16_length; i++)
    {
        *(pu8_value + i) = mculib_i2c_read_16bidx8bval(u8_address, u16_index + i);
    }
}

VOID mculib_i2c_burstwrite_16bidx8bval(UINT8 u8_address, UINT16 u16_index, UINT16 u16_length, UINT8 *pu8_value)
{
    UINT16 i;

    for (i = 0; i < u16_length; i++)
    {
        mculib_i2c_write_16bidx8bval(u8_address, u16_index + i, *(pu8_value + i));
    }
}

//beflow APIs is for 8bits I2C slaver address, for HD TX DDC
//if user need use hd tx ddc, must be implemented
//20K is for DDC. 100K is for chip register access
VOID mculib_i2c_set_speed(UINT8 u8_i2c_speed)
{
	printk("mculib_i2c_set_speed[%d]: Not Support...\n", u8_i2c_speed);
}

int i2c_raw_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
    u8 buf[2] = {reg, val};  // 寄存器地址 + 数据
    struct i2c_msg msg = {
        .addr = client->addr,  // 7 位地址
        .flags = 0,            // 写操作
        .len = 2,
        .buf = buf,
    };
    int ret = i2c_transfer(client->adapter, &msg, 1);
    if (ret != 1) {
        printk("i2c_raw_write_reg failed: reg=0x%02x, ret=%d\n", reg, ret);
        return (ret < 0) ? ret : -EIO;
    }
    return 0;
}

/* 读取寄存器 */
int i2c_raw_read_reg(struct i2c_client *client, u8 reg, u8 *val) {
    struct i2c_msg msgs[2] = {
        {  // 写入寄存器地址
            .addr = client->addr,
            .flags = 0, 
            .len = 1,
            .buf = &reg,
        },
        {  // 读取数据
            .addr = client->addr,
            .flags = I2C_M_RD,  // 读操作
            .len = 1,
            .buf = val,
        }
    };
    int ret = i2c_transfer(client->adapter, msgs, 2);
    if (ret != 2) {
        printk("i2c_raw_read_reg failed: reg=0x%02x, ret=%d\n", reg, ret);
        return (ret < 0) ? ret : -EIO;
    }
    return 0;
}

UINT8 mculib_i2c_read_8bidx8bval(UINT8 u8_address, UINT8 u8_index)
{
	UINT8 val;
	i2c_raw_read_reg(i2c_main, u8_index, &val);
    return val;
}

BOOL mculib_i2c_write_8bidx8bval(UINT8 u8_address, UINT8 u8_index, UINT8 u8_value)
{
	i2c_raw_write_reg(i2c_main, u8_index, u8_value);
    return TRUE;
}

VOID mculib_i2c_burstread_8bidx8bval(UINT8 u8_address, UINT8 u8_index, UINT8 u8_length, UINT8 *pu8_value)
{
	struct i2c_msg msgs[2] = {
        {  // 写入寄存器地址
            .addr = i2c_main->addr,
            .flags = 0,
            .len = 1,
            .buf = &u8_index,
        },
        {  // 读取数据
            .addr = i2c_main->addr,
            .flags = I2C_M_RD,  // 读操作
            .len = u8_length,
            .buf = pu8_value,
        }
    };
    int ret = i2c_transfer(i2c_main->adapter, msgs, 2);
    if (ret != 2) {
        printk("i2c burst read failed: reg=0x%02x, ret=%d\n", u8_index, ret);
    }
}

int read_i2c_data(struct i2c_adapter *adap, u8 dev_addr, u16 reg_addr, u8 *buf, size_t len) {
    int ret;
    // u8 reg_addr_buf[2] = { (reg_addr >> 8) & 0xFF, reg_addr & 0xFF }; // 16位地址拆分为高/低字节[6](@ref)
	u8 reg_addr_u8 = (u8)reg_addr;
    struct i2c_msg msg[2] = {
        {
            .addr = dev_addr,
            .flags = 0,
            // .len = sizeof(reg_addr_buf),
            .len = 1,
            .buf = &reg_addr_u8
        },
        {
            .addr = dev_addr,
            .flags = I2C_M_RD,
            .len = len,
            .buf = buf
        }
    };

    ret = i2c_transfer(adap, msg, 2);
    if (ret < 0) {
        printk("I2C transfer error: %d\n", ret);
        return ret;
    } else if (ret != 2) {
        printk("Partial transfer: %d/2 messages\n", ret);
        return -EIO;
    }
    return 0;
}

VOID mculib_ddc_i2c_readbytes(UINT8 u8_address, UINT8 u8_index, UINT8 u8_length, UINT8 *pu8_value)
{
	if (!i2c_main) {
		printk("i2c_edid is NULL\n");
		return;
	}

	if (read_i2c_data(i2c_main->adapter, 0x50, u8_index, pu8_value, u8_length) == 0) {
        printk("Read 256 bytes success!\n");
    }

	printk("[MS7210] DDC I2C Read: offset[%d] len[%d]\n", u8_index, u8_length);
}

//8-bit index for HD EDID block 2-3 read
//for HD CTS test only, not necessary for user
BOOL mculib_i2c_write_blank(UINT8 u8_address, UINT8 u8_index)
{
	printk("mculib_i2c_write_blank: Not Support...\n");
    return TRUE;
}

VOID mculib_i2c_burstread_8bidx8bval_ext(UINT8 u8_address, UINT8 u8_index, UINT8 u8_length)
{
	printk("mculib_i2c_burstread_8bidx8bval_ext: Not Support...\n");
}

//below APIs is for for sdk internal debug, not necessary for user
VOID mculib_uart_log(UINT8 *u8_string)
{

}

VOID mculib_uart_log1(UINT8 *u8_string, UINT16 u16_hex)
{

}

VOID mculib_uart_log2(UINT8 *u8_string, UINT16 u16_dec)
{

}
