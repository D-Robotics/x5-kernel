/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#include "rtl8723d_hal.h"

/*
 * ================================================================================
 * Interface to manipulate LED objects.
 * ================================================================================
 */

/*
 * ================================================================================
 * Default LED behavior.
 * ================================================================================
 */

/*
 * Description:
 * Initialize all LED_871x objects.
 */
void
rtl8723du_InitLeds(
	PADAPTER padapter
)
{
#if defined(CONFIG_RTW_HW_LED)
	u32 ledcfg;
	//LED2 HW config
	ledcfg = rtw_read32(padapter , REG_LEDCFG0);
	ledcfg |= BIT21;
	ledcfg &= ~(BIT22 | BIT30);
	rtw_write32(padapter , REG_LEDCFG0 , ledcfg);
	
	printk("LED2 HW config = %x\n", ledcfg);
	//Enable External WOL function
	ledcfg = rtw_read32(padapter , REG_GPIO_INTM);
	ledcfg &= ~(BIT16);
	rtw_write32(padapter , REG_GPIO_INTM , ledcfg);
	printk("Enable External WOL function = %x\n", ledcfg);
	
#elif defined(CONFIG_RTW_SW_LED)
	struct led_priv *pledpriv = adapter_to_led(padapter);

	pledpriv->LedControlHandler = LedControlUSB;

	pledpriv->SwLedOn = SwLedOn_8723DU;
	pledpriv->SwLedOff = SwLedOff_8723DU;

	InitLed(padapter, &(pledpriv->SwLed0), LED_PIN_LED0);

	InitLed(padapter, &(pledpriv->SwLed1), LED_PIN_LED1);
#endif

}

/*
 * Description:
 * DeInitialize all LED_819xUsb objects.
 */
void
rtl8723du_DeInitLeds(
	PADAPTER padapter
)
{
#if defined(CONFIG_RTW_HW_LED)
	u32 ledcfg;
	//LED2 HW config
	ledcfg = rtw_read32(padapter , REG_LEDCFG0);
	ledcfg &= ~(BIT21);
	ledcfg |= BIT22 | BIT30;
	rtw_write32(padapter , REG_LEDCFG0 , ledcfg);
#elif defined(CONFIG_RTW_SW_LED)
	struct led_priv *ledpriv = adapter_to_led(padapter);

	DeInitLed(&(ledpriv->SwLed0));
	DeInitLed(&(ledpriv->SwLed1));
#endif
}

#ifdef CONFIG_RTW_SW_LED

/*
 * ================================================================================
 * LED object.
 * ================================================================================
 */


/*
 * ================================================================================
 * Prototype of protected function.
 * ================================================================================
 */

/*
 * ================================================================================
 * LED_819xUsb routines.
 * ================================================================================
 */

/*
 * Description:
 * Turn on LED according to LedPin specified.
 */
void
SwLedOn_8723DU(
	PADAPTER padapter,
	PLED_USB pLed
)
{
	u8 LedCfg;

	if (RTW_CANNOT_RUN(padapter))
		return;

	pLed->bLedOn = _TRUE;

}


/*
 * Description:
 * Turn off LED according to LedPin specified.
 */
void
SwLedOff_8723DU(
	PADAPTER padapter,
	PLED_USB pLed
)
{
	u8 LedCfg;

	if (RTW_CANNOT_RUN(padapter))
		goto exit;

exit:
	pLed->bLedOn = _FALSE;

}
#endif
