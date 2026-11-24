/*
 * windows_appinstaller_update.c -- real C11 port of the update-probe decision
 * table and payloads of upstream Hermes
 *   hermes_cli/windows_appinstaller_update.py.
 *
 * PoP: w20_windowsapp_53b288_run @ hermes_cli/windows_appinstaller_update.py:main
 * PoP: w20_windowsapp_53b288_availability_name @ hermes_cli/windows_appinstaller_update.py:main
 *
 * The WinRT projection (Package.current, get_app_installer_info(),
 * check_update_availability_async()) is a seam: its outcomes -- including the
 * HRESULT of a failed identity read and the pre-rendered str(exc) -- arrive in
 * the spec. Every branch below is the upstream branch, message for message.
 */
#include "hermes_json.h"
#include "noc_w20_windowsapp_53b288.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* HRESULT 0x80073D54 — "The process has no package identity." */
#define WIN20_NO_PACKAGE_IDENTITY_HRESULT (-2147009196.0)

static const char *AVAILABILITY_NAMES[] = {
    "UNKNOWN", "NO_UPDATES", "AVAILABLE", "REQUIRED", "ERROR"
};

static int win20_is_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    return isfinite(d) && floor(d) == d && d > -9.007199254740992e15 &&
           d < 9.007199254740992e15;
}

const char *w20_windowsapp_53b288_availability_name(double value) {
    if (!isfinite(value) || floor(value) != value) return NULL;
    if (value < 0.0 || value > 4.0) return NULL;
    return AVAILABILITY_NAMES[(int)value];
}

static json_t *win20_payload(const char *error) {
    json_t *p = json_object();
    json_set(p, "available", json_null());
    json_set(p, "error", json_string(error ? error : ""));
    return p;
}

static json_t *win20_result(json_t *payload, int exit_code) {
    json_t *out = json_object();
    json_set(out, "exit", json_number((double)exit_code));
    json_set(out, "payload", payload);
    return out;
}

json_t *w20_windowsapp_53b288_run(const json_t *spec) {
    const char *import_error = json_get_str(spec, "import_error", NULL);
    json_t *identity = json_obj_get(spec, "identity");
    json_t *source = json_obj_get(spec, "source");
    const char *check_error = json_get_str(spec, "check_error", NULL);
    json_t *avail = json_obj_get(spec, "availability");
    char msg[1024];

    /* try: Package, PackageUpdateAvailability = _load_projection()
       except ImportError: unknown, not "no update" */
    if (import_error) {
        snprintf(msg, sizeof(msg), "winrt import failed: %s", import_error);
        return win20_result(win20_payload(msg), 1);
    }

    /* try: package = Package.current */
    if (identity && identity->type == JSON_OBJECT) {
        json_t *winerror = json_obj_get(identity, "winerror");
        const char *text = json_get_str(identity, "str", "");
        if (winerror && winerror->type == JSON_NUMBER &&
            winerror->num_val == WIN20_NO_PACKAGE_IDENTITY_HRESULT) {
            json_t *p = json_object();
            json_set(p, "available", json_bool(0));
            json_set(p, "reason", json_string("not-packaged"));
            return win20_result(p, 0);
        }
        snprintf(msg, sizeof(msg), "package identity failed: %s", text);
        return win20_result(win20_payload(msg), 1);
    }

    /* source = package.get_app_installer_info(); None -> no source registered */
    if (!source || source->type == JSON_NULL) {
        json_t *p = json_object();
        json_set(p, "available", json_null());
        json_set(p, "reason", json_string("no-app-installer-source"));
        json_set(p, "error", json_string(
            "No App Installer update source is registered. Install Hermes through its "
            ".appinstaller file to enable updates."));
        return win20_result(p, 1);
    }

    {
        const char *uri = json_get_str(source, "uri", "");

        /* result = package.check_update_availability_async().get()
           availability = PackageUpdateAvailability(result.availability) */
        if (check_error) {
            snprintf(msg, sizeof(msg), "check failed: %s", check_error);
            return win20_result(win20_payload(msg), 1);
        }
        if (!win20_is_int(avail) || avail->num_val < 0.0 || avail->num_val > 4.0) {
            char repr[64];
            double v = avail && avail->type == JSON_NUMBER ? avail->num_val : 0.0;
            if (win20_is_int(avail)) snprintf(repr, sizeof(repr), "%.0f", v);
            else snprintf(repr, sizeof(repr), "%g", v);
            snprintf(msg, sizeof(msg), "check failed: %s is not a valid PackageUpdateAvailability",
                     repr);
            return win20_result(win20_payload(msg), 1);
        }

        {
            const char *name = w20_windowsapp_53b288_availability_name(avail->num_val);
            if (!strcmp(name, "NO_UPDATES")) {
                json_t *p = json_object();
                json_set(p, "available", json_bool(0));
                json_set(p, "availability", json_string(name));
                json_set(p, "source_uri", json_string(uri));
                return win20_result(p, 0);
            }
            if (!strcmp(name, "AVAILABLE") || !strcmp(name, "REQUIRED")) {
                json_t *p = json_object();
                json_set(p, "available", json_bool(1));
                json_set(p, "availability", json_string(name));
                json_set(p, "source_uri", json_string(uri));
                return win20_result(p, 2);
            }
            snprintf(msg, sizeof(msg), "availability unknown: %s", name);
            return win20_result(win20_payload(msg), 1);
        }
    }
}
