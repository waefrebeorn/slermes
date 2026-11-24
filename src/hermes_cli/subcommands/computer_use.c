/*
 * computer_use.c - C11 port of upstream hermes_cli/subcommands/computer_use.py
 * (gate w26_computerus_4c7531).
 *
 * PoP: w26_computerus_4c7531_backend_gate @ hermes_cli/subcommands/computer_use.py:_exit_if_plugin_backend
 * PoP: w26_computerus_4c7531_perms_status @ hermes_cli/subcommands/computer_use.py:_cu_perms_status
 * PoP: w26_computerus_4c7531_surface @ hermes_cli/subcommands/computer_use.py:build_computer_use_parser
 *
 * The subcommand is mostly process seams -- the provider lookup, the driver
 * probe, TCC grants, stdout and sys.exit.  What is ported is the decision on
 * top of them: whether the cua-driver checks apply at all, every line the
 * non-JSON permissions report prints and the exit code it ends with, and the
 * argparse tree itself as a literal table.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_computerus_4c7531.h"

/* Python truthiness of a JSON value (None is falsy). */
static int cu_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(value) for the report's interpolations: strings verbatim, null -> "None". */
static const char *cu_str(const json_t *v, const char *nil) {
    if (!v || v->type == JSON_NULL) return nil;
    if (v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return nil;
}

static void cu_push(json_t *lines, const char *text) {
    json_append(lines, json_string(text ? text : ""));
}

static json_t *cu_report(int exited, int code, json_t *lines) {
    json_t *o = json_object();
    json_set(o, "exit", json_bool(exited != 0));
    json_set(o, "code", exited ? json_number((double)code) : json_null());
    json_set(o, "lines", lines);
    return o;
}

/* ------------------------------------------------------ _exit_if_plugin_backend */

json_t *w26_computerus_4c7531_backend_gate(const json_t *req) {
    const json_t *lookup_error = json_obj_get(req, "lookup_error");
    const char *configured = cu_str(json_obj_get(req, "configured_backend"), "");
    const char *default_backend = cu_str(json_obj_get(req, "default_backend"), "");
    json_t *lines = json_array();
    char buf[4096];
    int doctor = cu_truthy(json_obj_get(req, "doctor"));
    const json_t *provider_rc = json_obj_get(req, "provider_rc");
    int ok;

    /* configured_backend_name() == DEFAULT_BACKEND -> the built-in backend. */
    if (!strcmp(configured, default_backend)) return cu_report(0, 0, lines);

    if (lookup_error && lookup_error->type != JSON_NULL) {
        snprintf(buf, sizeof(buf), "Computer Use: %s",
                 cu_str(lookup_error, "None"));
        cu_push(lines, buf);
        return cu_report(1, 2, lines);
    }
    /* `if doctor and (rc := provider.doctor()) is not None: sys.exit(rc)` */
    if (doctor && provider_rc && provider_rc->type != JSON_NULL) {
        double rc = provider_rc->type == JSON_NUMBER ? provider_rc->num_val
                                                     : (cu_truthy(provider_rc) ? 1.0 : 0.0);
        return cu_report(1, (int)rc, lines);
    }
    ok = cu_truthy(json_obj_get(req, "available"));
    snprintf(buf, sizeof(buf),
             "Computer Use backend: %s (%s) \xe2\x80\x94 %s. cua-driver checks do not apply to this backend.",
             cu_str(json_obj_get(req, "name"), ""),
             cu_str(json_obj_get(req, "display_name"), ""),
             ok ? "available" : "not available");
    cu_push(lines, buf);
    return cu_report(1, ok ? 0 : 1, lines);
}

/* ----------------------------------------------------------- _cu_perms_status */

/* glyph = lambda v: "✅" if v is True else ("❌" if v is False else "•") */
static const char *cu_glyph(const json_t *v) {
    if (v && v->type == JSON_BOOL) return v->bool_val ? "\xe2\x9c\x85" : "\xe2\x9d\x8c";
    return "\xe2\x80\xa2";
}

json_t *w26_computerus_4c7531_perms_status(const json_t *req) {
    const json_t *st = json_obj_get(req, "st");
    json_t *lines = json_array();
    char buf[8192];
    const json_t *checks;
    size_t i;
    const char *platform;
    int ready;

    if (!st || st->type != JSON_OBJECT) st = NULL;
    platform = cu_str(st ? json_obj_get(st, "platform") : NULL, "");

    if (!cu_truthy(st ? json_obj_get(st, "platform_supported") : NULL)) {
        snprintf(buf, sizeof(buf), "Computer Use is not supported on %s.", platform);
        cu_push(lines, buf);
        return cu_report(1, 1, lines);
    }
    if (!cu_truthy(json_obj_get(st, "installed"))) {
        cu_push(lines, "cua-driver: not installed. Run: hermes computer-use install");
        return cu_report(1, 1, lines);
    }
    ready = cu_truthy(json_obj_get(st, "ready"));
    {
        const char *version = cu_str(json_obj_get(st, "version"), NULL);
        snprintf(buf, sizeof(buf), "cua-driver: %s (%s)",
                 (version && version[0]) ? version : "installed", platform);
        cu_push(lines, buf);
    }
    if (cu_truthy(json_obj_get(st, "can_grant"))) {
        snprintf(buf, sizeof(buf), "  %s Accessibility",
                 cu_glyph(json_obj_get(st, "accessibility")));
        cu_push(lines, buf);
        snprintf(buf, sizeof(buf), "  %s Screen Recording",
                 cu_glyph(json_obj_get(st, "screen_recording")));
        cu_push(lines, buf);
        if (!ready) {
            const char *hint;
            cu_push(lines, "  Grant: hermes computer-use permissions grant");
            hint = cu_str(json_obj_get(req, "hint"), NULL);
            if (hint && hint[0]) {
                snprintf(buf, sizeof(buf), "  %s", hint);
                cu_push(lines, buf);
            }
        }
    } else {
        snprintf(buf, sizeof(buf), "  %s driver health (no permission toggles on %s)",
                 cu_glyph(json_obj_get(st, "ready")), platform);
        cu_push(lines, buf);
    }
    checks = json_obj_get(st, "checks");
    if (checks && checks->type == JSON_ARRAY) {
        for (i = 0; i < json_len(checks); i++) {
            const json_t *c = json_get(checks, i);
            const char *status;
            if (!c || c->type != JSON_OBJECT) continue;
            status = cu_str(json_obj_get(c, "status"), "");
            if (!strcmp(status, "ok")) continue;
            snprintf(buf, sizeof(buf), "  \xe2\x9a\xa0 %s: %s",
                     cu_str(json_obj_get(c, "label"), ""),
                     cu_str(json_obj_get(c, "message"), ""));
            cu_push(lines, buf);
        }
    }
    {
        const char *error = cu_str(json_obj_get(st, "error"), NULL);
        if (error && error[0]) {
            snprintf(buf, sizeof(buf), "  \xe2\x9a\xa0 %s", error);
            cu_push(lines, buf);
        }
    }
    return cu_report(1, ready ? 0 : 1, lines);
}

/* --------------------------------------------------------------- parser table */

/* The ``hermes computer-use`` argparse tree, mirrored literally from
 * build_computer_use_parser: name / help / options(flags, dest, action, nargs,
 * default, choices, metavar) / subcommands.  Prose descriptions are omitted. */
static const char *const CU_SURFACE_JSON = "{\"name\":\"computer-use\",\"help\":\"Manage the Computer Use (cua-driver) backend (macOS/Windows/Linux)\",\"options\":[],\"subcommands\":[{\"name\":\"install\",\"help\":\"Install or repair the cua-driver binary (macOS/Windows/Linux)\",\"options\":[{\"flags\":[\"--upgrade\"],\"dest\":\"upgrade\",\"action\":\"_StoreTrueAction\",\"nargs\":0,\"default\":false,\"choices\":null,\"metavar\":null}],\"subcommands\":[]},{\"name\":\"status\",\"help\":\"Check the selected cua-driver and its runtime contract\",\"options\":[],\"subcommands\":[]},{\"name\":\"doctor\",\"help\":\"Run cua-driver `health_report` and surface the check matrix\",\"options\":[{\"flags\":[\"--include\"],\"dest\":\"include\",\"action\":\"_AppendAction\",\"nargs\":null,\"default\":[],\"choices\":null,\"metavar\":\"CHECK\"},{\"flags\":[\"--skip\"],\"dest\":\"skip\",\"action\":\"_AppendAction\",\"nargs\":null,\"default\":[],\"choices\":null,\"metavar\":\"CHECK\"},{\"flags\":[\"--json\"],\"dest\":\"json\",\"action\":\"_StoreTrueAction\",\"nargs\":0,\"default\":false,\"choices\":null,\"metavar\":null}],\"subcommands\":[]},{\"name\":\"permissions\",\"help\":\"Check or grant macOS Accessibility + Screen Recording (macOS)\",\"options\":[],\"subcommands\":[{\"name\":\"status\",\"help\":\"Report Accessibility + Screen Recording grant state (read-only)\",\"options\":[{\"flags\":[\"--json\"],\"dest\":\"json\",\"action\":\"_StoreTrueAction\",\"nargs\":0,\"default\":false,\"choices\":null,\"metavar\":null}],\"subcommands\":[]},{\"name\":\"grant\",\"help\":\"Request the grants (opens the dialog attributed to CuaDriver)\",\"options\":[],\"subcommands\":[]}]},{\"name\":\"screen\",\"help\":\"Bot Desktop: the headless screen this profile's computer_use drives (Linux)\",\"options\":[],\"subcommands\":[{\"name\":\"status\",\"help\":\"Show whether this profile's screen is installed/running and who holds control\",\"options\":[{\"flags\":[\"--json\"],\"dest\":\"json\",\"action\":\"_StoreTrueAction\",\"nargs\":0,\"default\":false,\"choices\":null,\"metavar\":null}],\"subcommands\":[]},{\"name\":\"start\",\"help\":\"Start this profile's screen\",\"options\":[],\"subcommands\":[]},{\"name\":\"stop\",\"help\":\"Stop this profile's screen (hands control back to the agent first)\",\"options\":[{\"flags\":[\"--force\"],\"dest\":\"force\",\"action\":\"_StoreTrueAction\",\"nargs\":0,\"default\":false,\"choices\":null,\"metavar\":null}],\"subcommands\":[]},{\"name\":\"install\",\"help\":\"Install TigerVNC + Xfce core via the host package manager\",\"options\":[{\"flags\":[\"-y\",\"--yes\"],\"dest\":\"yes\",\"action\":\"_StoreTrueAction\",\"nargs\":0,\"default\":false,\"choices\":null,\"metavar\":null}],\"subcommands\":[]}]}]}";

json_t *w26_computerus_4c7531_surface(void) {
    json_t *v = json_parse(CU_SURFACE_JSON, NULL);
    return v ? v : json_null();
}
