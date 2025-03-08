// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (c) Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/zstd.h>

#include "common/zstd_deps.h"

/* Common symbols. zstd_compress must depend on zstd_decompress. */

unsigned int zstd_is_error(size_t code)
{
	return ZSTD_isError(code);
}
EXPORT_SYMBOL(zstd_is_error);

zstd_error_code zstd_get_error_code(size_t code)
{
	return ZSTD_getErrorCode(code);
}
EXPORT_SYMBOL(zstd_get_error_code);

const char *zstd_get_error_name(size_t code)
{
	return ZSTD_getErrorName(code);
}
EXPORT_SYMBOL(zstd_get_error_name);

/* Decompression symbols. */

size_t zstd_dctx_workspace_bound(void)
{
	return ZSTD_estimateDCtxSize();
}
EXPORT_SYMBOL(zstd_dctx_workspace_bound);

zstd_dctx *zstd_init_dctx(void *workspace, size_t workspace_size)
{
	if (workspace == NULL)
		return NULL;
	return ZSTD_initStaticDCtx(workspace, workspace_size);
}
EXPORT_SYMBOL(zstd_init_dctx);

size_t zstd_decompress_dctx(zstd_dctx *dctx, void *dst, size_t dst_capacity,
	const void *src, size_t src_size)
{
	return ZSTD_decompressDCtx(dctx, dst, dst_capacity, src, src_size);
}
EXPORT_SYMBOL(zstd_decompress_dctx);

size_t zstd_dstream_workspace_bound(size_t max_window_size)
{
	return ZSTD_estimateDStreamSize(max_window_size);
}
EXPORT_SYMBOL(zstd_dstream_workspace_bound);

zstd_dstream *zstd_init_dstream(size_t max_window_size, void *workspace,
	size_t workspace_size)
{
	if (workspace == NULL)
		return NULL;
	(void)max_window_size;
	return ZSTD_initStaticDStream(workspace, workspace_size);
}
EXPORT_SYMBOL(zstd_init_dstream);

size_t zstd_reset_dstream(zstd_dstream *dstream)
{
	size_t ret = ZSTD_DCtx_reset(dstream, ZSTD_reset_session_only);
	int format;

	if (ZSTD_isError(ret))
		return ret;
	ret = ZSTD_DCtx_getParameter(dstream, ZSTD_d_format, &format);
	if (ZSTD_isError(ret))
		return ret;
	/* Keep ZSTD_resetDStream()'s next-input-size hint. */
	return ZSTD_FRAMEHEADERSIZE_PREFIX((ZSTD_format_e)format);
}
EXPORT_SYMBOL(zstd_reset_dstream);

size_t zstd_decompress_stream(zstd_dstream *dstream, zstd_out_buffer *output,
	zstd_in_buffer *input)
{
	return ZSTD_decompressStream(dstream, output, input);
}
EXPORT_SYMBOL(zstd_decompress_stream);

size_t zstd_find_frame_compressed_size(const void *src, size_t src_size)
{
	return ZSTD_findFrameCompressedSize(src, src_size);
}
EXPORT_SYMBOL(zstd_find_frame_compressed_size);

size_t zstd_get_frame_header(zstd_frame_header *header, const void *src,
	size_t src_size)
{
	ZSTD_FrameHeader parsed = { 0 };
	size_t ret;

	/* Preserve the old output size, padding, and short-input contract. */
	memset(header, 0, sizeof(*header));
	if (src_size < ZSTD_FRAMEHEADERSIZE_PREFIX(ZSTD_f_zstd1))
		return ZSTD_FRAMEHEADERSIZE_PREFIX(ZSTD_f_zstd1);
	ret = ZSTD_getFrameHeader(&parsed, src, src_size);

	/* The upstream header grew reserved fields after Linux 6.1. */
	header->frameContentSize = parsed.frameContentSize;
	header->windowSize = parsed.windowSize;
	header->blockSizeMax = parsed.blockSizeMax;
	header->frameType = parsed.frameType;
	/* Linux 6.1 reported no dictionary or header size for skipped frames. */
	if (parsed.frameType != ZSTD_skippableFrame) {
		header->headerSize = parsed.headerSize;
		header->dictID = parsed.dictID;
	}
	header->checksumFlag = parsed.checksumFlag;
	return ret;
}
EXPORT_SYMBOL(zstd_get_frame_header);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Zstd Decompressor");
