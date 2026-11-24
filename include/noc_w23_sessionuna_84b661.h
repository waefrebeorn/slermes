/*
 * noc_w23_sessionuna_84b661.h - C surface for upstream
 * gateway/session_unattended_unknown.py (gate w23_sessionuna_84b661).
 *
 * An ``unknown`` turn nobody is waiting on settles as interrupted: a cron run,
 * an automation envelope, a webhook delivery or a Kanban card attempt has no
 * person who could be asked to resend it, so the drained FIFO must not pause
 * behind it.  A person's row still pauses.
 *
 * Seams (injected by the harness, never decided here): the admitted rows the
 * store lists, which resolve calls the store accepts or refuses, and the media
 * release the settled rows trigger.
 */
#ifndef NOC_W23_SESSIONUNA_84B661_H
#define NOC_W23_SESSIONUNA_84B661_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* persistence.authority.vocabulary.CRON_PRINCIPAL_ID */
#define W23_SESSIONUNA_84B661_CRON_PRINCIPAL "cron-owner"

/* agent.turn_failure_copy.AUTOMATION_INTERRUPTED_NOTICE, committed with the
 * settlement in the same transaction. */
#define W23_SESSIONUNA_84B661_NOTICE \
    "An automated delivery was interrupted and was not run again. Some " \
    "actions may already have run; verify their effects."

const char *w23_sessionuna_84b661_cron_principal(void);
const char *w23_sessionuna_84b661_notice(void);

/* is_unattended(row): the principal is the cron owner or ``automation:...``, or
 * the payload/native markers name an automation, a Kanban card or a webhook. */
int w23_sessionuna_84b661_is_unattended(const json_t *row);

/* _unattended_unknown(records) -> the admission_id of every row that is
 * ``unknown`` and unattended, in the store's order. */
json_t *w23_sessionuna_84b661_unattended_unknown(const json_t *records);

/* settle_recovered_unattended(store, epoch) over req rows/epoch ->
 * {"settled", "resolved":[{admission_id,generation,epoch,notice}],
 *  "released":[id], "refused":[id]}.  A refused row is neither released nor
 * counted.  Rows carry the admission fields the store returns (id, status,
 * principal/payload, generation, target_session_id) and "refuse": true marks a
 * row the store's resolve refuses (AuthorityRefusal). */
json_t *w23_sessionuna_84b661_settle_recovered(const json_t *req);

/* settle_unattended_unknown(authority, ref) over req rows/session_id ->
 * {"settled":bool, "resolved":[{admission_id,generation,notice}],
 *  "refused":[id], "session_ids":[id]}. */
json_t *w23_sessionuna_84b661_settle_drain(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
