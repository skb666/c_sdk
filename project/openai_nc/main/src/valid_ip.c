#include "valid_ip.h"

#include <string.h>

int ipv4_judge(const char *s) {
    int count = 0;
    for (int i = 0; s[i] != '\0';) {
        if (s[i] == '0' && (s[i + 1] >= '0' && s[i + 1] <= '9')) {
            return 0;
        }

        int re = 0;
        while (s[i] >= '0' && s[i] <= '9') {
            re = re * 10 + s[i] - '0';
            i++;
            if (re > 255) {
                return 0;
            }
        }
        if (re > 255) {
            return 0;
        }

        if (s[i] == '\0') {
            break;
        }
        if (s[i] != '.') {
            return 0;
        } else {
            count++;
            i++;
            if (!(s[i] >= '0' && s[i] <= '9')) {
                return 0;
            }
        }
    }
    if (count != 3) {
        return 0;
    }
    return 1;
}

int ipv6_judge(const char *s) {
    int count = 0;
    for (int i = 0; s[i] != '\0';) {
        int index = i;

        while ((s[index] >= '0' && s[index] <= '9') || (s[index] >= 'a' && s[index] <= 'f') || (s[index] >= 'A' && s[index] <= 'F')) {
            index++;
        }
        if (index - i > 4) {
            return 0;
        } else {
            i = index;
        }

        if (s[i] == '\0') {
            break;
        }
        if (s[i] != ':') {
            return 0;
        } else {
            i++;
            count++;
            if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f') || (s[i] >= 'A' && s[i] <= 'F'))) {
                return 0;
            }
        }
    }
    if (count != 7) {
        return 0;
    }

    return 1;
}

IP_TYPE validIPAddress(const char *queryIP) {
    if (strlen(queryIP) < 7) {
        return E_NONE;
    }
    int i = 0;
    int r = 0;
    while (1) {
        if (queryIP[i] == ':') {
            r = 6;
            break;
        }
        if (queryIP[i] == '.') {
            r = 4;
            break;
        }
        i++;
    }
    if (r == 4) {
        if (ipv4_judge(queryIP)) {
            return E_IPV4;
        } else {
            return E_NONE;
        }
    } else {
        if (ipv6_judge(queryIP)) {
            return E_IPV6;
        } else {
            return E_NONE;
        }
    }

    return E_NONE;
}
