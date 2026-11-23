#ifndef NOC_TSCOPE_H
#define NOC_TSCOPE_H
/* Return whether ts_key is available on platform (hermes_cli/toolset_scope.py). */
int tscope_toolset_allowed_for_platform(const char *ts_key, const char *platform);
#endif
