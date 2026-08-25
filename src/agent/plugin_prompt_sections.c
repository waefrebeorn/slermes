/* plugin_prompt_sections.c — frozen plugin system-prompt sections.
 *
 * PoP: agent/system_prompt.py (_restore_plugin_prompt_sections,
 * _plugin_section_blocks, restore_plugin_prompt_sections) +
 * hermes_cli/plugins.py (format_system_prompt_section(s), container
 * constants).
 */

#include "plugin_prompt_sections.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTION_HEADING_PREFIX "## Plugin Context: "
#define SECTION_CHARS_TAG_FMT  "<!-- hermes-plugin-section-chars:%zu -->\n\n"

char *plugin_sections_format_one(const char *section_id, const char *content)
{
    if (!section_id) section_id = "";
    if (!content) content = "";
    size_t need = snprintf(NULL, 0,
                           SECTION_HEADING_PREFIX "%s\n"
                           "<!-- hermes-plugin-section-chars:%zu -->\n\n%s",
                           section_id, strlen(content), content);
    char *out = malloc(need + 1);
    if (!out) return NULL;
    snprintf(out, need + 1,
             SECTION_HEADING_PREFIX "%s\n"
             "<!-- hermes-plugin-section-chars:%zu -->\n\n%s",
             section_id, strlen(content), content);
    return out;
}

char *plugin_sections_format(const plugin_section_t *sections)
{
    if (!sections || !sections[0].id) return strdup("");

    /* Build blocks first so the container size is exact. */
    size_t n_blocks = 0;
    char **blocks = NULL;
    for (const plugin_section_t *s = sections; s && s->id; s++) {
        char *b = plugin_sections_format_one(s->id,
                                             s->content ? s->content : "");
        if (!b) continue;
        char **nb = realloc(blocks, (n_blocks + 1) * sizeof(char *));
        if (!nb) { free(b); break; }
        blocks = nb;
        blocks[n_blocks++] = b;
    }

    size_t cap = strlen(PLUGIN_SECTIONS_START) + strlen(PLUGIN_SECTIONS_END) + 3;
    for (size_t k = 0; k < n_blocks; k++)
        cap += strlen(blocks[k]) + 2;

    char *out = malloc(cap + 1);
    size_t off = 0;
    if (out && n_blocks == 0) { out[0] = '\0'; }
    else if (out) {
        off += (size_t)snprintf(out, cap + 1, "%s\n", PLUGIN_SECTIONS_START);
        bool first = true;
        for (size_t k = 0; k < n_blocks; k++) {
            if (!first) { out[off++] = '\n'; out[off++] = '\n'; }
            size_t bl = strlen(blocks[k]);
            memcpy(out + off, blocks[k], bl);
            off += bl;
            first = false;
        }
        off += (size_t)snprintf(out + off, cap + 1 - off, "\n%s",
                                PLUGIN_SECTIONS_END);
        out[off] = '\0';
    }

    for (size_t k = 0; k < n_blocks; k++) free(blocks[k]);
    free(blocks);
    return out;
}

/* Parse one frame starting at `p` (which must point at "## Plugin Context: ").
 * On success advances *pp past the header and returns malloc'd id; the frame
 * length is stored in *chars_out. Returns NULL on malformed frames. */
static char *parse_frame_header(const char **pp, size_t *chars_out)
{
    const char *p = *pp;
    static const char prefix[] = SECTION_HEADING_PREFIX;
    if (strncmp(p, prefix, sizeof(prefix) - 1) != 0) return NULL;
    p += sizeof(prefix) - 1;

    /* id: [a-z0-9][a-z0-9._-]{0,127} */
    const char *id_start = p;
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) return NULL;
    while ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
           *p == '.' || *p == '_' || *p == '-')
        p++;
    if (p - id_start > 128) return NULL;

    if (strncmp(p, "\n<!-- hermes-plugin-section-chars:", 34) != 0)
        return NULL;
    p += 34;
    char digits[8] = {0};
    size_t d = 0;
    while (*p >= '0' && *p <= '9' && d < sizeof(digits) - 1)
        digits[d++] = *p++;
    digits[d] = '\0';
    if (d == 0 || strncmp(p, " -->\n\n", 6) != 0) return NULL;
    p += 6;

    size_t id_len = (size_t)(p - id_start); /* p now sits at "\n<!--..." */
    /* p advanced past the 34-byte suffix; id spans id_start..newline */
    if (id_len == 0 || id_len > 128) return NULL;
    char *id = strndup(id_start, (size_t)((const char *)memchr(id_start, '\n', id_len) - id_start));
    *pp = p;
    *chars_out = (size_t)strtoul(digits, NULL, 10);
    return id;
}

json_t *plugin_sections_restore(const char *prompt)
{
    json_t *arr = json_new_array();
    if (!arr) return NULL;
    if (!prompt) return arr;

    const char *start = strstr(prompt, PLUGIN_SECTIONS_START);
    if (!start) return arr;
    start += strlen(PLUGIN_SECTIONS_START);
    if (*start == '\n') start++;

    const char *end = strstr(start, PLUGIN_SECTIONS_END);
    if (!end) return arr;
    /* LIVE requires the container be immediately followed by the
     * conversation-start marker. */
    size_t end_len = strlen(PLUGIN_SECTIONS_END);
    if (strncmp(end + end_len, "\n\nConversation started:", 23) != 0)
        return arr;

    const char *p = start;
    while (p < end) {
        if (strncmp(p, SECTION_HEADING_PREFIX,
                    sizeof(SECTION_HEADING_PREFIX) - 1) != 0)
            break;
        size_t chars = 0;
        const char *q = p;
        char *id = parse_frame_header(&q, &chars);
        if (!id || chars > MAX_SYSTEM_PROMPT_SECTION_CHARS) {
            free(id);
            break;
        }
        if (q + chars > end) { free(id); break; }
        char *content = strndup(q, chars);

        json_t *obj = json_object();
        if (!obj) { free(id); free(content); break; }
        json_set(obj, "id", json_string(id));
        json_set(obj, "content", json_string(content));
        json_append(arr, obj);
        free(id);
        free(content);

        p = q + chars;
        /* skip separator between blocks */
        if (strncmp(p, "\n\n", 2) == 0) p += 2;
    }
    return arr;
}

void plugin_sections_restore_into(json_t *agent_state, const char *prompt)
{
    if (!agent_state) return;
    json_t *restored = plugin_sections_restore(prompt);
    if (!restored) return;
    json_set(agent_state, "_plugin_system_prompt_sections_snapshot", restored);
}
