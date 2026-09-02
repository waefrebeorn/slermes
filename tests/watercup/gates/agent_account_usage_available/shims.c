#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>

/* account_usage_available: mirrors Python AccountUsageSnapshot.available() */

typedef struct {
    int window_count;
    int detail_count;
    char unavailable_reason[256];
} account_usage_snapshot_t;

bool account_usage_available(const account_usage_snapshot_t *snap) {
    if (!snap) return false;
    bool has_data = (snap->window_count > 0 || snap->detail_count > 0);
    bool no_unavailable = !snap->unavailable_reason[0];
    return has_data && no_unavailable;
}