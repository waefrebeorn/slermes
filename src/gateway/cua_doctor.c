/*
 * cua_doctor.c — C11 port of tools/computer_use/doctor.py (NS-610).
 * Pure-logic report assembly, validation, identity, and text rendering.
 * Subprocess spawning (open_mcp / mcp_rpc / close_mcp / _read_cli_version
 * exec) is done by the caller via slermes process infra; the captured bytes
 * are passed into the pre-captured fields of cua_doctor_probes_t.
 */

#define _POSIX_C_SOURCE 200809L
#include "cua_doctor.h"
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/utsname.h>

/* ── Status glyphs ───────────────────────────────────────── */
/* PoP: _STATUS_GLYPH @ tools/computer_use/doctor.py:_STATUS_GLYPH */
/* PoP: _OVERALL_GLYPH @ tools/computer_use/doctor.py:_OVERALL_GLYPH */
static const char *STATUS_PASS = "\xE2\x9C\x85";
static const char *STATUS_FAIL = "\xE2\x9D\x8C";
static const char *STATUS_SKIP = "\xE2\x8F\xAD\xEF\xB8\x8F";
static const char *OVERALL_OK   = "\xE2\x9C\x85";
static const char *OVERALL_WARN = "\xE2\x9A\xA0\xEF\xB8\x8F";
static const char *OVERALL_FAIL = "\xE2\x9D\x8C";

static const char *glyph_for_status(const char *s)
{
    if (!s) return "\xE2\x80\xa2";
    if (strcmp(s,"pass")==0) return STATUS_PASS;
    if (strcmp(s,"fail")==0) return STATUS_FAIL;
    if (strcmp(s,"skip")==0) return STATUS_SKIP;
    return "\xE2\x80\xa2";
}
static const char *glyph_for_overall(const char *s)
{
    if (!s) return "\xE2\x80\xa2";
    if (strcmp(s,"ok")==0) return OVERALL_OK;
    if (strcmp(s,"degraded")==0) return OVERALL_WARN;
    if (strcmp(s,"failed")==0) return OVERALL_FAIL;
    return "\xE2\x80\xa2";
}
/* helper: bool from json node — Python `perms.get("x") is True` */
static bool _json_is_true(const json_t *n) { return n && n->type==JSON_BOOL && n->bool_val; }
static bool _json_is_false(const json_t *n) { return n && n->type==JSON_BOOL && !n->bool_val; }
static const char *_jstr(const json_t *n) { return (n && n->type==JSON_STRING) ? n->str_val : NULL; }

/* ── Validation ──────────────────────────────────────────── */

/* PoP: _is_valid_health_report @ tools/computer_use/doctor.py:_is_valid_health_report */
bool cua_doctor_is_valid_health_report(const json_t *payload)
{
    if (!payload || payload->type != JSON_OBJECT) return false;
    if (!json_obj_get(payload, "schema_version")) return false;
    if (!json_obj_get(payload, "overall")) return false;
    json_t *checks = json_obj_get(payload, "checks");
    if (!checks || checks->type != JSON_ARRAY) return false;
    return true;
}

/* PoP: _normalize_version_token @ tools/computer_use/doctor.py:_normalize_version_token */
char *cua_doctor_normalize_version_token(const char *text)
{
    if (!text || !*text) return strdup("");
    for (const char *p = text; *p; p++) {
        if (isdigit((unsigned char)*p)) {
            const char *s = p;
            while (s[0] && isdigit((unsigned char)s[0])) s++;
            if (s[0]=='.' && isdigit((unsigned char)s[1])) {
                const char *e = s + 1;
                while (e[0] && isdigit((unsigned char)e[0])) e++;
                if (e[0]=='.' && isdigit((unsigned char)e[1])) { e++; while (e[0] && isdigit((unsigned char)e[0])) e++; }
                if (e[0]=='-'||e[0]=='+') { e++; while (e[0] && (isalnum((unsigned char)e[0])||e[0]=='_'||e[0]=='.')) e++; }
                return strndup(p, (size_t)(e - p));
            }
        }
    }
    char *low = strdup(text);
    for (char *q = low; *q; q++) *q = (char)tolower((unsigned char)*q);
    return low;
}

/* PoP: _platform_name @ tools/computer_use/doctor.py:_platform_name */
void cua_doctor_platform_name(char *out, size_t out_sz)
{
    if (!out || !out_sz) return;
    out[0] = '\0';
    struct utsname u;
    if (uname(&u) != 0) return;
    const char *sysname = u.sysname;
    if (!sysname) { out[0]='\0'; return; }
    char buf[64];
    char *p = buf;
    for (const char *s = sysname; *s && p < buf + sizeof(buf) - 1; s++)
        *p++ = (char)tolower((unsigned char)*s);
    *p = '\0';
    if (strcmp(buf,"darwin")==0) sysname="darwin";
    else if (strcmp(buf,"windows")==0) sysname="windows";
    else if (strcmp(buf,"linux")==0) sysname="linux";
    else sysname=buf;
    strncpy(out, sysname, out_sz - 1);
    out[out_sz-1]='\0';
}

/* ── Identity ────────────────────────────────────────────── */

/* PoP: _build_identity @ tools/computer_use/doctor.py:_build_identity */
json_t *cua_doctor_build_identity(const char *binary,
                                  const char *cli_version,
                                  const char *health_report_driver_version)
{
    char *cli_tok = cua_doctor_normalize_version_token(cli_version);
    char *rep_tok = cua_doctor_normalize_version_token(health_report_driver_version);
    bool mismatch = (cli_tok[0] && rep_tok[0] && strcmp(cli_tok,rep_tok)!=0);
    json_t *id = json_object();
    json_set(id, "resolved_binary", json_string(binary?binary:""));
    json_set(id, "cli_version", (cli_version&&cli_version[0])?json_string(cli_version):json_null());
    json_set(id, "health_report_driver_version", (health_report_driver_version&&health_report_driver_version[0])?json_string(health_report_driver_version):json_null());
    json_set(id, "version_mismatch", json_bool(mismatch));
    free(cli_tok); free(rep_tok);
    return id;
}

/* ── Report extraction ───────────────────────────────────── */

/* PoP: _extract_health_report_from_result @ tools/computer_use/doctor.py:_extract_health_report_from_result */
int cua_doctor_extract_health_report(const json_t *result, json_t **out, char **out_msg)
{
    if (!result || result->type != JSON_OBJECT) {
        if (out_msg) *out_msg = strdup("health_report response was not an object");
        return CUA_DOC_PROTOCOL_ERR;
    }
    json_t *iserr = json_obj_get(result, "isError");
    if (iserr && _json_is_true(iserr)) {
        const char *denial = "health_report returned isError=true";
        json_t *content = json_obj_get(result, "content");
        if (content && content->type == JSON_ARRAY) {
            for (size_t i = 0; i < content->c.count; i++) {
                json_t *item = content->c.items[i];
                if (!item || item->type != JSON_OBJECT) continue;
                const char *type = _jstr(json_obj_get(item,"type"));
                if (type && strcmp(type,"text")==0) {
                    const char *txt = _jstr(json_obj_get(item,"text"));
                    if (txt && *txt) { denial = txt; break; }
                }
            }
        }
        if (out_msg) *out_msg = strdup(denial);
        return CUA_DOC_UNAVAILABLE;
    }
    json_t *sc = json_obj_get(result, "structuredContent");
    if (cua_doctor_is_valid_health_report(sc)) {
        if (out) *out = json_copy(sc);
        return CUA_DOC_OK;
    }
    json_t *content = json_obj_get(result, "content");
    if (content && content->type == JSON_ARRAY) {
        for (size_t i = 0; i < content->c.count; i++) {
            json_t *item = content->c.items[i];
            if (!item || item->type != JSON_OBJECT) continue;
            const char *type = _jstr(json_obj_get(item,"type"));
            if (type && strcmp(type,"text")==0) {
                const char *txt = _jstr(json_obj_get(item,"text"));
                if (txt) {
                    char *err = NULL;
                    json_t *parsed = json_parse(txt, &err);
                    free(err);
                    if (parsed && cua_doctor_is_valid_health_report(parsed)) {
                        if (out) *out = parsed;
                        else json_free(parsed);
                        return CUA_DOC_OK;
                    }
                    if (parsed) json_free(parsed);
                }
            }
        }
    }
    if (sc && sc->type == JSON_OBJECT) {
        if (out_msg) {
            /* Python: f"...(keys={sorted(sc.keys())})" */
            char keys_buf[512] = "[";
            for (size_t k = 0; k < sc->c.count; k++) {
                if (k) strcat(keys_buf, ", ");
                strcat(keys_buf, "'");
                strcat(keys_buf, sc->c.keys[k]);
                strcat(keys_buf, "'");
            }
            strcat(keys_buf, "]");
            size_t n = strlen("health_report structuredContent lacks schema_version/overall/checks (keys=") + strlen(keys_buf) + 1;
            char *msg = malloc(n + 1);
            snprintf(msg, n + 1, "health_report structuredContent lacks schema_version/overall/checks (keys=%s)", keys_buf);
            *out_msg = msg;
        }
        return CUA_DOC_UNAVAILABLE;
    }
    if (out_msg) {
        /* Python: f"...JSON text block. Result keys: {list(result.keys())}" */
        char keys_buf[512] = "[";
        for (size_t k = 0; k < result->c.count; k++) {
            if (k) strcat(keys_buf, ", ");
            strcat(keys_buf, "'");
            strcat(keys_buf, result->c.keys[k]);
            strcat(keys_buf, "'");
        }
        strcat(keys_buf, "]");
        size_t n = strlen("health_report response carried neither structuredContent nor a parseable JSON text block. Result keys: ") + strlen(keys_buf) + 1;
        char *msg = malloc(n + 1);
        snprintf(msg, n + 1, "health_report response carried neither structuredContent nor a parseable JSON text block. Result keys: %s", keys_buf);
        *out_msg = msg;
    }
    return CUA_DOC_PROTOCOL_ERR;
}

/* ── CLI version ─────────────────────────────────────────── */
/* PoP: _read_cli_version @ tools/computer_use/doctor.py:_read_cli_version */
char *cua_doctor_read_cli_version(const char *stdout_text, const char *stderr_text)
{
    char *combined = NULL;
    if (stdout_text && stderr_text) {
        size_t l1 = strlen(stdout_text), l2 = strlen(stderr_text);
        combined = malloc(l1 + l2 + 2);
        memcpy(combined, stdout_text, l1); combined[l1] = '\n';
        memcpy(combined + l1 + 1, stderr_text, l2);
        combined[l1 + l2 + 1] = '\0';
    } else if (stdout_text) combined = strdup(stdout_text);
    else if (stderr_text) combined = strdup(stderr_text);
    else return NULL;
    /* Python: splitlines()[0].strip() — first line, whitespace-stripped */
    char *first_nl = strchr(combined, '\n');
    if (first_nl) *first_nl = '\0';
    char *p = combined;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    char *r = strdup(p);
    size_t n = strlen(r);
    while (n > 0 && isspace((unsigned char)r[n-1])) r[--n] = '\0';
    free(combined);
    return r;
}

/* PoP: _cli_driver_version @ tools/computer_use/doctor.py:_cli_driver_version */
cua_doc_ver_t cua_doctor_cli_driver_version(const char *combined_text, int return_code)
{
    cua_doc_ver_t v = {NULL, NULL};
    /* Python: text = (stdout + stderr).strip();
     * if returncode != 0 and not text: return "fail", f"--version exited {rc}" */
    bool empty = (!combined_text || !*combined_text);
    if (empty && return_code != 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "--version exited %d", return_code);
        v.status = strdup("fail");
        v.version_or_msg = strdup(buf);
        return v;
    }
    if (empty) {
        /* rc==0, no text: version="unknown", status=pass */
        v.status = strdup("pass");
        v.version_or_msg = strdup("unknown");
        return v;
    }
    char *tok = cua_doctor_normalize_version_token(combined_text);
    bool found = (tok[0] != '\0' && strchr(tok, '.') != NULL);
    free(tok);
    if (!found) {
        char *nl = strchr(combined_text, '\n');
        size_t ll = nl ? (size_t)(nl - combined_text) : strlen(combined_text);
        char *first = strndup(combined_text, ll);
        for (char *q = first; *q; q++) *q = (char)tolower((unsigned char)*q);
        char *sp = first; while (*sp == ' ') sp++;
        v.status = strdup(return_code!=0?"fail":"pass");
        v.version_or_msg = strdup(sp);
        free(first);
        return v;
    }
    char tokbuf[128];
    const char *p = combined_text;
    for (;;) {
        if (isdigit((unsigned char)*p)) {
            const char *s = p;
            while (s[0] && isdigit((unsigned char)s[0])) s++;
            if (s[0]=='.' && isdigit((unsigned char)s[1])) {
                const char *e = s+1; while (e[0]&&isdigit((unsigned char)e[0])) e++;
                if (e[0]=='.' && isdigit((unsigned char)e[1])) { e++; while (e[0]&&isdigit((unsigned char)e[0])) e++; }
                if (e[0]=='-'||e[0]=='+') { e++; while (e[0]&&(isalnum((unsigned char)e[0])||e[0]=='_'||e[0]=='.')) e++; }
                size_t n = (size_t)(e-p);
                if (n < sizeof(tokbuf)) { memcpy(tokbuf,p,n); tokbuf[n]='\0'; break; }
            }
        }
        if (!*p) { strcpy(tokbuf, "unknown"); break; }
        p++;
    }
    v.status = strdup(return_code!=0?"fail":"pass");
    v.version_or_msg = strdup(tokbuf);
    return v;
}

/* PoP: _cli_doctor_snippet @ tools/computer_use/doctor.py:_cli_doctor_snippet */
char *cua_doctor_cli_doctor_snippet(const char *raw_output)
{
    if (!raw_output || !*raw_output) return NULL;
    char *s = strdup(raw_output);
    char *p = s;
    while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r') p++;
    char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - p);
    char *out = malloc(n + 1);
    memcpy(out, p, n); out[n] = '\0';
    free(s);
    return (*out) ? out : (free(out), NULL);
}

/* ── Fallback report composition ─────────────────────────── */

/* PoP: _compose_fallback_report @ tools/computer_use/doctor.py:_compose_fallback_report
 * PoP: _drive_fallback_probes @ tools/computer_use/doctor.py:_drive_fallback_probes
 * (MCP subprocess probes pre-resolved by caller into probes_t) */
json_t *cua_doctor_compose_fallback_report(const char *binary,
                                           const cua_doctor_probes_t *probes,
                                           const char *reason,
                                           double timeout)
{
    (void)timeout; (void)binary;
    char plat[32]; cua_doctor_platform_name(plat, sizeof(plat));
    json_t *checks = json_array();
    json_t *report = json_object();
    json_set(report, "schema_version", json_string("1"));
    json_set(report, "platform", json_string(plat));
    json_set(report, "checks", checks);

    /* --- binary_version --- */
    const char *ver_status = "fail";
    char *ver_msg = strdup("?");
    char *driver_version = strdup("?");
    if (probes) {
        if (probes->init_version) {
            free(driver_version); driver_version = strdup(probes->init_version);
            ver_status = "pass";
            free(ver_msg); ver_msg = malloc(strlen(probes->init_version)+32);
            snprintf(ver_msg, strlen(probes->init_version)+32, "cua-driver %s", probes->init_version);
        } else if (probes->binary_version) {
            free(driver_version); driver_version = strdup(probes->binary_version);
            ver_status = probes->binary_version_pass ? "pass":"fail";
            if (probes->binary_version_pass) {
                free(ver_msg); ver_msg = malloc(strlen(probes->binary_version)+32);
                snprintf(ver_msg, strlen(probes->binary_version)+32, "cua-driver %s", probes->binary_version);
            } else {
                /* Python: ver_msg = ver_value or "version unknown" → ver_value is binary_version */
                free(ver_msg); ver_msg = strdup(probes->binary_version[0]?probes->binary_version:"version unknown");
            }
        }
        /* else: Python keeps ver_value="?" → ver_msg = "?" (truthy) */
    }
    json_t *bv = json_object();
    json_set(bv, "name", json_string("binary_version"));
    json_set(bv, "status", json_string(ver_status));
    json_set(bv, "message", json_string(ver_msg));
    json_append(checks, bv);
    free(ver_msg);

    /* --- platform_supported --- */
    bool supported = (strcmp(plat,"darwin")==0||strcmp(plat,"linux")==0||strcmp(plat,"windows")==0);
    json_t *ps = json_object();
    json_set(ps, "name", json_string("platform_supported"));
    json_set(ps, "status", json_string(supported?"pass":"fail"));
    { char pmsg[128]; snprintf(pmsg,sizeof(pmsg),"platform=%s%s",plat,supported?"":" (unsupported)"); json_set(ps,"message",json_string(pmsg)); }
    json_append(checks, ps);

    /* --- session_active (skip) --- */
    json_t *sa = json_object();
    json_set(sa,"name",json_string("session_active"));
    json_set(sa,"status",json_string("skip"));
    json_set(sa,"message",json_string("not probed (doctor does not open a cua session)"));
    json_append(checks, sa);

    /* --- TCC --- */
    const json_t *perms = probes ? probes->permissions : NULL;
    const char *perm_err = probes ? probes->permissions_error : NULL;
    if (perms && perms->type == JSON_OBJECT) {
        json_t *axval = json_obj_get(perms, "accessibility");
        json_t *axc = json_object();
        json_set(axc,"name",json_string("tcc_accessibility"));
        if (_json_is_true(axval)) {
            json_set(axc,"status",json_string("pass")); json_set(axc,"message",json_string("Accessibility is granted."));
            json_t *d=json_object(); json_set(d,"accessibility",json_bool(true)); json_set(axc,"data",d);
        } else if (_json_is_false(axval)) {
            json_set(axc,"status",json_string("fail")); json_set(axc,"message",json_string("Accessibility is not granted."));
            json_set(axc,"hint",json_string("Grant Accessibility to CuaDriver in System Settings \xE2\x86\x92 Privacy & Security."));
            json_t *d=json_object(); json_set(d,"accessibility",json_bool(false)); json_set(axc,"data",d);
        } else {
            json_set(axc,"status",json_string("skip")); json_set(axc,"message",json_string("accessibility field absent from check_permissions"));
        }
        json_append(checks, axc);

        json_t *scr = json_obj_get(perms,"screen_recording");
        json_t *capt = json_obj_get(perms,"screen_recording_capturable");
        json_t *src = json_object();
        json_set(src,"name",json_string("tcc_screen_recording"));
        if (_json_is_true(scr)) {
            bool capturable = _json_is_true(capt);
            if (!capturable) {
                json_set(src,"status",json_string("fail")); json_set(src,"message",json_string("Screen Recording granted but not capturable."));
                json_set(src,"hint",json_string("Screen Recording permission may need a restart of CuaDriver or a re-grant in System Settings."));
                json_t *d=json_object(); json_set(d,"screen_recording",json_bool(true)); json_set(d,"screen_recording_capturable",json_bool(false)); json_set(src,"data",d);
            } else {
                json_set(src,"status",json_string("pass")); json_set(src,"message",json_string("Screen Recording is granted."));
                json_t *d=json_object(); json_set(d,"screen_recording",json_bool(true)); json_set(d,"screen_recording_capturable",json_bool(capturable)); json_set(src,"data",d);
            }
        } else if (_json_is_false(scr)) {
            json_set(src,"status",json_string("fail")); json_set(src,"message",json_string("Screen Recording is not granted."));
            json_set(src,"hint",json_string("Grant Screen Recording to CuaDriver in System Settings \xE2\x86\x92 Privacy & Security."));
            json_t *d=json_object(); json_set(d,"screen_recording",json_bool(false)); json_set(src,"data",d);
        } else if (strcmp(plat,"darwin")!=0) {
            char m[96]; snprintf(m,sizeof(m),"not applicable on %s",plat);
            json_set(src,"status",json_string("skip")); json_set(src,"message",json_string(m));
        } else {
            json_set(src,"status",json_string("skip")); json_set(src,"message",json_string("screen_recording field absent from check_permissions"));
        }
        json_append(checks, src);
    } else {
        json_t *ac=json_object(), *sc2=json_object();
        json_set(ac,"name",json_string("tcc_accessibility"));
        json_set(ac,"status",json_string(perm_err?"fail":"skip"));
        json_set(ac,"message",json_string(perm_err?perm_err:"check_permissions unavailable"));
        json_append(checks, ac);
        json_set(sc2,"name",json_string("tcc_screen_recording"));
        json_set(sc2,"status",json_string(perm_err?"fail":"skip"));
        json_set(sc2,"message",json_string(perm_err?perm_err:"check_permissions unavailable"));
        json_append(checks, sc2);
    }

    /* --- ax_capability --- */
    /* Python: list_ok = probes.get("list_apps_ok"); None/True/False */
    int list_ok = probes ? probes->list_apps_ok : -1;
    const char *list_err = probes ? probes->list_apps_error : NULL;
    int list_count = probes ? probes->list_apps_count : -1;
    bool ax_granted = _json_is_true(json_obj_get(perms,"accessibility"));
    json_t *axc2 = json_object();
    json_set(axc2,"name",json_string("ax_capability"));
    if (list_ok == 1) {
        if (list_count >= 0) {
            char m[128]; snprintf(m,sizeof(m),"list_apps succeeded (%d apps)",list_count);
            json_set(axc2,"status",json_string("pass")); json_set(axc2,"message",json_string(m));
        } else {
            json_set(axc2,"status",json_string("pass")); json_set(axc2,"message",json_string("list_apps succeeded"));
        }
    } else if (list_ok == 0) {
        const char *msg = list_err?list_err:(ax_granted?"list_apps failed despite accessibility grant":"list_apps failed");
        json_set(axc2,"status",json_string("fail")); json_set(axc2,"message",json_string(msg));
    } else if (ax_granted) {
        json_set(axc2,"status",json_string("pass")); json_set(axc2,"message",json_string("inferred from accessibility grant (list_apps not probed)"));
    } else {
        json_set(axc2,"status",json_string("skip")); json_set(axc2,"message",json_string("not probed"));
    }
    json_append(checks, axc2);

    /* --- health_report_path (skip) --- */
    char reason_short[256];
    const char *r0 = (reason && reason[0])?reason:"health_report unavailable";
    if (strlen(r0)>160) snprintf(reason_short,sizeof(reason_short),"%.157s...",r0);
    else snprintf(reason_short,sizeof(reason_short),"%s",r0);
    json_t *hrp = json_object();
    json_set(hrp,"name",json_string("health_report_path"));
    json_set(hrp,"status",json_string("skip"));
    char hrmsg[320];
    snprintf(hrmsg,sizeof(hrmsg),"fallback composite (cua-driver 0.10 unclassified health_report); cause: %s",reason_short);
    json_set(hrp,"message",json_string(hrmsg));
    json_append(checks, hrp);

    /* --- optional cli_doctor --- */
    if (probes && probes->cli_doctor_text) {
        json_t *cd=json_object();
        json_set(cd,"name",json_string("cli_doctor"));
        /* Python: cli_ok = "[ok" in doctor_txt.lower() or "ok  ]" in doctor_txt */
        char lower[2048];
        size_t ln = strlen(probes->cli_doctor_text);
        if (ln >= sizeof(lower)) ln = sizeof(lower)-1;
        for (size_t i=0;i<ln;i++) lower[i]=(char)tolower((unsigned char)probes->cli_doctor_text[i]);
        lower[ln]='\0';
        bool cli_ok = (strstr(lower,"[ok")!=NULL)||(strstr(probes->cli_doctor_text,"ok  ]")!=NULL);
        json_set(cd,"status",json_string(cli_ok?"pass":"skip"));
        json_set(cd,"message",json_string(probes->cli_doctor_text));
        json_t *d=json_object(); json_set(d,"snippet",json_string(probes->cli_doctor_text)); json_set(cd,"data",d);
        json_append(checks, cd);
    }

    /* --- normalize stray statuses --- */
    for (size_t i=0;i<checks->c.count;i++){
        json_t *c=checks->c.items[i]; json_t *st=json_obj_get(c,"status");
        if (st&&st->type==JSON_STRING){
            const char *s=_jstr(st);
            if (s && strcmp(s,"pass")!=0&&strcmp(s,"fail")!=0&&strcmp(s,"skip")!=0)
                json_set(c,"status",json_string("fail"));
        }
    }

    /* --- overall --- */
    const char *bv_status=NULL;
    for (size_t i=0;i<checks->c.count;i++){
        json_t*cc=checks->c.items[i]; json_t*n=json_obj_get(cc,"name");
        if (n&&n->type==JSON_STRING){
            const char *nm=_jstr(n);
            if (nm&&strcmp(nm,"binary_version")==0){
                json_t*s=json_obj_get(cc,"status"); if(s&&_jstr(s))bv_status=_jstr(s); break;
            }
        }
    }
    bool binary_ok = bv_status && strcmp(bv_status,"pass")==0;
    const char *ax_status=NULL;
    for (size_t i=0;i<checks->c.count;i++){ json_t*cc=checks->c.items[i]; json_t*n=json_obj_get(cc,"name");
        if (n&&_jstr(n)&&strcmp(_jstr(n),"tcc_accessibility")==0){ json_t*s=json_obj_get(cc,"status"); if(s&&s->type==JSON_STRING)ax_status=_jstr(s); break; }
    }
    bool tcc_ok = (!ax_status)||strcmp(ax_status,"pass")==0||strcmp(ax_status,"skip")==0;
    int fail_count=0;
    for (size_t i=0;i<checks->c.count;i++){ json_t*s=json_obj_get(checks->c.items[i],"status"); if(s&&_jstr(s)&&strcmp(_jstr(s),"fail")==0)fail_count++; }
    const char *overall;
    if (!binary_ok) overall="failed";
    else if (tcc_ok&&fail_count==0) overall="ok";
    else if (tcc_ok&&fail_count>0) overall="degraded";
    else overall="degraded";

    json_set(report,"driver_version",json_string(
        (probes&&probes->init_version)?probes->init_version:
        (probes&&probes->binary_version)?probes->binary_version:"?"));
    json_set(report,"overall",json_string(overall));
    json_set(report,"fallback",json_bool(true));
    json_set(report,"fallback_reason",json_string((reason&&reason[0])?reason:"health_report unavailable"));
    free(driver_version);
    return report;
}

/* PoP: _drive_health_report_or_fallback @ tools/computer_use/doctor.py:_drive_health_report_or_fallback */
json_t *cua_doctor_health_report_or_fallback(json_t *maybe_report, bool report_available,
                                             const char *unavail_reason, const cua_doctor_probes_t *probes,
                                             const char *binary, const char *reason, char **out_msg)
{
    (void)out_msg; (void)unavail_reason;
    if (report_available && maybe_report) return json_copy(maybe_report);
    return cua_doctor_compose_fallback_report(binary, probes,
        unavail_reason?unavail_reason:reason, 12.0);
}

/* ── Text rendering ──────────────────────────────────────── */

/* PoP: _print_text_report @ tools/computer_use/doctor.py:_print_text_report */
void cua_doctor_print_text_report(FILE *out, const json_t *report, bool color, const json_t *identity)
{
    if (!out||!report) return;
    const char *platform = _jstr(json_obj_get(report,"platform")) ? _jstr(json_obj_get(report,"platform")) : "?";
    const char *report_v = _jstr(json_obj_get(report,"driver_version")) ? _jstr(json_obj_get(report,"driver_version")) : "?";
    const char *overall = _jstr(json_obj_get(report,"overall")) ? _jstr(json_obj_get(report,"overall")) : "?";
    const char *cli_v = NULL; bool mismatch=false;
    if (identity) { cli_v=_jstr(json_obj_get(identity,"cli_version")); mismatch=json_get_bool(identity,"version_mismatch",false); }
    char *hdr_v = (cli_v&&cli_v[0])?strdup(cli_v):strdup(report_v);

    const char *glyph = glyph_for_overall(overall);
    char rb[8]="",yb[8]="",gb[8]="",ab[8]="",db[8]="",col_for[8]="";
    if (color) {
        strcpy(rb,"\033[31m");strcpy(yb,"\033[33m");strcpy(gb,"\033[32m");
        strcpy(ab,"\033[0m");strcpy(db,"\033[2m");
        const char *cf = (strcmp(overall,"failed")==0)?rb:(strcmp(overall,"degraded")==0)?yb:gb;
        strcpy(col_for, cf);
    }
    fprintf(out, "%s cua-driver %s on %s — %s%s%s\n", glyph, hdr_v, platform, col_for, overall, ab);
    /* Python: if identity.get("resolved_binary") (truthy) */
    if (identity) {
        const char *rb_str = _jstr(json_obj_get(identity,"resolved_binary"));
        if (rb_str && rb_str[0]) {
            fprintf(out, "  %sbinary: %s%s\n", db, rb_str, ab);
        }
    }
    /* Python: only when cli_v and report_v and report_v not in cli_v and cli_v not in report_v */
    if (cli_v&&cli_v[0]&&report_v&&report_v[0] &&
        strstr(cli_v, report_v)==NULL && strstr(report_v, cli_v)==NULL) {
        fprintf(out,"  %s--version: %s%s\n", db, cli_v, ab);
        fprintf(out,"  %shealth_report.driver_version: %s%s\n", db, report_v, ab);
    }
    if (mismatch) {
        const char *w=color?yb:"";
        fprintf(out,"  %s⚠️ version mismatch: health_report says '%s' but binary --version is '%s'%s\n", w, report_v, cli_v, ab);
        fprintf(out,"  %s→ trust --version / packages/current for debugging; health_report's binary_version check can lag on Windows%s\n", db, ab);
    }
    json_t *checks = json_obj_get(report,"checks");
    if (checks && checks->type==JSON_ARRAY) {
        for (size_t i=0;i<checks->c.count;i++){
            json_t *c=checks->c.items[i];
            const char *name_n = _jstr(json_obj_get(c,"name"));
            const char *status_n = _jstr(json_obj_get(c,"status"));
            const char *msg_n = _jstr(json_obj_get(c,"message"));
            const char *name=name_n?name_n:"?";
            const char *status=status_n?status_n:"?";
            const char *g=glyph_for_status(status);
            const char *sc=""; if(color) sc=(strcmp(status,"pass")==0)?gb:(strcmp(status,"fail")==0)?rb:(strcmp(status,"skip")==0)?db:"";
            fprintf(out,"  %s %s%s: %s%s\n", g, sc, name, msg_n?msg_n:"", ab);
            json_t *hint=json_obj_get(c,"hint");
            if (hint&&hint->type==JSON_STRING) fprintf(out,"      → %s%s%s\n", db, hint->str_val, ab);
            json_t *data=json_obj_get(c,"data");
            if (data&&data->type==JSON_OBJECT&&data->c.count>0){
                for (size_t k=0;k<data->c.count;k++){
                    const char *key=data->c.keys[k]; json_t *val=data->c.items[k];
                    /* Python: rendered = value if not isinstance(value, (dict,list))
                     * else json.dumps(value).  str(True)=True, str("s")=s, str(None)=None */
                    char *rendered = NULL;
                    if (val->type==JSON_BOOL) { rendered = strdup(val->bool_val ? "True":"False"); }
                    else if (val->type==JSON_NULL) { rendered = strdup("None"); }
                    else if (val->type==JSON_STRING) { rendered = strdup(val->str_val); }
                    else if (val->type==JSON_NUMBER) { /* Python int/float repr */
                        char buf[64]; snprintf(buf,sizeof(buf), "%g", val->num_val);
                        rendered = strdup(buf);
                    } else if (val->type==JSON_ARRAY || val->type==JSON_OBJECT) {
                        rendered = json_serialize(val);
                    }
                    fprintf(out,"      %s%s=%s%s\n", db, key, rendered?rendered:(val->type==JSON_NULL?"None":"null"), ab);
                    free(rendered);
                }
            }
        }
    }
    free(hdr_v);
}
