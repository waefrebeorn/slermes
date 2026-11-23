/* noc_w12_plugincata_a970ad.h — public C API port of hermes_cli/plugin_catalog.py */
#ifndef NOC_W12_PLUGINCATA_A970AD_H
#define NOC_W12_PLUGINCATA_A970AD_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* is_allowed_image_url: https scheme with a GitHub host (the two exact hosts
 * or any *.githubusercontent.com subdomain). */
bool w12_plugincata_a970ad_is_allowed_image_url(const char *url);

/* is_https_url: https scheme with a non-blank hostname; malformed authority
 * (e.g. "https://[") is refused, never raised. */
bool w12_plugincata_a970ad_is_https_url(const char *url);

/* _normalize_repo: canonical host/path for a repo URL, matching the kill-list
 * normalizer (scheme/user/www/.git/trailing-slash insensitive, scp URLs). */
json_t *w12_plugincata_a970ad_normalize_repo(const char *url);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_PLUGINCATA_A970AD_H */
