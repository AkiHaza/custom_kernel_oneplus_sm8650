/* Protective Load Balancing (PLB)
 *
 * PLB was designed to reduce link load imbalance across datacenter
 * switches. PLB is a host-based optimization; it leverages congestion
 * signals from the transport layer to randomly change the path of the
 * connection experiencing sustained congestion. PLB prefers to repath
 * after idle periods to minimize packet reordering. It repaths by
 * changing the IPv6 Flow Label on the packets of a connection, which
 * datacenter switches include as part of ECMP/WCMP hashing.
 *
 * PLB is described in detail in:
 *
 *	Mubashir Adnan Qureshi, Yuchung Cheng, Qianwen Yin, Qiaobin Fu,
 *	Gautam Kumar, Masoud Moshref, Junhua Yan, Van Jacobson,
 *	David Wetherall,Abdul Kabbani:
 *	"PLB: Congestion Signals are Simple and Effective for
 *	 Network Load Balancing"
 *	In ACM SIGCOMM 2022, Amsterdam Netherlands.
 *
 */

#include <linux/init.h>
#include <linux/sysctl.h>
#include <net/netns/generic.h>
#include <net/tcp.h>
#include "tcp_bbr3.h"

/* Called once per round-trip to update PLB state for a connection. */
void tcp_plb_update_state(const struct sock *sk, struct tcp_plb_state *plb,
			  const int cong_ratio)
{
	struct net *net = sock_net(sk);
	struct tcp_plb_net_context *ctx;

	ctx = tcp_get_plb_ctx(net);
	if (!ctx)
		return;

	if (!READ_ONCE(ctx->params.sysctl_tcp_plb_enabled))
		return;

	if (cong_ratio >= 0) {
		if (cong_ratio < READ_ONCE(ctx->params.sysctl_tcp_plb_cong_thresh))
			plb->consec_cong_rounds = 0;
		else if (plb->consec_cong_rounds <
			 READ_ONCE(ctx->params.sysctl_tcp_plb_rehash_rounds))
			plb->consec_cong_rounds++;
	}
}
EXPORT_SYMBOL_GPL(tcp_plb_update_state);

/* Check whether recent congestion has been persistent enough to warrant
 * a load balancing decision that switches the connection to another path.
 */
void tcp_plb_check_rehash(struct sock *sk, struct tcp_plb_state *plb)
{
	struct net *net = sock_net(sk);
	struct tcp_plb_net_context *ctx;
	u32 max_suspend;
	bool forced_rehash = false, idle_rehash = false;

	ctx = tcp_get_plb_ctx(net);
	if (!ctx)
		return;

	if (!READ_ONCE(ctx->params.sysctl_tcp_plb_enabled))
		return;

	forced_rehash = plb->consec_cong_rounds >=
			READ_ONCE(ctx->params.sysctl_tcp_plb_rehash_rounds);
	/* If sender goes idle then we check whether to rehash. */
	idle_rehash = READ_ONCE(ctx->params.sysctl_tcp_plb_idle_rehash_rounds) &&
		      !tcp_sk(sk)->packets_out &&
		      plb->consec_cong_rounds >=
		      READ_ONCE(ctx->params.sysctl_tcp_plb_idle_rehash_rounds);

	if (!forced_rehash && !idle_rehash)
		return;

	/* Note that tcp_jiffies32 can wrap; we detect wraps by checking for
	 * cases where the max suspension end is before the actual suspension
	 * end. We clear pause_until to 0 to indicate there is no recent
	 * RTO event that constrains PLB rehashing.
	 */
	max_suspend = 2 * READ_ONCE(ctx->params.sysctl_tcp_plb_suspend_rto_sec) * HZ;
	if (plb->pause_until &&
	    (!before(tcp_jiffies32, plb->pause_until) ||
	     before(tcp_jiffies32 + max_suspend, plb->pause_until)))
		plb->pause_until = 0;

	if (plb->pause_until)
		return;

	sk_rethink_txhash(sk);
	plb->consec_cong_rounds = 0;
}
EXPORT_SYMBOL_GPL(tcp_plb_check_rehash);

/* Upon RTO, disallow load balancing for a while, to avoid having load
 * balancing decisions switch traffic to a black-holed path that was
 * previously avoided with a sk_rethink_txhash() call at RTO time.
 */
void tcp_plb_update_state_upon_rto(struct sock *sk, struct tcp_plb_state *plb)
{
	struct net *net = sock_net(sk);
	struct tcp_plb_net_context *ctx;
	u32 pause;

	ctx = tcp_get_plb_ctx(net);
	if (!ctx)
		return;

	if (!READ_ONCE(ctx->params.sysctl_tcp_plb_enabled))
		return;

	pause = READ_ONCE(ctx->params.sysctl_tcp_plb_suspend_rto_sec) * HZ;
	pause += prandom_u32_max(pause);
	plb->pause_until = tcp_jiffies32 + pause;

	/* Reset PLB state upon RTO, since an RTO causes a sk_rethink_txhash() call
	 * that may switch this connection to a path with completely different
	 * congestion characteristics.
	 */
	plb->consec_cong_rounds = 0;
}
EXPORT_SYMBOL_GPL(tcp_plb_update_state_upon_rto);

static unsigned int tcp_plb_net_id __read_mostly;
static bool tcp_plb_ready __read_mostly;

struct tcp_plb_net_context *tcp_get_plb_ctx(struct net *net)
{
	if (!READ_ONCE(tcp_plb_ready))
		return NULL;
	return net_generic(net, tcp_plb_net_id);
}
EXPORT_SYMBOL_GPL(tcp_get_plb_ctx);

#ifdef CONFIG_SYSCTL
static int tcp_plb_max_rounds = 31;
static int tcp_plb_max_cong_thresh = 256;
static struct ctl_table plb_ctl_table_template[] = {
	{
		.procname       = "tcp_plb_enabled",
		.maxlen         = sizeof(u8),
		.mode           = 0644,
		.proc_handler   = proc_dou8vec_minmax,
		.extra1         = SYSCTL_ZERO,
		.extra2         = SYSCTL_ONE,
	},
	{
		.procname       = "tcp_plb_idle_rehash_rounds",
		.maxlen         = sizeof(u8),
		.mode           = 0644,
		.proc_handler   = proc_dou8vec_minmax,
		.extra2			= &tcp_plb_max_rounds,
	},
	{
		.procname       = "tcp_plb_rehash_rounds",
		.maxlen         = sizeof(u8),
		.mode           = 0644,
		.proc_handler   = proc_dou8vec_minmax,
		.extra2         = &tcp_plb_max_rounds,
	},
	{
		.procname       = "tcp_plb_suspend_rto_sec",
		.maxlen         = sizeof(u8),
		.mode           = 0644,
		.proc_handler   = proc_dou8vec_minmax,
	},
	{
		.procname       = "tcp_plb_cong_thresh",
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1         = SYSCTL_ZERO,
		.extra2         = &tcp_plb_max_cong_thresh,
	},
	{ }
};

#endif

static int __net_init plb_net_init(struct net *net)
{
	struct tcp_plb_net_context *ctx = net_generic(net, tcp_plb_net_id);
#ifdef CONFIG_SYSCTL
	struct ctl_table *table;
#endif

	ctx->params.sysctl_tcp_plb_enabled = 0;
	ctx->params.sysctl_tcp_plb_idle_rehash_rounds = 31;
	ctx->params.sysctl_tcp_plb_rehash_rounds = 31;
	ctx->params.sysctl_tcp_plb_suspend_rto_sec = 0;
	ctx->params.sysctl_tcp_plb_cong_thresh = 256;
	if (!net_eq(net, &init_net)) {
		struct tcp_plb_net_context *init_ctx = net_generic(&init_net, tcp_plb_net_id);

		ctx->params.sysctl_tcp_plb_enabled = READ_ONCE(init_ctx->params.sysctl_tcp_plb_enabled);
		ctx->params.sysctl_tcp_plb_idle_rehash_rounds = READ_ONCE(init_ctx->params.sysctl_tcp_plb_idle_rehash_rounds);
		ctx->params.sysctl_tcp_plb_rehash_rounds = READ_ONCE(init_ctx->params.sysctl_tcp_plb_rehash_rounds);
		ctx->params.sysctl_tcp_plb_suspend_rto_sec = READ_ONCE(init_ctx->params.sysctl_tcp_plb_suspend_rto_sec);
		ctx->params.sysctl_tcp_plb_cong_thresh = READ_ONCE(init_ctx->params.sysctl_tcp_plb_cong_thresh);
	}

#ifdef CONFIG_SYSCTL
	table = kmemdup(plb_ctl_table_template, sizeof(plb_ctl_table_template), GFP_KERNEL);
	if (!table)
		return -ENOMEM;
	table[0].data = &ctx->params.sysctl_tcp_plb_enabled;
	table[1].data = &ctx->params.sysctl_tcp_plb_idle_rehash_rounds;
	table[2].data = &ctx->params.sysctl_tcp_plb_rehash_rounds;
	table[3].data = &ctx->params.sysctl_tcp_plb_suspend_rto_sec;
	table[4].data = &ctx->params.sysctl_tcp_plb_cong_thresh;
	ctx->sysctl_header = register_net_sysctl(net, "net/ipv4", table);
	if (!ctx->sysctl_header) {
		kfree(table);
		return -ENOMEM;
	}
#endif
	return 0;
}

static void __net_exit plb_net_exit(struct net *net)
{
#ifdef CONFIG_SYSCTL
	struct tcp_plb_net_context *ctx = net_generic(net, tcp_plb_net_id);
	struct ctl_table *table = ctx->sysctl_header->ctl_table_arg;

	unregister_net_sysctl_table(ctx->sysctl_header);
	kfree(table);
#endif
}

static struct pernet_operations plb_net_ops = {
	.init = plb_net_init,
	.exit = plb_net_exit,
	.id = &tcp_plb_net_id,
	.size = sizeof(struct tcp_plb_net_context),
};

static int __init tcp_plb_init_net(void)
{
	int err = register_pernet_subsys(&plb_net_ops);

	if (!err)
		WRITE_ONCE(tcp_plb_ready, true);
	return err;
}
subsys_initcall(tcp_plb_init_net);
