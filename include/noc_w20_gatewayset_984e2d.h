/*
 * noc_w20_gatewayset_984e2d.h -- public C API for the port of
 *   hermes_cli/gateway_setup_service.py (wave 20 batch 9).
 *
 * The setup wizard's questions, install/start orchestration and every refusal
 * path are pure decisions once the service manager is a scripted value, so the
 * whole module ports behind a `w20_gatewayset_984e2d_env` seam: the gateway
 * module's observable state and the two prompt answers go in, and a record of
 * the printed messages and the calls made comes out.
 */
#ifndef NOC_W20_GATEWAYSETH_984E2D_H
#define NOC_W20_GATEWAYSETH_984E2D_H

#include "hermes_json.h"

/* The question the wizard asks about persisting the gateway. */
#define W20_GATEWAYSET_984E2D_SERVICE_INSTALL_QUESTION \
    "Install the gateway service so Hermes starts automatically at login " \
    "and keeps scheduled jobs and messaging available?"

typedef struct {
    const json_t *config;    /* the `config` argument (NULL = None) */
    const json_t *stored;    /* what load_config() returns */
    const json_t *gw;        /* scripted hermes_cli.gateway state */
    const json_t *runtime;   /* scripted ensure_gateway_runtime() result */
    int interactive;         /* keyword-only `interactive` */
    int install;             /* keyword-only `install` */
    int isatty;              /* sys.stdin.isatty() */
    int prompt_answer;       /* answer of the first prompt_yes_no() */
    int start_now_answer;    /* answer of "  Start the gateway now?" */
} w20_gatewayset_984e2d_env;

/* record_service_choice(choice, config) */
json_t *w20_gatewayset_984e2d_record_service_choice(const char *choice,
                                                    const w20_gatewayset_984e2d_env *e);

/* wants_service_install(interactive=..., install=..., config=...) */
json_t *w20_gatewayset_984e2d_wants_service_install(const w20_gatewayset_984e2d_env *e);

/* ensure_gateway_service(context, interactive=..., install=..., config=...) */
json_t *w20_gatewayset_984e2d_ensure_gateway_service(const char *context,
                                                     const w20_gatewayset_984e2d_env *e);

/* _wizard_install_service(backend) */
json_t *w20_gatewayset_984e2d_wizard_install_service(const char *backend,
                                                     const w20_gatewayset_984e2d_env *e);

#endif /* NOC_W20_GATEWAYSETH_984E2D_H */
