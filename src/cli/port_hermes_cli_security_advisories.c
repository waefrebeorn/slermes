/*
 * port_hermes_cli_security_advisories.c — C port of hermes_cli/security_advisories.py
 *
 * Detects known-compromised Python packages installed in the active venv
 * (supply-chain attacks like the Mini Shai-Hulud worm of May 2026 that
 * poisoned mistralai 2.4.6 on PyPI) and surfaces remediation guidance.
 *
 * The Python original uses importlib.metadata.version(); the faithful C
 * equivalent resolves the active environment's site-packages directories
 * (VIRTUAL_ENV, sys.path-equivalent prefixes) and reads the Version: field
 * from <pkg>-<ver>.dist-info/METADATA — the same source of truth
 * importlib.metadata itself reads.
 *
 * Acks live under security.acked_advisories in config.yaml (list of IDs),
 * persisted via the ported config.py io layer.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_core_types.h"   /* hermes_get_home */

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* config.py io layer (ported) */
extern json_t *config_py_read_raw_config(void);
extern void    config_py_get_config_path(char *buf, size_t sz);
extern int     config_py_atomic_config_write(const char *config_path, const json_t *data);

#define BANNER_REPEAT_HOURS 24
#define BANNER_CACHE_FILE "advisory_banner_seen"

/* ============================================================
 * Advisory catalog (faithful to ADVISORIES tuple)
 * ============================================================ */

typedef struct {
    const char *pkg;
    /* NULL-terminated list of compromised version strings.
     * Empty list ({NULL}) means "any installed version is suspect". */
    const char *versions[8];
} advisory_pkg_t;

typedef struct {
    const char *id;
    const char *title;
    const char *summary;
    const char *url;
    advisory_pkg_t compromised[4];   /* terminated by .pkg == NULL */
    const char *remediation[8];      /* NULL-terminated */
    const char *published;
    const char *severity;
} advisory_t;

static const advisory_t ADVISORIES[] = {
    {
        .id = "shai-hulud-2026-05",
        .title = "Mini Shai-Hulud worm — mistralai 2.4.6 compromised on PyPI",
        .summary =
            "PyPI quarantined the mistralai package on 2026-05-12 after a "
            "malicious 2.4.6 release. The worm steals credentials from "
            "environment variables and credential files (~/.npmrc, ~/.pypirc, "
            "~/.aws/credentials, GitHub PATs, cloud SDK tokens) and exfils "
            "them to a hardcoded webhook. If you ran any Python process that "
            "imported mistralai 2.4.6 — including hermes when configured "
            "with provider=mistral for TTS or STT — assume those credentials "
            "are exposed. PyPI has since removed 2.4.6 and the project ships "
            "clean releases again (2.4.7, 2.4.8); this advisory only fires if "
            "the compromised 2.4.6 is still installed.",
        .url = "https://socket.dev/blog/mini-shai-hulud-worm-pypi",
        .compromised = {
            { .pkg = "mistralai", .versions = { "2.4.6", NULL } },
            { .pkg = NULL },
        },
        .remediation = {
            "Run: pip uninstall -y mistralai  (or: uv pip uninstall mistralai)",
            "Rotate API keys in ~/.hermes/.env (OpenRouter, Anthropic, OpenAI, "
            "Nous, GitHub, AWS, Google, Mistral, etc.).",
            "Audit ~/.npmrc, ~/.pypirc, ~/.aws/credentials, ~/.config/gh/hosts.yml, "
            "and any other credential files for tokens that may have been read.",
            "Check GitHub for unexpected new SSH keys, deploy keys, or webhook "
            "additions on repos you have admin on.",
            "After cleanup: hermes doctor --ack shai-hulud-2026-05  to dismiss "
            "this warning.",
            NULL,
        },
        .published = "2026-05-12",
        .severity = "critical",
    },
};
static const size_t N_ADVISORIES = sizeof(ADVISORIES) / sizeof(ADVISORIES[0]);

/* ============================================================
 * _installed_version — dist-info metadata scan
 * ============================================================ */

/* Normalize a package name per PEP 503 for dist-info dirname comparison:
 * lowercase, runs of -_. collapse to a single '-' (dist-info dirs use '_'
 * in place of '-', so compare with both folded to one canonical char). */
static void pep503_normalize(const char *name, char *out, size_t out_sz) {
    size_t j = 0;
    bool prev_sep = false;
    for (size_t i = 0; name[i] && j + 1 < out_sz; i++) {
        char ch = name[i];
        if (ch == '-' || ch == '_' || ch == '.') {
            if (!prev_sep) { out[j++] = '-'; prev_sep = true; }
        } else {
            out[j++] = (char)tolower((unsigned char)ch);
            prev_sep = false;
        }
    }
    out[j] = '\0';
}

/* Read "Version: x.y.z" from a dist-info METADATA file. */
static bool read_metadata_version(const char *metadata_path,
                                  char *out, size_t out_sz) {
    FILE *f = fopen(metadata_path, "r");
    if (!f) return false;
    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        /* Metadata headers end at the first blank line */
        if (line[0] == '\n' || line[0] == '\r') break;
        if (strncasecmp(line, "Version:", 8) == 0) {
            const char *v = line + 8;
            while (*v == ' ' || *v == '\t') v++;
            size_t len = strcspn(v, "\r\n");
            if (len >= out_sz) len = out_sz - 1;
            memcpy(out, v, len);
            out[len] = '\0';
            found = out[0] != '\0';
            break;
        }
    }
    fclose(f);
    return found;
}

/* Scan one site-packages dir for <pkg>-<ver>.dist-info. */
static bool scan_site_packages(const char *sp_dir, const char *norm_pkg,
                               char *out, size_t out_sz) {
    DIR *d = opendir(sp_dir);
    if (!d) return false;
    bool found = false;
    struct dirent *de;
    while (!found && (de = readdir(d)) != NULL) {
        const char *name = de->d_name;
        size_t nlen = strlen(name);
        const char *suffix = ".dist-info";
        size_t slen = strlen(suffix);
        if (nlen <= slen || strcmp(name + nlen - slen, suffix) != 0)
            continue;
        /* dirname is "<dist>-<version>.dist-info"; split at the last '-'
         * before the suffix. */
        char stem[512];
        size_t stem_len = nlen - slen;
        if (stem_len >= sizeof(stem)) continue;
        memcpy(stem, name, stem_len);
        stem[stem_len] = '\0';
        char *dash = strrchr(stem, '-');
        if (!dash || dash == stem) continue;
        *dash = '\0';
        char norm_stem[512];
        pep503_normalize(stem, norm_stem, sizeof(norm_stem));
        if (strcmp(norm_stem, norm_pkg) != 0) continue;
        /* Prefer the METADATA Version: field (authoritative); fall back to
         * the dirname version segment. */
        char metadata_path[HERMES_PATH_MAX];
        snprintf(metadata_path, sizeof(metadata_path), "%s/%s/METADATA",
                 sp_dir, name);
        if (read_metadata_version(metadata_path, out, out_sz)) {
            found = true;
        } else {
            snprintf(out, out_sz, "%s", dash + 1);
            found = out[0] != '\0';
        }
    }
    closedir(d);
    return found;
}

/* Collect candidate site-packages roots for the active environment.
 * Mirrors what importlib.metadata sees on sys.path:
 *   1. $VIRTUAL_ENV/lib/python3.N/site-packages   (active venv)
 *   2. dirs listed in $PYTHONPATH                  (rare but honored)
 * A prefix's lib/ may contain several python3.N dirs; scan them all. */
static int collect_site_packages(char roots[][HERMES_PATH_MAX], int max_roots) {
    int n = 0;
    const char *venv = getenv("VIRTUAL_ENV");
    if (venv && *venv) {
        char libdir[HERMES_PATH_MAX];
        snprintf(libdir, sizeof(libdir), "%s/lib", venv);
        DIR *d = opendir(libdir);
        if (d) {
            struct dirent *de;
            while (n < max_roots && (de = readdir(d)) != NULL) {
                if (strncmp(de->d_name, "python3", 7) != 0) continue;
                snprintf(roots[n], HERMES_PATH_MAX, "%s/%s/site-packages",
                         libdir, de->d_name);
                struct stat st;
                if (stat(roots[n], &st) == 0 && S_ISDIR(st.st_mode)) n++;
            }
            closedir(d);
        }
    }
    const char *pythonpath = getenv("PYTHONPATH");
    if (pythonpath && *pythonpath) {
        char buf[4096];
        snprintf(buf, sizeof(buf), "%s", pythonpath);
        char *save = NULL;
        for (char *tok = strtok_r(buf, ":", &save);
             tok && n < max_roots;
             tok = strtok_r(NULL, ":", &save)) {
            struct stat st;
            if (stat(tok, &st) == 0 && S_ISDIR(st.st_mode)) {
                snprintf(roots[n], HERMES_PATH_MAX, "%s", tok);
                n++;
            }
        }
    }
    return n;
}

/* PoP: cli_security_advisories_installed_version @ hermes_cli/security_advisories.py:_installed_version */
/* Return true and fill out with the installed version of pkg_name, or
 * false if not installed. Never raises — metadata corruption is treated
 * as "not installed", same as the Python except-all. */
bool cli_security_advisories_installed_version(const char *pkg_name,
                                               char *out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    out[0] = '\0';
    if (!pkg_name || !*pkg_name) return false;

    char norm_pkg[512];
    pep503_normalize(pkg_name, norm_pkg, sizeof(norm_pkg));

    char roots[16][HERMES_PATH_MAX];
    int nroots = collect_site_packages(roots, 16);
    for (int i = 0; i < nroots; i++) {
        if (scan_site_packages(roots[i], norm_pkg, out, out_sz))
            return true;
    }
    return false;
}

/* ============================================================
 * detect_compromised
 * ============================================================ */

/* Build one AdvisoryHit JSON object. */
static json_t *make_hit(const advisory_t *a, const char *pkg,
                        const char *installed) {
    json_t *h = json_object();
    json_set(h, "advisory_id", json_string(a->id));
    json_set(h, "title", json_string(a->title));
    json_set(h, "summary", json_string(a->summary));
    json_set(h, "url", json_string(a->url));
    json_set(h, "severity", json_string(a->severity));
    json_set(h, "published", json_string(a->published));
    json_set(h, "package", json_string(pkg));
    json_set(h, "installed_version", json_string(installed));
    json_t *rem = json_array();
    for (int i = 0; a->remediation[i]; i++)
        json_append(rem, json_string(a->remediation[i]));
    json_set(h, "remediation", rem);
    return h;
}

/* PoP: cli_security_advisories_detect_compromised @ hermes_cli/security_advisories.py:detect_compromised */
/* Scan installed packages against the advisory catalog. Returns a JSON
 * array of hit objects (caller frees). A hit means the package is
 * installed AND its version is in the compromised set (or the set is
 * empty, meaning any version is suspect). */
json_t *cli_security_advisories_detect_compromised(void) {
    json_t *hits = json_array();
    for (size_t ai = 0; ai < N_ADVISORIES; ai++) {
        const advisory_t *a = &ADVISORIES[ai];
        for (int pi = 0; a->compromised[pi].pkg; pi++) {
            const advisory_pkg_t *cp = &a->compromised[pi];
            char installed[128];
            if (!cli_security_advisories_installed_version(
                    cp->pkg, installed, sizeof(installed)))
                continue;
            bool empty_set = (cp->versions[0] == NULL);
            bool match = empty_set;
            for (int vi = 0; !match && cp->versions[vi]; vi++)
                if (strcmp(installed, cp->versions[vi]) == 0) match = true;
            if (match)
                json_append(hits, make_hit(a, cp->pkg, installed));
        }
    }
    return hits;
}

/* ============================================================
 * Ack persistence (config.security.acked_advisories)
 * ============================================================ */

/* PoP: cli_security_advisories_get_acked_ids @ hermes_cli/security_advisories.py:get_acked_ids */
/* Returns a JSON array of acked advisory ID strings (caller frees).
 * Empty array if config can't be loaded — don't block startup just
 * because config is broken. */
json_t *cli_security_advisories_get_acked_ids(void) {
    json_t *out = json_array();
    json_t *cfg = config_py_read_raw_config();
    if (!cfg) return out;
    json_t *sec = json_obj_get(cfg, "security");
    json_t *raw = (sec && sec->type == JSON_OBJECT)
                      ? json_obj_get(sec, "acked_advisories") : NULL;
    if (raw && raw->type == JSON_ARRAY) {
        for (size_t i = 0; i < raw->c.count; i++) {
            json_t *it = raw->c.items[i];
            if (!it || it->type != JSON_STRING || !it->str_val) continue;
            /* strip() then skip empties, same as the Python set-builder */
            const char *s = it->str_val;
            while (*s == ' ' || *s == '\t') s++;
            size_t len = strlen(s);
            while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t')) len--;
            if (len == 0) continue;
            char tmp[256];
            if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
            memcpy(tmp, s, len);
            tmp[len] = '\0';
            json_append(out, json_string(tmp));
        }
    }
    json_free(cfg);
    return out;
}

/* PoP: cli_security_advisories_ack_advisory @ hermes_cli/security_advisories.py:ack_advisory */
/* Persist an ack for advisory_id. Returns true on success. Idempotent. */
bool cli_security_advisories_ack_advisory(const char *advisory_id) {
    if (!advisory_id) return false;
    while (*advisory_id == ' ' || *advisory_id == '\t') advisory_id++;
    char trimmed[256];
    snprintf(trimmed, sizeof(trimmed), "%s", advisory_id);
    for (size_t len = strlen(trimmed);
         len > 0 && (trimmed[len-1] == ' ' || trimmed[len-1] == '\t');
         len--)
        trimmed[len-1] = '\0';
    if (!trimmed[0]) return false;

    json_t *cfg = config_py_read_raw_config();
    if (!cfg) {
        hermes_log(LOG_WARNING, "security",
                   "Could not load config to persist ack");
        return false;
    }
    json_t *sec = json_obj_get(cfg, "security");
    if (!sec || sec->type != JSON_OBJECT) {
        sec = json_object();
        json_set(cfg, "security", sec);
    }
    json_t *existing = json_obj_get(sec, "acked_advisories");
    if (!existing || existing->type != JSON_ARRAY) {
        existing = json_array();
        json_set(sec, "acked_advisories", existing);
    }
    bool present = false;
    for (size_t i = 0; !present && i < existing->c.count; i++) {
        json_t *it = existing->c.items[i];
        if (it && it->type == JSON_STRING && it->str_val &&
            strcmp(it->str_val, trimmed) == 0)
            present = true;
    }
    bool ok = true;
    if (!present) {
        json_append(existing, json_string(trimmed));
        char path[HERMES_PATH_MAX];
        config_py_get_config_path(path, sizeof(path));
        ok = (config_py_atomic_config_write(path, cfg) == 0);
        if (!ok)
            hermes_log(LOG_ERROR, "security",
                       "Failed to persist advisory ack for %s", trimmed);
    }
    json_free(cfg);
    return ok;
}

/* PoP: cli_security_advisories_filter_unacked @ hermes_cli/security_advisories.py:filter_unacked */
/* Return only hits whose advisories the user has not dismissed.
 * Input: JSON array of hits. Returns a new JSON array (caller frees). */
json_t *cli_security_advisories_filter_unacked(const json_t *hits) {
    json_t *out = json_array();
    if (!hits || hits->type != JSON_ARRAY || hits->c.count == 0) return out;
    json_t *acked = cli_security_advisories_get_acked_ids();
    for (size_t i = 0; i < hits->c.count; i++) {
        json_t *h = hits->c.items[i];
        if (!h || h->type != JSON_OBJECT) continue;
        json_t *idv = json_obj_get(h, "advisory_id");
        const char *hid = (idv && idv->type == JSON_STRING) ? idv->str_val : NULL;
        bool is_acked = false;
        if (hid) {
            for (size_t j = 0; !is_acked && j < acked->c.count; j++) {
                json_t *aid = acked->c.items[j];
                if (aid && aid->type == JSON_STRING && aid->str_val &&
                    strcmp(aid->str_val, hid) == 0)
                    is_acked = true;
            }
        }
        if (!is_acked) json_append(out, json_copy(h));
    }
    json_free(acked);
    return out;
}

/* ============================================================
 * Rendering helpers
 * ============================================================ */

/* PoP: cli_security_advisories_term_supports_color @ hermes_cli/security_advisories.py:_term_supports_color */
bool cli_security_advisories_term_supports_color(void) {
    const char *no_color = getenv("NO_COLOR");
    if (no_color && *no_color) return false;
    if (!isatty(STDOUT_FILENO)) return false;
    return true;
}

/* Append a line to a growing buffer. */
static void buf_append(char *buf, size_t buf_sz, size_t *pos, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));
static void buf_append(char *buf, size_t buf_sz, size_t *pos, const char *fmt, ...) {
    if (*pos >= buf_sz) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *pos, buf_sz - *pos, fmt, ap);
    va_end(ap);
    if (n > 0) *pos += (size_t)n < buf_sz - *pos ? (size_t)n : buf_sz - *pos;
}

/* PoP: cli_security_advisories_short_banner_lines @ hermes_cli/security_advisories.py:short_banner_lines */
/* Write 1-4 newline-joined short banner lines into out. Returns false
 * (empty out) if hits is empty. Always names the worst hit explicitly. */
bool cli_security_advisories_short_banner_lines(const json_t *hits,
                                                char *out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    out[0] = '\0';
    if (!hits || hits->type != JSON_ARRAY || hits->c.count == 0) return false;

    json_t *primary = hits->c.items[0];
    if (!primary || primary->type != JSON_OBJECT) return false;
    json_t *jid = json_obj_get(primary, "advisory_id");
    json_t *jtitle = json_obj_get(primary, "title");
    json_t *jpkg = json_obj_get(primary, "package");
    json_t *jver = json_obj_get(primary, "installed_version");
    const char *id = (jid && jid->str_val) ? jid->str_val : "unknown";
    const char *title = (jtitle && jtitle->str_val) ? jtitle->str_val : "";
    const char *pkg = (jpkg && jpkg->str_val) ? jpkg->str_val : "";
    const char *ver = (jver && jver->str_val) ? jver->str_val : "";

    size_t pos = 0;
    buf_append(out, out_sz, &pos, "SECURITY ADVISORY [%s]: %s\n", id, title);
    if (hits->c.count > 1) {
        size_t extra = hits->c.count - 1;
        buf_append(out, out_sz, &pos, "  (%zu additional advisor%s also active.)\n",
                   extra, extra > 1 ? "ies" : "y");
    }
    buf_append(out, out_sz, &pos, "  Detected: %s==%s\n", pkg, ver);
    buf_append(out, out_sz, &pos, "  Run 'hermes doctor' for remediation steps.");
    return true;
}

/* PoP: cli_security_advisories_full_remediation_text @ hermes_cli/security_advisories.py:full_remediation_text */
/* Write the multi-line advisory + remediation block for one hit. */
bool cli_security_advisories_full_remediation_text(const json_t *hit,
                                                   char *out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    out[0] = '\0';
    if (!hit || hit->type != JSON_OBJECT) return false;

    json_t *jid = json_obj_get(hit, "advisory_id");
    json_t *jtitle = json_obj_get(hit, "title");
    json_t *jsev = json_obj_get(hit, "severity");
    json_t *jpub = json_obj_get(hit, "published");
    json_t *jpkg = json_obj_get(hit, "package");
    json_t *jver = json_obj_get(hit, "installed_version");
    json_t *jurl = json_obj_get(hit, "url");
    json_t *jsum = json_obj_get(hit, "summary");
    json_t *jrem = json_obj_get(hit, "remediation");

    size_t pos = 0;
    buf_append(out, out_sz, &pos, "=== %s ===\n",
               (jtitle && jtitle->str_val) ? jtitle->str_val : "");
    buf_append(out, out_sz, &pos, "ID:        %s    Severity: %s    Published: %s\n",
               (jid && jid->str_val) ? jid->str_val : "",
               (jsev && jsev->str_val) ? jsev->str_val : "high",
               (jpub && jpub->str_val) ? jpub->str_val : "");
    buf_append(out, out_sz, &pos, "Detected:  %s==%s\n",
               (jpkg && jpkg->str_val) ? jpkg->str_val : "",
               (jver && jver->str_val) ? jver->str_val : "");
    buf_append(out, out_sz, &pos, "Reference: %s\n\n",
               (jurl && jurl->str_val) ? jurl->str_val : "");
    buf_append(out, out_sz, &pos, "%s\n\n",
               (jsum && jsum->str_val) ? jsum->str_val : "");
    buf_append(out, out_sz, &pos, "Remediation:");
    if (jrem && jrem->type == JSON_ARRAY) {
        for (size_t i = 0; i < jrem->c.count; i++) {
            json_t *step = jrem->c.items[i];
            if (step && step->type == JSON_STRING && step->str_val)
                buf_append(out, out_sz, &pos, "\n  %zu. %s", i + 1, step->str_val);
        }
    }
    return true;
}

/* ============================================================
 * Startup-banner gating (24h re-banner cache)
 * ============================================================ */

/* PoP: cli_security_advisories_banner_cache_path @ hermes_cli/security_advisories.py:_banner_cache_path */
/* Fill out with ~/.hermes/cache/advisory_banner_seen, creating the cache
 * dir. Returns false when the home/cache dir can't be resolved/created. */
bool cli_security_advisories_banner_cache_path(char *out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    char home[HERMES_PATH_MAX];
    hermes_get_home(home, sizeof(home));
    if (!home[0]) return false;
    char cache_dir[HERMES_PATH_MAX];
    snprintf(cache_dir, sizeof(cache_dir), "%s/cache", home);
    if (mkdir(cache_dir, 0755) != 0) {
        struct stat st;
        if (stat(cache_dir, &st) != 0 || !S_ISDIR(st.st_mode)) return false;
    }
    snprintf(out, out_sz, "%s/%s", cache_dir, BANNER_CACHE_FILE);
    return true;
}

/* PoP: cli_security_advisories_read_banner_cache @ hermes_cli/security_advisories.py:_read_banner_cache */
/* Returns a JSON object mapping advisory_id -> last-shown unix ts.
 * Caller frees. Corrupt/missing file yields {}. */
json_t *cli_security_advisories_read_banner_cache(void) {
    json_t *out = json_object();
    char path[HERMES_PATH_MAX];
    if (!cli_security_advisories_banner_cache_path(path, sizeof(path)))
        return out;
    FILE *f = fopen(path, "r");
    if (!f) return out;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char aid[256];
        double ts;
        if (sscanf(line, "%255s %lf", aid, &ts) == 2 && aid[0])
            json_set(out, aid, json_number(ts));
    }
    fclose(f);
    return out;
}

/* PoP: cli_security_advisories_write_banner_cache @ hermes_cli/security_advisories.py:_write_banner_cache */
/* seen: JSON object advisory_id -> ts. Failure is logged, not fatal. */
void cli_security_advisories_write_banner_cache(const json_t *seen) {
    if (!seen || seen->type != JSON_OBJECT) return;
    char path[HERMES_PATH_MAX];
    if (!cli_security_advisories_banner_cache_path(path, sizeof(path)))
        return;
    FILE *f = fopen(path, "w");
    if (!f) {
        hermes_log(LOG_DEBUG, "security",
                   "Could not write advisory banner cache");
        return;
    }
    for (size_t i = 0; i < seen->c.count; i++) {
        json_t *v = seen->c.items[i];
        double ts = (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
        fprintf(f, "%s %f\n", seen->c.keys[i], ts);
    }
    fclose(f);
}

/* PoP: cli_security_advisories_hits_due_for_banner @ hermes_cli/security_advisories.py:hits_due_for_banner */
/* Return only hits whose banner is due (not acked, not shown within
 * repeat_hours). Side effect: stamps the cache for hits about to be
 * shown, exactly like the Python. Caller frees the returned array. */
json_t *cli_security_advisories_hits_due_for_banner(const json_t *hits,
                                                    int repeat_hours) {
    json_t *fresh = cli_security_advisories_filter_unacked(hits);
    if (fresh->c.count == 0) return fresh;

    double now = (double)time(NULL);
    double cutoff = now - ((double)repeat_hours * 3600.0);
    json_t *cache = cli_security_advisories_read_banner_cache();

    json_t *due = json_array();
    for (size_t i = 0; i < fresh->c.count; i++) {
        json_t *h = fresh->c.items[i];
        json_t *jid = (h && h->type == JSON_OBJECT)
                          ? json_obj_get(h, "advisory_id") : NULL;
        const char *hid = (jid && jid->type == JSON_STRING) ? jid->str_val : NULL;
        if (!hid) continue;
        json_t *lastv = json_obj_get(cache, hid);
        double last = (lastv && lastv->type == JSON_NUMBER) ? lastv->num_val : 0.0;
        if (last < cutoff) {
            json_append(due, json_copy(h));
            json_set(cache, hid, json_number(now));
        }
    }
    if (due->c.count > 0)
        cli_security_advisories_write_banner_cache(cache);
    json_free(cache);
    json_free(fresh);
    return due;
}

/* ============================================================
 * Public entry points used by doctor / CLI / gateway
 * ============================================================ */

/* PoP: cli_security_advisories_render_doctor_section @ hermes_cli/security_advisories.py:render_doctor_section */
/* Render the security-advisory section for `hermes doctor`.
 * Returns true when there are active problems; out gets the section text. */
bool cli_security_advisories_render_doctor_section(const json_t *hits,
                                                   char *out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    json_t *fresh = cli_security_advisories_filter_unacked(hits);
    if (fresh->c.count == 0) {
        snprintf(out, out_sz, "No active security advisories.  ✓");
        json_free(fresh);
        return false;
    }
    size_t pos = 0;
    out[0] = '\0';
    for (size_t i = 0; i < fresh->c.count; i++) {
        if (i) buf_append(out, out_sz, &pos, "\n\n");
        char block[8192];
        cli_security_advisories_full_remediation_text(
            fresh->c.items[i], block, sizeof(block));
        buf_append(out, out_sz, &pos, "%s", block);
    }
    json_free(fresh);
    return true;
}

/* PoP: cli_security_advisories_startup_banner @ hermes_cli/security_advisories.py:startup_banner */
/* Fill out with a printable startup banner; returns false when nothing
 * is due. Updates the banner cache as a side effect. */
bool cli_security_advisories_startup_banner(const json_t *hits,
                                            char *out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    out[0] = '\0';
    json_t *due = cli_security_advisories_hits_due_for_banner(
        hits, BANNER_REPEAT_HOURS);
    if (due->c.count == 0) {
        json_free(due);
        return false;
    }
    char lines[8192];
    cli_security_advisories_short_banner_lines(due, lines, sizeof(lines));
    if (cli_security_advisories_term_supports_color())
        snprintf(out, out_sz, "\x1b[1;31m%s\x1b[0m", lines);
    else
        snprintf(out, out_sz, "%s", lines);
    json_free(due);
    return true;
}

/* PoP: cli_security_advisories_gateway_log_message @ hermes_cli/security_advisories.py:gateway_log_message */
/* Fill out with a one-line log message for gateway operators; returns
 * false when there are no unacked hits. */
bool cli_security_advisories_gateway_log_message(const json_t *hits,
                                                 char *out, size_t out_sz) {
    if (!out || out_sz == 0) return false;
    out[0] = '\0';
    json_t *fresh = cli_security_advisories_filter_unacked(hits);
    if (fresh->c.count == 0) {
        json_free(fresh);
        return false;
    }
    if (fresh->c.count == 1) {
        json_t *h = fresh->c.items[0];
        json_t *jid = json_obj_get(h, "advisory_id");
        json_t *jpkg = json_obj_get(h, "package");
        json_t *jver = json_obj_get(h, "installed_version");
        json_t *jtitle = json_obj_get(h, "title");
        json_t *jurl = json_obj_get(h, "url");
        snprintf(out, out_sz,
                 "Security advisory [%s] active: %s==%s matches %s. See %s",
                 (jid && jid->str_val) ? jid->str_val : "",
                 (jpkg && jpkg->str_val) ? jpkg->str_val : "",
                 (jver && jver->str_val) ? jver->str_val : "",
                 (jtitle && jtitle->str_val) ? jtitle->str_val : "",
                 (jurl && jurl->str_val) ? jurl->str_val : "");
    } else {
        size_t pos = 0;
        buf_append(out, out_sz, &pos, "%zu security advisories active (IDs: ",
                   fresh->c.count);
        for (size_t i = 0; i < fresh->c.count; i++) {
            json_t *h = fresh->c.items[i];
            json_t *jid = (h && h->type == JSON_OBJECT)
                              ? json_obj_get(h, "advisory_id") : NULL;
            buf_append(out, out_sz, &pos, "%s%s", i ? ", " : "",
                       (jid && jid->str_val) ? jid->str_val : "?");
        }
        buf_append(out, out_sz, &pos,
                   "). Run `hermes doctor` on the gateway host for details.");
    }
    json_free(fresh);
    return true;
}
