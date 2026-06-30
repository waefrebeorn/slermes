/*
 * port_agent_learn_prompt.c — Port of Python agent/learn_prompt.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Port of Python: _AUTHORING_STANDARDS */
const char *learn_prompt_authoring_standards =
    "Follow the Hermes skill-authoring standards exactly:\n\n"
    "Frontmatter:\n"
    "- name: lowercase-hyphenated, <=64 chars, no spaces.\n"
    "- description: ONE sentence, <=60 characters, ends with a period. State the\n"
    "  capability, not the implementation. No marketing words (powerful,\n"
    "  comprehensive, seamless, advanced). Do NOT repeat the skill name. If the\n"
    "  description contains a colon, wrap the whole value in double quotes.\n"
    "- version: 0.1.0\n"
    "- metadata.hermes.tags: a few Capitalized, Relevant, Tags.\n\n"
    "Body section order (omit a section only if it genuinely has no content):\n"
    "1. \"# <Human Title>\" then a 2-3 sentence intro: what it does, what it does NOT\n"
    "   do, and the key dependency stance (e.g. \"stdlib only\").\n"
    "2. \"## When to Use\" — bullet list of concrete trigger phrases.\n"
    "3. \"## Prerequisites\" — exact env vars, install steps, credentials.\n"
    "4. \"## How to Run\" — the canonical invocation, framed through Hermes tools.\n"
    "5. \"## Quick Reference\" — a flat command/endpoint list, no narration.\n"
    "6. \"## Procedure\" — numbered steps with copy-paste-exact commands.\n"
    "7. \"## Pitfalls\" — known limits, rate limits, things that look broken but aren't.\n"
    "8. \"## Verification\" — a single command/check that proves the skill worked.\n\n"
    "Hermes-tool framing (this is what makes it a skill, not shell docs):\n"
    "- Frame running scripts as \"invoke through the `terminal` tool\".\n"
    "- Use `read_file` (not cat/head/tail), `search_files` (not grep/find/ls),\n"
    "  `patch` (not sed/awk), `web_extract` (not curl-to-scrape),\n"
    "  `vision_analyze` for images. Reference these tools by name in backticks.\n"
    "- Do NOT name shell utilities the agent already has wrapped.\n\n"
    "Quality bar:\n"
    "- Prefer exact commands, endpoint URLs, function signatures, and config keys\n"
    "  that appear VERBATIM in the source. NEVER invent flags, paths, or APIs — if\n"
    "  you didn't see it in the source, don't write it.\n"
    "- Keep it tight and scannable: ~100 lines for a simple skill, ~200 for a\n"
    "  complex one. Don't re-paste the source docs.\n"
    "- Don't write a router/index/hub skill that only points at other skills.\n"
    "- Larger scripts/parsers belong in a `scripts/` file (add via\n"
    "  `skill_manage` write_file), referenced from SKILL.md by relative path — not\n"
    "  inlined for the agent to re-type every run.";

/* Port of Python: build_learn_prompt */
char *build_learn_prompt(const char *user_request) {
    if (!user_request || !*user_request) {
        user_request = "the workflow we just went through in this conversation — review "
                      "the steps taken and distill them into a reusable skill";
    }
    
    /* Build the prompt */
    static char prompt[4096];
    int pos = 0;
    
    pos += snprintf(prompt + pos, sizeof(prompt) - pos,
        "[/learn] The user wants you to learn a reusable skill from the "
        "source(s) they described below, and save it.\n\n"
        "WHAT TO LEARN FROM:\n%s\n\n"
        "Do this:\n"
        "1. Gather the material. Resolve whatever the user named using the "
        "tools you already have — `read_file`/`search_files` for local files "
        "or directories, `web_extract` for URLs, the current conversation "
        "history if they referred to something you just did, and the text "
        "they pasted as-is. If the request is ambiguous about scope, make a ",
        user_request);
    
    pos += snprintf(prompt + pos, sizeof(prompt) - pos,
        "reasonable assumption and proceed.\n"
        "2. Author a single `SKILL.md` via `skill_manage` that follows the Hermes "
        "skill-authoring standards embedded below. Use `skill_manage` action "
        "create/write_file with the skill name and the complete markdown content.\n\n"
        "STANDARDS:\n%s\n\n"
        "Return the skill name you created.",
        learn_prompt_authoring_standards);
    
    return prompt;
}