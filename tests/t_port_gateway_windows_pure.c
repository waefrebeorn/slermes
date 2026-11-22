/* t_port_gateway_windows_pure.c — fixture harness for gateway_windows_pure.c.
 * Reads {"func":..., "args":{...}}, emits the same input fields plus "out"
 * so the oracle compares the LIVE Python function against this C port.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *gw_normalize_windows_path(const char *value);
json_t *gw_quote_cmd_script_arg(const char *value);
json_t *gw_quote_vbs_string(const char *value);
json_t *gw_sanitize_filename(const char *value);
json_t *gw_gateway_run_argv(const char *python_exe, const char *profile_arg);
json_t *gw_is_access_denied(const char *detail);
json_t *gw_should_fall_back(long code, const char *detail);

static const char *jstr(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static long jnum(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_NUMBER) ? (long)v->num_val : 0;
}
static void emit(json_t *v) {
    char *s = json_dumps(v, 0);
    fputs(s ? s : "null", stdout);
    free(s);
}

int main(void) {
    char line[16384];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = jstr(in, "func");
        json_t *args = json_obj_get(in, "args");
        if (!args) args = json_object();
        json_t *out = NULL;
        if (!strcmp(func, "_normalize_windows_path"))
            out = gw_normalize_windows_path(jstr(args, "value"));
        else if (!strcmp(func, "_quote_cmd_script_arg"))
            out = gw_quote_cmd_script_arg(jstr(args, "value"));
        else if (!strcmp(func, "_quote_vbs_string"))
            out = gw_quote_vbs_string(jstr(args, "value"));
        else if (!strcmp(func, "_sanitize_filename"))
            out = gw_sanitize_filename(jstr(args, "value"));
        else if (!strcmp(func, "_gateway_run_argv"))
            out = gw_gateway_run_argv(jstr(args, "python_exe"), jstr(args, "profile_arg"));
        else if (!strcmp(func, "_is_access_denied"))
            out = gw_is_access_denied(jstr(args, "detail"));
        else if (!strcmp(func, "_should_fall_back"))
            out = gw_should_fall_back((long)jnum(args, "code"), jstr(args, "detail"));
        if (!out) out = json_string("NO_SUCH_FUNC");
        fputs("{\"func\":", stdout); emit(json_string(func));
        fputs(",\"args\":", stdout); emit(args);
        fputs(",\"out\":", stdout); emit(out);
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
