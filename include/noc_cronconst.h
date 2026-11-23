#ifndef NOC_CRONCONST_H
#define NOC_CRONCONST_H
#define CRONCONST_FIRE_CLAIM_TTL_SECONDS 300
#define CRONCONST_FIRE_CLAIM_SKEW_SECONDS 60
#define CRONCONST_CLAIM_TTL_INACTIVITY_HEADROOM 3
struct json_t;
int cronconst_is_recurring(const struct json_t *job);
#endif
