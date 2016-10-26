#ifndef __TEMPFREQ_NETLINK_H_
#define __TEMPFREQ_NETLINK_H_

#define NETLINK_MPDECISION_COEXIST NETLINK_USERSOCK

struct sk_buff;

extern struct sock *netlink_sk;
void netlink_send(char *msg);
void netlink_recv(struct sk_buff *skb);

#endif	/* __TEMPFREQ_NETLINK_H_ */
