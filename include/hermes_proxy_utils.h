#ifndef HERMES_PROXY_UTILS_H
#define HERMES_PROXY_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file hermes_proxy_utils.h
 * @brief HTTP proxy resolution utilities.
 *
 * Port of Python agent/process_bootstrap.py:_get_proxy_from_env()
 * and _get_proxy_for_base_url().
 */

/**
 * Read proxy URL from environment variables.
 *
 * Checks HTTPS_PROXY, HTTP_PROXY, ALL_PROXY (and lowercase variants)
 * in order. Returns the first valid proxy URL found, or NULL if no
 * proxy is configured.
 *
 * The returned string is malloc'd — caller must free.
 *
 * Port of Python process_bootstrap._get_proxy_from_env().
 */
char *get_proxy_from_env(void);

/**
 * Return an env-configured proxy unless NO_PROXY excludes the given
 * base URL.
 *
 * base_url can be a full URL (https://api.example.com/v1) or a bare
 * hostname. Returns NULL if no proxy is configured or if NO_PROXY
 * excludes the host.
 *
 * The returned string is malloc'd — caller must free.
 *
 * Port of Python process_bootstrap._get_proxy_for_base_url().
 */
char *get_proxy_for_base_url(const char *base_url);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PROXY_UTILS_H */
