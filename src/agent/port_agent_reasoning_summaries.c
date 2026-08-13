/*
 * port_agent_reasoning_summaries.c — Port of agent/reasoning_summaries.py.
 *
 * Boundary repair for providers that stream reasoning as discrete summary
 * parts. Reasoning-summary models (OpenAI gpt-5.x, Responses-API relays)
 * emit one reasoning_content delta per completed summary part, each opening
 * with a bold markdown heading ("**..."). On the OpenAI chat wire there is no
 * summary_index delimiter, so naive concatenation glues headings together:
 *
 *   **Investigating likely culprit PRs**Inspecting message schema**
 *
 * That "****" run is neither a bold close nor a bold open to a markdown parser,
 * so the whole trace renders as one unbroken half-bold paragraph. This module
 * inserts a paragraph break ("\n\n") when a delta opens a bold heading and the
 * accumulated text is mid-line.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int pcs_isspace_char(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f';
}

/* Return true if `s` ends with a whitespace character (mirrors Python
 * str[-1].isspace(), which returns False for the empty string). */
static int pcs_prev_ends_whitespace(const char *s)
{
    if (!s || !*s) return 0;
    size_t len = strlen(s);
    return pcs_isspace_char(s[len - 1]);
}

/* PoP: separate_glued_reasoning_blocks @ agent/reasoning_summaries.py:separate_glued_reasoning_blocks */
/* Return *delta*, prefixed with "\n\n" when it glues onto *previous*.
 *
 * A break is inserted when:
 *   - previous is non-empty AND
 *   - delta opens a bold heading ("**") AND
 *   - previous does NOT end in whitespace (mid-line) AND
 *   - delta contains a CLOSED heading ("**" after the opening pair) — a
 *     token-streamed fragment that merely opens emphasis without closing is
 *     not a part boundary.
 * Returns a malloc'd string (caller frees). */
char *rs_separate_glued_reasoning_blocks(const char *previous, const char *delta)
{
    if (!delta || !*delta) return strdup(delta ? delta : "");

    /* Need a previous segment that's non-empty and mid-line. */
    if (!previous || !*previous)
        return strdup(delta);

    /* delta must open a bold heading. */
    if (!(delta[0] == '*' && delta[1] == '*'))
        return strdup(delta);

    /* Already separated — the provider/previous ended the line. */
    if (pcs_prev_ends_whitespace(previous))
        return strdup(delta);

    /* Require a *closed* heading: a "**" after the opening pair.
     * delta[2:] must contain "**". */
    const char *rest = delta + 2;
    if (!strstr(rest, "**"))
        return strdup(delta);

    /* Insert the paragraph break. */
    size_t dlen = strlen(delta);
    char *out = malloc(dlen + 3); /* "\n\n" + delta + '\0' */
    if (!out) return strdup(delta);
    out[0] = '\n';
    out[1] = '\n';
    memcpy(out + 2, delta, dlen + 1);
    return out;
}
