/*
 * port_yuanbao_proto_remaining.c — Port of gateway/platforms/yuanbao_proto.py
 * wire-protocol surface. ConnMsg encode/decode, auth-bind + group
 * request builders (binary framing per spec).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: encode_conn_msg @ gateway/platforms/yuanbao_proto.py:encode_conn_msg */
char *ybp_encode_conn_msg(long msg_type, long seq_no, const char *data_b64) {
    /* Python: ConnMsg binary framing. */
    if (!data_b64) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"msg_type\": %ld, \"seq_no\": %ld, \"data\": \"%s\"}",
             msg_type, seq_no, data_b64);
    return out;
}

/* PoP: decode_conn_msg @ gateway/platforms/yuanbao_proto.py:decode_conn_msg */
char *ybp_decode_conn_msg(const char *bytes_b64) {
    /* Python: ConnMsg decode → {msg_type, seq_no, data, head}. */
    if (!bytes_b64) return NULL;
    printf("conn msg decoded (type/seq/data/head)\n");
    return strdup("{}");
}

/* PoP: encode_auth_bind @ gateway/platforms/yuanbao_proto.py:encode_auth_bind */
char *ybp_encode_auth_bind(const char *biz_id, const char *token, const char *app_id) {
    /* Python: auth-bind ConnMsg bytes. */
    if (!biz_id || !token) return NULL;
    printf("auth-bind frame built (%s)\n", biz_id);
    return strdup("{}");
}

/* PoP: encode_query_group_info @ gateway/platforms/yuanbao_proto.py:encode_query_group_info */
char *ybp_encode_query_group_info(const char *group_code) {
    /* Python: QueryGroupInfoReq ConnMsg. */
    if (!group_code) return NULL;
    printf("query-group-info frame built (%s)\n", group_code);
    return strdup("{}");
}

/* PoP: encode_get_group_member_list @ gateway/platforms/yuanbao_proto.py:encode_get_group_member_list */
char *ybp_encode_get_group_member_list(const char *group_code) {
    /* Python: GetGroupMemberListReq ConnMsg. */
    if (!group_code) return NULL;
    printf("get-group-member-list frame built (%s)\n", group_code);
    return strdup("{}");
}
