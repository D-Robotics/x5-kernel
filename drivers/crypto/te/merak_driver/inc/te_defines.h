//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_DEFINES_H__
#define __TRUSTENGINE_DEFINES_H__

#define TE_HANDLE_NULL                   0

#define TE_TIMEOUT_INFINITE              0xFFFFFFFF

/* API Error Codes (GP TEE compliant) */
#define TE_SUCCESS                       0x00000000
#define TE_ERROR_GENERIC                 0xFFFF0000
#define TE_ERROR_ACCESS_DENIED           0xFFFF0001
#define TE_ERROR_CANCEL                  0xFFFF0002
#define TE_ERROR_ACCESS_CONFLICT         0xFFFF0003
#define TE_ERROR_EXCESS_DATA             0xFFFF0004
#define TE_ERROR_BAD_FORMAT              0xFFFF0005
#define TE_ERROR_BAD_PARAMS              0xFFFF0006
#define TE_ERROR_BAD_STATE               0xFFFF0007
#define TE_ERROR_ITEM_NOT_FOUND          0xFFFF0008
#define TE_ERROR_NOT_IMPLEMENTED         0xFFFF0009
#define TE_ERROR_NOT_SUPPORTED           0xFFFF000A
#define TE_ERROR_NO_DATA                 0xFFFF000B
#define TE_ERROR_OOM                     0xFFFF000C
#define TE_ERROR_BUSY                    0xFFFF000D
#define TE_ERROR_COMMUNICATION           0xFFFF000E
#define TE_ERROR_SECURITY                0xFFFF000F
#define TE_ERROR_SHORT_BUFFER            0xFFFF0010
#define TE_ERROR_EXTERNAL_CANCEL         0xFFFF0011
#define TE_ERROR_TIMEOUT                 0xFFFF3001
#define TE_ERROR_OVERFLOW                0xFFFF300F

/**
 * Extended Error Codes
 *
 * +------+----------+
 * |Type  |Range     |
 * +------+----------+
 * |Common|0x800000xx|
 * +------+----------+
 * |Cipher|0x800001xx|
 * +------+----------+
 * |MPI   |0x800002xx|
 * +------+----------+
 * |DHM   |0x800003xx|
 * +------+----------+
 * |PK    |0x800004xx|
 * +------+----------+
 */
#define TE_ERROR_AGAIN                   0x80000000
#define TE_ERROR_FEATURE_UNAVAIL         0x80000001
#define TE_ERROR_BAD_KEY_LENGTH          0x80000002
#define TE_ERROR_INVAL_KEY               0x80000003
#define TE_ERROR_BAD_INPUT_LENGTH        0x80000004
#define TE_ERROR_BAD_INPUT_DATA          0x80000005
#define TE_ERROR_AUTH_FAILED             0x80000006
#define TE_ERROR_INVAL_CTX               0x80000007
#define TE_ERROR_UNKNOWN_ALG             0x80000008
#define TE_ERROR_INVAL_ALG               0x80000009

#define TE_ERROR_INVAL_PADDING           0x80000100
#define TE_ERROR_INCOMPLETE_BLOCK        0x80000101

#define TE_ERROR_INVAL_CHAR              0x80000200
#define TE_ERROR_NEGATIVE_VALUE          0x80000201
#define TE_ERROR_DIV_BY_ZERO             0x80000202
#define TE_ERROR_NOT_ACCEPTABLE          0x80000203
#define TE_ERROR_NO_SRAM_SPACE           0x80000204
#define TE_ERROR_NO_AVAIL_GR             0x80000205
#define TE_ERROR_NO_AVAIL_LEN_TYPE       0x80000206
#define TE_ERROR_INVAL_MOD               0x80000207
#define TE_ERROR_NOT_PRIME               0x80000208
#define TE_ERROR_OP_TOO_LONG             0x80000209

#define TE_ERROR_READ_PARAMS             0x80000300
#define TE_ERROR_MAKE_PARAMS             0x80000301
#define TE_ERROR_READ_PUBLIC             0x80000302
#define TE_ERROR_MAKE_PUBLIC             0x80000303
#define TE_ERROR_CALC_SECRET             0x80000304
#define TE_ERROR_SET_GROUP               0x80000305

#define TE_ERROR_GEN_RANDOM              0x80000400
#define TE_ERROR_TYPE_MISMATCH           0x80000401
#define TE_ERROR_KEY_VERSION             0x80000402
#define TE_ERROR_KEY_FORMAT              0x80000403
#define TE_ERROR_INVAL_PUBKEY            0x80000404
#define TE_ERROR_UNKNOWN_CURVE           0x80000405
#define TE_ERROR_SIG_LENGTH              0x80000406
#define TE_ERROR_GEN_KEY                 0x80000407
#define TE_ERROR_CHECK_KEY               0x80000408
#define TE_ERROR_PUBLIC_OP               0x80000409
#define TE_ERROR_PRIVATE_OP              0x8000040A
#define TE_ERROR_VERIFY_SIG              0x8000040B
#define TE_ERROR_OUT_TOO_LARGE           0x8000040C

/* Operations */
#define TE_OPERATION_CIPHER               0x1
#define TE_OPERATION_MAC                  0x3
#define TE_OPERATION_AE                   0x4
#define TE_OPERATION_DIGEST               0x5
#define TE_OPERATION_ASYMMETRIC_CIPHER    0x6
#define TE_OPERATION_ASYMMETRIC_SIGNATURE 0x7
#define TE_OPERATION_KEY_DERIVATION       0x8

/* Main Algorithm */
#define TE_MAIN_ALGO_MD5                  0x01
#define TE_MAIN_ALGO_SHA1                 0x02
#define TE_MAIN_ALGO_SHA224               0x03
#define TE_MAIN_ALGO_SHA256               0x04
#define TE_MAIN_ALGO_SHA384               0x05
#define TE_MAIN_ALGO_SHA512               0x06
#define TE_MAIN_ALGO_SM3                  0x07
#define TE_MAIN_ALGO_AES                  0x10
#define TE_MAIN_ALGO_DES                  0x11
#define TE_MAIN_ALGO_DDES                 0x12
#define TE_MAIN_ALGO_TDES                 0x13
#define TE_MAIN_ALGO_SM4                  0x14
#define TE_MAIN_ALGO_GFMUL                0x15
#define TE_MAIN_ALGO_SNOW3G               0x16
#define TE_MAIN_ALGO_ZUC                  0x17
#define TE_MAIN_ALGO_KASUMI               0x18
#define TE_MAIN_ALGO_RSA                  0x30
#define TE_MAIN_ALGO_DSA                  0x31
#define TE_MAIN_ALGO_DH                   0x32
#define TE_MAIN_ALGO_ECDSA                0x41
#define TE_MAIN_ALGO_ECDH                 0x42
#define TE_MAIN_ALGO_SM2                  0x45

/* Chain Mode */
#define TE_CHAIN_MODE_ECB_NOPAD           0x0
#define TE_CHAIN_MODE_CBC_NOPAD           0x1
#define TE_CHAIN_MODE_CTR                 0x2
#define TE_CHAIN_MODE_CTS                 0x3
#define TE_CHAIN_MODE_XTS                 0x4
#define TE_CHAIN_MODE_CBC_MAC_PKCS5       0x5
#define TE_CHAIN_MODE_CMAC                0x6
#define TE_CHAIN_MODE_CCM                 0x7
#define TE_CHAIN_MODE_GCM                 0x8
#define TE_CHAIN_MODE_PKCS1_PSS_MGF1      0x9
#define TE_CHAIN_MODE_OFB                 0xA
#define TE_CHAIN_MODE_ECB_PKCS7           0xB
#define TE_CHAIN_MODE_CBC_PKCS7           0xC
#define TE_CHAIN_MODE_F8                  0xD
#define TE_CHAIN_MODE_F9                  0xE

/*
 * Algorithm Identifiers (compliant with GP Core API v1.2.1)
 * Bitwise value with assignments
 *    [31:28]    class (operation)
 *    [23:20]    internal hash
 *    [15:12]    digest hash
 *    [11:8]     chain mode
 *    [7:0]      main algo
 */
#define TE_ALG_AES_ECB_NOPAD                   0x10000010
#define TE_ALG_AES_CBC_NOPAD                   0x10000110
#define TE_ALG_AES_CTR                         0x10000210
#define TE_ALG_AES_XTS                         0x10000410
#define TE_ALG_AES_OFB                         0x10000A10
#define TE_ALG_AES_CBC_MAC_NOPAD               0x30000110
#define TE_ALG_AES_CBC_MAC_PKCS5               0x30000510
#define TE_ALG_AES_CMAC                        0x30000610
#define TE_ALG_AES_CCM                         0x40000710
#define TE_ALG_AES_GCM                         0x40000810
#define TE_ALG_DES_ECB_NOPAD                   0x10000011
#define TE_ALG_DES_CBC_NOPAD                   0x10000111
#define TE_ALG_DES_CBC_MAC_NOPAD               0x30000111
#define TE_ALG_DES_CBC_MAC_PKCS5               0x30000511
#define TE_ALG_DES_CMAC                        0x30000611
#define TE_ALG_TDES_ECB_NOPAD                  0x10000013
#define TE_ALG_TDES_CBC_NOPAD                  0x10000113
#define TE_ALG_TDES_CBC_MAC_NOPAD              0x30000113
#define TE_ALG_TDES_CBC_MAC_PKCS5              0x30000513
#define TE_ALG_TDES_CMAC                       0x30000613
#define TE_ALG_SM4_ECB_NOPAD                   0x10000014
#define TE_ALG_SM4_CBC_NOPAD                   0x10000114
#define TE_ALG_SM4_CTR                         0x10000214
#define TE_ALG_SM4_XTS                         0x10000414
#define TE_ALG_SM4_OFB                         0x10000A14
#define TE_ALG_SM4_CBC_MAC_NOPAD               0x30000114
#define TE_ALG_SM4_CBC_MAC_PKCS5               0x30000514
#define TE_ALG_SM4_CMAC                        0x30000614
#define TE_ALG_SM4_CCM                         0x40000714
#define TE_ALG_SM4_GCM                         0x40000814
#define TE_ALG_SNOW3G_F8                       0x10000D16
#define TE_ALG_SNOW3G_F9                       0x30000E16
#define TE_ALG_ZUC_F8                          0x10000D17
#define TE_ALG_ZUC_F9                          0x30000E17
#define TE_ALG_KASUMI_F8                       0x10000D18
#define TE_ALG_KASUMI_F9                       0x30000E18
#define TE_ALG_GFMUL_CBC_MAC_NOPAD             0x30000115
#define TE_ALG_RSASSA_PKCS1_V1_5_MD5           0x70001830
#define TE_ALG_RSASSA_PKCS1_V1_5_SHA1          0x70002830
#define TE_ALG_RSASSA_PKCS1_V1_5_SHA224        0x70003830
#define TE_ALG_RSASSA_PKCS1_V1_5_SHA256        0x70004830
#define TE_ALG_RSASSA_PKCS1_V1_5_SHA384        0x70005830
#define TE_ALG_RSASSA_PKCS1_V1_5_SHA512        0x70006830
#define TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1      0x70212930
#define TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224    0x70313930
#define TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256    0x70414930
#define TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384    0x70515930
#define TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512    0x70616930
#define TE_ALG_RSAES_PKCS1_V1_5                0x60000130
#define TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1      0x60210230
#define TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224    0x60310230
#define TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256    0x60410230
#define TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384    0x60510230
#define TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512    0x60610230
#define TE_ALG_RSA_NOPAD                       0x60000030
#define TE_ALG_DSA_SHA1                        0x70002131
#define TE_ALG_DSA_SHA224                      0x70003131
#define TE_ALG_DSA_SHA256                      0x70004131
#define TE_ALG_DH_DERIVE_SHARED_SECRET         0x80000032
#define TE_ALG_MD5                             0x50000001
#define TE_ALG_SHA1                            0x50000002
#define TE_ALG_SHA224                          0x50000003
#define TE_ALG_SHA256                          0x50000004
#define TE_ALG_SHA384                          0x50000005
#define TE_ALG_SHA512                          0x50000006
#define TE_ALG_SM3                             0x50000007
#define TE_ALG_HMAC_MD5                        0x30000001
#define TE_ALG_HMAC_SHA1                       0x30000002
#define TE_ALG_HMAC_SHA224                     0x30000003
#define TE_ALG_HMAC_SHA256                     0x30000004
#define TE_ALG_HMAC_SHA384                     0x30000005
#define TE_ALG_HMAC_SHA512                     0x30000006
#define TE_ALG_HMAC_SM3                        0x30000007
#define TE_ALG_ECDSA_P192                      0x70001041
#define TE_ALG_ECDSA_P224                      0x70002041
#define TE_ALG_ECDSA_P256                      0x70003041
#define TE_ALG_ECDSA_P384                      0x70004041
#define TE_ALG_ECDSA_P521                      0x70005041
#define TE_ALG_ECDH_P192                       0x80001042
#define TE_ALG_ECDH_P224                       0x80002042
#define TE_ALG_ECDH_P256                       0x80003042
#define TE_ALG_ECDH_P384                       0x80004042
#define TE_ALG_ECDH_P521                       0x80005042
#define TE_ALG_SM2_KEP                         0x60000045
#define TE_ALG_SM2_DSA_SM3                     0x70007045
#define TE_ALG_SM2_PKE                         0x80000045

	/* Bits [31:28] */
#define TE_ALG_GET_CLASS(algo)         (((algo) >> 28) & 0xF)

	/* Bits [7:0] */
#define TE_ALG_GET_MAIN_ALG(algo)      ((algo) & 0xFF)

	/* Bits [11:8] */
#define TE_ALG_GET_CHAIN_MODE(algo)    (((algo) >> 8) & 0xF)

	/* Bits [15:12] */
#define TE_ALG_GET_DIGEST_HASH(algo)   (((algo) >> 12) & 0xF)

	/* Bits [23:20] */
#define TE_ALG_GET_INTERNAL_HASH(algo) (((algo) >> 20) & 0x7)

	/* Return hash algorithm based on main hash */
#define TE_ALG_HASH_ALGO(main_hash) \
        (TE_OPERATION_DIGEST << 28 | (main_hash))

	/* Extract internal hash and return hash algorithm */
#define TE_INTERNAL_HASH_TO_ALGO(algo) \
                TE_ALG_HASH_ALGO(TE_ALG_GET_INTERNAL_HASH(algo))

	/* Extract digest hash and return hash algorithm */
#define TE_DIGEST_HASH_TO_ALGO(algo) \
                TE_ALG_HASH_ALGO(TE_ALG_GET_DIGEST_HASH(algo))

/* Return HMAC algorithm based on main hash */
#define TE_ALG_HMAC_ALGO(main_hash) \
	(TE_OPERATION_MAC << 28 | (main_hash))


#define TE_AES_BLOCK_SIZE      16UL
#define TE_DES_BLOCK_SIZE      8UL
#define TE_SM4_BLOCK_SIZE      16UL
#define TE_SNOW3G_BLOCK_SIZE   4UL
#define TE_ZUC_BLOCK_SIZE      4UL
#define TE_KASUMI_BLOCK_SIZE   8UL
#define TE_EVAL_BLOCK_SIZE     8UL
#define TE_GHASH_BLOCK_SIZE    16UL
#define TE_MAX_SCA_BLOCK       16UL

#define TE_DES_KEY_SIZE        8UL
#define TE_TDES_KEY_SIZE       24UL
#define TE_SM4_KEY_SIZE        16UL
#define TE_MAX_AES_KEY         32UL
#define TE_MAX_SCA_KEY         32UL

#define TE_MD5_BLK_SIZE        64UL
#define TE_SHA1_BLK_SIZE       64UL
#define TE_SHA224_BLK_SIZE     64UL
#define TE_SHA256_BLK_SIZE     64UL
#define TE_SHA384_BLK_SIZE     128UL
#define TE_SHA512_BLK_SIZE     128UL
#define TE_SM3_BLK_SIZE        64UL
#define TE_MAX_HASH_BLOCK      TE_SHA512_BLK_SIZE

#define TE_MD5_HASH_SIZE       16UL
#define TE_SHA1_HASH_SIZE      20UL
#define TE_SHA224_HASH_SIZE    28UL
#define TE_SHA256_HASH_SIZE    32UL
#define TE_SHA384_HASH_SIZE    48UL
#define TE_SHA512_HASH_SIZE    64UL
#define TE_SM3_HASH_SIZE       32UL
#define TE_MAX_HASH_SIZE       TE_SHA512_HASH_SIZE

#define TE_MAX_DRV_NAME        8UL
#define TE_MAX_HOST_NUM        16UL

/* Link list align to 256bits */
#define LINK_LIST_ALIGN (32)

/* Link list entry size limit to 2^32 Bytes */
#define LLST_ENTRY_SZ_MAX   (0x100000000ULL)

#endif /* __TRUSTENGINE_DEFINES_H__ */
