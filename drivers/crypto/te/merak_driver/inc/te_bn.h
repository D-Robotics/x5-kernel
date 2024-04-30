//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_BN_H__
#define __TRUSTENGINE_BN_H__

#include "driver/te_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

/**
 * Trust engine bignumber structure
 */
typedef struct _te_bn_t te_bn_t;

/**
 * The bn sign only for base operations such as add, sub, mul.
 *
 * For mod operations and high opertaions and logic operations, sign MUST be
 * Positive
 *
 */

int te_bn_get_drv_handle(te_bn_t *bn, te_drv_handle *hdl);

/**
 * \brief This function allocate one bignumber instance \p bn in te driver \p
 * drv. The new allocated bignumber is binded on te driver \p drv.
 *
 * This makes the bignumber ready to be set or freed, but does not define a
 * value for the bignumber.
 *
 * if \p bytelen_hint is 0, will not allocate sram or GR.
 *
 * \param[in] drv           The te driver instance to bind.
 * \param[in] bytelen_hint  The bignumber byte length hint.
 *                          0 for empty.
 * \param[out] bn           The bignumber context pointer.
 *                          This must not be \c NULL.
 * \return                  See te error code.
 */
int te_bn_alloc(const te_drv_handle hdl, int32_t bytelen_hint, te_bn_t **bn);

/**
 * \brief This function frees the componets of an bignumber context.
 *
 * \param[in] bn    The bignumber context to be cleared.This may be \c NULL,
 *                  in which case this function is a no-op. If it is
 *                  not \c NULL, it must point to an initialized bignumber.
 * \return          See te error code.
 */
void te_bn_free(te_bn_t *bn);

/**
 * \brief Set the sign of one bignumber.
 *
 * \param[in] bn        The bignumber to set. It must be initialized.
 * \param[in] sign      The new sign. MUST be 1 or -1.
 * \return              See te error code.
 */
int te_bn_set_sign(te_bn_t *bn, int32_t sign);

/**
 * \brief Get the sign of one bignumber.
 *
 * \param[in] bn        The bignumber to query. It must be initialized.
 * \param[out] sign     The pointer to save sign.
 *                      1: positive. -1: negative.
 * \return              others: see te error code.
 */
int te_bn_get_sign(te_bn_t *bn, int32_t *sign);

/**
 * \brief Enlarge an bignumber to the specified number of limbs.
 *
 * \note  This function does nothing if the bignumer is already large enough.
 *
 * \param[in] bn        The bignumber to grow. It must be initialized.
 * \param[in] bytelen   The target byte length.
 * \return              See te error code.
 */
int te_bn_grow(te_bn_t *bn, size_t bytelen);

/**
 * \brief This function resizes an MPI downwards, keeping at least the
 *        specified number of limbs.
 *        Resize down as much as possible, while keeping at least the specified
 * number of limbs
 *
 * \param[in] bn        The MPI to shrink.
 *                      This must point to an initialized MPI.
 * \param[in] bytelen   The target byte length.
 * \return              See te error code.
 */

int te_bn_shrink(te_bn_t *bn, size_t bytelen);

/**
 * \brief Make a copy of an MPI.
 *
 * \param[out] bna   The destination MPI.
 *                   This must point to an initialized MPI.
 * \param[in] bnb    The source MPI.
 *                   This must point to an initialized MPI.
 * \return           See te error code.
 */
int te_bn_copy(te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Swap the contents of two MPIs.
 *
 * \param[in] bna   The first MPI. It must be initialized.
 * \param[in] bnb   The second MPI. It must be initialized.
 * \return           See te error code.
 */
int te_bn_swap(te_bn_t *bna, te_bn_t *bnb);

/**
 * \brief Import an MPI from unsigned big endian binary data.
 *
 * \param[in] bn    The destination MPI. This must point to an initialized MPI.
 * \param[in] buf   The input buffer. This must be a readable buffer of length
 *                 \p size Bytes.
 * \param[in] size  The length of the input buffer \p buf in Bytes.
 * \param[in] sign  The sign of destination MPI, can be 0 or 1.
 * \return          See te error code.
 */
int te_bn_import(te_bn_t *bn, const uint8_t *buf, size_t size, int32_t sign);

/**
 * \brief Export an MPI into unsigned big endian binary data
 *                 of fixed size.
 *
 * \param[in] bn        The source MPI. This must point to an initialized MPI.
 * \param[out] buf      The output buffer. This must be a writable buffer of
 *                      length \p size Bytes.
 * \param[in,out] size  The output buffer size pointer,
 *                      updated to the MPI byte length.
 * \return              See te error code.
 */
int te_bn_export(te_bn_t *bn, uint8_t *buf, size_t size);

/**
 * \brief Import an MPI from integer value \p z.
 *
 * \note    The sign of value \p z is also saved.
 *
 * \param[in] bn    The destination MPI. This must point to an initialized MPI.
 * \param[in] z     The value to use.
 * \return          See te error code.
 */
int te_bn_import_s32(te_bn_t *bn, int32_t z);

/**
 * \brief Import an MPI from unsigned integer value \p uz.
 *
 * \note    The sign of value \p uz is always positive.
 *
 * \param[in] bn    The destination MPI. This must point to an initialized MPI.
 * \param[in] uz    The value to use.
 * \return          See te error code.
 */
int te_bn_import_u32(te_bn_t *bn, uint32_t uz);

/**
 * \brief Export an MPI into an integer value \p z.
 *
 * \note  The sign is also set to \p z.
 *        The MPI should can fit to one integer.
 *
 * \param[in] bn    The source MPI. This must point to an initialized MPI.
 * \param[out] z    The pointer to save integer value.
 * \return          See te error code.
 */
int te_bn_export_s32(te_bn_t *bn, int32_t *z);

/**
 * \brief Export an MPI into an unsigned integer value \p uz.
 *
 * \note  The sign of bn MUST be positive.
 *        The MPI should can fit to one unsinged integer.
 *
 * \param[in] bn    The source MPI. This must point to an initialized MPI.
 * \param[out] uz   The pointer to save integer value.
 * \return          See te error code.
 */
int te_bn_export_u32(te_bn_t *bn, uint32_t *uz);

/**
 * \brief Import an MPI with random data.
 *
 * \param[in] bn    The destination MPI. This must point to an initialized MPI.
 * \param[in] size  The number of random bytes to generate.
 * \param[in] sign  The sign of destination MPI, can be 0 or 1.
 * \param[in] f_rng The RNG function to use. This must not be \c NULL.
 * \param[in] p_rng The RNG parameter to be passed to \p f_rng. This may be
 *                  \c NULL if \p f_rng doesn't need a context argument.
 * \return          See te error code.
 */
int te_bn_import_random(te_bn_t *bn,
                        size_t size,
                        int32_t sign,
                        int (*f_rng)(void *, uint8_t *, size_t),
                        void *p_rng);

/**
 * \brief Compare two MPIs.
 *
 * \param[in] bna       The left-hand MPI. This must point to an initialized
 * MPI. \param[in] bnb       The right-hand MPI. This must point to an
 * initialized MPI. \param[out] result   The pointer to store result. \c 1 if \p
 * bna is greater than \p bnb \c -1 if \p bna is lesser than \p bnb \c 0 if \p
 * bna is equal to \p bnb \return          See te error code.
 */
int te_bn_cmp_bn(te_bn_t *bna, te_bn_t *bnb, int *result);

/**
 * \brief Compare an MPI with an integer.
 *
 * \param[in] bna       The left-hand MPI. This must point to an initialized
 * MPI. \param[in] b         The integer value to compare \p bna to. \param[out]
 * result   The pointer to store result. \c 1 if \p bna is greater than \p bnb
 *                  \c -1 if \p bna is lesser than \p bnb
 *                  \c 0 if \p bna is equal to \p bnb
 * \return          See te error code.
 */
int te_bn_cmp_s32(te_bn_t *bna, int32_t b, int *result);

/**
 * \brief Perform a left-shift on an MPI: bna = bnb << count
 *
 * \note MPI \p bna and \p bnb can be same.
 *
 * \param[out] bna      The result to save shifted MPI.
 *                      This must point to an initialized MPI.
 * \param[in] bnb       The source MPI to shift.
 *                      This must point to an initialized MPI.
 * \param[in] count     The number of bits to shift by.
 * \return              See te error code.
 */
int te_bn_shift_l(te_bn_t *bna, const te_bn_t *bnb, int count);

/**
 * \brief Perform a right-shift on an MPI: bna = bnb >> count
 *
 * \note MPI \p bna and \p bnb can be same.
 *
 * \param[out] bna      The result to save shifted MPI.
 *                      This must point to an initialized MPI.
 * \param[in] bnb       The source MPI to shift.
 *                      This must point to an initialized MPI.
 * \param[in] count     The number of bits to shift by.
 * \return              See te error code.
 */
int te_bn_shift_r(te_bn_t *bna, const te_bn_t *bnb, int count);

/**
 * \brief Get a specific bit from an MPI.
 *
 * \param[in] bn    The MPI to query. This must point to an initialized MPI.
 * \param[in] pos   Zero-based index of the bit to query.
 * \return          \c 0 or \c 1 on success, depending on whether bit \c pos
 *                  of \c X is unset or set.
 * \return          A negative error code on failure.
 * \return          See te error code.
 */
int te_bn_get_bit(te_bn_t *bn, int pos);

/**
 * \brief Set a specific bit in an MPI.
 *
 * \note    This function will grow the target MPI if necessary to set a
 *          bit to \c 1 in a not yet existing limb. It will not grow if
 *          the bit should be set to \c 0.
 *
 * \param[in] bn    The MPI to modify. This must point to an initialized MPI.
 * \param[in] pos   Zero-based index of the bit to modify.
 * \param[in] val   The desired value of bit \c pos: \c 0 or \c 1.
 * \return          See te error code.
 */
int te_bn_set_bit(te_bn_t *bn, int pos, uint8_t val);

/**
 * \brief   Return the number of bits up to and including the most
 *          significant bit of value \c 1.
 *
 * \note    This is same as the one-based index of the most
 *          significant bit of value \c 1.
 *
 * \param[in] bn    The MPI to query. This must point to an initialized MPI.
 * \return          The number of bits up to and including the most
 *                  significant bit of value \c 1.
 * \return          A negative error code on failure.
 * \return          See te error code.
 */
int te_bn_bitlen(te_bn_t *bn);

/**
 * \brief           Return the number of bits of value \c 0 before the
 *                  least significant bit of value \c 1.
 *
 * \param[in] bn    The MPI to query. This must point to an initialized MPI.
 * \return          The number of bits of value \c 0 before the least
 * significant bit of value \c 1 in \p X.
 */
int te_bn_0bits_before_lsb1(te_bn_t *bn);

/**
 * \brief   Return the total size of an MPI value in bytes.
 *
 * \note    This is same as (bitlen + 7) / 8.
 *
 * \param[in] bn    The MPI to query. This must point to an initialized MPI.
 * \return          The least number of bytes capable of storing
 *                  the absolute value of \p bnb.
 * \return          A negative error code on failure.
 * \return          See te error code.
 */
int te_bn_bytelen(te_bn_t *bn);

/**
 * \brief Assign the absolute values of \p bnb to MPI \p bna : bnb = |bna|
 *
 * \note MPI \p bna and \p bnb can be same.
 *       The sign of \p bna is always positive (0).
 *
 * \param[out] bna  The destination MPI. This must point to an initialized MPI.
 * \param[in] bnb   The original MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_abs(te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Perform the AND logic of two MPIs: bnr = bna & bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first MPI. This must point to an initialized MPI.
 * \param[in] bnb   The second MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_and(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Perform the OR logic of two MPIs: bnr = bna | bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first MPI. This must point to an initialized MPI.
 * \param[in] bnb   The second MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_or(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Perform the XOR logic of two MPIs: bnr = bna ^ bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first MPI. This must point to an initialized MPI.
 * \param[in] bnb   The second MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_xor(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief The following section describes the basic arithmetic operations.
 */

/**
 * \brief Perform a signed addition of MPIs: bnr = bna + bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first summand. This must point to an initialized MPI.
 * \param[in] bnb   The second summand. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_add_bn(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnb);

/* bnr = bna - bnb */

/**
 * \brief Perform a signed substraction of MPIs: bnr = bna - bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first minuend. This must point to an initialized MPI.
 * \param[in] bnb   The subtrahend. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_sub_bn(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Perform a signed addition of MPI and integer number: bnr = bna + b
 *
 * \note    MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first summand. This must point to an initialized MPI.
 * \param[in] b     The second integer summand.
 * \return          See te error code.
 */
int te_bn_add_s32(te_bn_t *bnr, const te_bn_t *bna, int32_t b);

/**
 * \brief Perform a signed substraction of MPI and integer number: bnr = bna - b
 *
 * \note    MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first minuend. This must point to an initialized MPI.
 * \param[in] bnb   The integer subtrahend.
 * \return          See te error code.
 */
int te_bn_sub_s32(te_bn_t *bnr, const te_bn_t *bna, int32_t b);

/**
 * \brief Assign the negative values of \p bna to MPI \p bnr : bnr = -bna
 *
 * \note MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bnb   The original MPI. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_neg_bn(te_bn_t *bnr, const te_bn_t *bna);

/**
 * \brief Perform a unsigned addition of MPIs: bnr = |bna| + |bnb|
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first summand. This must point to an initialized MPI.
 * \param[in] bnb   The second summand. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_add_abs(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Perform a unsigned substraction of MPIs: bnr = |bna| - |bnb|
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first minuend. This must point to an initialized MPI.
 * \param[in] bnb   The subtrahend. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_sub_abs(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Perform a multiplication of two MPIs: bnr = bna * bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first factor. This must point to an initialized MPI.
 * \param[in] bnb   The second factor. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_mul_bn(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Perform a multiplication of an MPI with an integer: bnr = bna * b
 *
 * \note    MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first factor. This must point to an initialized MPI.
 * \param[in] b     The second factor.
 * \return          See te error code.
 */
int te_bn_mul_s32(te_bn_t *bnr, const te_bn_t *bna, const int32_t b);

/**
 * \brief Perform a square of an MPI: bnr = bna * bna
 *
 * \note    MPI \p bna and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The source MPI to be squared.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_square(te_bn_t *bnr, const te_bn_t *bna);

/**
 * \brief Perform a division with remainder of two MPIs: bna = bnq * bnb + bnr.
 *        bnr has the same sign as bna, bnq round to zero
 *
 * \note    MPI \p bna and \p bnq can be same.
 *          MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnq can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnq  The destination MPI for the quotient.
 *                  This may be \p NULL if the value of quotient is not needed.
 * \param[out] bnr  The destination MPI the remainder value.
 *                  This may be \c NULL if the value of the remainder is
 *                  not needed.
 * \param[in] bna   The dividend. This must point to an initialized MPi.
 * \param[in] bnb   The divisor. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_div_bn(te_bn_t *bnq,
                 te_bn_t *bnr,
                 const te_bn_t *bna,
                 const te_bn_t *bnb);

/**
 * \brief Perform a modular reduction: bnr = bna mod bnn.
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The MPI to compute the residue of.
 *                  This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_mod_bn(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnn);

/**
 * \brief Perform a modular reduction on unsigned value: r = bna mod n.
 *
 * \param[out] r    The destination value potiner for the residue value.
 * \param[in] bna   The MPI to compute the residue of.
 *                  This must point to an initialized MPI.
 * \param[in] n     The base of the modular reduction.
 *                  This must NOT be 0.
 * \return          See te error code.
 */
int te_bn_mod_u32(uint32_t *r, const te_bn_t *bna, uint32_t n);

/* bnr = (bna + bnb) mod bnn. bna and bnb MUST < bnn */

/**
 * \brief   Perform a modular reduction of signed addition of two MPIs:
 *          bnr = (bna + bnb) mod bnn
 *
 * \note    both \p bna and \p bnb MUST < \p bnb
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The first summand. This must point to an initialized
 * MPI. \param[in] bnb   The second summand. This must point to an
 * initialized MPI. \param[in] bnn   The base of the modular reduction. This
 * must point to an initialized MPI. \return          See te error code.
 */
int te_bn_add_mod(te_bn_t *bnr,
                  const te_bn_t *bna,
                  const te_bn_t *bnb,
                  const te_bn_t *bnn);

/**
 * \brief   Perform a modular reduction of signed substraction of two MPIs:
 *          bnr = (bna - bnb) mod bnn
 *
 * \note    both \p bna and \p bnb MUST < \p bnb
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The first minuend. This must point to an initialized MPI.
 * \param[in] bnb   The subtrahend. This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_sub_mod(te_bn_t *bnr,
                  const te_bn_t *bna,
                  const te_bn_t *bnb,
                  const te_bn_t *bnn);

/**
 * \brief Perform a modular reduction of multiplication of two MPIs:
 *          bnr = (bna * bnb) mod bnn
 *
 * \note    The bit length of \p bna and MUST < \p bnb
 *          The bit length of \p bnb and MUST < \p bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The first factor. This must point to an initialized MPI.
 * \param[in] bnb   The second factor. This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_mul_mod(te_bn_t *bnr,
                  const te_bn_t *bna,
                  const te_bn_t *bnb,
                  const te_bn_t *bnn);

/**
 * \brief Perform a modular reduction of square of MPIs:
 *          bnr = (bna * bna) mod bnn
 *
 * \note    The bit length of \p bna and MUST < \p bnb
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI for the residue value.
 *                  This must point to an initialized MPI.
 * \param[in] bna   The source MPI to be squared.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_square_mod(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnn);

/**
 * \brief   Compute the modular inverse: bnr = (a^-1) mod bnn
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The MPI to calculate the modular inverse of.
 *                  This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_inv_mod(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnn);

/**
 * \brief Perform a exponentiation: bnr = bna^bne mod bnn
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bne and \p bnr can be same.
 *          MPI \p bnn and \p bnr can NOT be same.
 *
 * \note    Both MPIs' bit length of \p bna and \p bne should < bit length of
 *          \p bnn
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The base of the exponentiation.
 *                  This must point to an initialized MPI.
 * \param[in] bne   The exponent MPI. This must point to an initialized MPI.
 * \param[in] bnn   The base of the modular reduction.
 *                  This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_exp_mod(te_bn_t *bnr,
                  const te_bn_t *bna,
                  const te_bn_t *bne,
                  const te_bn_t *bnn);

/**
 * \brief This section describes other arithmetic operations
 */

/**
 * \brief Compute the greatest common divisor: bnr = gcd(bna, bnb)
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The destination MPI. This must point to an initialized MPI.
 * \param[in] bna   The first operand. This must point to an initialized MPI.
 * \param[in] bnb   The second operand. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_gcd(te_bn_t *bnr, const te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Checks whether two MPIs are relative prime: gcd(bna, bnb) == 1
 *
 * \param[in] bna   The first operand. This must point to an initialized MPI.
 * \param[in] bnb   The second operand. This must point to an initialized MPI.
 * \return          \p TE_ERROR_NOT_ACCEPTABLE : not relative prime
 *                  \p TE_SUCCESS : relative prime
 *                  A negative error code on failure.
 * \return          See te error code.
 */
int te_bn_relative_prime(const te_bn_t *bna, const te_bn_t *bnb);

/**
 * \brief Compute the greatest common divisor: bnr = gcd(bna, bnb)
 *        u * bna + v *bnb = bnr
 *
 * \note    MPI \p bna and \p bnr can be same.
 *          MPI \p bnb and \p bnr can be same.
 *
 * \param[out] bnr  The greatest common divisor value.
 *                  This must point to an initialized MPI.
 * \param[out] bnu  The coefficient to \p bna.
 *                  This may be \p NULL if the value of coefficient to \p bna
 *                  is not needed.
 * \param[out] bnv  The coefficient to \p bnb.
 *                  This may be \p NULL if the value of coefficient to \p bnb
 *                  is not needed.
 * \param[in] bna   The first operand. This must point to an initialized MPI.
 * \param[in] bnb   The second operand. This must point to an initialized MPI.
 * \return          See te error code.
 */
int te_bn_gcd_ext(te_bn_t *bnr,
                  te_bn_t *bnu,
                  te_bn_t *bnv,
                  const te_bn_t *bna,
                  const te_bn_t *bnb);

/**
 * \brief Perform the Miller-Rabin primality test.
 *
 * \param[in] bn                The MPI to check for primality.
 *                              This must point to an initialized MPI.
 * \param[in] round             The number of bases to perform the Miller-Rabin
 *                              primality test for.
 *                              The probability of returning 0 on a composite is
 *                              at most 2<sup>-2*\p round</sup>.
 * \param[in] f_rng             The RNG function to use.
 *                              This must not be \c NULL.
 * \param[in] p_rng             The RNG parameter to be passed to \p f_rng.
 *                              This may be \c NULL if \p f_rng doesn't use
 *                              a context parameter.
 * \return                      \c 0 if successful, i.e. \p bn is probably prime.
 *                              \c TE_ERROR_NOT_ACCEPTABLE if \p bn is not prime
 *                              A negative error code on failure.
 *                              See te error code.
 */
int te_bn_is_probale_prime(const te_bn_t *bn,
                           uint32_t round,
                           int (*f_rng)(void *, uint8_t *, size_t),
                           void *p_rng);
/**
 * \brief Generae a prime number as bignumber.
 * To generate an RSA key in a way recommended by FIPS 186-4, both primes must
 * be either 1024 bits or 1536 bits long, and the is_low_err should be true.
 *
 * \param[out] bn
 *
 * \param[in] bn                The MPI to save the prime number.
 *                              This must point to an initialized MPI.
 * \param[in] is_low_err        Whether uses lower error rate:
 *                              from 2<sup>-80</sup> to 2<sup>-128</sup>
 *                              constraint to generate the prime.
 * \param[in] is_dh_prim        Whether makes (bn - 1)/2 also prime too.
 * \param[in] nbits             The bit number of the prime.
 * \param[in] f_rng             The RNG function to use.
 *                              This must not be \c NULL.
 * \param[in] p_rng             The RNG parameter to be passed to \p f_rng.
 *                              This may be \c NULL if \p f_rng doesn't use
 *                              a context parameter.
 * \return                      See te error code.
 */
int te_bn_gen_prime(te_bn_t *bn,
                    bool is_low_err,
                    bool is_dh_prim,
                    size_t nbits,
                    int (*f_rng)(void *, uint8_t *, size_t),
                    void *p_rng);
/**
 * \brief Perform the ECP Point multiplation:
 *      (Rx, Ry, Rz) = k * (X, Y, Z)
 *
 * \note  Only support Short Weierstrass cureve:
 *      y^2 = x^3 + A x + B mod P
 *
 * \note  The input point (X, Y, Z) fromat is detected by Z == 1:
 * Z == 1: The point is used as Affine format, and mixed_affine is enabled in
 * point add.
 * Z != 1: The point is used as Jacobian format, and mixed_affine is disabled in
 * point add.
 *
 * \param[in] P     The ECP base prime P
 * \param[in] A     The A in the equation
 * \param[in] X     The X value of base point
 * \param[in] Y     The Y value of base point
 * \param[in] Z     The Z value of base point
 * \param[in] k     The multiplation value
 * \param[out] Rx   The X value of output
 * \param[out] Ry   The Y value of output
 * \param[out] Rz   The Z value of output
 * \return          See te error code.
 */
int te_bn_ecp_mul(const te_bn_t *P,
                  const te_bn_t *A,
                  const te_bn_t *X,
                  const te_bn_t *Y,
                  const te_bn_t *Z,
                  const te_bn_t *k,
                  te_bn_t *Rx,
                  te_bn_t *Ry,
                  te_bn_t *Rz);
/**
 * \brief Perform the ECP Point add:
 *      (Rx, Ry, Rz) = (X1, Y1, Z1) + (X2, Y2, Z2)
 *
 * \note  Only support Short Weierstrass cureve:
 *      y^2 = x^3 + A x + B mod P
 *
 * \note  Here uses affine format.
 *
 * \param[in] P     The ECP base prime P
 * \param[in] X1    The X value of first summand
 * \param[in] Y1    The Y value of first summand
 * \param[in] Z1    The Z value of first summand
 * \param[in] X2    The X value of second summand
 * \param[in] Y2    The Y value of second summand
 * \param[in] Z2    The Z value of second summand
 * \param[out] Rx   The X value of output
 * \param[out] Ry   The Y value of output
 * \param[out] Rz   The Z value of output
 * \return          See te error code.
 */
int te_bn_ecp_add(const te_bn_t *P,
                  const te_bn_t *X1,
                  const te_bn_t *Y1,
                  const te_bn_t *Z1,
                  const te_bn_t *X2,
                  const te_bn_t *Y2,
                  const te_bn_t *Z2,
                  te_bn_t *Rx,
                  te_bn_t *Ry,
                  te_bn_t *Rz);

/**
 * \brief Conver one ECP Point from jacobian to affine:
 *      Jacobian(x, y, z) = Affine(X, Y)
 *
 * \note  Only support Short Weierstrass cureve:
 *      y^2 = x^3 + A x + B mod P
 *
 * \param[in] P     The ECP base prime P
 * \param[in] X     The X value in jacobian
 * \param[in] Y     The Y value in jacobian
 * \param[in] Z     The Z value in jacobian
 * \param[out] Rx   The X value of output
 * \param[out] Ry   The Y value of output
 * \return          See te error code.
 */

int te_bn_ecp_jacobian_to_affine(const te_bn_t *P,
                                 const te_bn_t *X,
                                 const te_bn_t *Y,
                                 const te_bn_t *Z,
                                 te_bn_t *Rx,
                                 te_bn_t *Ry,
                                 te_bn_t *Rz);

/**
 * \brief Dump one MPI's data and info
 * 
 * \param[in] name  The MPI name.
 * \param[in] bn    The BN pointer.
 */
void te_bn_dump(const char *name, const te_bn_t *bn);

/**
 * \brief Perform MP self test
 *
 * \param[in] drv   The te driver.
 * \return          \c 0: success
 *                  A negative error code on failure.
 * \return          See te error code.
 */
int te_bn_self_test(te_crypt_drv_t *drv);

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_BN_H__ */
