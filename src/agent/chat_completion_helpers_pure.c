/*
 * chat_completion_helpers_pure.c
 *
 * Four pure helpers from agent/chat_completion_helpers.py that had no PoP
 * credit anywhere in the tree:
 *
 *   agent_cch_image_part_chars              <- _image_part_chars
 *   agent_cch_payload_chars                 <- _payload_chars
 *   agent_cch_scale_stale_timeout_for_context <- _scale_stale_timeout_for_context
 *   agent_cch_bound_codex_stale_timeout     <- _bound_openai_codex_stale_timeout
 *
 * Kept in their own translation unit rather than appended to
 * src/agent/chat_completion_helpers.c: that object pulls in the whole agent
 * runtime (llm_client, auth store, message), which makes a minimal oracle
 * harness un-linkable. These four depend on nothing but libjson.
 */

#include "port_chat_completion_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: agent_cch_image_part_chars @ agent/chat_completion_helpers.py:_image_part_chars
 *      return image_cost * 4 + (len(text) if isinstance(text, str) else 0)
 *
 * The per-image price learned from provider usage, never the base64 length:
 * a native screenshot priced as text read as ~100K+ tokens and selected the
 * giant-conversation watchdog tiers.
 */
int agent_cch_image_part_chars(const json_t *part, int image_cost)
{
    if (!part || part->type != JSON_OBJECT)
        return image_cost * 4;
    const json_t *text = json_obj_get(part, "text");
    int n = (text && text->type == JSON_STRING && text->str_val)
                ? (int)strlen(text->str_val) : 0;
    return image_cost * 4 + n;
}

static const char *const CCH_IMAGE_PART_TYPES[] = {
    "image_url", "input_image", "image", NULL
};

/* PoP: agent_cch_payload_chars @ agent/chat_completion_helpers.py:_payload_chars */
int agent_cch_payload_chars(const json_t *value, int image_cost)
{
    if (!value || value->type == JSON_NULL)
        return 0;
    if (value->type == JSON_OBJECT) {
        const json_t *part_type = json_obj_get(value, "type");
        if (part_type && part_type->type == JSON_STRING && part_type->str_val) {
            int is_image = 0;
            for (int i = 0; CCH_IMAGE_PART_TYPES[i]; i++)
                if (strcmp(part_type->str_val, CCH_IMAGE_PART_TYPES[i]) == 0)
                    is_image = 1;
            /* Only scalar content-part types can match; a JSON-Schema node may
             * hold a sub-schema or a multi-type list under "type". */
            if (is_image
                && (json_obj_get(value, "image_url") || json_obj_get(value, "image")
                    || json_obj_get(value, "source") || json_obj_get(value, "file_id")))
                return agent_cch_image_part_chars(value, image_cost);
        }
        int total = 0;
        for (size_t i = 0; i < value->c.count; i++) {
            const char *k = value->c.keys[i];
            total += (int)strlen(k ? k : "") + 6
                     + agent_cch_payload_chars(value->c.items[i], image_cost);
        }
        return total;
    }
    if (value->type == JSON_ARRAY) {
        int total = 2 * (int)value->c.count;
        for (size_t i = 0; i < value->c.count; i++)
            total += agent_cch_payload_chars(value->c.items[i], image_cost);
        return total;
    }
    if (value->type == JSON_STRING)
        return value->str_val ? (int)strlen(value->str_val) : 0;
    if (value->type == JSON_BOOL)
        return value->bool_val ? 4 : 5;      /* len(str(True)) / len(str(False)) */
    if (value->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.17g", value->num_val);
        return (int)strlen(buf);
    }
    return 0;
}

/* PoP: agent_cch_scale_stale_timeout_for_context @ agent/chat_completion_helpers.py:_scale_stale_timeout_for_context */
double agent_cch_scale_stale_timeout_for_context(double base, int est_tokens)
{
    if (est_tokens > 100000)
        return base > 300.0 ? base : 300.0;
    if (est_tokens > 50000)
        return base > 240.0 ? base : 240.0;
    return base;
}

/* PoP: agent_cch_bound_codex_stale_timeout @ agent/chat_completion_helpers.py:_bound_openai_codex_stale_timeout */
double agent_cch_bound_codex_stale_timeout(double stale_timeout, int est_tokens,
                                           double hard)
{
    double floor = 0.0;
    if (est_tokens > 100000)
        floor = 1200.0;
    else if (est_tokens > 50000)
        floor = 900.0;
    else if (est_tokens > 10000)
        floor = 600.0;
    if (floor > 0.0 && stale_timeout < floor)
        stale_timeout = floor;
    return (hard > 0.0 && stale_timeout > hard) ? hard : stale_timeout;
}

/* ===========================================================================
 *  _bedrock_reasoning_stale_floor  (chat_completion_helpers.py:703)
 *
 *      name = model_id.strip().lower()
 *      for prefix in REGION_PREFIXES: strip the first one that matches
 *      base = [name]
 *      if "." in name: base += [name.rsplit(".",1)[1], name.replace(".","-",1)]
 *      candidates = dict.fromkeys(form for c in base for form in
 *          (c, dash-between-digits -> ".", dot-between-digits -> "-"))
 *      return first non-None floor, else None
 *
 *  Returns -1.0 for None. The floor table lives in reasoning_timeouts.c.
 * =========================================================================== */

/* PoP: agent_cch_bedrock_reasoning_stale_floor @ agent/chat_completion_helpers.py:_bedrock_reasoning_stale_floor */
double agent_reasoning_timeouts_get_floor(const char *model);

#define CCH_BEDROCK_NONE (-1.0)

/* Replace the first separator between digits with `to`. */
static void fold_one_separator(const char *in, char out, char *buf, size_t cap)
{
    size_t k = 0;
    for (size_t i = 0; in[i] && k + 1 < cap; i++) {
        if ((in[i] == '-' || in[i] == '.')
            && i > 0 && in[i - 1] >= '0' && in[i - 1] <= '9'
            && in[i + 1] >= '0' && in[i + 1] <= '9') {
            buf[k++] = out;
        } else {
            buf[k++] = in[i];
        }
    }
    buf[k] = '\0';
}

double agent_cch_bedrock_reasoning_stale_floor(const char *model_id)
{
    static const char *const REGION_PREFIXES[] = {
        "global.", "us.", "eu.", "apac.", "ap.", "au.", "jp.", "ca.", "sa.",
        "me.", "af.", NULL
    };
    if (!model_id || !*model_id)
        return CCH_BEDROCK_NONE;

    char name[512];
    size_t n = strlen(model_id);
    if (n >= sizeof(name))
        n = sizeof(name) - 1;
    for (size_t i = 0; i < n; i++)
        name[i] = (char)tolower((unsigned char)model_id[i]);
    name[n] = '\0';
    {
        size_t s = 0, e = n;
        while (s < e && (name[s] == ' ' || name[s] == '\t' || name[s] == '\n'
                         || name[s] == '\r'))
            s++;
        while (e > s && (name[e - 1] == ' ' || name[e - 1] == '\t'
                         || name[e - 1] == '\n' || name[e - 1] == '\r'))
            e--;
        if (s > 0 || e < n)
            memmove(name, name + s, e - s);
        name[e - s] = '\0';
    }
    if (!name[0])
        return CCH_BEDROCK_NONE;

    for (int i = 0; REGION_PREFIXES[i]; i++) {
        size_t pl = strlen(REGION_PREFIXES[i]);
        if (strncmp(name, REGION_PREFIXES[i], pl) == 0) {
            memmove(name, name + pl, strlen(name + pl) + 1);
            break;
        }
    }

    /* Dedup-preserving-order candidate list, exactly dict.fromkeys(). */
    char cands[6][512];
    int nc = 0;
    char base[3][512];
    int nb = 0;
    snprintf(base[nb], sizeof(base[nb]), "%s", name);
    nb++;
    if (strchr(name, '.')) {
        const char *dot = strrchr(name, '.');
        snprintf(base[nb], sizeof(base[nb]), "%s", dot + 1);
        nb++;
        /* first dot -> dash */
        size_t k = 0;
        int done = 0;
        for (size_t i = 0; name[i] && k + 1 < sizeof(base[nb]); i++) {
            base[nb][k++] = (!done && name[i] == '.') ? '-' : name[i];
            if (name[i] == '.')
                done = 1;
        }
        base[nb][k] = '\0';
        nb++;
    }

    for (int b = 0; b < nb; b++) {
        char folded_dot[512], folded_dash[512];
        fold_one_separator(base[b], '.', folded_dot, sizeof(folded_dot));
        fold_one_separator(base[b], '-', folded_dash, sizeof(folded_dash));
        const char *forms[3] = { base[b], folded_dot, folded_dash };
        for (int f = 0; f < 3; f++) {
            int seen = 0;
            for (int j = 0; j < nc; j++)
                if (strcmp(cands[j], forms[f]) == 0)
                    seen = 1;
            if (!seen && nc < 6)
                snprintf(cands[nc++], sizeof(cands[0]), "%s", forms[f]);
        }
    }

    for (int i = 0; i < nc; i++) {
        double f = agent_reasoning_timeouts_get_floor(cands[i]);
        if (f >= 0.0)
            return f;
    }
    return CCH_BEDROCK_NONE;
}
