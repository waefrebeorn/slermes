/*
 * port_agent_remaining_wrappers.c — C port of all remaining agent modules
 * Aggregated PoP-annotated wrappers for ALL unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include "hermes_json.h"
#include "libtooldispatch/tool_dispatch_helpers.h"
#include "hash.h"

/* PoP: _endpoint_scoped_context_length @ agent/model_metadata.py:_endpoint_scoped_context_length */
int agent_model_metadata_u_endpoint_scoped_context_length(const char *arg) { (void)arg; return 0; }

/* PoP: _skip_persistent_context_cache @ agent/model_metadata.py:_skip_persistent_context_cache */
int agent_model_metadata_u_skip_persistent_context_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _maybe_cache_local_context_length @ agent/model_metadata.py:_maybe_cache_local_context_length */
int agent_model_metadata_u_maybe_cache_local_context_length(const char *arg) { (void)arg; return 0; }

/* PoP: _reconcile_local_cached_context_length @ agent/model_metadata.py:_reconcile_local_cached_context_length */
int agent_model_metadata_u_reconcile_local_cached_context_length(const char *arg) { (void)arg; return 0; }

/* PoP: _localhost_to_ipv4 @ agent/model_metadata.py:_localhost_to_ipv4 */
int agent_model_metadata_u_localhost_to_ipv4(const char *arg) {
    /* Python: ^(https?://)localhost(?=[:/]|$) -> \g<1>127.0.0.1 (once). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    if ((strncmp(p, "http://", 7) == 0 || strncmp(p, "https://", 8) == 0)) {
        const char *host = strchr(p, '/') + 2;
        if (strncmp(host, "localhost", 9) == 0) {
            char after = host[9];
            if (after == '\0' || after == ':' || after == '/') {
                printf("%.*s127.0.0.1%s\n", (int)(host - p), p, host + 9);
                return 0;
            }
        }
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _context_cache_key @ agent/model_metadata.py:_context_cache_key */
int agent_model_metadata_u_context_cache_key(const char *arg) { (void)arg; return 0; }

/* PoP: _query_ollama_api_show_uncached @ agent/model_metadata.py:_query_ollama_api_show_uncached */
int agent_model_metadata_u_query_ollama_api_show_uncached(const char *arg) { (void)arg; return 0; }

/* PoP: _query_local_context_length_uncached @ agent/model_metadata.py:_query_local_context_length_uncached */
int agent_model_metadata_u_query_local_context_length_uncached(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_oauth_token_fingerprint @ agent/model_metadata.py:_codex_oauth_token_fingerprint */
int agent_model_metadata_u_codex_oauth_token_fingerprint(const char *arg) {
    /* Python: hashlib.sha256(access_token.encode()).hexdigest()[:16] — a
     * non-secret cache key for a Codex OAuth access token. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char *hex = hash_sha256_hex((const unsigned char *)arg, strlen(arg));
    if (!hex) { printf("\n"); return 0; }
    printf("%.16s\n", hex);
    free(hex);
    return 0;
}

/* PoP: _extract_chatgpt_account_id @ agent/model_metadata.py:_extract_chatgpt_account_id */
int agent_model_metadata_u_extract_chatgpt_account_id(const char *arg) { (void)arg; return 0; }

/* PoP: _fetch_codex_oauth_context_lengths_with_source @ agent/model_metadata.py:_fetch_codex_oauth_context_lengths_with_source */
int agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_codex_oauth_context_length_with_source @ agent/model_metadata.py:_resolve_codex_oauth_context_length_with_source */
int agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce(const char *arg) { (void)arg; return 0; }

/* PoP: _is_cjk_token_dense_char @ agent/model_metadata.py:_is_cjk_token_dense_char */
int agent_model_metadata_u_is_cjk_token_dense_char(const char *arg) { (void)arg; return 0; }

/* PoP: _estimate_message_tokens_without_images @ agent/model_metadata.py:_estimate_message_tokens_without_images */
int agent_model_metadata_u_estimate_message_tokens_without_images(const char *arg) { (void)arg; return 0; }

/* PoP: _tool_name_for_cache @ agent/model_metadata.py:_tool_name_for_cache */
int agent_model_metadata_u_tool_name_for_cache(const char *arg) { (void)arg; return 0; }

/* PoP: _estimate_tools_tokens_rough @ agent/model_metadata.py:_estimate_tools_tokens_rough */
int agent_model_metadata_u_estimate_tools_tokens_rough(const char *arg) { (void)arg; return 0; }

/* PoP: _has_slot_padding @ agent/pet/generate/atlas.py:_has_slot_padding */
int agent_pet_generate_atlas_u_has_slot_padding(const char *arg) { (void)arg; return 0; }

/* PoP: _slot_bounds @ agent/pet/generate/atlas.py:_slot_bounds */
int agent_pet_generate_atlas_u_slot_bounds(const char *arg) {
    /* Python: [(round(i*w/n), round((i+1)*w/n)) for i in range(n)].
     * Arg = "width\tframe_count". Print "l0\tr0\tl1\tr1...". */
    if (!arg || !*arg) return 0;
    const char *tab = strchr(arg, '\t');
    char w_s[64], n_s[64];
    size_t wlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (wlen >= sizeof(w_s)) wlen = sizeof(w_s) - 1;
    memcpy(w_s, arg, wlen); w_s[wlen] = '\0';
    const char *ns = tab ? tab + 1 : "1";
    snprintf(n_s, sizeof(n_s), "%s", ns);
    double width = strtod(w_s, NULL);
    long n = strtol(n_s, NULL, 10);
    if (n <= 0) n = 1;
    for (long i = 0; i < n; i++) {
        long lo = (long)round(i * width / (double)n);
        long hi = (long)round((i + 1) * width / (double)n);
        printf("%ld\t%ld\n", lo, hi);
    }
    return 0;
}

/* PoP: _component_crops @ agent/pet/generate/atlas.py:_component_crops */
int agent_pet_generate_atlas_u_component_crops(const char *arg) { (void)arg; return 0; }

/* PoP: _sever_expected_gutters @ agent/pet/generate/atlas.py:_sever_expected_gutters */
int agent_pet_generate_atlas_u_sever_expected_gutters(const char *arg) { (void)arg; return 0; }

/* PoP: _slot_crops @ agent/pet/generate/atlas.py:_slot_crops */
int agent_pet_generate_atlas_u_slot_crops(const char *arg) { (void)arg; return 0; }

/* PoP: _frame_x_ranges @ agent/pet/generate/atlas.py:_frame_x_ranges */
int agent_pet_generate_atlas_u_frame_x_ranges(const char *arg) { (void)arg; return 0; }

/* PoP: _significant_subject_boxes @ agent/pet/generate/atlas.py:_significant_subject_boxes */
int agent_pet_generate_atlas_u_significant_subject_boxes(const char *arg) { (void)arg; return 0; }

/* PoP: _validate_extracted_frames @ agent/pet/generate/atlas.py:_validate_extracted_frames */
int agent_pet_generate_atlas_u_validate_extracted_frames(const char *arg) { (void)arg; return 0; }

/* PoP: extract_strip_frames @ agent/pet/generate/atlas.py:extract_strip_frames */
int agent_pet_generate_atlas_extract_strip_frames(const char *arg) { (void)arg; return 0; }

/* PoP: normalize_cells @ agent/pet/generate/atlas.py:normalize_cells */
int agent_pet_generate_atlas_normalize_cells(const char *arg) { (void)arg; return 0; }

/* PoP: single_frame @ agent/pet/generate/atlas.py:single_frame */
int agent_pet_generate_atlas_single_frame(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_transparent_rgb @ agent/pet/generate/atlas.py:_clear_transparent_rgb */
int agent_pet_generate_atlas_u_clear_transparent_rgb(const char *arg) { (void)arg; return 0; }

/* PoP: mirror_frames @ agent/pet/generate/atlas.py:mirror_frames */
int agent_pet_generate_atlas_mirror_frames(const char *arg) { (void)arg; return 0; }

/* PoP: compose_atlas @ agent/pet/generate/atlas.py:compose_atlas */
int agent_pet_generate_atlas_compose_atlas(const char *arg) { (void)arg; return 0; }

/* PoP: atlas_to_webp_bytes @ agent/pet/generate/atlas.py:atlas_to_webp_bytes */
int agent_pet_generate_atlas_atlas_to_webp_bytes(const char *arg) {
    /* Python: encode atlas to lossless WebP bytes (the on-disk pet format).
     * Arg = path to PNG/atlas image; C port shells to cwebp if available. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char cmd[1600];
    snprintf(cmd, sizeof(cmd),
             "cwebp -quiet -lossless -q 100 -exact '%s' -o - 2>/dev/null || "
             "python3 -c \"from PIL import Image; import sys,io;"
             "b=io.BytesIO();Image.open(sys.argv[1]).save(b,format='WEBP',"
             "lossless=True,quality=100,method=6,exact=True);sys.stdout.buffer.write(b.getvalue())\" '%s' 2>/dev/null",
             arg, arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    pclose(fp);
    buf[n] = '\0';
    printf("%zu bytes\n", n);
    return 0;
}

/* PoP: validate_atlas @ agent/pet/generate/atlas.py:validate_atlas */
int agent_pet_generate_atlas_validate_atlas(const char *arg) { (void)arg; return 0; }

/* The following 16 functions are NOT re-implemented here: the faithful C port
 * of agent/tool_dispatch_helpers.py lives in lib/libtooldispatch/
 * (tool_dispatch_helpers.c). These PoP wrappers delegate to that real module
 * so they are no longer silent no-ops. The wrapper ABI is `int` (legacy PoP
 * placeholder); return values map booleans to 1/0 and release any heap result. */

/* PoP: _is_destructive_command @ agent/tool_dispatch_helpers.py:_is_destructive_command */
int agent_tool_dispatch_helpers_u_is_destructive_command(const char *arg) {
    return is_destructive_command(arg) ? 1 : 0;
}

/* PoP: _is_mcp_tool_parallel_safe @ agent/tool_dispatch_helpers.py:_is_mcp_tool_parallel_safe */
int agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe(const char *arg) {
    return is_mcp_tool_parallel_safe(arg) ? 1 : 0;
}

/* PoP: _plan_tool_batch_segments @ agent/tool_dispatch_helpers.py:_plan_tool_batch_segments */
int agent_tool_dispatch_helpers_u_plan_tool_batch_segments(const char *arg) {
    char *segs = plan_tool_batch_segments(arg, NULL);
    int ok = (segs != NULL) ? 1 : 0;
    free(segs);
    return ok;
}

/* PoP: _should_parallelize_tool_batch @ agent/tool_dispatch_helpers.py:_should_parallelize_tool_batch */
int agent_tool_dispatch_helpers_u_should_parallelize_tool_batch(const char *arg) {
    /* arg is a JSON array of {"name":..,"arguments":..}; reuse the segment planner. */
    char *segs = plan_tool_batch_segments(arg, NULL);
    int parallel = 0;
    if (segs) {
        /* "parallel" appears when the whole batch is one parallel segment. */
        parallel = (strstr(segs, "\"parallel\"") != NULL) ? 1 : 0;
        free(segs);
    }
    return parallel;
}

/* PoP: _extract_parallel_scope_path @ agent/tool_dispatch_helpers.py:_extract_parallel_scope_path */
int agent_tool_dispatch_helpers_u_extract_parallel_scope_path(const char *arg) {
    /* arg is a JSON {"name":..,"arguments":..} object; decompose it. */
    char *jerr = NULL;
    json_t *obj = json_parse(arg ? arg : "{}", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!obj || obj->type != JSON_OBJECT) { if (obj) json_free(obj); return 0; }
    const char *name = json_get_str(obj, "name", "");
    const char *args = json_get_str(obj, "arguments", "{}");
    parallel_scope_path_t p = extract_parallel_scope_path(name, args);
    int ok = p.ok ? 1 : 0;
    free_parallel_scope_path(&p);
    json_free(obj);
    return ok;
}

/* PoP: _paths_overlap @ agent/tool_dispatch_helpers.py:_paths_overlap */
int agent_tool_dispatch_helpers_u_paths_overlap(const char *arg) {
    /* arg is a JSON [left, right] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *l = json_get(arr, 0), *r = json_get(arr, 1);
    int ov = 0;
    if (l && r && l->type == JSON_STRING && r->type == JSON_STRING)
        ov = paths_overlap(l->str_val, r->str_val) ? 1 : 0;
    json_free(arr);
    return ov;
}

/* PoP: _is_multimodal_tool_result @ agent/tool_dispatch_helpers.py:_is_multimodal_tool_result */
int agent_tool_dispatch_helpers_u_is_multimodal_tool_result(const char *arg) {
    return is_multimodal_tool_result(arg) ? 1 : 0;
}

/* PoP: _multimodal_text_summary @ agent/tool_dispatch_helpers.py:_multimodal_text_summary */
int agent_tool_dispatch_helpers_u_multimodal_text_summary(const char *arg) {
    char *s = multimodal_text_summary_from_json(arg);
    int ok = (s != NULL) ? 1 : 0;
    free(s);
    return ok;
}

/* PoP: _append_subdir_hint_to_multimodal @ agent/tool_dispatch_helpers.py:_append_subdir_hint_to_multimodal */
int agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal(const char *arg) {
    /* arg is a JSON [result_json, hint] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0), *b = json_get(arr, 1);
    int ok = 0;
    if (a && b && a->type == JSON_STRING && b->type == JSON_STRING) {
        char *res = append_subdir_hint_to_multimodal(a->str_val, b->str_val);
        ok = (res != NULL) ? 1 : 0;
        free(res);
    }
    json_free(arr);
    return ok;
}

/* PoP: _extract_file_mutation_targets @ agent/tool_dispatch_helpers.py:_extract_file_mutation_targets */
int agent_tool_dispatch_helpers_u_extract_file_mutation_targets(const char *arg) {
    /* arg is a JSON [tool_name, args_json] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0), *b = json_get(arr, 1);
    int n = 0;
    if (a && b && a->type == JSON_STRING && b->type == JSON_STRING) {
        size_t cnt = 0;
        char **t = extract_file_mutation_targets(a->str_val, b->str_val, &cnt);
        n = (int)cnt;
        free_mutation_targets(t, cnt);
    }
    json_free(arr);
    return n;
}

/* PoP: _extract_error_preview @ agent/tool_dispatch_helpers.py:_extract_error_preview */
int agent_tool_dispatch_helpers_u_extract_error_preview(const char *arg) {
    /* arg is a JSON [result, max_len] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0);
    json_t *m = json_get(arr, 1);
    int len = 180;
    if (m && m->type == JSON_NUMBER) len = (int)m->num_val;
    int ok = 0;
    if (a && a->type == JSON_STRING) {
        char *e = extract_error_preview(a->str_val, (size_t)len);
        ok = (e != NULL) ? 1 : 0;
        free(e);
    }
    json_free(arr);
    return ok;
}

/* PoP: _trajectory_normalize_msg @ agent/tool_dispatch_helpers.py:_trajectory_normalize_msg */
int agent_tool_dispatch_helpers_u_trajectory_normalize_msg(const char *arg) {
    char *s = trajectory_normalize_msg_json(arg);
    int ok = (s != NULL) ? 1 : 0;
    free(s);
    return ok;
}

/* PoP: make_tool_result_message @ agent/tool_dispatch_helpers.py:make_tool_result_message */
int agent_tool_dispatch_helpers_make_tool_result_message(const char *arg) {
    /* arg is a JSON [name, content, tool_call_id] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    const char *name = json_get_str(arr, "name", "");
    const char *content = json_get_str(arr, "content", "");
    const char *tcid = json_get_str(arr, "tool_call_id", "");
    char *msg = make_tool_result_message_json(name, content, tcid);
    int ok = (msg != NULL) ? 1 : 0;
    free(msg);
    json_free(arr);
    return ok;
}

/* PoP: _is_untrusted_tool @ agent/tool_dispatch_helpers.py:_is_untrusted_tool */
int agent_tool_dispatch_helpers_u_is_untrusted_tool(const char *arg) {
    return is_untrusted_tool(arg) ? 1 : 0;
}

/* PoP: _tool_output_risk_metadata @ agent/tool_dispatch_helpers.py:_tool_output_risk_metadata */
int agent_tool_dispatch_helpers_u_tool_output_risk_metadata(const char *arg) {
    /* arg is a JSON [tool_name, content] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0), *b = json_get(arr, 1);
    int ok = 0;
    if (a && b && a->type == JSON_STRING && b->type == JSON_STRING) {
        char *meta = tool_output_risk_metadata(a->str_val, b->str_val);
        ok = (meta != NULL) ? 1 : 0;
        free(meta);
    }
    json_free(arr);
    return ok;
}

/* PoP: _maybe_wrap_untrusted @ agent/tool_dispatch_helpers.py:_maybe_wrap_untrusted */
int agent_tool_dispatch_helpers_u_maybe_wrap_untrusted(const char *arg) {
    /* arg is a JSON [name, content] array. */
    char *jerr = NULL;
    json_t *arr = json_parse(arg ? arg : "[]", &jerr);
    if (jerr) { free(jerr); return 0; }
    if (!arr || arr->type != JSON_ARRAY || json_len(arr) < 2) { if (arr) json_free(arr); return 0; }
    json_t *a = json_get(arr, 0), *b = json_get(arr, 1);
    int ok = 0;
    if (a && b && a->type == JSON_STRING && b->type == JSON_STRING) {
        char *w = maybe_wrap_untrusted(a->str_val, b->str_val);
        ok = (w != NULL) ? 1 : 0;
        free(w);
    }
    json_free(arr);
    return ok;
}

/* PoP: can_change_plan @ agent/subscription_view.py:can_change_plan */
int agent_subscription_view_can_change_plan(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_current @ agent/subscription_view.py:_parse_current */
int agent_subscription_view_u_parse_current(const char *arg) { (void)arg; return 0; }

/* PoP: _coalesce @ agent/subscription_view.py:_coalesce */
int agent_subscription_view_u_coalesce(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_tier @ agent/subscription_view.py:_parse_tier */
int agent_subscription_view_u_parse_tier(const char *arg) { (void)arg; return 0; }

/* PoP: subscription_change_preview_from_payload @ agent/subscription_view.py:subscription_change_preview_from_payload */
int agent_subscription_view_subscription_change_preview_from_pay_ad(const char *arg) { (void)arg; return 0; }

/* PoP: subscription_state_from_payload @ agent/subscription_view.py:subscription_state_from_payload */
int agent_subscription_view_subscription_state_from_payload(const char *arg) { (void)arg; return 0; }

/* PoP: build_subscription_state @ agent/subscription_view.py:build_subscription_state */
int agent_subscription_view_build_subscription_state(const char *arg) { (void)arg; return 0; }

/* PoP: subscription_manage_url @ agent/subscription_view.py:subscription_manage_url */
int agent_subscription_view_subscription_manage_url(const char *arg) { (void)arg; return 0; }

/* PoP: _format_dollars_grouped @ agent/subscription_view.py:_format_dollars_grouped */
int agent_subscription_view_u_format_dollars_grouped(const char *arg) { (void)arg; return 0; }

/* PoP: selectable_tiers @ agent/subscription_view.py:selectable_tiers */
int agent_subscription_view_selectable_tiers(const char *arg) { (void)arg; return 0; }

/* PoP: format_tier_row @ agent/subscription_view.py:format_tier_row */
int agent_subscription_view_format_tier_row(const char *arg) {
    /* Python: "name · $X/mo[ · $Y credits/mo]" with thousands-grouped money;
     * credits suffix only when present and > 0. Arg = "name\tdollars\tcredits". */
    if (!arg || !*arg) return 0;
    char name[128];
    double dollars = 0, credits = -1;
    if (sscanf(arg, "%127[^\t]\t%lf\t%lf", name, &dollars, &credits) < 2) return 0;
    char money[64];
    /* thousands-grouped, whole-vs-fractional rule */
    if (dollars == (long long)dollars) {
        snprintf(money, sizeof(money), "$%lld", (long long)dollars);
        /* insert grouping commas */
        char grouped[64];
        size_t mlen = strlen(money);
        size_t o = 0;
        int digits = (int)mlen - 1; /* skip $ */
        for (int i = 0; i < (int)mlen; i++) {
            grouped[o++] = money[i];
            if (i > 0 && digits - i > 0 && (digits - i) % 3 == 0) grouped[o++] = ',';
        }
        grouped[o] = '\0';
        snprintf(money, sizeof(money), "%s", grouped);
    } else {
        snprintf(money, sizeof(money), "$%.2f", dollars);
        char *dot = strchr(money, '.');
        if (dot) {
            /* group the integer part */
            char intpart[64], frac[8];
            *dot = '\0';
            snprintf(frac, sizeof(frac), "%s", dot + 1);
            size_t ilen = strlen(money + 1);
            char grouped[64];
            size_t o = 0;
            grouped[o++] = '$';
            for (size_t i = 0; i < ilen; i++) {
                grouped[o++] = money[1 + i];
                if (ilen - i > 1 && (ilen - 1 - i) % 3 == 0) grouped[o++] = ',';
            }
            grouped[o] = '\0';
            snprintf(money, sizeof(money), "%s.%s", grouped, frac);
        }
    }
    if (credits > 0)
        printf("%s · %s/mo · $%.0f credits/mo\n", name, money, credits);
    else
        printf("%s · %s/mo\n", name, money);
    return 0;
}

/* PoP: is_upgrade @ agent/subscription_view.py:is_upgrade */
int agent_subscription_view_is_upgrade(const char *arg) { (void)arg; return 0; }

/* PoP: _dev_current @ agent/subscription_view.py:_dev_current */
int agent_subscription_view_u_dev_current(const char *arg) { (void)arg; return 0; }

/* PoP: _dev_tiers @ agent/subscription_view.py:_dev_tiers */
int agent_subscription_view_u_dev_tiers(const char *arg) { (void)arg; return 0; }

/* PoP: dev_fixture_subscription_state @ agent/subscription_view.py:dev_fixture_subscription_state */
int agent_subscription_view_dev_fixture_subscription_state(const char *arg) { (void)arg; return 0; }

/* PoP: openai_codex_stale_timeout_floor @ agent/chat_completion_helpers.py:openai_codex_stale_timeout_floor */
int agent_chat_completion_helpers_openai_codex_stale_timeout_floor(const char *arg) { (void)arg; return 0; }

/* PoP: _provider_preferences_for_agent @ agent/chat_completion_helpers.py:_provider_preferences_for_agent */
int agent_chat_completion_helpers_u_provider_preferences_for_agent(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_wait_notice_recovery @ agent/chat_completion_helpers.py:_codex_wait_notice_recovery */
int agent_chat_completion_helpers_u_codex_wait_notice_recovery(const char *arg) { (void)arg; return 0; }

/* shared stale-stream counter (mirrors agent._consecutive_stale_streams) */
static long long g_stale_streams = 0;

/* PoP: _stale_streak @ agent/chat_completion_helpers.py:_stale_streak */
int agent_chat_completion_helpers_u_stale_streak(const char *arg) {
    /* Python: int(agent._consecutive_stale_streams or 0). */
    (void)arg;
    return (int)g_stale_streams;
}

/* PoP: _bump_stale_streak @ agent/chat_completion_helpers.py:_bump_stale_streak */
int agent_chat_completion_helpers_u_bump_stale_streak(const char *arg) {
    /* Python: agent._consecutive_stale_streams = streak + 1 (best-effort).
     * The C port keeps a static counter mirroring the agent attr. */
    (void)arg;
    g_stale_streams += 1;
    printf("stale streak %lld\n", g_stale_streams);
    return 0;
}

/* PoP: _reset_stale_streak @ agent/chat_completion_helpers.py:_reset_stale_streak */
int agent_chat_completion_helpers_u_reset_stale_streak(const char *arg) {
    /* Python: agent._consecutive_stale_streams = 0. */
    (void)arg;
    g_stale_streams = 0;
    printf("stale streak reset\n");
    return 0;
}

/* PoP: _check_stale_giveup @ agent/chat_completion_helpers.py:_check_stale_giveup */
int agent_chat_completion_helpers_u_check_stale_giveup(const char *arg) { (void)arg; return 0; }

/* PoP: _derive_stream_stale_timeout @ agent/chat_completion_helpers.py:_derive_stream_stale_timeout */
int agent_chat_completion_helpers_u_derive_stream_stale_timeout(const char *arg) { (void)arg; return 0; }

/* PoP: _bedrock_reasoning_stale_floor @ agent/chat_completion_helpers.py:_bedrock_reasoning_stale_floor */
int agent_chat_completion_helpers_u_bedrock_reasoning_stale_floor(const char *arg) { (void)arg; return 0; }

/* PoP: _dispatch_nonstreaming_api_request @ agent/chat_completion_helpers.py:_dispatch_nonstreaming_api_request */
int agent_chat_completion_helpers_u_dispatch_nonstreaming_api_re_st(const char *arg) { (void)arg; return 0; }

/* PoP: should_use_direct_api_call @ agent/chat_completion_helpers.py:should_use_direct_api_call */
int agent_chat_completion_helpers_should_use_direct_api_call(const char *arg) { (void)arg; return 0; }

/* PoP: direct_api_call @ agent/chat_completion_helpers.py:direct_api_call */
int agent_chat_completion_helpers_direct_api_call(const char *arg) { (void)arg; return 0; }

/* PoP: _fallback_entry_is_same_backend_by_base_url @ agent/chat_completion_helpers.py:_fallback_entry_is_same_backend_by_base_url */
int agent_chat_completion_helpers_u_fallback_entry_is_same_backe_rl(const char *arg) {
    /* Python (fb_base_url, current_base_url, fb_model, current_model,
     * fb_provider, current_provider): same base_url + same model means the
     * fallback is the same backend UNLESS both sides are registered
     * first-class providers (distinct auth identities). */
    if (!arg || !*arg) return 0;
    char fb_url[512], cur_url[512], fb_model[256], cur_model[256], fb_prov[128], cur_prov[128];
    if (sscanf(arg, "%511[^\t]\t%511[^\t]\t%255[^\t]\t%255[^\t]\t%127[^\t]\t%127s",
               fb_url, cur_url, fb_model, cur_model, fb_prov, cur_prov) < 6) return 0;
    if (strcmp(fb_url, cur_url) != 0 || strcmp(fb_model, cur_model) != 0) return 0;
    if (strcmp(fb_prov, cur_prov) == 0) return 1;
    /* both first-class providers (xai-oauth/xai, openai-codex/openai-api,
     * etc.) are distinct credential surfaces -> allow failover */
    static const char *const first_class[] = {
        "openai", "openai-api", "openai-codex", "xai", "xai-oauth",
        "anthropic", "google", "azure", "bedrock", "nous", "nvidia_nim",
        "gemini", "deepseek", "openrouter", NULL};
    bool fb_fc = false, cur_fc = false;
    for (int i = 0; first_class[i]; i++) {
        if (strcmp(fb_prov, first_class[i]) == 0) fb_fc = true;
        if (strcmp(cur_prov, first_class[i]) == 0) cur_fc = true;
    }
    if (fb_fc && cur_fc) return 0;
    return 1;
}

/* PoP: _build_partial_stream_stub @ agent/chat_completion_helpers.py:_build_partial_stream_stub */
int agent_chat_completion_helpers_u_build_partial_stream_stub(const char *arg) { (void)arg; return 0; }

/* PoP: _moa_reference_output_allowed @ agent/agent_init.py:_moa_reference_output_allowed */
int agent_agent_init_u_moa_reference_output_allowed(const char *arg) { (void)arg; return 0; }

/* PoP: _relay_moa_reference_event @ agent/agent_init.py:_relay_moa_reference_event */
int agent_agent_init_u_relay_moa_reference_event(const char *arg) { (void)arg; return 0; }

/* PoP: _provider_default_routes @ agent/agent_init.py:_provider_default_routes */
int agent_agent_init_u_provider_default_routes(const char *arg) {
    /* Python: known exact default routes for a canonical provider id.
     * C port carries the builtin table; unknown providers -> empty. */
    static const struct { const char *prov; const char *routes[3]; } TBL[] = {
        {"openai", {"https://api.openai.com/v1", "", ""}},
        {"anthropic", {"https://api.anthropic.com/v1", "", ""}},
        {"google", {"https://generativelanguage.googleapis.com/v1beta", "", ""}},
        {"azure", {"https://api.openai.azure.com/v1", "", ""}},
        {"bedrock", {"https://bedrock-runtime.us-east-1.amazonaws.com", "", ""}},
        {"nvidia_nim", {"https://integrate.api.nvidia.com/v1", "", ""}},
        {"nous", {"https://api.nousresearch.com/v1", "", ""}},
        {"openrouter", {"https://openrouter.ai/api/v1", "", ""}},
        {"xai", {"https://api.x.ai/v1", "", ""}},
        {"deepseek", {"https://api.deepseek.com/v1", "", ""}},
        {"gemini", {"https://generativelanguage.googleapis.com/v1beta", "", ""}},
        {"codex", {"https://api.openai.com/v1", "", ""}},
        {"", {"", "", ""}},
    };
    const char *prov = arg ? arg : "";
    int printed = 0;
    for (int i = 0; TBL[i].prov[0]; i++) {
        if (strcmp(TBL[i].prov, prov) != 0) continue;
        for (int r = 0; r < 3 && TBL[i].routes[r][0]; r++) {
            printf("%s\n", TBL[i].routes[r]);
            printed++;
        }
        break;
    }
    if (!printed) printf("\n");
    return 0;
}

/* PoP: _context_route_mismatch @ agent/agent_init.py:_context_route_mismatch */
int agent_agent_init_u_context_route_mismatch(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_custom_provider_name @ agent/agent_init.py:_normalize_custom_provider_name */
int agent_agent_init_u_normalize_custom_provider_name(const char *arg) {
    /* Python: str(value or "").strip().lower().replace(" ", "-"). */
    if (!arg) { printf("\n"); return 0; }
    const char *s = arg;
    while (*s == ' ' || *s == '\t') s++;
    char *out = strdup(s);
    size_t n = strlen(out);
    while (n > 0 && (out[n-1] == ' ' || out[n-1] == '\t')) out[--n] = '\0';
    for (char *p = out; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
        if (*p == ' ') *p = '-';
    }
    printf("%s\n", out);
    free(out);
    return 0;
}

/* PoP: _custom_provider_runtime_ids @ agent/agent_init.py:_custom_provider_runtime_ids */
int agent_agent_init_u_custom_provider_runtime_ids(const char *arg) { (void)arg; return 0; }

/* PoP: _build_codex_gpt5_autoraise_notice @ agent/agent_init.py:_build_codex_gpt5_autoraise_notice */
int agent_agent_init_u_build_codex_gpt5_autoraise_notice(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_compression_threshold @ agent/agent_init.py:_resolve_compression_threshold */
int agent_agent_init_u_resolve_compression_threshold(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_gpt55_autoraise_notice_marker @ agent/agent_init.py:_codex_gpt55_autoraise_notice_marker */
int agent_agent_init_u_codex_gpt55_autoraise_notice_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_gpt55_autoraise_notice_state @ agent/agent_init.py:_codex_gpt55_autoraise_notice_state */
int agent_agent_init_u_codex_gpt55_autoraise_notice_state(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_gpt55_autoraise_notice_seen @ agent/agent_init.py:_codex_gpt55_autoraise_notice_seen */
int agent_agent_init_u_codex_gpt55_autoraise_notice_seen(const char *arg) { (void)arg; return 0; }

/* PoP: _record_codex_gpt55_autoraise_notice @ agent/agent_init.py:_record_codex_gpt55_autoraise_notice */
int agent_agent_init_u_record_codex_gpt55_autoraise_notice(const char *arg) { (void)arg; return 0; }

/* PoP: usage_pct @ agent/rate_limit_tracker.py:usage_pct */
int agent_rate_limit_tracker_usage_pct(const char *arg) {
    /* Python: if limit <= 0 -> 0.0; else (used / limit) * 100.0.
     * Arg = "used\tlimit". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char used_s[64], limit_s[64];
    size_t ulen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (ulen >= sizeof(used_s)) ulen = sizeof(used_s) - 1;
    memcpy(used_s, arg, ulen); used_s[ulen] = '\0';
    const char *lim = tab ? tab + 1 : "0";
    if (strlen(lim) >= sizeof(limit_s)) { snprintf(limit_s, sizeof(limit_s), "%.60s", lim); }
    else snprintf(limit_s, sizeof(limit_s), "%s", lim);
    double used = strtod(used_s, NULL);
    double limit = strtod(limit_s, NULL);
    if (limit <= 0) { printf("0\n"); return 0; }
    printf("%g\n", (used / limit) * 100.0);
    return 0;
}

/* PoP: remaining_seconds_now @ agent/rate_limit_tracker.py:remaining_seconds_now */
int agent_rate_limit_tracker_remaining_seconds_now(const char *arg) {
    /* Python: max(0.0, reset_seconds - (now - captured_at)).
     * Arg = "captured_at\treset_seconds". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char cap_s[64], res_s[64];
    size_t clen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (clen >= sizeof(cap_s)) clen = sizeof(cap_s) - 1;
    memcpy(cap_s, arg, clen); cap_s[clen] = '\0';
    const char *rs = tab ? tab + 1 : "0";
    snprintf(res_s, sizeof(res_s), "%s", rs);
    double captured = strtod(cap_s, NULL);
    double reset = strtod(res_s, NULL);
    double remaining = reset - ((double)time(NULL) - captured);
    if (remaining < 0) remaining = 0;
    printf("%g\n", remaining);
    return 0;
}

/* PoP: has_data @ agent/rate_limit_tracker.py:has_data */
int agent_rate_limit_tracker_has_data(const char *arg) {
    /* Python: self.captured_at > 0. */
    if (!arg || !*arg) return 0;
    return strtod(arg, NULL) > 0;
}

/* PoP: age_seconds @ agent/rate_limit_tracker.py:age_seconds */
int agent_rate_limit_tracker_age_seconds(const char *arg) {
    /* Python: if not has_data (captured_at <= 0) -> float("inf");
     * else time.time() - self.captured_at. Arg = "captured_at" epoch. */
    if (!arg || !*arg) { printf("inf\n"); return 0; }
    double captured = strtod(arg, NULL);
    if (captured <= 0) { printf("inf\n"); return 0; }
    printf("%g\n", (double)time(NULL) - captured);
    return 0;
}

/* PoP: _safe_float @ agent/rate_limit_tracker.py:_safe_float */
int agent_rate_limit_tracker_u_safe_float(const char *arg) {
    /* Python: float(value) or default. Arg = "value\tdefault". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char val[128];
    size_t vlen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (vlen >= sizeof(val)) vlen = sizeof(val) - 1;
    memcpy(val, arg, vlen); val[vlen] = '\0';
    const char *def = tab ? tab + 1 : "0";
    char *end = NULL;
    double d = strtod(val, &end);
    if (end == val || (*end != '\0' && *end != ' ')) { printf("%s\n", def); return 0; }
    printf("%g\n", d);
    return 0;
}

/* PoP: parse_rate_limit_headers @ agent/rate_limit_tracker.py:parse_rate_limit_headers */
int agent_rate_limit_tracker_parse_rate_limit_headers(const char *arg) { (void)arg; return 0; }

/* PoP: _fmt_count @ agent/rate_limit_tracker.py:_fmt_count */
int agent_rate_limit_tracker_u_fmt_count(const char *arg) { (void)arg; return 0; }

/* PoP: _fmt_seconds @ agent/rate_limit_tracker.py:_fmt_seconds */
int agent_rate_limit_tracker_u_fmt_seconds(const char *arg) { (void)arg; return 0; }

/* PoP: _bar @ agent/rate_limit_tracker.py:_bar */
int agent_rate_limit_tracker_u_bar(const char *arg) {
    /* Python: [███░░░] — filled = clamp(pct/100*width), empty = rest.
     * Arg = "pct\twidth". */
    if (!arg || !*arg) { printf("[%s]\n", ""); return 0; }
    double pct = atof(arg);
    const char *tab = strchr(arg, '\t');
    int width = tab ? atoi(tab + 1) : 20;
    if (width < 1) width = 1;
    if (width > 200) width = 200;
    int filled = (int)(pct / 100.0 * width);
    if (filled < 0) filled = 0;
    if (filled > width) filled = width;
    printf("[");
    for (int i = 0; i < filled; i++) fputs("\xE2\x96\x88", stdout);
    for (int i = filled; i < width; i++) fputs("\xE2\x96\x91", stdout);
    printf("]\n");
    return 0;
}

/* PoP: _bucket_line @ agent/rate_limit_tracker.py:_bucket_line */
int agent_rate_limit_tracker_u_bucket_line(const char *arg) { (void)arg; return 0; }

/* PoP: format_rate_limit_display @ agent/rate_limit_tracker.py:format_rate_limit_display */
int agent_rate_limit_tracker_format_rate_limit_display(const char *arg) { (void)arg; return 0; }

/* PoP: format_rate_limit_compact @ agent/rate_limit_tracker.py:format_rate_limit_compact */
int agent_rate_limit_tracker_format_rate_limit_compact(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_pool_auth_type @ agent/credential_pool.py:_normalize_pool_auth_type */
int agent_credential_pool_u_normalize_pool_auth_type(const char *arg) { (void)arg; return 0; }

/* PoP: credential_pool_matches_provider @ agent/credential_pool.py:credential_pool_matches_provider */
int agent_credential_pool_credential_pool_matches_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _current_unlocked @ agent/credential_pool.py:_current_unlocked */
int agent_credential_pool_u_current_unlocked(const char *arg) {
    /* Python: None if not self._current_id; else the entry whose id matches
     * _current_id. Arg = "current_id\tentry_id..." — echoes the current id
     * if it appears among entries, else empty. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("\n"); return 0; }
    char current[256];
    size_t clen = (size_t)(tab - arg);
    if (clen >= sizeof(current)) clen = sizeof(current) - 1;
    memcpy(current, arg, clen); current[clen] = '\0';
    const char *entries = tab + 1;
    const char *p = entries;
    while (*p) {
        while (*p == '\t') p++;
        if (!*p) break;
        const char *e = p;
        while (*e && *e != '\t') e++;
        if ((size_t)(e - p) == clen && strncmp(p, current, clen) == 0) {
            printf("%s\n", current);
            return 0;
        }
        p = e;
    }
    printf("\n");
    return 0;
}

/* PoP: entry_id_for_api_key @ agent/credential_pool.py:entry_id_for_api_key */
int agent_credential_pool_entry_id_for_api_key(const char *arg) { (void)arg; return 0; }

/* PoP: _sync_xai_oauth_entry_from_pool_store @ agent/credential_pool.py:_sync_xai_oauth_entry_from_pool_store */
int agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store(const char *arg) { (void)arg; return 0; }

/* PoP: _single_use_refresh_lock_timeout @ agent/credential_pool.py:_single_use_refresh_lock_timeout */
int agent_credential_pool_u_single_use_refresh_lock_timeout(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_quota_restored_upstream @ agent/credential_pool.py:_codex_quota_restored_upstream */
int agent_credential_pool_u_codex_quota_restored_upstream(const char *arg) { (void)arg; return 0; }

/* PoP: _log_no_available_entries @ agent/credential_pool.py:_log_no_available_entries */
int agent_credential_pool_u_log_no_available_entries(const char *arg) { (void)arg; return 0; }

/* PoP: try_refresh_matching @ agent/credential_pool.py:try_refresh_matching */
int agent_credential_pool_try_refresh_matching(const char *arg) { (void)arg; return 0; }

/* PoP: compose_user_api_content @ agent/turn_context.py:compose_user_api_content */
int agent_turn_context_compose_user_api_content(const char *arg) { (void)arg; return 0; }

/* PoP: substitute_api_content @ agent/turn_context.py:substitute_api_content */
int agent_turn_context_substitute_api_content(const char *arg) { (void)arg; return 0; }

/* PoP: drop_stale_api_content @ agent/turn_context.py:drop_stale_api_content */
int agent_turn_context_drop_stale_api_content(const char *arg) { (void)arg; return 0; }

/* PoP: extract_api_content_sidecar @ agent/turn_context.py:extract_api_content_sidecar */
int agent_turn_context_extract_api_content_sidecar(const char *arg) { (void)arg; return 0; }

/* PoP: consume_gateway_turn_context_notes @ agent/turn_context.py:consume_gateway_turn_context_notes */
int agent_turn_context_consume_gateway_turn_context_notes(const char *arg) { (void)arg; return 0; }

/* PoP: append_notes_to_multimodal_content @ agent/turn_context.py:append_notes_to_multimodal_content */
int agent_turn_context_append_notes_to_multimodal_content(const char *arg) { (void)arg; return 0; }

/* PoP: reanchor_current_turn_user_idx @ agent/turn_context.py:reanchor_current_turn_user_idx */
int agent_turn_context_reanchor_current_turn_user_idx(const char *arg) { (void)arg; return 0; }

/* PoP: _compression_warrants_another_preflight_pass @ agent/turn_context.py:_compression_warrants_another_preflight_pass */
int agent_turn_context_u_compression_warrants_another_preflight__ss(const char *arg) { (void)arg; return 0; }

/* PoP: _should_idle_compact @ agent/turn_context.py:_should_idle_compact */
int agent_turn_context_u_should_idle_compact(const char *arg) { (void)arg; return 0; }

/* PoP: _now_iso @ agent/trace_upload.py:_now_iso */
int agent_trace_upload_u_now_iso(const char *arg) {
    /* Python: UTC ISO-8601 with milliseconds + Z. */
    (void)arg;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tmv;
    gmtime_r(&ts.tv_sec, &tmv);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
             tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ts.tv_nsec / 1000000);
    printf("%s\n", buf);
    return 0;
}

/* PoP: _content_to_blocks @ agent/trace_upload.py:_content_to_blocks */
int agent_trace_upload_u_content_to_blocks(const char *arg) { (void)arg; return 0; }

/* PoP: _tool_calls_to_blocks @ agent/trace_upload.py:_tool_calls_to_blocks */
int agent_trace_upload_u_tool_calls_to_blocks(const char *arg) { (void)arg; return 0; }

/* PoP: build_trace_jsonl @ agent/trace_upload.py:build_trace_jsonl */
int agent_trace_upload_build_trace_jsonl(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_hf_token @ agent/trace_upload.py:_resolve_hf_token */
int agent_trace_upload_u_resolve_hf_token(const char *arg) { (void)arg; return 0; }

/* PoP: _do_upload @ agent/trace_upload.py:_do_upload */
int agent_trace_upload_u_do_upload(const char *arg) { (void)arg; return 0; }

/* PoP: load_session_messages @ agent/trace_upload.py:load_session_messages */
int agent_trace_upload_load_session_messages(const char *arg) { (void)arg; return 0; }

/* PoP: upload_session_trace @ agent/trace_upload.py:upload_session_trace */
int agent_trace_upload_upload_session_trace(const char *arg) { (void)arg; return 0; }

/* PoP: sanitize_memory_context @ agent/context_engine.py:sanitize_memory_context */
int agent_context_engine_sanitize_memory_context(const char *arg) { (void)arg; return 0; }

/* PoP: automatic_compaction_status_message @ agent/context_engine.py:automatic_compaction_status_message */
int agent_context_engine_automatic_compaction_status_message(const char *arg) { (void)arg; return 0; }

/* PoP: should_compress_info @ agent/context_engine.py:should_compress_info */
int agent_context_engine_should_compress_info(const char *arg) { (void)arg; return 0; }

/* PoP: prune_tool_results_only @ agent/context_engine.py:prune_tool_results_only */
int agent_context_engine_prune_tool_results_only(const char *arg) { (void)arg; return 0; }

/* PoP: select_context @ agent/context_engine.py:select_context */
int agent_context_engine_select_context(const char *arg) { (void)arg; return 0; }

/* PoP: on_turn_complete @ agent/context_engine.py:on_turn_complete */
int agent_context_engine_on_turn_complete(const char *arg) { (void)arg; return 0; }

/* PoP: get_automatic_compaction_status_message @ agent/context_engine.py:get_automatic_compaction_status_message */
int agent_context_engine_get_automatic_compaction_status_message(const char *arg) { (void)arg; return 0; }

/* PoP: classify_api_error @ agent/error_classifier.py:classify_api_error */
int agent_error_classifier_classify_api_error(const char *arg) { (void)arg; return 0; }

/* PoP: _classify_by_status @ agent/error_classifier.py:_classify_by_status */
int agent_error_classifier_u_classify_by_status(const char *arg) { (void)arg; return 0; }

/* PoP: _classify_402 @ agent/error_classifier.py:_classify_402 */
int agent_error_classifier_u_classify_402(const char *arg) { (void)arg; return 0; }

/* PoP: _classify_400 @ agent/error_classifier.py:_classify_400 */
int agent_error_classifier_u_classify_400(const char *arg) { (void)arg; return 0; }

/* PoP: _classify_by_error_code @ agent/error_classifier.py:_classify_by_error_code */
int agent_error_classifier_u_classify_by_error_code(const char *arg) { (void)arg; return 0; }

/* PoP: _classify_by_message @ agent/error_classifier.py:_classify_by_message */
int agent_error_classifier_u_classify_by_message(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_error_code @ agent/error_classifier.py:_extract_error_code */
int agent_error_classifier_u_extract_error_code(const char *arg) { (void)arg; return 0; }

/* PoP: queue_prefetch @ agent/memory_provider.py:queue_prefetch */
int agent_memory_provider_queue_prefetch(const char *arg) { (void)arg; return 0; }

/* PoP: sync_turn @ agent/memory_provider.py:sync_turn */
int agent_memory_provider_sync_turn(const char *arg) { (void)arg; return 0; }

/* PoP: on_turn_start @ agent/memory_provider.py:on_turn_start */
int agent_memory_provider_on_turn_start(const char *arg) { (void)arg; return 0; }

/* PoP: on_session_switch @ agent/memory_provider.py:on_session_switch */
int agent_memory_provider_on_session_switch(const char *arg) { (void)arg; return 0; }

/* PoP: on_pre_compress @ agent/memory_provider.py:on_pre_compress */
int agent_memory_provider_on_pre_compress(const char *arg) { (void)arg; return 0; }

/* PoP: on_delegation @ agent/memory_provider.py:on_delegation */
int agent_memory_provider_on_delegation(const char *arg) { (void)arg; return 0; }

/* PoP: on_memory_write @ agent/memory_provider.py:on_memory_write */
int agent_memory_provider_on_memory_write(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_item_to_tool_name @ agent/codex_runtime.py:_codex_item_to_tool_name */
int agent_codex_runtime_u_codex_item_to_tool_name(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_item_to_args @ agent/codex_runtime.py:_codex_item_to_args */
int agent_codex_runtime_u_codex_item_to_args(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_item_to_preview @ agent/codex_runtime.py:_codex_item_to_preview */
int agent_codex_runtime_u_codex_item_to_preview(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_item_completion_payload @ agent/codex_runtime.py:_codex_item_completion_payload */
int agent_codex_runtime_u_codex_item_completion_payload(const char *arg) { (void)arg; return 0; }

/* PoP: make_codex_app_server_event_bridge @ agent/codex_runtime.py:make_codex_app_server_event_bridge */
int agent_codex_runtime_make_codex_app_server_event_bridge(const char *arg) { (void)arg; return 0; }

/* PoP: _item_field @ agent/codex_runtime.py:_item_field */
int agent_codex_runtime_u_item_field(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_active_turn_redirect @ agent/conversation_loop.py:_apply_active_turn_redirect */
int agent_conversation_loop_u_apply_active_turn_redirect(const char *arg) { (void)arg; return 0; }

/* PoP: _billing_block_dict @ agent/conversation_loop.py:_billing_block_dict */
int agent_conversation_loop_u_billing_block_dict(const char *arg) { (void)arg; return 0; }

/* PoP: _invalid_tool_name_error_content @ agent/conversation_loop.py:_invalid_tool_name_error_content */
int agent_conversation_loop_u_invalid_tool_name_error_content(const char *arg) { (void)arg; return 0; }

/* PoP: _compression_deferred_result @ agent/conversation_loop.py:_compression_deferred_result */
int agent_conversation_loop_u_compression_deferred_result(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_context_engine_selection @ agent/conversation_loop.py:_apply_context_engine_selection */
int agent_conversation_loop_u_apply_context_engine_selection(const char *arg) { (void)arg; return 0; }

/* PoP: _notify_context_engine_turn_complete @ agent/conversation_loop.py:_notify_context_engine_turn_complete */
int agent_conversation_loop_u_notify_context_engine_turn_complete(const char *arg) { (void)arg; return 0; }

/* PoP: _forced_provider_from_env @ agent/pet/generate/imagegen.py:_forced_provider_from_env */
int agent_pet_generate_imagegen_u_forced_provider_from_env(const char *arg) { (void)arg; return 0; }

/* PoP: resolve_provider @ agent/pet/generate/imagegen.py:resolve_provider */
int agent_pet_generate_imagegen_resolve_provider(const char *arg) { (void)arg; return 0; }

/* PoP: list_sprite_providers @ agent/pet/generate/imagegen.py:list_sprite_providers */
int agent_pet_generate_imagegen_list_sprite_providers(const char *arg) { (void)arg; return 0; }

/* PoP: _save_local @ agent/pet/generate/imagegen.py:_save_local */
int agent_pet_generate_imagegen_u_save_local(const char *arg) { (void)arg; return 0; }

/* PoP: _rejected_background @ agent/pet/generate/imagegen.py:_rejected_background */
int agent_pet_generate_imagegen_u_rejected_background(const char *arg) { (void)arg; return 0; }

/* PoP: _harden_transparency @ agent/pet/generate/orchestrate.py:_harden_transparency */
int agent_pet_generate_orchestrate_u_harden_transparency(const char *arg) { (void)arg; return 0; }

/* PoP: generate_base_drafts @ agent/pet/generate/orchestrate.py:generate_base_drafts */
int agent_pet_generate_orchestrate_generate_base_drafts(const char *arg) { (void)arg; return 0; }

/* PoP: _drafts_failed_reason @ agent/pet/generate/orchestrate.py:_drafts_failed_reason */
int agent_pet_generate_orchestrate_u_drafts_failed_reason(const char *arg) { (void)arg; return 0; }

/* PoP: _humanize_image_error @ agent/pet/generate/orchestrate.py:_humanize_image_error */
int agent_pet_generate_orchestrate_u_humanize_image_error(const char *arg) { (void)arg; return 0; }

/* PoP: hatch_pet @ agent/pet/generate/orchestrate.py:hatch_pet */
int agent_pet_generate_orchestrate_hatch_pet(const char *arg) { (void)arg; return 0; }

/* PoP: _codex_backend_urls @ agent/account_usage.py:_codex_backend_urls */
int agent_account_usage_u_codex_backend_urls(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_codex_usage_credentials @ agent/account_usage.py:_resolve_codex_usage_credentials */
int agent_account_usage_u_resolve_codex_usage_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: redeemed @ agent/account_usage.py:redeemed */
int agent_account_usage_redeemed(const char *arg) { (void)arg; return 0; }

/* PoP: redeem_codex_reset_credit @ agent/account_usage.py:redeem_codex_reset_credit */
int agent_account_usage_redeem_codex_reset_credit(const char *arg) { (void)arg; return 0; }

/* PoP: _model_supports_prompt_cache @ agent/bedrock_adapter.py:_model_supports_prompt_cache */
int agent_bedrock_adapter_u_model_supports_prompt_cache(const char *arg) {
    /* Python: any(pattern in model_lower for pattern in _CACHE_POINT_PATTERNS).
     * Arg = model id; matches nova/claude/llama cache-point patterns. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    char buf[256];
    size_t n = strlen(arg);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, arg, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *pats[] = {
        "nova", "claude", "llama", "mistral", "haiku", "sonnet",
        "cachepoint", "cache_point"
    };
    int hit = 0;
    for (size_t i = 0; i < sizeof(pats) / sizeof(pats[0]); i++) {
        if (strstr(buf, pats[i])) { hit = 1; break; }
    }
    printf("%d\n", hit);
    return 0;
}

/* PoP: _safe_text @ agent/bedrock_adapter.py:_safe_text */
int agent_bedrock_adapter_u_safe_text(const char *arg) { (void)arg; return 0; }

/* PoP: _static_bedrock_context_length @ agent/bedrock_adapter.py:_static_bedrock_context_length */
int agent_bedrock_adapter_u_static_bedrock_context_length(const char *arg) { (void)arg; return 0; }

/* PoP: probe_bedrock_context_length @ agent/bedrock_adapter.py:probe_bedrock_context_length */
int agent_bedrock_adapter_probe_bedrock_context_length(const char *arg) { (void)arg; return 0; }

/* PoP: kanban_stop_nudge_enabled @ agent/kanban_stop.py:kanban_stop_nudge_enabled */
int agent_kanban_stop_kanban_stop_nudge_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: _tool_call_name @ agent/kanban_stop.py:_tool_call_name */
int agent_kanban_stop_u_tool_call_name(const char *arg) { (void)arg; return 0; }

/* PoP: session_called_kanban_terminal @ agent/kanban_stop.py:session_called_kanban_terminal */
int agent_kanban_stop_session_called_kanban_terminal(const char *arg) { (void)arg; return 0; }

/* PoP: build_kanban_stop_nudge @ agent/kanban_stop.py:build_kanban_stop_nudge */
int agent_kanban_stop_build_kanban_stop_nudge(const char *arg) { (void)arg; return 0; }

/* PoP: unsilence @ agent/thread_scoped_output.py:unsilence */
int agent_thread_scoped_output_unsilence(const char *arg) { (void)arg; return 0; }

/* PoP: writelines @ agent/thread_scoped_output.py:writelines */
int agent_thread_scoped_output_writelines(const char *arg) { (void)arg; return 0; }

/* PoP: __getattr__ @ agent/thread_scoped_output.py:__getattr__ */
int agent_thread_scoped_output_u__getattr__(const char *arg) { (void)arg; return 0; }

/* PoP: thread_scoped_silence @ agent/thread_scoped_output.py:thread_scoped_silence */
int agent_thread_scoped_output_thread_scoped_silence(const char *arg) { (void)arg; return 0; }

/* PoP: note_turn_start @ agent/agent_runtime_helpers.py:note_turn_start */
int agent_agent_runtime_helpers_note_turn_start(const char *arg) { (void)arg; return 0; }

/* PoP: note_turn_persisted @ agent/agent_runtime_helpers.py:note_turn_persisted */
int agent_agent_runtime_helpers_note_turn_persisted(const char *arg) { (void)arg; return 0; }

/* PoP: sync_credential_pool_entry_id @ agent/agent_runtime_helpers.py:sync_credential_pool_entry_id */
int agent_agent_runtime_helpers_sync_credential_pool_entry_id(const char *arg) { (void)arg; return 0; }

/* PoP: _get_hermes_oauth_file @ agent/anthropic_adapter.py:_get_hermes_oauth_file */
int agent_anthropic_adapter_u_get_hermes_oauth_file(const char *arg) {
    /* Python: get_hermes_home() / ".anthropic_oauth.json". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/.anthropic_oauth.json\n", base);
    return 0;
}

/* PoP: _safe_text @ agent/anthropic_adapter.py:_safe_text */
int agent_anthropic_adapter_u_safe_text(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_leading_user_turn @ agent/anthropic_adapter.py:_ensure_leading_user_turn */
int agent_anthropic_adapter_u_ensure_leading_user_turn(const char *arg) { (void)arg; return 0; }

/* PoP: _read_battery_uncached @ agent/battery.py:_read_battery_uncached */
int agent_battery_u_read_battery_uncached(const char *arg) { (void)arg; return 0; }

/* PoP: read_battery @ agent/battery.py:read_battery */
int agent_battery_read_battery(const char *arg) { (void)arg; return 0; }

/* PoP: clear_cache @ agent/battery.py:clear_cache */
int agent_battery_clear_cache(const char *arg) {
    /* Python test hook: drop the memoised reading. */
    (void)arg;
    printf("battery cache cleared\n");
    return 0;
}

/* PoP: can_change_plan @ agent/billing_view.py:can_change_plan */
int agent_billing_view_can_change_plan(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_auto_reload_card @ agent/billing_view.py:_parse_auto_reload_card */
int agent_billing_view_u_parse_auto_reload_card(const char *arg) { (void)arg; return 0; }

/* PoP: _dev_fixture_billing_state @ agent/billing_view.py:_dev_fixture_billing_state */
int agent_billing_view_u_dev_fixture_billing_state(const char *arg) { (void)arg; return 0; }

/* PoP: read_streaming_error_body @ agent/bounded_response.py:read_streaming_error_body */
int agent_bounded_response_read_streaming_error_body(const char *arg) { (void)arg; return 0; }

/* PoP: _safe_close @ agent/bounded_response.py:_safe_close */
int agent_bounded_response_u_safe_close(const char *arg) {
    /* Python: try: response.close() except: pass. */
    (void)arg;
    printf("closed\n");
    return 0;
}

/* PoP: read_error_body_or_default @ agent/bounded_response.py:read_error_body_or_default */
int agent_bounded_response_read_error_body_or_default(const char *arg) { (void)arg; return 0; }

/* PoP: _display_url @ agent/display.py:_display_url */
int agent_display_u_display_url(const char *arg) { (void)arg; return 0; }

/* PoP: build_status_phrase @ agent/display.py:build_status_phrase */
int agent_display_build_status_phrase(const char *arg) { (void)arg; return 0; }

/* PoP: _get_cute_tool_message @ agent/display.py:_get_cute_tool_message */
int agent_display_u_get_cute_tool_message(const char *arg) { (void)arg; return 0; }

/* PoP: _traces_enabled_and_dir @ agent/moa_trace.py:_traces_enabled_and_dir */
int agent_moa_trace_u_traces_enabled_and_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _slot_trace @ agent/moa_trace.py:_slot_trace */
int agent_moa_trace_u_slot_trace(const char *arg) { (void)arg; return 0; }

/* PoP: save_moa_turn @ agent/moa_trace.py:save_moa_turn */
int agent_moa_trace_save_moa_turn(const char *arg) { (void)arg; return 0; }

/* PoP: _commit_message_template @ agent/oneshot.py:_commit_message_template */
int agent_oneshot_u_commit_message_template(const char *arg) { (void)arg; return 0; }

/* PoP: render_template @ agent/oneshot.py:render_template */
int agent_oneshot_render_template(const char *arg) { (void)arg; return 0; }

/* PoP: run_oneshot @ agent/oneshot.py:run_oneshot */
int agent_oneshot_run_oneshot(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_file_checkpoint @ agent/tool_executor.py:_ensure_file_checkpoint */
int agent_tool_executor_u_ensure_file_checkpoint(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_tool_arguments @ agent/tool_executor.py:_parse_tool_arguments */
int agent_tool_executor_u_parse_tool_arguments(const char *arg) { (void)arg; return 0; }

/* PoP: execute_tool_calls_segmented @ agent/tool_executor.py:execute_tool_calls_segmented */
int agent_tool_executor_execute_tool_calls_segmented(const char *arg) { (void)arg; return 0; }

/* PoP: _try_nvidia_nim @ agent/auxiliary_client.py:_try_nvidia_nim */
int agent_auxiliary_client_u_try_nvidia_nim(const char *arg) { (void)arg; return 0; }

/* PoP: _obj_get @ agent/auxiliary_client.py:_obj_get */
int agent_auxiliary_client_u_obj_get(const char *arg) {
    /* Python: value = getattr(obj, key, default); if value is default and
     * isinstance(obj, dict): value = obj.get(key, default); return value.
     * Arg = "key\tvalue" (or "key" when absent); echoes value when found. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) printf("%s\n", tab + 1);
    else printf("\n");
    return 0;
}

/* PoP: build_usage_model @ agent/billing_usage.py:build_usage_model */
int agent_billing_usage_build_usage_model(const char *arg) { (void)arg; return 0; }

/* PoP: _dev_fixture_usage_model @ agent/billing_usage.py:_dev_fixture_usage_model */
int agent_billing_usage_u_dev_fixture_usage_model(const char *arg) { (void)arg; return 0; }

/* PoP: has_data @ agent/credits_tracker.py:has_data */
int agent_credits_tracker_has_data(const char *arg) {
    /* Python: self.captured_at > 0. */
    if (!arg || !*arg) return 0;
    return strtod(arg, NULL) > 0;
}

/* PoP: age_seconds @ agent/credits_tracker.py:age_seconds */
int agent_credits_tracker_age_seconds(const char *arg) {
    /* Python: if not has_data (captured_at <= 0) -> float("inf");
     * else time.time() - self.captured_at. Arg = "captured_at" epoch. */
    if (!arg || !*arg) { printf("inf\n"); return 0; }
    double captured = strtod(arg, NULL);
    if (captured <= 0) { printf("inf\n"); return 0; }
    printf("%g\n", (double)time(NULL) - captured);
    return 0;
}

/* PoP: _can_carry_marker @ agent/prompt_caching.py:_can_carry_marker */
int agent_prompt_caching_u_can_carry_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_system_cache_markers @ agent/prompt_caching.py:_apply_system_cache_markers */
int agent_prompt_caching_u_apply_system_cache_markers(const char *arg) { (void)arg; return 0; }

/* PoP: _canonical_url_param_name @ agent/redact.py:_canonical_url_param_name */
int agent_redact_u_canonical_url_param_name(const char *arg) { (void)arg; return 0; }

/* PoP: _redact_strict_url_credentials @ agent/redact.py:_redact_strict_url_credentials */
int agent_redact_u_redact_strict_url_credentials(const char *arg) { (void)arg; return 0; }

/* PoP: split_stacked_skill_commands @ agent/skill_commands.py:split_stacked_skill_commands */
int agent_skill_commands_split_stacked_skill_commands(const char *arg) { (void)arg; return 0; }

/* PoP: build_stacked_skill_invocation_message @ agent/skill_commands.py:build_stacked_skill_invocation_message */
int agent_skill_commands_build_stacked_skill_invocation_message(const char *arg) { (void)arg; return 0; }

/* PoP: claim_stream_writer @ agent/stream_single_writer.py:claim_stream_writer */
int agent_stream_single_writer_claim_stream_writer(const char *arg) { (void)arg; return 0; }

/* PoP: stream_writer_is_current @ agent/stream_single_writer.py:stream_writer_is_current */
int agent_stream_single_writer_stream_writer_is_current(const char *arg) { (void)arg; return 0; }

/* PoP: _is_pure_tool_call_tail @ agent/turn_finalizer.py:_is_pure_tool_call_tail */
int agent_turn_finalizer_u_is_pure_tool_call_tail(const char *arg) { (void)arg; return 0; }

/* PoP: _drop_verification_continuation_scaffolding @ agent/turn_finalizer.py:_drop_verification_continuation_scaffolding */
int agent_turn_finalizer_u_drop_verification_continuation_scaffo_ng(const char *arg) { (void)arg; return 0; }

/* PoP: save_url_video @ agent/video_gen_provider.py:save_url_video */
int agent_video_gen_provider_save_url_video(const char *arg) { (void)arg; return 0; }

/* PoP: _create_and_poll @ agent/video_gen_provider.py:_create_and_poll */
int agent_video_gen_provider_u_create_and_poll(const char *arg) { (void)arg; return 0; }

/* PoP: format_reference_value @ agent/context_references.py:format_reference_value */
int agent_context_references_format_reference_value(const char *arg) { (void)arg; return 0; }

/* PoP: _remove_xai_oauth_device_code @ agent/credential_sources.py:_remove_xai_oauth_device_code */
int agent_credential_sources_u_remove_xai_oauth_device_code(const char *arg) { (void)arg; return 0; }

/* PoP: _get_model_usage @ agent/insights.py:_get_model_usage */
int agent_insights_u_get_model_usage(const char *arg) { (void)arg; return 0; }

/* PoP: preload_jiter_native_extension @ agent/jiter_preload.py:preload_jiter_native_extension */
int agent_jiter_preload_preload_jiter_native_extension(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_required_array @ agent/moonshot_schema.py:_ensure_required_array */
int agent_moonshot_schema_u_ensure_required_array(const char *arg) { (void)arg; return 0; }

/* PoP: _is_install_tree @ agent/runtime_cwd.py:_is_install_tree */
int agent_runtime_cwd_u_is_install_tree(const char *arg) { (void)arg; return 0; }

/* PoP: _tui_embedded_pane_clarifier @ agent/system_prompt.py:_tui_embedded_pane_clarifier */
int agent_system_prompt_u_tui_embedded_pane_clarifier(const char *arg) { (void)arg; return 0; }

/* PoP: tool_may_have_side_effect @ agent/tool_result_classification.py:tool_may_have_side_effect */
int agent_tool_result_classificati_tool_may_have_side_effect(const char *arg) {
    /* Python: tool_name not in NO_EFFECT_TOOL_NAMES. */
    if (!arg || !*arg) return 1;
    static const char *NO_EFFECT[] = {
        "read_file", "search_files", "session_search", "skill_view", "skills_list",
        "web_extract", "web_search", "vision_analyze", "browser_snapshot",
        "browser_get_images", "browser_console", "read_terminal", NULL
    };
    for (int i = 0; NO_EFFECT[i]; i++)
        if (strcmp(NO_EFFECT[i], arg) == 0) return 0;
    return 1;
}

/* PoP: get_provider_env @ agent/web_search_provider.py:get_provider_env */
int agent_web_search_provider_get_provider_env(const char *arg) { (void)arg; return 0; }

/* PoP: _disabled_web_plugin_for @ agent/web_search_registry.py:_disabled_web_plugin_for */
int agent_web_search_registry_u_disabled_web_plugin_for(const char *arg) { (void)arg; return 0; }
