/*
 * agent_init_memory_provider.c — C home for upstream Hermes agent/agent_init_memory_provider.py.
 * PoP: w26_agentinitm_60b1c8_init_kwargs @ agent/agent_init_memory_provider.py:_memory_provider_init_kwargs
 * PoP: w26_agentinitm_60b1c8_stored_transcript @ agent/agent_init_memory_provider.py:_stored_transcript_kwargs
 * PoP: w26_agentinitm_60b1c8_unavailable_message @ agent/agent_init_memory_provider.py:_warn_memory_provider_unavailable
 * PoP: w26_agentinitm_60b1c8_constants @ agent/agent_init_memory_provider.py:<module constants>
 *
 * At agent init the configured memory provider is resolved and the kwargs its
 * ``initialize`` receives are built: which session/platform/home it is scoped
 * to, whether the transcript already has an assistant reply, the title and its
 * source, the gateway identity params and the profile name.  The agent object,
 * its session store and the profile lookup are seams -- their values arrive in
 * a state object.  What is ported here is exactly what those values decide,
 * including the advisory text a user is shown once per home when their
 * configured provider reports itself unavailable.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_agentinitm_60b1c8.h"

/* _GATEWAY_IDENTITY_PARAMS from agent/agent_init_params.py. */
static const char *const AGENTINITM_IDENT_PARAMS[W26_AGENTINITM_60B1C8_IDENT_PARAMS] = {
    "user_id", "user_id_alt", "user_name", "chat_id",
    "chat_name", "chat_type", "thread_id", "gateway_session_key",
};

static const char *const AGENTINITM_DEFAULT_PLATFORM = "cli";
static const char *const AGENTINITM_PRIMARY_CONTEXT = "primary";
static const char *const AGENTINITM_AGENT_WORKSPACE = "hermes";

static int am_truthy(const json_t *v) {
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

/* A string field, or NULL when absent / not a string (a missing attribute). */
static const char *am_str(const json_t *state, const char *key) {
    const json_t *v = state ? json_obj_get(state, key) : NULL;
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

/* _stored_transcript_kwargs' decision: only an assistant reply in a non-empty
 * session counts as resumed (a brand-new chat can count 1 and still be new). */
static void am_transcript(const json_t *state, int *resumed, long long *count) {
    *resumed = 0;
    *count = 0;
    if (!am_truthy(state ? json_obj_get(state, "have_db") : NULL)) return;
    if (am_truthy(state ? json_obj_get(state, "row") : NULL)) {
        const json_t *c = state ? json_obj_get(state, "count") : NULL;
        *count = (c && c->type == JSON_NUMBER) ? (long long)c->num_val : 0;
    }
    if (am_truthy(state ? json_obj_get(state, "has_assistant") : NULL)) {
        if (*count > 0) *resumed = 1;
    }
}

json_t *w26_agentinitm_60b1c8_stored_transcript(const json_t *state) {
    int resumed;
    long long count;
    json_t *out = json_object();
    am_transcript(state, &resumed, &count);
    json_set(out, "resumed", json_bool(resumed ? true : false));
    json_set(out, "message_count", json_number((double)count));
    return out;
}

/* Python repr() of a str for the advisory's {name!r}. */
static char *am_repr(const char *s) {
    size_t n = 0, i, len;
    int use_double;
    char quote;
    char *out, *p;
    if (!s) s = "";
    use_double = strchr(s, '\'') != NULL && strchr(s, '"') == NULL;
    quote = use_double ? '"' : '\'';
    len = strlen(s);
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        n += (c == '\\' || c == (unsigned char)quote || c == '\n' || c == '\t' || c == '\r') ? 2 : 1;
    }
    out = (char *)malloc(n + 3);
    if (!out) return NULL;
    p = out;
    *p++ = quote;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\') { *p++ = '\\'; *p++ = '\\'; }
        else if (c == (unsigned char)quote) { *p++ = '\\'; *p++ = (char)c; }
        else if (c == '\n') { *p++ = '\\'; *p++ = 'n'; }
        else if (c == '\t') { *p++ = '\\'; *p++ = 't'; }
        else if (c == '\r') { *p++ = '\\'; *p++ = 'r'; }
        else *p++ = (char)c;
    }
    *p++ = quote;
    *p = '\0';
    return out;
}

json_t *w26_agentinitm_60b1c8_unavailable_message(const json_t *state) {
    const char *name = am_str(state, "name");
    const char *reason = am_str(state, "reason");
    char *repr = am_repr(name);
    char *buf;
    size_t n;
    json_t *out;
    static const char *const HEAD =
        "\xE2\x9A\xA0 Memory provider ";
    static const char *const TAIL =
        " is selected but reports unavailable \xE2\x80\x94 external memory "
        "is disabled for this session (built-in memory still works). Check the "
        "provider's credentials/config with 'hermes memory status'. Note: "
        "systemd/gateway services do not inherit ~/.hermes/.env automatically; set "
        "any required variables in the service environment.";
    size_t extra = (reason && reason[0]) ? strlen(reason) + 1 : 0;
    if (!repr) return json_null();
    n = strlen(HEAD) + strlen(repr) + strlen(TAIL) + extra;
    buf = (char *)malloc(n + 1);
    if (!buf) { free(repr); return json_null(); }
    strcpy(buf, HEAD);
    strcat(buf, repr);
    strcat(buf, TAIL);
    if (extra) {
        strcat(buf, " ");
        strcat(buf, reason);
    }
    out = json_string(buf);
    free(buf);
    free(repr);
    return out;
}

json_t *w26_agentinitm_60b1c8_init_kwargs(const json_t *state) {
    json_t *kw = json_object();
    const char *session_id = am_str(state, "session_id");
    const char *raw_platform = am_str(state, "platform");
    const char *home = am_str(state, "hermes_home");
    const char *affected;
    int resumed;
    long long count;
    size_t i;

    /* platform or "cli" */
    if (!raw_platform || !raw_platform[0]) raw_platform = AGENTINITM_DEFAULT_PLATFORM;
    json_set(kw, "session_id", json_string(session_id ? session_id : ""));
    json_set(kw, "platform", json_string(raw_platform));
    json_set(kw, "hermes_home", json_string(home ? home : ""));

    /* agent_context: the named scope, else cron/subagent, else primary */
    affected = am_str(state, "memory_agent_context");
    if (!affected || !affected[0]) {
        if (!strcmp(raw_platform, "cron") || !strcmp(raw_platform, "subagent"))
            affected = raw_platform;
        else
            affected = AGENTINITM_PRIMARY_CONTEXT;
    }
    json_set(kw, "agent_context", json_string(affected));

    {
        const char *parent = am_str(state, "parent_session_id");
        if (parent && parent[0]) json_set(kw, "parent_session_id", json_string(parent));
    }
    if (!strcmp(raw_platform, "cli")) {
        json_t *warn = json_object(), *status = json_object();
        json_set(warn, "__callable__", json_string("warning_callback"));
        json_set(status, "__callable__", json_string("status_callback"));
        json_set(kw, "warning_callback", warn);
        json_set(kw, "status_callback", status);
    }
    am_transcript(state, &resumed, &count);
    json_set(kw, "resumed", json_bool(resumed ? true : false));
    json_set(kw, "message_count", json_number((double)count));

    if (am_truthy(state ? json_obj_get(state, "have_db") : NULL)) {
        const char *title = am_str(state, "session_title");
        const char *tsrc = am_str(state, "session_title_source");
        if (title && title[0]) json_set(kw, "session_title", json_string(title));
        if (tsrc && tsrc[0]) json_set(kw, "session_title_source", json_string(tsrc));
    }

    {
        const json_t *ids = state ? json_obj_get(state, "identities") : NULL;
        for (i = 0; i < W26_AGENTINITM_60B1C8_IDENT_PARAMS; i++) {
            const char *key = AGENTINITM_IDENT_PARAMS[i];
            const json_t *v = (ids && ids->type == JSON_OBJECT) ? json_obj_get(ids, key) : NULL;
            if (am_truthy(v)) json_set(kw, key, json_copy(v));
        }
    }
    {
        const char *cwd = am_str(state, "session_cwd");
        if (cwd && cwd[0]) json_set(kw, "cwd", json_string(cwd));
    }
    /* both assignments sit in one suppressed block: a raising profile lookup
     * skips the workspace too */
    if (!am_truthy(state ? json_obj_get(state, "profile_raises") : NULL)) {
        const json_t *profile = state ? json_obj_get(state, "profile") : NULL;
        json_set(kw, "agent_identity", profile ? json_copy(profile) : json_null());
        json_set(kw, "agent_workspace", json_string(AGENTINITM_AGENT_WORKSPACE));
    }
    return kw;
}

json_t *w26_agentinitm_60b1c8_constants(void) {
    json_t *o = json_object();
    json_t *ids = json_array();
    size_t i;
    for (i = 0; i < W26_AGENTINITM_60B1C8_IDENT_PARAMS; i++)
        json_append(ids, json_string(AGENTINITM_IDENT_PARAMS[i]));
    json_set(o, "identity_params", ids);
    json_set(o, "default_platform", json_string(AGENTINITM_DEFAULT_PLATFORM));
    json_set(o, "primary_context", json_string(AGENTINITM_PRIMARY_CONTEXT));
    json_set(o, "agent_workspace", json_string(AGENTINITM_AGENT_WORKSPACE));
    json_set(o, "warning_kind_unavailable", json_string("unavailable"));
    json_set(o, "warning_kind_parked", json_string("parked"));
    json_set(o, "warning_kind_load", json_string("load"));
    return o;
}
