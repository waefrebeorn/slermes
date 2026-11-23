#ifndef NOC_W13_REDACTCONT_3747D1_H
#define NOC_W13_REDACTCONT_3747D1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_redactcont_3747d1_strip_shadow @ agent/redact_control_split.py:_strip_shadow
 * {shadow: text minus CSI sequences and lone control chars,
 *  noise: 1 per stripped byte, orig_idx: shadow -> text index}. */
struct json_t *w13_redactcont_3747d1_strip_shadow(const char *text);

/* PoP: w13_redactcont_3747d1_union @ agent/redact_control_split.py:_union
 * Sorted (start, end, flag) spans with overlaps merged; merged flag = all parts. */
struct json_t *w13_redactcont_3747d1_union(const struct json_t *spans);

/* PoP: w13_redactcont_3747d1_mask_control_split @ agent/redact_control_split.py:mask_control_split_tokens
 * Mask control/CSI-split prefix tokens. `prefix` drives the documented pattern
 * family `(<literal prefix>[A-Za-z0-9_+\-./]+)` used as prefix_re; sub_re=None
 * and skip=None; `mask` is the replacement for every masked span. */
struct json_t *w13_redactcont_3747d1_mask_control_split(const char *text,
                                                        const char *prefix, const char *mask);

/* PoP: w13_redactcont_3747d1_mask_prefix @ agent/redact_control_split.py:mask_prefix_matches
 * Mask every sub_re match (same documented pattern family) that skip keeps;
 * skip=None here, so every match (and swallowed one) is masked as one span. */
struct json_t *w13_redactcont_3747d1_mask_prefix(const char *text,
                                                 const char *prefix, const char *mask);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_REDACTCONT_3747D1_H */
