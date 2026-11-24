/*
 * noc_w26_gatewaymul_37308a.h - C surface for upstream
 * hermes_cli/gateway_multiplex_served.py (gate w26_gatewaymul_37308a).
 *
 * Every function in the upstream module answers one question about the DEFAULT
 * multiplexer's runtime record: which secondary profiles it serves, which of a
 * served profile's platforms it deliberately does not run, and which inbound
 * URLs a served profile answers on.  The seam here is the record itself: the
 * gateway's live-PID proof and the ``gateway_state.json`` reader become fixed
 * inputs (``live`` and ``runtime``), so what is ported is exactly the value ->
 * value reasoning over that record.
 *
 * Python ``str()`` of a record element is emulated for strings, booleans and
 * integral numbers only (the shapes a served-profile / platform key takes).
 */
#ifndef NOC_W26_GATEWAYMUL_37308A_H
#define NOC_W26_GATEWAYMUL_37308A_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of platform families the multiplexer mirrors from the default. */
#define W26_GATEWAYMUL_37308A_MIRROR_PLATFORMS 3
#define W26_GATEWAYMUL_37308A_SHARED_LISTENERS 2

/* recorded_served_profiles(): runtime.served_profiles as [str, ...], or null
 * when the key is absent / not a list (or no live gateway). */
json_t *w26_gatewaymul_37308a_served_profiles(const json_t *runtime, int live);

/* multiplexer_served_secondaries(): the served list minus ""/"default". */
json_t *w26_gatewaymul_37308a_secondaries(const json_t *runtime, int live);

/* served_profile_unserved_platforms(): {platform: reason} for the "<profile>:"
 * entries the multiplexer marks disabled for shared ingress / unpaired WhatsApp. */
json_t *w26_gatewaymul_37308a_unserved_platforms(const json_t *runtime, int live,
                                                 const char *profile);

/* served_profile_ingress_urls(): {profile: {platform: url}} over the record's
 * ingress_url entries plus the shared-listener mirrors of served profiles. */
json_t *w26_gatewaymul_37308a_ingress_urls(const json_t *runtime, int live,
                                           const char *profile);

/* format_ingress_url_lines(): "<indent><platform>: <url>" sorted by platform. */
json_t *w26_gatewaymul_37308a_format_lines(const json_t *urls, const char *indent);

/* The mirror tables the module reads from gateway.config, as an object. */
json_t *w26_gatewaymul_37308a_constants(void);

#ifdef __cplusplus
}
#endif
#endif
