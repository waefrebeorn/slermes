#define _POSIX_C_SOURCE 200809L
/*
 * skillevaluator_scan.c — faithful port of tools/skillevaluator_scan.py
 * (advisory NVIDIA SkillEvaluator Tier 1 scan for skill installs).
 *
 * Design contract carried over from the Python module:
 *  - Warn, don't block: PII-class findings are informational only.
 *  - Prompt only for secrets-class criticals (SECRETS_CLASS_CHECKS).
 *  - Never break installs: scanner missing/crashing/timing out/unparseable
 *    all degrade to available=false ("no opinion"), never an error.
 *
 * Side effects route through injectable hooks; defaults do the real work.
 */
#include "skillevaluator_scan.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

skillevaluator_avail_fn skillevaluator_scan_avail_hook = NULL;
skillevaluator_run_fn  skillevaluator_scan_run_hook = NULL;

/* ------------------------------------------------------------------ */
/* SECRETS_CLASS_CHECKS                                                */
/* ------------------------------------------------------------------ */
static const char *const SECRETS_CLASS_CHECKS[] = {
    "database_credentials",
    "hardcoded_secrets",
    "jwt_tokens",
    "webhook_urls",
    "aws_identifiers",
    "github_tokens",
    "private_keys",
};

/* PoP: is_secrets_class @ tools/skillevaluator_scan.py:is_secrets_class */
bool skillevaluator_scan_is_secrets_class(const char *check) {
    if (!check) return false;
    for (size_t i = 0; i < sizeof(SECRETS_CLASS_CHECKS) / sizeof(SECRETS_CLASS_CHECKS[0]); i++) {
        if (strcmp(check, SECRETS_CLASS_CHECKS[i]) == 0) return true;
    }
    return false;
}

bool skillevaluator_scan_finding_is_secrets_class(const skillevaluator_scan_finding_t *f) {
    return f && skillevaluator_scan_is_secrets_class(f->check);
}

/* Tier1Finding.location() */
char *skillevaluator_scan_finding_location(const skillevaluator_scan_finding_t *f) {
    if (!f) return strdup("?");
    if (f->file && f->file[0] && f->line) {
        size_t n = snprintf(NULL, 0, "%s:%ld", f->file, f->line);
        char *s = malloc(n + 1);
        if (!s) return NULL;
        snprintf(s, n + 1, "%s:%ld", f->file, f->line);
        return s;
    }
    if (f->file && f->file[0]) return strdup(f->file);
    return strdup("?");
}

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

/* Python str[:200]: truncate by codepoints, not bytes. */
static char *str_truncate_200(const char *s) {
    if (!s) return xstrdup("");
    size_t cps = 0, byte = 0;
    while (s[byte] && cps < 200) {
        unsigned char c = (unsigned char)s[byte];
        size_t len = (c < 0x80) ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
        byte += len;
        cps++;
    }
    char *out = malloc(byte + 1);
    if (!out) return NULL;
    memcpy(out, s, byte);
    out[byte] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* splits                                                              */
/* ------------------------------------------------------------------ */
/* PoP: advisory_findings @ tools/skillevaluator_scan.py:advisory_findings */
size_t skillevaluator_scan_advisory_findings(const skillevaluator_scan_report_t *r,
                                             skillevaluator_scan_finding_t **out) {
    size_t n = 0;
    if (!r || !out) return 0;
    for (size_t i = 0; i < r->n_findings; i++)
        if (!skillevaluator_scan_finding_is_secrets_class(&r->findings[i])) n++;
    *out = malloc((n ? n : 1) * sizeof(**out));
    size_t k = 0;
    for (size_t i = 0; i < r->n_findings; i++)
        if (!skillevaluator_scan_finding_is_secrets_class(&r->findings[i]))
            (*out)[k++] = r->findings[i];
    return n;
}

/* PoP: secrets_findings @ tools/skillevaluator_scan.py:secrets_findings */
size_t skillevaluator_scan_secrets_findings(const skillevaluator_scan_report_t *r,
                                            skillevaluator_scan_finding_t **out) {
    size_t n = 0;
    if (!r || !out) return 0;
    for (size_t i = 0; i < r->n_findings; i++)
        if (skillevaluator_scan_finding_is_secrets_class(&r->findings[i])) n++;
    *out = malloc((n ? n : 1) * sizeof(**out));
    size_t k = 0;
    for (size_t i = 0; i < r->n_findings; i++)
        if (skillevaluator_scan_finding_is_secrets_class(&r->findings[i]))
            (*out)[k++] = r->findings[i];
    return n;
}

void skillevaluator_scan_report_free(skillevaluator_scan_report_t *r) {
    if (!r) return;
    for (size_t i = 0; i < r->n_findings; i++) {
        free(r->findings[i].check);
        free(r->findings[i].validator);
        free(r->findings[i].severity);
        free(r->findings[i].message);
        free(r->findings[i].file);
        free(r->findings[i].suggestion);
    }
    free(r->findings);
    for (size_t i = 0; i < r->n_incomplete_checks; i++) free(r->incomplete_checks[i]);
    free(r->incomplete_checks);
    free(r->error);
    free(r);
}

/* ------------------------------------------------------------------ */
/* _parse_report                                                       */
/* ------------------------------------------------------------------ */
static char *json_str_or(const json_t *obj, const char *key, const char *def) {
    const json_t *v = json_object_get((json_t *)obj, key);
    if (v && v->type == JSON_STRING && v->str_val) return xstrdup(v->str_val);
    return xstrdup(def);
}

/* PoP: _parse_report @ tools/skillevaluator_scan.py:_parse_report */
skillevaluator_scan_report_t *skillevaluator_scan_parse_report(const json_t *report) {
    skillevaluator_scan_report_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->available = true;
    r->passed = true;
    bool any_complete_failed = false;
    size_t cap = 0, icap = 0;

    const json_t *results = json_object_get((json_t *)report, "results");
    if (results && results->type == JSON_ARRAY) {
        size_t n = json_len(results);
        for (size_t i = 0; i < n; i++) {
            const json_t *res = json_get(results, i);
            if (!res || res->type != JSON_OBJECT) continue;
            char *validator = json_str_or(res, "validator", "unknown");
            const json_t *status_v = json_object_get((json_t *)res, "status");
            bool is_incomplete = false;
            if (status_v && status_v->type == JSON_STRING && status_v->str_val) {
                /* str(...).lower() == "incomplete" */
                const char *sv = status_v->str_val;
                char *low = strdup(sv);
                for (char *p = low; p && *p; p++) *p = (char)tolower((unsigned char)*p);
                is_incomplete = strcmp(low, "incomplete") == 0;
                free(low);
            } else if (status_v && status_v->type != JSON_NULL &&
                       status_v->type != JSON_STRING) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%lld", (long long)status_v->num_val);
                is_incomplete = strcmp(buf, "incomplete") == 0;
            }
            if (is_incomplete) {
                if (r->n_incomplete_checks == icap) {
                    icap = icap ? icap * 2 : 4;
                    r->incomplete_checks = realloc(r->incomplete_checks, icap * sizeof(char *));
                }
                r->incomplete_checks[r->n_incomplete_checks++] = validator;
            } else {
                const json_t *passed_v = json_object_get((json_t *)res, "passed");
                bool passed = true; /* res.get("passed", True) */
                if (passed_v && passed_v->type == JSON_BOOL)
                    passed = passed_v->bool_val != 0;
                else if (passed_v && passed_v->type == JSON_NUMBER)
                    passed = passed_v->num_val != 0.0;
                else if (passed_v && passed_v->type == JSON_NULL)
                    passed = true;
                if (!passed) any_complete_failed = true;
            }
            const json_t *findings = json_object_get((json_t *)res, "findings");
            if (findings && findings->type == JSON_ARRAY) {
                size_t fn = json_len(findings);
                for (size_t j = 0; j < fn; j++) {
                    const json_t *f = json_get(findings, j);
                    if (!f || f->type != JSON_OBJECT) continue;
                    if (r->n_findings == cap) {
                        cap = cap ? cap * 2 : 8;
                        r->findings = realloc(r->findings, cap * sizeof(*r->findings));
                    }
                    skillevaluator_scan_finding_t *nf = &r->findings[r->n_findings];
                    memset(nf, 0, sizeof(*nf));
                    nf->check      = json_str_or(f, "check_name", "");
                    nf->validator  = xstrdup(validator);
                    char *sev      = json_str_or(f, "severity", "info");
                    for (char *p = sev; p && *p; p++) *p = (char)tolower((unsigned char)*p);
                    nf->severity   = sev;
                    nf->message    = str_truncate_200(json_str_or(f, "message", ""));
                    nf->file       = json_str_or(f, "file_path", "");
                    const json_t *ln = json_object_get((json_t *)f, "line_number");
                    nf->line = 0;
                    if (ln && ln->type == JSON_NUMBER) nf->line = (long)ln->num_val;
                    else if (ln && ln->type == JSON_STRING && ln->str_val)
                        nf->line = strtol(ln->str_val, NULL, 10);
                    nf->suggestion = str_truncate_200(json_str_or(f, "suggestion", ""));
                    r->n_findings++;
                }
            }
            /* validator freed above unless moved into incomplete_checks */
            if (!is_incomplete) free(validator);
        }
    }
    r->passed = !any_complete_failed && r->n_findings == 0;
    return r;
}

/* ------------------------------------------------------------------ */
/* format_tier1_report                                                 */
/* ------------------------------------------------------------------ */
/* PoP: format_tier1_report @ tools/skillevaluator_scan.py:format_tier1_report */
char *skillevaluator_scan_format_tier1_report(const skillevaluator_scan_report_t *r,
                                              int limit) {
    if (!r || !r->available) return strdup("");
    char *out = NULL;
    size_t len = 0;
    FILE *buf = open_memstream(&out, &len);
    if (!buf) return NULL;
    if (r->n_findings == 0) {
        if (r->n_incomplete_checks > 0)
            fputs("SkillEvaluator Tier 1: no findings from completed checks.", buf);
        else
            fputs("SkillEvaluator Tier 1: no findings.", buf);
    } else {
        fprintf(buf,
                "SkillEvaluator Tier 1 (advisory): %zu finding(s) — "
                "informational, verify before relying on this skill.",
                r->n_findings);
        /* shown = secrets_findings + advisory_findings */
        skillevaluator_scan_finding_t *shown =
            malloc((r->n_findings ? r->n_findings : 1) * sizeof(*shown));
        size_t n_shown = 0;
        skillevaluator_scan_finding_t *part;
        size_t np = skillevaluator_scan_secrets_findings(r, &part);
        for (size_t i = 0; i < np; i++) shown[n_shown++] = part[i];
        free(part);
        np = skillevaluator_scan_advisory_findings(r, &part);
        for (size_t i = 0; i < np; i++) shown[n_shown++] = part[i];
        free(part);
        int lim = limit > 0 ? limit : 10;
        for (int i = 0; i < lim && (size_t)i < n_shown; i++) {
            const skillevaluator_scan_finding_t *f = &shown[i];
            char tag[32];
            if (skillevaluator_scan_finding_is_secrets_class(f)) {
                snprintf(tag, sizeof(tag), "SECRETS");
            } else {
                const char *sev = f->severity ? f->severity : "";
                size_t k = 0;
                for (const char *p = sev; *p && k < sizeof(tag) - 1; p++)
                    tag[k++] = (char)toupper((unsigned char)*p);
                tag[k] = '\0';
            }
            char *loc = skillevaluator_scan_finding_location(f);
            fprintf(buf, "\n  [%s] %s — %s", tag, loc ? loc : "?",
                    f->message ? f->message : "");
            free(loc);
        }
        if (n_shown > (size_t)lim)
            fprintf(buf, "\n  … and %zu more", n_shown - (size_t)lim);
        free(shown);
    }
    if (r->n_incomplete_checks > 0) {
        fputs("\n  (not run: ", buf);
        for (size_t i = 0; i < r->n_incomplete_checks; i++)
            fprintf(buf, "%s%s", i ? ", " : "", r->incomplete_checks[i]);
        fputs(" — no opinion from these checks)", buf);
    }
    fclose(buf);
    return out;
}

/* ------------------------------------------------------------------ */
/* scanner_available                                                   */
/* ------------------------------------------------------------------ */
/* shutil.which(SCANNER_BIN): search PATH for an executable file. */
/* PoP: scanner_available @ tools/skillevaluator_scan.py:scanner_available */
static bool default_scanner_available(void) {
    const char *path_env = getenv("PATH");
    if (!path_env) path_env = "/usr/bin:/bin";
    char *paths = strdup(path_env);
    char *save = NULL;
    bool found = false;
    for (char *dir = strtok_r(paths, ":", &save); dir && !found;
         dir = strtok_r(NULL, ":", &save)) {
        size_t n = snprintf(NULL, 0, "%s/%s", dir, SKILLEVALUATOR_SCANNER_BIN);
        char *cand = malloc(n + 1);
        snprintf(cand, n + 1, "%s/%s", dir, SKILLEVALUATOR_SCANNER_BIN);
        struct stat st;
        if (stat(cand, &st) == 0 && S_ISREG(st.st_mode) && access(cand, X_OK) == 0)
            found = true;
        free(cand);
    }
    free(paths);
    return found;
}

bool skillevaluator_scanner_available(void) {
    if (skillevaluator_scan_avail_hook) return skillevaluator_scan_avail_hook();
    return default_scanner_available();
}

/* Default runner: real fork/exec of the scanner binary (output captured and
 * discarded, like capture_output=True). Ignores the exit code exactly as the
 * Python does. Sets *timed_out on timeout (we approximate with SIGKILL after
 * a poll deadline). */
static int default_run_scanner(const char *scanner_bin, const char *skill_dir,
                               const char *checks, const char *outdir, int timeout,
                               bool *timed_out, char **launch_err) {
    (void)checks;
    *timed_out = false;
    *launch_err = NULL;
    int errno_saved = ENOENT;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* child: silence stdout/stderr */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        char checks_arg[256], no_dedup[] = "--no-dedup", r_arg[] = "-r",
             json_arg[] = "json", o_arg[] = "-o";
        snprintf(checks_arg, sizeof(checks_arg), "--checks=%s", checks);
        char *argv[16];
        int i = 0;
        argv[i++] = (char *)scanner_bin;
        argv[i++] = "validate";
        argv[i++] = (char *)skill_dir;
        argv[i++] = checks_arg;
        argv[i++] = no_dedup;
        argv[i++] = r_arg;
        argv[i++] = json_arg;
        argv[i++] = o_arg;
        argv[i++] = (char *)outdir;
        argv[i] = NULL;
        execvp(scanner_bin, argv);
        _exit(127);
    }
    /* parent: wait up to timeout seconds */
    for (int waited = 0; waited <= timeout; waited++) {
        int status;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
                char buf[128];
                snprintf(buf, sizeof(buf), "[Errno %d] exec '%s' failed",
                         errno_saved, scanner_bin);
                *launch_err = strdup(buf);
                return -1;
            }
            return 0;
        }
        if (w < 0) return -1;
        sleep(1);
    }
    kill(pid, 9);
    waitpid(pid, NULL, 0);
    *timed_out = true;
    return 0;
}

static char *read_file_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    return buf;
}

/* sorted(Path(outdir).glob("skillevaluator-output-*.json"))[-1] — lexicographic
 * sort, take last. Returns malloc'd path or NULL. */
static char *newest_report_path(const char *outdir) {
    DIR *d = opendir(outdir);
    if (!d) return NULL;
    char *best = NULL;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "skillevaluator-output-", 22) != 0) continue;
        size_t len = strlen(e->d_name);
        if (len < 6 || strcmp(e->d_name + len - 5, ".json") != 0) continue;
        if (!best || strcmp(e->d_name, best) > 0) {
            free(best);
            best = strdup(e->d_name);
        }
    }
    closedir(d);
    if (!best) return NULL;
    size_t n = snprintf(NULL, 0, "%s/%s", outdir, best);
    char *full = malloc(n + 1);
    snprintf(full, n + 1, "%s/%s", outdir, best);
    free(best);
    return full;
}

/* PoP: run_tier1_scan @ tools/skillevaluator_scan.py:run_tier1_scan */
skillevaluator_scan_report_t *skillevaluator_run_tier1_scan(const char *skill_dir,
                                                            int timeout) {
    skillevaluator_scan_report_t *unavail = calloc(1, sizeof(*unavail));
    if (!unavail) return NULL;
    unavail->available = false;
    unavail->passed = true;

    if (!skillevaluator_scanner_available()) {
        unavail->error = strdup("scanner not on PATH");
        return unavail;
    }
    const char *tmpbase = getenv("TMPDIR");
    if (!tmpbase || !tmpbase[0]) tmpbase = "/tmp";
    char *tmpl = malloc(strlen(tmpbase) + strlen("/se-tier1-XXXXXX") + 1);
    sprintf(tmpl, "%s/se-tier1-XXXXXX", tmpbase);
    char *outdir = mkdtemp(tmpl);
    if (!outdir) {
        unavail->error = strdup("scanner failed to launch: mkdtemp failed");
        return unavail;
    }
    bool timed_out = false;
    char *launch_err = NULL;
    json_t *parsed = NULL;
    skillevaluator_scan_report_t *ok = NULL;
    int rc;
    if (skillevaluator_scan_run_hook)
        rc = skillevaluator_scan_run_hook(SKILLEVALUATOR_SCANNER_BIN, skill_dir,
                                          SKILLEVALUATOR_TIER1_CHECKS, outdir,
                                          timeout, &timed_out, &launch_err);
    else
        rc = default_run_scanner(SKILLEVALUATOR_SCANNER_BIN, skill_dir,
                                 SKILLEVALUATOR_TIER1_CHECKS, outdir, timeout,
                                 &timed_out, &launch_err);
    if (timed_out) {
        char msg[64];
        snprintf(msg, sizeof(msg), "scan timed out after %ds", timeout);
        unavail->error = strdup(msg);
        goto done;
    }
    if (rc != 0) {
        size_t n = snprintf(NULL, 0, "scanner failed to launch: %s",
                            launch_err ? launch_err : "");
        unavail->error = malloc(n + 1);
        snprintf(unavail->error, n + 1, "scanner failed to launch: %s",
                 launch_err ? launch_err : "");
        free(launch_err);
        goto done;
    }
    free(launch_err);
    char *report_path = newest_report_path(outdir);
    if (!report_path) {
        unavail->error = strdup("scanner produced no JSON report");
        goto done;
    }
    char *raw = read_file_all(report_path);
    free(report_path);
    if (!raw) {
        unavail->error = strdup("unparseable report: cannot read file");
        goto done;
    }
    parsed = json_parse(raw, NULL);
    free(raw);
    if (!parsed) {
        unavail->error = strdup("unparseable report");
        goto done;
    }
    if (parsed->type != JSON_OBJECT) {
        unavail->error = strdup("unexpected report shape");
        goto done;
    }
    ok = skillevaluator_scan_parse_report(parsed);
done:
    if (parsed) json_free(parsed);
    /* cleanup temp dir contents */
    {
        DIR *d = opendir(outdir);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL) {
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                size_t n = snprintf(NULL, 0, "%s/%s", outdir, e->d_name);
                char *p = malloc(n + 1);
                snprintf(p, n + 1, "%s/%s", outdir, e->d_name);
                unlink(p);
                free(p);
            }
            closedir(d);
            rmdir(outdir);
        }
    }
    if (unavail->error || !ok) {
        skillevaluator_scan_report_free(ok);
        return unavail;
    }
    skillevaluator_scan_report_free(unavail);
    return ok;
}
