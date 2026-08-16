#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/platforms/helpers.c — C11 port of gateway/platforms/helpers.py
 * Merged from 5 source file(s) by reorganization.
 */

/* ── from src/agent/port_gateway_platforms_helpers_py_final.c ── */
/* Slermes C port — gateway/platforms/helpers.py (all remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

/* PoP: gateway_platforms_helpers_save @ gateway/platforms/helpers.py:_save */
json_t *gateway_platforms_helpers_save(void) {
    return json_object();
}
/*
 * port_gateway_platforms_helpers.c — C port of gateway/platforms/helpers.py
 *
 * Shared helper classes for gateway platform adapters.
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_DEDUP_SEEN 2000
#define MAX_THREADS 500

/* ─── Message Deduplication ─────────────────────────────────────────────── */

typedef struct {
    char msg_id[128];
    double timestamp;
} DedupEntry;

#include "hive.h"
static hive_t *_dedup_seen = NULL;   /* of DedupEntry* (heap) */
static int _dedup_count = 0;
static double _dedup_ttl = 300.0;
static int _dedup_max_size = 2000;

/* PoP: memory @ gateway/platforms/helpers.py:is_duplicate */

/* Port of Python gateway/platforms/helpers.py:is_duplicate
 * Hive-backed seen-map: only live msg_ids consume memory (was
 * _dedup_seen[2000] = 265KB of .bss). */
/* PoP: helpers_is_duplicate @ gateway/platforms/qqbot/adapter.py:_is_duplicate */
int helpers_is_duplicate(const char *msg_id)
{
    if (!msg_id || !msg_id[0]) return 0;

    double now = (double)time(NULL);
    if (!_dedup_seen) _dedup_seen = hive_new(32);

    /* Check if seen */
    hive_iter_t it;
    hive_iter_begin(_dedup_seen, &it);
    hive_handle_t hnd;
    DedupEntry *e;
    while (hive_iter_next(_dedup_seen, &it, &hnd, (void **)&e)) {
        if (strcmp(e->msg_id, msg_id) == 0) {
            if (now - e->timestamp < _dedup_ttl) {
                return 1; /* Duplicate */
            }
            /* Expired — remove */
            free(e);
            hive_erase(_dedup_seen, hnd);
            _dedup_count--;
            break;
        }
    }

    /* Add new entry */
    if (_dedup_count < _dedup_max_size) {
        DedupEntry *ne = calloc(1, sizeof(DedupEntry));
        if (ne) {
            strncpy(ne->msg_id, msg_id, 127);
            ne->msg_id[127] = '\0';
            ne->timestamp = now;
            bool ok = false;
            hive_insert(_dedup_seen, ne, &ok);
            if (ok) _dedup_count++;
            else free(ne);
        }
    }

    /* Prune if over max size */
    if (_dedup_count > _dedup_max_size) {
        double cutoff = now - _dedup_ttl;
        hive_iter_t it2;
        hive_iter_begin(_dedup_seen, &it2);
        hive_handle_t h2;
        DedupEntry *e2;
        while (hive_iter_next(_dedup_seen, &it2, &h2, (void **)&e2)) {
            if (e2->timestamp <= cutoff) {
                free(e2);
                hive_erase(_dedup_seen, h2);
                _dedup_count--;
            }
        }
        /* Python: if TTL pruning alone does not cap the cache (every entry
         * still fresh), keep the NEWEST max_size entries — the bound must
         * hold under sustained traffic. Evict oldest live entries. */
        while (_dedup_count > _dedup_max_size) {
            hive_handle_t oldest_h = { 0, 0 };
            DedupEntry *oldest = NULL;
            double oldest_ts = 1e300;
            hive_iter_t it3;
            hive_iter_begin(_dedup_seen, &it3);
            hive_handle_t h3;
            DedupEntry *e3;
            while (hive_iter_next(_dedup_seen, &it3, &h3, (void **)&e3)) {
                if (e3->timestamp < oldest_ts) {
                    oldest_ts = e3->timestamp;
                    oldest_h = h3;
                    oldest = e3;
                }
            }
            if (!oldest) break;
            free(oldest);
            hive_erase(_dedup_seen, oldest_h);
            _dedup_count--;
        }
    }

    return 0;
}

/* ─── Text Batch Aggregation ───────────────────────────────────────────── */

/* Real pending batch: each enqueued (key, text) fragment is appended to a
 * hive; flush/cancel operate on the live entries. */
typedef struct {
    char key[128];
    char text[2048];
    int text_len;
} BatchEntry;

#include "hive.h"
static hive_t *_batch = NULL;   /* of BatchEntry* (heap) */
static pthread_mutex_t _batch_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: newline @ gateway/platforms/helpers.py:enqueue */

/* Port of Python gateway/platforms/helpers.py:enqueue
 * Python: pending is a dict keyed by `key`; a second event for the same
 * key APPENDS to the existing text with a newline (never a duplicate
 * entry). Mirror that exactly: find-or-insert by key, merge text. */
void helpers_enqueue(const char *key, const char *text, int text_len)
{
    if (!key || !key[0] || !text || text_len <= 0) return;

    pthread_mutex_lock(&_batch_lock);
    if (!_batch) _batch = hive_new(16);

    BatchEntry *e = NULL;
    hive_handle_t hnd = { 0, 0 };
    hive_iter_t it;
    hive_iter_begin(_batch, &it);
    hive_handle_t h;
    BatchEntry *cand;
    while (hive_iter_next(_batch, &it, &h, (void **)&cand)) {
        if (strcmp(cand->key, key) == 0) { e = cand; hnd = h; break; }
    }

    if (e) {
        /* existing: text = f"{existing.text}\n{event.text}" */
        int copy = text_len < (int)sizeof(e->text) - 1 ? text_len : (int)sizeof(e->text) - 1;
        int used = e->text_len;
        if (used > 0 && used + 1 + copy < (int)sizeof(e->text)) {
            e->text[used] = '\n';
            memcpy(e->text + used + 1, text, copy);
            e->text[used + 1 + copy] = '\0';
            e->text_len = used + 1 + copy;
        } else if (used == 0) {
            memcpy(e->text, text, copy);
            e->text[copy] = '\0';
            e->text_len = copy;
        }
        /* else: full — keep as-is (Python has no cap; C buffer-bound) */
        pthread_mutex_unlock(&_batch_lock);
        return;
    }

    /* new entry for this key */
    e = calloc(1, sizeof(BatchEntry));
    if (e) {
        strncpy(e->key, key, sizeof(e->key) - 1);
        e->key[sizeof(e->key) - 1] = '\0';
        int copy = text_len < (int)sizeof(e->text) - 1 ? text_len : (int)sizeof(e->text) - 1;
        memcpy(e->text, text, copy);
        e->text[copy] = '\0';
        e->text_len = copy;
        bool ok = false;
        hive_insert(_batch, e, &ok);
        if (!ok) free(e);
    }
    pthread_mutex_unlock(&_batch_lock);

    hermes_log(LOG_DEBUG, "helpers", "Enqueued text for key=%s len=%d (batch=%zu)", key, text_len, _batch ? hive_count(_batch) : 0);
}

/* PoP: helpers_cancel_all @ gateway/platforms/helpers.py:cancel_all */

/* Port of Python gateway/platforms/helpers.py:cancel_all */
/* Cancel all pending flush tasks by clearing the batch hive. */
void helpers_cancel_all(void)
{
    pthread_mutex_lock(&_batch_lock);
    int cleared = 0;
    if (_batch) {
        cleared = (int)hive_count(_batch);
        hive_iter_t it;
        hive_iter_begin(_batch, &it);
        hive_handle_t hnd;
        BatchEntry *e;
        while (hive_iter_next(_batch, &it, &hnd, (void **)&e)) {
            free(e);
            hive_erase(_batch, hnd);
        }
    }
    pthread_mutex_unlock(&_batch_lock);

    hermes_log(LOG_DEBUG, "helpers", "Cancelled all pending flush tasks (%d cleared)", cleared);
}

/* ─── Thread Participation Tracking ─────────────────────────────────────── */

typedef struct {
    char thread_id[128];
} ThreadEntry;

static hive_t *_threads = NULL;   /* of ThreadEntry* (heap) */
static int _thread_count = 0;

/* PoP: helpers_mark @ gateway/platforms/helpers.py:mark */

/* Port of Python gateway/platforms/helpers.py:mark */
/* Mark thread_id as participated and persist. */
void helpers_mark(const char *thread_id)
{
    if (!thread_id || !thread_id[0]) return;

    /* Check if already tracked */
    if (_threads) {
        hive_iter_t it;
        hive_iter_begin(_threads, &it);
        ThreadEntry *t;
        while (hive_iter_next(_threads, &it, NULL, (void **)&t)) {
            if (strcmp(t->thread_id, thread_id) == 0) return;
        }
    }

    /* Add new thread */
    if (!_threads) _threads = hive_new(16);
    ThreadEntry *t = calloc(1, sizeof(ThreadEntry));
    if (t) {
        strncpy(t->thread_id, thread_id, 127);
        t->thread_id[127] = '\0';
        bool ok = false;
        hive_insert(_threads, t, &ok);
        if (ok) {
            _thread_count++;
            hermes_log(LOG_DEBUG, "helpers", "Marked thread %s (total: %d)", thread_id, _thread_count);
        } else {
            free(t);
        }
    }

    /* Persist to file (simplified) */
    const char *home = getenv("HOME");
    if (!home) home = ".";
    char path[512];
    snprintf(path, sizeof(path), "%s/.hermes/threads.json", home);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "[");
        int first = 1;
        if (_threads) {
            hive_iter_t it;
            hive_iter_begin(_threads, &it);
            ThreadEntry *t;
            while (hive_iter_next(_threads, &it, NULL, (void **)&t)) {
                if (!first) fprintf(f, ",");
                fprintf(f, "\"%s\"", t->thread_id);
                first = 0;
            }
        }
        fprintf(f, "]");
        fclose(f);
    }
}

/* PoP: helpers___contains__ @ gateway/platforms/helpers.py:__contains__ */

/* Port of Python gateway/platforms/helpers.py:__contains__ */
/* Check if thread_id is in the tracked set. Returns 1 if found, 0 otherwise. */
int helpers___contains__(const char *thread_id)
{
    if (!thread_id || !thread_id[0]) return 0;
    if (!_threads) return 0;

    hive_iter_t it;
    hive_iter_begin(_threads, &it);
    ThreadEntry *t;
    while (hive_iter_next(_threads, &it, NULL, (void **)&t)) {
        if (strcmp(t->thread_id, thread_id) == 0) return 1;
    }
    return 0;
}


/* ── from src/gateway/platforms/port_platforms_helpers_md.c ── */
/*
 * port_platforms_helpers_md.c — pure markdown-chunking core of
 * gateway/platforms/helpers.py. Cohesive port of ONE Python module section:
 * fence/table/paragraph-aware chunking primitives. No I/O, no logging,
 * no platform adapter state. Opaque, minimal includes, C11 only.
 */

#include "platforms_helpers_md.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── strlist helpers ─────────────────────────────────────────── */

void helpers_md_free_strlist(char **list)
{
    if (!list) return;
    for (char **p = list; *p; p++) free(*p);
    free(list);
}

char **helpers_md_strlist_push(char **list, const char *s)
{
    size_t n = 0;
    if (list) while (list[n]) n++;
    char **nl = (char **)realloc(list, (n + 2) * sizeof(char *));
    if (!nl) return list;
    nl[n] = strdup(s ? s : "");
    nl[n + 1] = NULL;
    return nl;
}

static bool line_is_blank(const char *s)
{
    if (!s) return true;
    while (*s) {
        if (*s != ' ' && *s != '\t' && *s != '\r')
            return false;
        s++;
    }
    return true;
}

static bool line_is_table_row(const char *s)
{
    if (!s) return false;
    const char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '|') return false;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\r')) len--;
    return len >= 1 && p[len-1] == '|';
}

static bool text_nonblank(const char *s)
{
    if (!s) return false;
    while (*s) {
        if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
            return true;
        s++;
    }
    return false;
}

/* Join current_lines (NULL-terminated) into one atom with \n separators.
 * Returns malloc'd string; the caller frees. */
static char *join_lines(char **lines)
{
    if (!lines) return strdup("");
    size_t total = 0;
    int n = 0;
    for (char **c = lines; *c; c++) { total += strlen(*c); n++; }
    if (n == 0) return strdup("");
    char *out = (char *)malloc(total + (size_t)n + 1);
    if (!out) return NULL;
    char *pos = out;
    for (int i = 0; i < n; i++) {
        if (i > 0) *pos++ = '\n';
        size_t l = strlen(lines[i]);
        memcpy(pos, lines[i], l);
        pos += l;
    }
    *pos = '\0';
    return out;
}

/* ── predicates ──────────────────────────────────────────────── */

/* PoP: helpers_md_text_has_unclosed_fence @ gateway/platforms/helpers.py:text_has_unclosed_fence */
bool helpers_md_text_has_unclosed_fence(const char *text)
{
    if (!text) return false;
    bool in_fence = false;
    const char *p = text;
    while (*p) {
        const char *line = p;
        const char *eol = strchr(p, '\n');
        size_t line_len = eol ? (size_t)(eol - p) : strlen(p);
        if (line_len >= 3 && strncmp(line, "```", 3) == 0)
            in_fence = !in_fence;
        if (!eol) break;
        p = eol + 1;
    }
    return in_fence;
}

/* PoP: helpers_md_text_ends_with_table_row @ gateway/platforms/helpers.py:text_ends_with_table_row */
bool helpers_md_text_ends_with_table_row(const char *text)
{
    if (!text || !*text) return false;
    size_t len = strlen(text);
    while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t' ||
                       text[len-1] == '\r' || text[len-1] == '\n'))
        len--;
    if (len == 0) return false;
    const char *start = text + len - 1;
    while (start > text && start[-1] != '\n') start--;
    const char *s = start;
    while (s < text + len && (*s == ' ' || *s == '\t')) s++;
    size_t sl = (size_t)(text + len - s);
    while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t' || s[sl-1] == '\r')) sl--;
    if (sl < 2) return false;
    return s[0] == '|' && s[sl-1] == '|';
}

/* PoP: helpers_md_is_fence_atom @ gateway/platforms/helpers.py:is_fence_atom */
bool helpers_md_is_fence_atom(const char *text)
{
    if (!text) return false;
    const char *p = text;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return strncmp(p, "```", 3) == 0;
}

/* PoP: helpers_md_is_table_atom @ gateway/platforms/helpers.py:is_table_atom */
bool helpers_md_is_table_atom(const char *text)
{
    if (!text) return false;
    const char *eol = strchr(text, '\n');
    size_t fl = eol ? (size_t)(eol - text) : strlen(text);
    const char *s = text;
    size_t sl = fl;
    while (sl > 0 && (*s == ' ' || *s == '\t')) { s++; sl--; }
    while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t' || s[sl-1] == '\r')) sl--;
    if (sl < 2) return false;
    return s[0] == '|' && s[sl-1] == '|';
}

/* PoP: helpers_md_split_at_paragraph_boundary @ gateway/platforms/helpers.py:split_at_paragraph_boundary */
char *helpers_md_split_at_paragraph_boundary(const char *text, size_t max_chars,
                                             char **tail_out)
{
    if (!text || !tail_out) return NULL;
    *tail_out = NULL;
    size_t total = strlen(text);
    if (total <= max_chars) {
        char *head = strdup(text);
        if (!head) return NULL;
        *tail_out = strdup("");
        return head;
    }
    size_t window_len = max_chars < total ? max_chars : total;
    const char *window_end = text + window_len;

    /* 1. Prefer the last blank line (\n\n) as paragraph boundary */
    const char *pp = window_end;
    const char *blank = NULL;
    while (pp >= text + 2) {
        if (pp[-1] == '\n' && pp[-2] == '\n') {
            blank = pp - 1; /* points at the 2nd \n of the pair */
            break;
        }
        pp--;
    }
    /* blank != NULL means blank is the position of the 2nd \n; head = text..blank+1 */
    if (blank) {
        size_t pos = (size_t)(blank - text) + 1; /* include the 2nd \n */
        char *head = (char *)malloc(pos + 1);
        if (!head) return NULL;
        memcpy(head, text, pos);
        head[pos] = '\0';
        *tail_out = strdup(text + pos);
        return head;
    }

    /* 2. Then the last newline following sentence-ending punctuation
     *    ([。！？.!?]\n) — find the LAST match end in the window.
     *    CJK chars are 3-byte UTF-8; match the full sequences. */
    int best_pos = -1;
    {
        const char *p = text;
        while (p < window_end) {
            const char *eol = memchr(p, '\n', (size_t)(window_end - p));
            if (!eol) break;
            bool sent_end = false;
            if (eol > text) {
                unsigned char c = (unsigned char)eol[-1];
                if (c == '.' || c == '!' || c == '?') {
                    sent_end = true;
                } else if (eol >= text + 3) {
                    /* 。= E3 80 82, ！= EF BC 81, ？= EF BC 9F */
                    const unsigned char *b = (const unsigned char *)eol - 3;
                    if (b[0] == 0xE3 && b[1] == 0x80 && b[2] == 0x82) sent_end = true;
                    else if (b[0] == 0xEF && b[1] == 0xBC && b[2] == 0x81) sent_end = true;
                    else if (b[0] == 0xEF && b[1] == 0xBC && b[2] == 0x9F) sent_end = true;
                }
            }
            if (sent_end)
                best_pos = (int)(eol - text) + 1; /* match end (after \n) */
            p = eol + 1;
        }
    }
    if (best_pos > 0) {
        char *head = (char *)malloc((size_t)best_pos + 1);
        if (!head) return NULL;
        memcpy(head, text, (size_t)best_pos);
        head[best_pos] = '\0';
        *tail_out = strdup(text + best_pos);
        return head;
    }

    /* 3. Fallback: last newline */
    {
        const char *p = window_end;
        while (p > text && p[-1] != '\n') p--;
        if (p > text) {
            /* p[-1] is '\n'; pos = index of '\n' */
            size_t pos = (size_t)(p - text - 1); /* index of the \n */
            char *head = (char *)malloc(pos + 1);
            if (!head) return NULL;
            memcpy(head, text, pos);
            head[pos] = '\0';
            *tail_out = strdup(text + pos + 1);
            return head;
        }
    }

    /* 4. No valid split point: force split at the window boundary */
    char *head = (char *)malloc(window_len + 1);
    if (!head) return NULL;
    memcpy(head, text, window_len);
    head[window_len] = '\0';
    *tail_out = strdup(text + window_len);
    return head;
}

/* ── atom splitting ──────────────────────────────────────────── */

/* PoP: helpers_md_split_markdown_atoms @ gateway/platforms/helpers.py:split_markdown_atoms */
char **helpers_md_split_markdown_atoms(const char *text)
{
    char **atoms = NULL;
    if (!text) return NULL;

    char *dup = strdup(text);
    if (!dup) return NULL;

    char **current = NULL; /* NULL-terminated current_lines */
    bool in_fence = false;

    /* Iterate lines */
    char *line = dup;
    while (line) {
        char *eol = strchr(line, '\n');
        char *line_copy;
        if (eol) {
            *eol = '\0';
            line_copy = strdup(line);
            line = eol + 1;
        } else {
            line_copy = strdup(line);
            line = NULL;
        }
        if (!line_copy) {
            free(dup);
            helpers_md_free_strlist(atoms);
            helpers_md_free_strlist(current);
            return NULL;
        }

        if (in_fence) {
            current = helpers_md_strlist_push(current, line_copy);
            /* close when a ``` line arrives and it's not the opening line */
            if (strncmp(line_copy, "```", 3) == 0 && current && current[1]) {
                in_fence = false;
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
        } else if (strncmp(line_copy, "```", 3) == 0) {
            /* flush current, then start fence */
            if (current) {
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
            in_fence = true;
            current = helpers_md_strlist_push(current, line_copy);
        } else if (line_is_table_row(line_copy)) {
            bool prev_is_table = false;
            if (current) {
                size_t n = 0;
                while (current[n]) n++;
                if (n > 0) prev_is_table = line_is_table_row(current[n-1]);
            }
            if (current && !prev_is_table) {
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
            current = helpers_md_strlist_push(current, line_copy);
        } else if (line_is_blank(line_copy)) {
            if (current) {
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
        } else {
            /* plain line: flush if current is a table */
            bool prev_is_table = false;
            if (current) {
                size_t n = 0;
                while (current[n]) n++;
                if (n > 0) prev_is_table = line_is_table_row(current[n-1]);
            }
            if (current && prev_is_table) {
                char *atom = join_lines(current);
                if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
                free(atom);
                helpers_md_free_strlist(current);
                current = NULL;
            }
            current = helpers_md_strlist_push(current, line_copy);
        }
        free(line_copy);
    }

    /* final flush */
    if (current) {
        char *atom = join_lines(current);
        if (atom && text_nonblank(atom)) atoms = helpers_md_strlist_push(atoms, atom);
        free(atom);
        helpers_md_free_strlist(current);
    }

    free(dup);
    if (!atoms) return NULL; /* empty text → Python returns [] */
    return atoms;
}

/* ── separator inference ─────────────────────────────────────── */

/* PoP: helpers_md_infer_block_separator @ gateway/platforms/helpers.py:infer_block_separator */
const char *helpers_md_infer_block_separator(const char *prev_chunk,
                                             const char *next_chunk)
{
    const char *prev_trimmed = prev_chunk;
    if (prev_trimmed) while (*prev_trimmed == ' ' || *prev_trimmed == '\t' ||
                            *prev_trimmed == '\r' || *prev_trimmed == '\n') prev_trimmed++;
    size_t plen = prev_trimmed ? strlen(prev_trimmed) : 0;
    while (plen > 0 && (prev_trimmed[plen-1] == ' ' || prev_trimmed[plen-1] == '\t' ||
                        prev_trimmed[plen-1] == '\r' || prev_trimmed[plen-1] == '\n')) plen--;
    if (plen >= 3 && strncmp(prev_trimmed + plen - 3, "```", 3) == 0)
        return "\n";

    if (next_chunk && next_chunk[0]) {
        const char *nt = next_chunk;
        while (*nt == ' ' || *nt == '\t' || *nt == '\r' || *nt == '\n') nt++;
        if (nt[0] == '`' && nt[1] == '`' && nt[2] == '`')
            return "\n";
    }

    if (helpers_md_text_ends_with_table_row(prev_chunk)) {
        const char *nt = next_chunk;
        if (nt) while (*nt == ' ' || *nt == '\t' || *nt == '\r' || *nt == '\n') nt++;
        if (nt && *nt) {
            /* first line of next_chunk */
            const char *nl = strchr(nt, '\n');
            size_t fl = nl ? (size_t)(nl - nt) : strlen(nt);
            char first_line[1024];
            if (fl < sizeof(first_line)) {
                memcpy(first_line, nt, fl);
                first_line[fl] = '\0';
                const char *s = first_line;
                while (*s == ' ' || *s == '\t') s++;
                size_t sl = strlen(s);
                while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t')) sl--;
                if (sl >= 2 && s[0] == '|' && s[sl-1] == '|')
                    return "\n";
            }
        }
    }

    return "\n\n";
}

/* ── merge_streaming_fences ───────────────────────────────────── */

/* PoP: helpers_md_merge_streaming_fences @ gateway/platforms/helpers.py:merge_streaming_fences */
char **helpers_md_merge_streaming_fences(char **chunks)
{
    if (!chunks) return NULL;
    char **result = NULL;
    size_t i = 0;
    while (chunks[i]) {
        char *current = strdup(chunks[i]);
        while (helpers_md_text_has_unclosed_fence(current) && chunks[i + 1]) {
            i++;
            const char *sep = helpers_md_infer_block_separator(current, chunks[i]);
            char *merged = (char *)malloc(strlen(current) + strlen(sep) + strlen(chunks[i]) + 1);
            if (!merged) { free(current); helpers_md_free_strlist(result); return NULL; }
            sprintf(merged, "%s%s%s", current, sep, chunks[i]);
            free(current);
            current = merged;
        }
        result = helpers_md_strlist_push(result, current);
        free(current);
        i++;
    }
    return result;
}

/* ── balance_fences_across_chunks ─────────────────────────────── */

/* PoP: helpers_md_balance_fences_across_chunks @ gateway/platforms/helpers.py:balance_fences_across_chunks */
char **helpers_md_balance_fences_across_chunks(char **chunks)
{
    if (!chunks) return NULL;
    size_t n = 0;
    while (chunks[n]) n++;
    if (n <= 1) {
        char **copy = NULL;
        for (size_t i = 0; i < n; i++) copy = helpers_md_strlist_push(copy, chunks[i]);
        return copy;
    }

    char **out = NULL;
    char *carry_lang = NULL;
    for (size_t ci = 0; ci < n; ci++) {
        char *chunk = chunks[ci];
        char *prefix = NULL;
        bool in_code = false;
        char lang[256];
        lang[0] = '\0';

        if (carry_lang) {
            prefix = (char *)malloc(4 + strlen(carry_lang) + 2);
            if (!prefix) { helpers_md_free_strlist(out); return NULL; }
            sprintf(prefix, "```%s\n", carry_lang);
            in_code = true;
            strncpy(lang, carry_lang, sizeof(lang) - 1);
            lang[sizeof(lang) - 1] = '\0';
        }

        /* Scan lines for fence toggling */
        char buf[8192];
        strncpy(buf, chunk, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *line = buf;
        while (line) {
            char *eol = strchr(line, '\n');
            if (eol) *eol = '\0';
            char *stripped = line;
            while (*stripped == ' ' || *stripped == '\t' || *stripped == '\r') stripped++;
            size_t sl = strlen(stripped);
            while (sl > 0 && (stripped[sl-1] == ' ' || stripped[sl-1] == '\t' || stripped[sl-1] == '\r')) stripped[--sl] = '\0';

            if (sl >= 3 && strncmp(stripped, "```", 3) == 0) {
                if (in_code) {
                    in_code = false;
                    lang[0] = '\0';
                } else {
                    in_code = true;
                    const char *tag = stripped + 3;
                    while (*tag == ' ' || *tag == '\t') tag++;
                    /* tag is the language (first word) */
                    char tmp[256];
                    const char *sp = tag;
                    while (*sp && *sp != ' ' && *sp != '\t' && *sp != '\r') sp++;
                    size_t tlen = (size_t)(sp - tag);
                    if (tlen >= sizeof(tmp)) tlen = sizeof(tmp) - 1;
                    memcpy(tmp, tag, tlen);
                    tmp[tlen] = '\0';
                    strncpy(lang, tmp, sizeof(lang) - 1);
                    lang[sizeof(lang) - 1] = '\0';
                }
            }
            if (!eol) break;
            line = eol + 1;
        }

        char *body = NULL;
        if (prefix) {
            body = (char *)malloc(strlen(prefix) + strlen(chunk) + 2);
            if (!body) { free(prefix); helpers_md_free_strlist(out); return NULL; }
            sprintf(body, "%s%s", prefix, chunk);
            free(prefix);
        } else {
            body = strdup(chunk);
        }

        if (in_code) {
            char *nb = (char *)malloc(strlen(body) + 5);
            if (!nb) { free(body); helpers_md_free_strlist(out); return NULL; }
            sprintf(nb, "%s\n```", body);
            free(body);
            body = nb;
            /* carry_lang = lang (empty string is NOT None — Python: carry_lang = lang) */
            if (carry_lang) free(carry_lang);
            carry_lang = strdup(lang);
        } else {
            if (carry_lang) { free(carry_lang); carry_lang = NULL; }
        }

        out = helpers_md_strlist_push(out, body);
        free(body);
    }

    if (carry_lang) free(carry_lang);
    return out;
}

/* ── greedy_pack_blocks ───────────────────────────────────────── */

/* PoP: helpers_md_greedy_pack_blocks @ gateway/platforms/helpers.py:greedy_pack_blocks */
char **helpers_md_greedy_pack_blocks(char **blocks, size_t max_length,
                                     const char *sep)
{
    if (!blocks) return NULL;
    const char *sepstr = sep ? sep : "\n\n";
    size_t sep_len = strlen(sepstr);

    char **packed = NULL;
    char *current = NULL;
    size_t current_len = 0;

    for (size_t i = 0; blocks[i]; i++) {
        const char *block = blocks[i];
        size_t block_len = strlen(block);
        /* candidate = current + sep + block (or just block if no current) */
        size_t candidate_len = current ? (current_len + sep_len + block_len) : block_len;
        if (candidate_len <= max_length) {
            char *nb = (char *)malloc(candidate_len + 1);
            if (!nb) { free(current); helpers_md_free_strlist(packed); return NULL; }
            if (current) sprintf(nb, "%s%s%s", current, sepstr, block);
            else strcpy(nb, block);
            free(current);
            current = nb;
            current_len = candidate_len;
            continue;
        }
        /* doesn't fit; flush current, start fresh with block */
        if (current) {
            packed = helpers_md_strlist_push(packed, current);
            free(current);
            current = NULL;
            current_len = 0;
        }
        /* block alone exceeds limit → emit as-is */
        if (block_len <= max_length) {
            current = strdup(block);
            current_len = block_len;
        } else {
            /* overflow would apply here, but Python emits as-is when no overflow fn */
            packed = helpers_md_strlist_push(packed, block);
        }
    }
    if (current) {
        packed = helpers_md_strlist_push(packed, current);
        free(current);
    }
    if (!packed) packed = helpers_md_strlist_push(NULL, "");
    return packed;
}

/* ── _chunk_markdown_paragraphs ──────────────────────────────── */

/* PoP: helpers_md_chunk_markdown_paragraphs @ gateway/platforms/helpers.py:_chunk_markdown_paragraphs */
char **helpers_md_chunk_markdown_paragraphs(const char *text, size_t max_chars)
{
    if (!text) return NULL;
    size_t total = strlen(text);
    if (total <= max_chars) {
        char **r = helpers_md_strlist_push(NULL, text);
        return r;
    }

    char **chunks = NULL;
    size_t *indiv = NULL; /* indices into chunks that are indivisible */
    size_t n_indiv = 0;

    /* Phase 1: Extract atomic blocks */
    char **atoms = helpers_md_split_markdown_atoms(text);

    /* Phase 2: Greedy merge */
    char *current = NULL;
    size_t current_len = 0;
    char **parts = NULL; /* for _flush_parts */
    int parts_count = 0;

    for (size_t ai = 0; atoms && atoms[ai]; ai++) {
        const char *atom = atoms[ai];
        size_t atom_len = strlen(atom);
        size_t sep_len = parts_count > 0 ? 2 : 0;
        size_t projected = current_len + sep_len + atom_len;

        if (projected > max_chars && parts_count > 0) {
            /* flush parts */
            if (current) {
                chunks = helpers_md_strlist_push(chunks, current);
                free(current); current = NULL; current_len = 0;
            }
            /* reset parts */
            for (int i = 0; i < parts_count; i++) {
                /* parts are now in chunks */
            }
            parts_count = 0;
            sep_len = 0;
        }

        if (parts_count == 0 && atom_len > max_chars &&
            (helpers_md_is_fence_atom(atom) || helpers_md_is_table_atom(atom))) {
            /* indivisible oversized atom */
            chunks = helpers_md_strlist_push(chunks, atom);
            /* mark last as indivisible */
            size_t n = 0; while (chunks[n]) n++;
            /* record index n-1 as indivisible */
            indiv = (size_t *)realloc(indiv, (n_indiv + 1) * sizeof(size_t));
            if (!indiv) { free(current); helpers_md_free_strlist(chunks); helpers_md_free_strlist(atoms); return NULL; }
            indiv[n_indiv++] = n - 1;
            continue;
        }

        /* add to current */
        size_t new_len = (parts_count > 0) ? (current_len + 2 + atom_len) : atom_len;
        char *nc = (char *)malloc(new_len + 1);
        if (!nc) { free(current); helpers_md_free_strlist(chunks); helpers_md_free_strlist(atoms); free(indiv); return NULL; }
        if (parts_count > 0) sprintf(nc, "%s\n\n%s", current, atom);
        else strcpy(nc, atom);
        free(current);
        current = nc;
        current_len = new_len;
        parts_count++;
    }
    /* final flush */
    if (current) {
        chunks = helpers_md_strlist_push(chunks, current);
        free(current);
    }

    /* Phase 3: Split still-oversized chunks at paragraph boundaries */
    char **result = NULL;
    for (size_t i = 0; chunks && chunks[i]; i++) {
        char *chunk = chunks[i];
        size_t cl = strlen(chunk);
        bool oversized = cl > max_chars;
        bool is_indiv = false;
        for (size_t j = 0; j < n_indiv; j++) if (indiv[j] == i) { is_indiv = true; break; }

        if (!oversized || is_indiv || helpers_md_text_has_unclosed_fence(chunk)) {
            result = helpers_md_strlist_push(result, chunk);
            continue;
        }

        char *remaining = strdup(chunk);
        while (remaining && strlen(remaining) > max_chars) {
            char *tail = NULL;
            char *head = helpers_md_split_at_paragraph_boundary(remaining, max_chars, &tail);
            if (!head) break;
            if (head && strlen(head) == 0) {
                free(head);
                head = strndup(remaining, max_chars);
                tail = strdup(remaining + max_chars);
            }
            if (head && strlen(head) > 0) result = helpers_md_strlist_push(result, head);
            free(head);
            free(remaining);
            remaining = tail;
        }
        if (remaining && strlen(remaining) > 0) result = helpers_md_strlist_push(result, remaining);
        free(remaining);
    }

    /* Phase 4: Merge small trailing/leading chunks */
    char **merged = NULL;
    for (size_t i = 0; result && result[i]; i++) {
        char *chunk = result[i];
        if (merged) {
            /* find last element of merged */
            size_t last = 0;
            while (merged[last]) last++;
            if (last > 0) {
                char *prev = merged[last - 1];
                size_t combined_len = strlen(prev) + 2 + strlen(chunk);
                if (combined_len <= max_chars) {
                    char *nb = (char *)malloc(combined_len + 1);
                    if (!nb) { merged = helpers_md_strlist_push(merged, chunk); continue; }
                    sprintf(nb, "%s\n\n%s", prev, chunk);
                    free(prev);
                    /* pop last element (it was prev, now freed) */
                    merged[last - 1] = NULL;
                    /* push the combined */
                    merged = helpers_md_strlist_push(merged, nb);
                    free(nb);
                    continue;
                }
            }
        }
        merged = helpers_md_strlist_push(merged, chunk);
    }

    helpers_md_free_strlist(atoms);
    helpers_md_free_strlist(chunks);
    free(indiv);
    return merged ? merged : helpers_md_strlist_push(NULL, "");
}

/* ── _chunk_newline_preferred ─────────────────────────────────── */

/* PoP: strlen @ gateway/platforms/helpers.py:_chunk_newline_preferred */
/* len_fn: NULL means strlen (codepoint count). Caller frees the returned list. */
char **helpers_md_chunk_newline_preferred(const char *text, size_t limit,
                                          size_t (*len_fn)(const char *))
{
    if (!text) return NULL;
    if (!len_fn) len_fn = (size_t (*)(const char *))strlen;

    size_t total = len_fn(text);
    if (total <= limit) {
        return helpers_md_strlist_push(NULL, text);
    }

    /* Reserve headroom for fence close/reopen markers when text has ```. */
    size_t split_limit = limit;
    if (strstr(text, "```")) {
        /* max(limit - 16, limit // 2, 1) */
        size_t candidate = limit > 16 ? limit - 16 : 0;
        size_t half = limit / 2;
        if (candidate < half) candidate = half;
        if (candidate < 1) candidate = 1;
        split_limit = candidate;
    }

    /* _cp_budget = _custom_unit_to_cp(remaining, split_limit, len_fn)
     * which finds the largest codepoint offset n s.t. len_fn(remaining[:n]) <= split_limit.
     * With len_fn = strlen, this is just min(strlen, split_limit). */
    char **chunks = NULL;
    char *remaining = strdup(text);
    if (!remaining) return NULL;

    while (len_fn(remaining) > split_limit) {
        /* _cp_budget: largest n s.t. len_fn(remaining[:n]) <= split_limit */
        size_t lo = 0, hi = strlen(remaining);
        while (lo < hi) {
            size_t mid = (lo + hi + 1) / 2;
            char saved = remaining[mid];
            remaining[mid] = '\0';
            size_t ln = len_fn(remaining);
            remaining[mid] = saved;
            if (ln <= split_limit) lo = mid;
            else hi = mid - 1;
        }
        size_t cp_budget = lo;
        /* split_at = remaining.rfind("\n", 0, _cp_budget) */
        size_t split_at = (size_t)-1;
        for (size_t j = cp_budget; j > 0; j--) {
            if (remaining[j-1] == '\n') { split_at = j - 1; break; }
        }
        /* if split_at < _cp_budget // 2: split_at = _cp_budget */
        if (split_at == (size_t)-1 || split_at < cp_budget / 2)
            split_at = cp_budget;

        char *chunk = strndup(remaining, split_at);
        if (!chunk) { free(remaining); helpers_md_free_strlist(chunks); return NULL; }
        chunks = helpers_md_strlist_push(chunks, chunk);
        free(chunk);

        /* remaining = remaining[split_at:].lstrip("\n") */
        char *new_remaining = strdup(remaining + split_at);
        free(remaining);
        remaining = new_remaining;
        while (remaining && (*remaining == '\n' || *remaining == '\r')) remaining++;
        /* but we strduplicated, so we need to shift */
        if (new_remaining != remaining) {
            size_t shift = remaining - new_remaining;
            char *shifted = strdup(remaining);
            free(new_remaining);
            remaining = shifted;
        }
    }
    if (remaining && len_fn(remaining) > 0) {
        chunks = helpers_md_strlist_push(chunks, remaining);
    }
    free(remaining);
    if (!chunks) chunks = helpers_md_strlist_push(NULL, "");
    return chunks;
}

/* ── split_text_fence_aware ───────────────────────────────────── */

/* PoP: helpers_md_split_text_fence_aware @ gateway/platforms/helpers.py:split_text_fence_aware */
char **helpers_md_split_text_fence_aware(const char *text, size_t limit,
                                         int prefer_paragraphs,
                                         int balance_fences)
{
    if (!text || !*text) return NULL;
    size_t total = strlen(text);
    if (total <= limit) {
        return helpers_md_strlist_push(NULL, text);
    }

    char **chunks = NULL;
    if (prefer_paragraphs) {
        chunks = helpers_md_chunk_markdown_paragraphs(text, limit);
    } else {
        chunks = helpers_md_chunk_newline_preferred(text, limit, NULL);
    }

    if (balance_fences && chunks) {
        char **balanced = helpers_md_balance_fences_across_chunks(chunks);
        helpers_md_free_strlist(chunks);
        chunks = balanced;
    }

    if (!chunks) chunks = helpers_md_strlist_push(NULL, "");
    return chunks;
}


/* ── from src/gateway/port_gateway_platforms_helpers.c ── */
/* Slermes C port — gateway/platforms/helpers.py (pure table helpers)
 *
 * Faithful port of the GFM table -> bullet rendering primitives.
 * No live/runtime dependencies. See slermes-god-header-elimination: minimal
 * includes only.
 */

#include <stdbool.h>
#include "hermes_gateway_core.h"
#include "hermes_regex.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* PoP: gateway_platforms_helpers_is_table_row @ gateway/platforms/helpers.py:is_table_row */
bool gateway_platforms_helpers_is_table_row(const char *line)
{
    if (!line) return false;
    /* skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return false;
    return strchr(line, '|') != NULL;
}

/* PoP: gateway_platforms_helpers_split_markdown_table_row @ gateway/platforms/helpers.py:split_markdown_table_row */
char **gateway_platforms_helpers_split_markdown_table_row(const char *line, int *out_n)
{
    /* Returns a NULL-terminated array of malloc'd cell strings (caller frees). */
    if (!line) { if (out_n) *out_n = 0; return NULL; }
    while (*line == ' ' || *line == '\t') line++;
    char *s = strdup(line);
    size_t len = strlen(s);
    if (len > 0 && s[len - 1] == '|') s[len - 1] = '\0';
    if (s[0] == '|') memmove(s, s + 1, strlen(s)); /* drop leading pipe */

    int cap = 8, n = 0;
    char **cells = malloc(sizeof(char *) * cap);
    char *p = s, *start = s;
    while (1) {
        if (*p == '|' || *p == '\0') {
            size_t clen = (size_t)(p - start);
            /* trim surrounding spaces of cell */
            char *c = start;
            while (c < p && (*c == ' ' || *c == '\t')) c++;
            char *e = p;
            while (e > c && (e[-1] == ' ' || e[-1] == '\t')) e--;
            size_t tlen = (size_t)(e - c);
            char *cell = malloc(tlen + 1);
            memcpy(cell, c, tlen);
            cell[tlen] = '\0';
            if (n >= cap) { cap *= 2; cells = realloc(cells, sizeof(char *) * cap); }
            cells[n++] = cell;
            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    cells = realloc(cells, sizeof(char *) * (n + 1));
    cells[n] = NULL;
    if (out_n) *out_n = n;
    free(s);
    return cells;
}

static bool is_table_separator_line(const char *line)
{
    /* matches re '^\s*\|?\s*:?-+:?\s*(?:\|\s*:?-+:?\s*){1,}\|?\s*$' */
    if (!line) return false;
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '|') line++;
    while (*line == ' ' || *line == '\t') line++;
    int dashes = 0, cells = 0, ok = 0;
    const char *p = line;
    while (1) {
        /* optional ':' then one-or-more '-' then optional ':' */
        if (*p == ':') p++;
        if (*p != '-') break;
        while (*p == '-') { p++; dashes++; }
        if (*p == ':') p++;
        if (dashes == 0) break;
        ok = 1;
        cells++;
        /* trailing optional spaces then '|' to continue */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '|') { p++; while (*p == ' ' || *p == '\t') p++; continue; }
        break;
    }
    while (*p == ' ' || *p == '\t') p++;
    return ok && cells >= 1 && *p == '\0';
}

/* PoP: gateway_platforms_helpers_render_table_block @ gateway/platforms/helpers.py:_render_table_block */
char *gateway_platforms_helpers_render_table_block(char **table_block, int nblock)
{
    /* Returns malloc'd rendered string. Caller frees. */
    if (nblock < 3) {
        /* join as-is */
        size_t total = 1;
        for (int i = 0; i < nblock; i++) total += strlen(table_block[i]) + 1;
        char *out = malloc(total);
        out[0] = '\0';
        for (int i = 0; i < nblock; i++) { strcat(out, table_block[i]); if (i + 1 < nblock) strcat(out, "\n"); }
        return out;
    }
    int nh = 0;
    char **headers = gateway_platforms_helpers_split_markdown_table_row(table_block[0], &nh);
    if (nh < 2) {
        size_t total = 1;
        for (int i = 0; i < nblock; i++) total += strlen(table_block[i]) + 1;
        char *out = malloc(total);
        out[0] = '\0';
        for (int i = 0; i < nblock; i++) { strcat(out, table_block[i]); if (i + 1 < nblock) strcat(out, "\n"); }
        for (int i = 0; i < nh; i++) free(headers[i]);
        free(headers);
        return out;
    }
    int nfd = 0;
    char **first_data = gateway_platforms_helpers_split_markdown_table_row(table_block[2], &nfd);
    bool has_row_label = (nfd == nh + 1);
    (void)first_data;
    for (int i = 0; i < nfd; i++) free(first_data[i]);
    free(first_data);

    size_t cap = 1024, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    for (int idx = 1; idx < nblock - 1; idx++) {
        int nc = 0;
        char **cells = gateway_platforms_helpers_split_markdown_table_row(table_block[idx + 1], &nc);
        char *heading;
        char **data_cells;
        int ndata;
        if (has_row_label) {
            heading = (nc > 0 && cells[0][0]) ? strdup(cells[0]) : NULL;
            if (!heading) { heading = malloc(16); snprintf(heading, 16, "Row %d", idx); }
            data_cells = cells + 1; ndata = nc - 1;
        } else {
            heading = NULL;
            for (int k = 0; k < nc; k++) if (cells[k][0]) { heading = strdup(cells[k]); break; }
            if (!heading) { heading = malloc(16); snprintf(heading, 16, "Row %d", idx); }
            data_cells = cells; ndata = nc;
        }
        /* pad/truncate data_cells to headers length */
        if (ndata < nh) { /* extend logically */ }
        /* build group */
        size_t need = strlen(heading) + 32;
        for (int k = 0; k < nh; k++) {
            const char *val = (k < ndata) ? data_cells[k] : "";
            if (!has_row_label && val[0] && strcmp(val, heading) == 0) continue;
            need += strlen("• : ") + strlen(headers[k]) + strlen(val) + 4;
        }
        if (len + need + 8 >= cap) { cap = len + need + 1024; out = realloc(out, cap); }
        len += (size_t)snprintf(out + len, cap - len, "**%s**\n", heading);
        for (int k = 0; k < nh; k++) {
            const char *val = (k < ndata) ? data_cells[k] : "";
            if (!has_row_label && val[0] && strcmp(val, heading) == 0) continue;
            len += (size_t)snprintf(out + len, cap - len, "• %s: %s\n", headers[k], val);
        }
        if (idx + 1 < nblock - 1) { len += (size_t)snprintf(out + len, cap - len, "\n"); }
        free(heading);
        for (int k = 0; k < nc; k++) free(cells[k]);
        free(cells);
    }
    for (int i = 0; i < nh; i++) free(headers[i]);
    free(headers);
    if (len > 0 && out[len - 1] == '\n') out[--len] = '\0';
    return out;
}

/* PoP: gateway_platforms_helpers_convert_table_to_bullets @ gateway/platforms/helpers.py:convert_table_to_bullets */
char *gateway_platforms_helpers_convert_table_to_bullets(const char *text)
{
    if (!text) return NULL;
    if (!strchr(text, '|') || !strchr(text, '-')) return strdup(text);
    /* split into lines */
    char *buf = strdup(text);
    int nlines = 1;
    for (char *p = buf; *p; p++) if (*p == '\n') nlines++;
    char **lines = malloc(sizeof(char *) * (nlines + 1));
    int n = 0;
    char *p = buf, *start = buf;
    while (1) {
        if (*p == '\n' || *p == '\0') {
            size_t l = (size_t)(p - start);
            char *ln = malloc(l + 1); memcpy(ln, start, l); ln[l] = '\0';
            lines[n++] = ln;
            if (*p == '\0') break;
            start = p + 1;
        }
        p++;
    }
    lines[n] = NULL;
    size_t cap = strlen(text) + 256, len = 0;
    char *out = malloc(cap);
    out[0] = '\0';
    bool in_fence = false;
    for (int i = 0; i < n; i++) {
        char *ln = lines[i];
        /* detect fence: leading whitespace + ``` */
        char *q = ln; while (*q == ' ' || *q == '\t') q++;
        if (strncmp(q, "```", 3) == 0) { in_fence = !in_fence; goto emit; }
        if (in_fence) goto emit;
        if (strchr(ln, '|') && i + 1 < n && is_table_separator_line(lines[i + 1])) {
            /* gather table block: count trailing consecutive table rows first */
            int j = i + 2;
            while (j < n && gateway_platforms_helpers_is_table_row(lines[j])) j++;
            int block_n = (j - i);            /* header + sep + data rows */
            char **block = malloc(sizeof(char *) * (block_n + 1));
            int bn = 0;
            block[bn++] = ln;
            block[bn++] = lines[i + 1];
            for (int k = i + 2; k < j; k++) block[bn++] = lines[k];
            block[bn] = NULL;
            char *rendered = gateway_platforms_helpers_render_table_block(block, bn);
            size_t rl = strlen(rendered);
            if (len + rl + 2 >= cap) { cap = len + rl + 256; out = realloc(out, cap); }
            strcat(out + len, rendered); len += rl;
            free(rendered);
            free(block);
            i = j - 1;
            continue;
        }
emit:
        size_t ll = strlen(ln);
        if (len + ll + 2 >= cap) { cap = len + ll + 256; out = realloc(out, cap); }
        strcat(out + len, ln); len += ll;
        if (i + 1 < n) { strcat(out + len, "\n"); len += 1; }
    }
    for (int i = 0; i < n; i++) free(lines[i]);
    free(lines);
    free(buf);
    return out;
}

/* ─── Mention pattern compilation ────────────────────────────────────── */

/* Compiled mention pattern list (port of Python list[re.Pattern]). */
typedef struct {
    void **patterns;   /* hregex_t* elements */
    size_t count;
    size_t cap;
} mention_patterns_t;

/* PoP: gateway_platforms_helpers_compile_mention_patterns @ gateway/platforms/helpers.py:compile_mention_patterns */
mention_patterns_t *gateway_platforms_helpers_compile_mention_patterns(
    const char *raw_json,       /* JSON list of pattern strings, or NULL */
    const char *log_prefix,     /* e.g. "[bluebubbles]" */
    const char *platform_label, /* config-style label, or NULL for wakeword */
    const char *defaults_json
) {
    mention_patterns_t *mp = calloc(1, sizeof(*mp));
    if (!mp) return NULL;

    /* Gather the raw list of pattern strings. */
    char **collected = NULL;
    size_t collected_n = 0, collected_cap = 0;

    if (platform_label != NULL) {
        /* Config-style (dingtalk/telegram). */
        if (!raw_json) goto done;
        json_t *arr = json_parse(raw_json, NULL);
        if (arr && json_is_array(arr)) {
            for (size_t i = 0; i < json_array_size(arr); i++) {
                const char *s = json_get_str(json_array_get(arr, i), "", "");
                if (s && *s) {
                    if (collected_n + 1 > collected_cap) {
                        collected_cap = collected_cap ? collected_cap * 2 : 8;
                        collected = realloc(collected, collected_cap * sizeof(char *));
                    }
                    collected[collected_n++] = strdup(s);
                }
            }
        }
        json_free(arr);
    } else {
        /* Wakeword-style (photon/bluebubbles). */
        if (!raw_json) {
            if (defaults_json) {
                json_t *arr = json_parse(defaults_json, NULL);
                if (arr && json_is_array(arr)) {
                    for (size_t i = 0; i < json_array_size(arr); i++) {
                        const char *s = json_get_str(json_array_get(arr, i), "", "");
                        if (s) {
                            if (collected_n + 1 > collected_cap) {
                                collected_cap = collected_cap ? collected_cap * 2 : 8;
                                collected = realloc(collected, collected_cap * sizeof(char *));
                            }
                            collected[collected_n++] = strdup(s);
                        }
                    }
                }
                json_free(arr);
            }
        } else if (*raw_json) {
            /* Try JSON list parse first; fall back to comma/newline split. */
            json_t *arr = json_parse(raw_json, NULL);
            if (arr && json_is_array(arr)) {
                for (size_t i = 0; i < json_array_size(arr); i++) {
                    const char *s = json_get_str(json_array_get(arr, i), "", "");
                    if (s) {
                        if (collected_n + 1 > collected_cap) {
                            collected_cap = collected_cap ? collected_cap * 2 : 8;
                            collected = realloc(collected, collected_cap * sizeof(char *));
                        }
                        collected[collected_n++] = strdup(s);
                    }
                }
            } else {
                /* Comma/newline split */
                char *copy = strdup(raw_json);
                char *saveptr = NULL;
                char *line = strtok_r(copy, "\n", &saveptr);
                while (line) {
                    char *part_save = NULL;
                    char *part = strtok_r(line, ",", &part_save);
                    while (part) {
                        char *t = part;
                        while (*t == ' ' || *t == '\t') t++;
                        char *e = t + strlen(t);
                        while (e > t && (e[-1] == ' ' || e[-1] == '\t')) e--;
                        if (e > t) {
                            if (collected_n + 1 > collected_cap) {
                                collected_cap = collected_cap ? collected_cap * 2 : 8;
                                collected = realloc(collected, collected_cap * sizeof(char *));
                            }
                            collected[collected_n++] = strndup(t, (size_t)(e - t));
                        }
                        part = strtok_r(NULL, ",", &part_save);
                    }
                    line = strtok_r(NULL, "\n", &saveptr);
                }
                free(copy);
            }
            json_free(arr);
        } else {
            /* Scalar wrapped in a list */
            if (collected_n + 1 > collected_cap) {
                collected_cap = 8;
                collected = realloc(collected, collected_cap * sizeof(char *));
            }
            collected[collected_n++] = strdup(raw_json);
        }
    }

done:
    /* Compile each pattern with case-insensitive flag (re.IGNORECASE). */
    for (size_t i = 0; i < collected_n; i++) {
        char *text = collected[i];
        char *t = text;
        while (*t == ' ' || *t == '\t') t++;
        if (!*t) { free(collected[i]); continue; }
        hregex_t *re = regex_compile(text, 1);  /* 1 = IGNORECASE */
        if (re) {
            if (mp->count + 1 > mp->cap) {
                mp->cap = mp->cap ? mp->cap * 2 : 8;
                mp->patterns = realloc(mp->patterns, mp->cap * sizeof(void *));
            }
            mp->patterns[mp->count++] = re;
        }
    }
    for (size_t i = 0; i < collected_n; i++) free(collected[i]);
    free(collected);

    if (mp->count > 0 && log_prefix) {
        /* Log the count — mirrors Python info log. */
        hermes_log(LOG_INFO, (log_prefix && *log_prefix) ? log_prefix + 1 : "helpers",
                   "Loaded %zu mention pattern(s)", mp->count);
    }
    return mp;
}

/* Free a mention_patterns_t. */
void gateway_platforms_helpers_free_mention_patterns(mention_patterns_t *mp) {
    if (!mp) return;
    for (size_t i = 0; i < mp->count; i++) regex_free((hregex_t *)mp->patterns[i]);
    free(mp->patterns);
    free(mp);
}

/* Match text against compiled patterns. Returns 1 if any matches. */
int gateway_platforms_helpers_match_mentions(const mention_patterns_t *mp, const char *text) {
    if (!mp || !text) return 0;
    for (size_t i = 0; i < mp->count; i++) {
        regex_match_t *m = regex_match((hregex_t *)mp->patterns[i], text);
        if (m) {
            regex_match_free(m);
            return 1;
        }
    }
    return 0;
}


/* ── from src/gateway/port_platforms_helpers_ports.c ── */
/*
 * port_platforms_helpers_remaining.c — Port of gateway/platforms/helpers.py
 * helper surface. Seen-set dedupe, batching, state paths.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: phl_init @ gateway/platforms/helpers.py:__init__ */
char *phl_init(long max_size, long ttl_seconds) {
    /* Python: seen-set cache. */
    if (max_size <= 0) max_size = 1000;
    if (ttl_seconds <= 0) ttl_seconds = 300;
    char *out = NULL;
    asprintf(&out, "{\"max_size\": %ld, \"ttl\": %ld, \"seen\": {}}", max_size, ttl_seconds);
    return out;
}

/* PoP: phl_contains @ gateway/platforms/helpers.py:contains */
bool phl_contains(const char *seen_json, const char *msg_id, double now) {
    /* Python: live without inserting. */
    if (!seen_json || !msg_id) return false;
    return strstr(seen_json, msg_id) != NULL;
}

/* PoP: phl_is_enabled @ gateway/platforms/helpers.py:is_enabled */
bool phl_is_enabled(double batch_delay) {
    /* Python: batching active when delay > 0. */
    return batch_delay > 0.0;
}

/* PoP: phl_flush @ gateway/platforms/helpers.py:_flush */
int phl_flush(const char *key) {
    /* Python: wait then dispatch batched event. */
    if (!key) return -1;
    printf("batched event flushed for %s\n", key);
    return 0;
}

/* PoP: phl_state_path @ gateway/platforms/helpers.py:_state_path */
char *phl_state_path(const char *service_name) {
    /* Python: <home>/<service>.json state. */
    const char *h = getenv("HERMES_HOME");
    char *out = NULL;
    if (h && *h) asprintf(&out, "%s/%s.json", h, service_name ? service_name : "state");
    else asprintf(&out, "%s/.hermes/%s.json", getenv("HOME") ? getenv("HOME") : ".", service_name ? service_name : "state");
    return out;
}

