/*
* chip.h  --  Master header file to include TARGET variant chip
*
* Copyright 2016 Microsemi Inc.
*/


#ifndef __CHIP_H__
#define __CHIP_H__

#if (TARGET == TW)
#include "zl380xx_tw.h"
#include "zl38051.h"
#endif

#endif
