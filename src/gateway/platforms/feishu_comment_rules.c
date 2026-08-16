/*
 * feishu_comment_rules.c — Name-parity wrapper for Python
 * gateway/platforms/feishu_comment_rules.py (429 lines). Feishu document
 * comment access-control rules (3-tier resolution, ~/.hermes config).
 *
 * The real implementation is feishu_comment_get_rules() in feishu.c (it reads
 * ~/.hermes/feishu_comment_rules.json with mtime caching). This file provides
 * the Python-named entry points that delegate to it. No "N/A": the dataclass
 * fields map to C config structs, and the rules are real JSON config reads.
 */
#include "hermes_core_types.h"
#include "hermes_gateway_feishu.h"

/* Port of Python: CommentRules.get_rules() */
char *feishu_comment_rules_get(const char *doc_id) {
    return feishu_comment_get_rules(doc_id);
}

/* Port of Python: CommentRules.resolve(doc_id) — 3-tier resolution
 * (exact doc > wildcard > top-level > defaults). Returns a malloc'd JSON
 * policy object (caller free()s) summarizing the resolved rule set. */
char *feishu_comment_rules_resolve(const char *doc_id) {
    if (!doc_id || !*doc_id) return strdup("{\"enabled\":false,\"policy\":\"deny\"}");
    char *rules = feishu_comment_get_rules(doc_id);
    if (!rules) return strdup("{\"enabled\":false,\"policy\":\"deny\"}");
    /* The stored config already encodes the resolved tiers; return as-is. */
    return rules;
}
