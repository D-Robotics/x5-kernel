//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __OSAL_ERR_H__
#define __OSAL_ERR_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The common error code for OSAL
 *
 * We want to keep all the error codes aligned within this one error code
 * enumaration across the whole project.
 * */
typedef enum _osal_err_t {

    /* The following error code are aligned with TEE Internal APIs error
       code */
    OSAL_SUCCESS                       = 0x00000000,
    OSAL_ERROR_CORRUPT_OBJECT          = 0xF0100001,
    OSAL_ERROR_CORRUPT_OBJECT_2        = 0xF0100002,
    OSAL_ERROR_STORAGE_NOT_AVAILABLE   = 0xF0100003,
    OSAL_ERROR_STORAGE_NOT_AVAILABLE_2 = 0xF0100004,
    OSAL_ERROR_GENERIC                 = 0xFFFF0000,
    OSAL_ERROR_ACCESS_DENIED           = 0xFFFF0001,
    OSAL_ERROR_CANCEL                  = 0xFFFF0002,
    OSAL_ERROR_ACCESS_CONFLICT         = 0xFFFF0003,
    OSAL_ERROR_EXCESS_DATA             = 0xFFFF0004,
    OSAL_ERROR_BAD_FORMAT              = 0xFFFF0005,
    OSAL_ERROR_BAD_PARAMETERS          = 0xFFFF0006,
    OSAL_ERROR_BAD_STATE               = 0xFFFF0007,
    OSAL_ERROR_ITEM_NOT_FOUND          = 0xFFFF0008,
    OSAL_ERROR_NOT_IMPLEMENTED         = 0xFFFF0009,
    OSAL_ERROR_NOT_SUPPORTED           = 0xFFFF000A,
    OSAL_ERROR_NO_DATA                 = 0xFFFF000B,
    OSAL_ERROR_OUT_OF_MEMORY           = 0xFFFF000C,
    OSAL_ERROR_BUSY                    = 0xFFFF000D,
    OSAL_ERROR_COMMUNICATION           = 0xFFFF000E,
    OSAL_ERROR_SECURITY                = 0xFFFF000F,
    OSAL_ERROR_SHORT_BUFFER            = 0xFFFF0010,
    OSAL_ERROR_EXTERNAL_CANCEL         = 0xFFFF0011,
    OSAL_ERROR_OVERFLOW                = 0xFFFF300F,
    OSAL_ERROR_TARGET_DEAD             = 0xFFFF3024,
    OSAL_ERROR_STORAGE_NO_SPACE        = 0xFFFF3041,
    OSAL_ERROR_MAC_INVALID             = 0xFFFF3071,
    OSAL_ERROR_SIGNATURE_INVALID       = 0xFFFF3072,
    OSAL_ERROR_TIME_NOT_SET            = 0xFFFF5000,
    OSAL_ERROR_TIME_NEEDS_RESET        = 0xFFFF5001,

    OSAL_TEE_EXT_CONN_ERROR_TIMEOUT     = 0xF1007020,
    OSAL_TEE_EXT_CONN_RECEIVE_WANT_MORE = 0xF1007021,
    OSAL_TEE_EXT_HTTP_DOWNLOAD_DONE     = 0xF1007030,
    /**
     * The following error code are in GP TEE defined:
     *  Reserved for implementation-specific errors: 0x80000000 - 0x8FFFFFFF
     * So that we can keep the error code align with GP TEE
     */

    /* The file operation related error */
    OSAL_ERROR_FILE_OPEN_FAILED   = 0x80100000,
    OSAL_ERROR_FILE_READ_FAILED   = 0x80100001,
    OSAL_ERROR_FILE_WRITE_FAILED  = 0x80100002,
    OSAL_ERROR_FILE_SEEK_FAILED   = 0x80100003,
    OSAL_ERROR_FILE_CREATE_FAILED = 0x80100004,
    OSAL_ERROR_FILE_DELETE_FAILED = 0x80100005,

    /* The JSON operation related error */
    OSAL_ERROR_BAD_JSON             = 0x80200006,
    OSAL_ERROR_JSON_ITERM_NOT_FOUND = 0x80200007,
    OSAL_ERROR_JSON_CREATE_FAILED   = 0x80200008,

    /* The Socket operation error code */
    OSAL_ERROR_SOCKET_ERR         = 0x80300100,
    OSAL_ERROR_SOCKET_TIMEOUT     = 0x80300101,
    OSAL_ERROR_SOCKET_RETRY       = 0x80300102,
    OSAL_ERROR_SOCKET_RESET       = 0x80300103,
    OSAL_ERROR_SOCKET_SEND_FAILED = 0x80300104,
    OSAL_ERROR_SOCKET_RECV_FAILED = 0x80300105,

    /* The connection adaption layer error code */
    OSAL_ERROR_CONN_RECV_WANT_MORE  = 0x80400002,
    OSAL_ERROR_CONN_SEND_TIMEOUT    = 0x80400007,
    OSAL_ERROR_CONN_RECV_TIMEOUT    = 0x80400000,
    OSAL_ERROR_CONN_CONNECT_TIMEOUT = 0x80400001,
    OSAL_ERROR_HTTP_BAD_HEADER      = 0x80400020,
    OSAL_ERROR_HTTP_BAD_DATA        = 0x80400021,
    OSAL_ERROR_HTTP_BAD_STATUS_CODE = 0x80400022,
    OSAL_ERROR_HTTP_DOWNLOAD_DONE   = 0x80400023,
    OSAL_ERROR_HTTP_BAD_URL         = 0x80400009,

    /* The util error code */
    OSAL_ERROR_BAD_HEX_STRING = 0x80500004,
    OSAL_ERROR_BAD_DEC_STRING = 0x80500009,
    OSAL_ERROR_BAD_CONFIG     = 0x80500005,

    OSAL_ERROR_BAD_SERVER_DATA     = 0x80A00003,
    OSAL_ERROR_BAD_SERVER_HEADER   = 0x80A00004,
    OSAL_ERROR_COMM_CHANNEL_FAILED = 0x80A00007,
    OSAL_ERROR_BAD_COMM_DATA       = 0x80A00000,

    /* The Secure debug specified error code */
    OSAL_ERROR_DEVICE_TERMINATE         = 0x80B00000,
    OSAL_ERROR_HTTP_GET_FAILED          = 0x80B00001,
    OSAL_ERROR_VERIFY_PUBKEY_FAILED     = 0x80B00002,
    OSAL_ERROR_VERIFY_SIGNATURE_FAILED  = 0x80B00003,
    OSAL_ERROR_EXEC_CMD_FAILED          = 0x80B00004,

} osal_err_t;

#ifdef __cplusplus
}
#endif

#endif /* __OSAL_ERR_H__ */
