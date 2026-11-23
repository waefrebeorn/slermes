#ifndef NOC_W5_PACKAGES_6D8D5A_H
#define NOC_W5_PACKAGES_6D8D5A_H

/* C home for upstream Hermes pm/packages.py — gate w5_packages_6d8d5a. */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_packages_6d8d5a_dmgbuild_fetch_url @ pm/packages.py:Dmgbuild.fetch_url */
char *w5_packages_6d8d5a_dmgbuild_fetch_url(const char *version, const char *target);
/* PoP: w5_packages_6d8d5a_nodejs_fetch_url @ pm/packages.py:Nodejs.fetch_url */
char *w5_packages_6d8d5a_nodejs_fetch_url(const char *version, const char *target);
/* PoP: w5_packages_6d8d5a_git_fetch_url @ pm/packages.py:Git.fetch_url */
char *w5_packages_6d8d5a_git_fetch_url(const char *version, const char *target);
/* PoP: w5_packages_6d8d5a_gh_fetch_url @ pm/packages.py:Gh.fetch_url */
char *w5_packages_6d8d5a_gh_fetch_url(const char *version, const char *target);
/* PoP: w5_packages_6d8d5a_uv_fetch_url @ pm/packages.py:Uv.fetch_url */
char *w5_packages_6d8d5a_uv_fetch_url(const char *version, const char *target);
/* PoP: w5_packages_6d8d5a_ripgrep_fetch_url @ pm/packages.py:Ripgrep.fetch_url */
char *w5_packages_6d8d5a_ripgrep_fetch_url(const char *version, const char *target);
/* PoP: w5_packages_6d8d5a_chromium_store_entry @ pm/packages.py:Chromium.store_entry */
char *w5_packages_6d8d5a_chromium_store_entry(const char *version);
/* PoP: w5_packages_6d8d5a_npm_env @ pm/packages.py:npm_env */
json_t *w5_packages_6d8d5a_npm_env(const char *cache_dir, const json_t *base_env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_PACKAGES_6D8D5A_H */
