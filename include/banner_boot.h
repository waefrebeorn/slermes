/* banner_boot.h — Interactive TUI boot banner.
 *
 * Port of the LIVE Hermes boot appearance:
 *   - cli.py::show_banner            (compact/full dispatch, >=95-col logo)
 *   - hermes_cli/banner.py::build_welcome_banner
 *     (two-column Rich panel: caduceus left, tools/skills/MCP right)
 *   - cli.py::_build_compact_banner  (narrow-terminal box)
 *
 * Data sources are the REAL C subsystems: tool registry, skills dir scan,
 * ~/.slermes/config.yaml MCP servers, skin engine colors.
 */
#ifndef SLERMES_BANNER_BOOT_H
#define SLERMES_BANNER_BOOT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct skin_t skin_t;

typedef struct {
    const char *model;          /* active model slug ("" = unconfigured) */
    const char *cwd;            /* working directory shown dim           */
    const char *session_id;     /* session id or NULL                    */
    long long   context_length; /* 0 = unknown/omitted                   */
    bool        yolo;           /* YOLO mode warning line                */
    bool        compact;        /* force the compact banner              */
    const skin_t *skin;         /* active skin (NULL = default colors)   */
} banner_boot_info_t;

/* Render and print the interactive boot banner (full or compact). */
void banner_boot_show(const banner_boot_info_t *info);

#endif /* SLERMES_BANNER_BOOT_H */
