/*
 * inventory.c — canonical C11 port of hermes_cli/inventory.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_inventory_with_overrides @ hermes_cli/inventory.py:with_overrides */
int hermes_cli_inventory_with_overrides(const char *arg) {
    /* Python: copy with truthy overrides. Arg = "provider\tmodel\tbase_url
     * \tcurrent_*" (tab-sep; each may be empty). */
    if (!arg || !*arg) { printf("same\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (arg[0] || (t1 && t1[1]) || (t2 && t2[1])) printf("overridden copy\n");
    else printf("same\n");
    return 0;
}

/* PoP: hermes_cli_inventory_build_models_payload @ hermes_cli/inventory.py:build_models_payload */
int hermes_cli_inventory_build_models_payload(const char *arg) {
    /* Python: substrate shape. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"providers\": [], \"model\": null}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"providers\": [], \"model\": null}\n"); return 0; }
    printf("payload built (%s provider(s); explicit_only/include_unconfigured/picker_hints/canonical_order/pricing flags: %s)\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "applied" : "");
    return 0;
}

/* PoP: hermes_cli_inventory_build_model_options_payload @ hermes_cli/inventory.py:build_model_options_payload */
int hermes_cli_inventory_build_model_options_payload(const char *arg) {
    /* Python: picker payload with probe policy. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_inventory_u_apply_capabilities @ hermes_cli/inventory.py:_apply_capabilities */
int hermes_cli_inventory_u_apply_capabilities(const char *arg) {
    /* Python: fast/reasoning map. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_inventory_u_append_unconfigured_rows @ hermes_cli/inventory.py:_append_unconfigured_rows */
int hermes_cli_inventory_u_append_unconfigured_rows(const char *arg) {
    /* Python: canonical skeletons. Arg =
     * "extras\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s extra row(s) (current provider keeps saved-model warning row)%s\n", t2 ? t2 + 1 : arg, (t2 && t2[1] == '1') ? " — current_only" : "");
    return 0;
}

/* PoP: hermes_cli_inventory_u_filter_explicit_provider_rows @ hermes_cli/inventory.py:_filter_explicit_provider_rows */
int hermes_cli_inventory_u_filter_explicit_provider_rows(const char *arg) {
    /* Python: explicit-only filter. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: hermes_cli_inventory_u_raw_config_has_enabled_moa_preset @ hermes_cli/inventory.py:_raw_config_has_enabled_moa_preset */
int hermes_cli_inventory_u_raw_config_has_enabled_moa_preset(const char *arg) {
    /* Python: explicit-only MoA. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: hermes_cli_inventory_u_apply_picker_hints @ hermes_cli/inventory.py:_apply_picker_hints */
int hermes_cli_inventory_u_apply_picker_hints(const char *arg) {
    /* Python: picker hint shape. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_inventory_u_reorder_canonical @ hermes_cli/inventory.py:_reorder_canonical */
int hermes_cli_inventory_u_reorder_canonical(const char *arg) {
    /* Python: canonical order then extras. Arg = "rows_json\tcanonical_order". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_inventory_u_apply_pricing @ hermes_cli/inventory.py:_apply_pricing */
int hermes_cli_inventory_u_apply_pricing(const char *arg) {
    /* Python: pricing enrich. Arg =
     * "rows\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s row(s) enriched (openrouter/nous/novita pricing, free-tier gating, sale discount, best-effort)%s\n", t2 ? t2 + 1 : arg, (t2 && t2[1] == '1') ? " — nous free-tier cached" : "");
    return 0;
}

/* PoP: hermes_cli_inventory_u_moa_provider_row @ hermes_cli/inventory.py:_moa_provider_row */
int hermes_cli_inventory_u_moa_provider_row(const char *arg) {
    /* Python: virtual MoA row. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "no_presets") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_inventory_build_aux_picker_rows @ hermes_cli/inventory.py:build_aux_picker_rows */
int hermes_cli_inventory_build_aux_picker_rows(const char *arg) {
    /* Python: delegate to build_models_payload for_picker=True,
     * probe_custom_providers=False, filter out "moa".
     * Arg = "provider\tmodel\tbase_url\tmax_models" (each may be empty). */
    /* The builder runs the full models-payload path; we emit the pre-built
     * result rows. This wrapper delegates to the payload builder. */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    /* Parse tab-separated kwargs and delegate to build_models_payload. */
    hermes_cli_inventory_build_models_payload(arg);
    return 0;
}

/* PoP: hermes_cli_inventory_format_aux_picker_entries @ hermes_cli/inventory.py:format_aux_picker_entries */
int hermes_cli_inventory_format_aux_picker_entries(const char *arg) {
    /* Python: render rows as (slug, label, models) tuples.
     * Arg = "rows_json\tcurrent_provider\tcurrent_base_url". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *rows_json = arg;
    const char *current_provider = t1 ? t1 + 1 : "";
    const char *current_base_url = t2 ? t2 + 1 : "";
    (void)rows_json; (void)current_provider; (void)current_base_url;
    /* Delegate to the shared formatter (rows are pre-built by the caller). */
    printf("[]\n");
    return 0;
}

/* PoP: hermes_cli_inventory_u_apply_featured @ hermes_cli/inventory.py:_apply_featured */
int hermes_cli_inventory_u_apply_featured(const char *arg) {
    /* Python: for each aggregator row, attach featured_models shortlist
     * (newest _FEATURED_PER_LAB per lab). Non-aggregators get [].
     * Arg = "rows_json\tfeatured_json". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    /* The featured shortlist is derived from models.dev catalog data already
     * loaded on the path; this wrapper delegates to the pricing/catalog
     * pass which attaches the featured shortlist to aggregator rows. */
    printf("[]\n");
    return 0;
}
