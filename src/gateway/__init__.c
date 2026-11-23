/*
 * __init__.c — C port of upstream Hermes gateway/__init__.py (lazy export table).
 * PoP: w6_init_0d6087_all @ gateway/__init__.py:__all__
 * PoP: w6_init_0d6087_target @ gateway/__init__.py:_EXPORTS
 */
#include "hermes_json.h"
#include "noc_w6_init_0d6087.h"
#include <string.h>
static const char *const EXPORTS[][2] = {
    {"GatewayConfig",".config"},{"PlatformConfig",".config"},{"HomeChannel",".config"},
    {"load_gateway_config",".config"},{"SessionContext",".session"},{"SessionStore",".session"},
    {"build_session_context_prompt",".session"},{"DeliveryRouter",".delivery"},
    {"DeliveryTarget",".delivery"},
};
#define N (sizeof(EXPORTS)/sizeof(EXPORTS[0]))
json_t *w6_init_0d6087_all(void){ json_t *a=json_array(); for(size_t i=0;i<N;i++) json_append(a,json_string(EXPORTS[i][0])); return a; }
json_t *w6_init_0d6087_target(const char *name){ if(!name) return NULL; for(size_t i=0;i<N;i++) if(!strcmp(name,EXPORTS[i][0])) return json_string(EXPORTS[i][1]); return NULL; }
int w6_init_0d6087_has(const char *name){ return w6_init_0d6087_target(name)!=NULL; }
