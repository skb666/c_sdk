#ifndef __VALID_IP_H__
#define __VALID_IP_H__

typedef enum {
    E_NONE,
    E_IPV4,
    E_IPV6
} IP_TYPE;

int ipv4_judge(const char *s);
int ipv6_judge(const char *s);
IP_TYPE validIPAddress(const char *queryIP);

#endif
