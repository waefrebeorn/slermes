/*
 * plugin_video_gen.c — Video generation plugin.
 * Port of Python plugins/video_gen/ (video gen SDK wrappers).
 *
 * Provides video generation through FAL.ai and XAI providers.
 * The C equivalent is in src/tools/video_gen.c + src/tools/video_gen_registry.c.
 *
 * Port of Python plugins/video_gen: C handles in tools/video_gen.c + video_gen_registry.c
 */
#include "plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Plugin metadata ──────────────────────────────────────────── */

const char *plugin_meta_name(void) {
    return "video-gen";
}

const char *plugin_meta_version(void) {
    return "0.3.0";
}

const char *plugin_meta_type(void) {
    return "tool";
}

const char *plugin_meta_description(void) {
    return "Video generation (FAL.ai, XAI) — C impl in tools/video_gen.c + video_gen_registry.c";
}

int plugin_deps_count(void) { return 0; }
const plugin_dep_t *plugin_deps_list(void) { return NULL; }

/* ── Init ─────────────────────────────────────────────────────── */

int plugin_init(void) {
    /* video_gen_init() called from tool_init.c during startup. */
    return 0;
}
