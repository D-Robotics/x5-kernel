/*
* hbi.h  --  Header file for Microsemi VPROC Device Host Bus Interface
*
* Copyright 2016 Microsemi Inc.
*/


/*! \file hbi.h */

#ifndef __HBI_H__
#define __HBI_H__

/********************************************/
/*! \mainpage
*
* \section intro_sec Introduction
*
* This is an API specification document for Microsemi Voice Processor Class of
* Device SDK. This document covers platform-independent Host
* Bus Interface(HBI) And platform-dependent System Service Layer(SSL).
* HBI API are implemented over SSL.
* Below Diagram gives an overview of different components in system
*
* \image html vproc_sdk_flow.png  "Microsemi VPROC SDK FLOW"
* \image latex vproc_sdk_flow.eps "Microsemi VPROC SDK FLOW" width=\textwidth,height=\textheight/2
*
* Microsemi only define System Service Layer as an abstract API set . It should
* be ported by developer responsible for porting HBI Driver for a particular host platform.
* HBI Driver and Microsemi VPROC SDK is assisted by various compile-time options
* which gives flexibility to host to configure Microsemi Voice Processor Device
* and software as per their system requirement ex. SPI/I2C, endianness, memory resources.
* Below tables summarizes both HBI and System-wide various compile time options
* \image html hbi_options.png "Microsemi HBI Compile Time Options"
* \image latex hbi_options.eps "Microsemi HBI Compile Time Options" width=\textwidth,height=\textheight/4
* \image html vproc_sdk_options.png "Microsemi VPROC SDK Compile Time Options"
* \image latex vproc_sdk_options.eps "Microsemi VPROC SDK Compile Time Options" width=\textwidth,height=\textheight/2
*
* \defgroup hbi Host Bus Interface
* Host Bus Interface define control path communication to Microsemi VPROC Devices.
*
* Microsemi VPROC device control path communcation is divided in to transport
* and physical layer where Transport Layer sits on top of Physical Layer.
* Physical layer define actual bus protocol to use at ground level.Current
* Microsemi VPROC Devices support SPI and I2C as physical interface.
* Transport layer define a device specific protocol and termed as
* Host Bus Interface (HBI).
* Following section covers API of HBI Driver to configure VPROC Devices.
*
* \defgroup ssl System Service Layer
 * System Service Layer is Platform specific Abstraction Layer.
 *
 * This layer prototypes API and data structures. It should be implemented by
 * developer responsible for porting HBI driver over host platform.
 *
********************************************************************************/

#include "ssl.h"

#define INIT_TERM_AUTO
/*! \ingroup hbi
 * \{
 *
*/

/*! \enum hbi_cmd_t
 *  \brief Enumerates various HOST commands that can be issued to
 *          device.
 *
 *  HBI commands accepts either of these types of arguments:
 *  mandatory: means this command need an input to process
 *  optional : inputs are desirable but not mandatory.
 *  none : this command accepts no arguments.
 *  Details of argument w.r.t command is detailed as below
 *
 */
typedef enum
{

    HBI_CMD_LOAD_FWR_FROM_HOST,    /*!< Loads firmware image to device.
                                        [in] input args: pointer to hbi_data_buf_t
                                        (populated with pointer to image buffer
                                         and length of buffer)
                                        [out] output args: none
                                    */

    HBI_CMD_LOAD_CFGREC_FROM_HOST, /*!< Loads configuration record from host to device.
                                     [in] input args: pointer to hbi_data_buf_t
                                     (populated with pointer to config record buffer and length of
                                      buffer)
                                     [out] output args: none
                                  */

    HBI_CMD_LOAD_FWR_COMPLETE,  /*!< Signal device that host data transfer is complete.
                                     Should be called after HBI_CMD_LOAD_FWR_FROM_HOST or HBI_CMD_LOAD_CFGREC_FROM_HOST
                                     input args : none
                                     output args : none
                                */

    HBI_CMD_LOAD_FWRCFG_FROM_FLASH, /*!< Loads requested firmware and associated configuration record from flash
                                         connected to voice processor device. Need image number from user.
                                        input args: an unsigend int image_num
                                        output args: none
                                    */

    /*!< Saves current firmware and configuration record in device memory to flash.
       connected to voice processor device.Should be called after
       HBI_CMD_LOAD_FWR_FROM_HOST and HBI_CMD_HOST_LOAD_COMPLETE.
       input args : none
       output args: pointer to an unsigned int to pass back uploaded image number
                    (optional)
    */
    HBI_CMD_SAVE_FWRCFG_TO_FLASH,

    /*! Erases whole flash connected to voice processor device
       input args: none
       output args: none
    */
    HBI_CMD_ERASE_WHOLE_FLASH,

    /*! Erases specific firmware and associated configuration record from flash
       connected to voice processor device. Need an image number from user
       Input args: an unsigned int firmware image number
       output args: none
    */
    HBI_CMD_ERASE_FWRCFG_FROM_FLASH,

    /*! Starts executing current firmware in RAM after it is being loaded either
       from flash or host. Please note this should be called ONLY and IMMEDIATLY
       after HBI_CMD_LOAD_FROM_HOST and HBI_CMD_HOST_LOAD_COMPLETE OR
       HBI_CMD_LOAD_FWRCFG_FROM_FLASH.
       input args: none
       output args: none
    */
    HBI_CMD_START_FWR,

    /*!< Saves current configuration record in device memory to flash.
       connected to voice processor device. There must be a firmware image already stored
       on the flash. OTherwise this is not supported
       input args : none
       output args: pointer to an unsigned int to pass back uploaded image number
                    (optional)
    */

    HBI_CMD_SAVE_CFG_TO_FLASH,

    /*! End marker for Host Command List */
    HBI_CMD_END
}hbi_cmd_t;

/*! \brief enumerates various status codes of HBI Driver
 *
 */

typedef enum
{
    HBI_STATUS_NOT_INIT,     /*!<  driver not initilised */
    HBI_STATUS_INTERNAL_ERR, /*!< platform specific layer reported an error */
    HBI_STATUS_RESOURCE_ERR, /*!< request resource unavailable */
    HBI_STATUS_INVALID_ARG,  /*!< invalid argument passed to a function call */
    HBI_STATUS_BAD_HANDLE,   /*!< a bad reference handle passed */
    HBI_STATUS_BAD_IMAGE,    /*!< requested firmware image not present on flash */
    HBI_STATUS_FLASH_FULL,   /*!< no more space left on flash */
    HBI_STATUS_NO_FLASH_PRESENT, /*!< no flash connected to device */
    HBI_STATUS_COMMAND_ERR,    /*!< HBI Command failed */
    HBI_STATUS_INCOMPAT_APP,   /*!< firmware image is incompatible */
    HBI_STATUS_INVALID_STATE,  /*!< driver is in invalid state for current action */
    HBI_STATUS_OP_INCOMPLETE, /*!< operation incomplete */
    HBI_STATUS_SUCCESS         /*!< driver call successful */
}hbi_status_t;

/*! \brief enumerates types of data as passed by application
 *
 */
typedef enum
{
   HBI_IMG_TYPE_FWR=0, /*!< firmware image to load on Device */
   HBI_IMG_TYPE_CR=1,  /*!< Configuration Record to load Device */
   HBI_IMG_TYPE_LAST  /*!< Limiter on input type */
}hbi_img_type_t;

/*! \brief enumerates reset mode of device
 *
 */
typedef enum
{
    HBI_RST_POR, /*!< This simulates Power On Reset where it will clear everything
                    including DSP memory and restart device with invocation of
                    internal boot ROM code
                    followed by loading of firmware from flash, if present.
                    If flash not present in system then device will stop at
                    Boot ROM prompt waiting on host
                 */
}hbi_rst_mode_t;

/*! \brief user passed in device configuration parameters at the time of open.
 *
 */
typedef struct
{
    tw_device_id_t deviceId;  /*a number from 0 to MAX_NUM_DEVS-1 identifying the device*/
    uint8_t *pDevName; /*!< an optional pointer to device name - Pass NULL for it */
    uint8_t  dev_addr; /* Must not be referenced by the user-app- used internally by the SDK*/
    uint8_t  bus_num;   /* Must not be referenced by the user-app- used internally by the SDK*/
    ssl_lock_handle_t dev_lock;  /* Must not be referenced by the user-app- used internally by the SDK*/
}hbi_dev_cfg_t;

/*! \brief describes buffer format to be used by user to pass any  data to
 *         HBI Driver.
 */
typedef struct
{
   unsigned char *pData; /*!< pointer to user data buffer */
   size_t        size; /*!< length of the buffer in bytes */
}hbi_data_t;

/*! \brief user-specific configuration passed at the time of
 *         driver initialization
 */
typedef struct
{
    ssl_lock_handle_t lock; /*!< Driver serialising lock. supposed to be created
                               and passed by user. if passed, then driver will
                               serialize all calls using this lock.
                            */
}hbi_init_cfg_t;

/*! \brief provide header information of firmware image passed by user
 *
 */
typedef struct
{
   int   major_ver;  /*!< header version major num */
   int   minor_ver;  /*!< header version minor num */
   hbi_img_type_t image_type; /*!< firmware or configuration record */
   int    endianness; /*!< endianness of the image excluding header */
   int    fwr_code;   /*!< if image_type == firmware, tells the firmware opn code */
   size_t block_size; /*!< block length in words image is divided into */
   size_t img_len;  /*!< total length of the image excluding header  */
   int    hdr_len;    /*!< length of header */
}hbi_img_hdr_t;

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
    uint8_t  isImage_typeC;   /*0: for static *.bin, 1: for *.h */
}hbi_dev_info_t;

/*! \brief HBI Handle updated by driver during HBI_Open() call and
 *                      used in further driver calls
 */
typedef uint32_t      hbi_handle_t;
typedef int           hbi_device_id_t;
/*! \brief basic data type for buffer as should be used by user
                         in call to HBI_read() ,HBI_write()
 */
typedef unsigned char user_buffer_t;

hbi_status_t HBI_dev_info(hbi_handle_t handle);
/*! \fn  hbi_status_t HBI_init(const hbi_init_cfg_t *)
 *
 * \brief Driver initialization function.
 *
 * Allocates and initialize resources as required by driver. User may pass
 * one time initialization configuration. This function may be called multiple
 * times. however if already initialized with one configuration that will be
 * used for all users until HBI_term() is called.
 * This is not an interrupt safe function and should be called from process
 * context.
 *
 * @param [in] pCfg - Pointer to const driver init configuration. This is optional
 *                     parameter.
 *
 *
*/
hbi_status_t HBI_init(hbi_init_cfg_t *pCfg,  struct i2c_client *pClient, const struct i2c_device_id *pDeviceId);

/*! \fn hbi_status_t HBI_term(void)
 *
 * \brief Driver termination function.
 *
 * Deallocates and deinitializes all resources as acquired in HBI_init() function.
 * Make sure to close all of the devices and users before terminating the
 * driver else it will return an error. function is not interrupt safe and
 * should be called from process context.
 *
 *
*/

hbi_status_t HBI_term(void);


/*! \fn hbi_status_t HBI_open(hbi_handle_t *pHandle, hbi_dev_cfg_t *pDevCfg)
 *
 * \brief function to open a device.
 *
 * Inputs one-time device configuration as passed by user. User can call this
 * function multiple time for different or same device. if called on already
 * open device, driver will pass back already opened device reference with existing
 * configuration and that user passed configuration will be ignored.
 * This function is not interrupt safe.
 *
 * \param [in] pHandle - Updated by driver after successful open call. Reference handle
 *               of device to be used by user in subsequent HBI driver device
 *               access call.
 * \param [in] pDevCfg - Pointer to device configuration.
 *
 * \param [out] pHandle - Device handle
 *
*/

hbi_status_t HBI_open(hbi_handle_t *pHandle, hbi_dev_cfg_t *pDevCfg);

/*! \fn hbi_status_t HBI_close(hbi_handle_t handle)
 *
 * \brief function to close a device opened using HBI_open().
 *
 * Deallocate and free up resources acquired during open call. If there are
 * multiple user on the device, device physically doesn't close until user count
 * reaches to 0 otherwise it just free up user reference and access to device.
 * This function is not interrupt safe.
 *
 * \param [in] handle - Device handle as passed by HBI_open()
 *
 * \retval HBI_STATUS_SUCCESS
 * \retval HBI_STATUS_BAD_HANDLE
 * \retval HBI_STATUS_NOT_INIT
 *
*/
hbi_status_t HBI_close(hbi_handle_t handle);

/*! \fn hbi_status_t HBI_reset(hbi_handle_t handle, hbi_rst_mode_t mode)
 *
 * \brief function to reset a device.
 *
 * This function equivalent to Power On Reset.This function is not
 * interrupt safe.
 *
 * \param [in] handle  - Device handle as returned by HBI_open()
 * \param [in] mode    - Reset mode of device
 *
 * \retval HBI_STATUS_SUCCESS
 * \retval HBI_STATUS_BAD_HANDLE
 * \retval HBI_STATUS_NOT_INIT
 * \retval HBI_STATUS_INTERNAL_ERR
 *
*/

hbi_status_t HBI_reset(hbi_handle_t handle, hbi_rst_mode_t mode);

/*! \fn hbi_status_t HBI_read(hbi_handle_t handle, reg_addr_t reg,
 *                      user_buffer_t *pOutput,
 *                     size_t length)
 *
 * \brief Reads the data from device memory starting from register address up to
 *     specified length
 *
 * Reads the data from device memory starting from register address up to
 * specified length. It is caller responsibility to ensure that user buffer is sufficiently
 * large enough to hold requested length of data else it may result in
 * system crash. driver may expect length in multiple of 16-bit depending
 * upon device family it is compiled for. This function is not interrupt safe.
 *
 * \param [in] handle  - Device handle as returned by HBI_open()
 * \param [in] reg     - Device register address to read from
 * \param [in] pOutput   - Pointer to user buffer to read data into
 * \param [in] length  - length of the data to be read in bytes. should be
                         passed based on
                         number_of_elements_to_be_read*sizeof(user_buffer_t)
 * \param [out] pOutput - updated by driver on success
 *
 * \retval HBI_STATUS_SUCCESS
 * \retval HBI_STATUS_BAD_HANDLE
 * \retval HBI_STATUS_NOT_INIT
 * \retval HBI_STATUS_INTERNAL_ERR
 *
*/
hbi_status_t HBI_read(hbi_handle_t handle,
                     reg_addr_t reg,
                     user_buffer_t *pOutput,
                     size_t length);


/*! \fn hbi_status_t HBI_write(hbi_handle_t handle,
                     reg_addr_t reg,
                     user_buffer_t *pInput,
                     size_t length)
 *
 * \brief Writes the data from user buffer to device memory starting from register
 *       address up to specified length
 *
 * Writes the data from user buffer to device memory starting from register
 * address up to specified length. It is caller responsibility to ensure that
 * user buffer is sufficiently allocated and filled to number of elements to be written.
 * Else it may result in system crash. driver may expect
 * length in multiple of 16-bit depending upon device family it is compiled
 * for. This function is not interrupt safe.
 *
 * \param [in] handle  - Device handle as returned by HBI_open()
 * \param [in] reg     - Device register address to write to
 * \param [in] pInput   - Pointer to user buffer to read data from
 * \param [in] length  - length of the data to be written in bytes. should be calculated
 *                       based on sizeof(user_buffer_t)*number_of_elements_to_fill in
 *                       buffer. lets say, user allocate a user_buffer_t reg[2] and what
 *                       to read 2 entries of the buffer then length
 *                       length = 2*sizeof(user_buffer_t)
 *
 *
 * \retval HBI_STATUS_SUCCESS
 * \retval HBI_STATUS_BAD_HANDLE
 * \retval HBI_STATUS_NOT_INIT
 * \retval HBI_STATUS_INTERNAL_ERR
 *
*/

hbi_status_t HBI_write(hbi_handle_t handle,
                     reg_addr_t reg,
                     user_buffer_t *pInput,
                     size_t length);

 /*! \fn hbi_status_t HBI_set_command(hbi_handle_t handle, hbi_cmd_t cmd,
  *                                    void *pCmdArgs)
  *
  * \brief Set the Host Commands as listed in hbi_cmd_t enum
  *
  *  Set the Host Commands as listed in hbi_cmd_t enum.This function is not interrupt safe.
  *
  * \param [in] handle  - Device handle as returned by HBI_open()
  * \param [in] cmd     - Host Command as listed in hbi_cmd_t
  * \param [in] pCmdArgs - Pointer to respective command arguments
   *
  * \param [out] pCmdArgs - May be updated by driver depending upon command issued.
  *
  * \retval HBI_STATUS_SUCCESS
  * \retval HBI_STATUS_BAD_HANDLE
  * \retval HBI_STATUS_NOT_INIT
  * \retval HBI_STATUS_INTERNAL_ERR
  * \retval HBI_STATUS_INVALID_ARG
  *
 */
hbi_status_t HBI_set_command(hbi_handle_t handle, hbi_cmd_t cmd, void *pCmdArgs);

 /*! \fn hbi_status_t HBI_wake(hbi_handle_t handle);
  *
  * \brief wakes up the device from sleep mode.
  *
  * Wakes up the device from sleep mode.This function is not interrupt safe.
  *
  * \param [in] handle  - Device handle as returned by HBI_open()
  *
  *
  *
  * \retval HBI_STATUS_SUCCESS
  * \retval HBI_STATUS_BAD_HANDLE
  * \retval HBI_STATUS_NOT_INIT
  * \retval HBI_STATUS_INTERNAL_ERR
  *
 */

hbi_status_t HBI_wake(hbi_handle_t handle);

/*! \fn hbi_status_t HBI_sleep(hbi_handle_t handle);
 *
 * \brief This function puts device to sleep mode.
 *
 * Puts device to sleep mode.This function is not interrupt safe.
 *
 * \param [in] handle  - Device handle as returned by HBI_open()
 *
 *
 * \retval HBI_STATUS_SUCCESS
 * \retval HBI_STATUS_BAD_HANDLE
 * \retval HBI_STATUS_NOT_INIT
 * \retval HBI_STATUS_INTERNAL_ERR
 *
*/

hbi_status_t HBI_sleep(hbi_handle_t handle);

/*! \fn hbi_status_t HBI_get_header(hbi_data_t *, hbi_img_hdr_t *);

 *
 * \brief This function parses header of the image passed by user.
 *
 * Parses Image Header. It is caller responsibility to ensure that user buffer is sufficiently
 * allocated and filled to number of elements to be written.
 * Else it may result in system crash. driver may expect
 * length in multiple of 16-bit depending upon device family it is compiled
 * for. This function is not interrupt safe.
 *
 * \param [in] pImg  - pointer to a structure containing image buffer and its length
 *
 * \param [out] pHeaderInfo - pointer updated by function with information extracted from
 *                 header
 *
 * \retval HBI_STATUS_SUCCESS
 *
*/

hbi_status_t HBI_get_header(hbi_data_t *pImg, hbi_img_hdr_t *pHeaderInfo);

/** \} */

#endif /* __HBI_H__*/
