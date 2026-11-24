/*
 * lockowners.c -- real C11 port of persistence/sqlite/_state/lockowners.py.
 *
 * PoP: w22_lockowners_a9d36b_describe_range @ persistence/sqlite/_state/lockowners.py:_describe_range
 * PoP: w22_lockowners_a9d36b_parse_proc_locks @ persistence/sqlite/_state/lockowners.py:parse_proc_locks
 * PoP: w22_lockowners_a9d36b_warning_message @ persistence/sqlite/_state/lockowners.py:log_write_lock_holders
 *
 * stat()/open() of state.db and /proc/locks are the filesystem seam; the
 * byte-range naming and the line parse are pure. dev_t is rebuilt with the
 * Linux glibc packing (kernel exports major:minor as hex).
 */
#include "hermes_json.h"
#include "noc_w22_lockowners_a9d36b.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LK08013_PENDING_BYTE 0x40000000LL

typedef struct {
    unsigned long long dev;
    long long ino;
    const char *sidecar;
} lk08013_inode;

/* glibc gnu_dev_makedev: the encoding Python's os.makedev uses on Linux. */
static unsigned long long lk08013_makedev(long long major, long long minor) {
    unsigned long long ma = (unsigned long long)major;
    unsigned long long mi = (unsigned long long)minor;
    return (mi & 0xffULL) | ((ma & 0xfffULL) << 8) | ((mi & ~0xffULL) << 12) |
           ((ma & ~0xfffULL) << 32);
}

static char *lk08013_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *lk08013_dup_printf(const char *fmt, long long a, long long b) {
    int need = snprintf(NULL, 0, fmt, a, b);
    char *out;
    if (need < 0) return NULL;
    out = (char *)malloc((size_t)need + 1);
    if (!out) return NULL;
    snprintf(out, (size_t)need + 1, fmt, a, b);
    return out;
}

char *w22_lockowners_a9d36b_describe_range(const char *sidecar, long long start,
                                           long long end) {
    const char *sc = sidecar ? sidecar : "";
    if (!strcmp(sc, "-shm")) {
        if (end == start) {
            if (start == 120) return lk08013_dup("WAL write");
            if (start == 121) return lk08013_dup("WAL checkpoint");
            if (start == 122) return lk08013_dup("WAL recover");
            if (start == 128) return lk08013_dup("shm dead-man switch");
            if (start >= 123 && start <= 127) {
                char buf[32];
                snprintf(buf, sizeof(buf), "WAL read slot %lld", start - 123);
                return lk08013_dup(buf);
            }
        }
        return lk08013_dup_printf("shm bytes %lld-%lld", start, end);
    }
    if (start == end) {
        if (start == LK08013_PENDING_BYTE) return lk08013_dup("PENDING");
        if (start == LK08013_PENDING_BYTE + 1) return lk08013_dup("RESERVED");
    }
    if (start == LK08013_PENDING_BYTE + 2) return lk08013_dup("SHARED range");
    return lk08013_dup_printf("db bytes %lld-%lld", start, end);
}

static int lk08013_parse_tokens(char *line, char *tok[], int cap) {
    int n = 0;
    char *p = line;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (n < cap) tok[n] = p;
        n++;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

static int lk08013_int64(const char *s, long long *out) {
    char *end = NULL;
    long long v;
    if (!s || !*s) return 0;
    v = strtoll(s, &end, 10);
    if (!end || *end) return 0;
    *out = v;
    return 1;
}

static int lk08013_hex64(const char *s, long long *out) {
    char *end = NULL;
    long long v;
    if (!s || !*s) return 0;
    v = strtoll(s, &end, 16);
    if (!end || *end) return 0;
    *out = v;
    return 1;
}

json_t *w22_lockowners_a9d36b_parse_proc_locks(const char *text, const json_t *inodes) {
    json_t *out = json_array();
    lk08013_inode *known = NULL;
    size_t nknown = 0, i;
    char *copy, *line, *next;
    if (!out) return NULL;
    if (inodes && inodes->type == JSON_ARRAY) {
        nknown = inodes->c.count;
        known = (lk08013_inode *)calloc(nknown ? nknown : 1, sizeof(*known));
        if (!known) {
            json_free(out);
            return NULL;
        }
        for (i = 0; i < nknown; i++) {
            json_t *row = inodes->c.items[i];
            const char *sc;
            long long major, minor, ino;
            json_t *ino_node;
            if (!row || row->type != JSON_OBJECT) continue;
            major = (long long)json_get_num(row, "major", 0);
            minor = (long long)json_get_num(row, "minor", 0);
            ino_node = json_obj_get(row, "ino");
            if (ino_node && ino_node->type == JSON_STRING) {
                if (!lk08013_int64(ino_node->str_val, &ino)) continue;
            } else if (ino_node && ino_node->type == JSON_NUMBER) {
                ino = (long long)ino_node->num_val;
            } else {
                continue;
            }
            sc = json_get_str(row, "sidecar", "");
            known[i].dev = lk08013_makedev(major, minor);
            known[i].ino = ino;
            known[i].sidecar = sc ? sc : "";
        }
    }
    copy = lk08013_dup(text ? text : "");
    if (!copy) {
        free(known);
        json_free(out);
        return NULL;
    }
    line = copy;
    while (line) {
        char *tok[8];
        int ntok;
        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            next++;
        }
        ntok = lk08013_parse_tokens(line, tok, 8);
        if (ntok >= 8 && strcmp(tok[1], "->") != 0 && !strcmp(tok[3], "WRITE")) {
            const char *ident = tok[5];
            const char *first = strchr(ident, ':');
            const char *second = first ? strchr(first + 1, ':') : NULL;
            long long major, minor, ino, pid, start, end;
            int ok = 0;
            if (first && second && !strchr(second + 1, ':')) {
                char majbuf[32], minbuf[32];
                size_t mlen = (size_t)(first - ident);
                size_t nlen = (size_t)(second - first - 1);
                if (mlen > 0 && mlen < sizeof(majbuf) && nlen > 0 && nlen < sizeof(minbuf)) {
                    memcpy(majbuf, ident, mlen);
                    majbuf[mlen] = '\0';
                    memcpy(minbuf, first + 1, nlen);
                    minbuf[nlen] = '\0';
                    ok = lk08013_hex64(majbuf, &major) && lk08013_hex64(minbuf, &minor) &&
                         lk08013_int64(second + 1, &ino) && lk08013_int64(tok[4], &pid) &&
                         lk08013_int64(tok[6], &start);
                    if (ok) {
                        if (!strcmp(tok[7], "EOF")) {
                            end = start;
                        } else {
                            ok = lk08013_int64(tok[7], &end);
                        }
                    }
                }
            }
            if (ok) {
                unsigned long long dev = lk08013_makedev(major, minor);
                size_t k;
                for (k = 0; k < nknown; k++) {
                    if (known[k].dev == dev && known[k].ino == ino) {
                        char *kind = w22_lockowners_a9d36b_describe_range(
                            known[k].sidecar, start, end);
                        json_t *row = json_array();
                        json_append(row, json_number((double)pid));
                        json_append(row, kind ? json_string(kind) : json_null());
                        json_append(row, json_string(known[k].sidecar));
                        json_append(out, row);
                        free(kind);
                        break;
                    }
                }
            }
        }
        line = next;
    }
    free(copy);
    free(known);
    return out;
}

char *w22_lockowners_a9d36b_warning_message(const char *path_name, double patience_s,
                                            const json_t *holders) {
    const char *no_lock =
        "no write-class lock held at the deadline (the holder released just before it)";
    const char *name = path_name ? path_name : "";
    char *detail = NULL;
    size_t total = 0, i, pos = 0;
    const char *fmt = "state.db write lock unavailable for %.0fs (%s): %s";
    int need;
    char *out;
    if (holders && holders->type == JSON_ARRAY && holders->c.count > 0) {
        for (i = 0; i < holders->c.count; i++) {
            json_t *v = holders->c.items[i];
            const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
            total += strlen(s) + (i ? 3 : 0);
        }
        detail = (char *)malloc(total + 1);
        if (!detail) return NULL;
        for (i = 0; i < holders->c.count; i++) {
            json_t *v = holders->c.items[i];
            const char *s = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
            size_t n = strlen(s);
            if (i) {
                memcpy(detail + pos, " | ", 3);
                pos += 3;
            }
            memcpy(detail + pos, s, n);
            pos += n;
        }
        detail[pos] = '\0';
    }
    need = snprintf(NULL, 0, fmt, patience_s, name, detail ? detail : no_lock);
    if (need < 0) {
        free(detail);
        return NULL;
    }
    out = (char *)malloc((size_t)need + 1);
    if (!out) {
        free(detail);
        return NULL;
    }
    snprintf(out, (size_t)need + 1, fmt, patience_s, name, detail ? detail : no_lock);
    free(detail);
    return out;
}
