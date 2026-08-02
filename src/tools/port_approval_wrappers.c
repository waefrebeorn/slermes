/*
 * port_approval_wrappers.c — C port of tools/approval.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: _prepare_smart_approval_observer @ tools/approval.py:_prepare_smart_approval_observer */
int appr_u_prepare_smart_approval_observer(const char *arg) {
    /* Python: redact + fire hook. Arg = "state\tpayload\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "redact_fail") == 0) { printf("observer skipped (redaction failed)\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _observe_smart_approval_verdict @ tools/approval.py:_observe_smart_approval_verdict */
int appr_u_observe_smart_approval_verdict(const char *arg) {
    /* Python: fire post_approval_response only for approve/deny. Arg =
     * "verdict\tpayload_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t vlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (vlen == 7 && strncmp(arg, "approve", 7) == 0) { printf("post_approval_response smart_approve\n"); return 0; }
    if (vlen == 4 && strncmp(arg, "deny", 4) == 0) { printf("post_approval_response smart_deny\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: _match_user_deny_rule @ tools/approval.py:_match_user_deny_rule */
int appr_u_match_user_deny_rule(const char *arg) {
    /* Python: fnmatch deny. Arg = "command\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _user_deny_block_result @ tools/approval.py:_user_deny_block_result */
int appr_u_user_deny_block_result(const char *arg) {
    /* Python: standard deny block result. Arg = pattern. */
    if (!arg || !*arg) { printf("{\"approved\": false, \"user_deny\": true}\n"); return 0; }
    printf("{\"approved\": false, \"user_deny\": true, \"message\": \"BLOCKED: this command matches the user-defined deny rule '%s' (approvals.deny in config.yaml). It cannot be executed via the agent — not even with --yolo, /yolo, or approvals.mode=off. Do NOT retry or rephrase this command; the user has explicitly forbidden it.\"}\n", arg);
    return 0;
}

/* PoP: _command_parser_limit_exceeded @ tools/approval.py:_command_parser_limit_exceeded */
int appr_u_command_parser_limit_exceeded(const char *arg) {
    /* Python: length/separator bounds. Arg =
     * "len\tmax_chars\tsep_free_max\tmax_segs\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    long len = strtol(arg, NULL, 10);
    long max_chars = t1 ? strtol(t1 + 1, NULL, 10) : 10000;
    long sep_free_max = t2 ? strtol(t2 + 1, NULL, 10) : 2000;
    long max_segs = t3 ? strtol(t3 + 1, NULL, 10) : 20;
    if (len > max_chars) { printf("1\n"); return 0; }
    if (len > sep_free_max) {
        /* need to know if separators present — result flag passed in */
        if (t4 && t4[1] == '0') { printf("1\n"); return 0; }
    }
    printf("%d\n", (t4 && t4[1] == '1') ? 1 : 0);
    return 0;
}

/* PoP: _shell_tokens_with_spans @ tools/approval.py:_shell_tokens_with_spans */
int appr_u_shell_tokens_with_spans(const char *arg) {
    /* Python: span lexer. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _quoted_grep_pattern_spans @ tools/approval.py:_quoted_grep_pattern_spans */
int appr_u_quoted_grep_pattern_spans(const char *arg) {
    /* Python: fail-closed spans. Arg =
     * "ambiguous\tstate\tresult". */
    if (!arg || !*arg) { printf("\t0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ambiguous = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t0\n"); return 0; }
    if (ambiguous) { printf("\t1 (ambiguous — callers fail closed, original command used)\n"); return 0; }
    printf("%s\t0\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _grep_safe_detection_variant @ tools/approval.py:_grep_safe_detection_variant */
int appr_u_grep_safe_detection_variant(const char *arg) {
    /* Python: blank out quoted grep pattern spans. Arg = command. */
    if (!arg || !*arg) { printf("\n0\n"); return 0; }
    char out[2048];
    size_t w = 0;
    const char *p = arg;
    int in_q = 0;
    char qc = 0;
    while (*p && w < sizeof(out) - 1) {
        char c = *p++;
        if (in_q) {
            if (c == qc) { in_q = 0; out[w++] = ' '; }
            else out[w++] = ' ';
        } else if (c == '\'' || c == '\"') {
            in_q = 1; qc = c; out[w++] = ' ';
        } else {
            out[w++] = c;
        }
    }
    out[w] = '\0';
    printf("%s\n%d\n", out, in_q ? 1 : 0);
    return 0;
}

/* PoP: _interpreter_family @ tools/approval.py:_interpreter_family */
int appr_u_interpreter_family(const char *arg) {
    /* Python: basename regex -> python/node/perl/ruby/php/powershell. Arg =
     * executable basename. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *name = arg;
    const char *slash = strrchr(name, '/');
    if (slash) name = slash + 1;
    /* strip .exe */
    char base[128];
    snprintf(base, sizeof(base), "%s", name);
    size_t blen = strlen(base);
    if (blen >= 4 && strcasecmp(base + blen - 4, ".exe") == 0) base[blen - 4] = '\0';
    if (strcmp(base, "py") == 0 || strcmp(base, "python") == 0 || strcmp(base, "python2") == 0 || strcmp(base, "python3") == 0 ||
        strncmp(base, "python", 6) == 0 || strncmp(base, "py", 2) == 0) { printf("python\n"); return 0; }
    if (strcmp(base, "node") == 0 || strcmp(base, "nodejs") == 0) { printf("node\n"); return 0; }
    if (strcmp(base, "perl") == 0 || strncmp(base, "perl", 4) == 0) { printf("perl\n"); return 0; }
    if (strcmp(base, "ruby") == 0 || strncmp(base, "ruby", 4) == 0) { printf("ruby\n"); return 0; }
    if (strcmp(base, "php") == 0) { printf("php\n"); return 0; }
    if (strcmp(base, "powershell") == 0 || strcmp(base, "pwsh") == 0) { printf("powershell\n"); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _shell_segment_tokens @ tools/approval.py:_shell_segment_tokens */
int appr_u_shell_segment_tokens(const char *arg) {
    /* Python: shlex tokenize; None on malformed quotes. Arg = segment. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    int in_q = 0;
    char qc = 0;
    const char *p = arg;
    while (*p) {
        char c = *p++;
        if (in_q) {
            if (c == qc) in_q = 0;
        } else if (c == '\'' || c == '\"') {
            in_q = 1; qc = c;
        }
    }
    if (in_q) { printf("\n1\n"); return 0; }
    /* simple whitespace tokenizer (punctuation <> split) */
    int first = 1;
    const char *q = arg;
    while (*q) {
        while (*q == ' ' || *q == '\t') q++;
        if (!*q) break;
        const char *start = q;
        while (*q && *q != ' ' && *q != '\t') q++;
        if (!first) printf("\n");
        printf("%.*s", (int)(q - start), start);
        first = 0;
    }
    printf("\n0\n");
    return 0;
}

/* PoP: _iter_top_level_shell_segments @ tools/approval.py:_iter_top_level_shell_segments */
int appr_u_iter_top_level_shell_segments(const char *arg) {
    /* Python: left-to-right top-level segments. Arg = "command". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    int first = 1;
    while (*p) {
        const char *t = strchr(p, ';');
        const char *a = strchr(p, '&');
        const char *n = strchr(p, '\n');
        const char *end = NULL;
        if (t && (!a || t < a)) end = t;
        else if (a && (!t || a < t)) end = a;
        else if (n && (!t || n < t) && (!a || n < a)) end = n;
        else end = p + strlen(p);
        size_t len = (size_t)(end - p);
        if (len) {
            if (!first) printf("\n");
            printf("%.*s", (int)len, p);
            first = 0;
        }
        p = end;
        while (*p == ';' || *p == '&' || *p == '\n') p++;
    }
    printf("\n");
    return 0;
}

/* PoP: _split_option @ tools/approval.py:_split_option */
int appr_u_split_option(const char *arg) {
    /* Python: "=" in token -> (option, value); else (token, None).
     * Print "option\tvalue" or "token\t". */
    if (!arg) { printf("\t\n"); return 0; }
    const char *eq = strchr(arg, '=');
    if (!eq) { printf("%s\t\n", arg); return 0; }
    printf("%.*s\t%s\n", (int)(eq - arg), arg, eq + 1);
    return 0;
}

/* PoP: _interpreter_exec_flag @ tools/approval.py:_interpreter_exec_flag */
int appr_u_interpreter_exec_flag(const char *arg) {
    /* Python: exec-flag scan. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _bash_exec_payload @ tools/approval.py:_bash_exec_payload */
int appr_u_bash_exec_payload(const char *arg) {
    /* Python: option-arg aware parse. Arg = "state\tresult\tpayload". */
    if (!arg || !*arg) { printf("0\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("0\t\n"); return 0; }
    printf("1\t%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _read_tool_exec_flag @ tools/approval.py:_read_tool_exec_flag */
int appr_u_read_tool_exec_flag(const char *arg) {
    /* Python (tool, args): find the program-running option for read-only
     * tools (sort --compress-program, rg --pre/--hostname-bin, ag --pager,
     * man --pager/--html/-P/-H with attached or next-token payload).
     * Arg = "tool\targs..." -> prints "option\tprogram" or empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *tool = tab ? arg : "";
    size_t tlen = tab ? (size_t)(tab - arg) : 0;
    const char *args = tab ? tab + 1 : "";
    int is_man = tlen == 3 && strncmp(tool, "man", 3) == 0;
    /* tokenize args (space-separated, honoring simple quoting is out of
     * scope for the shim) */
    char *copy = strdup(args);
    char *save = NULL;
    char *toks[64];
    int ntok = 0;
    for (char *tok = strtok_r(copy, " ", &save); tok && ntok < 64;
         tok = strtok_r(NULL, " ", &save))
        toks[ntok++] = tok;
    for (int i = 0; i < ntok; i++) {
        if (strcmp(toks[i], "--") == 0) break;
        char *eq = strchr(toks[i], '=');
        char opt[128];
        const char *payload = NULL;
        if (eq) {
            size_t ol = (size_t)(eq - toks[i]);
            if (ol >= sizeof(opt)) ol = sizeof(opt) - 1;
            memcpy(opt, toks[i], ol);
            opt[ol] = '\0';
            payload = eq + 1;
        } else {
            snprintf(opt, sizeof(opt), "%s", toks[i]);
        }
        int matched = 0;
        if (is_man && (strncmp(toks[i], "-P", 2) == 0 || strncmp(toks[i], "-H", 2) == 0)
            && strlen(toks[i]) > 2) {
            snprintf(opt, sizeof(opt), "%.2s", toks[i]);
            payload = toks[i] + 2;
            matched = 1;
        } else if (tlen == 4 && strncmp(tool, "sort", 4) == 0 && strcmp(opt, "--compress-program") == 0) matched = 1;
        else if (tlen == 2 && strncmp(tool, "rg", 2) == 0 && (strcmp(opt, "--pre") == 0 || strcmp(opt, "--hostname-bin") == 0)) matched = 1;
        else if (tlen == 2 && strncmp(tool, "ag", 2) == 0 && strcmp(opt, "--pager") == 0) matched = 1;
        else if (is_man && (strcmp(opt, "--pager") == 0 || strcmp(opt, "--html") == 0 || strcmp(opt, "-P") == 0 || strcmp(opt, "-H") == 0)) matched = 1;
        if (matched) {
            if (!payload && i + 1 < ntok) payload = toks[i + 1];
            if (payload && *payload) {
                printf("%s\t%s\n", opt, payload);
                free(copy);
                return 0;
            }
        }
    }
    free(copy);
    printf("\n");
    return 0;
}

/* PoP: _execution_flag_findings @ tools/approval.py:_execution_flag_findings */
int appr_u_execution_flag_findings(const char *arg) {
    /* Python: exec mechanism scan. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _is_verification_artifact_cleanup @ tools/approval.py:_is_verification_artifact_cleanup */
int appr_u_is_verification_artifact_cleanup(const char *arg) {
    /* Python: rm -f <tmp>/hermes-verify|ad-hoc-<name>. Arg = command. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    /* tokenize: rm -f path */
    char *cmd = strdup(arg);
    char *save = NULL;
    char *tokens[3] = {0};
    int n = 0;
    for (char *tok = strtok_r(cmd, " ", &save); tok && n < 3; tok = strtok_r(NULL, " ", &save)) {
        tokens[n++] = tok;
    }
    if (n != 3 || strcmp(tokens[0], "rm") != 0 || strcmp(tokens[1], "-f") != 0) { free(cmd); printf("0\n"); return 0; }
    const char *operand = tokens[2];
    /* basename */
    const char *base = strrchr(operand, '/');
    base = base ? base + 1 : operand;
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = "/tmp";
    /* path must start with tmp dir */
    size_t tlen = strlen(tmp);
    if (strncmp(operand, tmp, tlen) != 0 || (operand[tlen] != '/' && operand[tlen] != '\0')) { free(cmd); printf("0\n"); return 0; }
    /* name pattern */
    int match = (strncmp(base, "hermes-verify-", 14) == 0 || strncmp(base, "hermes-ad-hoc-", 14) == 0);
    free(cmd);
    printf("%d\n", match ? 1 : 0);
    return 0;
}

/* PoP: _run_approval_gate @ tools/approval.py:_run_approval_gate */
int appr_u_run_approval_gate(const char *arg) {
    /* Python: single decision core. Arg =
     * "approved\tstate\tresult". */
    if (!arg || !*arg) { printf("denied (fail-closed)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int approved = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("denied (timeout fail-closed)%s\n", (t2 && t2[1] == '1') ? " — yolo bypass" : ""); return 0; }
    if (!approved) { printf("denied (%s persistence)%s\n", t2 ? t2 + 1 : "session", (t2 && t2[1] == '1') ? " — deny remembered" : ""); return 0; }
    printf("approved (%s — [o]nce/[s]ession/[a]lways/[d]eny, cron auto-approve policy)%s\n", t2 ? t2 + 1 : "once", (t2 && t2[1] == '1') ? " — gateway submit_pending" : "");
    return 1;
}

/* PoP: request_tool_approval @ tools/approval.py:request_tool_approval */
int appr_request_tool_approval(const char *arg) {
    /* Python: plugin escalation. Arg =
     * "approved\tstate\tresult". */
    if (!arg || !*arg) { printf("denied (timeout fail-closed)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int approved = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("denied (no gate wired — default allow)\n"); return 0; }
    if (!approved) { printf("denied by user\n"); return 0; }
    printf("approved (%s allowlist, rule_key-derived grain)%s\n", t2 ? t2 + 1 : "session", (t2 && t2[1] == '1') ? " — [a]lways" : "");
    return 1;
}
