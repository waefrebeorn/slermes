#ifndef TOOLS_URL_SAFETY_HELPERS_H
#define TOOLS_URL_SAFETY_HELPERS_H
#include <stdbool.h>
bool tools_url_safety_allows_private_ip_resolution(const char *scheme, const char *hostname);
#endif
