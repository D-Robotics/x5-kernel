/*
* ssl.h  --  Header file for System Service Layer
*
* Copyright 2016 Microsemi Inc.
*/

#ifndef __SSL_H__
#define __SSL_H__

/**
 * \file include/ssl.h
 * \brief System Service Layer.
 *
 * System Service Layer is combination of 3 header files :
 *
 * ssl.h - Master file that prototypes functions and data structures to be ported
 *
 * typedefs.h - Base file defining data types as used by layers above
 *
 * vproc_dbg.h - Debug header file ported for debug messages. If no debugging
 *               supported, implementation to leave function/macro as stub but
 *               still should define
 *
 * This is Platform-Specific layer and must be ported to host platform-specific
 * function for correct functioning of HBI Driver over host platform
 *
 * Expected and Intended use of data structures and functions are explained
 * here. In case of confusion, developer may reach at <>
*/

#include "typedefs.h"
#include "linux/i2c.h"
/**
 *  \ingroup ssl
 *
 *  \{
 */

/*! \enum ssl_status_t
 *  \brief Enumerates Status Codes supported by SSL
 *
 */
typedef enum
{
    SSL_STATUS_NOT_INIT = -1, /*!< SSL Driver not initialised. Should be returned
                               * by any other call to SSL API if called before
                               * SSL_init()
                             */
    SSL_STATUS_INVALID_ARG = -2, /*!< Invalid argument passed to SSL API.
                                  *  Should be returned by every AAL API, if
                                  *  given argument doesn't fall in supported range
                                 */
    SSL_STATUS_INTERNAL_ERR = -3,  /*!< Indicates to caller than an error is
                                       reported from platform specific layer*/
    SSL_STATUS_BAD_HANDLE = -4,    /*!< Invalid port handle passed.Should be returned
                                    *   by an API if called before successful
                                    *   SSL_port_open()
                                    */
    SSL_STATUS_RESOURCE_ERR = -5,  /*!< Requested resource unavailable.
                                    * Should be returned by SSL API if requested
                                    * resource not available.
                                    * Example memory, bus, device
                                    */
    SSL_STATUS_TIMEOUT = -6,       /*!< Return if API times out waiting on a
                                    * certain operation.
                                    */
    SSL_STATUS_FAILED = -7,        /*!< SSL API failed.
                                    *   A more generic message to indicate a
                                    *   call failure for any reason than listed
                                    * above
                                    */
    SSL_STATUS_OP_INCOMPLETE = -8, /*!< port read/write incomplete. May be returned
                                    *  when number of bytes read/written is less than
                                    *  actual requested. This is optional and
                                    *  depends upon developer choice of SSL_read()
                                    *  and SSL_write() implementation
                                    */
    SSL_STATUS_OK=0            /*!< SSL API call successful. */
}ssl_status_t;


typedef struct
{
    uint16_t chip; /*!< CHIP name EX: 38051*/
    uint8_t  dev_addr; /*!< device address. can be i2c address or spi chip
                          select id*/
    uint8_t  bus_num;  /*!< bus number device physically present on */
    uint8_t  isboot;   /*TRUE to boot load images into the device at power up, FALSE not to boot load the device*/
    uint8_t  *pFirmware; /*a pointer to either the filename if in *.bin format or data array  if in c code format*/
    uint8_t  *pConfig;   /*a pointer to either the filename if in *.cr2 format or data array  if in c code format*/
    ssl_lock_handle_t dev_lock; /*!< lock to serialise device access */
    uint8_t  imageType;   /*0: for static *.h, *.c code, 1: for *.bin */
}ssl_dev_info_t;

/*! \enum ssl_wait_t
 *  \brief Enumerates wait type on lock.
 *
 * Depends on System Service Layer implementation and support of locking
 * mechanism.
 *
 * if Port layer does not support locking, these can be don't care or stub.
 *
 */
typedef enum
{
    SSL_WAIT_NONE,  /*!< Return immediately, if failed to get lock */
    SSL_WAIT_FOREVER /*!< Wait until lock is attained.Please note this
                      * may block the call
                      */
}ssl_wait_t;

/*! \enum ssl_op_t
 *  \brief Enumerates port functions. Can be 'Read Only' ,
 *         'Write Only' or 'Read/Write' both
 *
 */
typedef enum
{
    SSL_OP_PORT_RD=0x01, /*!< Read only operation on Port */
    SSL_OP_PORT_WR=0x02, /*!< Write only operation on Port */
    SSL_OP_PORT_RW = (SSL_OP_PORT_RD | SSL_OP_PORT_WR) /*! both Read and write
                                                        * operation on port
                                                        */
}ssl_op_t;

/*! \brief user passed in structure during read/write done by making a call to
 *         SSL_port_read(), SSL_port_write() and SSL_port_rw()
 *
 */
typedef struct
{
    void    *pSrc;    /*!< Pointer to caller source buffer.
                       *  Must be set to a valid value for PORT WRITE Operation
                      */
    void    *pDst;    /*!< Pointer to caller destination buffer,
                       *  Must be set to a valid value for PORT READ Operation
                      */
    size_t   nread;   /*!< Number of bytes to read. Ignore in case of PORT
                       *  Write operation
                      */
    size_t   nwrite;  /*!< Number of bytes to write. Ignore in case of PORT Write
                       *  operation
                      */
    ssl_op_t op_type; /*!< Enum indicating port operation: 'read','write' or
                       * 'read/write'
                      */
}ssl_port_access_t;

extern ssl_dev_info_t sdk_devices_info[VPROC_MAX_NUM_DEVS];

/*! \fn  ssl_status_t sdk_register_board_devices_info(ssl_dev_info_t *pDevicesInfo)
 *
 * \brief device Driver initialization function.
 *
 * initialize of device resources as required by driver is performed by this funtion
 *
 * @param [in] pDevicesInfo    Pointer to driver init ressource configuration.
*/
ssl_status_t sdk_register_board_devices_info(ssl_dev_info_t *pDevicesInfo);
int ssl_get_device_id(ssl_dev_cfg_t *pDevCfg);
/*! \fn  ssl_status_t SSL_init(ssl_drv_cfg_t *pCfg)
 *
 * \brief Driver initialization function.
 *
 * Developer should Allocate and initialize resources as required by driver in
 * this function call. pCfg is optional argument and host system specific.
 * if required, developer may add initialization configuration paramter.
 * Definition of data type ssl_drv_cfg_t is developer-specific and should be
 * defined in typedefs.h in sdk. if used, it should be one-time configuration
 * parameter and should not be modified.
 *
 * @param [in] pCfg    Pointer to driver init configuration.
                       This is optional parameter.
*/
ssl_status_t SSL_init(ssl_drv_cfg_t *pCfg, struct i2c_client *pClient, const struct i2c_device_id *pDeviceId);

/*! \fn  ssl_status_t SSL_lock_create(ssl_lock_handle_t *pLock,
 *                                    const char *pName, void *pOption)
 *
 * \brief Lock Create Function
 *
 * This function implements  Locking and Unlocking mechanism.
 * pName and pOption are optional parameter and are implementation specific.
 * ssl_lock_handle_t is host specific data type and should be declared by
 * developer in typedefs.h
 *
 *
 * @param [in] pLock    Pointer to Lock structure. Should not be NULL.
 * @param [in] pName    Optional Null terminated string to identify lock
 * @param [in] pOption   Optional pointer to options structure as declared by
                        host system in typedef.h.
 * @param [out] pLock   Updated by call on successful completion
 *
*/
ssl_status_t SSL_lock_create(ssl_lock_handle_t *pLock,
                           const char *pName, void *pOption);

/*! \fn  ssl_status_t SSL_lock(ssl_lock_handle_t lock_id,ssl_wait_t wait_type)
 *
 * \brief Hold a lock
 *
 * This function is called to hold a Locking. This can act as stub function
 * if implementation does not support a locking mechanism.
 *
 * @param [in] lock_id  handle as returned by call to SSL_lock_create()
 * @param [in] wait_type enum indicating WAIT_FOREVER or WAIT_NONE
 *
*/
ssl_status_t SSL_lock(ssl_lock_handle_t lock_id,ssl_wait_t wait_type);

/*! \fn  ssl_status_t SSL_unlock(ssl_lock_handle_t lock_id)
 *
 * \brief Release a lock
 *
 * This function is called to release a Lock. This can act as stub function
 * if implementation doesnt support a locking mechanism.
 *
 * @param [in] lock_id  handle as returned by call to SSL_lock_create()
 *
*/
ssl_status_t SSL_unlock(ssl_lock_handle_t lock_id);

/*! \fn  ssl_status_t SSL_lock_delete(ssl_lock_handle_t lock_id)
 *
 * \brief Deletes lock
 *
 * This function is called to delete a Lock created by call to SSL_lock_create.
 *
 * @param [in] lock_id  handle as returned by call to SSL_lock_create()
 *
*/
ssl_status_t SSL_lock_delete(ssl_lock_handle_t lock_id);

/*! \fn  ssl_status_t SSL_term(void)
 *
 * \brief Terminates SSL Driver.
 *
 * This function should deallocate and free all resources as acquired
 * during SSL_init() call.
 *
*/
ssl_status_t SSL_term(void);


/*! \fn  ssl_status_t SSL_port_open(ssl_port_handle_t *pHandle,
                                    ssl_dev_cfg_t *pDevCfg)
 *
 * \brief Opens an port to device.
 *        This function should initialise, allocate
 *        and setup all necessary infrastructure required to transmit data to and
 *        from device. Initialisation of SPI / I2C interface happen here.
 *        User should be able to do read/write to device after successful
 *        execution of this API
 *
 * @param [in] *pHandle Pointer to port handle. ssl_port_handle_t data type should
 *                       be declared by developer in typedefs.h
 * @param [in] *pDevCfg Pointer to device configuration. User passed in bus number,
 *              chip address or id. Example for SPI, bus number can be spi bus
 *              number and device address can be chip select value. For i2c,
 *              device address can be allowable range of i2c addresses.
 * @param [out] pHandle Should be updated by driver on successful execution of
 *              call
 *
*/
ssl_status_t SSL_port_open(ssl_port_handle_t *pHandle, ssl_dev_cfg_t *pDevCfg);


/*! \fn  ssl_status_t SSL_port_close(ssl_port_handle_t handle)
 *
 * \brief Close the port opened to device
 *
 * This function should deallocate and free all resources as acquired
 * during SSL_port_open() call. After successful execution of this function, user
 * should not be able to send/receive data to device.
 *
 * @param handle Port Handle as returned by SSL_port_open()
*/
ssl_status_t SSL_port_close(ssl_port_handle_t handle);


/*! \fn ssl_status_t SSL_port_read(ssl_port_handle_t handle,
 *                                 void *pDst, size_t *pNread)
 *
 * \brief Reads the data from port into user buffer pointed by pDst
 *
 * This function read data from device into user buffer pointed by pDst
 *
 * @param [in] handle Port Handle as returned by SSL_port_open()
 * @param [in] pDst Pointer to user passed in destination buffer to read data
 *             in to
 * @param [in] pNread Pointer to variable containing size of data to read
 * @param [out] pNread Optional.May be updated by driver to indicate size of
 *              data actually read

*
 */
ssl_status_t SSL_port_read(ssl_port_handle_t handle, void *pDst, size_t *pNread);


/*! \fn ssl_status_t SSL_port_write(ssl_port_handle_t handle,
                                    void *pSrc, size_t *pNwrite)
 *
 * \brief Write the data from user buffer to device
 *
 * This function writes user passed data in pSrc buffer to device.
 *
 * @param [in] handle Port Handle as returned by SSL_port_open()
 * @param [in] pSrc Pointer to source buffer containing data to be sent to device
 * @param [in] pNwrite Pointer to buffer containing size of data to be written
 * @param [out] pNwrite Optional. May be updated by driver to indicate size of
                data actually written
 *
 */
ssl_status_t SSL_port_write(ssl_port_handle_t handle,void *pSrc, size_t *pNwrite);

/*! \fn ssl_status_t SSL_port_rw(ssl_port_handle_t handle,
 *                               ssl_port_access_t *pPort)
 *
 * \brief Read and Writes the data from and to device
 *
 * This function performs both read/write transaction to device in a single call.
 * May be supported by driver for combined transactions.
 *
 * @param [in] handle Port Handle as returned by SSL_port_open()
 * @param [in] pPort Pointer to structure populated with valid arguments
 *                   for read/write transactions
 * @param [out] pPort May update size of data actually read/written in pPort
 *                    structure
 *
 */
ssl_status_t SSL_port_rw(ssl_port_handle_t handle, ssl_port_access_t *pPort);

/*! \fn ssl_status_t SSL_memset(void *pDst, int32_t val,size_t size)
 *
 * \brief Sets the content of memory to specified value
 *
 * Sets the content of memory to specified value. May be ported over system
 * specific memset call.
 *
 * @param [in] pDst Pointer to memory location to be updated
 * @param [in] val Value to be written to
 * @param [in] size size of the memory to be updated
 *
 */
ssl_status_t SSL_memset(void *pDst, int32_t val,size_t size);

/*! \fn ssl_status_t SSL_memcpy(void *pDst,const void *pSrc, size_t size)
 *
 * \brief Copy the content of memory from source to destination pointer
 *
 * Copies the content of source buffer to destination buffer. May be ported over
 * system specific memcpy call.
 *
 * @param [in] pDst Pointer to memory location to be copied to
 * @param [in] pSrc Pointer to memory location to copy from
 * @param [in] size size of the data to be copied
 *
 */
ssl_status_t SSL_memcpy(void *pDst,const void *pSrc, size_t size);

/*! \fn ssl_status_t SSL_delay(uint32_t tmsec)
 *
 * \brief Implements delay in millisecond
 *
 * Implements delay in milliseconds . May be ported over
 * system specific delay/sleep call.
 *
 * @param [in] tmsec - time in milliseconds
 *
 */
ssl_status_t SSL_delay(uint32_t tmsec);

/** \} */

#endif
