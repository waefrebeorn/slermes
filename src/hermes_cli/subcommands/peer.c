/*
 * peer.c — canonical C11 port of hermes_cli/subcommands/peer.py.
 *
 * Remote Hermes peer management: register/list/remove peers in a local JSON
 * registry (~/.hermes/peers.json), each entry carrying its URL + auth key
 * reference. Secret values are never stored here — _peer_secret resolves via
 * agent.secret_scope / .env at request time.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PEER_LIST_TIMEOUT_S 10

static char *peers_registry_path(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) home = "/tmp";
    char *out = NULL;
    if (asprintf(&out, "%s/peers.json", home) < 0) out = NULL;
    return out;
}

/* PoP: peer_load_peers @ hermes_cli/subcommands/peer.py:_load_peers */
json_t *peer_load_peers(void) {
    char *path = peers_registry_path();
    json_t *peers = json_object();
    if (!path) return peers;
    FILE *f = fopen(path, "r");
    if (f) {
        fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
        if (sz>0 && sz<4*1024*1024) {
            char *buf=malloc(sz+1);
            size_t got=fread(buf,1,sz,f); buf[got]=0;
            json_t *raw=json_parse(buf,NULL);
            if (raw && raw->type==JSON_OBJECT) { json_free(peers); peers=raw; }
            else if (raw) json_free(raw);
            free(buf);
        }
        fclose(f);
    }
    free(path);
    return peers;
}

/* PoP: peer_save_peers @ hermes_cli/subcommands/peer.py:_save_peers */
void peer_save_peers(json_t *peers) {
    char *path = peers_registry_path();
    if (!path) return;
    char *payload = json_serialize(peers);
    if (payload) { FILE *f=fopen(path,"w"); if (f){fputs(payload,f);fclose(f);} free(payload); }
    free(path);
}

/* PoP: peer_list @ hermes_cli/subcommands/peer.py:list_peers */
void peer_list(char *out, size_t out_sz) {
    json_t *peers = peer_load_peers();
    char *s = json_serialize(peers);
    snprintf(out, out_sz, "%s", s?s:"{}");
    free(s); json_free(peers);
}
