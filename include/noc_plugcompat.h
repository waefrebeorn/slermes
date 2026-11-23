#ifndef NOC_PLUGCOMPAT_H
#define NOC_PLUGCOMPAT_H
#include "hermes_json.h"
json_t *plugcompat_compat_report(void);       /* empty object */
int    plugcompat_removal_in_effect(void);    /* 1 == True */
json_t *plugcompat_summary_lines(void);       /* empty array */
#endif /* NOC_PLUGCOMPAT_H */
