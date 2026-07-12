#ifndef CRON_LIFECYCLE_GUARD_H
#define CRON_LIFECYCLE_GUARD_H

#include <stdbool.h>

bool cron_lifecycle_contains_gateway_lifecycle_command(const char *text);

#endif
