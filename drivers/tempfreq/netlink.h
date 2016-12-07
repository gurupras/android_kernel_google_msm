#ifndef __TEMPFREQ_NETLINK_H_
#define __TEMPFREQ_NETLINK_H_

#define NETLINK_MPDECISION_COEXIST NETLINK_USERSOCK

struct sk_buff;

struct netlink_cmd {
	int cmd_len;
	char cmd[24];
	int args_len;
	char args[36];
};

extern struct tempfreq_attr netlink_cmd_test;

extern struct sock *netlink_sk;
int is_netlink_ready(void);
void netlink_send(struct netlink_cmd *cmd);
void netlink_recv(struct sk_buff *skb);

extern struct tempfreq_attr cs_attr_default;
extern struct tempfreq_attr cs_attr_bg_non_interactive;
extern struct tempfreq_attr cs_attr_fg_bg;
extern struct tempfreq_attr cs_attr_delay_tolerant;

#endif	/* __TEMPFREQ_NETLINK_H_ */
