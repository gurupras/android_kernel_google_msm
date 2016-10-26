#ifndef __TEMPFREQ_NETLINK_H_
#define __TEMPFREQ_NETLINK_H_

#define NETLINK_MPDECISION_COEXIST NETLINK_USERSOCK

struct sk_buff;

struct netlink_cmd {
	int cmd_len;
	char cmd[16];
	int args_len;
	char args[24];
};

extern struct tempfreq_attr netlink_cmd_test;

extern struct sock *netlink_sk;
void netlink_send(struct netlink_cmd *cmd);
void netlink_recv(struct sk_buff *skb);

#endif	/* __TEMPFREQ_NETLINK_H_ */
