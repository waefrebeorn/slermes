/*
 * noc_w26_telegramla_b660b0.h - C surface for upstream
 * plugins/platforms/telegram/telegram_latex.py (gate w26_telegramla_b660b0).
 *
 * MarkdownV2 has no math, so known LaTeX macros in prose become the Unicode
 * symbols they name.  Everything here is text in, text out: code, paths and URL
 * spans are the caller's already-excluded text, which is why the macro guards
 * (lookbehind, run, URL spans) are part of the port.
 */
#ifndef NOC_W26_TELEGRAMLA_B660B0_H
#define NOC_W26_TELEGRAMLA_B660B0_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* normalize_latex_macros(prose) -> prose with known macros mapped. */
json_t *w26_telegramla_b660b0_normalize(const char *prose);

/* replace_macros_outside(prose, kept_spans) -> the rich-message variant: the
 * spans the client typesets itself ([[start, end], ...]) are left verbatim. */
json_t *w26_telegramla_b660b0_replace_outside(const char *prose, const json_t *kept_spans);

/* The macro table's shape and a few sampled symbols, as an object. */
json_t *w26_telegramla_b660b0_constants(void);

#ifdef __cplusplus
}
#endif
#endif
