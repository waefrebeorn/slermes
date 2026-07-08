/*
 * t_port_config_pure.c — faithful verification harness for port_config_pure.c
 *
 * Compiled SEPARATELY and linked against the real lib/libjson/json.o
 * (NOT the slermes binary). Includes port_config_pure.c directly so it
 * exercises the exact compiled object that ships in the binary.
 *
 * Each check asserts the C port produces output byte-equivalent to the
 * Python source on the same inputs. Exits 0 only if ALL checks pass.
 */

#include "port_config_pure.c"   /* pull in the real functions + tables */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

/* compare two JSON trees by serializing (canonical, single-line) */
static int json_eq(const json_t *a, const json_t *b)
{
    if (!a || !b) return (a == b);
    char *sa = json_serialize(a);
    char *sb = json_serialize(b);
    int eq = sa && sb && strcmp(sa, sb) == 0;
    free(sa); free(sb);
    return eq;
}

static void check(const char *name, int cond)
{
    if (cond) printf("  PASS  %s\n", name);
    else { printf("  FAIL  %s\n", name); failures++; }
}

int main(void)
{
    /* ---- config_deep_merge ---- */
    {
        const char *base = "{\"tts\":{\"elevenlabs\":{\"model_id\":\"m1\"}},\"x\":1}";
        const char *ov   = "{\"tts\":{\"elevenlabs\":{\"voice_id\":\"v1\"}}}";
        json_t *b = json_parse(base, NULL);
        json_t *o = json_parse(ov, NULL);
        json_t *r = config_deep_merge(b, o);
        const char *exp = "{\"tts\":{\"elevenlabs\":{\"model_id\":\"m1\",\"voice_id\":\"v1\"}},\"x\":1}";
        json_t *e = json_parse(exp, NULL);
        check("deep_merge_recurses_nested", json_eq(r, e));
        check("deep_merge_override_replaces",
              json_get_num(r, "x", -1) == 1.0);
        json_free(b); json_free(o); json_free(r); json_free(e);
    }
    {
        /* override scalar replaces (does not grow) */
        json_t *b = json_parse("{\"a\":{\"b\":1}}", NULL);
        json_t *o = json_parse("{\"a\":5}", NULL);
        json_t *r = config_deep_merge(b, o);
        check("deep_merge_scalar_overrides", json_get_num(r, "a", -1) == 5.0);
        json_free(b); json_free(o); json_free(r);
    }

    /* ---- config_items_by_unique_name ---- */
    {
        json_t *ok = json_parse("[{\"name\":\"a\",\"v\":1},{\"name\":\"b\",\"v\":2}]", NULL);
        json_t *r = config_items_by_unique_name(ok);
        check("items_by_unique_name_indexes",
              r && json_get_num(json_obj_get(r, "a"), "v", -1) == 1.0);
        json_free(ok); json_free(r);
    }
    {
        /* duplicate name -> NULL */
        json_t *dup = json_parse("[{\"name\":\"a\"},{\"name\":\"a\"}]", NULL);
        json_t *r = config_items_by_unique_name(dup);
        check("items_by_unique_name_dup_returns_null", r == NULL);
        json_free(dup);
    }
    {
        /* non-dict item -> NULL */
        json_t *bad = json_parse("[{\"name\":\"a\"},\"notdict\"]", NULL);
        json_t *r = config_items_by_unique_name(bad);
        check("items_by_unique_name_nondict_null", r == NULL);
        json_free(bad);
    }
    {
        /* missing name -> NULL */
        json_t *non = json_parse("[{\"v\":1}]", NULL);
        json_t *r = config_items_by_unique_name(non);
        check("items_by_unique_name_missing_name_null", r == NULL);
        json_free(non);
    }

    /* ---- config_normalize_max_turns ---- */
    {
        /* root-level only -> migrates + injects default? NO: had_root true so
         * it copies root value, does NOT inject default 90. */
        json_t *c = json_parse("{\"max_turns\":7}", NULL);
        json_t *r = config_normalize_max_turns(c);
        json_t *ag = json_obj_get(r, "agent");
        check("max_turns_root_migrated", ag && json_get_num(ag, "max_turns", -1) == 7.0);
        check("max_turns_root_removed", json_obj_get(r, "max_turns") == NULL);
        json_free(c); json_free(r);
    }
    {
        /* agent-level only -> kept, no default injected */
        json_t *c = json_parse("{\"agent\":{\"model\":\"x\"}}", NULL);
        json_t *r = config_normalize_max_turns(c);
        json_t *ag = json_obj_get(r, "agent");
        check("max_turns_agent_kept_no_default",
              ag && json_obj_get(ag, "max_turns") == NULL);
        json_free(c); json_free(r);
    }
    {
        /* neither -> no agent.max_turns injected (default stays absent) */
        json_t *c = json_parse("{\"model\":\"gpt\"}", NULL);
        json_t *r = config_normalize_max_turns(c);
        json_t *ag = json_obj_get(r, "agent");
        check("max_turns_neither_no_inject",
              ag == NULL || json_obj_get(ag, "max_turns") == NULL);
        json_free(c); json_free(r);
    }
    {
        /* root + agent -> agent value wins, no default injected */
        json_t *c = json_parse("{\"max_turns\":7,\"agent\":{\"max_turns\":3}}", NULL);
        json_t *r = config_normalize_max_turns(c);
        json_t *ag = json_obj_get(r, "agent");
        check("max_turns_root_and_agent", ag && json_get_num(ag, "max_turns", -1) == 3.0);
        json_free(c); json_free(r);
    }

    /* ---- config_strip_non_ascii_credential ---- */
    {
        char *r = config_strip_non_ascii_credential("KEY", "abc123", NULL, 0);
        check("strip_ascii_passthrough", r && strcmp(r, "abc123") == 0);
        free(r);
    }
    {
        /* U+00E9 (é) and U+2014 (—) get stripped */
        char *r = config_strip_non_ascii_credential("KEY", "sk-éabc—def", NULL, 0);
        check("strip_removes_nonascii", r && strcmp(r, "sk-abcdef") == 0);
        free(r);
    }
    {
        char warn[1024];
        /* U+00E9 == 0xC3 0xA9 in UTF-8 */
        char nasty[] = {'a','b',(char)0xC3,(char)0xA9,'c','d','\0'};
        char *r = config_strip_non_ascii_credential("MYKEY", nasty, warn, sizeof(warn));
        check("strip_warns_on_nonascii", r && warn[0] != '\0' && strstr(warn, "MYKEY") != NULL);
        free(r);
    }

    /* ---- provider_group_for_slug ---- */
    {
        check("group_kimi_coding", strcmp(provider_group_for_slug("kimi-coding"), "kimi") == 0);
        check("group_minimax_oauth", strcmp(provider_group_for_slug("minimax-oauth"), "minimax") == 0);
        check("group_xai_oauth", strcmp(provider_group_for_slug("xai-oauth"), "xai") == 0);
        check("group_copilot_acp", strcmp(provider_group_for_slug("copilot-acp"), "copilot") == 0);
        check("group_gemini", strcmp(provider_group_for_slug("gemini"), "google") == 0);
        check("group_ungrouped_empty", strcmp(provider_group_for_slug("openrouter"), "") == 0);
        check("group_case_insensitive", strcmp(provider_group_for_slug("XAI-OAuth"), "xai") == 0);
        check("group_strips_ws", strcmp(provider_group_for_slug("  xai-oauth "), "xai") == 0);
        check("group_none_arg_empty", strcmp(provider_group_for_slug(NULL), "") == 0);
    }

    printf("\n%s (%d failures)\n", failures ? "FAILED" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
