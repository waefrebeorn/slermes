/*
 * noc_w21_adapterinb_940828.h -- public C API for the C11 port of upstream Hermes
 *   gateway/relay/adapter_inbound_media.py
 *
 * Relay inbound media localization: every per-attachment field (url, mime,
 * filename, inlined flag) is filtered together so survivors keep their own
 * semantics; a failed download drops that entry, never the message.
 *
 * The connector client (`download_with_name` / `is_relay_media_url`) is the
 * network seam: the C side takes its answers from the injected state. Every
 * exported symbol carries the w21_adapterinb_940828_ prefix.
 */
#ifndef NOC_W21_ADAPTERINB_940828_H
#define NOC_W21_ADAPTERINB_940828_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * pad_inline_flags(): state keys media_urls + media_text_inlined.
 * -> {"flags":[<bool|null>, ...]}  (missing/short flags pad with null)
 */
json_t *w21_adapterinb_940828_pad_flags(const json_t *state);

/*
 * localize_inbound_media() with the event and the media client as injected
 * state:
 *   media_urls, media_types, media_filenames, media_text_inlined  (as the event
 *     carries them; any may be missing or null)
 *   has_client(bool)             -- get_media_client() is not None
 *   client_raises(bool)          -- get_media_client() raised
 *   downloads{url: [path,name] | null | "__raise__"}
 *   relay_urls[url]              -- client.is_relay_media_url(url) is true
 * -> {"media_urls":[...],"media_types":[...],"media_filenames":[...],
 *     "media_text_inlined":[...],"raised":bool}
 * `raised` true means the live `except Exception` swallowed the failure and the
 * event was left untouched.
 */
json_t *w21_adapterinb_940828_localize(const json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_ADAPTERINB_940828_H */
