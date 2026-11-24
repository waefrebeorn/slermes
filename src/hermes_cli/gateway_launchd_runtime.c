/*
 * gateway_launchd_runtime.c -- real C11 port of the launchd job bookkeeping of
 * upstream Hermes hermes_cli/gateway_launchd_runtime.py.
 *
 * PoP: w21_gatewaylau_1a86bf_budget @ hermes_cli/gateway_launchd_runtime.py:_budget
 * PoP: w21_gatewaylau_1a86bf_poll @ hermes_cli/gateway_launchd_runtime.py:GatewayJob.poll
 * PoP: w21_gatewaylau_1a86bf_finished_jobs @ hermes_cli/gateway_launchd_runtime.py:_remove_finished_jobs
 * PoP: w21_gatewaylau_1a86bf_job @ hermes_cli/gateway_launchd_runtime.py:start_gateway_job
 *
 * What is ported is everything that decides without a process: the monotonic
 * deadline budget, the `launchctl print` state machine (`pid = N`,
 * `last exit code = N`, a dead job), the `launchctl list` filter for finished
 * jobs, and the label/target/plist identity a job is created with.  Left
 * upstream: `_launchctl`, `pgrep`, `os.open` + `plistlib.dump` + `bootstrap`,
 * and `gateway_launchd.launchd_program_arguments`.
 */
#include "hermes_json.h"
#include "noc_w21_gatewaylau_1a86bf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *glr_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

json_t *w21_gatewaylau_1a86bf_budget(const json_t *deadline, double now, double cap) {
    json_t *out = json_object();
    double val;
    if (!out) return NULL;
    if (!deadline || deadline->type == JSON_NULL) {
        val = cap;
    } else {
        double d = deadline->type == JSON_NUMBER ? deadline->num_val : cap;
        double left = d - now;
        val = left < cap ? left : cap;
        if (val < 0.0) val = 0.0;
    }
    json_set(out, "value", json_number(val));
    return out;
}

/* `^\s*pid = (\d+)` / `^\s*last exit code = (-?\d+)` with re.MULTILINE: the
 * first match in the text wins and `\s*` spans only that line's indentation. */
static int glr_scan(const char *stdout_text, const char *key, long long *value) {
    const char *p = stdout_text ? stdout_text : "";
    size_t keylen = strlen(key);
    while (*p) {
        const char *line = p;
        const char *eol = strchr(p, '\n');
        const char *q;
        long long acc = 0;
        int neg = 0, digits = 0;
        while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\f' || *line == '\v') line++;
        if (!strncmp(line, key, keylen)) {
            q = line + keylen;
            if (*q == '-') { neg = 1; q++; }
            while (*q >= '0' && *q <= '9') {
                if (acc > (9223372036854775807LL - (*q - '0')) / 10) break;
                acc = acc * 10 + (*q - '0');
                q++;
                digits++;
            }
            if (digits) {
                *value = neg ? -acc : acc;
                return 1;
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    return 0;
}

json_t *w21_gatewaylau_1a86bf_poll(const json_t *req) {
    json_t *out = json_object();
    json_t *prev = req ? json_obj_get(req, "prev_status") : NULL;
    json_t *pidv = req ? json_obj_get(req, "pid") : NULL;
    double elapsed = req ? json_get_num(req, "elapsed", 0.0) : 0.0;
    long long rc = req ? (long long)json_get_num(req, "rc", 0.0) : 0;
    const char *stdout_text = req ? json_get_str(req, "stdout", "") : NULL;
    long long pid = (pidv && pidv->type == JSON_NUMBER) ? (long long)pidv->num_val : -1;
    long long scanned = 0;
    int checked = 0;
    long long status = -1;   /* -1 stands for Python's None */
    if (!out) return NULL;
    if (prev && prev->type == JSON_NUMBER) {
        /* `if self._status is not None: return self._status` */
        status = (long long)prev->num_val;
    } else if (elapsed < W21_GATEWAYLAU_1A86BF_POLL_INTERVAL_S) {
        /* inside the poll interval: no launchctl call, nothing recorded */
    } else {
        checked = 1;
        if (rc != 0) {
            status = 1;   /* the job is gone without a recorded exit */
        } else if (glr_scan(stdout_text, "pid = ", &scanned)) {
            long long wrapper = scanned;
            double budget = req ? json_get_num(req, "budget", 5.0) : 5.0;
            if (pid != -1 && pid != wrapper) {
                /* a different gateway pid is already recorded */
            } else if (budget <= 0.0) {
                /* nothing left for pgrep: the wrapper pid stays unrecorded */
            } else {
                json_t *pg = req ? json_obj_get(req, "pgrep") : NULL;
                int failed = req ? json_get_bool(req, "pgrep_fail", false) : 0;
                if (!failed && pg && pg->type == JSON_ARRAY && pg->c.count > 0) {
                    json_t *first = pg->c.items[0];
                    if (first && first->type == JSON_NUMBER)
                        pid = (long long)first->num_val;
                    else if (first && first->type == JSON_STRING && first->str_val && first->str_val[0])
                        pid = strtoll(first->str_val, NULL, 10);
                    else
                        pid = wrapper;
                } else {
                    pid = wrapper;
                }
            }
        } else if (glr_scan(stdout_text, "last exit code = ", &scanned)) {
            status = scanned;
        }
    }
    json_set(out, "status", status == -1 ? json_null() : json_number((double)status));
    json_set(out, "pid", pid == -1 ? json_null() : json_number((double)pid));
    json_set(out, "checked", json_bool(checked));
    return out;
}

json_t *w21_gatewaylau_1a86bf_finished_jobs(const json_t *req) {
    json_t *out = json_array();
    long long rc = req ? (long long)json_get_num(req, "rc", 0.0) : 0;
    const char *p = req ? json_get_str(req, "stdout", "") : "";
    if (!out) return NULL;
    if (rc != 0) return out;   /* a failing `launchctl list` removes nothing */
    if (!p) p = "";
    while (*p) {
        char line[4096];
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        char *fields[8];
        size_t nf = 0;
        char *save = NULL, *tok;
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        for (tok = strtok_r(line, " \t\r", &save); tok && nf < 8;
             tok = strtok_r(NULL, " \t\r", &save))
            fields[nf++] = tok;
        if (nf == 3 && !strcmp(fields[0], "-") &&
            !strncmp(fields[2], W21_GATEWAYLAU_1A86BF_LABEL_PREFIX,
                     strlen(W21_GATEWAYLAU_1A86BF_LABEL_PREFIX)))
            json_append(out, json_string(fields[2]));
        if (!eol) break;
        p = eol + 1;
    }
    return out;
}

json_t *w21_gatewaylau_1a86bf_job(const json_t *req) {
    json_t *out = json_object();
    json_t *plist = json_object();
    json_t *wrapped = json_array();
    json_t *cmd = req ? json_obj_get(req, "command") : NULL;
    const char *hex12 = req ? json_get_str(req, "hex12", "") : "";
    long long uid = req ? (long long)json_get_num(req, "uid", 0.0) : 0;
    const char *cwd = req ? json_get_str(req, "cwd", "") : "";
    char label[128], target[160], plist_name[160];
    size_t i;
    if (!out || !plist || !wrapped) {
        json_free(out);
        json_free(plist);
        json_free(wrapped);
        return NULL;
    }
    snprintf(label, sizeof(label), "%s%.12s", W21_GATEWAYLAU_1A86BF_LABEL_PREFIX,
             hex12 ? hex12 : "");
    snprintf(target, sizeof(target), "gui/%lld/%s", uid, label);
    snprintf(plist_name, sizeof(plist_name), ".%s.plist", label);
    json_append(wrapped, json_string("/usr/bin/env"));
    json_append(wrapped, json_string("XPC_SERVICE_NAME=0"));
    if (cmd && cmd->type == JSON_ARRAY) {
        for (i = 0; i < cmd->c.count; i++) {
            json_t *item = cmd->c.items[i];
            if (item && item->type == JSON_STRING) json_append(wrapped, json_string(item->str_val));
        }
    }
    json_set(plist, "Label", json_string(label));
    json_set(plist, "EnvironmentVariables",
             (req && json_obj_get(req, "env")) ? json_copy(json_obj_get(req, "env")) : json_object());
    json_set(plist, "WorkingDirectory", json_string(cwd ? cwd : ""));
    json_set(plist, "RunAtLoad", json_bool(1));
    json_set(out, "label", json_string(label));
    json_set(out, "target", json_string(target));
    json_set(out, "plist_name", json_string(plist_name));
    json_set(out, "wrapped", wrapped);
    json_set(out, "plist", plist);
    return out;
}
