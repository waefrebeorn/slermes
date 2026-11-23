#ifndef NOC_W12_UPDATECHAN_790D05_H
#define NOC_W12_UPDATECHAN_790D05_H

#include "hermes_json.h"

/* PoP port of hermes_cli/update_channel.py (gate w12_updatechan_790d05). */

/* is_canary_tag(tag) -> bool. */
json_t *w12_updatechan_790d05_is_canary_tag(const char *tag);
/* canary_timestamp(tag) -> string|null. */
json_t *w12_updatechan_790d05_canary_timestamp(const char *tag);
/* canary_tag_for_date(version, date_utc) -> string|null (ValueError -> null). */
json_t *w12_updatechan_790d05_canary_tag_for_date(const char *version, const char *date_utc);
/* _install_records(config) -> dict. */
json_t *w12_updatechan_790d05_install_records(const json_t *config);
/* _package_channel(stamp) -> bool. */
json_t *w12_updatechan_790d05_package_channel(const json_t *stamp);

#endif /* NOC_W12_UPDATECHAN_790D05_H */
