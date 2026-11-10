/*
 * port_first_time.h — pure LRU seen-sender set from
 * gateway/run_inbound_unauthorized.py UnauthorizedOwnerNotifier.first_time.
 *
 * The C function operates on a caller-owned ordered string list (the seen set,
 * front = least recently used). It returns whether the key was new and updates
 * the list exactly like Python's OrderedDict LRU.
 */
#ifndef PORT_FIRST_TIME_H
#define PORT_FIRST_TIME_H

#include <stddef.h>

/* first_time on a caller-owned, NULL-terminated array of char** keys.
 *
 *   keys   pointer to an array of char* (NULL = empty set). The caller owns and
 *          frees each key; this routine may reorder (and shift) entries in
 *          place and may NULL a pointer during eviction.
 *   count  in/out: number of non-NULL keys.
 *   cap    max_seen (>=1). Keys are evicted from the front (LRU) when count>cap.
 *   key    the (platform,user_id) serialized key to test/insert.
 *
 * Returns 1 when key is new and was inserted; 0 when already present (and moved
 * to the most-recently-used end).
 */
int uin_seen_first_time(char **keys, size_t *count, int cap,
                        const char *key);

#endif /* PORT_FIRST_TIME_H */
