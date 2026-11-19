/*
 * session_policy_flow.c — local-client launch-policy FLOW cores of
 * gateway/session_policy.py. See include/session_policy_flow.h.
 *
 * Decision sequences only: every live I/O (forward-policy classification, the
 * adoption warning log) enters through seams. Refusals mirror AuthorityRefusal
 * (reason / field / detail); the live _one_line bounds are identity for the
 * clean fixture strings.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "session_policy_flow.h"

#include <ctype.h>

/* ── module constants (seeded from the live module) ── */
#define SESSION_PINS "session_pins"

/* SURFACES: creation label -> agent surface. */
static const char *surface_value(const char *label) {
    if (!label) return NULL;
    if (strcmp(label, "cli") == 0) return "cli";
    if (strcmp(label, "tui") == 0) return "tui";
    if (strcmp(label, "gui") == 0) return "desktop";
    if (strcmp(label, "acp") == 0) return "acp";
    if (strcmp(label, "tool") == 0) return "cli";
    if (strcmp(label, "oneshot") == 0) return "cli";
    return NULL;
}
/* STORED_AS_LOCAL = frozenset(SURFACES) - {'tool','oneshot'}. */
static bool stored_as_local(const char *label) {
    return label && (strcmp(label, "cli") == 0 || strcmp(label, "tui") == 0
                     || strcmp(label, "gui") == 0 || strcmp(label, "acp") == 0);
}

/* LocalSessionPolicy field names (dataclass order). */
static const char *POLICY_FIELDS[] = {
    "source", "platform", "cwd", "model", "toolsets", "config_json",
    "request_json", "terminal_json", "credential_ref", "config_secret_ref",
    "editor_mcp_json", "kanban_json", "safe_mode", "ignore_user_config",
    "skills_prompt", "coding_posture", "client_env_json", "client_env_remote",
    "client_env_rules_json", "edit_approval_policy",
};
#define N_POLICY_FIELDS (sizeof(POLICY_FIELDS) / sizeof(POLICY_FIELDS[0]))
/* Required fields (no dataclass default), sorted alphabetically. */
static const char *REQUIRED_FIELDS[] = {
    "config_json", "cwd", "model", "platform", "request_json", "source",
    "terminal_json", "toolsets",
};
#define N_REQUIRED (sizeof(REQUIRED_FIELDS) / sizeof(REQUIRED_FIELDS[0]))

static bool known_policy_field(const char *key) {
    for (size_t i = 0; i < N_POLICY_FIELDS; i++)
        if (strcmp(POLICY_FIELDS[i], key) == 0) return true;
    return false;
}

/* _LABEL_FOR_PLATFORM = {SURFACES[label]: label for label in STORED_AS_LOCAL}. */
static const char *label_for_platform(const char *platform) {
    if (!platform) return NULL;
    if (strcmp(platform, "cli") == 0) return "cli";
    if (strcmp(platform, "tui") == 0) return "tui";
    if (strcmp(platform, "desktop") == 0) return "gui";
    if (strcmp(platform, "acp") == 0) return "acp";
    return NULL;
}

/* ── internal helpers ── */
static void set_refusal(gsp_refusal_t *r, const char *reason, const char *field,
                        const char *detail) {
    if (!r) return;
    r->raised = true;
    snprintf(r->reason, sizeof(r->reason), "%s", reason ? reason : "");
    snprintf(r->field, sizeof(r->field), "%s", field ? field : "");
    snprintf(r->detail, sizeof(r->detail), "%s", detail ? detail : "");
}

static char *strip_dup(const char *s) {
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static void lower_inplace(char *s) {
    for (; s && *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* str(value or '') — falsy collapses to the empty string. malloc'd. */
static char *str_or_empty(const json_t *v) {
    if (!gflow_truth(v)) return strdup("");
    return gflow_pystr(v);
}

/* ── 1. session_pinned ── */
json_t *gsp_flow_session_pinned(const json_t *config, const char *field,
                                const json_t *launched) {
    const json_t *pins = gflow_obj_get(config, SESSION_PINS);
    if (gflow_is_obj(pins) && field) {
        const json_t *fv = json_obj_get(pins, field);
        if (gflow_is_bool(fv)) return json_copy(fv);
    }
    return launched ? json_copy(launched) : json_null();
}

/* ── 2. created_surface ── */
char *gsp_flow_created_surface(const json_t *label) {
    if (gflow_is_null(label)) return NULL;
    const char *s = gflow_str(label);
    if (!s) return NULL;   /* non-string labels are outside the fixture scope */
    if (s[0] == '\0' || strcmp(s, "local") == 0 || strcmp(s, "unknown") == 0)
        return NULL;
    if (stored_as_local(s)) return strdup(surface_value(s));
    return strdup(s);      /* tool / oneshot / any other label kept as itself */
}

/* ── 3. profile_target ── */
json_t *gsp_flow_profile_target(const json_t *config) {
    json_t *out = json_array();
    const json_t *model = gflow_obj_get(config, "model");
    if (gflow_is_obj(model)) {
        char *prov_raw = str_or_empty(json_obj_get(model, "provider"));
        char *prov = strip_dup(prov_raw);
        free(prov_raw);
        char *prov_lower = strdup(prov);
        lower_inplace(prov_lower);
        char *def_raw = str_or_empty(json_obj_get(model, "default"));
        char *def = strip_dup(def_raw);
        free(def_raw);
        json_append(out, json_string(def));
        json_append(out, json_string(strcmp(prov_lower, "auto") == 0 ? "" : prov));
        free(prov); free(prov_lower); free(def);
    } else {
        char *m = gflow_is_str(model) ? strip_dup(model->str_val) : strdup("");
        json_append(out, json_string(m ? m : ""));
        json_append(out, json_string(""));
        free(m);
    }
    return out;
}

/* ── 4. present_sections ── */
json_t *gsp_flow_present_sections(const json_t *config) {
    json_t *out = json_object();
    if (gflow_is_obj(config)) {
        for (size_t i = 0; i < config->c.count; i++) {
            const json_t *v = config->c.items[i];
            if (!gflow_is_null(v))
                json_set(out, config->c.keys[i], json_copy(v));
        }
    }
    const json_t *m = json_obj_get(out, "model");
    if (gflow_is_str(m)) {
        char *stripped = strip_dup(m->str_val);
        if (stripped && stripped[0]) {
            json_t *mo = json_object();
            json_set(mo, "default", json_string(m->str_val));
            json_set(out, "model", mo);
        } else {
            json_set(out, "model", json_object());
        }
        free(stripped);
    }
    return out;
}

/* ── 5. terminal_uses_host_cwd ── */
bool gsp_flow_terminal_uses_host_cwd(const json_t *terminal) {
    const json_t *env = gflow_obj_get(terminal, "TERMINAL_ENV");
    char *braw = gflow_truth(env) ? gflow_pystr(env) : strdup("local");
    char *backend = strip_dup(braw);
    free(braw);
    lower_inplace(backend);
    char *mraw = str_or_empty(gflow_obj_get(terminal, "TERMINAL_DOCKER_MOUNT_CWD_TO_WORKSPACE"));
    char *mnt = strip_dup(mraw);
    free(mraw);
    lower_inplace(mnt);
    bool mounts = strcmp(mnt, "1") == 0 || strcmp(mnt, "true") == 0
                  || strcmp(mnt, "yes") == 0 || strcmp(mnt, "on") == 0;
    bool r = (strcmp(backend, "local") == 0)
             || (strcmp(backend, "docker") == 0 && mounts);
    free(backend); free(mnt);
    return r;
}

/* ── 6. launch_skills ── */
json_t *gsp_flow_launch_skills(const json_t *params, gsp_refusal_t *refusal) {
    if (refusal) refusal->raised = false;
    const json_t *skills = gflow_obj_get(params, "skills");
    bool has = json_has(params, "skills");
    if (has) {
        bool bad = false;
        if (!gflow_is_arr(skills)) {
            bad = true;
        } else if (skills->c.count == 0) {
            bad = true;
        } else {
            for (size_t i = 0; i < skills->c.count; i++) {
                const json_t *s = skills->c.items[i];
                if (!gflow_is_str(s)) { bad = true; break; }
                char *st = strip_dup(s->str_val);
                bool empty = !st || !st[0];
                free(st);
                if (empty) { bad = true; break; }
            }
        }
        if (bad) {
            set_refusal(refusal, "invalid_params", "skills",
                        "must be a non-empty list of skill names");
            return NULL;
        }
    }
    if (gflow_truth(skills)) {
        const char *bypass[] = { "safe_mode", "ignore_user_config" };
        for (size_t i = 0; i < 2; i++) {
            const json_t *bv = gflow_obj_get(params, bypass[i]);
            if (gflow_is_bool(bv) && bv->bool_val) {
                set_refusal(refusal, "invalid_params", "skills",
                            "a bypass launch never reads the profile, so skills cannot be preloaded");
                return NULL;
            }
        }
    }
    json_t *out = json_array();
    if (gflow_is_arr(skills)) {
        for (size_t i = 0; i < skills->c.count; i++) {
            char *st = strip_dup(skills->c.items[i]->str_val);
            json_append(out, json_string(st ? st : ""));
            free(st);
        }
    }
    return out;
}

/* ── 7. _valid_client_env ── */
bool gsp_flow_valid_client_env(const json_t *value, bool kind_is_list) {
    if (gflow_is_null(value)) return true;
    if (!gflow_is_str(value)) return false;   /* env = None -> not a dict */
    char *err = NULL;
    json_t *env = json_parse(value->str_val, &err);
    if (err || !env) { free(err); json_free(env); return false; }
    bool ok = gflow_is_obj(env);
    if (ok) {
        for (size_t i = 0; i < env->c.count && ok; i++) {
            const json_t *v = env->c.items[i];
            if (kind_is_list) {
                if (!gflow_is_arr(v)) { ok = false; break; }
                for (size_t j = 0; j < v->c.count; j++)
                    if (!gflow_is_str(v->c.items[j])) { ok = false; break; }
            } else {
                if (!gflow_is_str(v)) ok = false;
            }
        }
    }
    json_free(env);
    free(err);
    return ok;
}

/* ── 8. _stored_cwd_is_absolute ── */
static bool posix_is_absolute(const char *cwd) {
    return cwd && cwd[0] == '/';
}
static bool win_is_absolute(const char *cwd) {
    if (!cwd || !cwd[0]) return false;
    /* UNC: two leading separators then a server name. */
    if ((cwd[0] == '\\' || cwd[0] == '/') && (cwd[1] == '\\' || cwd[1] == '/')
            && cwd[2] && cwd[2] != '\\' && cwd[2] != '/')
        return true;
    /* Drive letter + ':' + a root separator. Drive-relative (C:foo) and
     * drive-less rooted (\Users\x) both refuse. */
    if (isalpha((unsigned char)cwd[0]) && cwd[1] == ':'
            && (cwd[2] == '\\' || cwd[2] == '/'))
        return true;
    return false;
}
bool gsp_flow_stored_cwd_is_absolute(const char *cwd) {
    return posix_is_absolute(cwd) || win_is_absolute(cwd);
}

/* ── 9. restore_policy ── */

/* Apply a dataclass default for an optional field absent from the receipt. */
static void apply_default(json_t *policy, const char *key) {
    if (json_has(policy, key)) return;
    if (strcmp(key, "safe_mode") == 0 || strcmp(key, "ignore_user_config") == 0
            || strcmp(key, "coding_posture") == 0 || strcmp(key, "client_env_remote") == 0)
        json_set(policy, key, json_bool(false));
    else if (strcmp(key, "edit_approval_policy") == 0)
        json_set(policy, key, json_string("ask"));
    else
        json_set(policy, key, json_null());
}

/* json.loads on a string node; returns the parsed node (caller frees) and sets
 * *ok (false on TypeError/ValueError) and *is_dict. A non-string value is a
 * TypeError (ok=false). */
static json_t *loads_node(const json_t *v, bool *ok, bool *is_dict) {
    *ok = false; *is_dict = false;
    if (!gflow_is_str(v)) return NULL;          /* TypeError */
    char *err = NULL;
    json_t *parsed = json_parse(v->str_val, &err);
    if (err || !parsed) { free(err); json_free(parsed); return NULL; }  /* ValueError */
    free(err);
    *ok = true;
    *is_dict = gflow_is_obj(parsed);
    return parsed;
}

json_t *gsp_flow_restore_policy(const json_t *data, const gsp_restore_seams_t *s,
                                gsp_refusal_t *refusal) {
    if (refusal) refusal->raised = false;

    /* _stored_policy: not-an-object refusal. */
    if (!gflow_is_obj(data)) {
        set_refusal(refusal, "storage_unavailable", "policy",
                    "the stored policy is not an object");
        return NULL;
    }

    /* _adopt_stored_policy: drop keys this build does not define; warn once. */
    /* Collect unknown keys, sorted (strcmp order matches Python sorted()). */
    const char **unknown = malloc((data->c.count + 1) * sizeof(const char *));
    size_t n_unknown = 0;
    for (size_t i = 0; i < data->c.count; i++) {
        const char *k = data->c.keys[i];
        if (!known_policy_field(k)) {
            size_t j = n_unknown;
            while (j > 0 && strcmp(unknown[j - 1], k) > 0) {
                unknown[j] = unknown[j - 1];
                j--;
            }
            unknown[j] = k;
            n_unknown++;
        }
    }
    if (n_unknown > 0 && s && s->log) {
        /* ", ".join(unknown) */
        size_t cap = 64;
        for (size_t i = 0; i < n_unknown; i++) cap += strlen(unknown[i]) + 2;
        char *joined = malloc(cap);
        joined[0] = '\0';
        for (size_t i = 0; i < n_unknown; i++) {
            if (i) strcat(joined, ", ");
            strcat(joined, unknown[i]);
        }
        json_t *args = json_array();
        json_append(args, json_string(joined));
        s->log(s->ctx, 30,
               "restore_policy: adopting a stored receipt whose keys this build dropped: %s",
               args);
        json_free(args);
        free(joined);
    }
    free(unknown);

    /* Build the policy object: known fields from the receipt + defaults. */
    json_t *policy = json_object();
    for (size_t i = 0; i < data->c.count; i++) {
        const char *k = data->c.keys[i];
        if (known_policy_field(k))
            json_set(policy, k, json_copy(data->c.items[i]));
    }
    /* missing required field (alphabetically first) refuses by name. */
    for (size_t i = 0; i < N_REQUIRED; i++) {
        if (!json_has(policy, REQUIRED_FIELDS[i])) {
            set_refusal(refusal, "storage_unavailable", REQUIRED_FIELDS[i],
                        "the stored policy has no value for this required field");
            json_free(policy);
            return NULL;
        }
    }
    for (size_t i = 0; i < N_POLICY_FIELDS; i++)
        apply_default(policy, POLICY_FIELDS[i]);

    /* _check_stored_surface. */
    const json_t *source_v = json_obj_get(policy, "source");
    const json_t *platform_v = json_obj_get(policy, "platform");
    const char *source = gflow_str(source_v);
    const char *platform = gflow_str(platform_v);
    if (!source || !platform) {
        set_refusal(refusal, "storage_unavailable", "source",
                    "the stored source/platform is not a pair of names");
        json_free(policy);
        return NULL;
    }
    bool source_ok = false;
    const char *sv = surface_value(source);
    if (sv && strcmp(platform, sv) == 0) source_ok = true;
    if (strcmp(source, "cron") == 0 && strcmp(platform, "cron") == 0) source_ok = true;
    if (strcmp(source, "kanban") == 0 && strcmp(platform, "cli") == 0) source_ok = true;
    if (strcmp(source, "bot_room") == 0 && strcmp(platform, "bot_room") == 0) source_ok = true;
    if (strcmp(source, "local") == 0
            && (strcmp(platform, "cli") == 0 || strcmp(platform, "tui") == 0
                || strcmp(platform, "desktop") == 0 || strcmp(platform, "acp") == 0))
        source_ok = true;
    bool forward = s && s->is_forward_policy ? s->is_forward_policy(s->ctx, policy) : false;
    if (!forward && !source_ok) {
        char detail[192];
        snprintf(detail, sizeof(detail),
                 "stored source/platform %s/%s is not a known surface pair", source, platform);
        set_refusal(refusal, "storage_unavailable", "source", detail);
        json_free(policy);
        return NULL;
    }

    /* cwd absolute in either OS flavour. */
    const json_t *cwd_v = json_obj_get(policy, "cwd");
    const char *cwd = gflow_str(cwd_v);
    if (!cwd || !gsp_flow_stored_cwd_is_absolute(cwd)) {
        set_refusal(refusal, "storage_unavailable", "cwd",
                    "the stored working directory is missing or not absolute");
        json_free(policy);
        return NULL;
    }

    /* model: None/'' accepted; otherwise a non-empty stripped string. */
    const json_t *model_v = json_obj_get(policy, "model");
    bool model_empty = gflow_is_null(model_v)
                       || (gflow_is_str(model_v) && model_v->str_val[0] == '\0');
    if (!model_empty) {
        bool bad = !gflow_is_str(model_v);
        if (!bad) {
            char *st = strip_dup(model_v->str_val);
            bad = !st || !st[0];
            free(st);
        }
        if (bad) {
            set_refusal(refusal, "storage_unavailable", "model",
                        "the stored model is not a model name");
            json_free(policy);
            return NULL;
        }
    }

    /* toolsets: a list/tuple of names. */
    const json_t *toolsets_v = json_obj_get(policy, "toolsets");
    bool ts_bad = !gflow_is_arr(toolsets_v);
    if (!ts_bad) {
        for (size_t i = 0; i < toolsets_v->c.count; i++)
            if (!gflow_is_str(toolsets_v->c.items[i])) { ts_bad = true; break; }
    }
    if (ts_bad) {
        set_refusal(refusal, "storage_unavailable", "toolsets",
                    "the stored toolsets are not a list of names");
        json_free(policy);
        return NULL;
    }

    /* safe_mode / ignore_user_config must be exact bools. */
    const json_t *safe_v = json_obj_get(policy, "safe_mode");
    const json_t *ign_v = json_obj_get(policy, "ignore_user_config");
    if (!gflow_is_bool(safe_v)) {
        set_refusal(refusal, "storage_unavailable", "safe_mode",
                    "the stored flag is not a boolean");
        json_free(policy);
        return NULL;
    }
    if (!gflow_is_bool(ign_v)) {
        set_refusal(refusal, "storage_unavailable", "ignore_user_config",
                    "the stored flag is not a boolean");
        json_free(policy);
        return NULL;
    }

    /* skills_prompt: str or None. */
    const json_t *skills_v = json_obj_get(policy, "skills_prompt");
    if (!gflow_is_null(skills_v) && !gflow_is_str(skills_v)) {
        set_refusal(refusal, "storage_unavailable", "skills_prompt",
                    "the stored skills prompt is not a string");
        json_free(policy);
        return NULL;
    }

    /* coding_posture: exact bool. */
    if (!gflow_is_bool(json_obj_get(policy, "coding_posture"))) {
        set_refusal(refusal, "storage_unavailable", "coding_posture",
                    "the stored flag is not a boolean");
        json_free(policy);
        return NULL;
    }

    /* client_env_json: a valid client map (kind=str). */
    if (!gsp_flow_valid_client_env(json_obj_get(policy, "client_env_json"), false)) {
        set_refusal(refusal, "storage_unavailable", "client_env_json",
                    "the stored client environment map is malformed");
        json_free(policy);
        return NULL;
    }

    /* client_env_remote: exact bool. */
    if (!gflow_is_bool(json_obj_get(policy, "client_env_remote"))) {
        set_refusal(refusal, "storage_unavailable", "client_env_remote",
                    "the stored flag is not a boolean");
        json_free(policy);
        return NULL;
    }

    /* client_env_rules_json: valid rules (kind=list). */
    if (!gsp_flow_valid_client_env(json_obj_get(policy, "client_env_rules_json"), true)) {
        set_refusal(refusal, "storage_unavailable", "client_env_rules_json",
                    "the stored client environment rules are malformed");
        json_free(policy);
        return NULL;
    }

    /* edit_approval_policy: a known mode. */
    const json_t *eap_v = json_obj_get(policy, "edit_approval_policy");
    const char *eap = gflow_str(eap_v);
    bool eap_known = eap && (strcmp(eap, "ask") == 0 || strcmp(eap, "workspace_session") == 0
                             || strcmp(eap, "session") == 0);
    if (!eap_known) {
        set_refusal(refusal, "storage_unavailable", "edit_approval_policy",
                    "the stored edit approval policy is unknown");
        json_free(policy);
        return NULL;
    }
    if (strcmp(eap, "ask") != 0) {
        bool acp_pair = (strcmp(source, "acp") == 0 && strcmp(platform, "acp") == 0)
                        || (strcmp(source, "local") == 0 && strcmp(platform, "acp") == 0);
        if (!acp_pair) {
            set_refusal(refusal, "storage_unavailable", "edit_approval_policy",
                        "only an acp receipt may store a non-ask edit approval policy");
            json_free(policy);
            return NULL;
        }
    }

    /* safe_mode requires ignore_user_config. */
    if (safe_v->bool_val && !ign_v->bool_val) {
        set_refusal(refusal, "storage_unavailable", "safe_mode",
                    "safe mode requires ignore_user_config");
        json_free(policy);
        return NULL;
    }

    /* config_json / request_json / terminal_json: valid JSON objects. */
    const char *json_fields[] = { "config_json", "request_json", "terminal_json" };
    for (size_t i = 0; i < 3; i++) {
        bool ok = false, is_dict = false;
        json_t *parsed = loads_node(json_obj_get(policy, json_fields[i]), &ok, &is_dict);
        if (!ok) {
            set_refusal(refusal, "storage_unavailable", json_fields[i],
                        "the stored policy object is not valid JSON");
            json_free(parsed); json_free(policy);
            return NULL;
        }
        if (!is_dict) {
            set_refusal(refusal, "storage_unavailable", json_fields[i],
                        "the stored policy object is not a JSON object");
            json_free(parsed); json_free(policy);
            return NULL;
        }
        json_free(parsed);
    }

    /* terminal cwd must match the stored cwd on a host-cwd backend. */
    {
        bool ok = false, is_dict = false;
        json_t *terminal = loads_node(json_obj_get(policy, "terminal_json"), &ok, &is_dict);
        if (ok && gsp_flow_terminal_uses_host_cwd(terminal)) {
            const char *tcwd = gflow_str(gflow_obj_get(terminal, "TERMINAL_CWD"));
            if (!tcwd || strcmp(tcwd, cwd) != 0) {
                set_refusal(refusal, "storage_unavailable", "terminal_json",
                            "TERMINAL_CWD does not match the stored cwd");
                json_free(terminal); json_free(policy);
                return NULL;
            }
        }
        json_free(terminal);
    }

    /* Relabel a peer-refreshed 'local' receipt by its platform; a platform with
     * no creation label is a KeyError -> bare storage_unavailable. */
    if (strcmp(source, "local") == 0) {
        const char *relabel = label_for_platform(platform);
        if (!relabel) {
            set_refusal(refusal, "storage_unavailable", "", "");
            json_free(policy);
            return NULL;
        }
        json_set(policy, "source", json_string(relabel));
    }
    /* toolsets is already an array (Python tuple -> JSON array). */
    return policy;
}
