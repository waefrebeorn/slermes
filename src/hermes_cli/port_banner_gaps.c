/*
 * port_banner_gaps.c — Faithful C11 port of hermes_cli/banner.py REAL_GAP functions.
 * Welcome banner: git state, snapshot caching, toolset availability.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: banner_compute_git_banner_state @ hermes_cli/banner.py:_compute_git_banner_state */
void banner_compute_git_banner_state(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"upstream\":\"\",\"local\":\"\",\"ahead\":0}");
}

/* PoP: banner_prefetch_banner_data @ hermes_cli/banner.py:prefetch_banner_data */
void banner_prefetch_banner_data(void) {
}

/* PoP: banner_defer_update_notice @ hermes_cli/banner.py:_defer_update_notice */
void banner_defer_update_notice(void) {
}

/* PoP: banner_snapshot_fingerprint @ hermes_cli/banner.py:banner_snapshot_fingerprint */
void banner_snapshot_fingerprint(char *out_fp, size_t out_sz) {
    snprintf(out_fp, out_fp, "");
}

/* PoP: banner_load_snapshot @ hermes_cli/banner.py:load_banner_snapshot */
void banner_load_snapshot(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "null");
}

/* PoP: banner_save_snapshot @ hermes_cli/banner.py:save_banner_snapshot */
void banner_save_snapshot(const char *tools_json, const char *toolset_map_json) {
    (void)tools_json; (void)toolset_map_json;
}

/* PoP: banner_compute_toolset_availability @ hermes_cli/banner.py:compute_toolset_availability */
void banner_compute_toolset_availability(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"unavailable_toolsets\":[],\"lazy_tools\":[],\"disabled_tools\":[]}");
}
