/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/* Private environment for the LZ4 v1.10.0 implementation. */
#ifndef LZ4_KERNEL_H
#define LZ4_KERNEL_H
#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/build_bug.h>
#include <linux/lz4.h>

#define LZ4v110_FREESTANDING 1
#define LZ4v110_memcpy(dst, src, n) memcpy(dst, src, n)
#define LZ4v110_memmove(dst, src, n) memmove(dst, src, n)
#define LZ4v110_memset(dst, value, n) memset(dst, value, n)
#define LZ4v110_DISABLE_DEPRECATE_WARNINGS

#ifdef LZ4_KERNEL_HC
#include <linux/slab.h>
#define LZ4_KERNEL_ALLOC(n) kmalloc((n), GFP_NOWAIT | __GFP_NOWARN)
#define LZ4_KERNEL_ALLOC_ZERO(n) kzalloc((n), GFP_NOWAIT | __GFP_NOWARN)
#define LZ4_KERNEL_FREE(p) kfree(p)
#else
/* Public allocation APIs are excluded in freestanding mode. */
#define LZ4_KERNEL_ALLOC(n) NULL
#define LZ4_KERNEL_ALLOC_ZERO(n) NULL
#define LZ4_KERNEL_FREE(p) do { } while (0)
#endif
#endif
