// SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause
/* LZ4 v1.10.0 decoder with the original Linux public API. */
#include "lz4_kernel.h"
#include "upstream/lz4.c"
#ifndef STATIC
#include <linux/module.h>
#endif

static_assert(sizeof(LZ4v110_streamDecode_t) <= sizeof(LZ4_streamDecode_t));

/* The public decode state retains its original member order and meaning. */
static void lz4_decode_import(LZ4v110_streamDecode_t *dst,
                              const LZ4_streamDecode_t *src)
{
    dst->internal_donotuse.externalDict = src->internal_donotuse.externalDict;
    dst->internal_donotuse.extDictSize = src->internal_donotuse.extDictSize;
    dst->internal_donotuse.prefixEnd = src->internal_donotuse.prefixEnd;
    dst->internal_donotuse.prefixSize = src->internal_donotuse.prefixSize;
}

static void lz4_decode_export(LZ4_streamDecode_t *dst,
                              const LZ4v110_streamDecode_t *src)
{
    dst->internal_donotuse.externalDict = src->internal_donotuse.externalDict;
    dst->internal_donotuse.extDictSize = src->internal_donotuse.extDictSize;
    dst->internal_donotuse.prefixEnd = src->internal_donotuse.prefixEnd;
    dst->internal_donotuse.prefixSize = src->internal_donotuse.prefixSize;
}

int LZ4_decompress_safe(const char *src, char *dst, int compressed_size,
                       int dst_capacity)
{
    return LZ4v110_decompress_safe(src, dst, compressed_size, dst_capacity);
}

int LZ4_decompress_safe_partial(const char *src, char *dst, int compressed_size,
                               int target_size, int dst_capacity)
{
    return LZ4v110_decompress_safe_partial(src, dst, compressed_size,
                                          target_size, dst_capacity);
}

int LZ4_decompress_fast(const char *src, char *dst, int original_size)
{
    return LZ4v110_decompress_fast(src, dst, original_size);
}

int LZ4_setStreamDecode(LZ4_streamDecode_t *stream, const char *dictionary,
                        int dict_size)
{
    LZ4v110_streamDecode_t state;
    int ret = LZ4v110_setStreamDecode(&state, dictionary, dict_size);

    lz4_decode_export(stream, &state);
    return ret;
}

int LZ4_decompress_safe_continue(LZ4_streamDecode_t *stream, const char *src,
                                char *dst, int compressed_size, int dst_capacity)
{
    LZ4v110_streamDecode_t state;
    int ret;

    lz4_decode_import(&state, stream);
    ret = LZ4v110_decompress_safe_continue(&state, src, dst, compressed_size,
                                          dst_capacity);
    lz4_decode_export(stream, &state);
    return ret;
}

int LZ4_decompress_fast_continue(LZ4_streamDecode_t *stream, const char *src,
                                char *dst, int original_size)
{
    LZ4v110_streamDecode_t state;
    int ret;

    lz4_decode_import(&state, stream);
    ret = LZ4v110_decompress_fast_continue(&state, src, dst, original_size);
    lz4_decode_export(stream, &state);
    return ret;
}

int LZ4_decompress_safe_usingDict(const char *src, char *dst, int compressed_size,
                                 int dst_capacity, const char *dictionary,
                                 int dict_size)
{
    return LZ4v110_decompress_safe_usingDict(src, dst, compressed_size,
                                           dst_capacity, dictionary, dict_size);
}

int LZ4_decompress_fast_usingDict(const char *src, char *dst, int original_size,
                                 const char *dictionary, int dict_size)
{
    return LZ4v110_decompress_fast_usingDict(src, dst, original_size,
                                           dictionary, dict_size);
}

#ifndef STATIC
EXPORT_SYMBOL(LZ4_decompress_safe);
EXPORT_SYMBOL(LZ4_decompress_safe_partial);
EXPORT_SYMBOL(LZ4_decompress_fast);
EXPORT_SYMBOL(LZ4_setStreamDecode);
EXPORT_SYMBOL(LZ4_decompress_safe_continue);
EXPORT_SYMBOL(LZ4_decompress_fast_continue);
EXPORT_SYMBOL(LZ4_decompress_safe_usingDict);
EXPORT_SYMBOL(LZ4_decompress_fast_usingDict);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("LZ4 v1.10.0 decompressor");
#endif
