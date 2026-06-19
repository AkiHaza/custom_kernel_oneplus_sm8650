/* SPDX-License-Identifier: GPL-2.0 */
/* Private TCP state for the BBRv3 backport. Existing KMI types stay unchanged. */
#ifndef _TCP_BBR3_PRIVATE_H
#define _TCP_BBR3_PRIVATE_H

#include <net/tcp.h>

/* The TCP-owned skb control buffer is opaque to drivers. Keep its public
 * declaration intact, and pack 32-bit, wrap-safe timestamps into its existing
 * slots together with the additional delivery-rate snapshots.
 */
struct tcp_bbr3_skb_cb {
	u8 prefix[offsetof(struct tcp_skb_cb, tx.first_tx_mstamp)];
	u32 first_tx_mstamp;
	u32 in_flight:20,
	    unused:12;
	u32 delivered_mstamp;
	u32 lost;
};

#define TCPCB_IN_FLIGHT_BITS 20
#define TCPCB_IN_FLIGHT_MAX ((1U << TCPCB_IN_FLIGHT_BITS) - 1)
#define TCP_BBR3_SKB_CB(skb) ((struct tcp_bbr3_skb_cb *)((skb)->cb))
#define TCP_BBR3_SCB(scb) ((struct tcp_bbr3_skb_cb *)(scb))

static_assert(sizeof(struct tcp_bbr3_skb_cb) == sizeof(struct tcp_skb_cb));
static_assert(offsetof(struct tcp_bbr3_skb_cb, first_tx_mstamp) ==
	      offsetof(struct tcp_skb_cb, tx.first_tx_mstamp));
static_assert(offsetof(struct tcp_bbr3_skb_cb, delivered_mstamp) ==
	      offsetof(struct tcp_skb_cb, tx.delivered_mstamp));

/* Restore the recent epoch before exposing a sample to existing controllers.
 * The skb stores low bits, while rate_sample retains its absolute u64 clock.
 */
static inline u64 tcp_bbr3_stamp64(u64 now, u32 stamp)
{
	return now - (u32)(now - stamp);
}

/* Only TCP's ACK processing and BBRv3's synthetic samples allocate this
 * extension. Existing congestion controllers receive the unchanged prefix.
 */
struct tcp_bbr3_rate_sample {
	struct rate_sample rs;
	bool is_acking_tlp_retrans_seq;
	bool is_ece;
	u32 tx_in_flight;
	s32 lost;
	u32 prior_lost;
};

#define tcp_bbr3_rs(sample) container_of((sample), struct tcp_bbr3_rate_sample, rs)

/* Reuse previously unused bits without changing struct tcp_sock's type. */
static inline u8 tcp_bbr3_fast_ack_mode(const struct tcp_sock *tp)
{
	return tp->unused & 3;
}

static inline void tcp_bbr3_set_fast_ack_mode(struct tcp_sock *tp, u8 mode)
{
	tp->unused = (tp->unused & ~3U) | (mode & 3);
}

static inline bool tcp_bbr3_tlp_app_limited(const struct tcp_sock *tp)
{
	return tp->unused & 4;
}

static inline void tcp_bbr3_set_tlp_app_limited(struct tcp_sock *tp, bool limited)
{
	tp->unused = (tp->unused & ~4U) | (limited ? 4 : 0);
}

static inline void tcp_bbr3_reset(struct tcp_sock *tp)
{
	tp->unused &= ~7U;
	tp->ecn_flags &= ~TCP_ECN_ECT_PERMANENT;
}

/* Send this implementation-private event only to BBRv3. */
#define TCP_BBR3_TLP_RECOVERY ((enum tcp_ca_event)(CA_EVENT_ECN_IS_CE + 1))

bool tcp_ca_is_bbr3(const struct sock *sk);
int tcp_register_bbr3(struct tcp_congestion_ops *ca);
void tcp_unregister_bbr3(struct tcp_congestion_ops *ca);

struct tcp_plb_sysctl_params {
	u8 sysctl_tcp_plb_enabled;
	u8 sysctl_tcp_plb_idle_rehash_rounds;
	u8 sysctl_tcp_plb_rehash_rounds;
	u8 sysctl_tcp_plb_suspend_rto_sec;
	int sysctl_tcp_plb_cong_thresh;
};

struct tcp_plb_net_context {
	struct tcp_plb_sysctl_params params;
	struct ctl_table_header *sysctl_header;
};

struct tcp_plb_net_context *tcp_get_plb_ctx(struct net *net);

/* tcp_plb.c */

/*
 * Scaling factor for fractions in PLB. For example, tcp_plb_update_state
 * expects cong_ratio which represents fraction of traffic that experienced
 * congestion over a single RTT. In order to avoid floating point operations,
 * this fraction should be mapped to (1 << TCP_PLB_SCALE) and passed in.
 */
#define TCP_PLB_SCALE 8

/* State for PLB (Protective Load Balancing) for a single TCP connection. */
struct tcp_plb_state {
	u8	consec_cong_rounds:5, /* consecutive congested rounds */
		unused:3;
	u32	pause_until; /* jiffies32 when PLB can resume rerouting */
};

static inline void tcp_plb_init(const struct sock *sk,
				struct tcp_plb_state *plb)
{
	plb->consec_cong_rounds = 0;
	plb->pause_until = 0;
}
void tcp_plb_update_state(const struct sock *sk, struct tcp_plb_state *plb,
			  const int cong_ratio);
void tcp_plb_check_rehash(struct sock *sk, struct tcp_plb_state *plb);
void tcp_plb_update_state_upon_rto(struct sock *sk, struct tcp_plb_state *plb);

/* BBR3 congestion control block */
struct bbr3 {
	u32	min_rtt_us;	        /* min RTT in min_rtt_win_sec window */
	u32	min_rtt_stamp;	        /* timestamp of min_rtt_us */
	u32	probe_rtt_done_stamp;   /* end time for BBR_PROBE_RTT mode */
	u32	probe_rtt_min_us;	/* min RTT in probe_rtt_win_ms win */
	u32	probe_rtt_min_stamp;	/* timestamp of probe_rtt_min_us*/
	u32     next_rtt_delivered; /* scb->tx.delivered at end of round */
	u64	cycle_mstamp;	     /* time of this cycle phase start */
	u32     mode:2,		     /* current bbr_mode in state machine */
		prev_ca_state:3,     /* CA state on previous ACK */
		round_start:1,	     /* start of packet-timed tx->ack round? */
		ce_state:1,          /* If most recent data has CE bit set */
		bw_probe_up_rounds:5,   /* cwnd-limited rounds in PROBE_UP */
		try_fast_path:1,	/* can we take fast path? */
		idle_restart:1,	     /* restarting after idle? */
		probe_rtt_round_done:1,  /* a BBR_PROBE_RTT round at 4 pkts? */
		init_cwnd:7,         /* initial cwnd */
		unused_1:10;
	u32	pacing_gain:10,	/* current gain for setting pacing rate */
		cwnd_gain:10,	/* current gain for setting cwnd */
		full_bw_reached:1,   /* reached full bw in Startup? */
		full_bw_cnt:2,	/* number of rounds without large bw gains */
		cycle_idx:2,	/* current index in pacing_gain cycle array */
		has_seen_rtt:1, /* have we seen an RTT sample yet? */
		unused_2:6;
	u32	prior_cwnd;	/* prior cwnd upon entering loss recovery */
	u32	full_bw;	/* recent bw, to estimate if pipe is full */

	/* For tracking ACK aggregation: */
	u64	ack_epoch_mstamp;	/* start of ACK sampling epoch */
	u16	extra_acked[2];		/* max excess data ACKed in epoch */
	u32	ack_epoch_acked:20,	/* packets (S)ACKed in sampling epoch */
		extra_acked_win_rtts:5,	/* age of extra_acked, in round trips */
		extra_acked_win_idx:1,	/* current index in extra_acked array */
	/* BBR v3 state: */
		full_bw_now:1,		/* recently reached full bw plateau? */
		startup_ecn_rounds:2,	/* consecutive hi ECN STARTUP rounds */
		loss_in_cycle:1,	/* packet loss in this cycle? */
		ecn_in_cycle:1,		/* ECN in this cycle? */
		unused_3:1;
	u32	loss_round_delivered; /* scb->tx.delivered ending loss round */
	u32	undo_bw_lo;	     /* bw_lo before latest losses */
	u32	undo_inflight_lo;    /* inflight_lo before latest losses */
	u32	undo_inflight_hi;    /* inflight_hi before latest losses */
	u32	bw_latest;	 /* max delivered bw in last round trip */
	u32	bw_lo;		 /* lower bound on sending bandwidth */
	u32	bw_hi[2];	 /* max recent measured bw sample */
	u32	inflight_latest; /* max delivered data in last round trip */
	u32	inflight_lo;	 /* lower bound of inflight data range */
	u32	inflight_hi;	 /* upper bound of inflight data range */
	u32	bw_probe_up_cnt; /* packets delivered per inflight_hi incr */
	u32	bw_probe_up_acks;  /* packets (S)ACKed since inflight_hi incr */
	u32	probe_wait_us;	 /* PROBE_DOWN until next clock-driven probe */
	u32	prior_rcv_nxt;	/* tp->rcv_nxt when CE state last changed */
	u32	ecn_eligible:1,	/* sender can use ECN (RTT, handshake)? */
		ecn_alpha:9,	/* EWMA delivered_ce/delivered; 0..256 */
		bw_probe_samples:1,    /* rate samples reflect bw probing? */
		prev_probe_too_high:1, /* did last PROBE_UP go too high? */
		stopped_risky_probe:1, /* last PROBE_UP stopped due to risk? */
		rounds_since_probe:8,  /* packet-timed rounds since probed bw */
		loss_round_start:1,    /* loss_round_delivered round trip? */
		loss_in_round:1,       /* loss marked in this round trip? */
		ecn_in_round:1,	       /* ECN marked in this round trip? */
		ack_phase:3,	       /* bbr_ack_phase: meaning of ACKs */
		loss_events_in_round:4,/* losses in STARTUP round */
		initialized:1;	       /* has bbr_init() been called? */
	u32	alpha_last_delivered;	 /* tp->delivered    at alpha update */
	u32	alpha_last_delivered_ce; /* tp->delivered_ce at alpha update */

	u8	unused_4;		/* to preserve alignment */
	struct tcp_plb_state plb;

	/* react to a specific lost skb (optional) */
	void (*skb_marked_lost)(struct sock *sk, const struct sk_buff *skb);
};


#endif /* _TCP_BBR3_PRIVATE_H */
