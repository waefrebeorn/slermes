#ifndef NOC_W7_PROMPTREFRE_F02EC1_H
#define NOC_W7_PROMPTREFRE_F02EC1_H
struct json_t;
struct json_t *w7_promptrefre_f02ec1_session_epoch_key(void);
struct json_t *w7_promptrefre_f02ec1_applied_epoch(struct json_t *session_row);
struct json_t *w7_promptrefre_f02ec1_requested_epoch(struct json_t *spec);
struct json_t *w7_promptrefre_f02ec1_stored_prompt_refresh_epoch(struct json_t *spec);
#endif
