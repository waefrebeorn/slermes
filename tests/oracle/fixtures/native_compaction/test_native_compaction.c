/*
 * test_native_compaction.c — behavioral oracle for native_compaction.c.
 * Cross-checked against the real Python agent/native_compaction.py.
 */

#include "native_compaction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = malloc(n);
    memcpy(r, s, n);
    return r;
}

static int g_fail = 0;
static int g_pass = 0;

static void check_bool(const char *name, bool got, bool exp) {
    if (got == exp) { g_pass++; }
    else { g_fail++; printf("FAIL %s: got=%d exp=%d\n", name, got, exp); }
}

static void check_long(const char *name, long got, long exp) {
    if (got == exp) { g_pass++; }
    else { g_fail++; printf("FAIL %s: got=%ld exp=%ld\n", name, got, exp); }
}

/* Compare a json_t* to an expected JSON string (whitespace-normalized). */
static void check_json_str(const char *name, const json_t *got_node, const char *exp_str) {
    if (!got_node && strcmp(exp_str, "null") == 0) { g_pass++; return; }
    char *got = got_node ? json_serialize(got_node) : xstrdup("null");
    /* normalize: strip spaces after : and , */
    char ng[8192], ne[8192];
    size_t j = 0;
    for (size_t i = 0; got[i]; i++) if (!(got[i]==':'||got[i]==',') || got[i+1]!=' ') ng[j++]=got[i]; else if (got[i]!=':'&&got[i]!=',') ng[j++]=got[i];
    ng[j]='\0';
    j = 0;
    for (size_t i = 0; exp_str[i]; i++) if (!(exp_str[i]==':'||exp_str[i]==',') || exp_str[i+1]!=' ') ne[j++]=exp_str[i]; else if (exp_str[i]!=':'&&exp_str[i]!=',') ne[j++]=exp_str[i];
    ne[j]='\0';
    if (strcmp(ng, ne) == 0) g_pass++;
    else { g_fail++; printf("FAIL %s:\n  got=%s\n  exp=%s\n", name, ng, ne); }
    free(got);
}

/* Compare two json_t* serialized (whitespace-normalized). */
static void check_json_node(const char *name, const json_t *got, const json_t *exp) {
    char *gs = got ? json_serialize(got) : xstrdup("null");
    char *es = exp ? json_serialize(exp) : xstrdup("null");
    /* normalize */
    char ng[16384], ne[16384]; size_t j=0;
    for (size_t i=0; gs[i]; i++){ if((gs[i]==':'||gs[i]==',')&&gs[i+1]==' '){} else ng[j++]=gs[i]; }
    ng[j]='\0'; j=0;
    for (size_t i=0; es[i]; i++){ if((es[i]==':'||es[i]==',')&&es[i+1]==' '){} else ne[j++]=es[i]; }
    ne[j]='\0';
    if (strcmp(ng,ne)==0) g_pass++; else { g_fail++; printf("FAIL %s:\n  got=%s\n  exp=%s\n", name, ng, ne); }
    free(gs); free(es);
}

int main(void) {
    /* model gate */
    check_bool("model_gpt-5.6", native_compaction_is_native_model("gpt-5.6"), true);
    check_bool("model_gpt-5.6-mini", native_compaction_is_native_model("gpt-5.6-mini"), true);
    check_bool("model_gpt-5.1", native_compaction_is_native_model("gpt-5.1"), false);
    check_bool("model_gpt-4o", native_compaction_is_native_model("gpt-4o"), false);
    check_bool("model_None", native_compaction_is_native_model(NULL), false);

    /* route */
    check_bool("route_api.openai.com_v1", native_compaction_is_direct_openai_route("https://api.openai.com/v1", false), true);
    check_bool("route_api.openai.com", native_compaction_is_direct_openai_route("https://api.openai.com", false), true);
    check_bool("route_api.openai.com.evil.com", native_compaction_is_direct_openai_route("https://api.openai.com.evil.com", false), false);
    check_bool("route_xai.com", native_compaction_is_direct_openai_route("https://xai.com", false), false);
    check_bool("route_", native_compaction_is_direct_openai_route("", false), false);
    check_bool("route_codex", native_compaction_is_direct_openai_route("https://xai.com", true), true);

    /* threshold */
    check_long("thr_250000_None", native_compaction_resolve_threshold("250000", NULL), 250000);
    check_long("thr_250000_100000", native_compaction_resolve_threshold("250000", "100000"), 91808);
    check_long("thr_-5_100000", native_compaction_resolve_threshold("-5", "100000"), 91808);
    check_long("thr_abc_100000", native_compaction_resolve_threshold("abc", "100000"), 91808);
    check_long("thr_250000_5000", native_compaction_resolve_threshold("250000", "5000"), 4000);
    check_long("thr_250000_2000", native_compaction_resolve_threshold("250000", "2000"), 1600);

    /* rejection */
    check_bool("rej1", native_compaction_is_rejection("unknown parameter context_management", "400"), true);
    check_bool("rej2", native_compaction_is_rejection("compact_threshold is unsupported", "400"), true);
    check_bool("rej3", native_compaction_is_rejection("context_management field echoed but request ok", "200"), false);
    check_bool("rej4", native_compaction_is_rejection("random error mentioning context_management", NULL), false);
    check_bool("rej5", native_compaction_is_rejection("bad request: compact_threshold invalid", "400"), true);
    check_bool("rej6", native_compaction_is_rejection("server error 500: context_management", "500"), false);

    /* checkpoint */
    char *has_cp_json = "[{\"type\":\"message\",\"role\":\"user\",\"content\":\"hi\"},{\"type\":\"compaction\",\"compact_threshold\":1000},{\"type\":\"message\",\"role\":\"user\",\"content\":\"bye\"}]";
    char *has_cp_no_json = "[{\"type\":\"message\",\"role\":\"user\"}]";
    json_t *hc = json_parse(has_cp_json, NULL);
    json_t *hcn = json_parse(has_cp_no_json, NULL);
    check_bool("has_cp", native_compaction_has_checkpoint(hc), true);
    check_bool("has_cp_no", native_compaction_has_checkpoint(hcn), false);

    /* merge */
    char *prior_json = "[{\"type\":\"compaction\",\"compact_threshold\":1}]";
    char *new_json = "[{\"type\":\"message\",\"role\":\"assistant\",\"content\":\"x\"}]";
    json_t *prior = json_parse(prior_json, NULL);
    json_t *nw = json_parse(new_json, NULL);
    json_t *m_keep = native_compaction_merge_interim_items(prior, nw);
    check_json_str("merge_keep", m_keep, "[{\"type\":\"compaction\",\"compact_threshold\":1},{\"type\":\"message\",\"role\":\"assistant\",\"content\":\"x\"}]");
    json_t *m_nokeep = native_compaction_merge_interim_items(hcn, nw);
    check_json_str("merge_nokeep", m_nokeep, "[{\"type\":\"message\",\"role\":\"assistant\",\"content\":\"x\"}]");

    /* context_management */
    json_t *cm_yes = native_compaction_context_management(true, true, "gpt-5.6", "https://api.openai.com", false, false, false, "250000", "100000");
    check_json_str("cm_yes", cm_yes, "[{\"type\":\"compaction\",\"compact_threshold\":91808}]");
    json_t *cm_off = native_compaction_context_management(false, true, "gpt-5.6", "https://api.openai.com", false, false, false, "250000", "100000");
    check_bool("cm_off_null", cm_off == NULL, true);
    json_t *cm_bad = native_compaction_context_management(true, true, "gpt-4o", "https://api.openai.com", false, false, false, "250000", "100000");
    check_bool("cm_badmodel_null", cm_bad == NULL, true);

    /* prune */
    char *prune_in = "[{\"type\":\"message\",\"role\":\"user\",\"content\":\"";
    /* content 4000 'a's */
    char content[4002];
    for (int i = 0; i < 4000; i++) content[i] = 'a';
    content[4000] = '\0';
    char *prune_full = malloc(strlen(prune_in) + 4000 + 200);
    sprintf(prune_full, "%s%s\"},{\"type\":\"compaction\",\"compact_threshold\":1000},{\"type\":\"message\",\"role\":\"user\",\"content\":\"after\"}]", prune_in, content);
    json_t *prune_items = json_parse(prune_full, NULL);
    json_t *pruned = native_compaction_prune_pre_checkpoint_items(prune_items, 64000);
    /* expected matches Python: checkpoint run, then retained user msg, then post */
    char *exp_full = malloc(strlen(prune_in) + 4000 + 300);
    sprintf(exp_full, "[{\"type\":\"compaction\",\"compact_threshold\":1000},{\"type\":\"message\",\"role\":\"user\",\"content\":\"%s\"},{\"type\":\"message\",\"role\":\"user\",\"content\":\"after\"}]", content);
    json_t *exp_pruned = json_parse(exp_full, NULL);
    check_json_node("prune", pruned, exp_pruned);
    free(exp_full);
    free(prune_full);

    /* cleanup */
    json_free(hc); json_free(hcn); json_free(prior); json_free(nw);
    json_free(cm_yes); json_free(pruned); json_free(exp_pruned);
    json_free(prune_items); json_free(m_keep); json_free(m_nokeep);

    if (g_fail == 0) printf("ORACLE PASSED (%d checks)\n", g_pass);
    else printf("ORACLE FAILED (%d failures, %d passed)\n", g_fail, g_pass);
    return g_fail == 0 ? 0 : 1;
}
