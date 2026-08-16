#ifndef GATEWAY_DRAIN_CONTROL_H
#define GATEWAY_DRAIN_CONTROL_H
#include <stdbool.h>
char *gateway_drain_control_write_drain_request(const char *principal, bool suppress_notification, const char *home);
bool gateway_drain_control_clear_drain_request(const char *home);
char *gateway_drain_control_read_drain_request(const char *home);
#endif
