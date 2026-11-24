/*
 * update_serve_resume.c — C port of upstream Hermes hermes_cli/update_serve_resume.py.
 *
 * PoP: w18_updateserv_d756b8_candidates @ hermes_cli/update_serve_resume.py:main
 *
 * The entry -> candidate argv assembly in main() is ported verbatim: purpose
 * allow-list, strict int port range, profile/host flags, and the None-coercing
 * hermes_home. sys.executable and the import hooks are injected seams.
 */
#include "noc_w18_updateserv_d756b8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_t *w18_updateserv_d756b8_candidates(const char *root, const char *executable,
                                         const json_t *entries) {
    json_t *out = json_array();
    if (!json_is_array(entries)) return out;
    const char *r = root ? root : "";

    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *e = json_get(entries, i);
        if (!json_is_object(e)) continue;

        const char *purpose = json_get_str(e, "purpose", "");
        json_t *portnode = json_obj_get(e, "port");
        int valid_purpose = purpose && (!strcmp(purpose, "serve") || !strcmp(purpose, "dashboard"));
        int valid_port = 0;
        double port = 0;
        if (portnode && portnode->type == JSON_NUMBER && json_number_value(portnode) ==
            (double)(long long)json_number_value(portnode)) {
            port = json_number_value(portnode);
            valid_port = (port > 0 && port <= 65535);
        }
        if (!valid_purpose || !valid_port) continue;

        json_t *command = json_array();
        json_append(command, json_string(executable ? executable : ""));
        char launch[8192];
        snprintf(launch, sizeof(launch), "%s\\hermes", r);
        json_append(command, json_string(launch));

        const char *profile = json_get_str(e, "profile", NULL);
        if (profile && profile[0] && strcmp(profile, "default")) {
            json_append(command, json_string("--profile"));
            json_append(command, json_string(profile));
        }
        json_append(command, json_string(purpose));

        const char *host = json_get_str(e, "host", NULL);
        if (host && host[0]) {
            json_append(command, json_string("--host"));
            json_append(command, json_string(host));
        }
        json_append(command, json_string("--port"));
        char pbuf[64];
        snprintf(pbuf, sizeof(pbuf), "%.0f", port);
        json_append(command, json_string(pbuf));

        json_t *rec = json_object();
        json_t *pid = json_obj_get(e, "pid");
        json_set(rec, "pid", pid ? json_copy(pid) : json_number(0));
        json_set(rec, "command", command);
        json_t *hh = json_obj_get(e, "hermes_home");
        if (hh && hh->type == JSON_STRING && hh->str_val[0])
            json_set(rec, "hermes_home", json_copy(hh));
        else
            json_set(rec, "hermes_home", json_null());
        json_append(out, rec);
    }
    return out;
}
