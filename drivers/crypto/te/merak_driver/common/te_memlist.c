//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <te_memlist.h>

size_t te_memlist_get_total_len( te_memlist_t *list )
{
    size_t _i = 0;
    size_t total_len = 0;

    for (_i = 0; _i < list->nent; _i++){
        total_len += list->ents[_i].len;
    }
    return total_len;
}

void te_memlist_truncate_from_tail( te_memlist_t *list,
                                    uint8_t *buf,
                                    size_t size,
                                    bool b_copy,
                                    te_ml_bp_t *info )
{
    size_t len = 0;
    int n = 0;
    /*backup original info for recover*/
    info->nent = list->nent;
    if (0 == size) {
        info->len = list->ents[list->nent - 1].len;
        info->ind = list->nent - 1;
        info->offset = list->ents[list->nent - 1].len;
        return;
    }

    for (n = list->nent - 1; n >= 0; n--, list->nent--){
        len = (size > list->ents[n].len) ?
                                list->ents[n].len : size;
        if(b_copy){
            osal_memcpy(buf + size  - len,
                    (uint8_t*)list->ents[n].buf + list->ents[n].len - len,
                    len);
        }

        size -= len;
        /* hit breakpoint */
        if(0 == size){
            info->ind = n;
            info->len = list->ents[n].len;
            info->offset = list->ents[n].len - len;
            if (0 == info->offset) {
                list->nent--;
                info->ind -= 1;
                /**< two cases. case#1 nothing left if this case keep len and offset.
                 *              case#2 something left if this case we need to update
                 *                     the offset and len to the next node of the list.
                 *    if info->ind < 0, nothing left.
                 */
                if (info->ind >= 0) {
                    info->offset = list->ents[info->ind].len;
                    info->len = list->ents[info->ind].len;
                }
            } else {
                list->ents[n].len -= len;
            }
            return;
        }
    }
    /* should never reach here, unless reaminder is gt size of list */
    TE_ASSERT(0);
}

int te_memlist_copy_from_tail( te_memlist_t *list,
                                    uint8_t *buf,
                                    size_t size )
{
    size_t len = 0;
    int n = 0;

    if (!list || !buf || !size) {
        return TE_ERROR_BAD_PARAMS;
    }

    for (n = list->nent - 1; n >= 0; n--){
        len = (size > list->ents[n].len) ?
                                list->ents[n].len : size;
        osal_memcpy(buf + size  - len,
                (uint8_t*)list->ents[n].buf + list->ents[n].len - len,
                len);
        size -= len;
        if(0 == size){
            return TE_SUCCESS;
        }
    }

    return TE_ERROR_BAD_INPUT_LENGTH;
}

void te_memlist_truncate_from_head( te_memlist_t *list,
                                    size_t len,
                                    te_ml_bp_t *info )
{
    size_t offset = 0;
    size_t n = 0;

    info->nent = list->nent;
    for (n = 0; n < list->nent; n++){
        offset += list->ents[n].len;

        if(len <= offset){
            info->len = list->ents[n].len;
            list->ents[n].len -= (offset - len);
            info->offset = list->ents[n].len;
            info->ind = n;
            list->nent = n + 1;

            return;
        }
    }
    /* should never reach here, unless len is gt size of list */
    TE_ASSERT(0);
}

int te_memlist_clone( te_memlist_t *dst,
                      te_memlist_t *src,
                      size_t offset,
                      size_t len )
{
    size_t cp_offs = 0;
    size_t i = 0;
    size_t len_offs = 0;          /*mark offset for len */
    size_t dst_nodes = 0;         /*mark nodes for dst list */
    size_t dst_lst_nodes_len = 0; /*mark len of dst last nodes */
    size_t src_index = 0;         /*mark copy start index of src list*/
    size_t start_node_off = 0;    /*mark start node's offset*/
    size_t start_node_len = 0;    /*mark start node's length*/
    size_t last_pos = 0;          /*back up last cp_offs for overflow case*/

    if ((NULL == dst) || (NULL == src) || (0 == src->nent)) {
        return TE_ERROR_BAD_PARAMS;
    }
    /*locate copy slice in original list*/
    for (i = 0; i < src->nent; i++) {
        last_pos = cp_offs;
        cp_offs += src->ents[i].len;
        if ( (cp_offs > offset) || (last_pos > cp_offs) || (dst_nodes > 0) ) {
            len_offs = (cp_offs - offset);
            /*mark first node's position*/
            if (0 == dst_nodes) {
                src_index = i;
                start_node_off = src->ents[i].len - (cp_offs - offset);
                if (len_offs > len ) {
                    start_node_len = len;
                } else {
                    start_node_len = cp_offs - offset;
                }
            }
            dst_nodes++;
            if (len_offs >= len) {
                dst_lst_nodes_len = src->ents[i].len - (len_offs - len);
                break;
            }
        }
    }
    /*insuffient len return error*/
    if (len_offs < len) {
        return TE_ERROR_BAD_PARAMS;
    }
    /*create memory list slice*/
    dst->nent = dst_nodes;
    dst->ents =
         (te_mement_t *)osal_calloc(dst->nent * sizeof(te_mement_t), 1);

    if (NULL == dst->ents) {
        return TE_ERROR_OOM;
    }

    osal_memcpy(dst->ents, &src->ents[src_index],
                dst->nent * sizeof(te_mement_t));
    /*adjust first node's offset and lenght, also last node's length*/
    dst->ents[0].buf = (void*)((uintptr_t)dst->ents[0].buf + start_node_off);
    dst->ents[0].len = start_node_len;
    if (dst->nent > 1) {
        dst->ents[dst->nent - 1].len = dst_lst_nodes_len;
    }
    return TE_SUCCESS;
}

int te_memlist_dup( te_memlist_t *dst,
                    const te_memlist_t *src,
                    size_t offset,
                    size_t len )
{
    size_t i = 0;
    size_t oie = 0; /* offset into entry */
    size_t pos = 0;
    te_mement_t *s = NULL;
    te_mement_t *t = NULL;

    if ((NULL == dst) || (NULL == dst->ents) ||
        (NULL == src) || (NULL == src->ents)) {
        return TE_ERROR_BAD_PARAMS;
    }
    s = src->ents;
    t = dst->ents;
    dst->nent = 0;
    t->buf = NULL;
    for (i = 0; i < src->nent; i++, s++) {
        /* seek to 'offset' */
        if (0 == dst->nent) {
            if ((offset - pos) >= s->len) {
                pos += s->len;
                continue;
            } else {
                oie = offset - pos;
            }
        } else {
            oie = 0;
        }
        /* walk 'len' bytes from 'offset' */
        dst->nent++;
        pos += s->len;
        t->buf = (uint8_t *)s->buf + oie;
        if ((pos - offset) >= len) {
            t->len = s->len - (pos - (offset + len)) - oie;
            break; /* reach the end */
        } else {
            t->len = s->len - oie;
        }
        /* next node */
        t++;
    }

    if (i == src->nent) {
        return TE_ERROR_BAD_PARAMS;
    }

    return TE_SUCCESS;
}

int te_memlist_xor( te_memlist_t *dst,
                    const te_memlist_t *in,
                    const uint8_t *ks,
                    size_t len )
{
    size_t i = 0;
    size_t dst_i = 0;
    size_t left = len;
    size_t left_in;
    size_t proc_inlen;
    size_t proc_outlen;
    uint8_t *ptr_ks = (uint8_t *)ks;
    size_t dst_node_ofs = 0;   /** offset of the dst node from start */
    size_t in_node_ofs = 0;   /** offset of the in node from start */

    if ((NULL == dst) || (NULL == dst->ents) ||
        (NULL == in) || (NULL == in->ents) || (NULL == ks)) {
        return TE_ERROR_BAD_PARAMS;
    }
    while (left > 0) {
        left_in = proc_inlen = UTILS_MIN(left, in->ents[i].len - in_node_ofs);
        while (left_in > 0) {
            proc_outlen = UTILS_MIN(left_in , dst->ents[dst_i].len - dst_node_ofs);
            BS_XOR((uint8_t *)dst->ents[dst_i].buf + dst_node_ofs,
                   (uint8_t *)in->ents[i].buf + in_node_ofs,
                            ptr_ks, proc_outlen);
            left_in -= proc_outlen;
            left -= proc_outlen;
            ptr_ks += proc_outlen;
            /**
             *  consume the whole node then move to next one, or else record the
             *  offset from the start.
             */
            if (proc_outlen == (dst->ents[dst_i].len - dst_node_ofs)) {
                dst_i++;
                dst_node_ofs = 0;
            } else {
                dst_node_ofs += proc_outlen;
            }
            in_node_ofs += proc_outlen;
            TE_ASSERT(dst_i <= dst->nent);
        }
        if (proc_inlen == (in->ents[i].len - in_node_ofs)) {
            i++;
            in_node_ofs = 0;
        }
        TE_ASSERT(i <= in->nent);
    }
    return TE_SUCCESS;
}

int te_memlist_fill( te_memlist_t *dst,
                     size_t ofs,
                     const uint8_t *src,
                     size_t len )
{
    size_t i = 0;
    size_t pos = 0;
    size_t node_ofs = 0;   /** offset of the node from start */
    const uint8_t *s = src;

    if ((NULL == dst) || (NULL == dst->ents) || (NULL == src)) {
        return TE_ERROR_BAD_PARAMS;
    }

    for (i = 0; i < dst->nent; i++) {
         /* seek to 'offset' */
        if ( s == src ) {
            if ((ofs - pos) >= dst->ents[i].len) {
                pos += dst->ents[i].len;
                continue;
            } else {
                node_ofs = ofs - pos;
            }
        } else {
            node_ofs = 0;
        }
        /* fill 'len' bytes data into 'dst' from 'ofs' */
        pos += dst->ents[i].len;
        if ((pos - ofs) >= len) {
            osal_memcpy((uint8_t *)((uintptr_t)dst->ents[i].buf + node_ofs),
                         s, len - (s - src));
            break;  /* reach the end */
        } else {
            osal_memcpy((uint8_t *)((uintptr_t)dst->ents[i].buf + node_ofs),
                         s, pos - ofs - (s - src));
            s += (pos - ofs - (s - src));
        }
    }

    if (i == dst->nent) {
        return TE_ERROR_BAD_PARAMS;
    }

    return TE_SUCCESS;
}

int te_memlist_copy( uint8_t *dst,
                     const te_memlist_t *src,
                     size_t ofs,
                     size_t len )
{

    size_t i = 0;
    size_t node_ofs = 0;   /** offset of the node from start */
    size_t pos = 0;
    uint8_t *t = dst;

    if ((NULL == dst) || (NULL == src) || (NULL == src->ents)) {
        return TE_ERROR_BAD_PARAMS;
    }

    for (i = 0; i < src->nent; i++) {
         /* seek to 'offset' */
        if (t == dst) {
            if ((ofs - pos) > src->ents[i].len) {
                pos += src->ents[i].len;
                continue;
            } else {
                node_ofs = ofs - pos;
            }
        } else {
            node_ofs = 0;
        }
        /* copy \p len of data to \p dst from offset \p ofs */
        pos += src->ents[i].len;
        if ((pos - ofs) >= len) {
            osal_memcpy(t, (uint8_t *)((uintptr_t)src->ents[i].buf + node_ofs),
                        len - (t - dst));
            break; /* reach the end */
        } else {
            osal_memcpy(t, (uint8_t *)((uintptr_t)src->ents[i].buf + node_ofs),
                        pos - ofs - (t - dst));
            t += (pos - ofs - (t - dst));
        }
    }

    if (i == src->nent) {
        return TE_ERROR_BAD_PARAMS;
    }
    return TE_SUCCESS;
}
