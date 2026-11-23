#ifndef NOC_W7_MANAGEDWORK_1AC730_H
#define NOC_W7_MANAGEDWORK_1AC730_H
struct json_t;
struct json_t *w7_managedwork_1ac730_scalar(struct json_t *value);
struct json_t *w7_managedwork_1ac730_structured(const char *key, struct json_t *value);
struct json_t *w7_managedwork_1ac730_result_frame(struct json_t *result);
struct json_t *w7_managedwork_1ac730_accept_result(struct json_t *result);
struct json_t *w7_managedwork_1ac730_activity_interval(double wedge_timeout);
struct json_t *w7_managedwork_1ac730_outbox_dir(const char *home, const char *execution_id);
struct json_t *w7_managedwork_1ac730_activity_frame_interval(const char *request_json);
#endif
