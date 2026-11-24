/*
 * gateway_setup_service.c -- real C11 port of the gateway service setup
 * orchestration of upstream Hermes hermes_cli/gateway_setup_service.py.
 *
 * PoP: w20_gatewayset_984e2d_record_service_choice @ hermes_cli/gateway_setup_service.py:record_service_choice
 * PoP: w20_gatewayset_984e2d_wants_service_install @ hermes_cli/gateway_setup_service.py:wants_service_install
 * PoP: w20_gatewayset_984e2d_ensure_gateway_service @ hermes_cli/gateway_setup_service.py:ensure_gateway_service
 * PoP: w20_gatewayset_984e2d_wizard_install_service @ hermes_cli/gateway_setup_service.py:_wizard_install_service
 *
 * Every branch of the module is a decision over service-manager state, so the
 * service layer arrives as a scripted value (`env->gw`) and the observable
 * effects -- printed lines, service-manager calls, the saved config -- come out
 * as a record. Nothing here shells out, prompts a terminal or touches disk.
 */
#include "noc_w20_gatewayset_984e2d.h"

#include <stdio.h>
#include <string.h>

#define GSET_MAX_DEPTH 8

/* --- small helpers -------------------------------------------------------- */

static int gset_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }

static const char *gset_get(const json_t *obj, const char *key, const char *def) {
    if (!gset_is_obj(obj)) return def;
    return json_get_str(obj, key, def);
}

static int gset_get_bool(const json_t *obj, const char *key, int def) {
    const json_t *v;
    if (!gset_is_obj(obj)) return def;
    v = json_obj_get(obj, key);
    if (!v) return def;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_NULL:   return 0;
    default:          return 1;
    }
}

/* Python's type(value).__name__ for the JSON shapes this module can meet. */
static const char *gset_py_type(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return "int";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

static void gset_msg(json_t *ctx, const char *kind, const char *text) {
    json_t *arr = json_obj_get(ctx, "messages");
    json_t *m = json_object();
    json_set(m, "kind", json_string(kind));
    json_set(m, "text", json_string(text ? text : ""));
    json_append(arr, m);
}

static json_t *gset_call(json_t *ctx, const char *name) {
    json_t *arr = json_obj_get(ctx, "calls");
    json_t *c = json_object();
    json_set(c, "call", json_string(name));
    json_append(arr, c);
    return c;
}

static void gset_error(json_t *ctx, const char *type, const char *fmt_arg) {
    json_t *err = json_object();
    json_set(err, "type", json_string(type));
    json_set(err, "message", json_string(fmt_arg ? fmt_arg : ""));
    json_set(ctx, "ok", json_bool(0));
    json_set(ctx, "error", err);
}

static json_t *gset_context(void) {
    json_t *ctx = json_object();
    json_set(ctx, "ok", json_bool(1));
    json_set(ctx, "error", json_null());
    json_set(ctx, "messages", json_array());
    json_set(ctx, "calls", json_array());
    json_set(ctx, "recorded", json_null());
    json_set(ctx, "saved", json_null());
    json_set(ctx, "config", json_null());
    return ctx;
}

static json_t *gset_store_copy(const json_t *stored) {
    if (gset_is_obj(stored)) return json_copy(stored);
    return json_object();
}

/* current.setdefault("gateway", {})["service_install_choice"] = choice */
static int gset_record_into(json_t *store, const char *choice,
                            const char **err_type, const char *err_message[2]) {
    json_t *gateway = json_obj_get(store, "gateway");
    static char message[160];
    if (!gateway) {
        gateway = json_object();
        json_set(store, "gateway", gateway);
    } else if (gateway->type != JSON_OBJECT) {
        *err_type = "TypeError";
        snprintf(message, sizeof(message), "'%s' object does not support item assignment",
                 gset_py_type(gateway));
        err_message[0] = message;
        err_message[1] = NULL;
        return 0;
    }
    json_set(gateway, "service_install_choice", json_string(choice));
    return 1;
}

/* config.get("gateway", {}).get("service_install_choice") */
static int gset_choice_of(const json_t *cfg, const char **choice,
                          const char **err_type, const char *err_message[2]) {
    static char message[160];
    const json_t *gateway, *value;
    *choice = NULL;
    if (!gset_is_obj(cfg)) {
        *err_type = "AttributeError";
        snprintf(message, sizeof(message), "'%s' object has no attribute 'get'", gset_py_type(cfg));
        err_message[0] = message;
        err_message[1] = NULL;
        return 0;
    }
    gateway = json_obj_get(cfg, "gateway");
    if (!gateway) return 1;                       /* {} */
    if (!gset_is_obj(gateway)) {
        *err_type = "AttributeError";
        snprintf(message, sizeof(message), "'%s' object has no attribute 'get'",
                 gset_py_type(gateway));
        err_message[0] = message;
        err_message[1] = NULL;
        return 0;
    }
    value = json_obj_get(gateway, "service_install_choice");
    if (value && value->type == JSON_STRING) *choice = value->str_val;
    return 1;
}

/* Raise the scripted service-manager failure when it is scheduled for `op`. */
static int gset_raise_at(const w20_gatewayset_984e2d_env *e, const char *op,
                         const char **type, const char **message) {
    const char *at = gset_get(e->gw, "raise_at", NULL);
    if (!at || strcmp(at, op)) return 0;
    *type = gset_get(e->gw, "error_type", "RuntimeError");
    *message = gset_get(e->gw, "error_message", "boom");
    return 1;
}

/* The two `except` arms that print through the gateway module. */
static int gset_handle_service_error(json_t *ctx, const char *type, const char *message) {
    char buf[512];
    if (!strcmp(type, "UserSystemdUnavailableError")) {
        gset_msg(ctx, "warning", "Could not reach user systemd to start the gateway service:");
        {
            json_t *c = gset_call(ctx, "_print_indented");
            json_set(c, "text", json_string(message));
        }
        return 1;
    }
    if (!strcmp(type, "SystemScopeRequiresRootError")) {
        snprintf(buf, sizeof(buf), "Gateway service needs root for this scope: %s", message);
        gset_msg(ctx, "warning", buf);
        {
            json_t *c = gset_call(ctx, "_print_system_scope_remediation");
            json_set(c, "mode", json_string("start"));
        }
        return 1;
    }
    if (!strcmp(type, "SystemExit")) {
        gset_msg(ctx, "warning", "Gateway service install did not complete. Retry: hermes gateway install");
        return 1;
    }
    if (!strcmp(type, "CalledProcessError")) {
        snprintf(buf, sizeof(buf), "Install failed: %s", message);
        gset_msg(ctx, "error", buf);
        gset_msg(ctx, "info", "You can try manually: hermes gateway install");
        return 1;
    }
    snprintf(buf, sizeof(buf), "Gateway service setup failed: %s", message);
    gset_msg(ctx, "warning", buf);
    gset_msg(ctx, "info", "You can retry manually: hermes gateway install");
    return 1;
}

/* --- record_service_choice ------------------------------------------------ */

json_t *w20_gatewayset_984e2d_record_service_choice(const char *choice,
                                                    const w20_gatewayset_984e2d_env *e) {
    json_t *ctx = gset_context();
    json_t *store;
    const char *err_type = NULL;
    const char *err_message[2] = {NULL, NULL};

    if (e && e->config && e->config->type != JSON_NULL) {
        json_set(ctx, "config", json_copy(e->config));
    }

    if (!choice || (strcmp(choice, "install") && strcmp(choice, "decline"))) {
        gset_error(ctx, "ValueError", "service_install_choice must be install or decline");
        return ctx;
    }
    store = e ? gset_store_copy(e->stored) : json_object();
    if (!gset_record_into(store, choice, &err_type, err_message)) {
        gset_error(ctx, err_type, err_message[0]);
        json_free(store);
        return ctx;
    }
    json_set(ctx, "saved", store);
    /* Setup keeps a working config which it saves again after this step. */
    if (e && e->config && e->config->type != JSON_NULL) {
        json_t *work = json_copy(e->config);
        if (work->type != JSON_OBJECT) {
            char message[160];
            snprintf(message, sizeof(message), "'%s' object has no attribute 'setdefault'",
                     gset_py_type(work));
            json_free(work);
            gset_error(ctx, "AttributeError", message);
            return ctx;
        }
        if (gset_record_into(work, choice, &err_type, err_message)) {
            json_set(ctx, "config", work);
        } else {
            json_free(work);
            gset_error(ctx, err_type, err_message[0]);
        }
    }
    return ctx;
}

/* --- wants_service_install ------------------------------------------------ */

/* prompt_yes_no(SERVICE_INSTALL_QUESTION, False) */
static int gset_prompt(json_t *ctx, const char *question, int def, int answer) {
    json_t *c = gset_call(ctx, "prompt_yes_no");
    json_set(c, "question", json_string(question));
    json_set(c, "default", json_bool(def ? 1 : 0));
    json_set(c, "answer", json_bool(answer ? 1 : 0));
    return answer ? 1 : 0;
}

/* record_service_choice(choice, config) as called from inside this module. */
static int gset_record_nested(json_t *ctx, json_t *store, const char *choice,
                              const w20_gatewayset_984e2d_env *e) {
    const char *err_type = NULL;
    const char *err_message[2] = {NULL, NULL};
    json_t *c;
    if (!gset_record_into(store, choice, &err_type, err_message)) {
        gset_error(ctx, err_type, err_message[0]);
        return 0;
    }
    (void)c;
    json_set(ctx, "saved", json_copy(store));
    json_set(ctx, "recorded", json_string(choice));
    if (e && e->config && e->config->type != JSON_NULL) {
        json_t *work = gset_is_obj(json_obj_get(ctx, "config"))
                           ? json_copy(json_obj_get(ctx, "config"))
                           : json_copy(e->config);
        if (!gset_record_into(work, choice, &err_type, err_message)) {
            json_free(work);
            gset_error(ctx, err_type, err_message[0]);
            return 0;
        }
        json_set(ctx, "config", work);
    }
    return 1;
}

static int gset_wants(json_t *ctx, const w20_gatewayset_984e2d_env *e, json_t *store) {
    const json_t *cfg;
    const char *choice = NULL;
    const char *err_type = NULL;
    const char *err_message[2] = {NULL, NULL};

    json_set(ctx, "decision", json_bool(0));
    if (e->install) {
        json_set(ctx, "decision", json_bool(1));
        return 1;
    }
    if (!e->interactive || !e->isatty) return 1;

    cfg = (e->config && e->config->type != JSON_NULL) ? e->config : store;
    if (!gset_choice_of(cfg, &choice, &err_type, err_message)) {
        gset_error(ctx, err_type, err_message[0]);
        return 0;
    }
    if (!choice) {
        if (gset_prompt(ctx, W20_GATEWAYSET_984E2D_SERVICE_INSTALL_QUESTION, 0, e->prompt_answer)) {
            json_set(ctx, "decision", json_bool(1));
            return 1;
        }
        if (!gset_record_nested(ctx, store, "decline", e)) return 0;
        return 1;
    }
    json_set(ctx, "decision", json_bool(choice && !strcmp(choice, "install") ? 1 : 0));
    return 1;
}

json_t *w20_gatewayset_984e2d_wants_service_install(const w20_gatewayset_984e2d_env *e) {
    w20_gatewayset_984e2d_env local;
    json_t *ctx, *store;
    if (!e) return NULL;
    local = *e;
    ctx = gset_context();
    store = gset_store_copy(local.stored);
    if (local.config && local.config->type != JSON_NULL) json_set(ctx, "config", json_copy(local.config));
    if (gset_wants(ctx, &local, store)) {
        const json_t *rec = json_obj_get(ctx, "recorded");
        if (rec && rec->type == JSON_STRING) json_set(ctx, "saved", json_copy(store));
    }
    json_free(store);
    return ctx;
}

/* --- ensure_gateway_service ----------------------------------------------- */

json_t *w20_gatewayset_984e2d_ensure_gateway_service(const char *context,
                                                     const w20_gatewayset_984e2d_env *e) {
    json_t *ctx, *store, *c;
    const char *err_type;
    const char *err_message;
    int supports_systemd, macos, windows, installed, valid = 0;
    char buf[512];

    if (!e) return NULL;
    ctx = gset_context();
    store = gset_store_copy(e->stored);
    if (e->config && e->config->type != JSON_NULL) json_set(ctx, "config", json_copy(e->config));
    json_set(ctx, "result", json_bool(0));

    if (gset_get_bool(e->gw, "in_container", 0)) {
        gset_msg(ctx, "info", "Run hermes gateway run as the container main process; use a Docker restart policy for persistence.");
        goto done;
    }
    supports_systemd = gset_get_bool(e->gw, "supports_systemd", 0);
    macos = gset_get_bool(e->gw, "is_macos", 0);
    windows = gset_get_bool(e->gw, "is_windows", 0);
    if (!(supports_systemd || macos || windows)) {
        gset_msg(ctx, "info", "No supported service manager found. Run: hermes gateway run");
        goto done;
    }

    /* try: */
    if (gset_raise_at(e, "running", &err_type, &err_message)) goto raised;
    if (gset_get_bool(e->gw, "running", 0)) {
        json_set(ctx, "result", json_bool(1));
        goto done;
    }
    if (gset_raise_at(e, "needs_no_service", &err_type, &err_message)) goto raised;
    if (gset_get_bool(e->gw, "needs_no_service", 0)) {
        json_set(ctx, "result", json_bool(1));
        goto done;
    }
    if (gset_raise_at(e, "installed", &err_type, &err_message)) goto raised;
    installed = gset_get_bool(e->gw, "installed", 0);
    if (!installed) {
        int wants;
        if (context && !strcmp(context, "import")) {
            wants = 0;
        } else {
            /* wants_service_install(interactive=..., install=..., config=...) */
            json_t *probe = gset_context();
            if (!gset_wants(probe, e, store)) {
                json_t *err = json_obj_get(probe, "error");
                gset_handle_service_error(ctx, json_get_str(err, "type", "Exception"),
                                          json_get_str(err, "message", ""));
                json_free(probe);
                goto done;
            }
            wants = json_get_bool(probe, "decision", 0) ? 1 : 0;
            /* Replay the probe's recorded effects onto this context. */
            {
                size_t i;
                json_t *pcalls = json_obj_get(probe, "calls");
                for (i = 0; i < json_len(pcalls); i++) {
                    json_append(json_obj_get(ctx, "calls"), json_copy(json_get(pcalls, i)));
                }
                if (gset_is_obj(json_obj_get(probe, "saved"))) {
                    json_set(ctx, "saved", json_copy(json_obj_get(probe, "saved")));
                    json_set(ctx, "recorded", json_copy(json_obj_get(probe, "recorded")));
                    json_free(store);
                    store = json_copy(json_obj_get(probe, "saved"));
                }
                if (gset_is_obj(json_obj_get(probe, "config"))) {
                    json_set(ctx, "config", json_copy(json_obj_get(probe, "config")));
                }
            }
            json_free(probe);
        }
        if (!wants) {
            gset_msg(ctx, "info", "No service installed. Run: hermes gateway run");
            gset_msg(ctx, "info", "Without a service, scheduled jobs and messaging stop at logout/reboot; jobs cannot run while the host is off.");
            goto done;
        }
        if (gset_raise_at(e, "conflict", &err_type, &err_message)) goto raised;
        if (supports_systemd && gset_get_bool(e->gw, "conflicting_units", 0)) {
            gset_call(ctx, "print_systemd_scope_conflict_warning");
            goto done;
        }
        if (gset_raise_at(e, "install", &err_type, &err_message)) goto raised;
        if (supports_systemd) {
            c = gset_call(ctx, "systemd_install");
            json_set(c, "force", json_bool(0));
            json_set(c, "non_interactive", json_bool(1));
        } else if (macos) {
            c = gset_call(ctx, "launchd_install");
            json_set(c, "force", json_bool(0));
        } else {
            c = gset_call(ctx, "_gw_windows.install");
            json_set(c, "force", json_bool(0));
            json_set(c, "start_now", json_bool(1));
            json_set(c, "start_on_login", json_bool(1));
        }
        /* Installers can refuse without raising (e.g. temporary-home guard). */
        if (!gset_get_bool(e->gw, "installed_after_install", 1)) {
            gset_msg(ctx, "warning", "Gateway service install did not complete. Retry: hermes gateway install");
            goto done;
        }
        if (!gset_record_nested(ctx, store, "install", e)) goto done;
        valid = 1;
    }
    (void)valid;

    if (gset_raise_at(e, "start", &err_type, &err_message)) goto raised;
    if (supports_systemd) gset_call(ctx, "systemd_start");
    else if (macos) gset_call(ctx, "launchd_start");
    else gset_call(ctx, "_gw_windows.start");
    gset_msg(ctx, "success", "Gateway service running (cron jobs + messaging platforms).");
    json_set(ctx, "result", json_bool(1));
    goto done;

raised:
    gset_handle_service_error(ctx, err_type, err_message);
    json_set(ctx, "result", json_bool(0));

done:
    (void)buf;
    json_free(store);
    return ctx;
}

/* --- _wizard_install_service ---------------------------------------------- */

json_t *w20_gatewayset_984e2d_wizard_install_service(const char *backend,
                                                     const w20_gatewayset_984e2d_env *e) {
    json_t *ctx, *store, *c;
    const char *err_type, *err_message, *state, *reason;
    int start_now, wants;
    char buf[512];

    if (!e) return NULL;
    ctx = gset_context();
    store = gset_store_copy(e->stored);
    if (e->config && e->config->type != JSON_NULL) json_set(ctx, "config", json_copy(e->config));

    if (!e->isatty) goto done;                       /* not a terminal: no wizard */

    start_now = gset_prompt(ctx, "  Start the gateway now?", 1, e->start_now_answer);
    {
        /* wants_service_install(interactive=True): config=None, so the store. */
        w20_gatewayset_984e2d_env probe_env = *e;
        json_t *probe = gset_context();
        probe_env.config = NULL;
        probe_env.interactive = 1;
        if (!gset_wants(probe, &probe_env, store)) {
            json_t *err = json_obj_get(probe, "error");
            gset_error(ctx, json_get_str(err, "type", "Exception"),
                       json_get_str(err, "message", ""));
            json_free(probe);
            goto done;
        }
        {
            size_t i;
            json_t *pcalls = json_obj_get(probe, "calls");
            for (i = 0; i < json_len(pcalls); i++) {
                json_append(json_obj_get(ctx, "calls"), json_copy(json_get(pcalls, i)));
            }
            json_set(ctx, "recorded", json_copy(json_obj_get(probe, "recorded")));
            if (gset_is_obj(json_obj_get(probe, "saved"))) {
                json_set(ctx, "saved", json_copy(json_obj_get(probe, "saved")));
                json_free(store);
                store = json_copy(json_obj_get(probe, "saved"));
            }
        }
        wants = json_get_bool(probe, "decision", 0);
        json_free(probe);
    }

    if (!wants) {
        gset_msg(ctx, "info", "Without a service, messaging and scheduled jobs stop at logout/reboot; jobs cannot run while the host is off.");
        if (start_now) {
            state = gset_get(e->runtime, "state", "pending");
            reason = gset_get(e->runtime, "reason_code", NULL);
            c = gset_call(ctx, "ensure_gateway_runtime");
            json_set(c, "state", json_string(state));
            json_set(c, "reason_code", reason && reason[0] ? json_string(reason) : json_null());
            if (!strcmp(state, "ready")) {
                gset_msg(ctx, "success", "Gateway runtime ready; no service was installed.");
            } else {
                snprintf(buf, sizeof(buf),
                         "Gateway runtime: %s (%s). Check: hermes gateway status",
                         state, (reason && reason[0]) ? reason : "pending");
                gset_msg(ctx, "info", buf);
            }
        } else {
            gset_msg(ctx, "info", "Run later: hermes gateway run. Install later: hermes gateway install");
        }
        goto done;
    }

    /* try: */
    if (gset_raise_at(e, "install", &err_type, &err_message)) goto wizard_raised;
    if (backend && !strcmp(backend, "systemd")) {
        const char *scope = gset_get(e->gw, "setup_scope", "user");
        int did_install = gset_get_bool(e->gw, "setup_did_install", 1);
        c = gset_call(ctx, "install_linux_gateway_from_setup");
        json_set(c, "force", json_bool(0));
        json_set(c, "enable_on_startup", json_bool(1));
        json_set(c, "scope", json_string(scope));
        json_set(c, "did_install", json_bool(did_install ? 1 : 0));
        if (!did_install) {
            gset_msg(ctx, "warning", "Gateway service install did not complete. Retry: hermes gateway install");
            goto done;
        }
        if (!gset_get_bool(e->gw, "installed_after_install", 1)) {
            gset_msg(ctx, "warning", "Gateway service install did not complete. Retry: hermes gateway install");
            goto done;
        }
        {
            w20_gatewayset_984e2d_env no_cfg = *e;
            no_cfg.config = NULL;
            if (!gset_record_nested(ctx, store, "install", &no_cfg)) goto done;
        }
        if (start_now) {
            c = gset_call(ctx, "_setup_service_action");
            json_set(c, "action", json_string("start"));
            json_set(c, "failed_label", json_string("Start failed"));
            json_set(c, "system", json_bool(!strcmp(scope, "system") ? 1 : 0));
        }
        goto done;
    }
    if (backend && !strcmp(backend, "launchd")) {
        c = gset_call(ctx, "launchd_install");
        json_set(c, "force", json_bool(0));
    } else {
        c = gset_call(ctx, "_gw_windows.install");
        json_set(c, "force", json_bool(0));
        json_set(c, "start_now", json_bool(start_now ? 1 : 0));
        json_set(c, "start_on_login", json_bool(1));
    }
    if (!gset_get_bool(e->gw, "installed_after_install", 1)) {
        gset_msg(ctx, "warning", "Gateway service install did not complete. Retry: hermes gateway install");
        goto done;
    }
    {
        w20_gatewayset_984e2d_env no_cfg = *e;
        no_cfg.config = NULL;
        if (!gset_record_nested(ctx, store, "install", &no_cfg)) goto done;
    }
    if (start_now && !(backend && !strcmp(backend, "windows"))) {
        c = gset_call(ctx, "_setup_service_action");
        json_set(c, "action", json_string("start"));
        json_set(c, "failed_label", json_string("Start failed"));
        json_set(c, "system", json_bool(0));
    }
    goto done;

wizard_raised:
    if (!strcmp(err_type, "CalledProcessError") || !strcmp(err_type, "SystemExit")) {
        snprintf(buf, sizeof(buf), "Install failed: %s", err_message);
        gset_msg(ctx, "error", buf);
        gset_msg(ctx, "info", "You can try manually: hermes gateway install");
    } else {
        /* No generic `except` in _wizard_install_service: the error escapes. */
        gset_error(ctx, err_type, err_message);
    }

done:
    json_free(store);
    return ctx;
}
