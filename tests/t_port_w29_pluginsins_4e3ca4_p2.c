/* JSON-lines harness for the w29_pluginsins_4e3ca4_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_pluginsins_4e3ca4_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const char *func, json_t *in) {
    noc_finding_t f;
    const char *line = json_get_str(in, "line", "");
    const char *rel = json_get_str(in, "rel_path", "");
    const char *joined = json_get_str(in, "joined", "");
    int is_code = json_get_bool(in, "is_code", false) ? 1 : 0;
    int is_doc = json_get_bool(in, "is_doc", false) ? 1 : 0;
    f.pattern_id = json_get_str(in, "pid", "");
    f.severity = json_get_str(in, "sev", "");
    f.category = json_get_str(in, "cat", "");

    if (!strcmp(func, "is_inert_fixture_line"))
        return json_bool(noc_pis_p2_is_inert_fixture_line(&f, line, is_code, rel));
    if (!strcmp(func, "is_base64_media")) return json_bool(noc_pis_p2_is_base64_media(line));
    if (!strcmp(func, "is_base64_archive_roundtrip"))
        return json_bool(noc_pis_p2_is_base64_archive_roundtrip(&f, line));
    if (!strcmp(func, "is_regex_alternation_token"))
        return json_bool(noc_pis_p2_is_regex_alternation_token(&f, line));
    if (!strcmp(func, "is_sudo_inert")) return json_bool(noc_pis_p2_is_sudo_inert(&f, line, rel));
    if (!strcmp(func, "is_env_dump_in_safety_clause"))
        return json_bool(noc_pis_p2_is_env_dump_in_safety_clause(&f, line));
    if (!strcmp(func, "is_quoted_injection_phrase"))
        return json_bool(noc_pis_p2_is_quoted_injection_phrase(&f, line));
    if (!strcmp(func, "is_benign_lazy_os_import"))
        return json_bool(noc_pis_p2_is_benign_lazy_os_import(&f, line));
    if (!strcmp(func, "is_authorized_keys_prose"))
        return json_bool(noc_pis_p2_is_authorized_keys_prose(&f, line, is_doc));
    if (!strcmp(func, "is_loopback_only")) return json_bool(noc_pis_p2_is_loopback_only(&f, line));
    if (!strcmp(func, "is_loopback_continuation"))
        return json_bool(noc_pis_p2_is_loopback_continuation(&f, line, joined));
    if (!strcmp(func, "is_coin_name_only")) return json_bool(noc_pis_p2_is_coin_name_only(&f, line));
    if (!strcmp(func, "is_public_client_key"))
        return json_bool(noc_pis_p2_is_public_client_key(&f, line));
    if (!strcmp(func, "is_own_proc_read")) return json_bool(noc_pis_p2_is_own_proc_read(&f, line));
    return json_string("UNKNOWN_FUNC");
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!*func) { json_free(in); continue; }
        out = dispatch(func, in);
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
