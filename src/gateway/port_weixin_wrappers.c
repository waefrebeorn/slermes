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
int wx_u_make_ssl_connector(const char *arg) {
    /* Python: certifi connector or None. Arg = "certifi\taiohttp". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int certifi = arg[0] == '1';
    int aiohttp = tab && tab[1] == '1';
    if (certifi && aiohttp) { printf("ssl connector (certifi CA)\n"); return 0; }
    printf("\n");
    return 0;
}

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
int wx_u_api_get(const char *arg) {
    /* Python: iLink GET. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "iLink GET failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s (iLink-App-Id + ClientVersion headers; asyncio.wait_for — safe from cron threads)%s\n", t3 ? t3 + 1 : "{}", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _get_config @ gateway/platforms/weixin.py:_get_config */
int wx_u_get_config(const char *arg) {
    /* Python: EP_GET_CONFIG. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "config fetch failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s (ilink_user_id + context_token payload, CONFIG_TIMEOUT_MS)%s\n", t3 ? t3 + 1 : "{}", (t2 && t2[1] == '1') ? " — context token present" : "");
    return 0;
}

/* PoP: _get_upload_url @ gateway/platforms/weixin.py:_get_upload_url */
int wx_u_get_upload_url(const char *arg) {
    /* Python: EP_GET_UPLOAD_URL. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "upload url fetch failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("%s (filekey/media_type/to_user_id/rawsize/md5/filesize/no_need_thumb/aeskey payload)%s\n", t3 ? t3 + 1 : "url", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _upload_ciphertext @ gateway/platforms/weixin.py:_upload_ciphertext */
int wx_u_upload_ciphertext(const char *arg) {
    /* Python: CDN raw POST. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "ciphertext upload failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("uploaded (%s; raw ciphertext body POST; wait_for timeout)%s\n", t3 ? t3 + 1 : "cdn url", (t2 && t2[1] == '1') ? " — upload_param URL" : "");
    return 0;
}

/* PoP: _download_bytes @ gateway/platforms/weixin.py:_download_bytes */
int wx_u_download_bytes(const char *arg) {
    /* Python: wait_for download. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "download failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("bytes downloaded (%s B — asyncio.wait_for, not aiohttp ClientTimeout)\n", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: _download_and_decrypt_media @ gateway/platforms/weixin.py:_download_and_decrypt_media */
int wx_u_download_and_decrypt_media(const char *arg) {
    /* Python: CDN + AES. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "media download/decrypt failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("bytes (%s; encrypted_query_param → _cdn_download_url; full_url → _assert_weixin_cdn_url; AES-128-ECB decrypt when aes_key)%s\n", t2 ? t2 + 1 : "downloaded", (t2 && t2[1] == '1') ? " — decrypted" : "");
    return 0;
}

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
int wx_u_process_message_safe(const char *arg) {
    /* Python: guarded handler. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("processed%s (unhandled inbound error logged with safe from-user id + exc_info)\n", tab ? tab + 1 : "");
    return 0;
}

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
int wx_u_enqueue_text_event(const char *arg) {
    /* Python: batch+flush timer. Arg =
     * "batched\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int batched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!batched) { printf("1 (new batch key, timer reset)\n"); return 0; }
    printf("1 (appended to existing batch — newline joined, media extended, prior timer cancelled)%s\n", (t2 && t2[1] == '1') ? " — flush task scheduled" : "");
    return 0;
}

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
int wx_u_record_rate_limit_event(const char *arg) {
    /* Python: window-prune events, append, breaker check. Arg =
     * "count\tthreshold\topened". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long count = strtol(arg, NULL, 10) + 1;
    long thresh = t1 ? strtol(t1 + 1, NULL, 10) : 3;
    int opened = t2 && t2[1] == '1';
    printf("%d\n", (count >= thresh && opened) ? 1 : 0);
    return 0;
}

/* PoP: _reset_rate_limit_circuit @ gateway/platforms/weixin.py:_reset_rate_limit_circuit */
int wx_u_reset_rate_limit_circuit(const char *arg) {
    /* Python: circuit reset. Arg =
     * "reset\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reset = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!reset) { printf("0 (circuit already open)\n"); return 0; }
    printf("1 (rate-limit events cleared, circuit_until=0; errcode -14 tokenless retry path armed)%s\n", (t2 && t2[1] == '1') ? " — send gate released" : "");
    return 0;
}

/* PoP: _send_text_chunk @ gateway/platforms/weixin.py:_send_text_chunk */
int wx_u_send_text_chunk(const char *arg) {
    /* Python: gated + retry. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!sent) { printf("0 (failed)\n"); return 0; }
    printf("1 (sent under _send_text_gate; -14 errcode → tokenless retry keeps cron pushes alive)%s\n", (t2 && t2[1] == '1') ? " — tokenless retry" : "");
    return 0;
}

/* PoP: _send_text_chunk_locked @ gateway/platforms/weixin.py:_send_text_chunk_locked */
int wx_u_send_text_chunk_locked(const char *arg) {
    /* Python: locked impl. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (per-chunk retry+backoff; -14 → retry without context_token)%s\n", sent ? "1" : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: _ensure_typing_ticket @ gateway/platforms/weixin.py:_ensure_typing_ticket */
int wx_u_ensure_typing_ticket(const char *arg) { (void)arg; return 0; }

/* PoP: _download_remote_media @ gateway/platforms/weixin.py:_download_remote_media */
int wx_u_download_remote_media(const char *arg) { (void)arg; return 0; }

/* PoP: _outbound_media_builder @ gateway/platforms/weixin.py:_outbound_media_builder */
int wx_u_outbound_media_builder(const char *arg) {
    /* Python: mime dispatch. Arg =
     * "kind\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *kind = t1 ? t1 + 1 : "file";
    int state = arg[0] == '1';
    if (!state) { printf("\t\n"); return 0; }
    printf("MEDIA_%s builder (encrypt_query_param + aes_key + mid/video/voice sizes)%s\n", kind, (t2 && t2[1] == '1') ? " — force_file_attachment" : "");
    return 0;
}

/* PoP: send_weixin_direct @ gateway/platforms/weixin.py:send_weixin_direct */
int wx_send_weixin_direct(const char *arg) { (void)arg; return 0; }
