/*
* vproc_dbg.h  --  Header file for debugging macros/functions
*
* Copyright 2016 Microsemi Inc.
*/

#ifndef __VPROC_DBG_H__
#define __VPROC_DBG_H__


/**
 * \file vproc_dbg.h
 * \brief Port layer for Host Platform Specific Debug functions
 *
 * Defines the Debug Macro to host-specific supported types.
 * This file should be defined or ported by developer responsible
 * for porting System Service Layer over host platform.
 *
 * System Service Layer Debugging Mechanism should support logging at different
 * layer as listed below.
 *
 * Implementation may support them individual or in combination
 *
 * VPROC_DBG_LVL_NONE - Logging is disabled
 *
 * VPROC_DBG_LVL_FUNC - Print every function Entry and Exit
 *
 * VPROC_DBG_LVL_INFO - Print Informational Messages
 *
 * VPROC_DBG_LVL_WARN - Print Only Warning Messages
 *
 * VPROC_DBG_LVL_ERR - Print Only Error Messages
 *
 * VPROC_DBG_LVL_ALL - Enable all of the above
 *
 * Following are typical Debug APIs to be ported. It is upto developer to
 * implement them as Macro / inline functions/ functions.
 *
 * VPROC_DBG_SET_LVL - Set the log level of SDK. Values is enumeration from
 *                     VPROC_DBG_LVL
 *
 * VPROC_DBG_PRINT - Prints out the debug messages to console.
 *
*/


#include <linux/printk.h>
/* Bitmask for selecting debug level information */
typedef enum
{
    VPROC_DBG_LVL_NONE=0x0,
    VPROC_DBG_LVL_FUNC=0x1,
    VPROC_DBG_LVL_INFO=0x2,
    VPROC_DBG_LVL_WARN=0x4,
    VPROC_DBG_LVL_ERR=0x8,
    VPROC_DBG_LVL_ALL=(VPROC_DBG_LVL_FUNC|VPROC_DBG_LVL_INFO|VPROC_DBG_LVL_WARN|VPROC_DBG_LVL_ERR)
}VPROC_DBG_LVL;

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL VPROC_DBG_LVL_ALL
//#define DEBUG_LEVEL VPROC_DBG_LVL_ERR
#endif

#define DEBUG 1
/* Macros defining SSL_print */
#ifdef DEBUG
extern VPROC_DBG_LVL vproc_dbg_lvl;
#define VPROC_DBG_SET_LVL(dbg_lvl) (vproc_dbg_lvl = dbg_lvl)
#define VPROC_DBG_PRINT(level,msg,args...)  if(level & vproc_dbg_lvl) {printk("[%s:%d]"msg,__FUNCTION__,__LINE__,##args);}
//#define VPROC_DBG_PRINT(level,msg,args...)
#else
#define VPROC_DBG_PRINT(level,msg,args...)
#endif



#endif /*__VPROC_DBG_H__ */
