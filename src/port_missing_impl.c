/*
 * port_missing_impl.c — the definitions the tree calls but never provided.
 *
 * Linking every ported translation unit surfaced eight symbols that are *declared* in headers and
 * *called* by linked code, but defined nowhere in src/ or lib/:
 *
 *   desktop_controller_init, desktop_controller_set_boot_state      include/desktop_controller.h
 *   ctx_token_path                                                  gateway/platforms/weixin_http.h
 *   discover_entrypoint_manifests                                   hermes_cli/plugins_cmd_gaps.c
 *   tui (the global), tui_redraw_status                             src/cli/tui_state.h
 *
 * These are not duplicates and not platform seams; the Python side has them and the port never
 * carried them over. Each is implemented here against its header's contract, with the behaviour
 * kept honest: state is stored and reported, nothing pretends to do work it does not do.
 */
#include "desktop_controller.h"
#include "../gateway/platforms/weixin_http.h"
#include "tui_state.h"
#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── desktop controller ──────────────────────────────────────────────────────────────── */

static boot_state_t g_boot_state = BOOT_INIT;

/* Python: desktop controller keeps a single boot state the UI polls while starting up. */
void desktop_controller_init(void)
{
    g_boot_state = BOOT_INIT;
}

void desktop_controller_set_boot_state(boot_state_t state, const char *msg)
{
    (void)msg;                 /* the message is display-only; state is what callers read */
    g_boot_state = state;
}

boot_state_t desktop_controller_get_boot_state(void)
{
    return g_boot_state;
}

/* ── weixin context token cache path ─────────────────────────────────────────────────── */

/* Python: ctx_token_path builds the cache file path for weixin context tokens. Same shape as the
 * other per-platform cache files: $HERMES_HOME (or ~/.hermes) plus a fixed leaf name. */
void ctx_token_path(char *buf, size_t sz)
{
    if (!buf || sz == 0) return;
    const char *home = getenv("HERMES_HOME");
    if (home && *home)
        snprintf(buf, sz, "%s/weixin_ctx_token", home);
    else
        snprintf(buf, sz, ".hermes/weixin_ctx_token");
}

/* ── plugin entrypoint manifests ─────────────────────────────────────────────────────── */

/* Python: discover_entrypoint_manifests() walks installed distributions for
 * hermes.plugins entry points and returns { name: {...} }. Nothing in this tree indexes
 * installed distributions yet, so the honest answer is the empty object the callers already
 * handle -- returned as a real JSON document rather than NULL, because callers iterate it. */
json_t *discover_entrypoint_manifests(void)
{
    json_t *doc = json_object();
    return doc;
}

/* ── TUI model picker / status line ──────────────────────────────────────────────────── */

/* The global UI state the TUI files all share (src/cli/tui_state.h:951). */
tui_global_state_t tui;



/* model_picker_state_t has no `visible` flag: an empty list (count == 0) is what makes the
 * overlay not draw, so that is what "closed" means here. */
void tui_model_picker_init(void)
{
    memset(&tui.model_picker, 0, sizeof tui.model_picker);
}

void tui_draw_model_picker(void)
{
    /* The overlay is painted by the main draw loop when count > 0; there is no curses surface
     * to write to from here. */
}

void tui_redraw_status(void)
{
    /* The status line is repainted by the main draw loop on the next frame; there is no
     * dirty flag in tui_global_state_t, so this is the caller's request and nothing more. */
}
