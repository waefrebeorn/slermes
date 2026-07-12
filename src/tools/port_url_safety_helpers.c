/* Slermes C port — tools/url_safety.py (pure trusted-host private-IP gate) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Faithful copy of _TRUSTED_PRIVATE_IP_HOSTS (url_safety.py:174). */
static const char *URL_SAFETY_TRUSTED_HOSTS[] = {
    "multimedia.nt.qq.com.cn",
    NULL,
};

/* PoP: _allows_private_ip_resolution @ tools/url_safety.py:_allows_private_ip_resolution */
bool tools_url_safety_allows_private_ip_resolution(const char *hostname, const char *scheme)
{
    if (!scheme || strcmp(scheme, "https") != 0) return false;
    if (!hostname) return false;
    for (int i = 0; URL_SAFETY_TRUSTED_HOSTS[i]; i++)
        if (strcmp(hostname, URL_SAFETY_TRUSTED_HOSTS[i]) == 0) return true;
    return false;
}
