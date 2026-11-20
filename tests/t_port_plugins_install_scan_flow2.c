/* t_port_plugins_install_scan_flow2.c — harness for the UNCLAIMED FLOW port
 * src/cli/plugins_install_scan_flow2.c of
 * hermes_cli/plugins_install_scan_context.py.
 *
 * Reads JSON LINES {"func":<name>,"in":{...}} from stdin; emits JSON LINES
 * {"func","in","out":{"ret","trace","state"}}. "ret" is the canonical return
 * (None -> null, bytes -> array of ints, uncaught exception -> {"__raise__":cls}).
 * The oracle drives the LIVE quarry defs under the same inputs and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "cli/plugins_install_scan_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *pis2_run(pis2_ctx *ctx, const char *func, json_t *in) {
    if (strcmp(func, "is_doc_prose") == 0)               return pis2_is_doc_prose(ctx, in);
    if (strcmp(func, "is_ci_workflow") == 0)             return pis2_is_ci_workflow(ctx, in);
    if (strcmp(func, "is_agent_facing") == 0)            return pis2_is_agent_facing(ctx, in);
    if (strcmp(func, "prose_cap") == 0)                  return pis2_prose_cap(ctx, in);
    if (strcmp(func, "_is_trusted_installer_url") == 0)  return pis2_is_trusted_installer_url(ctx, in);
    if (strcmp(func, "is_trusted_installer_pipe") == 0)  return pis2_is_trusted_installer_pipe(ctx, in);
    if (strcmp(func, "is_locale_catalog") == 0)          return pis2_is_locale_catalog(ctx, in);
    if (strcmp(func, "catalog_cap") == 0)                return pis2_catalog_cap(ctx, in);
    if (strcmp(func, "_uninstall_operand") == 0)         return pis2_uninstall_operand(ctx, in);
    if (strcmp(func, "is_self_uninstall_doc") == 0)      return pis2_is_self_uninstall_doc(ctx, in);
    if (strcmp(func, "_exec_first_arg_literal_spans")==0)return pis2_exec_first_arg_literal_spans(ctx, in);
    if (strcmp(func, "_executes") == 0)                  return pis2_executes_fn(ctx, in);
    if (strcmp(func, "is_inert_fixture_line") == 0)      return pis2_inert_fixture_line(ctx, in);
    if (strcmp(func, "is_test_tree") == 0)               return pis2_is_test_tree(ctx, in);
    if (strcmp(func, "_decoded_head") == 0)              return pis2_decoded_head(ctx, in);
    if (strcmp(func, "is_base64_media") == 0)            return pis2_is_base64_media(ctx, in);
    if (strcmp(func, "is_base64_archive_roundtrip") == 0)return pis2_is_base64_archive_roundtrip(ctx, in);
    if (strcmp(func, "_is_alternation_member") == 0)     return pis2_is_alternation_member(ctx, in);
    if (strcmp(func, "_is_whole_literal") == 0)          return pis2_is_whole_literal(ctx, in);
    if (strcmp(func, "is_regex_alternation_token") == 0) return pis2_is_regex_alternation_token(ctx, in);
    if (strcmp(func, "_sudo_in_safety_clause") == 0)     return pis2_sudo_in_safety_clause_fn(ctx, in);
    if (strcmp(func, "is_sudo_inert") == 0)              return pis2_is_sudo_inert(ctx, in);
    if (strcmp(func, "is_env_dump_in_safety_clause")==0) return pis2_is_env_dump_in_safety_clause(ctx, in);
    if (strcmp(func, "is_quoted_injection_phrase") == 0) return pis2_is_quoted_injection_phrase(ctx, in);
    if (strcmp(func, "is_repo_root_traversal") == 0)     return pis2_is_repo_root_traversal(ctx, in);
    if (strcmp(func, "is_benign_lazy_os_import") == 0)   return pis2_is_benign_lazy_os_import(ctx, in);
    if (strcmp(func, "is_disclosed_config_note") == 0)   return pis2_is_disclosed_config_note(ctx, in);
    if (strcmp(func, "is_authorized_keys_prose") == 0)   return pis2_is_authorized_keys_prose(ctx, in);
    if (strcmp(func, "is_loopback_only") == 0)           return pis2_is_loopback_only(ctx, in);
    if (strcmp(func, "is_pip_install_in_prose_literal")==0) return pis2_is_pip_install_in_prose_literal(ctx, in);
    if (strcmp(func, "is_hex_in_char_class") == 0)       return pis2_is_hex_in_char_class(ctx, in);
    if (strcmp(func, "logical_line") == 0)               return pis2_logical_line(ctx, in);
    if (strcmp(func, "is_loopback_continuation") == 0)   return pis2_is_loopback_continuation(ctx, in);
    if (strcmp(func, "is_json_prose_value") == 0)        return pis2_is_json_prose_value(ctx, in);
    if (strcmp(func, "_is_standalone_coin_name") == 0)   return pis2_is_standalone_coin_name(ctx, in);
    if (strcmp(func, "is_coin_name_only") == 0)          return pis2_is_coin_name_only(ctx, in);
    if (strcmp(func, "is_public_client_key") == 0)       return pis2_is_public_client_key(ctx, in);
    if (strcmp(func, "is_own_proc_read") == 0)           return pis2_is_own_proc_read(ctx, in);
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 17];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) {
            fprintf(stderr, "harness: unparseable case line\n");
            return 1;
        }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        pis2_ctx ctx;
        ctx.io = pis2_get(in, "io");
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *ret = pis2_run(&ctx, func, in);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(pis2_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
        } else {
            ret_out = ret ? ret : json_null();
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", json_array());
        json_set(out, "state", json_object());
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) {
            fputs(s, stdout);
            fputc('\n', stdout);
            free(s);
        }
        json_free(emit);
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}
