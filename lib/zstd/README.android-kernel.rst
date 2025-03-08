Zstandard 1.5.7 on Android/Linux 6.1
==================================

This library is based on Linux commit
65d1f5507ed2c78c64fce40e44e5574a9419eb09 (Nick Terrell), which imports
the official facebook/zstd ``v1.5.7-kernel`` sources. It also carries the
GCC dynamic-BMI2 cutoff adjustment from Linux commit
1400c87e6cac47eb243f260352c854474d9a9073 (Ingo Molnar).

The Linux-facing API in ``include/linux/zstd.h`` remains unchanged.
Existing exported function names, parameter types, and allocation helpers
are retained. New QAT and dictionary wrapper APIs are not introduced.

Upstream added reserved members to ``ZSTD_FrameHeader``. The older
``ZSTD_frameHeader`` layout remains available for the Linux 6.1 wrapper;
``zstd_get_frame_header()`` uses an internal new header and copies only
the old fields. It clears the complete old object, preserves its short-input
hint, and keeps ``dictID`` and ``headerSize`` zero for skippable frames.
The decompression reset wrapper retains the old next-input-size hint.

New error enum members retain all existing numeric values and the existing
maximum of 120; they do not change the enum representation. Only these new
members are hidden from ``__GENKSYMS__`` to preserve the unchanged Linux 6.1
module type checksums. Normal compilation exposes all Zstandard 1.5.7 codes.
No struct-layout or calling-convention changes are hidden from genksyms.

The source uses Linux 6.1's ``asm/unaligned.h`` and provides its existing
local ``intptr_t`` fallback. ``decompress_sources.h`` continues to support
preboot source aggregation. Host regression tests cover official kernel
use cases, source aggregation, and clients compiled against the old headers.
