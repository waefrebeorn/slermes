/*
 * port_platforms_base_helpers.c
 *
 * Pure, portable helper ported from gateway/platforms/base.py.
 *
 * The sibling gateway C port (src/gateway/platforms/base.c,
 * port_gateway_platforms_base_media.c, base_adapter.c, base_ext2.c) already
 * implements the other pure base.py helpers (_platform_name, _float_env,
 * _mark_notify_metadata, is_network_accessible, safe_url_for_log,
 * _looks_like_image, _log_safe_path, _normalize_media_tag_path,
 * _strip_media_tag_directives, _path_is_within, _error_blob, etc.) under
 * different symbols — so those are intentionally NOT duplicated here to avoid
 * collisions. Only the one genuinely-unported pure helper is ported below.
 *
 * Module prefix used by the scanner for gateway/platforms/base.py is "gw_".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* ---- lowercase helper ---- */
static void lc(char *s)
{
    for (; *s; s++) if (isupper((unsigned char)*s)) *s = (char)tolower((unsigned char)*s);
}

/*
 * PoP: gw_proxy_kwargs_for_bot @ gateway/platforms/base.py:proxy_kwargs_for_bot
 */
char *gw_proxy_kwargs_for_bot(const char *proxy_url)
{
    if (!proxy_url || !proxy_url[0]) return strdup("none");
    char *l = strdup(proxy_url);
    lc(l);
    int is_socks = strncmp(l, "socks", 5) == 0;
    free(l);
    if (is_socks) return strdup("connector:socks");
    char *out = malloc(strlen(proxy_url) + 8);
    snprintf(out, strlen(proxy_url) + 8, "proxy:%s", proxy_url);
    return out;
}
