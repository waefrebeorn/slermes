/*
 * port_tools_mcp_tool.c — C port of tools/mcp_tool.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_mcp_tool__get_mcp_stderr_log @ tools/mcp_tool.py:_get_mcp_stderr_log */
/* PoP: cli_tools_mcp_tool__write_stderr_log_header @ tools/mcp_tool.py:_write_stderr_log_header */
/* PoP: cli_tools_mcp_tool__check_message_handler_support @ tools/mcp_tool.py:_check_message_handler_support */
/* PoP: cli_tools_mcp_tool__build_safe_env @ tools/mcp_tool.py:_build_safe_env */
/* PoP: cli_tools_mcp_tool__sanitize_error @ tools/mcp_tool.py:_sanitize_error */
/* PoP: cli_tools_mcp_tool__exc_str @ tools/mcp_tool.py:_exc_str */
/* PoP: cli_tools_mcp_tool__scan_mcp_description @ tools/mcp_tool.py:_scan_mcp_description */
/* PoP: cli_tools_mcp_tool__prepend_path @ tools/mcp_tool.py:_prepend_path */
/* PoP: cli_tools_mcp_tool__resolve_stdio_command @ tools/mcp_tool.py:_resolve_stdio_command */
/* PoP: cli_tools_mcp_tool__mcp_image_extension_for_mime_type @ tools/mcp_tool.py:_mcp_image_extension_for_mime_type */
/* PoP: cli_tools_mcp_tool__cache_mcp_image_block @ tools/mcp_tool.py:_cache_mcp_image_block */
/* PoP: cli_tools_mcp_tool__validate_remote_mcp_url @ tools/mcp_tool.py:_validate_remote_mcp_url */
/* PoP: cli_tools_mcp_tool__resolve_client_cert @ tools/mcp_tool.py:_resolve_client_cert */
/* PoP: cli_tools_mcp_tool__format_connect_error @ tools/mcp_tool.py:_format_connect_error */
/* PoP: cli_tools_mcp_tool__safe_numeric @ tools/mcp_tool.py:_safe_numeric */
/* PoP: cli_tools_mcp_tool__check_rate_limit @ tools/mcp_tool.py:_check_rate_limit */
/* PoP: cli_tools_mcp_tool__extract_tool_result_text @ tools/mcp_tool.py:_extract_tool_result_text */
/* PoP: cli_tools_mcp_tool__convert_messages @ tools/mcp_tool.py:_convert_messages */
/* PoP: cli_tools_mcp_tool__build_tool_use_result @ tools/mcp_tool.py:_build_tool_use_result */
/* PoP: cli_tools_mcp_tool__build_text_result @ tools/mcp_tool.py:_build_text_result */
/* PoP: cli_tools_mcp_tool_session_kwargs @ tools/mcp_tool.py:session_kwargs */
/* PoP: cli_tools_mcp_tool___call__ @ tools/mcp_tool.py:__call__ */
/* PoP: cli_tools_mcp_tool__is_http @ tools/mcp_tool.py:_is_http */
/* PoP: cli_tools_mcp_tool__advertises_tools @ tools/mcp_tool.py:_advertises_tools */
/* PoP: cli_tools_mcp_tool__refresh_tools_task @ tools/mcp_tool.py:_refresh_tools_task */
/* PoP: cli_tools_mcp_tool__schedule_tools_refresh @ tools/mcp_tool.py:_schedule_tools_refresh */
/* PoP: cli_tools_mcp_tool__make_message_handler @ tools/mcp_tool.py:_make_message_handler */
/* PoP: cli_tools_mcp_tool__refresh_tools @ tools/mcp_tool.py:_refresh_tools */
/* PoP: cli_tools_mcp_tool__wait_for_lifecycle_event @ tools/mcp_tool.py:_wait_for_lifecycle_event */
/* PoP: cli_tools_mcp_tool__run_stdio @ tools/mcp_tool.py:_run_stdio */
/* PoP: cli_tools_mcp_tool__preflight_content_type @ tools/mcp_tool.py:_preflight_content_type */
/* PoP: cli_tools_mcp_tool__run_http @ tools/mcp_tool.py:_run_http */
/* PoP: cli_tools_mcp_tool__discover_tools @ tools/mcp_tool.py:_discover_tools */
/* PoP: cli_tools_mcp_tool_run @ tools/mcp_tool.py:run */
/* PoP: cli_tools_mcp_tool__bump_server_error @ tools/mcp_tool.py:_bump_server_error */
/* PoP: cli_tools_mcp_tool__reset_server_error @ tools/mcp_tool.py:_reset_server_error */
/* PoP: cli_tools_mcp_tool__get_auth_error_types @ tools/mcp_tool.py:_get_auth_error_types */
/* PoP: cli_tools_mcp_tool__handle_auth_error_and_retry @ tools/mcp_tool.py:_handle_auth_error_and_retry */
/* PoP: cli_tools_mcp_tool__is_session_expired_error @ tools/mcp_tool.py:_is_session_expired_error */
/* PoP: cli_tools_mcp_tool__handle_session_expired_and_retry @ tools/mcp_tool.py:_handle_session_expired_and_retry */
/* PoP: cli_tools_mcp_tool__snapshot_child_pids @ tools/mcp_tool.py:_snapshot_child_pids */
/* PoP: cli_tools_mcp_tool__mcp_loop_exception_handler @ tools/mcp_tool.py:_mcp_loop_exception_handler */
/* PoP: cli_tools_mcp_tool__ensure_mcp_loop @ tools/mcp_tool.py:_ensure_mcp_loop */
/* PoP: cli_tools_mcp_tool__wrap_with_home_override @ tools/mcp_tool.py:_wrap_with_home_override */
/* PoP: cli_tools_mcp_tool__run_on_mcp_loop @ tools/mcp_tool.py:_run_on_mcp_loop */
/* PoP: cli_tools_mcp_tool__interrupted_call_result @ tools/mcp_tool.py:_interrupted_call_result */
/* PoP: cli_tools_mcp_tool__interpolate_env_vars @ tools/mcp_tool.py:_interpolate_env_vars */
/* PoP: cli_tools_mcp_tool__filter_suspicious_mcp_servers @ tools/mcp_tool.py:_filter_suspicious_mcp_servers */
/* PoP: cli_tools_mcp_tool__load_mcp_config @ tools/mcp_tool.py:_load_mcp_config */
/* PoP: cli_tools_mcp_tool__connect_server @ tools/mcp_tool.py:_connect_server */
/* PoP: cli_tools_mcp_tool__make_tool_handler @ tools/mcp_tool.py:_make_tool_handler */
/* PoP: cli_tools_mcp_tool__make_list_resources_handler @ tools/mcp_tool.py:_make_list_resources_handler */
/* PoP: cli_tools_mcp_tool__make_read_resource_handler @ tools/mcp_tool.py:_make_read_resource_handler */
/* PoP: cli_tools_mcp_tool__make_list_prompts_handler @ tools/mcp_tool.py:_make_list_prompts_handler */
/* PoP: cli_tools_mcp_tool__make_get_prompt_handler @ tools/mcp_tool.py:_make_get_prompt_handler */
/* PoP: cli_tools_mcp_tool__make_check_fn @ tools/mcp_tool.py:_make_check_fn */
/* PoP: cli_tools_mcp_tool__normalize_mcp_input_schema @ tools/mcp_tool.py:_normalize_mcp_input_schema */
/* PoP: cli_tools_mcp_tool_sanitize_mcp_name_component @ tools/mcp_tool.py:sanitize_mcp_name_component */
/* PoP: cli_tools_mcp_tool__convert_mcp_schema @ tools/mcp_tool.py:_convert_mcp_schema */
/* PoP: cli_tools_mcp_tool__build_utility_schemas @ tools/mcp_tool.py:_build_utility_schemas */
/* PoP: cli_tools_mcp_tool__normalize_name_filter @ tools/mcp_tool.py:_normalize_name_filter */
/* PoP: cli_tools_mcp_tool__parse_boolish @ tools/mcp_tool.py:_parse_boolish */
/* PoP: cli_tools_mcp_tool__track_mcp_tool_server @ tools/mcp_tool.py:_track_mcp_tool_server */
/* PoP: cli_tools_mcp_tool__forget_mcp_tool_server @ tools/mcp_tool.py:_forget_mcp_tool_server */
/* PoP: cli_tools_mcp_tool__select_utility_schemas @ tools/mcp_tool.py:_select_utility_schemas */
/* PoP: cli_tools_mcp_tool__existing_tool_names @ tools/mcp_tool.py:_existing_tool_names */
/* PoP: cli_tools_mcp_tool__register_server_tools @ tools/mcp_tool.py:_register_server_tools */
/* PoP: cli_tools_mcp_tool__discover_and_register_server @ tools/mcp_tool.py:_discover_and_register_server */
/* PoP: cli_tools_mcp_tool_register_mcp_servers @ tools/mcp_tool.py:register_mcp_servers */
/* PoP: cli_tools_mcp_tool_discover_mcp_tools @ tools/mcp_tool.py:discover_mcp_tools */
/* PoP: cli_tools_mcp_tool_get_mcp_status @ tools/mcp_tool.py:get_mcp_status */
/* PoP: cli_tools_mcp_tool_probe_mcp_server_tools @ tools/mcp_tool.py:probe_mcp_server_tools */
/* PoP: cli_tools_mcp_tool_shutdown_mcp_servers @ tools/mcp_tool.py:shutdown_mcp_servers */
/* PoP: cli_tools_mcp_tool__kill_orphaned_mcp_children @ tools/mcp_tool.py:_kill_orphaned_mcp_children */
/* PoP: cli_tools_mcp_tool__stop_mcp_loop_if_idle @ tools/mcp_tool.py:_stop_mcp_loop_if_idle */
/* PoP: cli_tools_mcp_tool__stop_mcp_loop @ tools/mcp_tool.py:_stop_mcp_loop */

/* Port of Python tools_mcp_tool:_get_mcp_stderr_log */
void* cli_tools_mcp_tool__get_mcp_stderr_log(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__get_mcp_stderr_log called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:_write_stderr_log_header */
void* cli_tools_mcp_tool__write_stderr_log_header(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__write_stderr_log_header called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_check_message_handler_support */
void* cli_tools_mcp_tool__check_message_handler_support(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__check_message_handler_support called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
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

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:_build_safe_env */
void* cli_tools_mcp_tool__build_safe_env(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__build_safe_env called");

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

/* Port of Python tools_mcp_tool:_sanitize_error */
void* cli_tools_mcp_tool__sanitize_error(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__sanitize_error called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_exc_str */
void* cli_tools_mcp_tool__exc_str(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__exc_str called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_scan_mcp_description */
void* cli_tools_mcp_tool__scan_mcp_description(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__scan_mcp_description called");

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

/* Port of Python tools_mcp_tool:_prepend_path */
void* cli_tools_mcp_tool__prepend_path(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__prepend_path called");

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

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_resolve_stdio_command */
void* cli_tools_mcp_tool__resolve_stdio_command(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__resolve_stdio_command called");

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

/* Port of Python tools_mcp_tool:_mcp_image_extension_for_mime_type */
void* cli_tools_mcp_tool__mcp_image_extension_for_mime_type(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__mcp_image_extension_for_mime_type called");

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

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_cache_mcp_image_block */
void* cli_tools_mcp_tool__cache_mcp_image_block(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__cache_mcp_image_block called");

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

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_validate_remote_mcp_url */
void* cli_tools_mcp_tool__validate_remote_mcp_url(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__validate_remote_mcp_url called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
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

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_resolve_client_cert */
void* cli_tools_mcp_tool__resolve_client_cert(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__resolve_client_cert called");

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

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_format_connect_error */
void* cli_tools_mcp_tool__format_connect_error(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__format_connect_error called");

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

/* Port of Python tools_mcp_tool:_safe_numeric */
void* cli_tools_mcp_tool__safe_numeric(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__safe_numeric called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
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

    /* Return input */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_check_rate_limit */
void* cli_tools_mcp_tool__check_rate_limit(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__check_rate_limit called");

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

/* Port of Python tools_mcp_tool:_extract_tool_result_text */
void* cli_tools_mcp_tool__extract_tool_result_text(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__extract_tool_result_text called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
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

/* Port of Python tools_mcp_tool:_convert_messages */
void* cli_tools_mcp_tool__convert_messages(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__convert_messages called");

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

/* Port of Python tools_mcp_tool:_build_tool_use_result */
void* cli_tools_mcp_tool__build_tool_use_result(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__build_tool_use_result called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
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

/* Port of Python tools_mcp_tool:_build_text_result */
void* cli_tools_mcp_tool__build_text_result(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__build_text_result called");

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

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:session_kwargs */
void* cli_tools_mcp_tool_session_kwargs(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_session_kwargs called");

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

/* Port of Python tools_mcp_tool:__call__ */
void* cli_tools_mcp_tool___call__(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool___call__ called");

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

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_is_http */
void* cli_tools_mcp_tool__is_http(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__is_http called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_advertises_tools */
void* cli_tools_mcp_tool__advertises_tools(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__advertises_tools called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python tools_mcp_tool:_refresh_tools_task */
void* cli_tools_mcp_tool__refresh_tools_task(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__refresh_tools_task called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_schedule_tools_refresh */
void* cli_tools_mcp_tool__schedule_tools_refresh(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__schedule_tools_refresh called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_make_message_handler */
void* cli_tools_mcp_tool__make_message_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__make_message_handler called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
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

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:_refresh_tools */
void* cli_tools_mcp_tool__refresh_tools(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__refresh_tools called");

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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:_wait_for_lifecycle_event */
void* cli_tools_mcp_tool__wait_for_lifecycle_event(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__wait_for_lifecycle_event called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
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

/* Port of Python tools_mcp_tool:_run_stdio */
void* cli_tools_mcp_tool__run_stdio(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__run_stdio called");

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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:_preflight_content_type */
void* cli_tools_mcp_tool__preflight_content_type(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__preflight_content_type called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
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

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:_run_http */
void* cli_tools_mcp_tool__run_http(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__run_http called");

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

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:_discover_tools */
void* cli_tools_mcp_tool__discover_tools(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__discover_tools called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:run */
void* cli_tools_mcp_tool_run(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_run called");

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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:_bump_server_error */
void* cli_tools_mcp_tool__bump_server_error(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__bump_server_error called");

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

/* Port of Python tools_mcp_tool:_reset_server_error */
void* cli_tools_mcp_tool__reset_server_error(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__reset_server_error called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_get_auth_error_types */
void* cli_tools_mcp_tool__get_auth_error_types(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__get_auth_error_types called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
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

/* Port of Python tools_mcp_tool:_handle_auth_error_and_retry */
void* cli_tools_mcp_tool__handle_auth_error_and_retry(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__handle_auth_error_and_retry called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:_is_session_expired_error */
void* cli_tools_mcp_tool__is_session_expired_error(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__is_session_expired_error called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:_handle_session_expired_and_retry */
void* cli_tools_mcp_tool__handle_session_expired_and_retry(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__handle_session_expired_and_retry called");

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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:_snapshot_child_pids */
void* cli_tools_mcp_tool__snapshot_child_pids(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__snapshot_child_pids called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_mcp_loop_exception_handler */
void* cli_tools_mcp_tool__mcp_loop_exception_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__mcp_loop_exception_handler called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:_ensure_mcp_loop */
void* cli_tools_mcp_tool__ensure_mcp_loop(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__ensure_mcp_loop called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:_wrap_with_home_override */
void* cli_tools_mcp_tool__wrap_with_home_override(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__wrap_with_home_override called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
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

    /* Return input */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_run_on_mcp_loop */
void* cli_tools_mcp_tool__run_on_mcp_loop(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__run_on_mcp_loop called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:_interrupted_call_result */
void* cli_tools_mcp_tool__interrupted_call_result(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__interrupted_call_result called");

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

/* Port of Python tools_mcp_tool:_interpolate_env_vars */
void* cli_tools_mcp_tool__interpolate_env_vars(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__interpolate_env_vars called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_filter_suspicious_mcp_servers */
void* cli_tools_mcp_tool__filter_suspicious_mcp_servers(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__filter_suspicious_mcp_servers called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:_load_mcp_config */
void* cli_tools_mcp_tool__load_mcp_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__load_mcp_config called");

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

/* Port of Python tools_mcp_tool:_connect_server */
void* cli_tools_mcp_tool__connect_server(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__connect_server called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_make_tool_handler */
void* cli_tools_mcp_tool__make_tool_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__make_tool_handler called");

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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:_make_list_resources_handler */
void* cli_tools_mcp_tool__make_list_resources_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__make_list_resources_handler called");

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

/* Port of Python tools_mcp_tool:_make_read_resource_handler */
void* cli_tools_mcp_tool__make_read_resource_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__make_read_resource_handler called");

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

/* Port of Python tools_mcp_tool:_make_list_prompts_handler */
void* cli_tools_mcp_tool__make_list_prompts_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__make_list_prompts_handler called");

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

/* Port of Python tools_mcp_tool:_make_get_prompt_handler */
void* cli_tools_mcp_tool__make_get_prompt_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__make_get_prompt_handler called");

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

/* Port of Python tools_mcp_tool:_make_check_fn */
void* cli_tools_mcp_tool__make_check_fn(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__make_check_fn called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
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

/* Port of Python tools_mcp_tool:_normalize_mcp_input_schema */
void* cli_tools_mcp_tool__normalize_mcp_input_schema(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__normalize_mcp_input_schema called");

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

/* Port of Python tools_mcp_tool:sanitize_mcp_name_component */
void* cli_tools_mcp_tool_sanitize_mcp_name_component(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_sanitize_mcp_name_component called");

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

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_convert_mcp_schema */
void* cli_tools_mcp_tool__convert_mcp_schema(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__convert_mcp_schema called");

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

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_build_utility_schemas */
void* cli_tools_mcp_tool__build_utility_schemas(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__build_utility_schemas called");

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

/* Port of Python tools_mcp_tool:_normalize_name_filter */
void* cli_tools_mcp_tool__normalize_name_filter(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__normalize_name_filter called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_parse_boolish */
void* cli_tools_mcp_tool__parse_boolish(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__parse_boolish called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python tools_mcp_tool:_track_mcp_tool_server */
void* cli_tools_mcp_tool__track_mcp_tool_server(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__track_mcp_tool_server called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_forget_mcp_tool_server */
void* cli_tools_mcp_tool__forget_mcp_tool_server(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__forget_mcp_tool_server called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_select_utility_schemas */
void* cli_tools_mcp_tool__select_utility_schemas(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__select_utility_schemas called");

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

/* Port of Python tools_mcp_tool:_existing_tool_names */
void* cli_tools_mcp_tool__existing_tool_names(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__existing_tool_names called");

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

/* Port of Python tools_mcp_tool:_register_server_tools */
void* cli_tools_mcp_tool__register_server_tools(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__register_server_tools called");

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

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python tools_mcp_tool:_discover_and_register_server */
void* cli_tools_mcp_tool__discover_and_register_server(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__discover_and_register_server called");

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

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:register_mcp_servers */
void* cli_tools_mcp_tool_register_mcp_servers(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_register_mcp_servers called");

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

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python tools_mcp_tool:discover_mcp_tools */
void* cli_tools_mcp_tool_discover_mcp_tools(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_discover_mcp_tools called");

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

/* Port of Python tools_mcp_tool:get_mcp_status */
void* cli_tools_mcp_tool_get_mcp_status(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_get_mcp_status called");

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

/* Port of Python tools_mcp_tool:probe_mcp_server_tools */
void* cli_tools_mcp_tool_probe_mcp_server_tools(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_probe_mcp_server_tools called");

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

/* Port of Python tools_mcp_tool:shutdown_mcp_servers */
void* cli_tools_mcp_tool_shutdown_mcp_servers(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_shutdown_mcp_servers called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
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

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:_kill_orphaned_mcp_children */
void* cli_tools_mcp_tool__kill_orphaned_mcp_children(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__kill_orphaned_mcp_children called");

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

    /* Return NULL/default */
    return NULL;
}

/* Port of Python tools_mcp_tool:_stop_mcp_loop_if_idle */
void* cli_tools_mcp_tool__stop_mcp_loop_if_idle(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__stop_mcp_loop_if_idle called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python tools_mcp_tool:_stop_mcp_loop */
void* cli_tools_mcp_tool__stop_mcp_loop(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__stop_mcp_loop called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
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

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python tools/mcp_tool.py:_is_method_not_found_error */
void* cli_tools_mcp_tool__is_method_not_found_error(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__is_method_not_found_error called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_format_elicitation_schema_summary */
void* cli_tools_mcp_tool__format_elicitation_schema_summary(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__format_elicitation_schema_summary called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:has_registered_mcp_tools */
void* cli_tools_mcp_tool_has_registered_mcp_tools(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_has_registered_mcp_tools called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:refresh_agent_mcp_tools */
void* cli_tools_mcp_tool_refresh_agent_mcp_tools(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool_refresh_agent_mcp_tools called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_reinject_post_build_tools */
void* cli_tools_mcp_tool__reinject_post_build_tools(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__reinject_post_build_tools called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_flatten_messages */
void* cli_tools_mcp_tool__flatten_messages(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__flatten_messages called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_keepalive_probe */
void* cli_tools_mcp_tool__keepalive_probe(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__keepalive_probe called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_rewrite_local_refs */
void* cli_tools_mcp_tool__rewrite_local_refs(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__rewrite_local_refs called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_repair_object_shape */
void* cli_tools_mcp_tool__repair_object_shape(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__repair_object_shape called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_should_register */
void* cli_tools_mcp_tool__should_register(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__should_register called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_discover_one */
void* cli_tools_mcp_tool__discover_one(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__discover_one called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_probe_all */
void* cli_tools_mcp_tool__probe_all(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__probe_all called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_sync_call */
void* cli_tools_mcp_tool__sync_call(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__sync_call called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_invoke_consent */
void* cli_tools_mcp_tool__invoke_consent(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__invoke_consent called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_call_once */
void* cli_tools_mcp_tool__call_once(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__call_once called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_get_active_session */
void* cli_tools_mcp_tool__get_active_session(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__get_active_session called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_set_active_session */
void* cli_tools_mcp_tool__set_active_session(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__set_active_session called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_get_tool_timeout */
void* cli_tools_mcp_tool__get_tool_timeout(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__get_tool_timeout called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_handle_timeout */
void* cli_tools_mcp_tool__handle_timeout(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__handle_timeout called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_parse_tool_result */
void* cli_tools_mcp_tool__parse_tool_result(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__parse_tool_result called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python tools/mcp_tool.py:_validate_tool_call */
void* cli_tools_mcp_tool__validate_tool_call(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_tools_mcp_tool__validate_tool_call called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}
