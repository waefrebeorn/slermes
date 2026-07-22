/*
 * debug_cli.c — `hermes debug` helpers (faithful C11 port of
 * hermes_cli/debug.py pure logic). See debug_cli.h.
 */

#include "debug_cli.h"
#include "hermes_redact.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define AUTO_DELETE_SECONDS 21600
#define GRACE_SECONDS 86400

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

/* PoP: debug_extract_paste_id @ hermes_cli/debug.py:_extract_paste_id */
char *debug_extract_paste_id(const char *url) {
    if (!url) return NULL;
    while (*url == ' ' || *url == '\t') url++;
    char *tmp = xstrdup(url);
    size_t L = strlen(tmp);
    while (L > 0 && (tmp[L-1]==' '||tmp[L-1]=='\t'||tmp[L-1]=='/')) tmp[--L]='\0';
    char *id = NULL;
    size_t https_len = strlen("https://paste.rs/");
    size_t http_len  = strlen("http://paste.rs/");
    if (strncmp(tmp, "https://paste.rs/", https_len) == 0) id = xstrdup(tmp + https_len);
    else if (strncmp(tmp, "http://paste.rs/", http_len) == 0) id = xstrdup(tmp + http_len);
    free(tmp);
    if (id && id[0] == '\0') { free(id); return NULL; }
    return id;
}

char *debug_pending_path(const char *hermes_home) {
    size_t need = strlen(hermes_home) + 1 + 6 + 1 + 11 + 1;
    char *p = (char*)malloc(need);
    snprintf(p, need, "%s/pastes/pending.json", hermes_home);
    return p;
}

/* Minimal JSON parse of [{"url":"...","expire_at":<num>}, ...]. Returns count. */
debug_pending_t *debug_load_pending(const char *hermes_home, int *out_count) {
    char *path = debug_pending_path(hermes_home);
    FILE *f = fopen(path, "rb");
    free(path);
    *out_count = 0;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char *buf = (char*)malloc(sz + 1);
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);

    debug_pending_t *arr = NULL; int n = 0, cap = 0;
    const char *p = buf;
    /* expect leading '[' */
    while (*p && *p != '[') p++;
    if (*p != '[') { free(buf); return NULL; }
    p++;
    while (*p) {
        while (*p && *p != '{' && *p != ']') p++;
        if (*p == ']') break;
        if (*p != '{') break;
        /* parse one object */
        char *url = NULL; double expire = 0;
        const char *q = p + 1;
        while (*q && *q != '}') {
            if (strncmp(q, "\"url\"", 5) == 0) {
                q += 5; while (*q && *q != '"') q++;
                if (*q == '"') { q++; const char *s = q; while (*q && *q != '"') q++; size_t l = (size_t)(q - s); url = (char*)malloc(l+1); memcpy(url, s, l); url[l]='\0'; }
            } else if (strncmp(q, "\"expire_at\"", 11) == 0) {
                q += 11; while (*q && (*q==':'||isspace((unsigned char)*q))) q++;
                expire = strtod(q, (char**)&q);
            } else {
                q++;
            }
        }
        if (url && *q == '}') {
            if (n >= cap) { cap = cap?cap*2:8; arr = (debug_pending_t*)realloc(arr, cap*sizeof(debug_pending_t)); }
            arr[n].url = url; arr[n].expire_at = expire; n++;
        } else {
            free(url);
        }
        while (*p && *p != '}') p++;
        if (*p == '}') p++;
        /* skip until next '{' or ']' */
    }
    free(buf);
    *out_count = n;
    return arr;
}

/* PoP: debug_save_pending @ hermes_cli/debug.py:_save_pending */
bool debug_save_pending(const char *hermes_home, debug_pending_t *entries, int n) {
    char *path = debug_pending_path(hermes_home);
    /* ensure dir */
    char dirstr[4096];
    snprintf(dirstr, sizeof(dirstr), "%s/pastes", hermes_home);
    mkdir(dirstr, 0755);
    size_t need = strlen(path) + 5;
    char *tmp = (char*)malloc(need);
    snprintf(tmp, need, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) { free(tmp); free(path); return false; }
    fprintf(f, "[\n");
    for (int i = 0; i < n; i++) {
        fprintf(f, "  {\"url\": \"%s\", \"expire_at\": %.0f}%s\n",
                entries[i].url ? entries[i].url : "",
                entries[i].expire_at,
                i + 1 < n ? "," : "");
    }
    fprintf(f, "]\n");
    fclose(f);
    rename(tmp, path); /* atomic-ish on same fs */
    free(tmp); free(path);
    return true;
}

/* PoP: debug_record_pending @ hermes_cli/debug.py:_record_pending */
void debug_record_pending(const char *hermes_home, const char *urls[], int n,
                          double now, int delay_seconds) {
    /* collect paste.rs urls */
    const char **keep = (const char**)malloc((n>0?n:1)*sizeof(char*));
    int nk = 0;
    for (int i = 0; i < n; i++) {
        char *id = debug_extract_paste_id(urls[i]);
        if (id) { free(id); keep[nk++] = urls[i]; }
    }
    if (nk == 0) { free(keep); return; }

    int existing_n; debug_pending_t *existing = debug_load_pending(hermes_home, &existing_n);
    /* merge by url, keep later expire_at */
    for (int i = 0; i < existing_n; i++) { /* ensure url non-null */ if (!existing[i].url) existing[i].url = xstrdup(""); }
    double expire_at = now + delay_seconds;
    for (int i = 0; i < nk; i++) {
        const char *u = keep[i];
        int found = -1;
        for (int j = 0; j < existing_n; j++) if (existing[j].url && strcmp(existing[j].url, u) == 0) { found = j; break; }
        if (found >= 0) {
            if (expire_at > existing[found].expire_at) existing[found].expire_at = expire_at;
        } else {
            existing = (debug_pending_t*)realloc(existing, (existing_n+1)*sizeof(debug_pending_t));
            existing[existing_n].url = xstrdup(u);
            existing[existing_n].expire_at = expire_at;
            existing_n++;
        }
    }
    debug_save_pending(hermes_home, existing, existing_n);
    for (int i = 0; i < existing_n; i++) free(existing[i].url);
    free(existing);
    free(keep);
}

/* PoP: debug_sweep_expired_pastes @ hermes_cli/debug.py:_sweep_expired_pastes */
void debug_sweep_expired_pastes(const char *hermes_home, double now,
                                int (*delete_cb)(const char *url),
                                int *out_deleted, int *out_remaining) {
    int n; debug_pending_t *entries = debug_load_pending(hermes_home, &n);
    int deleted = 0;
    debug_pending_t *remaining = NULL; int rn = 0;
    for (int i = 0; i < n; i++) {
        double exp = entries[i].expire_at;
        if (exp > now) {
            remaining = (debug_pending_t*)realloc(remaining, (rn+1)*sizeof(debug_pending_t));
            remaining[rn].url = xstrdup(entries[i].url ? entries[i].url : "");
            remaining[rn].expire_at = exp; rn++;
            continue;
        }
        int ok = 0;
        if (delete_cb && entries[i].url) ok = delete_cb(entries[i].url);
        if (ok) { deleted++; continue; }
        /* retain failed deletes up to 24h past expiration */
        if (exp + GRACE_SECONDS > now) {
            remaining = (debug_pending_t*)realloc(remaining, (rn+1)*sizeof(debug_pending_t));
            remaining[rn].url = xstrdup(entries[i].url ? entries[i].url : "");
            remaining[rn].expire_at = exp; rn++;
        } else {
            deleted++; /* reaped */
        }
    }
    if (deleted) debug_save_pending(hermes_home, remaining, rn);
    for (int i = 0; i < n; i++) free(entries[i].url);
    free(entries);
    if (out_deleted) *out_deleted = deleted;
    if (out_remaining) *out_remaining = rn;
    for (int i = 0; i < rn; i++) free(remaining[i].url);
    free(remaining);
}

void debug_free_pending(debug_pending_t *arr, int n) {
    if (!arr) return;
    for (int i = 0; i < n; i++) free(arr[i].url);
    free(arr);
}

/* PoP: debug_redact_log_text @ hermes_cli/debug.py:_redact_log_text */
char *debug_redact_log_text(const char *text) {
    if (!text || !*text) return xstrdup(text ? text : "");
    char *red = hermes_redact(text);          /* force-redact secrets */
    /* mask email addresses: user@host.tld -> [REDACTED_EMAIL] */
    /* tokenize and replace */
    size_t L = strlen(red);
    char *out = (char*)malloc(L * 2 + 1);
    size_t o = 0;
    size_t i = 0;
    while (i < L) {
        /* detect local@domain.tld */
        size_t at = (size_t)-1;
        for (size_t j = i; j < L; j++) { if (red[j] == '@') { at = j; break; } }
        if (at != (size_t)-1) {
            /* walk back for local part start */
            size_t ls = at;
            while (ls > 0 && (isalnum((unsigned char)red[ls-1]) || red[ls-1]=='.'||red[ls-1]=='_'||red[ls-1]=='%'||red[ls-1]=='+'||red[ls-1]=='-')) ls--;
            /* walk forward for domain */
            size_t de = at + 1;
            while (de < L && (isalnum((unsigned char)red[de]) || red[de]=='.'||red[de]=='-')) de++;
            /* ensure domain has a dot and TLD >=2 */
            bool has_dot = false;
            for (size_t k = at+1; k < de; k++) if (red[k]=='.') { has_dot = true; break; }
            if (has_dot && de > at+3) {
                /* copy pre-local */
                while (i < ls) out[o++] = red[i++];
                memcpy(out+o, "[REDACTED_EMAIL]", 16); o += 16;
                i = de;
                continue;
            }
        }
        out[o++] = red[i++];
    }
    out[o] = '\0';
    free(red);
    return out;
}
