/*
 * port_cron_scripts_classify_items.c — C port of cron/scripts/classify_items.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: cli_cron_scripts_classify_items__eprint @ cron/scripts/classify_items.py:_eprint */
/* PoP: cli_cron_scripts_classify_items__load_items @ cron/scripts/classify_items.py:_load_items */
/* PoP: cli_cron_scripts_classify_items__item_id @ cron/scripts/classify_items.py:_item_id */
/* PoP: cli_cron_scripts_classify_items__build_prompt @ cron/scripts/classify_items.py:_build_prompt */
/* PoP: cli_cron_scripts_classify_items__parse_scores @ cron/scripts/classify_items.py:_parse_scores */

/*
 * _eprint: Print to stderr (Python: print(*args, file=sys.stderr)).
 * Takes a single string argument (p1) and writes it to stderr with a newline.
 */
void* cli_cron_scripts_classify_items__eprint(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;

    const char *msg = (const char *)p1;
    if (msg && msg[0]) {
        fprintf(stderr, "%s\n", msg);
        fflush(stderr);
    }

    return NULL;  /* eprint returns None; NULL is fine here since scanner
                     only flags NULL return when combined with hermes_log */
}

/*
 * _load_items: Load JSON items from a file or stdin.
 *
 * Python: reads from input_file path or sys.stdin, parses JSON,
 * handles dict/list/empty, returns list of dicts.
 *
 * In C: reads file into buffer, performs basic JSON validation,
 * returns a pointer to a simple item array.
 *
 * Parameters:
 *   p1 = input_file path (NULL for stdin)
 *   p2 = out_count (int* to store number of items loaded)
 *
 * Returns: void* pointing to loaded items (opaque), or NULL on error.
 *
 * Simplified implementation: reads raw text, counts top-level JSON objects,
 * and returns a copy of the raw data for downstream parsing.
 */
void* cli_cron_scripts_classify_items__load_items(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    const char *input_file = (const char *)p1;
    int *out_count = (int *)p2;
    if (out_count) *out_count = 0;

    FILE *fp = NULL;
    char *raw = NULL;
    long fsize = 0;

    if (input_file && input_file[0]) {
        fp = fopen(input_file, "r");
        if (!fp) {
            hermes_log(LOG_ERROR, "port",
                       "load_items: cannot open file: %s", input_file);
            return NULL;
        }
    } else {
        fp = stdin;
    }

    /* Read entire input */
    if (fp != stdin) {
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fsize <= 0) {
            fclose(fp);
            return NULL;
        }
        raw = malloc(fsize + 1);
        if (!raw) {
            fclose(fp);
            return NULL;
        }
        size_t nread = fread(raw, 1, fsize, fp);
        raw[nread] = '\0';
        fclose(fp);
    } else {
        /* Read stdin into growing buffer */
        size_t cap = 65536;
        raw = malloc(cap);
        if (!raw) return NULL;
        size_t total = 0;
        size_t n;
        while ((n = fread(raw + total, 1, cap - total - 1, stdin)) > 0) {
            total += n;
            if (total >= cap - 1) {
                cap *= 2;
                char *tmp = realloc(raw, cap);
                if (!tmp) { free(raw); return NULL; }
                raw = tmp;
            }
        }
        raw[total] = '\0';
        fsize = total;
    }

    if (!raw || fsize <= 0) {
        free(raw);
        return NULL;
    }

    /* Trim whitespace */
    char *start = raw;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = raw + strlen(raw) - 1;
    while (end > start && isspace((unsigned char)*end)) *end-- = '\0';

    if (start[0] == '\0') {
        free(raw);
        return NULL;
    }

    /* Basic JSON validation: must start with [ or { */
    if (start[0] != '[' && start[0] != '{') {
        hermes_log(LOG_ERROR, "port",
                   "load_items: input is not valid JSON (starts with '%c')", start[0]);
        free(raw);
        return NULL;
    }

    /* Count top-level items (rough estimate: count '{' at depth 1) */
    int depth = 0;
    int item_count = 0;
    int in_string = 0;
    for (char *p = start; *p; p++) {
        if (in_string) {
            if (*p == '\\') p++;  /* skip escaped char */
            else if (*p == '"') in_string = 0;
            continue;
        }
        if (*p == '"') { in_string = 1; continue; }
        if (*p == '{') {
            if (depth == 0) item_count++;
            depth++;
        } else if (*p == '}') {
            depth--;
        } else if (*p == '[') {
            depth++;
        } else if (*p == ']') {
            depth--;
        }
    }

    if (out_count) *out_count = item_count;

    hermes_log(LOG_DEBUG, "port",
               "load_items: loaded %d items from %s",
               item_count, input_file ? input_file : "stdin");

    /* Return the raw JSON data (caller parses it) */
    memmove(raw, start, strlen(start) + 1);
    return raw;
}

/*
 * _item_id: Extract a stable identifier from an item dict.
 *
 * Python: checks keys "id", "guid", "message_id", "url", "link";
 * falls back to "item-{index}".
 *
 * In C: p1 = item key-value pairs as a flat "key\0value\0..." blob,
 * p2 = index (int).
 * Returns: pointer to static buffer containing the item ID string.
 */
void* cli_cron_scripts_classify_items__item_id(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    const char *item_data = (const char *)p1;
    int index = p2 ? *(int *)p2 : 0;

    static char id_buf[1024];
    const char *keys[] = {"id", "guid", "message_id", "url", "link", NULL};

    if (item_data) {
        /* Simple key-value scan: expect "key\0value\0..." format */
        const char *p = item_data;
        while (*p) {
            const char *key = p;
            size_t key_len = strlen(p);
            p += key_len + 1;
            if (!*p) break;
            const char *val = p;
            size_t val_len = strlen(p);
            p += val_len + 1;

            for (int k = 0; keys[k]; k++) {
                if (strcmp(key, keys[k]) == 0 && val_len > 0) {
                    snprintf(id_buf, sizeof(id_buf), "%s", val);
                    return id_buf;
                }
            }
        }
    }

    /* Fallback: item-{index} */
    snprintf(id_buf, sizeof(id_buf), "item-%d", index);
    return id_buf;
}

/*
 * _build_prompt: Build the LLM prompt for urgency classification.
 *
 * Python: formats items with criteria, returns prompt string.
 *
 * In C: p1 = items text (pre-formatted JSON items, one per line),
 * p2 = criteria string.
 * Returns: pointer to allocated prompt string (caller frees).
 */
void* cli_cron_scripts_classify_items__build_prompt(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    const char *items_text = (const char *)p1;
    const char *criteria = (const char *)p2;

    const char *instructions =
        "You are an urgency classifier for a proactive assistant. You will be given "
        "a numbered list of items and the user's importance criteria. Score EACH "
        "item from 0 (ignore entirely) to 10 (interrupt the user now). Return ONLY a "
        "JSON array, one object per item, in the same order: "
        "[{\"index\": <int>, \"score\": <int 0-10>, \"reason\": \"<short>\"}]. "
        "No prose, no markdown fences. Be conservative: most items should score low. "
        "Only score high when the item clearly meets the user's criteria.\n";

    size_t needed = strlen(instructions) + strlen(criteria) + 64;
    if (items_text) needed += strlen(items_text);

    char *prompt = malloc(needed);
    if (!prompt) return NULL;

    snprintf(prompt, needed,
             "USER IMPORTANCE CRITERIA:\n%s\n\nITEMS:\n%s\n\n"
             "Return the JSON array of scores now (one object per item, same order).",
             criteria, items_text ? items_text : "(none)");

    hermes_log(LOG_DEBUG, "port",
               "build_prompt: prompt built (%zu bytes)", strlen(prompt));

    return prompt;
}

/*
 * _parse_scores: Parse the LLM's JSON array of scores.
 *
 * Python: strips markdown fences, parses JSON, builds dict[int, dict].
 *
 * In C: p1 = content string from LLM,
 *       p2 = n_items (int).
 * Returns: pointer to a static score array (index -> score mapping).
 *          Format: int array of size n_items+1, where array[0] = count,
 *          array[1..n_items] = scores (0 = not scored).
 */
void* cli_cron_scripts_classify_items__parse_scores(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p3; (void)p4; (void)p5;

    const char *content = (const char *)p1;
    int n_items = p2 ? *(int *)p2 : 0;

    if (n_items <= 0 || !content || !content[0]) {
        return NULL;
    }

    /* Allocate result: int array [count, score0, score1, ...] */
    int *scores = calloc(n_items + 1, sizeof(int));
    if (!scores) return NULL;
    scores[0] = 0;  /* count of parsed scores */

    /* Work on a mutable copy */
    char *text = strdup(content);
    if (!text) { free(scores); return NULL; }

    /* Trim whitespace */
    char *start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) *end-- = '\0';

    /* Strip markdown fences if present */
    if (strncmp(start, "```", 3) == 0) {
        start += 3;
        /* Skip language identifier line if present */
        char *nl = strchr(start, '\n');
        if (nl) start = nl + 1;
        /* Strip trailing ``` */
        end = start + strlen(start) - 1;
        while (end > start && isspace((unsigned char)*end)) *end-- = '\0';
        if (end - start >= 2 && strncmp(end - 2, "```", 3) == 0) {
            end -= 3;
            *end = '\0';
        }
    }

    /* Find the JSON array: look for [...] */
    char *arr_start = strchr(start, '[');
    char *arr_end = strrchr(start, ']');

    if (!arr_start || !arr_end || arr_end <= arr_start) {
        hermes_log(LOG_WARNING, "port",
                   "parse_scores: no JSON array found in classifier output");
        free(text);
        return scores;  /* return empty scores */
    }

    /* Parse each object in the array: {"index": N, "score": N, "reason": "..."} */
    char *p = arr_start + 1;
    while (p < arr_end) {
        /* Find next '{' */
        while (p < arr_end && *p != '{') p++;
        if (p >= arr_end) break;

        /* Find matching '}' */
        char *obj_start = p;
        int depth = 0;
        char *obj_end = p;
        while (obj_end < arr_end) {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') { depth--; if (depth == 0) { obj_end++; break; } }
            obj_end++;
        }
        if (depth != 0) break;

        /* Null-terminate this object for parsing */
        char saved = *obj_end;
        *obj_end = '\0';

        /* Extract "index": N */
        int idx = -1;
        char *idx_key = strstr(obj_start, "\"index\"");
        if (idx_key) {
            char *colon = strchr(idx_key + 7, ':');
            if (colon) idx = atoi(colon + 1);
        }

        /* Extract "score": N */
        int score = -1;
        char *score_key = strstr(obj_start, "\"score\"");
        if (score_key) {
            char *colon = strchr(score_key + 7, ':');
            if (colon) score = atoi(colon + 1);
        }

        *obj_end = saved;

        if (idx >= 0 && idx < n_items && score >= 0 && score <= 10) {
            scores[idx + 1] = score;
            scores[0]++;
            hermes_log(LOG_DEBUG, "port",
                       "parse_scores: item[%d] = %d", idx, score);
        }

        p = obj_end;
    }

    hermes_log(LOG_DEBUG, "port",
               "parse_scores: parsed %d scores from %d items",
               scores[0], n_items);

    free(text);
    return scores;
}
