/* t_port_gateway_session_state.c — differential oracle driver for
 * gateway/session_state.py. Exercises the port and prints one compact JSON
 * object per case so tests/sta_oracle_gateway_session_state.py can compare
 * against live Python.
 */

#include "port_gateway_session_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jb(bool v) { return v ? "true" : "false"; }

/* Serialize a json_t compactly to stdout (no newline). */
static void emit_json(const json_t *v) {
    if (!v) { fputs("null", stdout); return; }
    char *s = json_serialize(v);
    fputs(s ? s : "null", stdout);
    free(s);
}

static session_state_registry_t *new_runner(void) {
    return relay_session_state_registry_new();
}

int main(void) {
    /* --- turn.clear --- */
    {
        session_state_registry_t *r = new_runner();
        session_state_t *s = relay_session_state_get_or_create(r, "k");
        s->turn.agent = json_string("AGENT");
        s->turn.started_ts = json_number(123.0);
        s->turn.lease = json_string("LEASE");
        s->turn.lease_token = json_string("TOK");
        s->turn.lease_generation = json_number(7);
        relay_session_state_turn_clear(&s->turn);
        printf("{\"case\":\"turn_clear\",\"agent_null\":%s,\"started_zero\":%s,\"lease_null\":%s,\"lease_token_kept\":%s,\"lease_gen_kept\":%s}\n",
               jb(s->turn.agent->type == JSON_NULL),
               jb(s->turn.started_ts->num_val == 0.0),
               jb(s->turn.lease->type == JSON_NULL),
               jb(s->turn.lease_token->type != JSON_NULL),
               jb((long)s->turn.lease_generation->num_val == 7));
        relay_session_state_registry_free(r);
    }

    /* --- conversation.clear --- */
    {
        session_state_registry_t *r = new_runner();
        session_state_t *s = relay_session_state_get_or_create(r, "k");
        s->conversation.model_override = json_string("M");
        s->conversation.service_tier_override = json_string("priority");
        s->conversation.last_resolved_model = json_string("gpt");
        json_t *qe = json_array(); json_append(qe, json_string("e"));
        s->conversation.queued_events = qe;
        relay_session_state_conversation_clear(&s->conversation);
        printf("{\"case\":\"conv_clear\",\"model_null\":%s,\"tier_unset\":%s,\"last_empty\":%s,\"queued_empty\":%s}\n",
               jb(s->conversation.model_override->type == JSON_NULL),
               jb(s->conversation.service_tier_override == relay_session_state_unset_tier()),
               jb(strcmp(s->conversation.last_resolved_model->str_val ? s->conversation.last_resolved_model->str_val : "", "") == 0),
               jb(s->conversation.queued_events->c.count == 0));
        relay_session_state_registry_free(r);
    }

    /* --- SessionFieldView set/get/contains/absent --- */
    {
        session_state_registry_t *r = new_runner();
        session_field_view_t *v = relay_session_field_view_new(r, LEGACY_RUNNING_AGENTS);
        int rc;
        relay_session_field_view_set(v, "k1", json_string("A"));
        int rc2;
        json_t *got = relay_session_field_view_get(v, "k1", &rc);
        bool contains = relay_session_field_view_contains(v, "k1");
        json_t *miss = relay_session_field_view_get(v, "nope", &rc2);
        bool contains_miss = relay_session_field_view_contains(v, "nope");
        printf("{\"case\":\"view_getset\",\"get_val\":");
        emit_json(got);
        printf(",\"rc_ok\":%s,\"contains\":%s,\"miss_null\":%s,\"contains_miss\":%s}\n",
               jb(rc == SFV_OK), jb(contains), jb(miss == NULL), jb(!contains_miss));
        relay_session_field_view_free(v);
        relay_session_state_registry_free(r);
    }

    /* --- SessionFieldView del --- */
    {
        session_state_registry_t *r = new_runner();
        session_field_view_t *v = relay_session_field_view_new(r, LEGACY_RUNNING_AGENTS);
        relay_session_field_view_set(v, "k1", json_string("A"));
        int d1 = relay_session_field_view_del(v, "k1");
        bool after_del = relay_session_field_view_contains(v, "k1");
        int d2 = relay_session_field_view_del(v, "k1"); /* already absent */
        printf("{\"case\":\"view_del\",\"del_ok\":%s,\"after_del_absent\":%s,\"del_absent_rc\":%d}\n",
               jb(d1 == SFV_OK), jb(!after_del), d2);
        relay_session_field_view_free(v);
        relay_session_state_registry_free(r);
    }

    /* --- SessionFieldView iter/len --- */
    {
        session_state_registry_t *r = new_runner();
        session_field_view_t *v = relay_session_field_view_new(r, LEGACY_RUNNING_AGENTS);
        relay_session_field_view_set(v, "k1", json_string("A"));
        relay_session_field_view_set(v, "k2", json_string("B"));
        relay_session_field_view_set(v, "k3", json_string("C"));
        const char **keys = NULL;
        size_t n = relay_session_field_view_keys(v, &keys);
        printf("{\"case\":\"view_iter\",\"count\":%zu", n);
        for (size_t i = 0; i < n; i++) printf(",\"key%d\":\"%s\"", (int)i, keys[i]);
        printf("}\n");
        free((void*)keys);
        relay_session_field_view_free(v);
        relay_session_state_registry_free(r);
    }

    /* --- SessionFieldView clear --- */
    {
        session_state_registry_t *r = new_runner();
        session_field_view_t *v = relay_session_field_view_new(r, LEGACY_RUNNING_AGENTS);
        relay_session_field_view_set(v, "k1", json_string("A"));
        relay_session_field_view_set(v, "k2", json_string("B"));
        relay_session_field_view_clear(v);
        bool c1 = relay_session_field_view_contains(v, "k1");
        bool c2 = relay_session_field_view_contains(v, "k2");
        printf("{\"case\":\"view_clear\",\"c1\":%s,\"c2\":%s}\n", jb(!c1), jb(!c2));
        relay_session_field_view_free(v);
        relay_session_state_registry_free(r);
    }

    /* --- service_tier_override presence (UNSET sentinel) --- */
    {
        session_state_registry_t *r = new_runner();
        session_field_view_t *v = relay_session_field_view_new(r, LEGACY_SESSION_SERVICE_TIER_OVERRIDES);
        bool def_present = relay_session_field_view_contains(v, "k1");
        relay_session_field_view_set(v, "k1", json_string("priority"));
        bool set_present = relay_session_field_view_contains(v, "k1");
        relay_session_field_view_set(v, "k1", (json_t*)relay_session_state_unset_tier());
        bool unset_present = relay_session_field_view_contains(v, "k1");
        printf("{\"case\":\"service_tier\",\"def_present\":%s,\"set_present\":%s,\"unset_present\":%s}\n",
               jb(def_present), jb(set_present), jb(unset_present));
        relay_session_field_view_free(v);
        relay_session_state_registry_free(r);
    }

    /* --- last_resolved_model / run_generation zero-presence --- */
    {
        session_state_registry_t *r = new_runner();
        session_field_view_t *vm = relay_session_field_view_new(r, LEGACY_LAST_RESOLVED_MODEL);
        session_field_view_t *vg = relay_session_field_view_new(r, LEGACY_SESSION_RUN_GENERATION);
        bool m_def = relay_session_field_view_contains(vm, "k1");
        relay_session_field_view_set(vm, "k1", json_string("gpt-4"));
        bool m_set = relay_session_field_view_contains(vm, "k1");
        bool g_def = relay_session_field_view_contains(vg, "k1");
        relay_session_field_view_set(vg, "k1", json_number(5));
        bool g_set = relay_session_field_view_contains(vg, "k1");
        printf("{\"case\":\"zero_presence\",\"model_def\":%s,\"model_set\":%s,\"gen_def\":%s,\"gen_set\":%s}\n",
               jb(m_def), jb(m_set), jb(g_def), jb(g_set));
        relay_session_field_view_free(vm); relay_session_field_view_free(vg);
        relay_session_state_registry_free(r);
    }

    /* --- TurnLeaseTokenView --- */
    {
        session_state_registry_t *r = new_runner();
        turn_lease_token_view_t *v = relay_turn_lease_token_view_new(r);
        json_t *kpair = json_array();
        json_append(kpair, json_string("k1"));
        json_append(kpair, json_number(3));
        int rc;
        relay_turn_lease_token_view_set(v, kpair, json_string("TOK"));
        int rc2;
        json_t *got = relay_turn_lease_token_view_get(v, kpair, &rc);
        /* wrong generation */
        json_t *kpair2 = json_array();
        json_append(kpair2, json_string("k1"));
        json_append(kpair2, json_number(9));
        json_t *miss = relay_turn_lease_token_view_get(v, kpair2, &rc2);
        bool contains_ok = relay_turn_lease_token_view_contains(v, kpair);
        bool contains_bad = relay_turn_lease_token_view_contains(v, kpair2);
        json_t **kp = NULL;
        size_t kn = relay_turn_lease_token_view_keys(v, &kp);
        printf("{\"case\":\"lease_view\",\"set_ok\":%s,\"get_val\":", jb(rc == SFV_OK));
        emit_json(got);
        printf(",\"miss_null\":%s,\"contains_ok\":%s,\"contains_bad\":%s,\"iter_count\":%zu}\n",
               jb(miss == NULL), jb(contains_ok), jb(!contains_bad), kn);
        for (size_t i = 0; i < kn; i++) json_free(kp[i]);
        free(kp);
        json_free(kpair); json_free(kpair2);
        relay_turn_lease_token_view_free(v);
        relay_session_state_registry_free(r);
    }

    /* --- TurnLeaseTokenView del --- */
    {
        session_state_registry_t *r = new_runner();
        turn_lease_token_view_t *v = relay_turn_lease_token_view_new(r);
        json_t *kpair = json_array();
        json_append(kpair, json_string("k1"));
        json_append(kpair, json_number(3));
        relay_turn_lease_token_view_set(v, kpair, json_string("TOK"));
        int d1 = relay_turn_lease_token_view_del(v, kpair);
        bool after = relay_turn_lease_token_view_contains(v, kpair);
        int d2 = relay_turn_lease_token_view_del(v, kpair);
        printf("{\"case\":\"lease_del\",\"del_ok\":%s,\"after_absent\":%s,\"del_absent_rc\":%d}\n",
               jb(d1 == SFV_OK), jb(!after), d2);
        json_free(kpair);
        relay_turn_lease_token_view_free(v);
        relay_session_state_registry_free(r);
    }

    return 0;
}
