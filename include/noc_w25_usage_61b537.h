/*
 * noc_w25_usage_61b537.h - C surface for upstream
 * hermes_cli/subcommands/usage.py (gate w25_usage_61b537).
 *
 * What is ported is the value->value core of ``hermes usage``: the ``--json``
 * document built from an account-usage snapshot, the stderr advisory printed
 * when a provider yields no snapshot, and the subcommand's own argparse
 * contract (name/help/description/flags) that cron jobs and shell loops bind
 * to.  The fetch (``agent.account_usage.fetch_account_usage``), the credential
 * resolution and the human renderer stay in the caller: they are I/O seams.
 */
#ifndef NOC_W25_USAGE_61B537_H
#define NOC_W25_USAGE_61B537_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* snapshot: {provider, source, title, plan, fetched_at, windows:[{label,
 * used_percent, reset_at, detail}], details, unavailable_reason} ->
 * the ``usage_snapshot_document`` dict: timestamps rendered with
 * ``datetime.isoformat()`` (already-ISO input passes through), keys stable.
 * null when the snapshot is not an object (the live code would raise). */
json_t *w25_usage_61b537_snapshot_document(const json_t *snapshot);

/* provider -> the stderr line ``cmd_usage`` prints before returning 1. */
json_t *w25_usage_61b537_no_usage_message(const json_t *provider);

/* The subcommand's argparse contract, as an object. */
json_t *w25_usage_61b537_constants(void);

#ifdef __cplusplus
}
#endif
#endif
