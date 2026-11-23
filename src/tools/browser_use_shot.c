/*
 * browser_use_shot.c - C home for upstream Hermes tools/browser_use_shot.py.
 *
 * PoP: w9_browseruse_e47d68_harness_command @ tools/browser_use_shot.py:harness_command
 * PoP: w9_browseruse_e47d68_activation @ tools/browser_use_shot.py:_activation_check
 * PoP: w9_browseruse_e47d68_is_harness_helper @ tools/browser_use_shot.py:_is_harness_helper
 * PoP: w9_browseruse_e47d68_constants @ tools/browser_use_shot.py:<module constants>
 */
#include "noc_w9_browseruse_e47d68.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const HARNESS_MODULE = "browser_harness.helpers";
static const char *const ACTIVATE_NEVER = "never";
static const char *const ACTIVATE_ALWAYS = "always";
static const char *const ACTIVATE_IF_CLOUD = "if-cloud";

/* Python repr() of a str, for str.format(..., {x!r}). */
static char *py_repr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t j = 0;
    out[j++] = '\'';
    for (size_t i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '\'' || ch == '\\') { out[j++] = '\\'; out[j++] = (char)ch; }
        else if (ch == '\n') { out[j++] = '\\'; out[j++] = 'n'; }
        else if (ch == '\r') { out[j++] = '\\'; out[j++] = 'r'; }
        else if (ch == '\t') { out[j++] = '\\'; out[j++] = 't'; }
        else if (ch < 32 || ch == 127) { sprintf(out + j, "\\x%02x", ch); j += 4; }
        else out[j++] = (char)ch;
    }
    out[j++] = '\'';
    out[j] = '\0';
    return out;
}

struct json_t *w9_browseruse_e47d68_harness_command(const struct json_t *cmd, const char *activate, const char *this_file) {
    if (!cmd || cmd->type != JSON_ARRAY || json_len(cmd) < 1)
        return cmd ? json_copy(cmd) : json_array();
    /* cmd[1:] == ["-m", "browser_harness.run"] ? */
    int match = json_len(cmd) == 3;
    if (match) {
        struct json_t *a1 = json_get(cmd, 1), *a2 = json_get(cmd, 2);
        match = a1 && a2 && a1->type == JSON_STRING && a2->type == JSON_STRING &&
                !strcmp(a1->str_val, "-m") && !strcmp(a2->str_val, "browser_harness.run");
    }
    if (!match) return json_copy(cmd);
    char *pr = py_repr(this_file ? this_file : "");
    char *pa = py_repr(activate ? activate : "");
    size_t cap = (pr ? strlen(pr) : 0) + (pa ? strlen(pa) : 0) + 640;
    char *launcher = (char *)malloc(cap);
    if (launcher) {
        snprintf(launcher, cap,
            "import importlib.util, runpy\n"
            "spec = importlib.util.spec_from_file_location('hermes_browser_use_shot', %s)\n"
            "shot = importlib.util.module_from_spec(spec); spec.loader.exec_module(shot)\n"
            "import browser_harness.helpers\n"
            "shot.install(browser_harness.helpers, activate=%s)\n"
            "runpy.run_module('browser_harness.run', run_name='__main__', alter_sys=True)\n",
            pr ? pr : "''", pa ? pa : "''");
    }
    struct json_t *out = json_array();
    json_append(out, json_copy(json_get(cmd, 0)));
    json_append(out, json_string("-c"));
    json_append(out, json_string(launcher ? launcher : ""));
    free(launcher); free(pr); free(pa);
    return out;
}

int w9_browseruse_e47d68_activation(const char *mode, int is_cloud) {
    if (mode && !strcmp(mode, ACTIVATE_IF_CLOUD)) return is_cloud ? 1 : 0;
    if (mode && !strcmp(mode, ACTIVATE_ALWAYS)) return 1;
    return 0;
}

int w9_browseruse_e47d68_is_harness_helper(const char *module_name, int is_callable) {
    return is_callable && module_name && !strcmp(module_name, HARNESS_MODULE);
}

struct json_t *w9_browseruse_e47d68_constants(void) {
    struct json_t *o = json_object();
    json_set(o, "png_deadline_s", json_number(10.0));
    json_set(o, "jpeg_deadline_s", json_number(60.0));
    json_set(o, "harness_module", json_string(HARNESS_MODULE));
    json_set(o, "activate_never", json_string(ACTIVATE_NEVER));
    json_set(o, "activate_always", json_string(ACTIVATE_ALWAYS));
    json_set(o, "activate_if_cloud", json_string(ACTIVATE_IF_CLOUD));
    struct json_t *a = json_array();
    json_append(a, json_string("-m"));
    json_append(a, json_string("browser_harness.run"));
    json_set(o, "harness_module_args", a);
    return o;
}
