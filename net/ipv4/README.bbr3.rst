BBRv3 for Android 14 / Linux 6.1
================================

Source
------

The algorithm retains the Google MODULE_AUTHOR entries and dual BSD/GPL
license in tcp_bbr3.c. Protective Load Balancing retains its original design
attribution in tcp_plb.c. This port starts from fatalcoder524's Android 14 / 6.1
backport, distributed as::

  WildKernels/kernel_patches
  commit 3a75d651cf999e213c6998a7101eea7c5c6c741e
  common/bbrv3/0001-net-tcp-backport-BBRv3-to-android14-6.1.patch

The patch identifies its original kernel commit as
252cb6d65180a449ba09d52d929cd8cd6bc04b8d. Its SHA-256 is
4d7a1272df8d12d727729903227a91b207b66740291f709594de79fddacdd429.

Android adaptation
------------------

Existing public TCP structure and enum definitions remain unchanged. In
particular, the 104-byte congestion-control private area, rate_sample,
tcp_congestion_ops, tcp_sock and the 20-byte tcp_cc_info union retain their
original layout and type definitions. No new __GENKSYMS__ layout exceptions or
Android KABI reserve slots are used.

BBRv3 keeps its separately allocated state behind a pointer in its existing
private area. If allocation fails, a Reno operations object owned by the same
module takes over, following the tcp_dctcp.c fallback pattern. The socket keeps
its module reference until normal cleanup. Core loss notifications identify
BBRv3 by the registered operations pointer, never by the algorithm name.

TCP ACK processing and BBRv3 synthetic samples allocate an internal extension
whose first member is the unchanged rate_sample. Existing controllers receive
that original prefix. The TCP-owned skb control buffer uses a private overlay
within its original 48 bytes; compile-time checks verify its size and timestamp
offsets. Short timestamps use wrap-safe differences, and the public rate sample
recovers the recent absolute 64-bit timestamp epoch. The existing tcp_sock
64-bit clocks remain unchanged.

Private flags occupy previously unused TCP bits and are reset on congestion
algorithm changes and disconnect. Releasing BBRv3 clears its permanent-ECT flag
without clearing negotiated ECN or per-route ECN eligibility. PLB settings and
sysctls share one per-network-namespace allocation and lifecycle.

Policy and diagnostics
----------------------

The GKI configuration makes BBRv3 available as ``bbr3``. The default congestion
controller remains CUBIC, and the existing TCP ECN default remains 2 (accept
peer-requested ECN; do not request it for outgoing connections). PLB remains
disabled until explicitly configured. This port does not select a new default
qdisc or enable IPv6 NAT.

An administrator can select BBRv3 for new connections through the existing
``net.ipv4.tcp_congestion_control`` sysctl or a per-socket TCP_CONGESTION option.
BBRv3's low-latency ECN response additionally requires negotiated ECN and an
explicit per-route RTAX_FEATURE_ECN_LOW setting for a suitable ECN environment.
It is not forced on arbitrary Internet paths.

BBRv3 reports the existing five-field, 20-byte INET_DIAG_BBRINFO format. Extra
upstream BBRv3 phase and inflight diagnostic fields are deliberately omitted so
that existing kernel consumers of tcp_cc_info retain the same buffer contract.
