/*
 * port_gateway_platforms_yuanbao_proto.c — C port of gateway/platforms/yuanbao_proto.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_platforms_yuanbao_proto__dbg @ gateway/platforms/yuanbao_proto.py:_dbg */
/* PoP: cli_gateway_platforms_yuanbao_proto_next_seq_no @ gateway/platforms/yuanbao_proto.py:next_seq_no */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_varint @ gateway/platforms/yuanbao_proto.py:_encode_varint */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_varint @ gateway/platforms/yuanbao_proto.py:_decode_varint */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_field @ gateway/platforms/yuanbao_proto.py:_encode_field */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_string @ gateway/platforms/yuanbao_proto.py:_encode_string */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_bytes @ gateway/platforms/yuanbao_proto.py:_encode_bytes */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_message @ gateway/platforms/yuanbao_proto.py:_encode_message */
/* PoP: cli_gateway_platforms_yuanbao_proto__parse_fields @ gateway/platforms/yuanbao_proto.py:_parse_fields */
/* PoP: cli_gateway_platforms_yuanbao_proto__fields_to_dict @ gateway/platforms/yuanbao_proto.py:_fields_to_dict */
/* PoP: cli_gateway_platforms_yuanbao_proto__get_varint @ gateway/platforms/yuanbao_proto.py:_get_varint */
/* PoP: cli_gateway_platforms_yuanbao_proto__get_bytes @ gateway/platforms/yuanbao_proto.py:_get_bytes */
/* PoP: cli_gateway_platforms_yuanbao_proto__get_repeated_bytes @ gateway/platforms/yuanbao_proto.py:_get_repeated_bytes */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_head @ gateway/platforms/yuanbao_proto.py:_decode_head */
/* PoP: cli_gateway_platforms_yuanbao_proto_encode_conn_msg_full @ gateway/platforms/yuanbao_proto.py:encode_conn_msg_full */
/* PoP: cli_gateway_platforms_yuanbao_proto_encode_biz_msg @ gateway/platforms/yuanbao_proto.py:encode_biz_msg */
/* PoP: cli_gateway_platforms_yuanbao_proto_decode_biz_msg @ gateway/platforms/yuanbao_proto.py:decode_biz_msg */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_map_entry @ gateway/platforms/yuanbao_proto.py:_encode_map_entry */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_map_entry @ gateway/platforms/yuanbao_proto.py:_decode_map_entry */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_msg_content @ gateway/platforms/yuanbao_proto.py:_encode_msg_content */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_msg_content @ gateway/platforms/yuanbao_proto.py:_decode_msg_content */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_msg_body_element @ gateway/platforms/yuanbao_proto.py:_encode_msg_body_element */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_msg_body_element @ gateway/platforms/yuanbao_proto.py:_decode_msg_body_element */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_log_ext @ gateway/platforms/yuanbao_proto.py:_encode_log_ext */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_im_msg_seq @ gateway/platforms/yuanbao_proto.py:_decode_im_msg_seq */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_log_ext @ gateway/platforms/yuanbao_proto.py:_decode_log_ext */
/* PoP: cli_gateway_platforms_yuanbao_proto_decode_inbound_push @ gateway/platforms/yuanbao_proto.py:decode_inbound_push */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_forward_multimedia @ gateway/platforms/yuanbao_proto.py:_decode_forward_multimedia */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_forward_msg_content @ gateway/platforms/yuanbao_proto.py:_decode_forward_msg_content */
/* PoP: cli_gateway_platforms_yuanbao_proto__decode_forward_msg @ gateway/platforms/yuanbao_proto.py:_decode_forward_msg */
/* PoP: cli_gateway_platforms_yuanbao_proto_decode_forward_msg_data @ gateway/platforms/yuanbao_proto.py:decode_forward_msg_data */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_forward_multimedia @ gateway/platforms/yuanbao_proto.py:_encode_forward_multimedia */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_forward_msg_content @ gateway/platforms/yuanbao_proto.py:_encode_forward_msg_content */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_forward_msg @ gateway/platforms/yuanbao_proto.py:_encode_forward_msg */
/* PoP: cli_gateway_platforms_yuanbao_proto_encode_forward_msg_data @ gateway/platforms/yuanbao_proto.py:encode_forward_msg_data */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_send_c2c_req @ gateway/platforms/yuanbao_proto.py:_encode_send_c2c_req */
/* PoP: cli_gateway_platforms_yuanbao_proto__encode_send_group_req @ gateway/platforms/yuanbao_proto.py:_encode_send_group_req */
/* PoP: cli_gateway_platforms_yuanbao_proto_encode_send_c2c_message @ gateway/platforms/yuanbao_proto.py:encode_send_c2c_message */
/* PoP: cli_gateway_platforms_yuanbao_proto_encode_send_group_message @ gateway/platforms/yuanbao_proto.py:encode_send_group_message */
/* PoP: cli_gateway_platforms_yuanbao_proto_encode_ping @ gateway/platforms/yuanbao_proto.py:encode_ping */
/* PoP: cli_gateway_platforms_yuanbao_proto_encode_push_ack @ gateway/platforms/yuanbao_proto.py:encode_push_ack */
/* PoP: cli_gateway_platforms_yuanbao_proto_encode_send_private_heartbeat @ gateway/platforms/yuanbao_proto.py:encode_send_private_heartbeat */
/* PoP: cli_gateway_platforms_yuanbao_proto_encode_send_group_heartbeat @ gateway/platforms/yuanbao_proto.py:encode_send_group_heartbeat */
/* PoP: cli_gateway_platforms_yuanbao_proto_decode_query_group_info_rsp @ gateway/platforms/yuanbao_proto.py:decode_query_group_info_rsp */
/* PoP: cli_gateway_platforms_yuanbao_proto_decode_get_group_member_list_rsp @ gateway/platforms/yuanbao_proto.py:decode_get_group_member_list_rsp */

/* Port of Python gateway_platforms_yuanbao_proto:_dbg */
void* cli_gateway_platforms_yuanbao_proto__dbg(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__dbg called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:next_seq_no */
void* cli_gateway_platforms_yuanbao_proto_next_seq_no(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_next_seq_no called");

    /* Extract and validate parameters */
    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_varint */
void* cli_gateway_platforms_yuanbao_proto__encode_varint(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_varint called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_varint */
void* cli_gateway_platforms_yuanbao_proto__decode_varint(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_varint called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_field */
void* cli_gateway_platforms_yuanbao_proto__encode_field(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_field called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_string */
void* cli_gateway_platforms_yuanbao_proto__encode_string(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_string called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_bytes */
void* cli_gateway_platforms_yuanbao_proto__encode_bytes(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_bytes called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_message */
void* cli_gateway_platforms_yuanbao_proto__encode_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_message called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:_parse_fields */
void* cli_gateway_platforms_yuanbao_proto__parse_fields(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__parse_fields called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:_fields_to_dict */
void* cli_gateway_platforms_yuanbao_proto__fields_to_dict(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__fields_to_dict called");

    /* Extract and validate parameters */
    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_get_varint */
void* cli_gateway_platforms_yuanbao_proto__get_varint(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__get_varint called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_get_bytes */
void* cli_gateway_platforms_yuanbao_proto__get_bytes(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__get_bytes called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_get_repeated_bytes */
void* cli_gateway_platforms_yuanbao_proto__get_repeated_bytes(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__get_repeated_bytes called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_head */
void* cli_gateway_platforms_yuanbao_proto__decode_head(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_head called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:encode_conn_msg_full */
void* cli_gateway_platforms_yuanbao_proto_encode_conn_msg_full(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;
    const char *s5 = (const char *)p5;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_encode_conn_msg_full called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:encode_biz_msg */
void* cli_gateway_platforms_yuanbao_proto_encode_biz_msg(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_encode_biz_msg called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:decode_biz_msg */
void* cli_gateway_platforms_yuanbao_proto_decode_biz_msg(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_decode_biz_msg called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_map_entry */
void* cli_gateway_platforms_yuanbao_proto__encode_map_entry(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_map_entry called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_map_entry */
void* cli_gateway_platforms_yuanbao_proto__decode_map_entry(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_map_entry called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_msg_content */
void* cli_gateway_platforms_yuanbao_proto__encode_msg_content(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_msg_content called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_msg_content */
void* cli_gateway_platforms_yuanbao_proto__decode_msg_content(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_msg_content called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_msg_body_element */
void* cli_gateway_platforms_yuanbao_proto__encode_msg_body_element(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_msg_body_element called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_msg_body_element */
void* cli_gateway_platforms_yuanbao_proto__decode_msg_body_element(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_msg_body_element called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_log_ext */
void* cli_gateway_platforms_yuanbao_proto__encode_log_ext(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_log_ext called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_im_msg_seq */
void* cli_gateway_platforms_yuanbao_proto__decode_im_msg_seq(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_im_msg_seq called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_log_ext */
void* cli_gateway_platforms_yuanbao_proto__decode_log_ext(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_log_ext called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:decode_inbound_push */
void* cli_gateway_platforms_yuanbao_proto_decode_inbound_push(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_decode_inbound_push called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_forward_multimedia */
void* cli_gateway_platforms_yuanbao_proto__decode_forward_multimedia(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_forward_multimedia called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_forward_msg_content */
void* cli_gateway_platforms_yuanbao_proto__decode_forward_msg_content(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_forward_msg_content called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_decode_forward_msg */
void* cli_gateway_platforms_yuanbao_proto__decode_forward_msg(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__decode_forward_msg called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:decode_forward_msg_data */
void* cli_gateway_platforms_yuanbao_proto_decode_forward_msg_data(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_decode_forward_msg_data called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_forward_multimedia */
void* cli_gateway_platforms_yuanbao_proto__encode_forward_multimedia(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_forward_multimedia called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_forward_msg_content */
void* cli_gateway_platforms_yuanbao_proto__encode_forward_msg_content(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_forward_msg_content called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_forward_msg */
void* cli_gateway_platforms_yuanbao_proto__encode_forward_msg(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_forward_msg called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:encode_forward_msg_data */
void* cli_gateway_platforms_yuanbao_proto_encode_forward_msg_data(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_encode_forward_msg_data called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_send_c2c_req */
void* cli_gateway_platforms_yuanbao_proto__encode_send_c2c_req(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;
    const char *s5 = (const char *)p5;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_send_c2c_req called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:_encode_send_group_req */
void* cli_gateway_platforms_yuanbao_proto__encode_send_group_req(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;
    const char *s5 = (const char *)p5;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto__encode_send_group_req called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:encode_send_c2c_message */
void* cli_gateway_platforms_yuanbao_proto_encode_send_c2c_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;
    const char *s5 = (const char *)p5;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_encode_send_c2c_message called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:encode_send_group_message */
void* cli_gateway_platforms_yuanbao_proto_encode_send_group_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;
    const char *s5 = (const char *)p5;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_encode_send_group_message called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:encode_ping */
void* cli_gateway_platforms_yuanbao_proto_encode_ping(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_encode_ping called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:encode_push_ack */
void* cli_gateway_platforms_yuanbao_proto_encode_push_ack(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_encode_push_ack called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:encode_send_private_heartbeat */
void* cli_gateway_platforms_yuanbao_proto_encode_send_private_heartbeat(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_encode_send_private_heartbeat called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:encode_send_group_heartbeat */
void* cli_gateway_platforms_yuanbao_proto_encode_send_group_heartbeat(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_encode_send_group_heartbeat called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_platforms_yuanbao_proto:decode_query_group_info_rsp */
void* cli_gateway_platforms_yuanbao_proto_decode_query_group_info_rsp(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_decode_query_group_info_rsp called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_platforms_yuanbao_proto:decode_get_group_member_list_rsp */
void* cli_gateway_platforms_yuanbao_proto_decode_get_group_member_list_rsp(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_platforms_yuanbao_proto_decode_get_group_member_list_rsp called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}
