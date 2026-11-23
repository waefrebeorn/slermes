#ifndef NOC_W7_PROVIDERMED_9F3E4F_H
#define NOC_W7_PROVIDERMED_9F3E4F_H
struct json_t;
struct json_t *w7_providermed_9f3e4f_generated_subdir(void);
struct json_t *w7_providermed_9f3e4f_media_cache_max_age_hours(void);
struct json_t *w7_providermed_9f3e4f_max_save_url_redirects(void);
struct json_t *w7_providermed_9f3e4f_is_redirect_status(int code);
struct json_t *w7_providermed_9f3e4f_cache_path_ex(const char *home, const char *kind, const char *prefix,
                                                   const char *ts, const char *short_id, const char *extension);
#endif
