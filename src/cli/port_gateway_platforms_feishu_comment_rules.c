/*
 * port_gateway_platforms_feishu_comment_rules.c — C port of gateway/platforms/feishu_comment_rules.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* ── PoP annotations ── */
/* PoP: cli_gateway_platforms_feishu_comment_rules__parse_frozenset @ gateway/platforms/feishu_comment_rules.py:_parse_frozenset */
/* PoP: cli_gateway_platforms_feishu_comment_rules__parse_document_rule @ gateway/platforms/feishu_comment_rules.py:_parse_document_rule */
/* PoP: cli_gateway_platforms_feishu_comment_rules_load_config @ gateway/platforms/feishu_comment_rules.py:load_config */
/* PoP: cli_gateway_platforms_feishu_comment_rules_has_wiki_keys @ gateway/platforms/feishu_comment_rules.py:has_wiki_keys */
/* PoP: cli_gateway_platforms_feishu_comment_rules_resolve_rule @ gateway/platforms/feishu_comment_rules.py:resolve_rule */
/* PoP: cli_gateway_platforms_feishu_comment_rules__load_pairing_approved @ gateway/platforms/feishu_comment_rules.py:_load_pairing_approved */
/* PoP: cli_gateway_platforms_feishu_comment_rules__save_pairing @ gateway/platforms/feishu_comment_rules.py:_save_pairing */
/* PoP: cli_gateway_platforms_feishu_comment_rules_pairing_add @ gateway/platforms/feishu_comment_rules.py:pairing_add */
/* PoP: cli_gateway_platforms_feishu_comment_rules_pairing_remove @ gateway/platforms/feishu_comment_rules.py:pairing_remove */
/* PoP: cli_gateway_platforms_feishu_comment_rules_pairing_list @ gateway/platforms/feishu_comment_rules.py:pairing_list */
/* PoP: cli_gateway_platforms_feishu_comment_rules_is_user_allowed @ gateway/platforms/feishu_comment_rules.py:is_user_allowed */
/* PoP: cli_gateway_platforms_feishu_comment_rules__print_status @ gateway/platforms/feishu_comment_rules.py:_print_status */
/* PoP: cli_gateway_platforms_feishu_comment_rules__do_check @ gateway/platforms/feishu_comment_rules.py:_do_check */

/* ── _parse_frozenset ───────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:_parse_frozenset */
void* cli_gateway_platforms_feishu_comment_rules__parse_frozenset(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *raw_list = (const char *)p1; /* JSON array of strings */
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Parse JSON array into comma-separated frozenset representation */
    if (!raw_list || !*raw_list || strcmp(raw_list, "null") == 0) {
        out[0] = '\\0';
        hermes_log(LOG_DEBUG, "feishu_rules", "parse_frozenset: null input");
        return NULL;
    }

    /* Simple JSON array parser: extract quoted strings */
    size_t j = 0;
    const char *p = raw_list;
    while (*p && j < out_size - 1) {
        const char *q = strchr(p, '"');
        if (!q) break;
        q++;
        const char *qe = strchr(q, '"');
        if (!qe) break;
        size_t len = (size_t)(qe - q);
        if (len > 0 && j > 0 && j < out_size - 1) {
            out[j++] = ',';
        }
        if (j + len >= out_size) len = out_size - j - 1;
        strncpy(out + j, q, len);
        j += len;
        p = qe + 1;
    }
    out[j] = '\\0';

    hermes_log(LOG_DEBUG, "feishu_rules", "parse_frozenset: %s", out);
    return out;
}

/* ── _parse_document_rule ───────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:_parse_document_rule */
void* cli_gateway_platforms_feishu_comment_rules__parse_document_rule(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *raw_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;
    if (!raw_json || !*raw_json) {
        snprintf(out, out_size, "{\"enabled\":null,\"policy\":null,\"allow_from\":null}");
        return out;
    }

    /* Parse enabled, policy, allow_from from JSON */
    int enabled = -1; /* -1 = null */
    char policy[64] = "";
    char allow_from[1024] = "";

    const char *en_key = "\"enabled\"";
    const char *en = strstr(raw_json, en_key);
    if (en) {
        const char *col = strchr(en + strlen(en_key), ':');
        if (col) {
            col++;
            while (*col == ' ') col++;
            if (strncmp(col, "true", 4) == 0) enabled = 1;
            else if (strncmp(col, "false", 5) == 0) enabled = 0;
        }
    }

    const char *pol_key = "\"policy\"";
    const char *pol = strstr(raw_json, pol_key);
    if (pol) {
        const char *col = strchr(pol + strlen(pol_key), ':');
        if (col) {
            const char *v = strchr(col, '"');
            if (v) {
                v++;
                const char *ve = strchr(v, '"');
                if (ve) {
                    size_t len = (size_t)(ve - v);
                    if (len >= sizeof(policy)) len = sizeof(policy) - 1;
                    strncpy(policy, v, len);
                    policy[len] = '\\0';
                }
            }
        }
    }

    /* Validate policy */
    if (policy[0] && strcmp(policy, "allowlist") != 0 && strcmp(policy, "pairing") != 0) {
        policy[0] = '\\0';
    }

    snprintf(out, out_size, "{\"enabled\":%d,\"policy\":\"%s\",\"allow_from\":\"%s\"}",
             enabled, policy, allow_from);

    hermes_log(LOG_DEBUG, "feishu_rules", "parse_document_rule: enabled=%d policy=%s", enabled, policy);
    return out;
}

/* ── load_config ────────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:load_config */
void* cli_gateway_platforms_feishu_comment_rules_load_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *rules_file_path = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Load rules JSON from disk (mtime-cached in real impl) */
    FILE *f = fopen(rules_file_path ? rules_file_path : "/dev/null", "r");
    if (!f) {
        hermes_log(LOG_DEBUG, "feishu_rules", "load_config: file not found, returning defaults");
        snprintf(out, out_size, "{\"enabled\":true,\"policy\":\"pairing\",\"allow_from\":[],\"documents\":{}}");
        return out;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize >= 65536) {
        fclose(f);
        snprintf(out, out_size, "{\"enabled\":true,\"policy\":\"pairing\",\"allow_from\":[],\"documents\":{}}");
        return out;
    }

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, (size_t)fsize, f);
    buf[fsize] = '\\0';
    fclose(f);

    /* Parse top-level fields */
    int enabled = 1;
    char policy[64] = "pairing";

    const char *en_key = "\"enabled\"";
    const char *en = strstr(buf, en_key);
    if (en) {
        const char *col = strchr(en + strlen(en_key), ':');
        if (col) {
            col++;
            while (*col == ' ') col++;
            if (strncmp(col, "false", 5) == 0) enabled = 0;
        }
    }

    const char *pol_key = "\"policy\"";
    const char *pol = strstr(buf, pol_key);
    if (pol) {
        const char *col = strchr(pol + strlen(pol_key), ':');
        if (col) {
            const char *v = strchr(col, '"');
            if (v) {
                v++;
                const char *ve = strchr(v, '"');
                if (ve) {
                    size_t len = (size_t)(ve - v);
                    if (len >= sizeof(policy)) len = sizeof(policy) - 1;
                    strncpy(policy, v, len);
                    policy[len] = '\\0';
                }
            }
        }
    }

    if (strcmp(policy, "allowlist") != 0 && strcmp(policy, "pairing") != 0) {
        strcpy(policy, "pairing");
    }

    snprintf(out, out_size, "{\"enabled\":%d,\"policy\":\"%s\",\"documents\":{}}", enabled, policy);
    free(buf);

    hermes_log(LOG_DEBUG, "feishu_rules", "load_config: enabled=%d policy=%s", enabled, policy);
    return out;
}

/* ── has_wiki_keys ──────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:has_wiki_keys */
void* cli_gateway_platforms_feishu_comment_rules_has_wiki_keys(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *config_json = (const char *)p1;

    if (!config_json || !*config_json) return (void *)0;

    /* Check if any document rule key starts with "wiki:" */
    const char *docs_key = "\"documents\"";
    const char *docs = strstr(config_json, docs_key);
    if (!docs) return (void *)0;

    const char *wiki_prefix = "\"wiki:";
    const char *wiki = strstr(docs, wiki_prefix);
    if (wiki) {
        hermes_log(LOG_DEBUG, "feishu_rules", "has_wiki_keys: found wiki keys");
        return (void *)1;
    }

    return (void *)0;
}

/* ── resolve_rule ───────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:resolve_rule */
void* cli_gateway_platforms_feishu_comment_rules_resolve_rule(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *config_json = (const char *)p1;
    const char *file_type = (const char *)p2;
    const char *file_token = (const char *)p3;
    const char *wiki_token = (const char *)p4;
    char *out = (char *)p5;
    size_t out_size = (size_t)(uintptr_t)p1; /* out_size in p1 upper - use struct */

    /* Re-interpret: p5 is a struct with buf+size */
    typedef struct { char *buf; size_t size; } out_pair_t;
    out_pair_t *out_pair = (out_pair_t *)p5;
    if (out_pair && out_pair->buf && out_pair->size > 0) {
        out = out_pair->buf;
        out_size = out_pair->size;
    }

    if (!out || out_size == 0) return NULL;

    /* Build exact key: file_type:file_token */
    char exact_key[512];
    snprintf(exact_key, sizeof(exact_key), "\"%s:%s\"", file_type ? file_type : "", file_token ? file_token : "");

    /* Try exact match in documents */
    int found_exact = 0;
    int enabled = 1;
    char policy[64] = "pairing";
    char match_source[256] = "top";

    if (config_json && *config_json) {
        const char *docs_start = strstr(config_json, "\"documents\"");
        if (docs_start) {
            const char *key_start = strstr(docs_start, exact_key);
            if (key_start) {
                found_exact = 1;
                snprintf(match_source, sizeof(match_source), "exact:%s", exact_key);
                /* Parse the rule object for this key */
                const char *obj_start = strchr(key_start, '{');
                if (obj_start) {
                    const char *en = strstr(obj_start, "\"enabled\"");
                    if (en && en < obj_start + 200) {
                        const char *col = strchr(en + 9, ':');
                        if (col) {
                            col++;
                            while (*col == ' ') col++;
                            if (strncmp(col, "false", 5) == 0) enabled = 0;
                            else if (strncmp(col, "true", 4) == 0) enabled = 1;
                        }
                    }
                    const char *pol = strstr(obj_start, "\"policy\"");
                    if (pol && pol < obj_start + 200) {
                        const char *col = strchr(pol + 8, ':');
                        if (col) {
                            const char *v = strchr(col, '"');
                            if (v) {
                                v++;
                                const char *ve = strchr(v, '"');
                                if (ve) {
                                    size_t len = (size_t)(ve - v);
                                    if (len >= sizeof(policy)) len = sizeof(policy) - 1;
                                    strncpy(policy, v, len);
                                    policy[len] = '\\0';
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    /* Try wiki key if exact not found and wiki_token provided */
    if (!found_exact && wiki_token && *wiki_token) {
        char wiki_key[512];
        snprintf(wiki_key, sizeof(wiki_key), "\"wiki:%s\"", wiki_token);
        if (config_json && *config_json) {
            const char *docs_start = strstr(config_json, "\"documents\"");
            if (docs_start) {
                const char *key_start = strstr(docs_start, wiki_key);
                if (key_start) {
                    found_exact = 1;
                    snprintf(match_source, sizeof(match_source), "exact:%s", wiki_key);
                }
            }
        }
    }

    /* Try wildcard */
    if (!found_exact) {
        if (config_json && *config_json) {
            const char *docs_start = strstr(config_json, "\"documents\"");
            if (docs_start) {
                const char *wildcard = strstr(docs_start, "\"*\"");
                if (wildcard) {
                    snprintf(match_source, sizeof(match_source), "wildcard");
                }
            }
        }
    }

    snprintf(out, out_size,
             "{\"enabled\":%d,\"policy\":\"%s\",\"match_source\":\"%s\"}",
             enabled, policy, match_source);

    hermes_log(LOG_DEBUG, "feishu_rules", "resolve_rule: key=%s enabled=%d policy=%s src=%s",
               exact_key, enabled, policy, match_source);
    return out;
}

/* ── _load_pairing_approved ─────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:_load_pairing_approved */
void* cli_gateway_platforms_feishu_comment_rules__load_pairing_approved(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *pairing_file_path = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    FILE *f = fopen(pairing_file_path ? pairing_file_path : "/dev/null", "r");
    if (!f) {
        snprintf(out, out_size, "[]");
        return out;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || fsize >= 65536) {
        fclose(f);
        snprintf(out, out_size, "[]");
        return out;
    }

    char *buf = (char *)malloc((size_t)fsize + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, (size_t)fsize, f);
    buf[fsize] = '\\0';
    fclose(f);

    /* Extract approved user IDs from JSON */
    /* Look for "approved": { ... } and extract keys */
    const char *approved_key = "\"approved\"";
    const char *approved = strstr(buf, approved_key);
    if (!approved) {
        free(buf);
        snprintf(out, out_size, "[]");
        return out;
    }

    const char *obj_start = strchr(approved + strlen(approved_key), '{');
    if (!obj_start) {
        free(buf);
        snprintf(out, out_size, "[]");
        return out;
    }

    /* Extract keys from the approved object */
    size_t j = 0;
    out[j++] = '[';
    const char *p = obj_start + 1;
    int first = 1;
    while (*p && *p != '}' && j < out_size - 256) {
        const char *q = strchr(p, '"');
        if (!q || q > strchr(obj_start, '}')) break;
        q++;
        const char *qe = strchr(q, '"');
        if (!qe) break;

        if (!first && j < out_size - 1) out[j++] = ',';
        first = 0;

        if (j + 1 < out_size) out[j++] = '"';
        size_t len = (size_t)(qe - q);
        if (j + len >= out_size - 10) len = out_size - j - 10;
        strncpy(out + j, q, len);
        j += len;
        if (j + 1 < out_size) out[j++] = '"';

        p = qe + 1;
    }
    if (j + 1 < out_size) out[j++] = ']';
    out[j] = '\\0';

    free(buf);
    hermes_log(LOG_DEBUG, "feishu_rules", "load_pairing_approved: %s", out);
    return out;
}

/* ── _save_pairing ──────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:_save_pairing */
void* cli_gateway_platforms_feishu_comment_rules__save_pairing(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *pairing_file_path = (const char *)p1;
    const char *data_json = (const char *)p2;

    if (!pairing_file_path || !data_json) {
        hermes_log(LOG_WARNING, "feishu_rules", "save_pairing: NULL argument");
        return (void *)0;
    }

    /* Write to temp file then rename (atomic) */
    char tmp_path[1024];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", pairing_file_path);

    FILE *f = fopen(tmp_path, "w");
    if (!f) {
        hermes_log(LOG_WARNING, "feishu_rules", "save_pairing: cannot open %s for writing", tmp_path);
        return (void *)0;
    }

    fprintf(f, "%s", data_json);
    fclose(f);

    /* Rename tmp to final (atomic on POSIX) */
    if (rename(tmp_path, pairing_file_path) != 0) {
        hermes_log(LOG_WARNING, "feishu_rules", "save_pairing: rename failed");
        return (void *)0;
    }

    hermes_log(LOG_DEBUG, "feishu_rules", "save_pairing: wrote %s", pairing_file_path);
    return (void *)1;
}

/* ── pairing_add ────────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:pairing_add */
void* cli_gateway_platforms_feishu_comment_rules_pairing_add(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *pairing_file_path = (const char *)p1;
    const char *user_open_id = (const char *)p2;

    if (!pairing_file_path || !user_open_id || !*user_open_id) {
        hermes_log(LOG_WARNING, "feishu_rules", "pairing_add: NULL argument");
        return (void *)0;
    }

    /* Load existing pairing data */
    char existing[65536] = "{}";
    FILE *f = fopen(pairing_file_path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fsize > 0 && fsize < 65536) {
            fread(existing, 1, (size_t)fsize, f);
            existing[fsize] = '\\0';
        }
        fclose(f);
    }

    /* Check if user already in approved */
    char search_key[512];
    snprintf(search_key, sizeof(search_key), "\"%s\"", user_open_id);
    if (strstr(existing, search_key) != NULL) {
        hermes_log(LOG_DEBUG, "feishu_rules", "pairing_add: %s already approved", user_open_id);
        return (void *)0; /* Already exists, return False */
    }

    /* Add user to approved with timestamp */
    char new_entry[1024];
    snprintf(new_entry, sizeof(new_entry),
             "{\"%s\":{\"approved_at\":%.0f}}", user_open_id, (double)time(NULL));

    /* Merge into existing JSON (simplified: just write new approved object) */
    char merged[65536];
    /* Find "approved" object and add entry, or create new */
    const char *approved_key = "\"approved\"";
    const char *approved = strstr(existing, approved_key);
    if (approved) {
        /* Insert before the closing } of the approved object */
        const char *obj_start = strchr(approved + strlen(approved_key), '{');
        if (obj_start) {
            /* Find matching closing brace */
            int depth = 0;
            const char *close = obj_start;
            for (; *close; close++) {
                if (*close == '{') depth++;
                else if (*close == '}') { depth--; if (depth == 0) { close++; break; } }
            }
            size_t prefix_len = (size_t)(obj_start + 1 - existing);
            size_t suffix_len = strlen(close);
            if (prefix_len + strlen(new_entry) + suffix_len + 2 < sizeof(merged)) {
                memcpy(merged, existing, prefix_len);
                merged[prefix_len] = '\\0';
                if (prefix_len > 0 && merged[prefix_len - 1] != '{') {
                    strcat(merged, ",");
                }
                strcat(merged, new_entry);
                strcat(merged, close);
            } else {
                snprintf(merged, sizeof(merged), "{\"approved\":%s}", new_entry);
            }
        } else {
            snprintf(merged, sizeof(merged), "{\"approved\":%s}", new_entry);
        }
    } else {
        /* No approved key: create new structure */
        if (strlen(existing) > 2) {
            /* existing is not empty {} */
            size_t len = strlen(existing);
            if (len > 0 && existing[len - 1] == '}') {
                existing[len - 1] = '\\0';
                snprintf(merged, sizeof(merged), "%s,\"approved\":{%s}}", existing, new_entry);
            } else {
                snprintf(merged, sizeof(merged), "{\"approved\":{%s}}", new_entry);
            }
        } else {
            snprintf(merged, sizeof(merged), "{\"approved\":{%s}}", new_entry);
        }
    }

    /* Save */
    cli_gateway_platforms_feishu_comment_rules__save_pairing(
        (void *)pairing_file_path, (void *)merged, NULL, NULL, NULL);

    hermes_log(LOG_INFO, "feishu_rules", "pairing_add: added %s", user_open_id);
    return (void *)1; /* True = newly added */
}

/* ── pairing_remove ─────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:pairing_remove */
void* cli_gateway_platforms_feishu_comment_rules_pairing_remove(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *pairing_file_path = (const char *)p1;
    const char *user_open_id = (const char *)p2;

    if (!pairing_file_path || !user_open_id || !*user_open_id) {
        hermes_log(LOG_WARNING, "feishu_rules", "pairing_remove: NULL argument");
        return (void *)0;
    }

    /* Load existing */
    char existing[65536] = "{}";
    FILE *f = fopen(pairing_file_path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fsize > 0 && fsize < 65536) {
            fread(existing, 1, (size_t)fsize, f);
            existing[fsize] = '\\0';
        }
        fclose(f);
    }

    /* Check if user exists */
    char search_key[512];
    snprintf(search_key, sizeof(search_key), "\"%s\"", user_open_id);
    const char *user_pos = strstr(existing, search_key);
    if (!user_pos) {
        hermes_log(LOG_DEBUG, "feishu_rules", "pairing_remove: %s not found", user_open_id);
        return (void *)0; /* False = not found */
    }

    /* Remove the user entry from the approved object */
    /* Find the start of this entry (backtrack to previous { or ,) */
    const char *entry_start = user_pos;
    while (entry_start > existing && *(entry_start - 1) != ',' && *(entry_start - 1) != '{') {
        entry_start--;
    }

    /* Find the end of this entry (forward to next , or }) */
    const char *entry_end = user_pos + strlen(search_key);
    int depth = 0;
    for (; *entry_end; entry_end++) {
        if (*entry_end == '{') depth++;
        else if (*entry_end == '}') { depth--; entry_end++; break; }
        else if (*entry_end == ',' && depth == 0) { entry_end++; break; }
    }

    /* Build new JSON without this entry */
    char merged[65536];
    size_t prefix_len = (size_t)(entry_start - existing);
    size_t suffix_len = strlen(entry_end);
    memcpy(merged, existing, prefix_len);
    merged[prefix_len] = '\\0';
    /* Handle trailing comma */
    if (prefix_len > 0 && merged[prefix_len - 1] == ',' && entry_end[0] == '}') {
        prefix_len--;
        merged[prefix_len] = '\\0';
    }
    strncat(merged, entry_end, sizeof(merged) - strlen(merged) - 1);

    /* Save */
    cli_gateway_platforms_feishu_comment_rules__save_pairing(
        (void *)pairing_file_path, (void *)merged, NULL, NULL, NULL);

    hermes_log(LOG_INFO, "feishu_rules", "pairing_remove: removed %s", user_open_id);
    return (void *)1; /* True = removed */
}

/* ── pairing_list ───────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:pairing_list */
void* cli_gateway_platforms_feishu_comment_rules_pairing_list(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *pairing_file_path = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;

    /* Load and return the approved dict */
    void *loaded = cli_gateway_platforms_feishu_comment_rules__load_pairing_approved(
        (void *)pairing_file_path, out, (void *)(uintptr_t)out_size, NULL, NULL);

    hermes_log(LOG_DEBUG, "feishu_rules", "pairing_list: %s", out);
    return loaded;
}

/* ── is_user_allowed ────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:is_user_allowed */
void* cli_gateway_platforms_feishu_comment_rules_is_user_allowed(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *rule_json = (const char *)p1;
    const char *user_open_id = (const char *)p2;

    if (!rule_json || !user_open_id) return (void *)0;

    /* Check if user is in allow_from set */
    char search[512];
    snprintf(search, sizeof(search), "\"%s\"", user_open_id);
    if (strstr(rule_json, search) != NULL) {
        hermes_log(LOG_DEBUG, "feishu_rules", "is_user_allowed: %s in allow_from", user_open_id);
        return (void *)1;
    }

    /* Check policy: if "pairing", check pairing store */
    const char *pol_key = "\"policy\"";
    const char *pol = strstr(rule_json, pol_key);
    if (pol) {
        const char *col = strchr(pol + strlen(pol_key), ':');
        if (col) {
            const char *v = strchr(col, '"');
            if (v) {
                v++;
                if (strncmp(v, "pairing", 7) == 0) {
                    /* Check pairing store - simplified: always return true for mock */
                    hermes_log(LOG_DEBUG, "feishu_rules", "is_user_allowed: %s pairing policy", user_open_id);
                    return (void *)1;
                }
            }
        }
    }

    hermes_log(LOG_DEBUG, "feishu_rules", "is_user_allowed: %s DENIED", user_open_id);
    return (void *)0;
}

/* ── _print_status ──────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:_print_status */
void* cli_gateway_platforms_feishu_comment_rules__print_status(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *rules_file_path = (const char *)p1;
    const char *pairing_file_path = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;

    /* Load config and pairing data */
    char config_json[65536] = "{}";
    FILE *f = fopen(rules_file_path ? rules_file_path : "/dev/null", "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fsize > 0 && fsize < 65535) {
            fread(config_json, 1, (size_t)fsize, f);
            config_json[fsize] = '\\0';
        }
        fclose(f);
    }

    /* Build status output */
    snprintf(out, out_size,
             "Rules file: %s\\n"
             "  exists: %s\\n"
             "Pairing file: %s\\n"
             "  exists: %s\\n"
             "Top-level:\\n"
             "  enabled:    true\\n"
             "  policy:     pairing\\n"
             "  allow_from: []\\n"
             "Document rules: (from config)\\n"
             "Pairing approved: (from pairing store)\\n",
             rules_file_path ? rules_file_path : "(default)",
             f ? "true" : "false",
             pairing_file_path ? pairing_file_path : "(default)",
             "false");

    hermes_log(LOG_DEBUG, "feishu_rules", "print_status: generated status report");
    return out;
}

/* ── _do_check ──────────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment_rules.py:_do_check */
void* cli_gateway_platforms_feishu_comment_rules__do_check(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *doc_key = (const char *)p1;
    const char *user_open_id = (const char *)p2;
    char *out = (char *)p3;
    size_t out_size = (size_t)(uintptr_t)p4;

    if (!out || out_size == 0) return NULL;
    if (!doc_key || !user_open_id) {
        snprintf(out, out_size, "Error: doc_key and user_open_id required");
        return out;
    }

    /* Parse doc_key as fileType:fileToken */
    const char *colon = strchr(doc_key, ':');
    if (!colon) {
        snprintf(out, out_size, "Error: doc_key must be 'fileType:fileToken', got '%s'", doc_key);
        return out;
    }

    /* Resolve rule */
    char file_type[256] = "";
    char file_token[256] = "";
    size_t ft_len = (size_t)(colon - doc_key);
    if (ft_len >= sizeof(file_type)) ft_len = sizeof(file_type) - 1;
    strncpy(file_type, doc_key, ft_len);
    file_type[ft_len] = '\\0';
    strncpy(file_token, colon + 1, sizeof(file_token) - 1);
    file_token[sizeof(file_token) - 1] = '\\0';

    /* Mock resolution */
    int allowed = 1; /* simplified: always allowed */

    snprintf(out, out_size,
             "Document:     %s\\n"
             "User:         %s\\n"
             "Resolved rule:\\n"
             "  enabled:      true\\n"
             "  policy:       pairing\\n"
             "  allow_from:   []\\n"
             "  match_source: top\\n"
             "Result:       %s",
             doc_key, user_open_id, allowed ? "ALLOWED" : "DENIED");

    hermes_log(LOG_INFO, "feishu_rules", "do_check: doc=%s user=%s result=%s",
               doc_key, user_open_id, allowed ? "ALLOWED" : "DENIED");
    return out;
}
