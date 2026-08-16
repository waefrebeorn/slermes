/* Oracle test: prove codex_responses_adapter.c matches Python behavior. */
#include "codex_responses_adapter.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)){ printf("FAIL: %s\n", msg); failures++; } else { printf("ok: %s\n", msg); } } while(0)

int main(void) {
    /* 1) Harmony-token neutralization: zero-width Cf + fullwidth pipe. */
    const char *in = "hello\u200bworld\uff5csecret";
    char *out = codex_neutralize_harmony_tokens(in);
    CHECK(out != NULL, "neutralize returns non-null");
    /* U+200B (Cf) is stripped; U+FF5C is NOT Cf so it survives as fullwidth pipe. */
    CHECK(strcmp(out, "helloworld｜secret") == 0, "zero-width stripped, fullwidth pipe kept");
    free(out);

    /* 2) Classify issuer by base url. */
    const char *iss1 = codex_classify_responses_issuer(0, 0, 1, "https://api.openai.com/v1");
    CHECK(iss1 && strcmp(iss1, "codex_backend") == 0, "codex_backend issuer");
    const char *iss2 = codex_classify_responses_issuer(0, 1, 0, "https://api.githubcopilot.com");
    CHECK(iss2 && strcmp(iss2, "github_responses") == 0, "github_responses issuer");

    /* 2b) Harmony control token defang <|word|> -> <｜word｜>. */
    char *def = codex_neutralize_harmony_tokens("think <|constrain|> now");
    CHECK(def && strcmp(def, "think <｜constrain｜> now") == 0, "harmony token defanged to fullwidth pipe");
    free(def);

    /* 3) Tool schema conversion: chat format -> responses format. */
    json_node_t *tools = json_new_array();
    json_node_t *t = json_new_object();
    json_object_set(t, "type", json_new_string("function"));
    json_node_t *fn = json_new_object();
    json_object_set(fn, "name", json_new_string("search"));
    json_object_set(fn, "description", json_new_string("do a search"));
    json_node_t *params = json_new_object();
    json_object_set(params, "type", json_new_string("object"));
    json_object_set(fn, "parameters", params);
    json_object_set(t, "function", fn);
    json_array_append(tools, t);

    json_node_t *conv = codex_responses_tools(tools);
    CHECK(conv != NULL, "responses_tools returns non-null");
    if (conv) {
        json_node_t *first = json_get(conv, 0);
        CHECK(first != NULL, "one tool emitted");
        const char *nm = json_get_str(first, "name", NULL);
        CHECK(nm && strcmp(nm, "search") == 0, "tool name preserved");
        const char *ty = json_get_str(first, "type", NULL);
        CHECK(ty && strcmp(ty, "function") == 0, "tool type preserved");
        CHECK(json_object_get(first, "parameters") != NULL, "parameters preserved");
        CHECK(json_object_get(first, "function") == NULL, "no nested function wrapper (responses format)");
        json_free(conv);
    }
    json_free(tools);

    /* 4) Deterministic call_id is stable + 24-hex. */
    char *a = codex_deterministic_call_id("search", "{\"q\":1}", 0);
    char *b = codex_deterministic_call_id("search", "{\"q\":1}", 0);
    CHECK(a && b && strcmp(a, b) == 0, "deterministic call_id stable");
    CHECK(a && strncmp(a, "call_", 5) == 0, "call_id prefixed call_");
    free(a); free(b);

    /* 5) tool-id split/derive. */
    char *cid = NULL, *rid = NULL;
    codex_split_responses_tool_id("fc_abc123", &cid, &rid);
    CHECK(cid == NULL, "split: fc_ id maps to response_item_id, call_id NULL");
    CHECK(rid && strcmp(rid, "fc_abc123") == 0, "split keeps response_item_id");
    free(cid); free(rid);
    char *d = codex_derive_responses_function_call_id("call_xyz", "resp_1");
    CHECK(d && strncmp(d, "fc_", 3) == 0, "derive yields fc_ id");
    free(d);

    printf("\n%s (%d failures)\n", failures ? "ORACLE FAIL" : "ORACLE PASS", failures);
    return failures ? 1 : 0;
}
