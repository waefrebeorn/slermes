/* AUTO-GENERATED integration oracle harness for port_approval_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_approval_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int appr_u_prepare_smart_approval_observer(const char *);
extern int appr_u_observe_smart_approval_verdict(const char *);
extern int appr_u_match_user_deny_rule(const char *);
extern int appr_u_user_deny_block_result(const char *);
extern int appr_u_command_parser_limit_exceeded(const char *);
extern int appr_u_shell_tokens_with_spans(const char *);
extern int appr_u_quoted_grep_pattern_spans(const char *);
extern int appr_u_grep_safe_detection_variant(const char *);
extern int appr_u_interpreter_family(const char *);
extern int appr_u_shell_segment_tokens(const char *);
extern int appr_u_iter_top_level_shell_segments(const char *);
extern int appr_u_split_option(const char *);
extern int appr_u_interpreter_exec_flag(const char *);
extern int appr_u_bash_exec_payload(const char *);
extern int appr_u_read_tool_exec_flag(const char *);
extern int appr_u_execution_flag_findings(const char *);
extern int appr_u_is_verification_artifact_cleanup(const char *);
extern int appr_u_run_approval_gate(const char *);
extern int appr_request_tool_approval(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_appr_u_prepare_smart_approval_observer(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_prepare_smart_approval_observer(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_prepare_smart_approval_observer"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_observe_smart_approval_verdict(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_observe_smart_approval_verdict(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_observe_smart_approval_verdict"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_match_user_deny_rule(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_match_user_deny_rule(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_match_user_deny_rule"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_user_deny_block_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_user_deny_block_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_user_deny_block_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_command_parser_limit_exceeded(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_command_parser_limit_exceeded(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_command_parser_limit_exceeded"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_shell_tokens_with_spans(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_shell_tokens_with_spans(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_shell_tokens_with_spans"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_quoted_grep_pattern_spans(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_quoted_grep_pattern_spans(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_quoted_grep_pattern_spans"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_grep_safe_detection_variant(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_grep_safe_detection_variant(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_grep_safe_detection_variant"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_interpreter_family(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_interpreter_family(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_interpreter_family"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_shell_segment_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_shell_segment_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_shell_segment_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_iter_top_level_shell_segments(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_iter_top_level_shell_segments(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_iter_top_level_shell_segments"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_split_option(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_split_option(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_split_option"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_interpreter_exec_flag(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_interpreter_exec_flag(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_interpreter_exec_flag"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_bash_exec_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_bash_exec_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_bash_exec_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_read_tool_exec_flag(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_read_tool_exec_flag(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_read_tool_exec_flag"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_execution_flag_findings(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_execution_flag_findings(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_execution_flag_findings"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_is_verification_artifact_cleanup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_is_verification_artifact_cleanup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_is_verification_artifact_cleanup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_u_run_approval_gate(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_u_run_approval_gate(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_u_run_approval_gate"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_appr_request_tool_approval(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)appr_request_tool_approval(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("appr_request_tool_approval"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "appr_u_prepare_smart_approval_observer") == 0) o = emit_appr_u_prepare_smart_approval_observer(c);
        if (strcmp(op, "appr_u_observe_smart_approval_verdict") == 0) o = emit_appr_u_observe_smart_approval_verdict(c);
        if (strcmp(op, "appr_u_match_user_deny_rule") == 0) o = emit_appr_u_match_user_deny_rule(c);
        if (strcmp(op, "appr_u_user_deny_block_result") == 0) o = emit_appr_u_user_deny_block_result(c);
        if (strcmp(op, "appr_u_command_parser_limit_exceeded") == 0) o = emit_appr_u_command_parser_limit_exceeded(c);
        if (strcmp(op, "appr_u_shell_tokens_with_spans") == 0) o = emit_appr_u_shell_tokens_with_spans(c);
        if (strcmp(op, "appr_u_quoted_grep_pattern_spans") == 0) o = emit_appr_u_quoted_grep_pattern_spans(c);
        if (strcmp(op, "appr_u_grep_safe_detection_variant") == 0) o = emit_appr_u_grep_safe_detection_variant(c);
        if (strcmp(op, "appr_u_interpreter_family") == 0) o = emit_appr_u_interpreter_family(c);
        if (strcmp(op, "appr_u_shell_segment_tokens") == 0) o = emit_appr_u_shell_segment_tokens(c);
        if (strcmp(op, "appr_u_iter_top_level_shell_segments") == 0) o = emit_appr_u_iter_top_level_shell_segments(c);
        if (strcmp(op, "appr_u_split_option") == 0) o = emit_appr_u_split_option(c);
        if (strcmp(op, "appr_u_interpreter_exec_flag") == 0) o = emit_appr_u_interpreter_exec_flag(c);
        if (strcmp(op, "appr_u_bash_exec_payload") == 0) o = emit_appr_u_bash_exec_payload(c);
        if (strcmp(op, "appr_u_read_tool_exec_flag") == 0) o = emit_appr_u_read_tool_exec_flag(c);
        if (strcmp(op, "appr_u_execution_flag_findings") == 0) o = emit_appr_u_execution_flag_findings(c);
        if (strcmp(op, "appr_u_is_verification_artifact_cleanup") == 0) o = emit_appr_u_is_verification_artifact_cleanup(c);
        if (strcmp(op, "appr_u_run_approval_gate") == 0) o = emit_appr_u_run_approval_gate(c);
        if (strcmp(op, "appr_request_tool_approval") == 0) o = emit_appr_request_tool_approval(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
