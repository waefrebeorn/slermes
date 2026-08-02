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
int appr_u_prepare_smart_approval_observer(const char *arg) { (void)arg; return 0; }

/* PoP: _observe_smart_approval_verdict @ tools/approval.py:_observe_smart_approval_verdict */
int appr_u_observe_smart_approval_verdict(const char *arg) { (void)arg; return 0; }

/* PoP: _match_user_deny_rule @ tools/approval.py:_match_user_deny_rule */
int appr_u_match_user_deny_rule(const char *arg) { (void)arg; return 0; }

/* PoP: _user_deny_block_result @ tools/approval.py:_user_deny_block_result */
int appr_u_user_deny_block_result(const char *arg) { (void)arg; return 0; }

/* PoP: _command_parser_limit_exceeded @ tools/approval.py:_command_parser_limit_exceeded */
int appr_u_command_parser_limit_exceeded(const char *arg) { (void)arg; return 0; }

/* PoP: _shell_tokens_with_spans @ tools/approval.py:_shell_tokens_with_spans */
int appr_u_shell_tokens_with_spans(const char *arg) { (void)arg; return 0; }

/* PoP: _quoted_grep_pattern_spans @ tools/approval.py:_quoted_grep_pattern_spans */
int appr_u_quoted_grep_pattern_spans(const char *arg) { (void)arg; return 0; }

/* PoP: _grep_safe_detection_variant @ tools/approval.py:_grep_safe_detection_variant */
int appr_u_grep_safe_detection_variant(const char *arg) { (void)arg; return 0; }

/* PoP: _interpreter_family @ tools/approval.py:_interpreter_family */
int appr_u_interpreter_family(const char *arg) { (void)arg; return 0; }

/* PoP: _shell_segment_tokens @ tools/approval.py:_shell_segment_tokens */
int appr_u_shell_segment_tokens(const char *arg) { (void)arg; return 0; }

/* PoP: _iter_top_level_shell_segments @ tools/approval.py:_iter_top_level_shell_segments */
int appr_u_iter_top_level_shell_segments(const char *arg) { (void)arg; return 0; }

/* PoP: _split_option @ tools/approval.py:_split_option */
int appr_u_split_option(const char *arg) { (void)arg; return 0; }

/* PoP: _interpreter_exec_flag @ tools/approval.py:_interpreter_exec_flag */
int appr_u_interpreter_exec_flag(const char *arg) { (void)arg; return 0; }

/* PoP: _bash_exec_payload @ tools/approval.py:_bash_exec_payload */
int appr_u_bash_exec_payload(const char *arg) { (void)arg; return 0; }

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
int appr_u_execution_flag_findings(const char *arg) { (void)arg; return 0; }

/* PoP: _is_verification_artifact_cleanup @ tools/approval.py:_is_verification_artifact_cleanup */
int appr_u_is_verification_artifact_cleanup(const char *arg) { (void)arg; return 0; }

/* PoP: _run_approval_gate @ tools/approval.py:_run_approval_gate */
int appr_u_run_approval_gate(const char *arg) { (void)arg; return 0; }

/* PoP: request_tool_approval @ tools/approval.py:request_tool_approval */
int appr_request_tool_approval(const char *arg) { (void)arg; return 0; }
