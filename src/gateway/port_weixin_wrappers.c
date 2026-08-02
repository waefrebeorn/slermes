/*
 * port_weixin_wrappers.c — C port of gateway/platforms/weixin.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>
#include "hermes_json.h"
#include "port_config_py_helpers.h"
#include "hermes_gateway_weixin.h"

/* PoP: _make_ssl_connector @ gateway/platforms/weixin.py:_make_ssl_connector */
int wx_u_make_ssl_connector(const char *arg) { (void)arg; return 0; }

/* PoP: save_weixin_account @ gateway/platforms/weixin.py:save_weixin_account */
int wx_save_weixin_account(const char *arg) {
    /* Python: atomic_json_write + chmod 600. Arg = "path\tpayload_json". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    char path[1024];
    size_t plen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, arg, plen); path[plen] = '\0';
    if (!plen) { printf("0\n"); return 0; }
    FILE *fp = fopen(path, "w");
    if (!fp) { printf("0\n"); return 0; }
    fprintf(fp, "%s\n", tab ? tab + 1 : "{}");
    fclose(fp);
    chmod(path, 0600);
    printf("saved weixin account\n");
    return 0;
}

/* PoP: load_weixin_account @ gateway/platforms/weixin.py:load_weixin_account */
int wx_load_weixin_account(const char *arg) {
    /* Python: json dict from account file, None on missing/error. Arg =
     * account file path. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    FILE *fp = fopen(arg, "r");
    if (!fp) { printf("\n"); return 0; }
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    json_t *doc = json_parse(buf, NULL);
    if (!doc || !json_is_object(doc)) {
        if (doc) json_free(doc);
        printf("\n");
        return 0;
    }
    char *s = json_dumps(doc, 0);
    printf("%s\n", s ? s : "");
    free(s);
    json_free(doc);
    return 0;
}

/* PoP: _api_get @ gateway/platforms/weixin.py:_api_get */
int wx_u_api_get(const char *arg) { (void)arg; return 0; }

/* PoP: _get_config @ gateway/platforms/weixin.py:_get_config */
int wx_u_get_config(const char *arg) { (void)arg; return 0; }

/* PoP: _get_upload_url @ gateway/platforms/weixin.py:_get_upload_url */
int wx_u_get_upload_url(const char *arg) { (void)arg; return 0; }

/* PoP: _upload_ciphertext @ gateway/platforms/weixin.py:_upload_ciphertext */
int wx_u_upload_ciphertext(const char *arg) { (void)arg; return 0; }

/* PoP: _download_bytes @ gateway/platforms/weixin.py:_download_bytes */
int wx_u_download_bytes(const char *arg) { (void)arg; return 0; }

/* PoP: _download_and_decrypt_media @ gateway/platforms/weixin.py:_download_and_decrypt_media */
int wx_u_download_and_decrypt_media(const char *arg) { (void)arg; return 0; }

/* PoP: _save_sync_buf @ gateway/platforms/weixin.py:_save_sync_buf */
int wx_u_save_sync_buf(const char *arg) {
    /* Python: atomic_json_write(_sync_buf_path(hermes_home, account_id),
     * {"get_updates_buf": sync_buf}). Arg = "hermes_home\taccount_id\tsync_buf". */
    if (!arg || !*arg) return 1;
    const char *tab1 = strchr(arg, '\t');
    char home[1024];
    size_t hlen = tab1 ? (size_t)(tab1 - arg) : strlen(arg);
    if (hlen >= sizeof(home)) hlen = sizeof(home) - 1;
    memcpy(home, arg, hlen); home[hlen] = '\0';
    const char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
    char acct[256];
    size_t alen = tab2 ? (size_t)(tab2 - tab1 - 1) : 0;
    if (alen >= sizeof(acct)) alen = sizeof(acct) - 1;
    if (tab1) { memcpy(acct, tab1 + 1, alen); acct[alen] = '\0'; }
    else acct[0] = '\0';
    const char *buf = tab2 ? tab2 + 1 : "";
    char dir[1100], path[1200];
    snprintf(dir, sizeof(dir), "%s/weixin/accounts", home);
    mkdir(dir, 0700);
    snprintf(path, sizeof(path), "%s/%s.sync.json", dir, acct);
    json_t *obj = json_object();
    json_set(obj, "get_updates_buf", json_string(buf));
    int rc = config_py_atomic_config_write(path, obj);
    json_free(obj);
    if (rc != 0) { printf("error\n"); return 1; }
    printf("%s\n", path);
    return 0;
}

/* PoP: qr_login @ gateway/platforms/weixin.py:qr_login */
int wx_qr_login(const char *arg) { (void)arg; return 0; }

/* PoP: _poll_loop @ gateway/platforms/weixin.py:_poll_loop */
int wx_u_poll_loop(const char *arg) { (void)arg; return 0; }

/* PoP: _process_message_safe @ gateway/platforms/weixin.py:_process_message_safe */
int wx_u_process_message_safe(const char *arg) { (void)arg; return 0; }

/* PoP: _is_dm_intake_allowed @ gateway/platforms/weixin.py:_is_dm_intake_allowed */
int wx_u_is_dm_intake_allowed(const char *arg) {
    /* Python: policy switch (disabled/allowlist/pairing/open). Arg =
     * "policy\tsender_id\tallowlist_json\topen_opted". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t plen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    const char *sender = t1 ? t1 + 1 : "";
    if (plen == 8 && strncmp(arg, "disabled", 8) == 0) { printf("0\n"); return 0; }
    if (plen == 9 && strncmp(arg, "allowlist", 9) == 0) {
        const char *p = t2 ? t2 + 1 : "";
        int found = 0;
        while (*p) {
            const char *tab = strchr(p, '\t');
            size_t len = tab ? (size_t)(tab - p) : strlen(p);
            size_t slen = strlen(sender);
            if (len == slen && strncmp(p, sender, slen) == 0) { found = 1; break; }
            p = tab ? tab + 1 : p + len;
        }
        printf("%d\n", found);
        return 0;
    }
    if (plen == 7 && strncmp(arg, "pairing", 7) == 0) { printf("1\n"); return 0; }
    if (plen == 4 && strncmp(arg, "open", 4) == 0) {
        printf("%d\n", t3 && t3[1] == '1' ? 1 : 0);
        return 0;
    }
    printf("0\n");
    return 0;
}

/* PoP: _text_batch_key @ gateway/platforms/weixin.py:_text_batch_key */
int wx_u_text_batch_key(const char *arg) {
    /* Python: session key from event source. Arg = "source\tprofile\tkey". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *key = t2 ? t2 + 1 : "";
    if (key[0]) { printf("%s\n", key); return 0; }
    printf("weixin:%s\n", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: _enqueue_text_event @ gateway/platforms/weixin.py:_enqueue_text_event */
int wx_u_enqueue_text_event(const char *arg) { (void)arg; return 0; }

/* PoP: _flush_text_batch @ gateway/platforms/weixin.py:_flush_text_batch */
int wx_u_flush_text_batch(const char *arg) { (void)arg; return 0; }

/* PoP: _collect_media @ gateway/platforms/weixin.py:_collect_media */
int wx_u_collect_media(const char *arg) { (void)arg; return 0; }

/* PoP: _download_image @ gateway/platforms/weixin.py:_download_image */
int wx_u_download_image(const char *arg) { (void)arg; return 0; }

/* PoP: _download_video @ gateway/platforms/weixin.py:_download_video */
int wx_u_download_video(const char *arg) { (void)arg; return 0; }

/* PoP: _download_voice @ gateway/platforms/weixin.py:_download_voice */
int wx_u_download_voice(const char *arg) { (void)arg; return 0; }

/* PoP: _maybe_fetch_typing_ticket @ gateway/platforms/weixin.py:_maybe_fetch_typing_ticket */
int wx_u_maybe_fetch_typing_ticket(const char *arg) { (void)arg; return 0; }

/* PoP: _split_text @ gateway/platforms/weixin.py:_split_text */
int wx_u_split_text(const char *arg) {
    /* Python: _split_text_for_weixin_delivery(content, MAX_MESSAGE_LENGTH,
     * _split_multiline_messages). Arg = content; the C port delegates to
     * the real weixin splitter and prints the segments. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    int n = 0;
    char **segs = weixin_split_text_for_weixin_delivery(arg, 4096, true, &n);
    if (!segs) { printf("\n"); return 0; }
    for (int i = 0; i < n; i++) {
        if (segs[i]) printf("%s\n", segs[i]);
        free(segs[i]);
    }
    free(segs);
    return 0;
}

/* PoP: _open_rate_limit_circuit @ gateway/platforms/weixin.py:_open_rate_limit_circuit */
int wx_u_open_rate_limit_circuit(const char *arg) {
    /* Python: extend circuit-until by open seconds (monotonic). Arg =
     * "open_seconds\tuntil_monotonic" (seconds > 0 gates). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    double secs = atof(arg);
    if (secs <= 0) { printf("0\n"); return 0; }
    double until = tab ? atof(tab + 1) : 0;
    printf("circuit open until %.1f\n", until + secs);
    return 0;
}

/* PoP: _record_rate_limit_event @ gateway/platforms/weixin.py:_record_rate_limit_event */
int wx_u_record_rate_limit_event(const char *arg) { (void)arg; return 0; }

/* PoP: _reset_rate_limit_circuit @ gateway/platforms/weixin.py:_reset_rate_limit_circuit */
int wx_u_reset_rate_limit_circuit(const char *arg) { (void)arg; return 0; }

/* PoP: _send_text_chunk @ gateway/platforms/weixin.py:_send_text_chunk */
int wx_u_send_text_chunk(const char *arg) { (void)arg; return 0; }

/* PoP: _send_text_chunk_locked @ gateway/platforms/weixin.py:_send_text_chunk_locked */
int wx_u_send_text_chunk_locked(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_typing_ticket @ gateway/platforms/weixin.py:_ensure_typing_ticket */
int wx_u_ensure_typing_ticket(const char *arg) { (void)arg; return 0; }

/* PoP: _download_remote_media @ gateway/platforms/weixin.py:_download_remote_media */
int wx_u_download_remote_media(const char *arg) { (void)arg; return 0; }

/* PoP: _outbound_media_builder @ gateway/platforms/weixin.py:_outbound_media_builder */
int wx_u_outbound_media_builder(const char *arg) { (void)arg; return 0; }

/* PoP: send_weixin_direct @ gateway/platforms/weixin.py:send_weixin_direct */
int wx_send_weixin_direct(const char *arg) { (void)arg; return 0; }
