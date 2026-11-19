#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/skills_guard.py: 4 missing functions ── */

/* ── rest_revive1 (skills_guard_gaps) ───────────────────────────────────────
 * Pure pattern builders: each returns the exact regex SOURCE string the live
 * rf-templates produce for the given file_alt (the caller passes an already
 * escaped alternation; the builders themselves do not escape). The template
 * parts below were generated from the live defs with a sentinel file_alt and
 * verified to reconstruct the live output byte-for-byte. MODIFY_VERB_RE is
 * transcribed verbatim inside the prose template parts.
 * json_t *fn(void *io, json_t *in) — no I/O seams; io is unused. */

/* Concatenate template parts around file_alt (no printf: file_alt may
 * contain '%'). Returns a malloc'd string. */
static char *sg_build(const char *const *parts, size_t nparts, const char *file_alt) {
    size_t total = strlen(file_alt) * (nparts - 1) + 1;
    for (size_t i = 0; i < nparts; i++) total += strlen(parts[i]);
    char *out = malloc(total);
    if (!out) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < nparts; i++) {
        size_t n = strlen(parts[i]);
        memcpy(out + k, parts[i], n);
        k += n;
        if (i + 1 < nparts) {
            n = strlen(file_alt);
            memcpy(out + k, file_alt, n);
            k += n;
        }
    }
    out[k] = '\0';
    return out;
}

static json_t *sg_result(const char *const *parts, size_t nparts, json_t *in) {
    const char *file_alt = json_get_str(in, "file_alt", "");
    char *s = sg_build(parts, nparts, file_alt);
    json_t *out = json_string(s ? s : "");
    free(s);
    return out;
}

/* PoP: shell_write_re @ tools/skills_guard.py:_shell_write_re */
json_t *shell_write_re(void *io, json_t *in) {
    (void)io;
    static const char *const parts[] = {
        "(?:>>|[\\w\"\\'`)\\]]\\s*>)\\s*[~\\w./-]*",
        "(?!\\.?\\w)|\\bsed\\b[^\\n]*\\s(?:-[A-Za-z]*i[A-Za-z]*|--in-place)\\b[^\\n]*",
        "(?!\\.?\\w)|\\btee\\s+(?:-a\\s+)?[~\\w./\"\\'-]*",
        "(?!\\.?\\w)|\\b(?:cp|mv)\\s+[^\\s|;&]+\\s+[^\\n|;&]{0,40}?",
        "(?!\\.?\\w)",
    };
    return sg_result(parts, sizeof(parts) / sizeof(parts[0]), in);
}

/* PoP: prose_modify_re @ tools/skills_guard.py:_prose_modify_re */
json_t *prose_modify_re(void *io, json_t *in) {
    (void)io;
    static const char *const parts[] = {
        "^\\s*(?:[-*+]\\s+|\\d+[.)]\\s+)?(?:\\bwrit(?:e|es|ing)\\b|\\bwritten\\b|\\bedit(?:s|ed|ing)?\\b|\\bmodif(?:y|ies|ied|ying|ication)s?\\b|\\bupdat(?:e|es|ed|ing)\\b|\\bappend(?:s|ed|ing)?\\b|\\bprepend(?:s|ed|ing)?\\b|\\binject(?:s|ed|ing)?\\b|\\boverwrit(?:e|es|ing)\\b|\\boverwritten\\b|\\breplac(?:e|es|ed|ing)\\b|\\balter(?:s|ed|ing)?\\b|\\badd(?:s|ed|ing)\\b)[^\\n,]{0,80}?",
        "\\b|(?:\\byou\\s+(?:must|should|need\\s+to)\\s+|\\bplease\\s+|\\bmake\\s+sure\\s+(?:to\\s+|you\\s+)|\\bbe\\s+sure\\s+to\\s+)(?:\\bwrit(?:e|es|ing)\\b|\\bwritten\\b|\\bedit(?:s|ed|ing)?\\b|\\bmodif(?:y|ies|ied|ying|ication)s?\\b|\\bupdat(?:e|es|ed|ing)\\b|\\bappend(?:s|ed|ing)?\\b|\\bprepend(?:s|ed|ing)?\\b|\\binject(?:s|ed|ing)?\\b|\\boverwrit(?:e|es|ing)\\b|\\boverwritten\\b|\\breplac(?:e|es|ed|ing)\\b|\\balter(?:s|ed|ing)?\\b|\\badd(?:s|ed|ing)\\b)[^\\n,]{0,80}?",
        "\\b",
    };
    return sg_result(parts, sizeof(parts) / sizeof(parts[0]), in);
}

/* PoP: content_contract_re @ tools/skills_guard.py:_content_contract_re */
json_t *content_contract_re(void *io, json_t *in) {
    (void)io;
    static const char *const parts[] = {
        "",
        "\\b[^\\n]{0,40}?\\b(?:should|must|needs?\\s+to)\\s+(?:contain|say|include|have|list)\\b",
    };
    return sg_result(parts, sizeof(parts) / sizeof(parts[0]), in);
}

/* compute_docstring_lines: deferred (PoP tag removed; stub left untagged).
 * Needs the Python tokenizer; not ported in this revival. */
char *compute_docstring_lines(void) { return strdup("[]"); }
