LZ4 v1.10.0 kernel adaptation
============================

Source
------

The private implementation in ``upstream/`` is derived from the official
LZ4 v1.10.0 tag, commit ebb370ca83af193212df4dcbadcc5d87bc0de2f0:

https://github.com/lz4/lz4/tree/v1.10.0/lib

The original copyright notices and BSD license terms are retained in each
upstream file.  This directory supplies block compression/decompression and
HC support.  The userspace command-line tools, frame/file APIs, threading,
and allocation APIs are not part of the kernel interface.

Linux interface and memory
--------------------------

The public ``include/linux/lz4.h`` is unchanged.  In particular, Linux's
five-argument ``LZ4_compress_default`` continues to use caller-provided work
memory.  The exported symbol set and all existing function prototypes are
preserved.  The implementation's private identifiers are prefixed and its
API declarations are static, so no additional global library ABI is exposed.

The normal compressor continues to fit the original 16416-byte workspace.
The HC compressor retains the original 262192-byte workspace, including for
callers built with the old header.  The upstream HC union reserves 262200
bytes, but its actual internal structure fits 262192 bytes on arm64; the
private reserve is reduced to the existing kernel limit.  Compile-time
assertions check both the internal structure, union size, and alignment.
Compression state remains private library data.  Decode state is explicitly
imported and exported to retain the existing public field order and meaning.

HC optimal-parser adaptation
----------------------------

HC levels 10 through 12 require a temporary optimal-parser array of
approximately 64 KiB, separate from the persistent stream.  This array is
allocated with ``GFP_NOWAIT | __GFP_NOWARN`` and freed before returning; it
is never placed on the kernel stack.  This avoids sleeping and direct reclaim
inside compression callbacks.  No long-lived hidden allocations are added.

If that optional allocation fails, the same v1.10.0 implementation compresses
the block with the allocation-free level-9 hash-chain parser.  The fallback
runs before consuming input or writing compressed output and retains the
same output-capacity and stream-state rules.  Default HC level 9 and normal
LZ4 compression do not allocate this array.

The Linux header continues to accept the historic maximum HC level 16.
As in upstream v1.10.0, values above 12 are clamped to 12; the exact compressed
size and speed at a given HC level can therefore differ from the old kernel
implementation.  The compressed block format remains LZ4.

Other integration changes
-------------------------

Standard-library headers and memory primitives are mapped to their kernel
equivalents.  Public allocating userspace APIs are disabled.  Generic private macros (including MIN/MAX) and library symbols are
namespaced to avoid collisions with kernel headers.  Private helpers
are static, and the HC-only compilation supplies its local compress-bound
helper.  The normal decompressor remains usable when included by the early
boot decompression code with ``STATIC`` defined.

The upstream v1.10.0 decoder's short-offset initialization is retained,
including the zero-offset initialization omitted by the earlier reverted
FAST_DEC_LOOP backport.
