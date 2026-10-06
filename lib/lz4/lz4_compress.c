// SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause
/* LZ4 v1.10.0, with the existing Linux work-memory API preserved. */
#include "lz4_kernel.h"
#include "upstream/lz4.c"
#include <linux/module.h>

static_assert(sizeof(LZ4v110_stream_t) <= LZ4_MEM_COMPRESS);
static_assert(__alignof__(LZ4v110_stream_t) <= __alignof__(LZ4_stream_t));

int LZ4_compress_fast(const char *src, char *dst, int src_size,
                      int dst_capacity, int acceleration, void *wrkmem)
{
    return LZ4v110_compress_fast_extState(wrkmem, src, dst, src_size,
                                         dst_capacity, acceleration);
}
EXPORT_SYMBOL(LZ4_compress_fast);

int LZ4_compress_default(const char *src, char *dst, int src_size,
                         int dst_capacity, void *wrkmem)
{
    return LZ4_compress_fast(src, dst, src_size, dst_capacity, 1, wrkmem);
}
EXPORT_SYMBOL(LZ4_compress_default);

int LZ4_compress_destSize(const char *src, char *dst, int *src_size,
                         int dst_capacity, void *wrkmem)
{
    return LZ4v110_compress_destSize_extState_internal(wrkmem, src, dst,
                                                     src_size, dst_capacity, 1);
}
EXPORT_SYMBOL(LZ4_compress_destSize);

void LZ4_resetStream(LZ4_stream_t *stream)
{
    LZ4v110_resetStream((LZ4v110_stream_t *)stream);
}

int LZ4_loadDict(LZ4_stream_t *stream, const char *dictionary, int dict_size)
{
    return LZ4v110_loadDict((LZ4v110_stream_t *)stream, dictionary, dict_size);
}
EXPORT_SYMBOL(LZ4_loadDict);

int LZ4_saveDict(LZ4_stream_t *stream, char *buffer, int dict_size)
{
    return LZ4v110_saveDict((LZ4v110_stream_t *)stream, buffer, dict_size);
}
EXPORT_SYMBOL(LZ4_saveDict);

int LZ4_compress_fast_continue(LZ4_stream_t *stream, const char *src, char *dst,
                               int src_size, int dst_capacity, int acceleration)
{
    return LZ4v110_compress_fast_continue((LZ4v110_stream_t *)stream, src, dst,
                                         src_size, dst_capacity, acceleration);
}
EXPORT_SYMBOL(LZ4_compress_fast_continue);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("LZ4 v1.10.0 compressor");
