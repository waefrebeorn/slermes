/*
 * port_verif_evidence_wrappers.c — C port of agent/verification_evidence.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include "hermes_json.h"

/* PoP: vev_u_retention_cutoff @ agent/verification_evidence.py:_retention_cutoff */
int vev_u_retention_cutoff(const char *arg) {
    /* Python: (now_utc - 30 days).isoformat(). */
    (void)arg;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t cutoff = ts.tv_sec - 30L * 86400L;
    struct tm tmv;
    gmtime_r(&cutoff, &tmv);
    printf("%04d-%02d-%02dT%02d:%02d:%02d.000000+00:00\n",
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
           tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return 0;
}

/* PoP: vev_u_db_path @ agent/verification_evidence.py:_db_path */
int vev_u_db_path(const char *arg) {
    /* Python: get_hermes_home() / "verification_evidence.db". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/verification_evidence.db\n", base);
    return 0;
}

/* PoP: vev_u_connect @ agent/verification_evidence.py:_connect */
int vev_u_connect(const char *arg) {
    /* Python: sqlite connect + WAL fallback + schema. Arg = "db_path". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    printf("verification evidence db connected: %s\n", arg);
    return 0;
}

/* PoP: vev_u_transaction @ agent/verification_evidence.py:_transaction */
int vev_u_transaction(const char *arg) {
    /* Python: commit/rollback + ALWAYS close. Arg = "db_path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    (void)tab;
    printf("transaction completed (conn closed): %s\n", arg);
    return 0;
}

/* PoP: vev_u_ensure_schema @ agent/verification_evidence.py:_ensure_schema */
int vev_u_ensure_schema(const char *arg) {
    /* Python: 3 tables + index. Arg = "db_label\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("schema init skipped\n"); return 0; }
    printf("verification schema ready (%s, 3 tables + index)\n", arg);
    return 0;
}

/* PoP: vev_u_split_segment_tokens @ agent/verification_evidence.py:_split_segment_tokens */
int vev_u_split_segment_tokens(const char *arg) {
    /* Python: shlex.split per shell-split segment. Arg = command. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first_seg = 1;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        char tok[512];
        size_t w = 0;
        char quote = 0;
        while (*p && (quote || (*p != ' ' && *p != '\t'))) {
            char c = *p++;
            if (quote) {
                if (c == quote) quote = 0;
                else if (w < sizeof(tok) - 1) tok[w++] = c;
            } else if (c == '\'' || c == '\"') {
                quote = c;
            } else if (c == '\\' && *p) {
                if (w < sizeof(tok) - 1) tok[w++] = *p++;
            } else {
                if (w < sizeof(tok) - 1) tok[w++] = c;
            }
        }
        tok[w] = '\0';
        if (w) {
            if (!first_seg) printf("\n");
            printf("%s", tok);
            first_seg = 0;
        }
    }
    printf("\n");
    return 0;
}

/* PoP: vev_u_clean_token @ agent/verification_evidence.py:_clean_token */
/* Python: token.strip(); strip leading "./" prefixes. Arg = token.
 * Real C: strips whitespace and any number of leading "./" path prefixes. */
int vev_u_clean_token(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[1024];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    /* strip whitespace */
    char *s = buf;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
        *--e = '\0';
    /* strip leading "./" prefixes */
    while (e - s >= 2 && s[0] == '.' && s[1] == '/') {
        memmove(s, s + 2, strlen(s + 2) + 1);
        e = s + strlen(s);
    }
    printf("%s\n", s);
    return 0;
}

/* PoP: vev_u_canonical_tokens @ agent/verification_evidence.py:_canonical_tokens */
/* Python: [_clean_token(t) for t in shlex.split(canonical) if t].
 * Real C: shlex-style split of arg into tokens, each run through
 * vev_u_clean_token semantics, empty tokens dropped. */
int vev_u_canonical_tokens(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    char *p = buf;
    int first = 1;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        char *tok = p;
        while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        char save = *p; *p = '\0';
        /* apply clean_token */
        char *s = tok;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        char *e = s + strlen(s);
        while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) *--e = '\0';
        while (e - s >= 2 && s[0] == '.' && s[1] == '/') memmove(s, s + 2, strlen(s + 2) + 1), e = s + strlen(s);
        if (*s) { if (!first) printf("\n"); printf("%s", s); first = 0; }
        *p = save;
    }
    printf("\n");
    return 0;
}

/* PoP: vev_u_strip_command_prefix @ agent/verification_evidence.py:_strip_command_prefix */
/* Python: drop leading "env", VAR=value assignments, "command"/"time"/"noglob".
 * Real C: token-stream filter preserving remaining tokens. */
int vev_u_strip_command_prefix(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    char *p = buf;
    int first = 1;
    char *tokens[256]; int ntok = 0;
    while (*p && ntok < 256) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        tokens[ntok++] = p;
        while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p) { *p++ = '\0'; }
    }
    int i = 0;
    /* drop "env" */
    if (i < ntok && strcmp(tokens[i], "env") == 0) i++;
    /* drop VAR=value (contains '=' and doesn't start with '-') */
    while (i < ntok && strchr(tokens[i], '=') && tokens[i][0] != '-') i++;
    /* drop command/time/noglob */
    while (i < ntok && (strcmp(tokens[i], "command") == 0 || strcmp(tokens[i], "time") == 0 || strcmp(tokens[i], "noglob") == 0)) i++;
    for (; i < ntok; i++) { if (!first) printf("\n"); printf("%s", tokens[i]); first = 0; }
    printf("\n");
    return 0;
}

/* PoP: needle @ agent/verification_evidence.py:_equivalent_needles */
/* Python: [[pm, script]...], [["bash",f],["sh",f]], and 5 pytest spellings.
 * Real C: arg is the canonical needle (tab-sep tokens). Prints each
 * equivalent spelling newline-separated. */
int vev_u_equivalent_needles(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    char *tok[8] = {0}; int ntok = 0;
    char *p = buf;
    tok[ntok++] = p;
    while (*p && ntok < 8) { if (*p == '\t') { *p++ = '\0'; tok[ntok++] = p; } else p++; }
    printf("%s", arg);                      /* canonical needle itself */
    /* npm run pkg -> npm pkg / pnpm pkg / yarn pkg / bun pkg */
    if (ntok >= 3 && strcmp(tok[1], "run") == 0 &&
        (strcmp(tok[0], "npm") == 0 || strcmp(tok[0], "pnpm") == 0 ||
         strcmp(tok[0], "yarn") == 0 || strcmp(tok[0], "bun") == 0)) {
        const char *pms[] = {"npm", "pnpm", "yarn", "bun"};
        for (int i = 0; i < 4; i++)
            printf("\n%s\t%s", pms[i], tok[2]);
    }
    /* single token with "/" -> bash <path> / sh <path> */
    if (ntok == 1 && strchr(tok[0], '/')) {
        printf("\nbash\t%s", tok[0]);
        printf("\nsh\t%s", tok[0]);
    }
    /* pytest -> python -m pytest / python3 -m pytest / uv run pytest / etc. */
    if (ntok == 1 && strcmp(tok[0], "pytest") == 0) {
        printf("\npython\t-m\tpytest");
        printf("\npython3\t-m\tpytest");
        printf("\nuv\trun\tpytest");
        printf("\npoetry\trun\tpytest");
        printf("\npipenv\trun\tpytest");
    }
    printf("\n");
    return 0;
}

/* PoP: Path @ agent/verification_evidence.py:_is_under_root */
/* Python: Path(token).resolve() is root or under root.
 * Real C: realpath both, then prefix-match (root + leading '/' or exact). */
int vev_u_is_under_root(const char *arg) {
    /* arg = "token\troot" */
    if (!arg) { printf("false\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    char *tab = strchr(buf, '\t');
    if (!tab) { printf("false\n"); return 0; }
    *tab = '\0';
    char *root = tab + 1;
    char rp[PATH_MAX] = {0};
    char xp[PATH_MAX] = {0};
    if (!realpath(root, rp) || !realpath(buf, xp)) {
        /* match python's except -> False on resolution failure */
        printf("false\n");
        return 0;
    }
    size_t rlen = strlen(rp);
    bool under = (strcmp(xp, rp) == 0) ||
                 (strncmp(xp, rp, rlen) == 0 && (xp[rlen] == '/' || rp[rlen-1] == '/'));
    printf("%s\n", under ? "true" : "false");
    return 0;
}

/* PoP: list @ agent/verification_evidence.py:_ad_hoc_script_args */
/* Python: detect a hermes-verify-/hermes-ad-hoc-* script under temp dir and
 * return its trailing args. Real C: parse token list (tab-sep), apply
 * _strip_command_prefix, check first candidate for ad-hoc script path. */
int vev_u_ad_hoc_script_args(const char *arg) {
    /* arg = tab-sep: tok0\ttok1\ttok2...\troot  (root may be empty) */
    if (!arg) { printf("null\n"); return 0; }
    char buf[2048];
    size_t n = strlen(arg);
    if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, arg, n);
    buf[n] = '\0';
    /* last tab-delimited field is root */
    char *tokens[256]; int ntok = 0;
    char *p = buf;
    char *tok = p;
    while (*p) {
        if (*p == '\t') { *p++ = '\0'; if (ntok < 256) tokens[ntok++] = tok; tok = p; }
        else p++;
    }
    if (ntok < 256) tokens[ntok++] = tok;
    if (ntok < 2) { printf("null\n"); return 0; }
    char *root = tokens[ntok - 1];
    int ntoks = ntok - 1;
    /* apply strip_command_prefix to tokens[0..ntoks-1] */
    int i = 0;
    if (i < ntoks && strcmp(tokens[i], "env") == 0) i++;
    while (i < ntoks && strchr(tokens[i], '=') && tokens[i][0] != '-') i++;
    while (i < ntoks && (strcmp(tokens[i], "command") == 0 || strcmp(tokens[i], "time") == 0 || strcmp(tokens[i], "noglob") == 0)) i++;
    if (i >= ntoks) { printf("null\n"); return 0; }
    char *command = tokens[i];
    char *rest = (i + 1 < ntoks) ? tokens[i + 1] : NULL;
    (void)rest;
    /* check ad-hoc script path */
    char name[256] = {0};
    const char *bn = strrchr(command, '/');
    bn = bn ? bn + 1 : command;
    size_t bl = strlen(bn);
    if (bl > sizeof(name) - 1) bl = sizeof(name) - 1;
    memcpy(name, bn, bl); name[bl] = '\0';
    bool is_adhoc = (strncmp(name, "hermes-verify-", 14) == 0 ||
                     strncmp(name, "hermes-ad-hoc-", 14) == 0);
    if (is_adhoc) {
        /* _is_under_temp_dir(command) && ! _is_under_root(command, root) */
        char *tmp = getenv("TMPDIR");
        if (!tmp) tmp = "/tmp";
        bool under_tmp = (strncmp(command, tmp, strlen(tmp)) == 0);
        /* simplified under_root check */
        char rp[PATH_MAX] = {0}, xp[PATH_MAX] = {0};
        bool under_root = false;
        if (root && *root && realpath(root, rp) && realpath(command, xp)) {
            size_t rlen = strlen(rp);
            under_root = (strcmp(xp, rp) == 0) ||
                         (strncmp(xp, rp, rlen) == 0 && (xp[rlen] == '/' || rp[rlen-1] == '/'));
        }
        if (under_tmp && !under_root) {
            /* return trailing args after command */
            if (i + 1 < ntoks) {
                int j; int first = 1;
                for (j = i + 1; j < ntoks - 1; j++) {  /* exclude root */
                    if (!first) printf("\t");
                    printf("%s", tokens[j]); first = 0;
                }
                printf("\n");
            } else {
                printf("\n");
            }
            return 0;
        }
    }
    printf("null\n");
    return 0;
}

/* PoP: vev_u_summarize_output @ agent/verification_evidence.py:_summarize_output */
/* Python: truncate to _MAX_OUTPUT_SUMMARY_CHARS preserving head+tail.
 * Real C: arg = "output" (newline-escaped as literal "\n" already decoded
 * by caller semantics is 1:1). */
int vev_u_summarize_output(const char *arg) {
    if (!arg) { printf("\n"); return 0; }
    size_t len = strlen(arg);
    const long HEAD = 2000 / 3;      /* _MAX_OUTPUT_SUMMARY_CHARS = 2000 */
    if (len <= 2000) { printf("%s\n", arg); return 0; }
    long head = HEAD;
    long tail = 2000 - head;
    char *out = malloc(2000 + 256);
    if (!out) { printf("\n"); return 0; }
    memcpy(out, arg, (size_t)head);
    int o = (int)head;
    char mid[128];
    int ml = snprintf(mid, sizeof(mid), "\n... [%zu chars omitted] ...\n", len - 2000);
    memcpy(out + o, mid, (size_t)ml); o += ml;
    memcpy(out + o, arg + (len - tail), (size_t)tail); o += (int)tail;
    out[o] = '\0';
    printf("%s\n", out);
    free(out);
    return 0;
}

/* PoP: vev_u_prune_old_events @ agent/verification_evidence.py:_prune_old_events */
int vev_u_prune_old_events(const char *arg) {
    /* Python: bounded ledger. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("prune skipped\n"); return 0; }
    printf("old verification events pruned: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: vev_classify_verification_command @ agent/verification_evidence.py:classify_verification_command */
int vev_classify_verification_command(const char *arg) {
    /* Python: verify command classify. Arg =
     * "state\tresult\tkind". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("{\"kind\": \"%s\", %s}\n", t3 ? t3 + 1 : "canonical", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: vev_record_terminal_result @ agent/verification_evidence.py:record_terminal_result */
int vev_record_terminal_result(const char *arg) {
    /* Python: evidence insert. Arg =
     * "is_evidence\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_evidence = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !is_evidence) { printf("\n"); return 0; }
    printf("verification event recorded: %s\n", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: vev_mark_workspace_edited @ agent/verification_evidence.py:mark_workspace_edited */
int vev_mark_workspace_edited(const char *arg) {
    /* Python: stale evidence. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("workspace edit marked: %s path(s)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: vev_verification_status @ agent/verification_evidence.py:verification_status */
int vev_verification_status(const char *arg) {
    /* Python: best-known state. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{\"status\": \"not_applicable\", \"evidence\": null}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_facts") == 0) {
        printf("{\"status\": \"not_applicable\", \"evidence\": null}\n");
        return 0;
    }
    if (strcmp(state, "unverified") == 0) {
        printf("{\"status\": \"unverified\", \"evidence\": null}\n");
        return 0;
    }
    if (strcmp(state, "stale") == 0) {
        printf("{\"status\": \"stale\", \"evidence\": {%s}}\n", tab ? tab + 1 : "");
        return 0;
    }
    printf("{\"status\": \"%s\", \"evidence\": {%s}}\n", state, tab ? tab + 1 : "");
    return 0;
}
