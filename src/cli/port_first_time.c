/*
 * port_first_time.c — first_time LRU seen-sender set.
 *
 * Python (UnauthorizedOwnerNotifier.first_time):
 *     key = (platform_name, str(user_id))
 *     if key in seen: seen.move_to_end(key); return False
 *     seen[key] = None
 *     while len(seen) > max_seen: seen.popitem(last=False)   # evict LRU (front)
 *     return True
 *
 * We model the OrderedDict as a caller-owned array; "back" is index count-1,
 * "front" is index 0. The key is passed already serialized (platform,user_id).
 */

#include "port_first_time.h"

#include <string.h>

int uin_seen_first_time(char **keys, size_t *count, int cap, const char *key)
{
    size_t n = count ? *count : 0;
    size_t hit = (size_t)-1;
    size_t i;

    if (cap < 1) cap = 1;
    if (!keys) return 1; /* caller gave no storage: treat as always-new */

    for (i = 0; i < n; i++) {
        if (keys[i] && strcmp(keys[i], key) == 0) { hit = i; break; }
    }

    if (hit != (size_t)-1) {
        /* move_to_end: rotate keys[hit+1..n-1] down, place key at n-1 */
        char *k = keys[hit];
        for (i = hit; i + 1 < n; i++) keys[i] = keys[i + 1];
        keys[n - 1] = k;
        if (count) *count = n;
        return 0;
    }

    /* insert at the back */
    keys[n] = (char *)key;
    n++;
    /* evict LRU (front) while over cap */
    {
        size_t shift = 0;
        while (n > (size_t)cap) {
            keys[0] = NULL;
            for (i = 1; i < n; i++) keys[i - 1] = keys[i];
            keys[n - 1] = NULL;
            n--;
            shift++;
        }
        (void)shift;
    }
    if (count) *count = n;
    return 1;
}
