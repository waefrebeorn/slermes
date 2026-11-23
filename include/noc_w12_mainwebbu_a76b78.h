#ifndef NOC_W12_MAIN_WEB_BU_A76B78_H
#define NOC_W12_MAIN_WEB_BU_A76B78_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_mainwebbu_a76b78_web_project_root @ hermes_cli/main_web_build.py:_web_project_root
 * Repo root for a frontend dir (web/ or apps/<name>/). Caller frees. */
char *w12_mainwebbu_a76b78_web_project_root(const char *web_dir);

/* PoP: w12_mainwebbu_a76b78_web_dist_dir @ hermes_cli/main_web_build.py:_web_dist_dir
 * Vite output dir <project_root>/hermes_cli/web_dist. Caller frees. */
char *w12_mainwebbu_a76b78_web_dist_dir(const char *web_dir);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_MAIN_WEB_BU_A76B78_H */
