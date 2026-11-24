/*
 * telegram_addressing.c — C11 port of upstream Hermes plugins/platforms/telegram/telegram_addressing.py
 * (gate w26_telegramad_942de7).
 * STATUS: PORTED — behavior verified by tests/oracle_w26_telegramad_942de7.py.
 * PoP: w26_telegramad_942de7_utf16_offsets @ plugins/platforms/telegram/telegram_addressing.py:_utf16_to_str_offsets
 * PoP: w26_telegramad_942de7_entity_handle @ plugins/platforms/telegram/telegram_addressing.py:_entity_handle
 * PoP: w26_telegramad_942de7_leading_handles @ plugins/platforms/telegram/telegram_addressing.py:_leading_handles
 * PoP: w26_telegramad_942de7_leading_bot_handles @ plugins/platforms/telegram/telegram_addressing.py:leading_bot_handles
 * PoP: w26_telegramad_942de7_command_after_address @ plugins/platforms/telegram/telegram_addressing.py:_command_after_address
 * PoP: w26_telegramad_942de7_foreign_command_target @ plugins/platforms/telegram/telegram_addressing.py:foreign_command_target
 * PoP: w26_telegramad_942de7_clean_trigger @ plugins/platforms/telegram/telegram_addressing.py:clean_bot_trigger_text
 *
 * Seams: the ptb message object (and `addressing_sources`, which expands a
 * rich message) is not ported -- its (text, entities) pair arrives as values.
 * Everything else is the shipped decision: only a LEADING run of handles
 * addresses anyone, the UTF-16 entity offsets decide where a handle counts,
 * our handle anywhere still wins, and `/cmd@ourbot args` keeps its separator.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_telegramad_942de7.h"

typedef struct {
    const char *s;
    int n;      /* codepoint count */
    int *cp;    /* decoded codepoints */
    int *b;     /* b[i] = byte offset of codepoint i; b[n] = strlen(s) */
} tg;

static int tg_lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int tg_isword(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}
static int tg_isws(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
/* _SEPARATORS = " \t\n\r,:;" */
static int tg_issep(int c) { return tg_isws(c) || c == ',' || c == ':' || c == ';'; }
/* [,:-] in the command patterns */
static int tg_iscls(int c) { return c == ',' || c == ':' || c == '-'; }
static int tg_isdash(int c) { return c == '-' || c == 0x2013 || c == 0x2014; }

static int tg_build(tg *t, const char *s) {
    size_t L, i = 0;
    t->s = s ? s : "";
    L = strlen(t->s);
    t->n = 0;
    t->cp = (int *)malloc((L + 1) * sizeof(int));
    t->b = (int *)malloc((L + 2) * sizeof(int));
    if (!t->cp || !t->b) { free(t->cp); free(t->b); t->cp = t->b = NULL; return 0; }
    while (i < L) {
        unsigned char c = (unsigned char)t->s[i];
        int len = 1, cp = c;
        if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; }
        if (len > 1) {
            int k;
            if (i + (size_t)len > L) { len = 1; cp = c; }
            else for (k = 1; k < len; k++) cp = (cp << 6) | ((unsigned char)t->s[i + k] & 0x3F);
        }
        t->b[t->n] = (int)i;
        t->cp[t->n] = cp;
        t->n++;
        i += (size_t)len;
    }
    t->b[t->n] = (int)L;
    return 1;
}

static void tg_free(tg *t) { free(t->cp); free(t->b); t->cp = t->b = NULL; }

static char *tg_sub(const tg *t, int i, int j) {
    size_t start, len;
    char *out;
    if (i < 0) i = 0;
    if (j > t->n) j = t->n;
    if (j < i) j = i;
    start = (size_t)t->b[i];
    len = (size_t)(t->b[j] - t->b[i]);
    out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, t->s + start, len);
    out[len] = '\0';
    return out;
}

static int tg_u16_prefix(const tg *t, int upto) {
    int i, units = 0;
    for (i = 0; i < upto && i < t->n; i++) units += (t->cp[i] > 0xFFFF) ? 2 : 1;
    return units;
}

/* @bot\b[,:-]*\s* at pos -> end. */
static int tg_addr_at(const tg *t, int pos, const char *bot, int *end) {
    size_t blen = strlen(bot), i;
    int p = pos;
    if (p >= t->n || t->cp[p] != '@' || blen == 0) return 0;
    p++;
    for (i = 0; i < blen; i++) {
        if (p >= t->n || tg_lower(t->cp[p]) != tg_lower((unsigned char)bot[i])) return 0;
        p++;
    }
    if (p < t->n && tg_isword(t->cp[p])) return 0;
    while (p < t->n && tg_iscls(t->cp[p])) p++;
    while (p < t->n && tg_isws(t->cp[p])) p++;
    *end = p;
    return 1;
}

typedef struct {
    int matched;
    int start1;   /* codepoint index of the '/' */
    int len1;     /* codepoints of group(1) */
    int end;      /* codepoint index of match end */
    int has_g2;
    int g2_start;
    int g2_len;
} tg_cmd;

static int tg_plain_assert(const tg *t, int pos) {
    if (pos >= t->n) return 1;
    return tg_isws(t->cp[pos]) || tg_iscls(t->cp[pos]) || t->cp[pos] == ';';
}

static int tg_entity_assert(const tg *t, int pos) {
    if (pos >= t->n) return 1;
    return t->cp[pos] != '/' && t->cp[pos] != '\\';
}

static void tg_match_cmd(const tg *t, const char *bot, int has_entities, tg_cmd *m) {
    int p = 0, e, q, cmd_end, sstart, send, chosen = -1;

    memset(m, 0, sizeof(*m));
    while (p < t->n && tg_isws(t->cp[p])) p++;
    while (tg_addr_at(t, p, bot, &e)) p = e;
    if (p >= t->n || t->cp[p] != '/') return;
    q = p + 1;
    if (q >= t->n || !(tg_isword(t->cp[q]) || t->cp[q] == '-')) return;
    while (q < t->n && (tg_isword(t->cp[q]) || t->cp[q] == '-')) q++;
    cmd_end = q;
    sstart = q;
    send = q;
    if (q < t->n && t->cp[q] == '@') {
        int r = q + 1;
        sstart = q + 1;
        while (r < t->n && r < q + 33 && tg_isword(t->cp[r])) r++;
        send = r;
    }
    if (send > sstart) {
        if (has_entities) {
            int L;
            for (L = send - sstart; L >= 1; L--) {
                int nx = sstart + L;
                if (tg_entity_assert(t, nx)) { chosen = nx; break; }
            }
        } else if (tg_plain_assert(t, send)) {
            chosen = send;
        }
    }
    if (chosen < 0) {
        if (has_entities) {
            if (tg_entity_assert(t, cmd_end)) chosen = cmd_end;
        } else if (tg_plain_assert(t, cmd_end)) {
            chosen = cmd_end;
        }
    }
    if (chosen < 0) return;
    m->matched = 1;
    m->start1 = p;
    m->len1 = cmd_end - p;
    m->end = chosen;
    if (sstart > cmd_end && chosen > cmd_end) {
        m->has_g2 = 1;
        m->g2_start = sstart;
        m->g2_len = chosen - sstart;
    }
}

/* ---- entity helpers ---------------------------------------------------- */

static char *str_dup_lower(const char *s, size_t len) {
    size_t i;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    for (i = 0; i < len; i++) out[i] = (char)tg_lower((unsigned char)s[i]);
    out[len] = '\0';
    return out;
}

static char *str_strip_dup(const char *s) {
    size_t a = 0, b = strlen(s);
    char *out;
    while (s[a] && tg_isws((unsigned char)s[a])) a++;
    while (b > a && tg_isws((unsigned char)s[b - 1])) b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* ---- public surface ---------------------------------------------------- */

json_t *w26_telegramad_942de7_utf16_offsets(const char *text) {
    json_t *arr = json_array();
    tg t;
    int i;
    if (!tg_build(&t, text)) return arr;
    for (i = 0; i < t.n; i++) {
        json_append(arr, json_number((double)i));
        if (t.cp[i] > 0xFFFF) json_append(arr, json_number((double)i));
    }
    json_append(arr, json_number((double)t.n));
    tg_free(&t);
    return arr;
}

json_t *w26_telegramad_942de7_entity_handle(const char *etype, const char *span) {
    char *stripped, *out;
    const char *at;
    size_t len;

    stripped = str_strip_dup(span ? span : "");
    if (!stripped) return json_null();
    if (etype && !strcmp(etype, "mention")) {
        const char *p = stripped;
        while (*p == '@') p++;
        out = str_dup_lower(p, strlen(p));
        free(stripped);
        if (!out) return json_null();
        {
            json_t *r = json_string(out);
            free(out);
            return r;
        }
    }
    at = strchr(stripped, '@');
    if (!at) {
        free(stripped);
        return json_null();
    }
    at++;
    len = strlen(at);
    {
        char *trimmed = str_strip_dup(at);
        free(stripped);
        if (!trimmed) return json_null();
        out = str_dup_lower(trimmed, strlen(trimmed));
        free(trimmed);
    }
    if (!out) return json_null();
    {
        json_t *r = json_string(out);
        free(out);
        return r;
    }
}

typedef struct {
    const char *type;   /* normalized entity type, borrowed copy below */
    char *type_owned;
    int off;
    int length;
} tg_ent;

static void tg_ents_parse(const json_t *entities, tg_ent **out, size_t *count) {
    size_t i, n = 0, cnt;
    tg_ent *arr = NULL;
    *out = NULL;
    *count = 0;
    if (!entities || entities->type != JSON_ARRAY) return;
    cnt = json_len(entities);
    if (!cnt) return;
    arr = (tg_ent *)calloc(cnt, sizeof(tg_ent));
    if (!arr) return;
    for (i = 0; i < cnt; i++) {
        const json_t *e = json_get(entities, i);
        const char *raw;
        const json_t *off, *len;
        char *norm;
        const char *dot;
        if (!e || e->type != JSON_OBJECT) continue;
        raw = json_get_str(e, "type", "");
        norm = (char *)malloc(strlen(raw) + 1);
        if (!norm) continue;
        strcpy(norm, raw);
        for (char *p = norm; *p; p++) *p = (char)tg_lower((unsigned char)*p);
        dot = strrchr(norm, '.');
        arr[n].type = dot ? dot + 1 : norm;
        arr[n].type_owned = norm;
        off = json_obj_get(e, "offset");
        len = json_obj_get(e, "length");
        arr[n].off = (off && off->type == JSON_NUMBER) ? (int)off->num_val : -1;
        arr[n].length = (len && len->type == JSON_NUMBER) ? (int)len->num_val : 0;
        n++;
    }
    *out = arr;
    *count = n;
}

static void tg_ents_free(tg_ent *arr, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) free(arr[i].type_owned);
    free(arr);
}

json_t *w26_telegramad_942de7_leading_handles(const char *text, const json_t *entities) {
    tg t;
    tg_ent *ents = NULL;
    size_t nents = 0, i;
    json_t *out = json_array();

    if (!tg_build(&t, text)) return out;
    tg_ents_parse(entities, &ents, &nents);

    if (nents > 0) {
        int *offsets;
        int u16 = 0, k, pos;
        /* offsets[utf16 index] -> codepoint index */
        for (i = 0; i < (size_t)t.n; i++) u16 += (t.cp[i] > 0xFFFF) ? 2 : 1;
        offsets = (int *)malloc((size_t)(u16 + 2) * sizeof(int));
        if (!offsets) {
            tg_ents_free(ents, nents);
            tg_free(&t);
            return out;
        }
        k = 0;
        for (i = 0; i < (size_t)t.n; i++) {
            offsets[k++] = (int)i;
            if (t.cp[i] > 0xFFFF) offsets[k++] = (int)i;
        }
        offsets[k] = t.n;
        {
            int *starts = (int *)calloc(nents, sizeof(int));
            int *ends = (int *)calloc(nents, sizeof(int));
            char **handles = (char **)calloc(nents, sizeof(char *));
            size_t nstarts = 0, j;
            if (!starts || !ends || !handles) {
                free(starts); free(ends); free(handles); free(offsets);
                tg_ents_free(ents, nents);
                tg_free(&t);
                return out;
            }
            for (i = 0; i < nents; i++) {
                int start, end, dup = 0;
                char *span, *handle = NULL;
                if (!ents[i].type) continue;
                if (strcmp(ents[i].type, "mention") && strcmp(ents[i].type, "bot_command")) continue;
                if (ents[i].off < 0 || ents[i].length <= 0 || ents[i].off + ents[i].length >= u16 + 1)
                    continue;
                start = offsets[ents[i].off];
                end = offsets[ents[i].off + ents[i].length];
                for (j = 0; j < nstarts; j++)
                    if (starts[j] == start) { dup = 1; break; }
                if (dup) continue;
                span = tg_sub(&t, start, end);
                if (span) {
                    json_t *h = w26_telegramad_942de7_entity_handle(ents[i].type, span);
                    if (h && h->type == JSON_STRING) handle = str_dup_lower(h->str_val, strlen(h->str_val));
                    json_free(h);
                    free(span);
                }
                starts[nstarts] = start;
                ends[nstarts] = end;
                handles[nstarts] = handle;   /* NULL = handle None */
                nstarts++;
            }
            pos = 0;
            while (pos < t.n && tg_isws(t.cp[pos])) pos++;
            while (pos < t.n) {
                const char *sep = " \t\n\r,:;";
                char *handle = NULL;
                int found = -1;
                for (j = 0; j < nstarts; j++)
                    if (starts[j] == pos) { found = (int)j; break; }
                if (found >= 0) {
                    pos = ends[found];
                    handle = handles[found];
                } else {
                    /* without a start here, no entity covers pos: stop */
                    break;
                }
                if (!handle) break;
                json_append(out, json_string(handle));
                while (pos < t.n && strchr(sep, t.cp[pos]) && t.cp[pos] < 128) pos++;
            }
            for (j = 0; j < nstarts; j++) free(handles[j]);
            free(starts); free(ends); free(handles);
        }
        free(offsets);
        tg_ents_free(ents, nents);
        tg_free(&t);
        return out;
    }

    /* No entities: read @handle / /cmd@handle from the text itself. */
    {
        int pos = 0;
        while (pos < t.n && tg_isws(t.cp[pos])) pos++;
        while (pos < t.n) {
            int p = pos, q, run;
            char *handle;
            if (t.cp[p] == '/') {
                q = p + 1;
                while (q < t.n && tg_isword(t.cp[q])) q++;
                if (q == p + 1 || q >= t.n || t.cp[q] != '@') break;
                p = q;
            }
            if (t.cp[p] != '@') break;
            q = p + 1;
            run = 0;
            while (q + run < t.n && tg_isword(t.cp[q + run])) run++;
            if (run < 2 || run > 31) break;
            handle = tg_sub(&t, q, q + run);
            if (!handle) break;
            {
                char *lowered = str_dup_lower(handle, strlen(handle));
                free(handle);
                if (!lowered) break;
                json_append(out, json_string(lowered));
                free(lowered);
            }
            pos = q + run;
            while (pos < t.n && strchr(" \t\n\r,:;", t.cp[pos]) && t.cp[pos] < 128) pos++;
        }
    }
    tg_free(&t);
    return out;
}

json_t *w26_telegramad_942de7_leading_bot_handles(const char *text,
                                                  const json_t *entities,
                                                  const json_t *bots) {
    json_t *all = w26_telegramad_942de7_leading_handles(text, entities);
    json_t *out = json_array();
    size_t i, j;

    for (i = 0; i < json_len(all); i++) {
        const json_t *h = json_get(all, i);
        int keep = 0;
        if (!h || h->type != JSON_STRING) continue;
        for (j = 0; j < json_len(bots); j++) {
            const json_t *b = json_get(bots, j);
            if (b && b->type == JSON_STRING && b->str_val && !strcmp(b->str_val, h->str_val)) {
                keep = 1;
                break;
            }
        }
        if (!keep) continue;
        keep = 1;
        for (j = 0; j < json_len(out); j++) {
            const json_t *prev = json_get(out, j);
            if (prev && prev->type == JSON_STRING && !strcmp(prev->str_val, h->str_val)) { keep = 0; break; }
        }
        if (keep) json_append(out, json_string(h->str_val));
    }
    json_free(all);
    /* sorted(set(...)) -- insertion sort keeps this allocation-free. */
    for (i = 0; i + 1 < json_len(out); i++) {
        for (j = 0; j + 1 < json_len(out) - i; j++) {
            json_t *a = json_get(out, j), *b = json_get(out, j + 1);
            if (a && b && a->type == JSON_STRING && b->type == JSON_STRING &&
                strcmp(a->str_val, b->str_val) > 0) {
                char *tmp = a->str_val;
                a->str_val = b->str_val;
                b->str_val = tmp;
            }
        }
    }
    return out;
}

json_t *w26_telegramad_942de7_command_after_address(const char *text,
                                                    const char *bot_username,
                                                    int has_entities) {
    tg t;
    tg_cmd m;
    json_t *out;

    if (!tg_build(&t, text)) return json_null();
    tg_match_cmd(&t, bot_username ? bot_username : "", has_entities ? 1 : 0, &m);
    if (!m.matched) {
        tg_free(&t);
        return json_null();
    }
    out = json_object();
    json_set(out, "start1", json_number((double)m.start1));
    json_set(out, "len1", json_number((double)m.len1));
    json_set(out, "end", json_number((double)m.end));
    if (m.has_g2) {
        char *g2 = tg_sub(&t, m.g2_start, m.g2_start + m.g2_len);
        char *lowered = g2 ? str_dup_lower(g2, strlen(g2)) : NULL;
        free(g2);
        json_set(out, "group2", lowered ? json_string(lowered) : json_null());
        free(lowered);
    } else {
        json_set(out, "group2", json_null());
    }
    tg_free(&t);
    return out;
}

json_t *w26_telegramad_942de7_foreign_command_target(const char *text,
                                                     const json_t *entities,
                                                     const char *bot_username) {
    tg t;
    tg_cmd m;
    tg_ent *ents = NULL;
    size_t nents = 0, i;
    const char *bot = bot_username ? bot_username : "";
    json_t *result = json_null();

    if (!tg_build(&t, text)) return result;
    tg_ents_parse(entities, &ents, &nents);
    tg_match_cmd(&t, bot, nents > 0, &m);
    if (!m.matched || !m.has_g2) goto done;
    {
        char *g2 = tg_sub(&t, m.g2_start, m.g2_start + m.g2_len);
        int same = 1;
        size_t bl = strlen(bot), k;
        if (!g2) goto done;
        if (strlen(g2) != bl) same = 0;
        else for (k = 0; k < bl; k++)
            if (tg_lower((unsigned char)g2[k]) != tg_lower((unsigned char)bot[k])) { same = 0; break; }
        if (same) { free(g2); goto done; }
        if (nents > 0) {
            int offset = tg_u16_prefix(&t, m.start1);
            int accept = 0;
            for (i = 0; i < nents; i++) {
                if (!ents[i].type || strcmp(ents[i].type, "bot_command")) continue;
                if (ents[i].off != offset) continue;
                if (ents[i].length == m.end - m.start1 ||
                    (m.start1 + ents[i].length < t.n && t.cp[m.start1 + ents[i].length] == '-')) {
                    accept = 1;
                    break;
                }
            }
            if (!accept) { free(g2); goto done; }
        }
        {
            char *lowered = str_dup_lower(g2, strlen(g2));
            free(g2);
            if (lowered) {
                result = json_string(lowered);
                free(lowered);
            }
        }
    }
done:
    tg_ents_free(ents, nents);
    tg_free(&t);
    return result;
}

/* ---- trigger-text cleanup ---------------------------------------------- */

static char *str_concat(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    char *out = (char *)malloc(la + lb + 1);
    if (!out) return NULL;
    memcpy(out, a, la);
    memcpy(out + la, b, lb);
    out[la + lb] = '\0';
    return out;
}

/* @bot\b[,:-]*\s* at p (plain string). */
static int tok_at(const char *s, size_t p, const char *bot, size_t *end) {
    size_t blen = strlen(bot), i, len = strlen(s);
    if (!blen || p >= len || s[p] != '@') return 0;
    p++;
    for (i = 0; i < blen; i++) {
        if (p + i >= len || (char)tg_lower((unsigned char)s[p + i]) != (char)tg_lower((unsigned char)bot[i]))
            return 0;
    }
    p += blen;
    if (p < len && tg_isword((unsigned char)s[p])) return 0;
    while (p < len && tg_iscls((unsigned char)s[p])) p++;
    while (p < len && tg_isws((unsigned char)s[p])) p++;
    *end = p;
    return 1;
}

/* One or more handle tokens from p; returns how many. */
static int chain_at(const char *s, size_t p, const char *bot, size_t *end, int *count) {
    int n = 0;
    size_t e;
    for (;;) {
        if (!tok_at(s, p, bot, &e)) break;
        p = e;
        n++;
    }
    *end = p;
    *count = n;
    return n;
}

/* ^(\s*)(?:@bot\b[,:-]*\s*)+ -> \1 */
static char *sub_a(const char *s, const char *bot) {
    size_t len = strlen(s), i = 0, p, e;
    int count = 0;
    char *head, *tail, *out;
    while (i < len && tg_isws((unsigned char)s[i])) i++;
    p = i;
    chain_at(s, p, bot, &e, &count);
    if (!count) return strdup(s);
    head = (char *)malloc(i + 1);
    if (!head) return strdup(s);
    memcpy(head, s, i);
    head[i] = '\0';
    tail = (char *)malloc(len - e + 1);
    if (!tail) { free(head); return strdup(s); }
    memcpy(tail, s + e, len - e);
    tail[len - e] = '\0';
    out = str_concat(head, tail);
    free(head);
    free(tail);
    return out ? out : strdup(s);
}

/* (?:@bot\b[,:-]*\s*+)+(?=$|[-–—]++\w) -> "" */
static char *sub_b(const char *s, const char *bot) {
    size_t len = strlen(s), i = 0, oi = 0;
    char *out = (char *)malloc(len + 1);
    if (!out) return strdup(s);
    while (i < len) {
        size_t e;
        int count = 0;
        chain_at(s, i, bot, &e, &count);
        if (count) {
            size_t look = e;
            int dashes = 0;
            while (look < len) {
                /* decode one codepoint's worth: ASCII '-' or the two em dashes */
                if (s[look] == '-') { look++; dashes++; continue; }
                if ((unsigned char)s[look] == 0xE2 && look + 2 < len &&
                    (unsigned char)s[look + 1] == 0x80 &&
                    ((unsigned char)s[look + 2] == 0x93 || (unsigned char)s[look + 2] == 0x94)) {
                    look += 3;
                    dashes++;
                    continue;
                }
                break;
            }
            if (dashes && look < len && tg_isword((unsigned char)s[look])) {
                i = e;
                continue;
            }
            if (dashes == 0 && look >= len) {
                i = e;
                continue;
            }
        }
        out[oi++] = s[i++];
    }
    out[oi] = '\0';
    return out;
}

/* @bot\b[,:\-]*\s* -> "" */
static char *sub_d(const char *s, const char *bot) {
    size_t len = strlen(s), i = 0, oi = 0, e;
    char *out = (char *)malloc(len + 1);
    if (!out) return strdup(s);
    while (i < len) {
        if (tok_at(s, i, bot, &e)) { i = e; continue; }
        out[oi++] = s[i++];
    }
    out[oi] = '\0';
    return out;
}

/* ^[,:-]*\s* -> "" */
static char *strip_lead_run(const char *s) {
    size_t i = 0, len = strlen(s);
    while (i < len && tg_iscls((unsigned char)s[i])) i++;
    while (i < len && tg_isws((unsigned char)s[i])) i++;
    return strdup(s + i);
}

json_t *w26_telegramad_942de7_clean_trigger(const char *text, const char *bot_username) {
    tg t;
    tg_cmd m;
    const char *bot = bot_username ? bot_username : "";

    if (!text) return json_null();
    if (!text[0] || !bot[0]) return json_string(text);
    if (!tg_build(&t, text)) return json_null();
    tg_match_cmd(&t, bot, 0, &m);
    if (m.matched) {
        int g2_ok = 1;
        if (m.has_g2) {
            char *g2 = tg_sub(&t, m.g2_start, m.g2_start + m.g2_len);
            size_t bl = strlen(bot), k;
            g2_ok = g2 && strlen(g2) == bl;
            if (g2_ok) for (k = 0; k < bl; k++)
                if (tg_lower((unsigned char)g2[k]) != tg_lower((unsigned char)bot[k])) { g2_ok = 0; break; }
            free(g2);
        }
        if (g2_ok) {
            char *rest = tg_sub(&t, m.end, t.n);
            char *g1 = tg_sub(&t, m.start1, m.start1 + m.len1);
            char *out, *trimmed;
            if (!rest || !g1) { free(rest); free(g1); tg_free(&t); return json_null(); }
            if (m.has_g2) {
                char *cleanrest = strip_lead_run(rest);
                char *spaced = cleanrest ? str_concat(" ", cleanrest) : NULL;
                free(rest);
                free(cleanrest);
                rest = spaced;
                if (!rest) { free(g1); tg_free(&t); return json_null(); }
            }
            {
                char *a = sub_a(rest, bot);
                char *b = a ? sub_b(a, bot) : NULL;
                free(a);
                free(rest);
                rest = b;
            }
            if (!rest) { free(g1); tg_free(&t); return json_null(); }
            out = str_concat(g1, rest);
            free(g1);
            free(rest);
            tg_free(&t);
            if (!out) return json_null();
            trimmed = str_strip_dup(out);
            free(out);
            if (!trimmed) return json_null();
            {
                json_t *r = json_string(trimmed);
                free(trimmed);
                return r;
            }
        }
    }
    tg_free(&t);

    /* Legacy cleanup: paths and every other non-command text. */
    {
        size_t len = strlen(text), i = 0, j, mend;
        while (i < len && tg_isws((unsigned char)text[i])) i++;
        if (i < len && text[i] == '/') {
            j = i + 1;
            while (j < len && tg_isword((unsigned char)text[j])) j++;
            if (j > i + 1 && j < len && text[j] == '@' && tok_at(text, j, bot, &mend)) {
                char *rest = strdup(text + mend);
                char *clean = rest ? sub_d(rest, bot) : NULL;
                char *trimmed, *g1, *out;
                free(rest);
                if (!clean) return json_null();
                trimmed = str_strip_dup(clean);
                free(clean);
                if (!trimmed) return json_null();
                g1 = (char *)malloc(j - i + 1);
                if (!g1) { free(trimmed); return json_null(); }
                memcpy(g1, text + i, j - i);
                g1[j - i] = '\0';
                if (trimmed[0]) {
                    char *spaced = str_concat(g1, " ");
                    out = spaced ? str_concat(spaced, trimmed) : NULL;
                    free(spaced);
                } else {
                    out = strdup(g1);
                }
                free(g1);
                free(trimmed);
                if (!out) return json_null();
                {
                    json_t *r = json_string(out);
                    free(out);
                    return r;
                }
            }
        }
    }
    {
        char *clean = sub_d(text, bot);
        char *trimmed;
        json_t *r;
        if (!clean) return json_null();
        trimmed = str_strip_dup(clean);
        free(clean);
        if (!trimmed) return json_null();
        if (!trimmed[0]) {
            free(trimmed);
            return json_string(text);
        }
        r = json_string(trimmed);
        free(trimmed);
        return r;
    }
}
