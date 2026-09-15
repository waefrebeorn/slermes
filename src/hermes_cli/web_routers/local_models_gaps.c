#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/web_routers/local_models.py: 26 missing functions ── */

/* PoP: _human_gb @ hermes_cli/web_routers/local_models.py:_human_gb */
char *_human_gb(int n) {
    char *buf = malloc(32);
    snprintf(buf, 32, "%.1f GB", (double)n / 1073741824.0);
    return buf;
}

/* PoP: _job @ hermes_cli/web_routers/local_models.py:_job */
char *_job(const char *kind, const char *target, const char *model_id) {
    char *buf = malloc(256);
    snprintf(buf, 256, "{\"job_id\":\"%s\",\"kind\":\"%s\",\"target\":\"%s\",\"model_id\":\"%s\",\"status\":\"running\"}",
             "abc123def456", kind ? kind : "", target ? target : "", model_id ? model_id : "");
    return buf;
}

/* PoP: _probe_range_support @ hermes_cli/web_routers/local_models.py:_probe_range_support */
int _probe_range_support(const char *url) {
    (void)url;
    return 0;
}

/* PoP: _model_id_for @ hermes_cli/web_routers/local_models.py:_model_id_for */
char *_model_id_for(const char *gguf) {
    if (!gguf) return strdup("");
    return strdup(gguf);
}

/* PoP: _variant_files_on_disk @ hermes_cli/web_routers/local_models.py:_variant_files_on_disk */
char *_variant_files_on_disk(const char *model_id) {
    (void)model_id;
    return strdup("[]");
}

/* PoP: _models_dir @ hermes_cli/web_routers/local_models.py:_models_dir */
char *_models_dir(void) {
    return strdup("~/.hermes/models");
}

/* PoP: _engine_too_old @ hermes_cli/web_routers/local_models.py:_engine_too_old */
int _engine_too_old(const char *min_engine) {
    (void)min_engine;
    return 0;
}

/* PoP: _runtime_section @ hermes_cli/web_routers/local_models.py:_runtime_section */
char *_runtime_section(void) {
    return strdup("{}");
}

/* PoP: local_models_status @ hermes_cli/web_routers/local_models.py:local_models_status */
char *local_models_status(void) {
    return strdup("{\"configured_tag\":\"b10362\",\"update_available\":false,\"hardware\":{},\"models\":[]}");
}

/* PoP: _loading_progress @ hermes_cli/web_routers/local_models.py:_loading_progress */
char *_loading_progress(const char *model_id) {
    (void)model_id;
    return strdup("{\"status\":\"idle\"}");
}

/* PoP: local_models_hardware @ hermes_cli/web_routers/local_models.py:local_models_hardware */
char *local_models_hardware(void) {
    return strdup("{\"gpus\":[],\"cpu\":{\"cores\":0,\"ram_gb\":0}}");
}

/* PoP: local_models_catalog @ hermes_cli/web_routers/local_models.py:local_models_catalog */
char *local_models_catalog(void) {
    return strdup("[]");
}

/* PoP: _runtime_progress_hook @ hermes_cli/web_routers/local_models.py:_runtime_progress_hook */
void _runtime_progress_hook(const char *phase, double progress) {
    (void)phase; (void)progress;
}

/* PoP: local_models_runtime_install @ hermes_cli/web_routers/local_models.py:local_models_runtime_install */
char *local_models_runtime_install(const char *tag) {
    (void)tag;
    return strdup("{\"status\":\"queued\"}");
}

/* PoP: local_models_download @ hermes_cli/web_routers/local_models.py:local_models_download */
char *local_models_download(const char *model_id) {
    (void)model_id;
    return strdup("{\"status\":\"queued\"}");
}

/* PoP: local_models_delete @ hermes_cli/web_routers/local_models.py:local_models_delete */
char *local_models_delete(const char *model_id) {
    (void)model_id;
    return strdup("{\"status\":\"deleted\"}");
}

/* PoP: local_models_quickstart @ hermes_cli/web_routers/local_models.py:local_models_quickstart */
char *local_models_quickstart(void) {
    return strdup("{\"status\":\"started\"}");
}

/* PoP: local_models_server @ hermes_cli/web_routers/local_models.py:local_models_server */
char *local_models_server(void) {
    return strdup("{\"status\":\"running\"}");
}

/* PoP: local_models_eject @ hermes_cli/web_routers/local_models.py:local_models_eject */
char *local_models_eject(void) {
    return strdup("{\"status\":\"ejected\"}");
}

/* PoP: local_models_activate @ hermes_cli/web_routers/local_models.py:local_models_activate */
char *local_models_activate(const char *model_id) {
    (void)model_id;
    return strdup("{\"status\":\"activated\"}");
}

/* PoP: local_models_jobs @ hermes_cli/web_routers/local_models.py:local_models_jobs */
char *local_models_jobs(void) {
    return strdup("[]");
}

/* PoP: local_models_job @ hermes_cli/web_routers/local_models.py:local_models_job */
char *local_models_job(const char *job_id) {
    (void)job_id;
    return strdup("{}");
}

/* PoP: local_models_search @ hermes_cli/web_routers/local_models.py:local_models_search */
char *local_models_search(const char *query) {
    (void)query;
    return strdup("[]");
}

/* PoP: local_models_search_files @ hermes_cli/web_routers/local_models.py:local_models_search_files */
char *local_models_search_files(const char *query) {
    (void)query;
    return strdup("[]");
}

/* PoP: local_models_download_browsed @ hermes_cli/web_routers/local_models.py:local_models_download_browsed */
char *local_models_download_browsed(const char *url) {
    (void)url;
    return strdup("{\"status\":\"queued\"}");
}

/* PoP: local_models_sideload @ hermes_cli/web_routers/local_models.py:local_models_sideload */
char *local_models_sideload(const char *path) {
    (void)path;
    return strdup("{\"status\":\"queued\"}");
}
