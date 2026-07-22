/*
* ssl_port.c - System Service Layer implementation for Raspberry Pi
*
*
* Copyright 2016 Microsemi Inc.
*
* This program is free software you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option)any later version.
*/

#include <linux/mutex.h>
#include <linux/delay.h>
#include "typedefs.h"
#include "ssl.h"
#include "vproc_dbg.h"

#define SSL_LOCK_NAME_SIZE 32

/* Variable for current debug level set in the system.*/
//VPROC_DBG_LVL vproc_dbg_lvl = DEBUG_LEVEL;

/* structure defining lock created by ssl */
struct ssl_lock
{
    uint8_t     name[SSL_LOCK_NAME_SIZE]; /* name of lock */
    struct mutex lock; /* lock of the type mutex */
    bool   inuse;       /* flag indicating current entry is in use */
};


/* structure defining driver */
struct ssl_drv{
    struct ssl_lock     lock[NUM_MAX_LOCKS]; /* locks managed by driver */
};

/* variable keeping driver level information */
static struct ssl_drv ssl_drv_priv;




#define CHK_LOCK_HANDLE_VALIDITY(handle) \
   for(i=0;i<NUM_MAX_LOCKS;i++) { \
      if(handle == (ssl_lock_handle_t)&(ssl_drv_priv.lock[i])) break; \
   } \
   if(i>=NUM_MAX_LOCKS) return SSL_STATUS_BAD_HANDLE

ssl_status_t SSL_memset(void *pDst, int32_t val,size_t size)
{
    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter...\n");

    if(pDst == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL destination pointer\n");
        return SSL_STATUS_INVALID_ARG;
    }

    memset(pDst,val,size);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");

    return SSL_STATUS_OK;
}

ssl_status_t SSL_memcpy(void *pDst,const void *pSrc, size_t size)
{
    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter...\n");

    if(pSrc == NULL || pDst == NULL)
    {
        VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"NULL src or dst pointer\n");
        return SSL_STATUS_INVALID_ARG;
    }

    memcpy(pDst,pSrc,size);

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");

    return SSL_STATUS_OK;
}

ssl_status_t SSL_delay(uint32_t tms)
{
    mdelay(tms);
    return SSL_STATUS_OK;
}


ssl_status_t SSL_lock_create(ssl_lock_handle_t *pLock,
                              const char *pName, void *pOption)
{
   int32_t i;

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Enter...\n");

   if(pLock == NULL)
   {
     VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,"Invalid Parameter list\n");
     return SSL_STATUS_INVALID_ARG;
   }

   for(i=0;i<NUM_MAX_LOCKS;i++)
   {
     if(!(ssl_drv_priv.lock[i].inuse))
         break;
   }

   if(i>=NUM_MAX_LOCKS)
   {
      VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                     "Unable to create a lock.resources exhausted\n");
      return SSL_STATUS_RESOURCE_ERR;
   }

   mutex_init(&(ssl_drv_priv.lock[i].lock));

   if(pName)
      strncpy(ssl_drv_priv.lock[i].name,pName,sizeof(ssl_drv_priv.lock[i].name));

   ssl_drv_priv.lock[i].inuse = TRUE;

   *pLock = (ssl_lock_handle_t )&(ssl_drv_priv.lock[i]);

   VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");
   return SSL_STATUS_OK;
}

/* wait type parameter if doesnt match to any value as in enum
   SSL_WAIT_TYPE then will be assumed as timeout in millisecond*/
ssl_status_t SSL_lock(ssl_lock_handle_t lock_id,ssl_wait_t wait_type)
{
   int32_t i;

   CHK_LOCK_HANDLE_VALIDITY(lock_id);

   switch(wait_type)
   {
      case SSL_WAIT_NONE:
         mutex_trylock(&(((struct ssl_lock*)lock_id)->lock));
      break;
      case SSL_WAIT_FOREVER:
         mutex_lock(&(((struct ssl_lock*)lock_id)->lock));
      break;
      default:
         VPROC_DBG_PRINT(VPROC_DBG_LVL_ERR,
                       "timed wait not supported by this SSL implementation\n");
         return SSL_STATUS_INVALID_ARG;
   }

   return SSL_STATUS_OK;
}

ssl_status_t SSL_unlock(ssl_lock_handle_t lock_id)
{
    int32_t i;

    CHK_LOCK_HANDLE_VALIDITY(lock_id);
    mutex_unlock(&(((struct ssl_lock*)lock_id)->lock));

    return SSL_STATUS_OK;
}

ssl_status_t SSL_lock_delete(ssl_lock_handle_t lock_id)
{
    struct ssl_lock *pLock;
    int32_t i;

    CHK_LOCK_HANDLE_VALIDITY(lock_id);

    pLock = (struct ssl_lock *)lock_id;
    mutex_destroy(&(pLock->lock));
    pLock->inuse = FALSE;

    VPROC_DBG_PRINT(VPROC_DBG_LVL_FUNC,"Exit...\n");

    return SSL_STATUS_OK;
}
