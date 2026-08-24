/*
 * acp_adapter_session.c — canonical C11 port of acp_adapter/session.py
 * (module-level helpers: cwd translation/normalization, session titles,
 * updated-at formatting/sort keys, task-cwd registry).
 *
 * The AcpSession class itself composes the agent core + state DB, both of
 * which exist natively in C (src/gateway/session.c, src/agent/hermes_state/);
 * the helpers here are the pure surface the session object uses.
 */
#define _GNU_SOURCE
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>
#include <libpath/path.h>

/* ── cwd helpers ─────────────────────────────────────────────────────── */

/*
 * Delegates to hermes_constants.translate_cwd_for_wsl_backend — ported in
 * src/hermes_constants.c (WSL backend translation). Native Linux/macOS keeps
 * PoP: acps_translate_acp_cwd @ acp_adapter/session.py:_translate_acp_cwd
 */
static int hcs_is_wsl(void) {
    FILE *f = fopen("/proc/sys/kernel/osrelease", "r");
    if (!f) return 0;
    char buf[256]; int wsl = 0;
    if (fgets(buf, sizeof buf, f)) {
        for (char *p = buf; *p; p++) {
            unsigned char c=(unsigned char)*p;
            if (islower(c)) buf[p-buf]=(char)toupper(c);
        }
        wsl = strstr(buf,"WSL") != NULL || strstr(buf,"MICROSOFT") != NULL;
    }
    fclose(f);
    return wsl;
}

/*
 * Normalize a cross-boundary cwd when Hermes runs inside WSL. No-op off WSL
 * PoP: hc_translate_cwd_for_wsl_backend @ hermes_constants.py:translate_cwd_for_wsl_backend
 */
int hc_translate_cwd_for_wsl_backend(const char *cwd, char *out, size_t out_sz) {
    if (!hcs_is_wsl()) return 0;  /* keep original */
    /* UNC \\wsl.localhost\... -> strip to the POSIX path inside */
    if (strncmp(cwd, "\\\\wsl", 5) == 0 || strncmp(cwd, "//wsl", 5) == 0 ||
        strncmp(cwd, "\\\\wsl.localhost", 15) == 0) {
        const char *rest = strstr(cwd, "\x5cx5c"); /* find inner separator */
        snprintf(out, out_sz, "%s", rest ? rest : cwd);
        for (char *p = out; *p; p++) if (*p == '\\') *p = '/';
        return 1;
    }
    /* Windows drive path E:\X -> /mnt/e/X */
    if ((cwd[0]>='A'&&cwd[0]<='Z') && cwd[1]==':' && (cwd[2]=='\\'||cwd[2]=='/')) {
        snprintf(out, out_sz, "/mnt/%c/%s",
                 (char)tolower((unsigned char)cwd[0]), cwd+3);
        for (char *p = out + 5; *p; p++) if (*p == '\\') *p = '/';
        return 1;
    }
    return 0; /* already POSIX */
}

void acps_translate_acp_cwd(const char *cwd, char *out, size_t out_sz) {
    if (!hc_translate_cwd_for_wsl_backend(cwd ? cwd : "", out, out_sz)) {
        /* no translation applied — keep original */
        snprintf(out, out_sz, "%s", cwd ? cwd : "");
    }
}

/* PoP: acps_normalize_cwd_for_compare @ acp_adapter/session.py:_normalize_cwd_for_compare */
void acps_normalize_cwd_for_compare(const char *cwd, char *out, size_t out_sz) {
    char raw[1024];
    snprintf(raw, sizeof raw, "%s", (cwd && *cwd) ? cwd : ".");
    /* strip */
    char *e = raw + strlen(raw);
    while (e > raw && (e[-1]==' '||e[-1]=='\t')) *--e = '\0';
    char *s2 = raw; while (*s2==' '||*s2=='\t') s2++;
    if (!*s2) strcpy(raw, ".");

    /* expanduser: ~/ -> $HOME/ */
    char expanded[2048];
    if (s2[0]=='~' && s2[1]=='/') {
        const char *home = getenv("HOME");
        snprintf(expanded, sizeof expanded, "%s%s", home?home:"", s2+1);
    } else {
        snprintf(expanded, sizeof expanded, "%s", s2);
    }

    /* Windows drive path -> WSL mount form (E:\X -> /mnt/e/X) */
    if ((expanded[0]>='A'&&expanded[0]<='Z') && expanded[1]==':' &&
        (expanded[2]=='\\'||expanded[2]=='/')) {
        char drive=(char)tolower((unsigned char)expanded[0]);
        snprintf(out, out_sz, "/mnt/%c/%s", drive, expanded+3);
        return;
    }

    /* normalize /mnt/X case */
    if (strncmp(expanded,"/mnt/",5)==0 && expanded[5]>='A' && expanded[5]<='Z') {
        snprintf(out, out_sz, "/mnt/%c/%s", tolower((unsigned char)expanded[5]), expanded+7);
    } else {
        snprintf(out, out_sz, "%s", expanded);
    }
}

/* ── session title ───────────────────────────────────────────────────── */

/* PoP: acps_build_session_title @ acp_adapter/session.py:_build_session_title */
void acps_build_session_title(const char *title, const char *preview,
                              const char *cwd, char *out, size_t out_sz) {
    /* explicit wins */
    const char *t = title ? title : "";
    while (*t==' '||*t=='\t') t++;
    if (*t) { snprintf(out, out_sz, "%s", t); return; }
    /* then preview */
    const char *pv = preview ? preview : "";
    while (*pv==' '||*pv=='\t') pv++;
    if (*pv) { snprintf(out, out_sz, "%s", pv); return; }
    /* then cwd leaf */
    const char *leaf = (cwd && *cwd) ? cwd : "/";
    const char *last = strrchr(leaf, '/');
    last = last ? last+1 : leaf;
    if (!*last || strcmp(last,".")==0 || strcmp(last,"..")==0)
        snprintf(out, out_sz, "New thread");
    else
        snprintf(out, out_sz, "%s", last);
}

/* ── updated-at formatting / sort keys ───────────────────────────────── */

/* PoP: acps_format_updated_at @ acp_adapter/session.py:_format_updated_at */
void acps_format_updated_at(const char *value, char *out, size_t out_sz) {
    if (!value || !*value) { out[0]='\0'; return; }
    /* numeric epoch -> ISO 8601 UTC; string passes through */
    int numeric = 1;
    for (const char *p=value; *p; p++)
        if ((*p<'0'||*p>'9') && *p!='.' && *p!='-') { numeric=0; break; }
    if (numeric) {
        double v = atof(value);
        time_t t = (time_t)v;
        struct tm tm_v;
        gmtime_r(&t, &tm_v);
        strftime(out, out_sz, "%Y-%m-%dT%H:%M:%SZ", &tm_v);
    } else {
        snprintf(out, out_sz, "%s", value);
    }
}

/* PoP: acps_updated_at_sort_key @ acp_adapter/session.py:_updated_at_sort_key */
double acps_updated_at_sort_key(const char *value) {
    if (!value || !*value) return -1.0/0.0; /* -inf */
    /* numeric? */
    int numeric = 1;
    for (const char *p=value; *p; p++)
        if ((*p<'0'||*p>'9') && *p!='.' && *p!='-' && *p!='+') { numeric=0; break; }
    if (numeric) return atof(value);
    /* ISO 8601 */
    struct tm tm_v;
    memset(&tm_v,0,sizeof tm_v);
    char buf[64]; snprintf(buf,sizeof buf,"%s",value);
    char *z=strchr(buf,'Z'); if(z)*z=' ';
    if (strptime(buf,"%Y-%m-%dT%H:%M:%S",&tm_v)) {
        return (double)timegm(&tm_v);
    }
    return -1.0/0.0;
}

/* ── task-cwd registry ───────────────────────────────────────────────── */

static pthread_mutex_t g_task_cwd_lock = PTHREAD_MUTEX_INITIALIZER;
#define ACPS_MAX_TASKS 256
static struct { char id[64]; char cwd[1024]; } g_task_cwds[ACPS_MAX_TASKS];
static int g_task_cwd_n = 0;

/* PoP: acps_register_task_cwd @ acp_adapter/session.py:_register_task_cwd */
void acps_register_task_cwd(const char *task_id, const char *cwd) {
    pthread_mutex_lock(&g_task_cwd_lock);
    for (int i = 0; i < g_task_cwd_n; i++) {
        if (strcmp(g_task_cwds[i].id, task_id) == 0) {
            snprintf(g_task_cwds[i].cwd, sizeof g_task_cwds[i].cwd, "%s",
                     cwd ? cwd : "");
            pthread_mutex_unlock(&g_task_cwd_lock);
            return;
        }
    }
    if (g_task_cwd_n < ACPS_MAX_TASKS) {
        snprintf(g_task_cwds[g_task_cwd_n].id,  64,   "%s", task_id?task_id:"");
        snprintf(g_task_cwds[g_task_cwd_n].cwd, 1024, "%s", cwd?cwd:"");
        g_task_cwd_n++;
    }
    pthread_mutex_unlock(&g_task_cwd_lock);
}

/* PoP: acps_clear_task_cwd @ acp_adapter/session.py:_clear_task_cwd */
void acps_clear_task_cwd(const char *task_id) {
    pthread_mutex_lock(&g_task_cwd_lock);
    for (int i = 0; i < g_task_cwd_n; i++) {
        if (strcmp(g_task_cwds[i].id, task_id) == 0) {
            memmove(&g_task_cwds[i], &g_task_cwds[i+1],
                    sizeof(g_task_cwds[0]) * (size_t)(g_task_cwd_n - i - 1));
            g_task_cwd_n--;
            break;
        }
    }
    pthread_mutex_unlock(&g_task_cwd_lock);
}
