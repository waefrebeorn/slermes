#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
/* gate: traced copy of src/hermes_cli/main_wrappers.c::dashboard_procs_valid_lockfile_payload
 * (PoP: hermes_cli/dashboard_procs.py:_valid_lockfile_payload).
 * Input: 13 pipe-separated fields:
 *   own_param|parsed_own|schemaVersion|protocolVersion|spawnNonce|tokenFingerprint|pid|port|profile|hermesPath|hermesHome|logPath|startedAt
 * '-' = key absent from the record, '~' = empty string value. The rung order
 * below is the C port's ladder verbatim; JSON plumbing is factored into the
 * encoding (documented in gate.json notes). */
static const char *F(const char *f) {
    if (!f) return NULL;
    return strcmp(f, "-") == 0 ? NULL : (strcmp(f, "~") == 0 ? "" : f);
}
static int all_hex(const char *s) {
    for (const char *p = s; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) return 0;
    return 1;
}
static int valid_one(const char *rec) {
    char buf[8192]; snprintf(buf, sizeof buf, "%s", rec);
    char *fields[16]; int nf = 0;
    for (char *t = strtok(buf, "|"); t && nf < 16; t = strtok(NULL, "|")) fields[nf++] = t;
    while (nf < 13) fields[nf++] = (char *)"-";
    int res = 0;
    WCUP_BEGIN("valid_lockfile_payload");
    const char *own_p   = F(fields[0]);
    const char *own     = F(fields[1]);
    const char *sv_s    = F(fields[2]);
    const char *pv_s    = F(fields[3]);
    const char *nonce   = F(fields[4]);
    const char *tf      = F(fields[5]);
    const char *pid_s   = F(fields[6]);
    const char *port_s  = F(fields[7]);
    const char *strs[5] = { F(fields[8]), F(fields[9]), F(fields[10]), F(fields[11]), F(fields[12]) };
    do {
        if (!own || strcmp(own, own_p ? own_p : "") != 0) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        if (!sv_s || atol(sv_s) != 2) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        if (!pv_s || atol(pv_s) != 1) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        /* spawnNonce: str len16 hex */
        if (!nonce || strlen(nonce) != 16) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        if (!all_hex(nonce)) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        /* tokenFingerprint: str len32 hex */
        if (!tf || strlen(tf) != 32) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        if (!all_hex(tf)) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        /* pid: >0 <=4194304 ; port: 0..65535 */
        long pidv = pid_s ? atol(pid_s) : -1;
        int pid_num = pid_s && pid_s[strspn(pid_s, "0123456789")] == '\0' && pid_s[0] != '\0';
        if (!pid_num || pidv <= 0 || pidv > 4194304) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        long portv = port_s ? atol(port_s) : -1;
        int port_num = port_s && port_s[strspn(port_s, "0123456789")] == '\0' && port_s[0] != '\0';
        if (!port_num || portv < 0 || portv > 65535) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        /* bounded string fields */
        int bad_str = 0;
        for (int i = 0; i < 5; i++) {
            if (!strs[i] || strlen(strs[i]) > 1024) { bad_str = 1; break; }
        }
        if (bad_str) { WCUP_BRANCH(1); break; } WCUP_BRANCH(0);
        WCUP_LOOP_TICK();
        /* logPath suffix /{own}/{nonce}.log */
        char suffix[512]; snprintf(suffix, sizeof suffix, "/%s/%s.log", own_p, nonce);
        size_t ll = strlen(strs[3]), sl = strlen(suffix);
        res = (ll >= sl && strcmp(strs[3] + ll - sl, suffix) == 0);
    } while (0);
    char v[4]; snprintf(v, sizeof v, "%d", res);
    WCUP_VALSTR(v);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("valid_lockfile_payload");
    return res;
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s valid_lockfile_payload <rec>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) valid_one(argv[i]);
    return 0;
}
