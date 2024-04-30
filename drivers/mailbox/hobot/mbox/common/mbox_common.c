/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file mbox_common.c
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#include "mbox_os.h"
#include "mbox_common.h"

#define IPCMxSOURCE(x, m)	(x + (m) * 0x40)/**< mbox source register*/
#define IPCMxDSET(x, m)		(x + ((m) * 0x40) + 0x004)/**< mbox destination set register*/
#define IPCMxDCLEAR(x, m)	(x + ((m) * 0x40) + 0x008)/**< mbox destination clear register*/
#define IPCMxDSTATUS(x, m)	(x + ((m) * 0x40) + 0x00C)/**< mbox destination status register*/
#define IPCMxMODE(x, m)		(x + ((m) * 0x40) + 0x010)/**< mbox mode register*/
#define IPCMxSEND(x, m)		(x + ((m) * 0x40) + 0x014)/**< mbox send register*/
#define IPCMxMSET(x, m)		(x + ((m) * 0x40) + 0x018)/**< mbox mask set register*/
#define IPCMxMCLEAR(x, m)	(x + ((m) * 0x40) + 0x01C)/**< mbox mask clear register*/
#define IPCMxMSTATUS(x, m)	(x + ((m) * 0x40) + 0x020)/**< mbox mask status register*/

#define IPCMxDR(x, m, dr)	(x + ((m) * 0x40) + ((dr) * 4) + 0x024)/**< mbox data register*/
// dr: 0 for tx, 1 for rx, which is different from pl320

// #define IPCMxFIFOWR(x, m)	(x + ((m) * 0x40) + 0x024)/**< mbox mask clear register*/
// #define IPCMxFIFORD(x, m)	(x + ((m) * 0x40) + 0x028)/**< mbox mask status register*/
#define IPCMxFIFOSTAT(x, m)	(x + ((m) * 0x40) + 0x02C)/**< mbox mask status register*/

#define IPCMSRCSECMIS(x, irq)		(x + ((irq) * 0x20) + 0x800)/**< mbox mask interrupt status registr*/
#define IPCMSRCNSECMIS(x, irq)		(x + ((irq) * 0x20) + 0x804)/**< mbox raw interrupt status registr*/
#define IPCMDSTSECMIS(x, irq)		(x + ((irq) * 0x20) + 0x808)/**< mbox mask interrupt status registr*/
#define IPCMDSTNSECMIS(x, irq)		(x + ((irq) * 0x20) + 0x80C)/**< mbox raw interrupt status registr*/

#define IPCMSRCSECRIS(x, irq)		(x + ((irq) * 0x20) + 0x810)/**< mbox mask interrupt status registr*/
#define IPCMSRCNSECRIS(x, irq)		(x + ((irq) * 0x20) + 0x814)/**< mbox raw interrupt status registr*/
#define IPCMDSTSECRIS(x, irq)		(x + ((irq) * 0x20) + 0x818)/**< mbox mask interrupt status registr*/
#define IPCMDSTNSECRIS(x, irq)		(x + ((irq) * 0x20) + 0x81C)/**< mbox raw interrupt status registr*/


// #define IPCMxDR(x, m, dr)	(x + ((m) * 0x40) + ((dr) * 4) + 0x024)/**< mbox data register*/
// #define IPCMMIS(x, irq)		(x + ((irq) * 8) + 0x800)/**< mbox mask interrupt status registr*/
// #define IPCMRIS(x, irq)		(x + ((irq) * 8) + 0x814)/**< mbox raw interrupt status registr*/


enum ipc_interruot_id {
	IPC_SRC_SEC_ID = 0,
	IPC_SRC_NSEC_ID,
	IPC_DST_SEC_ID,
	IPC_DST_NSEC_ID,
};

uint32_t com_get_src(uint8_t *base, uint8_t mbox_id)
{
	uint8_t *addr = IPCMxSOURCE(base, mbox_id);

	return os_read_register(addr);
}

int32_t com_set_src(uint8_t *base, uint8_t mbox_id, uint8_t src_id)
{
	uint8_t *addr = IPCMxSOURCE(base, mbox_id);

	if (os_read_register(addr) != 0 && os_read_register(addr) != BIT(src_id)) {
		mbox_err("src access denied\n");

		return -EACCES;
	}

	(void)os_write_register(addr, BIT(src_id));

	return 0;
}

int32_t com_clear_src(uint8_t *base, uint8_t mbox_id, uint8_t src_id)
{
	uint8_t *addr = IPCMxSOURCE(base, mbox_id);

	if (os_read_register(addr) != BIT(src_id)) {
		mbox_err("access denied\n");

		return -EACCES;
	}

	(void)os_write_register(addr, 0);

	return 0;
}

uint32_t com_get_dst(uint8_t *base, uint8_t mbox_id)
{
	uint8_t *addr = IPCMxDSTATUS(base, mbox_id);

	return os_read_register(addr);
}

void com_set_dst(uint8_t *base, uint8_t mbox_id, uint8_t dst_id)
{
	uint8_t *addr = IPCMxDSET(base, mbox_id);
	uint32_t val = (com_get_dst(base, mbox_id)) | (BIT(dst_id));

	(void)os_write_register(addr, val);
}

#if 0//reserved
void com_clear_dst(uint8_t *base, uint8_t mbox_id, uint8_t dst_id)
{
	uint8_t *addr = IPCMxDCLEAR(base, mbox_id);
	uint32_t val = (com_get_dst(base, mbox_id)) & (BIT(dst_id));
	(void)os_write_register(addr, val);
}
#endif

uint32_t com_get_irq_cfgstatus(uint8_t *base, uint8_t mbox_id)
{
	uint8_t *addr = IPCMxMSTATUS(base, mbox_id);

	return os_read_register(addr);
}

void com_enable_irq(uint8_t *base, uint8_t mbox_id, uint8_t int_id)
{
	uint8_t *addr = IPCMxMSET(base, mbox_id);
	uint32_t val = (com_get_irq_cfgstatus(base, mbox_id)) | (BIT(int_id));

	(void)os_write_register(addr, val);
}

void com_disable_irq(uint8_t *base, uint8_t mbox_id, uint8_t int_id)
{
	uint8_t *addr = IPCMxMCLEAR(base, mbox_id);
	uint32_t val = (com_get_irq_cfgstatus(base, mbox_id)) & (BIT(int_id));

	(void)os_write_register(addr, val);
}

#if 0//reserved
uint32_t com_get_mode(uint8_t *base, uint8_t mbox_id)
{
	uint8_t *addr = IPCMxMODE(base, mbox_id);

	return os_read_register(addr);
}
#endif

void com_set_mode(uint8_t *base, uint8_t mbox_id, uint32_t mode)
{
	uint8_t *addr = IPCMxMODE(base, mbox_id);
	uint32_t val = os_read_register(addr) | mode;

	(void)os_write_register(addr, val);
}

uint32_t com_get_sendreg(uint8_t *base, uint8_t mbox_id)
{
	uint8_t *addr = IPCMxSEND(base, mbox_id);

	return os_read_register(addr);
}

void com_clear_sendreg(uint8_t *base, uint8_t mbox_id)
{
	uint8_t *addr = IPCMxSEND(base, mbox_id);

	(void)os_write_register(addr, (uint32_t)0);
}

void com_tx_notify(uint8_t *base, uint8_t mbox_id)
{
	uint8_t *addr = IPCMxSEND(base, mbox_id);

	(void)os_write_register(addr, (uint32_t)SEND_NOTIFY);
}

void com_ack_notify(uint8_t *base, uint8_t mbox_id)
{
	uint8_t *addr = IPCMxSEND(base, mbox_id);

	(void)os_write_register(addr, (uint32_t)ACK_NOTIFY);
}

uint32_t com_get_ipcmmis(uint8_t *base, uint8_t int_id, uint8_t dr)
{
	uint8_t *addr = NULL;
	if (dr == IPC_SRC_SEC_ID)
		addr = IPCMSRCSECMIS(base, int_id);
	else if (dr == IPC_SRC_NSEC_ID)
		addr = IPCMSRCNSECMIS(base, int_id);
	else if (dr == IPC_DST_SEC_ID)
		addr = IPCMDSTSECMIS(base, int_id);
	else
		addr = IPCMDSTNSECMIS(base, int_id);

//	os_write_register(addr, 0x400);//qemu
	return os_read_register(addr);
}

uint32_t com_get_ipcmris(uint8_t *base, uint8_t int_id, uint8_t dr)
{
	uint8_t *addr = NULL;
	if (dr == IPC_SRC_SEC_ID)
		addr = IPCMSRCSECRIS(base, int_id);
	else if (dr == IPC_SRC_NSEC_ID)
		addr = IPCMSRCNSECRIS(base, int_id);
	else if (dr == IPC_DST_SEC_ID)
		addr = IPCMDSTSECRIS(base, int_id);
	else
		addr = IPCMDSTNSECRIS(base, int_id);

//	os_write_register(addr, 0x400);//qemu
	return os_read_register(addr);
}

uint32_t com_get_mboxdata(uint8_t *base, uint8_t mbox_id, uint8_t dr)
{
	uint8_t *addr = IPCMxDR(base, mbox_id, 1);

	return os_read_register(addr);
}

uint8_t *com_get_mboxdata_addr(uint8_t *base, uint8_t mbox_id, uint8_t dr)
{
	return IPCMxDR(base, mbox_id, dr);
}

uint32_t com_get_mboxdata_level(uint8_t *base, uint8_t mbox_id)
{
	return os_read_register(IPCMxFIFOSTAT(base, mbox_id)) & 0x3F;

}
