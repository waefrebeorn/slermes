/*
 * onboard.c - C11 port of upstream plugins/platforms/qqbot/onboard.py
 * (gate w24_onboard_d18acb).
 *
 * PoP: w24_onboard_d18acb_url @ plugins/platforms/qqbot/onboard.py:build_connect_url
 * PoP: w24_onboard_d18acb_poll_result @ plugins/platforms/qqbot/onboard.py:_poll_bind_result
 * PoP: w24_onboard_d18acb_portal_error @ plugins/platforms/qqbot/onboard.py:_portal_post
 * PoP: w24_onboard_d18acb_create_result @ plugins/platforms/qqbot/onboard.py:_create_bind_task
 * PoP: w24_onboard_d18acb_expiry @ plugins/platforms/qqbot/onboard.py:qr_register
 * PoP: w24_onboard_d18acb_status_name @ plugins/platforms/qqbot/onboard.py:BindStatus
 * PoP: w24_onboard_d18acb_constants @ plugins/platforms/qqbot/onboard.py:ONBOARD_*,_MAX_REFRESHES,QR_URL_TEMPLATE
 *
 * What is ported is the pure part of the scan-to-configure flow: the percent
 * escaped QR target URL, the bind-task response checks (a non-zero retcode
 * raises the portal message, a missing task_id raises the fixed message), the
 * poll response parse into (status, app_id, encrypted secret, openid), the
 * expired-QR refresh decision bounded by _MAX_REFRESHES, and the constants the
 * flow is tuned with.
 *
 * Seams, deliberately not ported: httpx.Client (the portal POST), qrcode's
 * terminal renderer, time.monotonic/time.sleep, decrypt_secret and the AES bind
 * key.  Each becomes a value handed in: the response bodies, the expiry counter.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_onboard_d18acb.h"

/* .constants */
static const char *const ON_PORTAL_HOST = "q.qq.com";
static const char *const ON_CREATE_PATH = "/lite/create_bind_task";
static const char *const ON_POLL_PATH = "/lite/poll_bind_result";
static const char *const ON_QR_TEMPLATE =
    "https://q.qq.com/qqbot/openclaw/connect.html?task_id={task_id}&_wv=2&source=hermes";
static const double ON_API_TIMEOUT = 10.0;
static const double ON_POLL_INTERVAL = 2.0;

/* BindStatus: NONE, PENDING, COMPLETED, EXPIRED */
static const char *const ON_STATUS_NAME[W24_ONBOARD_D18ACB_STATUS_COUNT] = {
    "NONE", "PENDING", "COMPLETED", "EXPIRED",
};

static int on_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str(): None -> "None", the scalar shapes verbatim; NULL for a container. */
static char *on_pystr(const json_t *v) {
    char *buf;
    double d;

    if (!v || v->type == JSON_NULL) {
        buf = (char *)malloc(5);
        if (buf) strcpy(buf, "None");
        return buf;
    }
    if (v->type == JSON_STRING) {
        size_t n = strlen(v->str_val ? v->str_val : "");
        buf = (char *)malloc(n + 1);
        if (buf) memcpy(buf, v->str_val ? v->str_val : "", n + 1);
        return buf;
    }
    if (v->type == JSON_BOOL) {
        const char *t = v->bool_val ? "True" : "False";
        buf = (char *)malloc(strlen(t) + 1);
        if (buf) strcpy(buf, t);
        return buf;
    }
    if (v->type == JSON_NUMBER) {
        buf = (char *)malloc(40);
        if (!buf) return NULL;
        d = v->num_val;
        if (d == (double)(long long)d && d > -1e17 && d < 1e17) snprintf(buf, 40, "%.0f", d);
        else snprintf(buf, 40, "%.15g", d);
        return buf;
    }
    return NULL;
}

/* A Python int view of the value (bool included), or 0 when it is not one. */
static int on_int(const json_t *v, long *out) {
    double d;
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (d != (double)(long long)d) return 0;   /* IntEnum(1.0) is a ValueError */
    *out = (long)d;
    return 1;
}

/* urllib.parse.quote(s) with the default safe='/'. */
static char *on_quote(const char *s) {
    static const char *hex = "0123456789ABCDEF";
    size_t i, n = strlen(s), cap = n * 3 + 1, len = 0;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        int safe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                   c == '_' || c == '.' || c == '-' || c == '~' || c == '/';
        if (safe) {
            out[len++] = (char)c;
        } else {
            out[len++] = '%';
            out[len++] = hex[c >> 4];
            out[len++] = hex[c & 0x0f];
        }
    }
    out[len] = '\0';
    return out;
}

json_t *w24_onboard_d18acb_url(const json_t *task_id) {
    char *quoted, *buf;
    size_t n;
    json_t *out;

    if (!task_id || task_id->type != JSON_STRING) return json_null();  /* quote() seam */
    quoted = on_quote(task_id->str_val ? task_id->str_val : "");
    if (!quoted) return json_null();
    n = strlen(ON_QR_TEMPLATE) + strlen(quoted) + 1;
    buf = (char *)malloc(n);
    if (!buf) { free(quoted); return json_null(); }
    /* QR_URL_TEMPLATE.format(task_id=quote(task_id)) */
    {
        const char *mark = strstr(ON_QR_TEMPLATE, "{task_id}");
        size_t head = (size_t)(mark - ON_QR_TEMPLATE);
        memcpy(buf, ON_QR_TEMPLATE, head);
        strcpy(buf + head, quoted);
        strcat(buf, mark + strlen("{task_id}"));
    }
    out = json_string(buf);
    free(buf);
    free(quoted);
    return out;
}

/* The ``data`` body of a portal response: response.get("data", {}).  An absent
 * body is the empty mapping; a non-mapping body is the AttributeError seam. */
static int on_data_body(const json_t *data, const json_t **body) {
    const json_t *inner;

    *body = NULL;
    if (!data || data->type != JSON_OBJECT) return 0;
    inner = json_obj_get(data, "data");
    if (!inner) return 2;                             /* .get("data", {}) */
    if (inner->type != JSON_OBJECT) return 0;
    *body = inner;
    return 1;
}

json_t *w24_onboard_d18acb_poll_result(const json_t *data) {
    const json_t *body, *appid_v;
    int got;
    long status = 0;
    char *appid;
    json_t *out;

    got = on_data_body(data, &body);
    if (!got) return json_null();
    if (got == 1) {
        if (json_obj_get(body, "status")) {
            if (!on_int(json_obj_get(body, "status"), &status)) return json_null();  /* ValueError */
        }
        if (status < 0 || status >= W24_ONBOARD_D18ACB_STATUS_COUNT) return json_null();
    }
    appid_v = got == 1 ? json_obj_get(body, "bot_appid") : NULL;
    appid = appid_v ? on_pystr(appid_v) : NULL;
    if (!appid) {
        appid = (char *)malloc(1);                    /* .get("bot_appid", "") */
        if (!appid) return json_null();
        appid[0] = '\0';
    }
    out = json_object();
    json_set(out, "status", json_number((double)status));
    json_set(out, "status_name", json_string(ON_STATUS_NAME[status]));
    json_set(out, "bot_appid", json_string(appid));
    json_set(out, "bot_encrypt_secret",
             got == 1 && json_obj_get(body, "bot_encrypt_secret")
                 ? json_copy(json_obj_get(body, "bot_encrypt_secret")) : json_string(""));
    json_set(out, "user_openid",
             got == 1 && json_obj_get(body, "user_openid")
                 ? json_copy(json_obj_get(body, "user_openid")) : json_string(""));
    free(appid);
    return out;
}

json_t *w24_onboard_d18acb_portal_error(const json_t *data, const json_t *fail_msg) {
    const json_t *retcode, *msg;
    int zero = 0;
    json_t *out;

    if (!data || data->type != JSON_OBJECT) return json_null();  /* .get() seam */
    retcode = json_obj_get(data, "retcode");
    /* if data.get("retcode") != 0: raise RuntimeError(data.get("msg", fail_msg)) */
    if (retcode && retcode->type == JSON_NUMBER && retcode->num_val == 0.0) zero = 1;
    if (retcode && retcode->type == JSON_BOOL && !retcode->bool_val) zero = 1;
    if (zero) return json_null();
    msg = json_obj_get(data, "msg");
    out = json_object();
    json_set(out, "error", msg ? json_copy(msg) : (fail_msg ? json_copy(fail_msg) : json_null()));
    return out;
}

json_t *w24_onboard_d18acb_create_result(const json_t *data) {
    const json_t *body, *task_id;
    json_t *out;

    if (!data || data->type != JSON_OBJECT) return json_null();  /* .get() seam */
    /* task_id = (data.get("data") or {}).get("task_id") */
    body = json_obj_get(data, "data");
    if (body && body->type != JSON_OBJECT) return json_null();    /* .get() seam */
    task_id = body ? json_obj_get(body, "task_id") : NULL;
    if (!on_truthy(task_id)) {
        out = json_object();
        json_set(out, "error", json_string("create_bind_task: missing task_id in response"));
        return out;
    }
    out = json_object();
    json_set(out, "task_id", json_copy(task_id));
    return out;
}

json_t *w24_onboard_d18acb_expiry(const json_t *refresh_count) {
    double rc;
    json_t *out;

    /* if refresh_count >= _MAX_REFRESHES: give up (the comparison is numeric) */
    if (!refresh_count) return json_null();
    if (refresh_count->type == JSON_BOOL) rc = refresh_count->bool_val ? 1.0 : 0.0;
    else if (refresh_count->type == JSON_NUMBER) rc = refresh_count->num_val;
    else return json_null();
    if (rc >= (double)(W24_ONBOARD_D18ACB_MAX_TASKS - 1)) {
        out = json_object();
        json_set(out, "action", json_string("give_up"));
        return out;
    }
    out = json_object();
    json_set(out, "action", json_string("refresh"));
    json_set(out, "refresh_number", json_number(rc + 1.0));
    return out;
}

json_t *w24_onboard_d18acb_status_name(const json_t *status) {
    long st;
    if (!on_int(status, &st)) return json_null();          /* BindStatus ValueError */
    if (st < 0 || st >= W24_ONBOARD_D18ACB_STATUS_COUNT) return json_null();
    return json_string(ON_STATUS_NAME[st]);
}

json_t *w24_onboard_d18acb_constants(void) {
    json_t *o = json_object();
    json_t *statuses = json_object();
    size_t i;

    json_set(o, "api_timeout", json_number(ON_API_TIMEOUT));
    json_set(o, "poll_interval", json_number(ON_POLL_INTERVAL));
    json_set(o, "create_path", json_string(ON_CREATE_PATH));
    json_set(o, "poll_path", json_string(ON_POLL_PATH));
    json_set(o, "portal_host", json_string(ON_PORTAL_HOST));
    json_set(o, "qr_url_template", json_string(ON_QR_TEMPLATE));
    json_set(o, "max_refreshes", json_number(W24_ONBOARD_D18ACB_MAX_TASKS - 1));
    json_set(o, "max_tasks", json_number(W24_ONBOARD_D18ACB_MAX_TASKS));
    for (i = 0; i < W24_ONBOARD_D18ACB_STATUS_COUNT; i++)
        json_set(statuses, ON_STATUS_NAME[i], json_number((double)i));
    json_set(o, "statuses", statuses);
    return o;
}
