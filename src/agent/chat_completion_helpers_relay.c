/*
 * chat_completion_helpers_relay.c -- real C11 port of Relay's chat-completions
 * stream accumulator, upstream Hermes agent/chat_completion_helpers_relay.py.
 *
 * PoP: w20_chatcomple_449db3_observe @ agent/chat_completion_helpers_relay.py:RelayChatAccumulator.observe
 * PoP: w20_chatcomple_449db3_finalize @ agent/chat_completion_helpers_relay.py:RelayChatAccumulator.finalize
 * PoP: w20_chatcomple_449db3_flatten_text @ session_rules/turns.py:flatten_message_text
 * PoP: w20_chatcomple_449db3_separate_reasoning @ agent/reasoning_summaries.py:separate_glued_reasoning_blocks
 * PoP: w20_chatcomple_449db3_run @ agent/chat_completion_helpers_tool_calls.py:_ToolCallAccumulator.feed
 *
 * The finalizer builds the recorded response from collector-observed state
 * only, which is exactly what this value does: no consumer closures, no live
 * stream objects. Tool-call deltas are assembled with the sibling module's
 * documented slot rules so one wire shape can be rebuilt here.
 */
#include "noc_w20_chatcomple_449db3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RLY_MAX_SLOTS 64

/* --- allocation helpers --------------------------------------------------- */

static char *rly_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static void rly_free(void *p) { free(p); }

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} rly_parts;

static void rly_parts_push(rly_parts *p, const char *s) {
    if (p->count == p->cap) {
        size_t cap = p->cap ? p->cap * 2 : 4;
        char **grown = (char **)realloc(p->items, cap * sizeof(char *));
        if (!grown) return;
        p->items = grown;
        p->cap = cap;
    }
    p->items[p->count++] = rly_dup(s);
}

static char *rly_parts_join(const rly_parts *p) {
    size_t total = 1, i, at = 0;
    char *out;
    for (i = 0; i < p->count; i++) total += strlen(p->items[i]);
    out = (char *)malloc(total);
    if (!out) return NULL;
    for (i = 0; i < p->count; i++) {
        size_t n = strlen(p->items[i]);
        memcpy(out + at, p->items[i], n);
        at += n;
    }
    out[at] = '\0';
    return out;
}

static void rly_parts_free(rly_parts *p) {
    size_t i;
    for (i = 0; i < p->count; i++) rly_free(p->items[i]);
    rly_free(p->items);
    p->items = NULL;
    p->count = p->cap = 0;
}

/* Python truthiness; absent (NULL) reads as None. */
static int rly_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static const json_t *rly_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

/* --- flatten_message_text / _text_from_part ------------------------------- */

static int rly_non_text_type(const char *type) {
    static const char *const NON_TEXT[] = {"image", "image_url", "input_image", "audio",
                                           "input_audio", NULL};
    int i;
    if (!type) return 0;
    for (i = 0; NON_TEXT[i]; i++) {
        if (!strcmp(type, NON_TEXT[i])) return 1;
    }
    return 0;
}

static void rly_lower_trim(char *dst, size_t cap, const char *src) {
    size_t at = 0;
    const char *p = src ? src : "";
    while (*p && at + 1 < cap) dst[at++] = (char)((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p), p++;
    dst[at] = '\0';
}

static char *rly_text_from_part(const json_t *part) {
    static const char *const TEXT_KEYS[] = {"text", "content", "input_text", "output_text",
                                            "summary_text", NULL};
    char type[64];
    int i;
    if (!part) return rly_dup("");
    if (part->type == JSON_STRING) return rly_dup(part->str_val);
    rly_lower_trim(type, sizeof(type), json_get_str(part, "type", ""));
    if (rly_non_text_type(type)) return rly_dup("");
    for (i = 0; TEXT_KEYS[i]; i++) {
        const json_t *v = rly_get(part, TEXT_KEYS[i]);
        if (v && v->type == JSON_STRING) return rly_dup(v->str_val);
    }
    return rly_dup("");
}

char *w20_chatcomple_449db3_flatten_text(const json_t *content) {
    rly_parts parts = {NULL, 0, 0};
    char *out;
    size_t i;
    if (!content) return rly_dup("");
    if (content->type == JSON_STRING) return rly_dup(content->str_val);
    if (content->type == JSON_ARRAY) {
        for (i = 0; i < content->c.count; i++) {
            char *chunk = rly_text_from_part(content->c.items[i]);
            if (chunk && chunk[0]) rly_parts_push(&parts, chunk);
            rly_free(chunk);
        }
        out = rly_parts_join(&parts);
        rly_parts_free(&parts);
        return out;
    }
    {
        char *text = rly_text_from_part(content);
        if (text && text[0]) return text;
        rly_free(text);
    }
    return rly_dup("");   /* non-string, no text part: str(content) is not a wire shape we see */
}

/* --- separate_glued_reasoning_blocks -------------------------------------- */

static int rly_py_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

char *w20_chatcomple_449db3_separate_reasoning(const char *previous, const json_t *delta_node) {
    char *delta = w20_chatcomple_449db3_flatten_text(delta_node);
    char *out;
    size_t n, i;
    int glued = 0, closed_bold = 0;
    if (!delta) return NULL;
    n = strlen(delta);
    if (previous && previous[0] && n) {
        size_t plen = strlen(previous);
        if (!rly_py_isspace(previous[plen - 1]) && n > 2 && delta[0] == '*' && delta[1] == '*') {
            for (i = 2; i + 1 < n; i++) {
                if (delta[i] == '*' && delta[i + 1] == '*') { closed_bold = 1; break; }
            }
            glued = closed_bold;
        }
    }
    if (!glued) return delta;
    out = (char *)malloc(n + 3);
    if (!out) { rly_free(delta); return NULL; }
    out[0] = '\n';
    out[1] = '\n';
    memcpy(out + 2, delta, n + 1);
    rly_free(delta);
    return out;
}

/* --- tool-call assembly --------------------------------------------------- */

typedef struct {
    int used;
    char *id;
    char *name;
    char *thought_signature;
    json_t *extra;          /* owned copy or NULL (wire None) */
    rly_parts args;
    size_t scanned;
    int depth;
    int in_string;
    int escaped;
    int started;
} rly_slot;

struct w20_chatcomple_449db3_acc {
    rly_parts content;
    rly_parts reasoning;
    rly_parts refusal;
    json_t *details;        /* array, owned */
    rly_slot slots[RLY_MAX_SLOTS];
    int slot_count;         /* highest slot index + 1 */
    int active_slot[RLY_MAX_SLOTS];   /* raw index -> slot, -1 unset */
    char *last_id[RLY_MAX_SLOTS];     /* raw index -> last non-empty id */
    json_t *model;          /* owned or NULL */
    json_t *usage;          /* owned or NULL */
    json_t *finish_reason;  /* owned or NULL */
    json_t *role;           /* owned, defaults to "assistant" */
};

w20_chatcomple_449db3_acc *w20_chatcomple_449db3_new(void) {
    w20_chatcomple_449db3_acc *acc = (w20_chatcomple_449db3_acc *)calloc(1, sizeof(*acc));
    int i;
    if (!acc) return NULL;
    acc->details = json_array();
    acc->role = json_string("assistant");
    for (i = 0; i < RLY_MAX_SLOTS; i++) acc->active_slot[i] = -1;
    return acc;
}

void w20_chatcomple_449db3_free(w20_chatcomple_449db3_acc *acc) {
    int i;
    if (!acc) return;
    rly_parts_free(&acc->content);
    rly_parts_free(&acc->reasoning);
    rly_parts_free(&acc->refusal);
    json_free(acc->details);
    json_free(acc->model);
    json_free(acc->usage);
    json_free(acc->finish_reason);
    json_free(acc->role);
    for (i = 0; i < RLY_MAX_SLOTS; i++) {
        rly_free(acc->slots[i].id);
        rly_free(acc->slots[i].name);
        rly_free(acc->slots[i].thought_signature);
        json_free(acc->slots[i].extra);
        rly_parts_free(&acc->slots[i].args);
        rly_free(acc->last_id[i]);
    }
    free(acc);
}

/* _arguments_closed(slot): brackets balanced outside strings, scanned once. */
static int rly_arguments_closed(rly_slot *slot) {
    size_t i;
    for (i = slot->scanned; i < slot->args.count; i++) {
        const char *ch = slot->args.items[i];
        while (*ch) {
            char c = *ch++;
            if (slot->in_string) {
                if (slot->escaped) slot->escaped = 0;
                else if (c == '\\') slot->escaped = 1;
                else if (c == '"') slot->in_string = 0;
            } else if (c == '"') {
                slot->in_string = 1;
            } else if (c == '{' || c == '[') {
                slot->depth += 1;
                slot->started = 1;
            } else if (c == '}' || c == ']') {
                slot->depth -= 1;
            }
        }
    }
    slot->scanned = slot->args.count;
    return slot->started && slot->depth == 0 && !slot->in_string;
}

static rly_slot *rly_slot_at(w20_chatcomple_449db3_acc *acc, int slot) {
    if (slot < 0 || slot >= RLY_MAX_SLOTS) return NULL;
    if (!acc->slots[slot].used) {
        acc->slots[slot].used = 1;
        acc->slots[slot].id = rly_dup("");
        acc->slots[slot].name = rly_dup("");
        if (slot + 1 > acc->slot_count) acc->slot_count = slot + 1;
    }
    return &acc->slots[slot];
}

/* _starts_new_call(tc_delta, slot) */
static int rly_starts_new_call(w20_chatcomple_449db3_acc *acc, const json_t *tc, int slot) {
    const json_t *function = rly_get(tc, "function");
    const char *name = json_get_str(function, "name", NULL);
    rly_slot *cur;
    if (!name || !name[0]) return 0;
    cur = rly_slot_at(acc, slot);
    if (!cur) return 0;
    if (strcmp(name, cur->name)) return 1;
    return rly_arguments_closed(cur);
}

static void rly_feed_tool_call(w20_chatcomple_449db3_acc *acc, const json_t *tc) {
    const json_t *index = rly_get(tc, "index");
    int raw_idx = 0, slot;
    const char *id;
    char idbuf[64];
    rly_slot *entry;
    const json_t *function, *extra;

    if (index && index->type == JSON_NUMBER) raw_idx = (int)index->num_val;
    if (raw_idx < 0) raw_idx = 0;
    if (raw_idx >= RLY_MAX_SLOTS) return;

    id = json_get_str(tc, "id", NULL);
    if (id && !id[0]) id = NULL;                     /* delta_id = tc_id or "" */
    if (id && acc->last_id[raw_idx] && strcmp(id, acc->last_id[raw_idx]) &&
        rly_starts_new_call(acc, tc, acc->active_slot[raw_idx] >= 0 ? acc->active_slot[raw_idx] : raw_idx)) {
        acc->active_slot[raw_idx] = acc->slot_count;  /* max(acc) + 1 */
    }
    if (id) {
        rly_free(acc->last_id[raw_idx]);
        acc->last_id[raw_idx] = rly_dup(id);
    }
    if (acc->active_slot[raw_idx] < 0) acc->active_slot[raw_idx] = raw_idx;
    slot = acc->active_slot[raw_idx];
    entry = rly_slot_at(acc, slot);
    if (!entry) return;

    if (id) {
        rly_free(entry->id);
        entry->id = rly_dup(id);
    }
    function = rly_get(tc, "function");
    if (function && rly_truthy(function)) {
        const char *name = json_get_str(function, "name", NULL);
        const char *args = json_get_str(function, "arguments", NULL);
        const json_t *sig = rly_get(function, "thought_signature");
        if (name && name[0]) {
            /* Assignment, not append: some providers resend the whole name. */
            rly_free(entry->name);
            entry->name = rly_dup(name);
        }
        if (args) rly_parts_push(&entry->args, args);
        if (sig && sig->type == JSON_STRING && rly_truthy(sig)) {
            rly_free(entry->thought_signature);
            entry->thought_signature = rly_dup(sig->str_val);
        }
    }
    extra = rly_get(tc, "extra_content");
    if (extra && extra->type != JSON_NULL) {
        json_free(entry->extra);
        entry->extra = json_copy(extra);
    }
    (void)idbuf;
}

/* --- observe -------------------------------------------------------------- */

void w20_chatcomple_449db3_observe(w20_chatcomple_449db3_acc *acc, const json_t *chunk) {
    const json_t *model, *usage, *choices, *choice, *finish, *delta;
    char *text;

    if (!acc || !chunk || chunk->type != JSON_OBJECT) return;

    model = rly_get(chunk, "model");
    if (rly_truthy(model)) {
        json_free(acc->model);
        acc->model = json_copy(model);
    }
    usage = rly_get(chunk, "usage");
    if (rly_truthy(usage)) {
        json_free(acc->usage);
        acc->usage = json_copy(usage);
    }
    choices = rly_get(chunk, "choices");
    choice = (choices && choices->type == JSON_ARRAY && choices->c.count) ? choices->c.items[0] : NULL;
    if (!choice || choice->type != JSON_OBJECT) return;

    finish = rly_get(choice, "finish_reason");
    if (rly_truthy(finish)) {
        json_free(acc->finish_reason);
        acc->finish_reason = json_copy(finish);
    }
    delta = rly_get(choice, "delta");
    if (!delta || delta->type != JSON_OBJECT) return;

    if (rly_truthy(rly_get(delta, "role"))) {
        json_free(acc->role);
        acc->role = json_copy(rly_get(delta, "role"));
    }
    text = w20_chatcomple_449db3_flatten_text(rly_get(delta, "content"));
    if (text && text[0]) rly_parts_push(&acc->content, text);
    rly_free(text);

    {
        const json_t *details = rly_get(delta, "reasoning_details");
        if (details && details->type == JSON_ARRAY) {
            size_t i;
            for (i = 0; i < details->c.count; i++) {
                json_append(acc->details, json_copy(details->c.items[i]));
            }
        }
    }

    {
        const json_t *reasoning = rly_get(delta, "reasoning_content");
        if (!rly_truthy(reasoning)) reasoning = rly_get(delta, "reasoning");
        if (rly_truthy(reasoning)) {
            char *previous = acc->reasoning.count ? rly_dup(acc->reasoning.items[acc->reasoning.count - 1])
                                                  : rly_dup("");
            char *block = w20_chatcomple_449db3_separate_reasoning(previous, reasoning);
            if (block) rly_parts_push(&acc->reasoning, block);
            rly_free(previous);
            rly_free(block);
        }
    }

    {
        const json_t *refusal = rly_get(delta, "refusal");
        if (refusal && refusal->type == JSON_STRING && refusal->str_val && refusal->str_val[0]) {
            rly_parts_push(&acc->refusal, refusal->str_val);
        }
    }

    {
        const json_t *tool_calls = rly_get(delta, "tool_calls");
        size_t i;
        if (tool_calls && tool_calls->type == JSON_ARRAY) {
            for (i = 0; i < tool_calls->c.count; i++) {
                const json_t *tc = tool_calls->c.items[i];
                if (tc && tc->type == JSON_OBJECT) rly_feed_tool_call(acc, tc);
            }
        }
    }
}

/* --- finalize ------------------------------------------------------------- */

static json_t *rly_joined_or_null(rly_parts *p) {
    char *joined;
    json_t *out;
    if (!p->count) return json_null();
    joined = rly_parts_join(p);
    if (!joined || !joined[0]) { rly_free(joined); return json_null(); }
    out = json_string(joined);
    rly_free(joined);
    return out;
}

json_t *w20_chatcomple_449db3_finalize(const w20_chatcomple_449db3_acc *acc) {
    json_t *message = json_object(), *tool_calls = NULL, *choices, *choice, *out;
    int i;

    json_set(message, "role", json_copy(acc->role));
    json_set(message, "content", rly_joined_or_null((rly_parts *)&acc->content));
    json_set(message, "reasoning_content", rly_joined_or_null((rly_parts *)&acc->reasoning));
    json_set(message, "refusal", rly_joined_or_null((rly_parts *)&acc->refusal));
    json_set(message, "reasoning_details",
             acc->details->c.count ? json_copy(acc->details) : json_null());

    for (i = 0; i < acc->slot_count; i++) {
        const rly_slot *slot = &acc->slots[i];
        json_t *entry, *function;
        char *joined;
        if (!slot->used) continue;
        if (!tool_calls) tool_calls = json_array();
        entry = json_object();
        json_set(entry, "id", json_string(slot->id ? slot->id : ""));
        json_set(entry, "type", json_string("function"));
        function = json_object();
        json_set(function, "name", json_string(slot->name ? slot->name : ""));
        joined = rly_parts_join(&slot->args);
        json_set(function, "arguments", json_string(joined ? joined : ""));
        rly_free(joined);
        json_set(entry, "function", function);
        json_set(entry, "extra_content", slot->extra ? json_copy(slot->extra) : json_null());
        if (slot->thought_signature) {
            json_set(entry, "thought_signature", json_string(slot->thought_signature));
        }
        json_append(tool_calls, entry);
    }
    json_set(message, "tool_calls", tool_calls ? tool_calls : json_null());

    choice = json_object();
    json_set(choice, "message", message);
    json_set(choice, "finish_reason",
             rly_truthy(acc->finish_reason) ? json_copy(acc->finish_reason) : json_string("stop"));
    choices = json_array();
    json_append(choices, choice);

    out = json_object();
    json_set(out, "model", acc->model ? json_copy(acc->model) : json_null());
    json_set(out, "usage", acc->usage ? json_copy(acc->usage) : json_null());
    json_set(out, "choices", choices);
    return out;
}

json_t *w20_chatcomple_449db3_run(const json_t *chunks) {
    w20_chatcomple_449db3_acc *acc = w20_chatcomple_449db3_new();
    json_t *out;
    size_t i;
    if (!acc) return NULL;
    if (chunks && chunks->type == JSON_ARRAY) {
        for (i = 0; i < chunks->c.count; i++) {
            w20_chatcomple_449db3_observe(acc, chunks->c.items[i]);
        }
    }
    out = w20_chatcomple_449db3_finalize(acc);
    w20_chatcomple_449db3_free(acc);
    return out;
}
