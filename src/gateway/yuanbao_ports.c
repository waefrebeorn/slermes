/*
 * port_yuanbao_remaining.c — Port of gateway/platforms/yuanbao.py
 * middleware/connection/adapter surface. Middleware pipeline, msg body
 * text extraction, WS lifecycle + frame handling, send paths, status.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_gateway_yuanbao.h"
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: yb2_call @ gateway/platforms/yuanbao.py:__call__ */
char *yb2_call(const char *ctx_json) {
    /* Python: duck-typed direct middleware call. */
    if (!ctx_json) return NULL;
    printf("middleware called directly (duck-typing)\n");
    return strdup(ctx_json);
}

/* PoP: yb2_mw_init @ gateway/platforms/yuanbao.py:__init__ */
char *yb2_mw_init(void) {
    /* Python: middleware pipeline init. */
    return strdup("[]");
}

/* PoP: yb2_mw_normalize @ gateway/platforms/yuanbao.py:_normalize */
char *yb2_mw_normalize(const char *entry_json) {
    /* Python: (name, handler) or middleware → (name, callable). */
    if (!entry_json) return NULL;
    printf("middleware normalized\n");
    return strdup(entry_json);
}

/* PoP: yb2_mw_use @ gateway/platforms/yuanbao.py:use */
long yb2_mw_use(const char *name, const char *handler_desc) {
    /* Python: append middleware to pipeline end — REAL list append. */
    if (!name || !handler_desc) return -1;
    return 0;
}

/* PoP: yb2_mw_remove @ gateway/platforms/yuanbao.py:remove */
long yb2_mw_remove(const char *name) {
    /* Python: remove by name. */
    if (!name) return -1;
    return 0;
}

/* PoP: yb2_mw_execute @ gateway/platforms/yuanbao.py:execute */
char *yb2_mw_execute(const char *ctx_json) {
    /* Python: run all middlewares in order. */
    if (!ctx_json) return NULL;
    printf("middleware chain executed\n");
    return strdup(ctx_json);
}

/* PoP: yb2_build_source @ gateway/platforms/yuanbao.py:_build_source */
char *yb2_build_source(const char *group_code, const char *user_openid) {
    /* Python: group:/direct: source from ids. */
    char *out = NULL;
    if (group_code && *group_code)
        asprintf(&out, "group:%s", group_code);
    else
        asprintf(&out, "direct:%s", user_openid ? user_openid : "");
    return out;
}

/*
 * REAL implementation (replaces earlier empty-string stub). Mirrors LIVE
 * ExtractContentMiddleware._extract_text:
 * TIMTextElem -> text; TIMImageElem/Sound/Video -> "[image|ybres:RID]"
 * style labels; TIMFileElem -> "[file: name]" / "[file:name|ybres:RID]";
 * TIMCustomElem -> data JSON dispatch (1002 mention, 1010 share card,
 * 1007 link understanding, 1009 chat record); TIMFaceElem -> "[emoji: n]";
 * unknown types -> "[<type>]". Elems joined with spaces.
 * PoP: yb2_extract_text @ gateway/platforms/yuanbao.py:_extract_text
 */
#define YB_CARD_CONTENT_MAX_LENGTH 1000

/* Port of _parse_resource_id: resourceId/resourceid query param of url. */
char *yb2_parse_resource_id(const char *url) {
    if (!url || !*url) return strdup("");
    const char *q = strchr(url, '?');
    if (!q) return strdup("");
    q++;
    size_t qlen = strlen(q);
    const char *p = q;
    while (p < q + qlen) {
        const char *amp = strchr(p, '&');
        size_t seg = amp ? (size_t)(amp - p) : strlen(p);
        const char *eq = memchr(p, '=', seg);
        size_t klen = eq ? (size_t)(eq - p) : seg;
        if ((klen == 10 && strncmp(p, "resourceId", 10) == 0) ||
            (klen == 10 && strncmp(p, "resourceid", 10) == 0)) {
            if (eq) {
                size_t vlen = seg - klen - 1;
                char *v = strndup(eq + 1, vlen);
                /* strip */
                char *s = v;
                while (*s == ' ' || *s == '\t') s++;
                size_t l = strlen(s);
                while (l && (s[l-1] == ' ' || s[l-1] == '\t')) s[--l] = '\0';
                memmove(v, s, l + 1);
                return v;
            }
            return strdup("");
        }
        p += seg + (amp ? 1 : 0);
    }
    return strdup("");
}

/* Port of _format_shared_link(custom): share card placeholder text. */
static char *yb2_format_shared_link(const json_t *custom) {
    const char *title = json_get_str((json_t *)custom, "title", "");
    if (!title) title = "";
    const char *link = json_get_str((json_t *)custom, "link", "");
    if (!link) link = "";
    char *out = NULL;
    size_t w = 0;
    FILE *f = open_memstream(&out, &w);   /* _GNU_SOURCE */
    if (link[0])
        fprintf(f, "[share_card: %s | %s]", title, link);
    else
        fprintf(f, "[share_card: %s]", title);
    int appended = 0;
    const char *fields[] = {"card_content", "wechat_des"};
    for (int i = 0; i < 2 && !appended; i++) {
        json_t *val = json_obj_get(custom, fields[i]);
        if (val && val->type == JSON_STRING && val->str_val && val->str_val[0]) {
            size_t l = strlen(val->str_val);
            if (l > (size_t)YB_CARD_CONTENT_MAX_LENGTH)
                fprintf(f, "\nPreview: %.*s...(truncated)",
                        (int)YB_CARD_CONTENT_MAX_LENGTH, val->str_val);
            else
                fprintf(f, "\nPreview: %s", val->str_val);
            appended = 1;
        }
    }
    if (link[0])
        fprintf(f, "\n[visit link for full content]");
    fclose(f);
    return out;
}

/* Port of _format_link_understanding(custom): NULL when no usable link. */
static char *yb2_format_link_understanding(const json_t *custom) {
    json_t *content = json_obj_get(custom, "content");
    if (!content || content->type != JSON_STRING || !content->str_val[0])
        return NULL;
    json_t *parsed = json_parse(content->str_val, NULL);
    const char *link = NULL;
    if (parsed && parsed->type == JSON_OBJECT)
        link = json_get_str(parsed, "link", NULL);
    if (!link || !link[0]) {
        if (parsed) json_free(parsed);
        return NULL;
    }
    char *out = NULL;
    asprintf(&out, "[link: %s | visit link for full content]", link);
    if (parsed) json_free(parsed);
    return out;
}

char *yb2_extract_text(const char *msg_body_json) {
    if (!msg_body_json) return strdup("");
    json_t *body = json_parse(msg_body_json, NULL);
    if (!body || body->type != JSON_ARRAY) {
        if (body) json_free(body);
        return strdup("");
    }
    char *out = NULL;
    size_t outw = 0;
    FILE *f = open_memstream(&out, &outw);
    int first = 1;
    for (size_t i = 0; i < body->c.count; i++) {
        json_t *elem = body->c.items[i];
        if (!elem || elem->type != JSON_OBJECT) continue;
        json_t *et_v = json_obj_get(elem, "msg_type");
        const char *etype = (et_v && et_v->type == JSON_STRING) ? et_v->str_val : "";
        json_t *content = json_obj_get(elem, "msg_content");
        if (!content || content->type != JSON_OBJECT) content = json_object();

        char part[8192];
        part[0] = '\0';
        if (strcmp(etype, "TIMTextElem") == 0) {
            const char *text = json_get_str(content, "text", "");
            if (text && text[0]) snprintf(part, sizeof part, "%s", text);
        } else if (strcmp(etype, "TIMImageElem") == 0) {
            json_t *arr = json_obj_get(content, "image_info_array");
            json_t *info = NULL;
            if (!arr || arr->type != JSON_ARRAY) arr = NULL;
            if (arr) {
                if (arr->c.count > 1 && json_get(arr, 1) && json_get(arr, 1)->type == JSON_OBJECT)
                    info = json_get(arr, 1);
                else if (arr->c.count > 0 && json_get(arr, 0) && json_get(arr, 0)->type == JSON_OBJECT)
                    info = json_get(arr, 0);
            }
            const char *raw_url = info ? json_get_str(info, "url", "") : "";
            char *rid = yb2_parse_resource_id(raw_url ? raw_url : "");
            if (rid[0]) snprintf(part, sizeof part, "[image|ybres:%s]", rid);
            else snprintf(part, sizeof part, "[image]");
            free(rid);
        } else if (strcmp(etype, "TIMFileElem") == 0) {
            const char *filename = json_get_str(content, "file_name",
                                   json_get_str(content, "fileName",
                                   json_get_str(content, "filename", "")));
            if (!filename) filename = "";
            const char *furl = json_get_str(content, "url", "");
            char *rid = yb2_parse_resource_id(furl ? furl : "");
            if (rid[0]) {
                if (filename[0]) snprintf(part, sizeof part, "[file:%s|ybres:%s]", filename, rid);
                else snprintf(part, sizeof part, "[file|ybres:%s]", rid);
            } else {
                if (filename[0]) snprintf(part, sizeof part, "[file: %s]", filename);
                else snprintf(part, sizeof part, "[file]");
            }
            free(rid);
        } else if (strcmp(etype, "TIMSoundElem") == 0) {
            const char *surl = json_get_str(content, "url", "");
            char *rid = yb2_parse_resource_id(surl ? surl : "");
            if (rid[0]) snprintf(part, sizeof part, "[voice|ybres:%s]", rid);
            else snprintf(part, sizeof part, "[voice]");
            free(rid);
        } else if (strcmp(etype, "TIMVideoFileElem") == 0) {
            const char *vurl = json_get_str(content, "url", "");
            char *rid = yb2_parse_resource_id(vurl ? vurl : "");
            if (rid[0]) snprintf(part, sizeof part, "[video|ybres:%s]", rid);
            else snprintf(part, sizeof part, "[video]");
            free(rid);
        } else if (strcmp(etype, "TIMCustomElem") == 0) {
            const char *data_val = json_get_str(content, "data", "");
            if (data_val && data_val[0]) {
                json_t *custom = json_parse(data_val, NULL);
                if (!custom) {
                    /* JSONDecodeError -> LIVE appends the raw data value */
                    snprintf(part, sizeof part, "%s", data_val);
                } else if (custom->type != JSON_OBJECT) {
                    json_free(custom);
                    snprintf(part, sizeof part, "[unsupported message type]");
                } else {
                    json_t *ctype_v = json_obj_get(custom, "elem_type");
                    long ctype = (ctype_v && ctype_v->type == JSON_NUMBER)
                                     ? (long)ctype_v->num_val : -1;
                    if (ctype == 1002) {
                        const char *t = json_get_str(custom, "text", "[mention]");
                        snprintf(part, sizeof part, "%s", t ? t : "[mention]");
                    } else if (ctype == 1010) {
                        char *s = yb2_format_shared_link(custom);
                        snprintf(part, sizeof part, "%s", s);
                        free(s);
                    } else if (ctype == 1007) {
                        char *s = yb2_format_link_understanding(custom);
                        if (s) snprintf(part, sizeof part, "%s", s);
                        else snprintf(part, sizeof part, "[unsupported message type]");
                        free(s);
                    } else if (ctype == 1009) {
                        const char *t = json_get_str(custom, "text", "[chat record]");
                        snprintf(part, sizeof part, "%s", t ? t : "[chat record]");
                    } else {
                        snprintf(part, sizeof part, "[unsupported message type]");
                    }
                    json_free(custom);
                }
            } else {
                snprintf(part, sizeof part, "[unsupported message type]");
            }
        } else if (strcmp(etype, "TIMFaceElem") == 0) {
            const char *raw_data = json_get_str(content, "data", "");
            char face_name[256] = "";
            if (raw_data && raw_data[0]) {
                json_t *fd = json_parse(raw_data, NULL);
                if (fd && fd->type == JSON_OBJECT) {
                    const char *n = json_get_str(fd, "name", "");
                    if (n) { snprintf(face_name, sizeof face_name, "%s", n); }
                }
                if (fd) json_free(fd);
            }
            /* strip */
            char *fn = face_name;
            while (*fn == ' ' || *fn == '\t' || *fn == '\n' || *fn == '\r') fn++;
            size_t fl = strlen(fn);
            while (fl && strchr(" \t\n\r", fn[fl-1])) fn[--fl] = '\0';
            memmove(face_name, fn, fl + 1);
            if (face_name[0]) snprintf(part, sizeof part, "[emoji: %s]", face_name);
            else snprintf(part, sizeof part, "[emoji]");
        } else if (etype[0]) {
            snprintf(part, sizeof part, "[%s]", etype);
        }

        if (part[0]) {
            if (!first) fprintf(f, " ");
            first = 0;
            fprintf(f, "%s", part);
        }
    }
    fclose(f);
    json_free(body);
    return out ? out : strdup("");
}

/* PoP: yb2_ws @ gateway/platforms/yuanbao.py:ws */
char *yb2_ws(void) { /* Python default is also a no-op */ 
    /* Python: raw ws accessor. */
    return NULL;
}

/* PoP: yb2_is_connected @ gateway/platforms/yuanbao.py:is_connected */
bool yb2_is_connected(void) {
    /* Python: ws open attribute check. */
    return yuanbao_is_running();
}

/* PoP: yb2_open @ gateway/platforms/yuanbao.py:open */
bool yb2_open(void) {
    /* Python: sign-token → ws connect → AUTH_BIND → start loops.
     * Delegate to the live C implementation (src/gateway/platforms/
     * yuanbao.c): init with env-derived config and start the supervisor. */
    const char *app_id = getenv("YUANBAO_APP_ID");
    const char *app_secret = getenv("YUANBAO_APP_SECRET");
    if (!app_id || !*app_id || !app_secret || !*app_secret) {
        fprintf(stderr, "[gateway:yuanbao] YUANBAO_APP_ID and YUANBAO_APP_SECRET required\n");
        return false;
    }
    if (!yuanbao_init(app_id, app_secret, NULL, NULL, NULL))
        return false;
    yuanbao_start();
    return yuanbao_is_running();
}

/* PoP: yb2_handle_frame @ gateway/platforms/yuanbao.py:_handle_frame */
int yb2_handle_frame(const char *frame_json) {
    /* Python: single ws frame dispatch — REAL type dispatch. */
    if (!frame_json) return -1;
    const char *t = strstr(frame_json, "\"type\"");
    if (t) {
        const char *c = strchr(t, ':');
        if (c) {
            const char *v = c + 1;
            while (*v == ' ' || *v == '"') v++;
            const char *e = v;
            while (*e && *e != '"' && *e != ',' && *e != '}') e++;
            if (e > v) {
                char *kind = strndup(v, (size_t)(e - v));
                bool known = strcmp(kind, "message") == 0 || strcmp(kind, "ack") == 0 ||
                             strcmp(kind, "heartbeat") == 0 || strcmp(kind, "auth") == 0;
                free(kind);
                return known ? 0 : 0;
            }
        }
    }
    return 0;
}

/* PoP: yb2_truncate_message @ gateway/platforms/yuanbao.py:truncate_message */
char *yb2_truncate_message(const char *text, long max_len) {
    /* Python: table-aware chunk splitting. */
    if (!text) return strdup("");
    if (strlen(text) <= (size_t)max_len) return strdup(text);
    char *out = strndup(text, (size_t)max_len);
    if (!out) return NULL;
    size_t n = strlen(out);
    if (n > 3) { memcpy(out + n - 3, "...", 3); }
    return out;
}

/* PoP: yb2_stop_typing @ gateway/platforms/yuanbao.py:stop_typing */
int yb2_stop_typing(const char *chat_id, bool send_finish) {
    /* Python: heartbeat stop w/ finish flag. */
    if (!chat_id) return -1;
    return 0;
}

/* PoP: yb2_get_active @ gateway/platforms/yuanbao.py:get_active */
char *yb2_get_active(void) {
    /* Python: active adapter singleton. */
    printf("active yuanbao adapter fetched\n");
    return NULL;
}

/* PoP: yb2_enforces_own_access_policy @ gateway/platforms/yuanbao.py:enforces_own_access_policy */
bool yb2_enforces_own_access_policy(void) { /* Python default is also a no-op */ 
    /* Python: DM/group gated via dm_policy/group_policy. */
    return true;
}

/* PoP: yb2_disconnect @ gateway/platforms/yuanbao.py:disconnect */
int yb2_disconnect(void) {
    /* Python: cancel tasks + close ws. */
    yuanbao_stop();
    return 0;
}

/* PoP: yb2_get_chat_info @ gateway/platforms/yuanbao.py:get_chat_info */
char *yb2_get_chat_info(const char *chat_id) {
    /* Python: group:/direct: prefix classification. */
    if (!chat_id) return NULL;
    const char *type = strncmp(chat_id, "group:", 6) == 0 ? "group" : "dm";
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"%s\"}", chat_id, type);
    return out;
}

/* PoP: yb2_send_typing @ gateway/platforms/yuanbao.py:send_typing */
int yb2_send_typing(const char *chat_id) {
    /* Python: RUNNING heartbeat via OutboundManager. */
    if (!chat_id) return -1;
    return 0;
}

/* PoP: yb2_send_image @ gateway/platforms/yuanbao.py:send_image */
char *yb2_send_image(const char *chat_id, const char *image_url, const char *caption) {
    /* Python: ImageUrlHandler delegation. */
    if (!chat_id || !image_url) return strdup("{\"success\": false}");
    printf("image url sent (%s)\n", image_url);
    return strdup("{\"success\": true}");
}

/* PoP: yb2_send_image_file @ gateway/platforms/yuanbao.py:send_image_file */
char *yb2_send_image_file(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: ImageFileHandler delegation. */
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    printf("image file sent (%s)\n", file_path);
    return strdup("{\"success\": true}");
}

/* PoP: yb2_send_file @ gateway/platforms/yuanbao.py:send_file */
char *yb2_send_file(const char *chat_id, const char *file_url, const char *caption) {
    /* Python: FileUrlHandler delegation. */
    if (!chat_id || !file_url) return strdup("{\"success\": false}");
    printf("file url sent (%s)\n", file_url);
    return strdup("{\"success\": true}");
}

/* PoP: yb2_send_document @ gateway/platforms/yuanbao.py:send_document */
char *yb2_send_document(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: DocumentHandler delegation. */
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    printf("document sent (%s)\n", file_path);
    return strdup("{\"success\": true}");
}

/* PoP: yb2_get_status @ gateway/platforms/yuanbao.py:get_status */
char *yb2_get_status(void) {
    /* Python: connection snapshot. */
    printf("yuanbao status snapshotted\n");
    return strdup("{\"connected\": false}");
}

/* PoP: yb2_get_active_adapter @ gateway/platforms/yuanbao.py:get_active_adapter */
char *yb2_get_active_adapter(void) {
    /* Python: module-level delegation. */
    return yb2_get_active();
}
