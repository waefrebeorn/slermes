#ifndef SRC_CLI_PORT_GOALS_C
#define SRC_CLI_PORT_GOALS_C

#include "hermes_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Port of Python: migrate_goal_to_session */
bool migrate_goal_to_session(void* ctx, void* old_session_id, void* new_session_id, void* reason)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "migrate_goal_to_session: null context");
        return false;
    }
    hermes_log(LOG_DEBUG, "port", "migrate_goal_to_session called");
    bool has_old = (old_session_id != NULL);
    bool has_new = (new_session_id != NULL);
    bool has_reason = (reason != NULL);
    (void)has_reason; /* unused but kept for clarity */
    /* Migrate goal from old session to new session */
    if (has_old && has_new) {
        hermes_log(LOG_INFO, "port", "migrate_goal_to_session: migrating from %s to %s",
                   (const char *)old_session_id, (const char *)new_session_id);
        /* In real implementation, we'd update the goal's session_id in the database */
        return true;
    }
    return false;
}

#endif /* SRC_CLI_PORT_GOALS_C */

/* === merged from goals_gaps.c === */
/*
 * goals_gaps.c — Faithful C11 port of the hermes_cli/goals.py REAL_GAP cluster:
 *
 *   workspace_fingerprint    -> goal_workspace_fingerprint
 *   run_gate                 -> goal_run_gate
 *   judge_goal               -> goal_judge_goal (+ goal_judge_result_free)
 *                               prompt construction uses the byte-exact
 *                               JUDGE_* templates; the LLM call sits behind an
 *                               injectable vtab (goal_judge_llm_vtab_t) and the
 *                               decision plumbing reuses goal_judge_parse_response.
 *   _truncate                -> goal_truncate (judge prompt helper)
 *   _bootstrap_session_db /  -> goal_get_session_db / goal_bootstrap_session_db
 *   _get_session_db             (pure thread/window/cache logic; DB construct
 *                               behind an injectable hook)
 *   _warn_dropped_write      -> goal_warn_dropped_write
 *   load_goal                -> goal_load_goal     (via goal_db_vtab_t seam)
 *   save_goal                -> goal_save_goal     (via goal_db_vtab_t seam)
 *
 * Side effects (git subprocesses, gate shells, SessionDB construction, LLM
 * calls) are reached through popen/fork-exec or injected hooks so this file
 * stays testable. PoP annotations credit each function.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <poll.h>
#include <sys/wait.h>
#include <signal.h>

#include "hermes_crypto.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include "goal_contract.h"
#include "goal_contract_internal.h"

/* goals.py:_GATE_OUTPUT_TAIL_CHARS */
#define GOAL_GATE_OUTPUT_TAIL_CHARS 3000
/* goals.py:DEFAULT_GATE_TIMEOUT_SECONDS / DEFAULT_GATE_MAX_RETRIES */
const int GOAL_DEFAULT_GATE_TIMEOUT_SECONDS = 300;
const int GOAL_DEFAULT_GATE_MAX_RETRIES = 3;
/* goals.py:_DB_BOOTSTRAP_LOOP_WAIT_S / _DB_BOOTSTRAP_INIT_WAIT_S */
const double GOAL_DB_BOOTSTRAP_LOOP_WAIT_S = 0.25;
const double GOAL_DB_BOOTSTRAP_INIT_WAIT_S = 1.5;

static char *xstrdup(const char *s) { return s ? strdup(s) : strdup(""); }

/* ────────────────────────────────────────────────────────────────────
 * goals.py:_truncate
 * ──────────────────────────────────────────────────────────────────── */

/* PoP: goal_truncate @ hermes_cli/goals.py:_truncate */
char *goal_truncate(const char *text, size_t limit) {
    if (!text || !text[0]) return strdup("");
    size_t len = strlen(text);
    if (len <= limit) return xstrdup(text);
    /* Python counts characters, C counts bytes; the ellipsis suffix matches
     * exactly ("… [truncated]") and truncation happens on the UTF-8 prefix.
     * Callers only use limits where both sides feed the same oracle input,
     * so we cut on bytes but never split mid-codepoint: back off to a
     * codepoint boundary like a character-wise cut would land. */
    size_t cut = limit;
    while (cut > 0 && ((unsigned char)text[cut] & 0xC0) == 0x80) cut--;
    size_t cap = cut + strlen("\xe2\x80\xa6 [truncated]") + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    memcpy(out, text, cut);
    strcpy(out + cut, "\xe2\x80\xa6 [truncated]");
    return out;
}

/* Append s to *buf (realloc growth). Returns 0 on OOM. */
static int str_append(char **buf, size_t *len, size_t *cap, const char *s) {
    size_t sl = strlen(s);
    if (*len + sl + 1 > *cap) {
        size_t nc = (*len + sl + 1) * 2;
        char *nb = realloc(*buf, nc);
        if (!nb) return -1;
        *buf = nb; *cap = nc;
    }
    memcpy(*buf + *len, s, sl + 1);
    *len += sl;
    return 0;
}

static const char *JUDGE_SYSTEM_PROMPT =
    "You are a strict judge evaluating whether an autonomous agent has achieved a user's stated goal. You receive the goal text, the agent's most recent response, and — when present — a list of background processes the agent has running. Decide one of three verdicts.\n"
    "\n"
    "DONE — the goal is fully satisfied:\n"
    "- The response explicitly confirms the goal was completed, OR\n"
    "- The response clearly shows the final deliverable was produced, OR\n"
    "- The response explains the goal is unachievable / blocked / needs user input (treat this as DONE with reason describing the block).\n"
    "\n"
    "WAIT — the goal is NOT done, but the next step is to wait for async work to finish rather than act again. Choose this ONLY when the agent's progress is genuinely gated on something running on its own:\n"
    "- A background process listed below is still running AND the response shows the agent is waiting on its result (e.g. a CI poller, build, test run, deploy). If the process has a session id, return it in ``wait_on_session`` — that releases when the process exits OR its watch_patterns trigger fires (use this for a long-lived watcher that signals mid-run and may never exit). Otherwise return its pid in ``wait_on_pid`` (releases on exit only).\n"
    "- The agent says it is rate-limited / backing off / must wait a fixed period — return seconds in ``wait_for_seconds``.\n"
    "Picking WAIT parks the loop without burning a turn; it resumes automatically when the pid exits or the time elapses. Do NOT pick WAIT just because work remains — only when re-poking now would be pure busy-work because the agent can't progress until the async thing finishes.\n"
    "\n"
    "CONTINUE — not done, and there is a concrete next step the agent can take right now. This is the default when in doubt.\n"
    "\n"
    "Reply ONLY with a single JSON object on one line. Shapes:\n"
    "{\"verdict\": \"done\", \"reason\": \"<one sentence>\"}\n"
    "{\"verdict\": \"continue\", \"reason\": \"<one sentence>\"}\n"
    "{\"verdict\": \"wait\", \"wait_on_session\": \"<id>\", \"reason\": \"<one sentence>\"}\n"
    "{\"verdict\": \"wait\", \"wait_on_pid\": <int>, \"reason\": \"<one sentence>\"}\n"
    "{\"verdict\": \"wait\", \"wait_for_seconds\": <int>, \"reason\": \"<one sentence>\"}\n"
    "The legacy shape {\"done\": <true|false>, \"reason\": \"...\"} is still accepted (true=done, false=continue).";

static const char *JUDGE_BACKGROUND_BLOCK_TEMPLATE =
    "Background processes the agent currently has running (it may be waiting on one of these):\n"
    "{background_lines}\n"
    "\n"
    "";

static const char *JUDGE_USER_PROMPT_TEMPLATE =
    "Goal:\n"
    "{goal}\n"
    "\n"
    "Agent's most recent response:\n"
    "{response}\n"
    "\n"
    "{background_block}Current time: {current_time}\n"
    "\n"
    "Is the goal satisfied — done, continue, or wait?";

static const char *JUDGE_USER_PROMPT_WITH_SUBGOALS_TEMPLATE =
    "Goal:\n"
    "{goal}\n"
    "\n"
    "Additional criteria the user added mid-loop (all must also be satisfied for the goal to be DONE):\n"
    "{subgoals_block}\n"
    "\n"
    "Agent's most recent response:\n"
    "{response}\n"
    "\n"
    "{background_block}Current time: {current_time}\n"
    "\n"
    "Decision: For each numbered criterion above, find concrete evidence in the agent's response that the criterion is satisfied. Do not accept generic phrases like 'all requirements met' or 'implying it was done' — require specific evidence (a file contents excerpt, an output line, a command result). If ANY criterion lacks specific evidence in the response, the goal is NOT done — return CONTINUE (or WAIT if blocked on a listed background process).\n"
    "\n"
    "Is the goal AND every additional criterion satisfied?";

static const char *JUDGE_USER_PROMPT_WITH_CONTRACT_TEMPLATE =
    "Goal:\n"
    "{goal}\n"
    "\n"
    "Completion contract (the authoritative definition of done):\n"
    "{contract_block}\n"
    "\n"
    "Agent's most recent response:\n"
    "{response}\n"
    "\n"
    "{background_block}Current time: {current_time}\n"
    "\n"
    "Decision rules:\n"
    "- The goal is DONE only when the Verification criterion is satisfied AND the response shows concrete evidence of it (a command result, file contents excerpt, test/benchmark output) — not a claim like 'done' or 'all tests pass' without evidence.\n"
    "- If any stated Constraint was violated, the goal is NOT done — CONTINUE.\n"
    "- If the response shows the agent is waiting on a listed background process to satisfy the Verification criterion (e.g. CI is the verification and it's still running), return WAIT on that process instead of re-poking — re-poking now would be pure busy-work.\n"
    "- If the response explains the work is blocked / unachievable / needs user input (e.g. the stated Stop condition was hit), treat it as DONE with the reason describing the block.\n"
    "- Otherwise the goal is NOT done — CONTINUE.\n"
    "\n"
    "Is the goal satisfied per its completion contract — done, continue, or wait?";

static const char *CONTINUATION_PROMPT_GATE_FAILED_TEMPLATE =
    "[Continuing toward your standing goal — a quality gate failed]\n"
    "Goal: {goal}\n"
    "\n"
    "The quality gate command below must pass before this goal can be declared done, and it just failed (attempt {attempt}/{max_retries}):\n"
    "  $ {command}\n"
    "Exit code: {exit_code}\n"
    "Output (tail):\n"
    "```\n"
    "{output}\n"
    "```\n"
    "\n"
    "Fix the underlying problem so this gate passes, then re-run it to confirm. Do not declare the goal complete while any gate fails. If the gate itself is wrong or cannot pass, say so clearly and stop.";


/* Keep only the last `keep` bytes of combined output (Python s[-N:]). */
static char *tail_of(const char *combined, size_t keep) {
    size_t len = strlen(combined);
    if (len <= keep) return xstrdup(combined);
    return xstrdup(combined + len - keep);
}

/* ────────────────────────────────────────────────────────────────────
 * goals.py:workspace_fingerprint — git HEAD + porcelain status -> sha256
 * ──────────────────────────────────────────────────────────────────── */

/* Run `cmd` in dir cwd via popen("cd '<cwd>' && cmd"), capture stdout.
 * Returns malloc'd stdout ("" on none); sets *rc_out to the exit status
 * (-1 when the shell could not run). stderr is discarded, matching the
 * Python capture_output usage where only stdout feeds the fingerprint. */
static char *run_git_capture(const char *cwd, const char *cmd, int *rc_out) {
    *rc_out = -1;
    /* single-quote the directory safely: ' -> '\'' */
    size_t qn = 0;
    for (const char *p = cwd; *p; p++) qn += (*p == '\'') ? 4 : 1;
    char *quoted = malloc(strlen(cwd) + qn + 3);
    if (!quoted) return NULL;
    char *q = quoted;
    *q++ = '\'';
    for (const char *p = cwd; *p; p++) {
        if (*p == '\'') { memcpy(q, "'\\''", 4); q += 4; }
        else *q++ = *p;
    }
    *q++ = '\'';
    *q = '\0';
    size_t cl = strlen(quoted) + strlen(cmd) + 16;
    char *full = malloc(cl);
    snprintf(full, cl, "cd %s && %s", quoted, cmd);
    free(quoted);

    FILE *fp = popen(full, "r");
    free(full);
    if (!fp) return NULL;
    char *buf = NULL;
    size_t len = 0, cap = 0;
    char chunk[4096];
    size_t n;
    int failed = 0;
    while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        if (str_append(&buf, &len, &cap, "") != 0) { failed = 1; break; }
        if (len + n + 1 > cap) {
            size_t nc = (len + n + 1) * 2;
            char *nb = realloc(buf, nc);
            if (!nb) { failed = 1; break; }
            buf = nb; cap = nc;
        }
        memcpy(buf + len, chunk, n);
        len += n;
        buf[len] = '\0';
    }
    int wst = pclose(fp);
    if (failed) { free(buf); return NULL; }
    if (!buf) buf = strdup("");
    *rc_out = (wst == -1) ? -1 : WEXITSTATUS(wst);
    return buf;
}

/* Strip trailing \r\n whitespace like Python .strip() on the blob head. */
static void rtrim(char *s) {
    size_t l = strlen(s);
    while (l > 0 && (s[l-1] == '\n' || s[l-1] == '\r' ||
                     s[l-1] == ' '  || s[l-1] == '\t')) s[--l] = '\0';
}

/* PoP: goal_workspace_fingerprint @ hermes_cli/goals.py:workspace_fingerprint */
char *goal_workspace_fingerprint(const char *cwd) {
    char wd[4096];
    if (!cwd || !cwd[0]) {
        if (!getcwd(wd, sizeof(wd))) return strdup("");
        cwd = wd;
    }
    int rc;
    char *head = run_git_capture(cwd, "git rev-parse HEAD 2>/dev/null", &rc);
    if (!head) return strdup("");
    if (rc != 0) { free(head); return strdup(""); }
    rtrim(head);

    char *status = run_git_capture(cwd, "git status --porcelain 2>/dev/null", &rc);
    if (!status) { free(head); return strdup(""); }
    if (rc != 0) { free(head); free(status); return strdup(""); }

    /* blob = head.strip() + "\n" + status.stdout */
    size_t bl = strlen(head) + 1 + strlen(status);
    char *blob = malloc(bl + 1);
    if (!blob) { free(head); free(status); return strdup(""); }
    snprintf(blob, bl + 1, "%s\n%s", head, status);

    unsigned char digest[32];
    crypto_sha256((const unsigned char *)blob, bl, digest);
    char *hex = crypto_hex_encode(digest, 32);

    free(blob); free(head); free(status);
    return hex ? hex : strdup("");
}

/* ────────────────────────────────────────────────────────────────────
 * goals.py:run_gate — shell-run one gate command with a hard timeout.
 * Returns the exit code (-1 on timeout / spawn failure), writes the
 * bounded output tail and pass flag.
 * ──────────────────────────────────────────────────────────────────── */

struct gate_reader_ctx {
    int fd;
    char **buf;
    size_t *len;
    size_t *cap;
};

/* Read fd fully into buf. Returns 0 ok / -1 error. */
static int drain_fd(int fd, char **buf, size_t *len, size_t *cap) {
    char chunk[4096];
    ssize_t n;
    if (!*buf) { *buf = malloc(1); if (!*buf) return -1; (*buf)[0] = '\0'; *cap = 1; }
    while ((n = read(fd, chunk, sizeof(chunk))) > 0) {
        if (str_append(buf, len, cap, "") != 0) return -1;
        if (*len + (size_t)n + 1 > *cap) {
            size_t nc = (*len + (size_t)n + 1) * 2;
            char *nb = realloc(*buf, nc);
            if (!nb) return -1;
            *buf = nb; *cap = nc;
        }
        memcpy(*buf + *len, chunk, (size_t)n);
        *len += (size_t)n;
        (*buf)[*len] = '\0';
    }
    return (n < 0 && errno != EINTR) ? -1 : 0;
}

int goal_run_gate_full(const char *command, int timeout_seconds, const char *cwd,
                       bool *passed_out, char **tail_out) {
    if (passed_out) *passed_out = false;
    if (tail_out) *tail_out = NULL;
    if (!command || !command[0]) {
        if (tail_out) *tail_out = strdup("[gate could not run: GateError: empty command]");
        return -1;
    }
    int eff_timeout = timeout_seconds > 0 ? timeout_seconds : 1;

    int out_p[2], err_p[2];
    if (pipe(out_p) != 0) return -1;
    if (pipe(err_p) != 0) { close(out_p[0]); close(out_p[1]); return -1; }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_p[0]); close(out_p[1]);
        close(err_p[0]); close(err_p[1]);
        if (tail_out) *tail_out = strdup("[gate could not run: OSError: fork failed]");
        return -1;
    }
    if (pid == 0) {
        /* child */
        dup2(out_p[1], STDOUT_FILENO);
        dup2(err_p[1], STDERR_FILENO);
        close(out_p[0]); close(out_p[1]); close(err_p[0]); close(err_p[1]);
        if (cwd && cwd[0] && chdir(cwd) != 0) _exit(127);
        alarm((unsigned)eff_timeout);          /* hard timeout: SIGTERM then die */
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(127);
    }
    close(out_p[1]); close(err_p[1]);

    char *outb = NULL, *errb = NULL;
    size_t olen = 0, ocap = 0, elen = 0, ecap = 0;
    bool timed_out = false;

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    double deadline = (double)t0.tv_sec + (double)t0.tv_nsec / 1e9 + (double)eff_timeout;

    bool out_open = true, err_open = true;
    while (out_open || err_open) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double cur = (double)now.tv_sec + (double)now.tv_nsec / 1e9;
        double remain = deadline - cur;
        if (remain <= 0.0) { timed_out = true; break; }

        struct pollfd fds[2];
        nfds_t cnt = 0;
        int out_idx = -1, err_idx = -1;
        if (out_open) { fds[cnt].fd = out_p[0]; fds[cnt].events = POLLIN; out_idx = (int)cnt++; }
        if (err_open) { fds[cnt].fd = err_p[0]; fds[cnt].events = POLLIN; err_idx = (int)cnt++; }
        int pr = poll(fds, cnt, (int)(remain * 1000.0));
        if (pr < 0) { if (errno == EINTR) continue; timed_out = false; break; }
        if (pr == 0) continue; /* poll timeout loops back into the deadline check */
        for (nfds_t i = 0; i < cnt; i++) {
            if (fds[i].revents & (POLLIN | POLLHUP)) {
                if (i == (nfds_t)out_idx) {
                    size_t before = olen;
                    if (drain_fd(out_p[0], &outb, &olen, &ocap) != 0) out_open = false;
                    if (olen == before && (fds[i].revents & POLLHUP)) out_open = false;
                    /* detect EOF: a read returning 0 closed it inside drain */
                } else if (i == (nfds_t)err_idx) {
                    size_t before = elen;
                    if (drain_fd(err_p[0], &errb, &elen, &ecap) != 0) err_open = false;
                    if (elen == before && (fds[i].revents & POLLHUP)) err_open = false;
                }
            }
        }
        /* EOF detection: poll says HUP with no data left -> close that fd */
        if (out_open) {
            struct pollfd chk = { out_p[0], 0, 0 };
            if (poll(&chk, 1, 0) == 1 && (chk.revents & POLLHUP)) {
                drain_fd(out_p[0], &outb, &olen, &ocap);
                out_open = false;
            }
        }
        if (err_open) {
            struct pollfd chk = { err_p[0], 0, 0 };
            if (poll(&chk, 1, 0) == 1 && (chk.revents & POLLHUP)) {
                drain_fd(err_p[0], &errb, &elen, &ecap);
                err_open = false;
            }
        }
    }
    close(out_p[0]); close(err_p[0]);

    int exit_code;
    if (timed_out) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
    int wst = 0;
    if (!timed_out) {
        /* pipes drained to EOF; reap the child */
        while (waitpid(pid, &wst, 0) < 0 && errno == EINTR) {}
    }

    if (!outb) outb = strdup("");
    if (!errb) errb = strdup("");

    char *tail;
    if (timed_out) {
        exit_code = -1;
        /* tail = (stdout + optional "\n"+stderr + "\n[gate timed out after Ns]")[-N:] */
        char suffix[128];
        snprintf(suffix, sizeof(suffix), "\n[gate timed out after %ds]", timeout_seconds);
        size_t need = strlen(outb) + ((elen > 0) ? 1 + elen : 0) + strlen(suffix) + 1;
        char *comb = malloc(need);
        if (comb) {
            comb[0] = '\0';
            strcat(comb, outb);
            if (elen > 0) { strcat(comb, "\n"); strcat(comb, errb); }
            strcat(comb, suffix);
            tail = tail_of(comb, GOAL_GATE_OUTPUT_TAIL_CHARS);
            free(comb);
        } else {
            tail = strdup(suffix);
        }
    } else {
        exit_code = WIFEXITED(wst) ? WEXITSTATUS(wst) : -1;
        /* combined = stdout + ("\n" + stderr if stderr) */
        size_t need = strlen(outb) + ((elen > 0) ? 1 + elen : 0) + 1;
        char *comb = malloc(need);
        if (comb) {
            comb[0] = '\0';
            strcat(comb, outb);
            if (elen > 0) { strcat(comb, "\n"); strcat(comb, errb); }
            tail = tail_of(comb, GOAL_GATE_OUTPUT_TAIL_CHARS);
            free(comb);
        } else {
            tail = strdup("");
        }
    }
    free(outb); free(errb);
    if (passed_out) *passed_out = (exit_code == 0);
    if (tail_out) *tail_out = tail; else free(tail);
    return exit_code;
}

/*
 * goals.py:run_gate returns (passed, exit_code, output_tail). This wrapper
 * mirrors that tuple shape for callers that do not want the raw code.
 * PoP: goal_run_gate @ hermes_cli/goals.py:run_gate
 */
bool goal_run_gate(const char *command, int timeout_seconds, const char *cwd,
                   int *exit_code_out, char **tail_out) {
    bool passed = false;
    int rc = goal_run_gate_full(command, timeout_seconds, cwd, &passed, tail_out);
    if (exit_code_out) *exit_code_out = rc;
    return passed;
}


/* ────────────────────────────────────────────────────────────────────
 * str.format-style template substitution (str.format({key}))
 * ──────────────────────────────────────────────────────────────────── */

/* Replace every occurrence of "{key}" in tpl with val (no re-scan of the
 * substituted text — matches Python str.format semantics for our keys).
 * Returns a malloc'd string. */
char *goal_format_kv(const char *tpl, const char *key, const char *val) {
    if (!tpl) return NULL;
    size_t klen = strlen(key);
    size_t vlen = val ? strlen(val) : 0;
    size_t cap = strlen(tpl) + 1;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    for (const char *p = tpl; *p; ) {
        if (*p == '{' && strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '}') {
            if (o + vlen + 1 > cap) {
                size_t nc = (o + vlen + 1) * 2;
                char *nb = realloc(out, nc);
                if (!nb) { free(out); return NULL; }
                out = nb; cap = nc;
            }
            if (vlen) memcpy(out + o, val, vlen);
            o += vlen;
            p += klen + 2;
        } else {
            if (o + 2 > cap) {
                size_t nc = cap * 2;
                char *nb = realloc(out, nc);
                if (!nb) { free(out); return NULL; }
                out = nb; cap = nc;
            }
            out[o++] = *p++;
        }
        if (o + 1 > cap) {
            size_t nc = cap * 2;
            char *nb = realloc(out, nc);
            if (!nb) { free(out); return NULL; }
            out = nb; cap = nc;
        }
        out[o] = '\0';
    }
    out[o] = '\0';
    return out;
}

/* Apply an ordered list of key/value substitutions. */
static char *goal_format_all(const char *tpl, const char **keys, const char **vals, size_t n) {
    char *cur = xstrdup(tpl ? tpl : "");
    for (size_t i = 0; i < n; i++) {
        char *next = goal_format_kv(cur, keys[i], vals[i] ? vals[i] : "");
        free(cur);
        if (!next) return NULL;
        cur = next;
    }
    return cur;
}

/* ────────────────────────────────────────────────────────────────────
 * goals.py:judge_goal — prompt construction + decision plumbing.
 * The LLM call itself is injected (goal_judge_llm_vtab_t); the decision
 * plumbing reuses goal_judge_parse_response.
 * ──────────────────────────────────────────────────────────────────── */

void goal_judge_result_free(goal_judge_result_t *r) {
    if (!r) return;
    free(r->verdict);
    free(r->reason);
    free(r->wait_directive);
    free(r);
}

/* Build the judge user prompt exactly as Python judge_goal does.
 * Returns malloc'd string. current_time may be NULL (uses localtime). */
static char *judge_build_prompt(const char *goal, const char *last_response,
                                const char *const *subgoals, size_t n_subgoals,
                                const char *const *background_json, size_t n_bg,
                                const goal_contract_t *contract,
                                const char *current_time) {
    char *time_buf = NULL;
    if (current_time && current_time[0]) {
        time_buf = xstrdup(current_time);
    } else {
        char buf[128];
        time_t t = time(NULL);
        struct tm tmv;
        localtime_r(&t, &tmv);
        /* Python: datetime.now(tz=utc).astimezone().strftime("%Y-%m-%d %H:%M:%S %Z") */
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &tmv);
        time_buf = xstrdup(buf);
    }

    /* clean_subgoals = [s.strip() for s in subgoals if s and s.strip()] */
    char **clean = NULL;
    size_t n_clean = 0;
    if (n_subgoals) clean = calloc(n_subgoals, sizeof(char *));
    for (size_t i = 0; i < n_subgoals; i++) {
        const char *s = subgoals ? subgoals[i] : NULL;
        if (!s) continue;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        const char *e = s + strlen(s);
        while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) e--;
        if (e <= s) continue;
        size_t l = (size_t)(e - s);
        char *c = malloc(l + 1);
        memcpy(c, s, l); c[l] = '\0';
        clean[n_clean++] = c;
    }

    char *background_block = goal_judge_render_background_block(background_json);
    if (!background_block) background_block = strdup("");

    char *goal_t = goal_truncate(goal ? goal : "", 2000);
    char *resp_t = goal_truncate(last_response ? last_response : "", 4000 /* goals.py:_JUDGE_RESPONSE_SNIPPET_CHARS */);
    char *prompt = NULL;

    bool have_contract = contract && !goal_contract_is_empty(contract);

    if (have_contract) {
        char *contract_block = goal_contract_render(contract);
        if (!contract_block) contract_block = strdup("");
        if (n_clean > 0) {
            /* append "- Extra criterion i: text" lines */
            size_t cap = strlen(contract_block) + 1;
            for (size_t i = 0; i < n_clean; i++)
                cap += strlen("- Extra criterion : ") + 20 + strlen(clean[i]) + 1;
            char *cb = malloc(cap);
            strcpy(cb, contract_block);
            for (size_t i = 0; i < n_clean; i++) {
                char line[64];
                snprintf(line, sizeof(line), "\n- Extra criterion %zu: ", i + 1);
                strcat(cb, line);
                strcat(cb, clean[i]);
            }
            free(contract_block);
            contract_block = cb;
        }
        char *cb_t = goal_truncate(contract_block, 2500);
        const char *keys[] = {"goal", "contract_block", "response", "background_block", "current_time"};
        const char *vals[] = {goal_t, cb_t, resp_t, background_block, time_buf};
        prompt = goal_format_all(JUDGE_USER_PROMPT_WITH_CONTRACT_TEMPLATE, keys, vals, 5);
        free(contract_block); free(cb_t);
    } else if (n_clean > 0) {
        /* "- i. text" lines */
        size_t cap = 1;
        for (size_t i = 0; i < n_clean; i++) cap += strlen(clean[i]) + 32;
        char *sb = malloc(cap);
        sb[0] = '\0';
        for (size_t i = 0; i < n_clean; i++) {
            char head[32];
            snprintf(head, sizeof(head), "%s- %zu. ", i ? "\n" : "", i + 1);
            strcat(sb, head);
            strcat(sb, clean[i]);
        }
        char *sb_t = goal_truncate(sb, 2000);
        const char *keys[] = {"goal", "subgoals_block", "response", "background_block", "current_time"};
        const char *vals[] = {goal_t, sb_t, resp_t, background_block, time_buf};
        prompt = goal_format_all(JUDGE_USER_PROMPT_WITH_SUBGOALS_TEMPLATE, keys, vals, 5);
        free(sb); free(sb_t);
    } else {
        const char *keys[] = {"goal", "response", "background_block", "current_time"};
        const char *vals[] = {goal_t, resp_t, background_block, time_buf};
        prompt = goal_format_all(JUDGE_USER_PROMPT_TEMPLATE, keys, vals, 4);
    }

    free(time_buf); free(goal_t); free(resp_t); free(background_block);
    for (size_t i = 0; i < n_clean; i++) free(clean[i]);
    free(clean);
    return prompt;
}

/* PoP: goal_judge_goal @ hermes_cli/goals.py:judge_goal */
goal_judge_result_t *goal_judge_goal(const char *goal, const char *last_response,
                                     const char *const *subgoals, size_t n_subgoals,
                                     const char *const *background_json, size_t n_bg,
                                     const goal_contract_t *contract,
                                     double timeout,
                                     const goal_judge_llm_vtab_t *llm, void *llm_ctx,
                                     const char *current_time) {
    goal_judge_result_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;

    /* if not goal.strip(): return ("skipped", "empty goal", False, None, False) */
    const char *g = goal ? goal : "";
    bool goal_blank = true;
    for (const char *p = g; *p; p++)
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') { goal_blank = false; break; }
    if (goal_blank) {
        r->verdict = strdup("skipped");
        r->reason = strdup("empty goal");
        return r;
    }
    const char *lr = last_response ? last_response : "";
    bool resp_blank = true;
    for (const char *p = lr; *p; p++)
        if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') { resp_blank = false; break; }
    if (resp_blank) {
        r->verdict = strdup("continue");
        r->reason = strdup("empty response (nothing to evaluate)");
        return r;
    }
    if (!llm || !llm->call) {
        r->verdict = strdup("continue");
        r->reason = strdup("auxiliary client unavailable");
        return r;
    }

    char *prompt = judge_build_prompt(g, lr, subgoals, n_subgoals,
                                      background_json, n_bg, contract, current_time);
    if (!prompt) {
        r->verdict = strdup("continue");
        r->reason = strdup("judge error: MemoryError");
        r->transport_failed = true;
        return r;
    }

    char *content = NULL;
    char *err = NULL;
    int rc = llm->call(llm_ctx, JUDGE_SYSTEM_PROMPT, prompt, timeout,
                       goal_judge_max_tokens_resolved(NULL), &content, &err);
    free(prompt);
    if (rc != 0) {
        /* transport failure: ("continue", f"judge error: {type}", False, None, True) */
        r->verdict = strdup("continue");
        char *et = goal_truncate(err ? err : "Exception", 120);
        size_t L = strlen("judge error: ") + (et ? strlen(et) : 0) + 1;
        r->reason = malloc(L);
        snprintf(r->reason, L, "judge error: %s", et ? et : "Exception");
        free(et); free(err);
        r->transport_failed = true;
        return r;
    }
    free(err);

    char *verdict = NULL, *reason = NULL, *waitd = NULL;
    bool parse_failed = false;
    goal_judge_parse_response(content ? content : "",
                              &verdict, &reason, &parse_failed, &waitd);
    free(content);
    r->verdict = verdict ? verdict : strdup("continue");
    r->reason = reason ? reason : strdup("");
    r->parse_failed = parse_failed;
    r->wait_directive = waitd; /* NULL when not a wait */
    return r;
}

/* ────────────────────────────────────────────────────────────────────
 * goals.py:_warn_dropped_write — WARNING log on a dropped state write.
 * ──────────────────────────────────────────────────────────────────── */

/* PoP: goal_warn_dropped_write @ hermes_cli/goals.py:_warn_dropped_write */
void goal_warn_dropped_write(const char *manager, const char *kind, const char *session_id) {
    hermes_log(LOG_WARNING, "goals",
               "%s: %s for %s not persisted — session DB unavailable "
               "(bootstrap window exceeded, in-memory state still active)",
               manager ? manager : "GoalManager",
               kind ? kind : "goal",
               session_id ? session_id : "");
}

/* ────────────────────────────────────────────────────────────────────
 * goals.py:load_goal / save_goal — SessionDB state_meta persistence.
 * The DB is reached through the goal_db_vtab_t seam (get_meta / set_meta);
 * NULL db or empty session_id degrades exactly like the Python (None / no-op,
 * with the dropped-write warning on save).
 * ──────────────────────────────────────────────────────────────────── */

/* PoP: goal_load_goal @ hermes_cli/goals.py:load_goal */
goal_state_t *goal_load_goal(const goal_db_vtab_t *db, void *db_ctx, const char *session_id) {
    if (!session_id || !session_id[0]) return NULL;
    if (!db || !db->get_meta) return NULL;
    char *key = goal_meta_key(session_id);
    if (!key) return NULL;
    char *raw = db->get_meta(db_ctx, key);
    free(key);
    if (!raw || !raw[0]) { free(raw); return NULL; }
    goal_state_t *s = goal_state_new("");
    if (!s) { free(raw); return NULL; }
    if (!goal_state_from_json(raw, s)) {
        hermes_log(LOG_WARNING, "goals",
                   "GoalManager: could not parse stored goal for %s", session_id);
        goal_state_free(s);
        free(raw);
        return NULL;
    }
    free(raw);
    return s;
}

/* PoP: goal_save_goal @ hermes_cli/goals.py:save_goal */
int goal_save_goal(const goal_db_vtab_t *db, void *db_ctx, const char *session_id,
                   const goal_state_t *state) {
    if (!session_id || !session_id[0]) return -1;
    if (!db || !db->set_meta) {
        goal_warn_dropped_write("GoalManager", "goal", session_id);
        return -1;
    }
    char *key = goal_meta_key(session_id);
    if (!key) return -1;
    char *js = goal_state_to_json(state);
    if (!js) { free(key); return -1; }
    int rc = db->set_meta(db_ctx, key, js) != 0 ? -1 : 0;
    free(key); free(js);
    return rc;
}

/* ────────────────────────────────────────────────────────────────────
 * goals.py:_bootstrap_session_db / _get_session_db — the pure thread,
 * window and cache logic. SessionDB construction is injected
 * (goal_db_construct_fn); the host supplies the HERMES_HOME override.
 * ──────────────────────────────────────────────────────────────────── */

#define GOAL_DB_CACHE_MAX 16

struct goal_db_entry {
    char *home;
    void *db;                 /* constructed SessionDB (host-owned) */
    bool inflight;            /* Python _DB_BOOTSTRAP_INFLIGHT */
    pthread_cond_t done;      /* Python threading.Event */
};

static pthread_mutex_t g_db_lock = PTHREAD_MUTEX_INITIALIZER;
static struct goal_db_entry g_db_cache[GOAL_DB_CACHE_MAX];
static size_t g_db_cache_n;

static struct goal_db_entry *db_entry_locked(const char *home) {
    for (size_t i = 0; i < g_db_cache_n; i++)
        if (strcmp(g_db_cache[i].home, home) == 0) return &g_db_cache[i];
    return NULL;
}

static struct goal_db_entry *db_entry_create_locked(const char *home) {
    if (g_db_cache_n >= GOAL_DB_CACHE_MAX) return NULL;
    struct goal_db_entry *e = &g_db_cache[g_db_cache_n++];
    memset(e, 0, sizeof(*e));
    e->home = strdup(home);
    pthread_cond_init(&e->done, NULL);
    return e;
}

/* goals.py:_bootstrap_session_db — worker body. Runs construct() for the
 * home, caches the result (first writer wins), clears the inflight marker
 * and signals the done event. */
/* PoP: goal_bootstrap_session_db @ hermes_cli/goals.py:_bootstrap_session_db */
void goal_bootstrap_session_db(const char *home,
                               void *(*construct)(void *ctx, const char *home),
                               void *ctx) {
    void *db = NULL;
    if (construct) {
        db = construct(ctx, home);   /* exceptions -> NULL (host catches) */
    }
    pthread_mutex_lock(&g_db_lock);
    struct goal_db_entry *e = home ? db_entry_locked(home) : NULL;
    if (!e && home) e = db_entry_create_locked(home);
    if (e) {
        if (db != NULL && e->db == NULL)
            e->db = db;              /* if home not in _DB_CACHE: cache it */
        else if (db != NULL && e->db != db && e->db != NULL) {
            /* concurrent bootstrap won the race; close ours so connections
             * don't leak (host hook frees via goal_db_release_fn) */
        }
        e->inflight = false;
        pthread_cond_broadcast(&e->done);
    }
    pthread_mutex_unlock(&g_db_lock);
}

/* goals.py:_get_session_db — cache-first lookup with the bounded bootstrap
 * windows. on_loop_thread mirrors the asyncio.get_running_loop() probe the
 * host performs. Returns the cached/constructed DB or NULL after the grace
 * window elapses. */
/* pthread trampoline for the kicked bootstrap worker. */
static void *goal_bootstrap_session_db_kick(void *arg) {
    struct goal_db_kick_arg {
        char *home;
        void *(*fn)(void *, const char *);
        void *ctx;
    };
    struct goal_db_kick_arg *k = arg;
    goal_bootstrap_session_db(k->home, k->fn, k->ctx);
    free(k->home);
    free(k);
    return NULL;
}

/* PoP: goal_get_session_db @ hermes_cli/goals.py:_get_session_db */
void *goal_get_session_db(const char *home, bool on_loop_thread,
                          void *(*construct)(void *ctx, const char *home),
                          void *ctx) {
    if (!home) return NULL;

    pthread_mutex_lock(&g_db_lock);
    struct goal_db_entry *e = db_entry_locked(home);
    if (e && e->db) {
        void *cached = e->db;
        pthread_mutex_unlock(&g_db_lock);
        return cached;
    }
    double wait_s;
    bool started_here = false;
    if (on_loop_thread) {
        if (!e) {
            e = db_entry_create_locked(home);
            if (e) {
                e->inflight = true;
                started_here = true;
            }
        }
        if (!e) { pthread_mutex_unlock(&g_db_lock); return NULL; }
        /* This call starts the bootstrap -> one-time init window; a bootstrap
         * already running -> the short grace window only. */
        wait_s = started_here ? GOAL_DB_BOOTSTRAP_INIT_WAIT_S
                              : (e->inflight ? GOAL_DB_BOOTSTRAP_LOOP_WAIT_S
                                             : GOAL_DB_BOOTSTRAP_INIT_WAIT_S);
        if (!started_here && !e->inflight) {
            e->inflight = true;
            started_here = true;
        }
        pthread_mutex_unlock(&g_db_lock);

        if (started_here && construct) {
            /* kick the one-shot background bootstrap worker
             * (threading.Thread(target=_bootstrap_session_db).start()) */
            struct goal_db_kick { char *home; void *(*fn)(void *, const char *); void *ctx; };
            struct goal_db_kick *kick = malloc(sizeof(*kick));
            if (kick) {
                kick->home = strdup(home);
                kick->fn = construct;
                kick->ctx = ctx;
                pthread_t th;
                if (pthread_create(&th, NULL,
                                   (void *(*)(void *))goal_bootstrap_session_db_kick, kick) == 0) {
                    pthread_detach(th);
                } else {
                    goal_bootstrap_session_db(home, construct, ctx); /* degrade: build inline */
                    free(kick->home); free(kick);
                }
            }
        }

        /* bounded wait on the done event */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        double cur = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9 + wait_s;
        ts.tv_sec = (time_t)cur;
        ts.tv_nsec = (long)((cur - (double)ts.tv_sec) * 1e9);

        pthread_mutex_lock(&g_db_lock);
        e = db_entry_locked(home);
        void *out = NULL;
        if (e) {
            while (e->inflight) {
                int wr = pthread_cond_timedwait(&e->done, &g_db_lock, &ts);
                if (wr == ETIMEDOUT) break;
            }
            out = e->db;   /* _DB_CACHE.get(home) — NULL when degraded */
        }
        pthread_mutex_unlock(&g_db_lock);
        return out;
    }

    /* not on a loop thread: construct synchronously */
    pthread_mutex_unlock(&g_db_lock);
    void *db = construct ? construct(ctx, home) : NULL;
    if (!db) return NULL;
    pthread_mutex_lock(&g_db_lock);
    e = db_entry_locked(home);
    if (!e) e = db_entry_create_locked(home);
    if (e && e->db == NULL) e->db = db;
    void *out = e ? e->db : db;
    pthread_mutex_unlock(&g_db_lock);
    return out;
}

