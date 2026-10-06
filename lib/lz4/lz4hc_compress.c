// SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause
/* LZ4 HC v1.10.0, preserving the 262192-byte Linux work-memory contract. */
#define LZ4_KERNEL_HC
#include "lz4_kernel.h"
#include "upstream/lz4hc.c"
#include <linux/module.h>

static_assert(sizeof(LZ4v110_streamHC_t) <= LZ4HC_MEM_COMPRESS);
static_assert(sizeof(LZ4HCv110_CCtx_internal) <= LZ4HC_MEM_COMPRESS);
static_assert(__alignof__(LZ4v110_streamHC_t) <= __alignof__(LZ4_streamHC_t));

int LZ4_compress_HC(const char *src, char *dst, int src_size, int dst_capacity,
                    int compression_level, void *wrkmem)
{
    return LZ4v110_compress_HC_extStateHC(wrkmem, src, dst, src_size,
                                        dst_capacity, compression_level);
}
EXPORT_SYMBOL(LZ4_compress_HC);

void LZ4_resetStreamHC(LZ4_streamHC_t *stream, int compression_level)
{
    LZ4v110_resetStreamHC((LZ4v110_streamHC_t *)stream, compression_level);
}

int LZ4_loadDictHC(LZ4_streamHC_t *stream, const char *dictionary, int dict_size)
{
    return LZ4v110_loadDictHC((LZ4v110_streamHC_t *)stream, dictionary, dict_size);
}
EXPORT_SYMBOL(LZ4_loadDictHC);

int LZ4_compress_HC_continue(LZ4_streamHC_t *stream, const char *src, char *dst,
                             int src_size, int dst_capacity)
{
    return LZ4v110_compress_HC_continue((LZ4v110_streamHC_t *)stream, src, dst,
                                       src_size, dst_capacity);
}
EXPORT_SYMBOL(LZ4_compress_HC_continue);

int LZ4_saveDictHC(LZ4_streamHC_t *stream, char *buffer, int dict_size)
{
    return LZ4v110_saveDictHC((LZ4v110_streamHC_t *)stream, buffer, dict_size);
}
EXPORT_SYMBOL(LZ4_saveDictHC);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("LZ4 HC v1.10.0 compressor");
