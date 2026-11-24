/*
 * noc_w25_codexrunti_62951b.h - C surface for upstream
 * agent/codex_runtime_history_seed.py (gate w25_codexrunti_62951b).
 *
 * The prior-transcript renderer handed to a FRESH codex app-server thread as
 * ``thread/start.developerInstructions``.  The app-server transport, the thread
 * id and the request payload are seams; what is ported is the pure rendering
 * law: how one content value flattens to text, how one row renders, and how the
 * transcript is tail-capped at MAX_HISTORY_SEED_CHARS.
 */
#ifndef NOC_W25_CODEXRUNTI_62951B_H
#define NOC_W25_CODEXRUNTI_62951B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define W25_CODEXRUNTI_62951B_MAX_HISTORY_SEED_CHARS 32000
#define W25_CODEXRUNTI_62951B_TOOL_RESULT_PREVIEW_CHARS 400
#define W25_CODEXRUNTI_62951B_HEADER \
    "Prior conversation from this Hermes session (the thread you are continuing was started fresh; " \
    "treat these turns as already having happened):"

/* A message ``content`` (str | list of str/{"text":..}) -> its text. */
json_t *w25_codexrunti_62951b_text_of(const json_t *content);

/* One message row -> its "[USER]/[ASSISTANT]/[TOOL RESULT]" rendering. */
json_t *w25_codexrunti_62951b_render_row(const json_t *msg);

/* {"messages": [...]} -> the full seed block ("" when there is nothing). */
json_t *w25_codexrunti_62951b_render_seed(const json_t *req);

/* The module constants + header, as an object. */
json_t *w25_codexrunti_62951b_constants(void);

#ifdef __cplusplus
}
#endif
#endif
