#ifndef NOC_W6_REDACTDBUR_875C0B_H
#define NOC_W6_REDACTDBUR_875C0B_H

/* C port of agent/redact_db_url.py (DB URL query-string password masking).
 * All returned char* values are malloc'd; caller frees them. */

/* PoP: w6_redactdbur_875c0b_is_userinfo_placeholder @ agent/redact_db_url.py:_is_userinfo_placeholder */
int w6_redactdbur_875c0b_is_userinfo_placeholder(const char *s);

/* PoP: w6_redactdbur_875c0b_is_template_value @ agent/redact_db_url.py:_is_template_value */
int w6_redactdbur_875c0b_is_template_value(const char *s);

/* PoP: w6_redactdbur_875c0b_mask_query_params @ agent/redact_db_url.py:_mask_query_params */
char *w6_redactdbur_875c0b_mask_query_params(const char *query);

/* PoP: w6_redactdbur_875c0b_redact_db_url_query_passwords @ agent/redact_db_url.py:redact_db_url_query_passwords */
char *w6_redactdbur_875c0b_redact_db_url_query_passwords(const char *text);

#endif /* NOC_W6_REDACTDBUR_875C0B_H */
