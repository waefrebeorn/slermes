/*
 * browser.c -- real C11 port of the `hermes browser` subcommand parser and its
 * dispatch closure of upstream Hermes hermes_cli/subcommands/browser.py.
 *
 * PoP: w21_browser_30d956_parser_shape @ hermes_cli/subcommands/browser.py:build_browser_parser
 * PoP: w21_browser_30d956_dispatch @ hermes_cli/subcommands/browser.py:_dispatch_browser
 *
 * Ported: the parser shape (two actions, their options, the suppressed
 * export-signins help) and every branch of the dispatch closure. Seams: the
 * three OS/process calls (default-browser detection, profile directory,
 * closing the process tree) arrive as inputs, `run_export_signins` arrives as
 * its return code, and argparse's own help text is reported as "<HELP>".
 */
#include "hermes_json.h"
#include "noc_w21_browser_30d956.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const BRS_UNSUPPORTED_CHANNEL = "__unsupported_channel__";

/* --- parser shape ---------------------------------------------------------- */

json_t *w21_browser_30d956_parser_shape(void) {
    json_t *out = json_object();
    json_t *subs = json_array();
    json_t *close_opts = json_array();
    json_t *export_opts = json_array();
    json_append(subs, json_string("close-profile"));
    json_append(subs, json_string("export-signins"));
    json_append(close_opts, json_string("--browser"));
    json_append(export_opts, json_string("--json"));
    json_set(out, "subcommands", subs);
    json_set(out, "close_options", close_opts);
    json_set(out, "export_options", export_opts);
    json_set(out, "export_hidden", json_bool(1));
    json_set(out, "has_dispatch", json_bool(1));
    return out;
}

/* --- dispatch -------------------------------------------------------------- */

static int brs_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `getattr(ns, name, None)`: NULL for an absent or explicitly null attribute. */
static const json_t *brs_attr(const json_t *req, const char *name) {
    const json_t *v;
    if (!req || req->type != JSON_OBJECT) return NULL;
    v = json_obj_get(req, name);
    if (v && v->type == JSON_NULL) return NULL;
    return v;
}

static const char *brs_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

json_t *w21_browser_30d956_dispatch(const json_t *req) {
    char errbuf[1024];
    const json_t *action_v = brs_attr(req, "browser_action");
    const json_t *action_alt = brs_attr(req, "action");
    const json_t *action = action_v ? action_v : action_alt;
    const char *action_s = brs_str(action);
    json_t *out = json_object();
    const json_t *browser_v, *detected, *profile, *src;
    const char *browser;
    int rc;

    /* action = getattr(_args, "browser_action", None) */
    if (action_s && !strcmp(action_s, "export-signins")) {
        json_t *erc = req ? json_obj_get(req, "export_rc") : NULL;
        rc = (erc && erc->type == JSON_NUMBER) ? (int)erc->num_val : 0;
        json_set(out, "rc", json_number((double)rc));
        json_set(out, "stdout", json_string(""));
        json_set(out, "stderr", json_string(""));
        return out;
    }
    if (!action_s || strcmp(action_s, "close-profile")) {
        json_set(out, "rc", json_number(2.0));
        json_set(out, "stdout", json_string("<HELP>"));
        json_set(out, "stderr", json_string(""));
        return out;
    }

    /* browser = getattr(_args, "browser", None) or detect_default_chromium() */
    browser_v = brs_attr(req, "browser");
    if (brs_truthy(browser_v)) {
        browser = brs_str(browser_v);
    } else {
        detected = brs_attr(req, "detected");
        browser = brs_str(detected);
    }
    if (!browser || !browser[0] || !strcmp(browser, BRS_UNSUPPORTED_CHANNEL)) {
        json_set(out, "rc", json_number(1.0));
        json_set(out, "stdout", json_string(""));
        json_set(out, "stderr",
                 json_string("\xe2\x9c\x97 No supported Chromium default browser detected.\n"));
        return out;
    }

    /* src = real_profile_data_dir(browser) */
    profile = req ? json_obj_get(req, "profile") : NULL;
    src = (profile && profile->type == JSON_OBJECT) ? json_obj_get(profile, browser) : NULL;
    if (!brs_truthy(src)) {
        snprintf(errbuf, sizeof(errbuf),
                 "\xe2\x9c\x97 Could not resolve the %s profile directory.\n", browser);
        json_set(out, "rc", json_number(1.0));
        json_set(out, "stdout", json_string(""));
        json_set(out, "stderr", json_string(errbuf));
        return out;
    }

    /* closed, msg = close_browser_holding_profile(src) */
    {
        json_t *closed = req ? json_obj_get(req, "closed") : NULL;
        json_t *msg = req ? json_obj_get(req, "msg") : NULL;
        const char *msg_s = brs_str(msg);
        if (!msg_s) msg_s = "";
        if (brs_truthy(closed)) {
            snprintf(errbuf, sizeof(errbuf), "\xe2\x9c\x93 %s\n", msg_s);
            json_set(out, "rc", json_number(0.0));
            json_set(out, "stdout", json_string(errbuf));
            json_set(out, "stderr", json_string(""));
            return out;
        }
        snprintf(errbuf, sizeof(errbuf), "\xe2\x9c\x97 %s\n", msg_s);
        json_set(out, "rc", json_number(1.0));
        json_set(out, "stdout", json_string(""));
        json_set(out, "stderr", json_string(errbuf));
        return out;
    }
}
