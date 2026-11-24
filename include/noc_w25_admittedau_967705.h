/*
 * noc_w25_admittedau_967705.h - C surface for upstream
 * gateway/relay/admitted_authors.py (gate w25_admittedau_967705).
 *
 * The pure decision core of the relay's per-author admission record: which
 * Discord senders this gateway admitted by delivering one of their messages,
 * and therefore which of their interactions may reach the runner.  The seams --
 * the process-level OrderedDict, gateway.config.Platform, the event object's
 * accessors and hermes_cli.commands.resolve_command -- stay in the caller; this
 * file owns only what the values decide.
 */
#ifndef NOC_W25_ADMITTEDAU_967705_H
#define NOC_W25_ADMITTEDAU_967705_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _MAX: the admission record's recency bound. */
#define W25_ADMITTEDAU_967705_MAX 4096

/* {"source": <object|null>} -> whether the source is a relayed Discord one. */
json_t *w25_admittedau_967705_relayed_discord(const json_t *req);

/* {"admitted": [<id>...], "max": <int|null>, "source": <object|null>} ->
 * {"admitted": [<id>...]} after note_delivered (append, move-to-end, evict). */
json_t *w25_admittedau_967705_note_delivered(const json_t *req);

/* {"admitted": [...], "source": ..., "sender": <any>} -> bool. */
json_t *w25_admittedau_967705_answer_admitted(const json_t *req);

/* {"admitted": [...], "source": ..., "sender": <any>, "admin_ids": [...],
 *  "command": <string|null>} -> bool. */
json_t *w25_admittedau_967705_interaction_admitted(const json_t *req);

/* The record's literal tables, as an object. */
json_t *w25_admittedau_967705_constants(void);

#ifdef __cplusplus
}
#endif
#endif
