/*
 * port_cli_logs.c — Port of Python: hermes_cli/logs.py
 *
 * ``hermes logs`` — view and filter Hermes log files. All pure (file read +
 * regex + filtering + tail); no network, no external backends.
 *
 * Ownership: plain C strings (char*) are returned for some helpers; callers
 * free with free(). This module uses the POSIX-ERE regex wrapper
 * (lib/libregex/hermes_regex.h) — NOT PCRE (no \s/\S/?:/\d), so all patterns
 * below are written in POSIX-safe form.
 *
 * PoP mappings:
 *  _parse_since          -> cli_logs_parse_since
 *  _parse_line_timestamp -> cli_logs_parse_line_timestamp
 *  _extract_level        -> cli_logs_extract_level
 *  _extract_logger_name  -> cli_logs_extract_logger_name
 *  _line_matches_component -> cli_logs_line_matches_component
 *  _matches_filters      -> cli_logs_matches_filters
 *  tail_log              -> cli_logs_tail
 *  _read_tail            -> cli_logs_read_tail
 *  _read_last_n_lines    -> cli_logs_read_last_n_lines
 *  _follow_log           -> cli_logs_follow
 *  list_logs             -> cli_logs_list
 */

#define _GNU_SOURCE
#include "hermes_json.h"  /* not used, keeps include style */
#include "hermes_regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

/* forward declaration (defined below cli_logs_tail) */
void cli_logs_follow(const char *path, const char *min_level,
                     const char *session_filter, time_t since,
                     const char *comp_prefixes[], int ncomp);

/* ---- Hermes home resolution (mirrors hermes_constants.get_hermes_home) ---- */
static const char *cli_logs_home(void)
{
    const char *h = getenv("HERMES_HOME");
    if (h && h[0]) return h;
    static char buf[4096];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sizeof(buf), "%s/.hermes", home);
    return buf;
}

static const char *cli_logs_display_home(void)
{
    return cli_logs_home();
}

/* ---- Known log files (name -> filename) ---- */
typedef struct { const char *name; const char *file; } log_file_t;
static const log_file_t LOG_FILES[] = {
    {"agent",    "agent.log"},
    {"errors",   "errors.log"},
    {"gateway",  "gateway.log"},
    {"gui",      "gui.log"},
    {"desktop",  "desktop.log"},
};
static const int LOG_FILES_N = sizeof(LOG_FILES) / sizeof(LOG_FILES[0]);

/* ---- Level ordering for >= filtering ---- */
static const char *LEVELS[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
static int level_order(const char *lvl)
{
    for (int i = 0; i < 5; i++)
        if (strcasecmp(LEVELS[i], lvl) == 0) return i;
    return 0;
}

/* ---- Component -> logger-name prefixes (hermes_logging.COMPONENT_PREFIXES) ---- */
typedef struct { const char *comp; const char **prefixes; } comp_prefix_t;
static const char *P_GATEWAY[] = {"gateway", "hermes_plugins", "plugins.platforms", NULL};
static const char *P_AGENT[]   = {"agent", "run_agent", "model_tools", "batch_runner", NULL};
static const char *P_TOOLS[]   = {"tools", NULL};
static const char *P_CLI[]     = {"hermes_cli", "cli", NULL};
static const char *P_CRON[]    = {"cron", NULL};
static const char *P_GUI[]     = {"hermes_cli.web_server", "hermes_cli.pty_bridge",
                                   "tui_gateway", "uvicorn", NULL};
static const comp_prefix_t COMPONENT_PREFIXES[] = {
    {"gateway", P_GATEWAY},
    {"agent",   P_AGENT},
    {"tools",   P_TOOLS},
    {"cli",     P_CLI},
    {"cron",    P_CRON},
    {"gui",     P_GUI},
};
static const int COMPONENT_PREFIXES_N = sizeof(COMPONENT_PREFIXES)/sizeof(COMPONENT_PREFIXES[0]);

/* ---- Regexes (POSIX ERE — no \s/\S/?:/\d) ---- */
static hregex_t *re_since;        /* ^([0-9]+)[[:space:]]*([smhd])$ */
static hregex_t *re_ts;           /* ^([0-9]{4}-[0-9]{2}-[0-9]{2}[[:space:]]+[0-9]{2}:[0-9]{2}:[0-9]{2}) */
static hregex_t *re_level;        /* [[:space:]](DEBUG|INFO|WARNING|ERROR|CRITICAL)[[:space:]] */
static hregex_t *re_logger;       /* [[:space:]](...level...)([[:space:]]+\[[^]]*\])?[[:space:]]+([^[:space:]]+): */

static void ensure_regexes(void)
{
    if (re_since) return;
    re_since  = regex_compile("^([0-9]+)[[:space:]]*([smhd])$", 0);
    re_ts     = regex_compile("^([0-9]{4}-[0-9]{2}-[0-9]{2}[[:space:]]+[0-9]{2}:[0-9]{2}:[0-9]{2})", 0);
    re_level  = regex_compile("[[:space:]](DEBUG|INFO|WARNING|ERROR|CRITICAL)[[:space:]]", 0);
    re_logger = regex_compile("[[:space:]](DEBUG|INFO|WARNING|ERROR|CRITICAL)([[:space:]]+\\[[^]]*\\])?[[:space:]]+([^[:space:]]+):", 0);
}

/* ============================================================
 * Pure helpers
 * ============================================================ */

/* PoP: cli_logs_parse_since @ hermes_cli/logs.py:_parse_since */
/* Parse '1h'/'30m'/'2d' into a UNIX cutoff (now - delta). NULL if unparseable. */
time_t cli_logs_parse_since(const char *since_str)
{
    ensure_regexes();
    if (!since_str) return (time_t)-1;
    char *s = strdup(since_str);
    for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
    regex_match_t *m = regex_search(re_since, s);
    free(s);
    if (!m || !m->matched) { regex_match_free(m); return (time_t)-1; }
    if (!m->groups[1] || !m->groups[2]) { regex_match_free(m); return (time_t)-1; }
    int value = atoi(m->groups[1]);
    const char *unit = m->groups[2];
    long delta = 0;
    if (unit[0] == 's') delta = value;
    else if (unit[0] == 'm') delta = value * 60L;
    else if (unit[0] == 'h') delta = value * 3600L;
    else if (unit[0] == 'd') delta = value * 86400L;
    else { regex_match_free(m); return (time_t)-1; }
    regex_match_free(m);
    return time(NULL) - delta;
}

/* PoP: cli_logs_parse_line_timestamp @ hermes_cli/logs.py:_parse_line_timestamp */
/* Extract "2026-04-05 22:35:00" from a line -> UNIX time, or -1 if none. */
time_t cli_logs_parse_line_timestamp(const char *line)
{
    ensure_regexes();
    regex_match_t *m = regex_search(re_ts, line);
    if (!m || !m->matched) { regex_match_free(m); return (time_t)-1; }
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (strptime(m->groups[1], "%Y-%m-%d %H:%M:%S", &tm) == NULL) {
        regex_match_free(m); return (time_t)-1;
    }
    regex_match_free(m);
    return timegm(&tm);
}

/* PoP: cli_logs_extract_level @ hermes_cli/logs.py:_extract_level */
/* Returns malloc'd level string or NULL. Caller frees. */
char *cli_logs_extract_level(const char *line)
{
    ensure_regexes();
    regex_match_t *m = regex_search(re_level, line);
    if (!m || !m->matched) { regex_match_free(m); return NULL; }
    char *r = strdup(m->groups[1]);
    regex_match_free(m);
    return r;
}

/* PoP: cli_logs_extract_logger_name @ hermes_cli/logs.py:_extract_logger_name */
/* Returns malloc'd logger name or NULL. Caller frees. */
char *cli_logs_extract_logger_name(const char *line)
{
    ensure_regexes();
    regex_match_t *m = regex_search(re_logger, line);
    if (!m || !m->matched) { regex_match_free(m); return NULL; }
    /* groups[1]=level, groups[2]=optional session tag, groups[3]=logger name */
    char *r = m->groups[3] ? strdup(m->groups[3]) : NULL;
    regex_match_free(m);
    return r;
}

/* PoP: cli_logs_line_matches_component @ hermes_cli/logs.py:_line_matches_component */
int cli_logs_line_matches_component(const char *line, const char *prefixes[], int nprefix)
{
    char *name = cli_logs_extract_logger_name(line);
    if (!name) return 0;
    int ok = 0;
    for (int i = 0; i < nprefix; i++) {
        if (strncmp(name, prefixes[i], strlen(prefixes[i])) == 0) { ok = 1; break; }
    }
    free(name);
    return ok;
}

/* PoP: cli_logs_matches_filters @ hermes_cli/logs.py:_matches_filters */
int cli_logs_matches_filters(const char *line,
                             const char *min_level,
                             const char *session_filter,
                             time_t since,
                             const char *component_prefixes[], int ncomp)
{
    if (since != (time_t)-1) {
        time_t ts = cli_logs_parse_line_timestamp(line);
        if (ts != (time_t)-1 && ts < since) return 0;
    }
    if (min_level) {
        char *lvl = cli_logs_extract_level(line);
        if (lvl) {
            if (level_order(lvl) < level_order(min_level)) { free(lvl); return 0; }
            free(lvl);
        }
    }
    if (session_filter) {
        if (strstr(line, session_filter) == NULL) return 0;
    }
    if (component_prefixes && ncomp > 0) {
        if (!cli_logs_line_matches_component(line, component_prefixes, ncomp)) return 0;
    }
    return 1;
}

/* ============================================================
 * File reading
 * ============================================================ */

/* PoP: cli_logs_read_last_n_lines @ hermes_cli/logs.py:_read_last_n_lines */
/* Read the last n lines from a file. Returns malloc'd array of malloc'd
 * line strings (each includes trailing \n), count in *out_n. NULL on error. */
char **cli_logs_read_last_n_lines(const char *path, int n, int *out_n)
{
    *out_n = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    struct stat st;
    if (fstat(fileno(f), &st) != 0) { fclose(f); return NULL; }
    if (st.st_size == 0) { fclose(f); return NULL; }

    char **lines = NULL;
    int cap = 0, cnt = 0;

    if (st.st_size <= 1048576) {
        /* Small file: read whole thing. */
        char *buf = malloc(st.st_size + 1);
        size_t rd = fread(buf, 1, st.st_size, f);
        buf[rd] = 0;
        char *save = NULL;
        char *tok = strtok_r(buf, "\n", &save);
        while (tok) {
            char *l = malloc(strlen(tok) + 2);
            sprintf(l, "%s\n", tok);
            if (cnt >= cap) { cap = cap ? cap * 2 : 64; lines = realloc(lines, cap * sizeof(char*)); }
            lines[cnt++] = l;
            tok = strtok_r(NULL, "\n", &save);
        }
        free(buf);
    } else {
        /* Large file: read whole file into memory (correct; the Python
         * chunked path is a perf optimization only — behavior is identical
         * for the last-N-lines contract we verify). */
        char *buf = malloc(st.st_size + 1);
        size_t rd = fread(buf, 1, st.st_size, f);
        buf[rd] = 0;
        char *save = NULL;
        char *tok = strtok_r(buf, "\n", &save);
        while (tok) {
            char *l = malloc(strlen(tok) + 2);
            sprintf(l, "%s\n", tok);
            if (cnt >= cap) { cap = cap ? cap * 2 : 64; lines = realloc(lines, cap * sizeof(char*)); }
            lines[cnt++] = l;
            tok = strtok_r(NULL, "\n", &save);
        }
        free(buf);
    }
    fclose(f);
    if (cnt > n) {
        int drop = cnt - n;
        for (int i = 0; i < drop; i++) free(lines[i]);
        memmove(lines, lines + drop, n * sizeof(char*));
        cnt = n;
    }
    *out_n = cnt;
    return lines;
}

/* PoP: cli_logs_read_tail @ hermes_cli/logs.py:_read_tail */
/* Returns last num_lines matching lines (or raw last num_lines). */
char **cli_logs_read_tail(const char *path, int num_lines, int has_filters,
                          const char *min_level, const char *session_filter,
                          time_t since, const char *comp_prefixes[], int ncomp,
                          int *out_n)
{
    *out_n = 0;
    if (has_filters) {
        int raw_n = 0;
        int raw_cap = num_lines * 20 > 2000 ? num_lines * 20 : 2000;
        char **raw = cli_logs_read_last_n_lines(path, raw_cap, &raw_n);
        char **out = NULL; int oc = 0, ocap = 0;
        for (int i = 0; i < raw_n; i++) {
            if (cli_logs_matches_filters(raw[i], min_level, session_filter, since, comp_prefixes, ncomp)) {
                if (oc >= ocap) { ocap = ocap ? ocap*2 : 64; out = realloc(out, ocap*sizeof(char*)); }
                out[oc++] = raw[i];
            } else {
                free(raw[i]);
            }
        }
        free(raw);
        /* keep last num_lines */
        if (oc > num_lines) {
            int drop = oc - num_lines;
            for (int i = 0; i < drop; i++) free(out[i]);
            memmove(out, out + drop, num_lines * sizeof(char*));
            oc = num_lines;
        }
        *out_n = oc;
        return out;
    }
    return cli_logs_read_last_n_lines(path, num_lines, out_n);
}

/* ============================================================
 * Public commands (mirror tail_log / _follow_log / list_logs)
 * ============================================================ */

/* PoP: cli_logs_tail @ hermes_cli/logs.py:tail_log */
void cli_logs_tail(const char *log_name, int num_lines, int follow,
                   const char *level, const char *session,
                   const char *since, const char *component)
{
    const char *filename = NULL;
    for (int i = 0; i < LOG_FILES_N; i++)
        if (strcmp(LOG_FILES[i].name, log_name) == 0) { filename = LOG_FILES[i].file; break; }
    if (!filename) {
        printf("Unknown log: '%s'. Available:", log_name);
        for (int i = 0; i < LOG_FILES_N; i++) printf(" %s", LOG_FILES[i].name);
        printf("\n");
        exit(1);
    }
    char path[4096];
    snprintf(path, sizeof(path), "%s/logs/%s", cli_logs_home(), filename);
    if (access(path, F_OK) != 0) {
        printf("Log file not found: %s\n", path);
        printf("(Logs are created when Hermes runs — try 'hermes chat' first)\n");
        exit(1);
    }
    time_t since_dt = (time_t)-1;
    if (since) { since_dt = cli_logs_parse_since(since);
        if (since_dt == (time_t)-1) { printf("Invalid --since value: '%s'. Use format like '1h', '30m', '2d'.\n", since); exit(1); } }
    const char *min_level = NULL;
    char ml_buf[16] = {0};
    if (level) { for (int i=0; level[i] && i<15; i++) ml_buf[i] = (char)toupper((unsigned char)level[i]); min_level = ml_buf; }
    const char *comp_prefixes[16]; int ncomp = 0;
    if (component) {
        int found = 0;
        for (int i = 0; i < COMPONENT_PREFIXES_N; i++) {
            if (strcasecmp(COMPONENT_PREFIXES[i].comp, component) == 0) {
                for (int j = 0; COMPONENT_PREFIXES[i].prefixes[j]; j++)
                    comp_prefixes[ncomp++] = COMPONENT_PREFIXES[i].prefixes[j];
                found = 1; break;
            }
        }
        if (!found) {
            printf("Unknown component: '%s'. Available:", component);
            for (int i = 0; i < COMPONENT_PREFIXES_N; i++) printf(" %s", COMPONENT_PREFIXES[i].comp);
            printf("\n"); exit(1);
        }
    }
    int has_filters = (min_level != NULL) || (session != NULL) || (since_dt != (time_t)-1) || (ncomp > 0);
    int n = 0;
    char **lines = cli_logs_read_tail(path, num_lines, has_filters, min_level, session,
                                      since_dt, comp_prefixes, ncomp, &n);
    /* header */
    char desc[256] = {0}; int dp = 0;
    if (min_level) dp += snprintf(desc + dp, sizeof(desc)-dp, "level>=%s", min_level);
    if (session) dp += snprintf(desc + dp, sizeof(desc)-dp, "%s session=%s", dp?",":"", session);
    if (component) dp += snprintf(desc + dp, sizeof(desc)-dp, "%s component=%s", dp?",":"", component);
    if (since) dp += snprintf(desc + dp, sizeof(desc)-dp, "%s since=%s", dp?",":"", since);
    if (follow)
        printf("--- %s/logs/%s%s (Ctrl+C to stop) ---\n", cli_logs_display_home(), filename, desc);
    else
        printf("--- %s/logs/%s%s (last %d) ---\n", cli_logs_display_home(), filename, desc, num_lines);
    for (int i = 0; i < n; i++) { fputs(lines[i], stdout); free(lines[i]); }
    free(lines);
    if (!follow) return;
    cli_logs_follow(path, min_level, session, since_dt, comp_prefixes, ncomp);
}

/* PoP: cli_logs_follow @ hermes_cli/logs.py:_follow_log */
void cli_logs_follow(const char *path, const char *min_level,
                     const char *session_filter, time_t since,
                     const char *comp_prefixes[], int ncomp)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    char *line = NULL; size_t cap = 0;
    while (1) {
        ssize_t len = getline(&line, &cap, f);
        if (len > 0) {
            if (cli_logs_matches_filters(line, min_level, session_filter, since, comp_prefixes, ncomp)) {
                fputs(line, stdout); fflush(stdout);
            }
        } else {
            usleep(300000);
            /* re-check EOF position for appended data */
            struct stat st;
            if (fstat(fileno(f), &st) == 0 && st.st_size < ftell(f)) fseek(f, 0, SEEK_SET);
        }
    }
    free(line);
    fclose(f);
}

/* PoP: cli_logs_list @ hermes_cli/logs.py:list_logs */
void cli_logs_list(void)
{
    char dir[4096];
    snprintf(dir, sizeof(dir), "%s/logs", cli_logs_home());
    DIR *d = opendir(dir);
    if (!d) { printf("No logs directory at %s/logs/\n", cli_logs_display_home()); return; }
    printf("Log files in %s/logs/:\n\n", cli_logs_display_home());
    int found = 0;
    struct dirent *e;
    /* collect entries for sorted output */
    char names[128][256]; int nn = 0;
    while ((e = readdir(d)) != NULL) {
        if (e->d_type != DT_REG) continue;
        size_t L = strlen(e->d_name);
        if (L < 4 || strcmp(e->d_name + L - 4, ".log") != 0) continue;
        if (nn < 128) strncpy(names[nn++], e->d_name, 255);
    }
    closedir(d);
    /* simple bubble sort by name */
    for (int i = 0; i < nn; i++)
        for (int j = i+1; j < nn; j++)
            if (strcmp(names[i], names[j]) > 0) { char t[256]; strcpy(t,names[i]); strcpy(names[i],names[j]); strcpy(names[j],t); }
    time_t now = time(NULL);
    for (int i = 0; i < nn; i++) {
        char p[4352];
        snprintf(p, sizeof(p), "%s/%s", dir, names[i]);
        struct stat st;
        if (stat(p, &st) != 0) continue;
        long size = st.st_size;
        char size_str[32];
        if (size < 1024) snprintf(size_str, sizeof(size_str), "%ldB", size);
        else if (size < 1048576) snprintf(size_str, sizeof(size_str), "%.1fKB", size/1024.0);
        else snprintf(size_str, sizeof(size_str), "%.1fMB", size/1048576.0);
        double age = difftime(now, st.st_mtime);
        char age_str[32];
        if (age < 60) strcpy(age_str, "just now");
        else if (age < 3600) snprintf(age_str, sizeof(age_str), "%dm ago", (int)(age/60));
        else if (age < 86400) snprintf(age_str, sizeof(age_str), "%dh ago", (int)(age/3600));
        else { struct tm tm; localtime_r(&st.st_mtime, &tm); strftime(age_str, sizeof(age_str), "%Y-%m-%d", &tm); }
        printf("  %-25s %8s   %s\n", names[i], size_str, age_str);
        found = 1;
    }
    if (!found) printf("  (no .log files)\n");
}
