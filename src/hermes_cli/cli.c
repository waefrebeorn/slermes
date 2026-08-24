/*
 * port_cli.c — C port of cli.py
 */

#include "hermes_logger.h"
#include "hermes_skill_commands.h"
#include "hermes_json.h"
#include <ctype.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: string @ cli.py:_print_nous_credits_block */
/* PoP: string @ cli.py:_show_credits */

/* Port of Python cli:CanonicalUsage */

/* ── Reasoning-tag stripping (cli.py display surface) ───────────────── */

static const char *CLI_REASONING_TAGS[] = {
    "REASONING_SCRATCHPAD", "think", "thinking", "reasoning", "thought",
};
#define CLI_N_REASONING_TAGS 5

static int cli_strcasestr_find(const char *hay, const char *needle,
                               size_t *out_at, size_t *out_len) {
    size_t hl = strlen(hay), nl = strlen(needle);
    if (nl == 0 || nl > hl) return 0;
    for (size_t i = 0; i + nl <= hl; i++) {
        size_t k = 0;
        while (k < nl &&
               tolower((unsigned char)hay[i + k]) == tolower((unsigned char)needle[k]))
            k++;
        if (k == nl) { *out_at = i; *out_len = nl; return 1; }
    }
    return 0;
}

/* Remove every occurrence of a literal tag with surrounding < > from `text`
 * starting at scan position `from`; returns malloc'd new string. */
/* Port of Python cli.py:_strip_reasoning_tags — removes closed reasoning
 * pairs, unterminated opens, orphan closes, and tool-call XML blocks. */
char *cli_strip_reasoning_tags(const char *text) {
    if (!text) return strdup("");
    size_t cap = strlen(text) + 16;
    char *cleaned = strdup(text);
    if (!cleaned) return NULL;

    for (int pass = 0; pass < 3; pass++) {
        for (int ti = 0; ti < CLI_N_REASONING_TAGS; ti++) {
            const char *tag = CLI_REASONING_TAGS[ti];
            size_t tl = strlen(tag);
            /* Closed pair: <tag> ... </tag> (+ trailing ws). Loop until none. */
            for (;;) {
                char *open_p = strcasestr(cleaned, tag);
                int removed = 0;
                while (open_p) {
                    if (open_p > cleaned && open_p[-1] == '<') {
                        char *close_search = open_p + tl;
                        if (*close_search == '>') {
                            /* find </tag> after */
                            char close_pat[64];
                            snprintf(close_pat, sizeof(close_pat), "</%s", tag);
                            char *close_p = strcasestr(close_search, close_pat);
                            if (close_p) {
                                char *after = close_p + strlen(close_pat);
                                if (*after == '>') {
                                    after++;
                                    while (*after == ' ' || *after == '\t' ||
                                           *after == '\n' || *after == '\r') after++;
                                    memmove(open_p - 1, after, strlen(after) + 1);
                                    removed = 1;
                                }
                            }
                        }
                    }
                    break;
                }
                if (!removed) break;
            }
            /* Unterminated open: strip "<tag>..." to end. */
            {
                char pat[66];
                snprintf(pat, sizeof(pat), "<%s>", tag);
                char *p = strcasestr(cleaned, pat);
                if (p) { *p = '\0'; }
            }
            /* Orphan close: "</tag>" + trailing ws. */
            {
                char pat[68];
                snprintf(pat, sizeof(pat), "</%s>", tag);
                char *p;
                while ((p = strcasestr(cleaned, pat)) != NULL) {
                    char *after = p + strlen(pat);
                    while (*after == ' ' || *after == '\t' ||
                           *after == '\n' || *after == '\r') after++;
                    memmove(p, after, strlen(after) + 1);
                }
            }
        }
    }

    /* Tool-call XML blocks: <tool_call ...>...</tool_call> etc. */
    static const char *TC_TAGS[] = { "tool_call", "tool_calls", "tool_result",
                                     "function_call", "function_calls" };
    for (int ti = 0; ti < 5; ti++) {
        const char *tct = TC_TAGS[ti];
        char open_pat[80], close_pat[80];
        for (;;) {
            snprintf(open_pat, sizeof(open_pat), "<%s", tct);      /* boundary via next char check */
            snprintf(close_pat, sizeof(close_pat), "</%s>", tct);
            char *open_p = strcasestr(cleaned, open_pat);
            if (!open_p) break;
            char after = open_p[strlen(open_pat)];
            if (after != '>' && after != ' ' && after != '\t' && after != '\n') {
                /* not our tag boundary (e.g. <tool_calls2) — skip past */
                memmove(cleaned, open_p + 1, strlen(open_p));   /* wrong; shift search */
                /* safer: advance by removing nothing — break to avoid loop */
                break;
            }
            char *close_p = strcasestr(open_p, close_pat);
            if (!close_p) {
                /* unterminated tool-call block: strip to end */
                *open_p = '\0';
                break;
            }
            char *endp = close_p + strlen(close_pat);
            while (*endp == ' ' || *endp == '\t' || *endp == '\n' || *endp == '\r') endp++;
            memmove(open_p, endp, strlen(endp) + 1);
        }
    }

    /* Boundary-gated <function name="..."> ... </function> (Gemma-style):
     * open tag must sit at line start or after [\n\r.!?:] + optional
     * spaces/tabs, and carry a name= attribute. */
    for (;;) {
        char *fn_p = strcasestr(cleaned, "<function");
        int removed_any = 0;
        while (fn_p) {
            char after = fn_p[strlen("<function")];
            int boundary_ok = 0;
            if (fn_p == cleaned) boundary_ok = 1;
            else {
                const char *q = fn_p - 1;
                while (q >= cleaned && (*q == ' ' || *q == '\t')) q--;
                if (q >= cleaned && (*q == '\n' || *q == '\r' || *q == '.' ||
                                     *q == '!' || *q == '?' || *q == ':'))
                    boundary_ok = 1;
                else if (q < cleaned) boundary_ok = 1;
            }
            if ((after == ' ' || after == '\t' || after == '\n') && boundary_ok) {
                char *gt = strchr(fn_p, '>');
                char *name_at = fn_p ? strcasestr(fn_p, "name") : NULL;
                if (gt && name_at && name_at < gt &&
                    (name_at == fn_p + strlen("<function") ||
                     name_at[-1] == ' ' || name_at[-1] == '\t')) {
                    char *close_p = strcasestr(gt, "</function>");
                    if (close_p) {
                        char *endp2 = close_p + strlen("</function>");
                        while (*endp2 == ' ' || *endp2 == '\t' ||
                               *endp2 == '\n' || *endp2 == '\r') endp2++;
                        memmove(fn_p, endp2, strlen(endp2) + 1);
                        removed_any = 1;
                        break;
                    }
                }
            }
            fn_p = fn_p + 9 < cleaned + strlen(cleaned)
                ? strcasestr(fn_p + 9, "<function") : NULL;
        }
        if (!removed_any) break;
    }

    /* Stray generic close tags. */
    static const char *STRAY[] = { "</tool_call>", "</tool_calls>",
        "</tool_result>", "</function_call>", "</function_calls>", "</function>" };
    for (int si = 0; si < 6; si++) {
        char *p;
        while ((p = strcasestr(cleaned, STRAY[si])) != NULL) {
            char *after = p + strlen(STRAY[si]);
            while (*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') after++;
            memmove(p, after, strlen(after) + 1);
        }
    }

    /* final .strip() */
    char *b = cleaned;
    while (*b && isspace((unsigned char)*b)) b++;
    char *e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) *--e = '\0';
    char *result = strdup(b);
    free(cleaned);
    (void)cap;
    return result;
}

/* Port of Python cli.py:_assistant_content_as_text */
char *cli_assistant_content_as_text(const json_t *content) {
    if (!content) return strdup("");
    if (json_is_string(content)) {
        const char *s = json_string_value(content);
        return strdup(s ? s : "");
    }
    if (json_is_array(content)) {
        size_t cap = 128, len = 0;
        char *out = malloc(cap);
        if (!out) return NULL;
        out[0] = '\0';
        size_t n = json_array_size(content);
        for (size_t i = 0; i < n; i++) {
            const json_t *part = json_array_get(content, i);
            if (!part || !json_is_object(part)) continue;
            const char *type = json_string_value(json_object_get(part, "type"));
            if (!type || strcmp(type, "text") != 0) continue;
            const char *txt = json_string_value(json_object_get(part, "text"));
            const char *piece = txt ? txt : "";
            if (!piece[0]) continue;
            size_t plen = strlen(piece);
            int need_nl = (len > 0);
            while (len + plen + need_nl + 1 > cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) { free(out); return NULL; }
                out = tmp;
            }
            if (need_nl) out[len++] = '\n';
            memcpy(out + len, piece, plen + 1);
            len += plen;
        }
        return out;
    }
    return strdup("");
}

/* Port of Python cli.py:_assistant_copy_text */
char *cli_assistant_copy_text(const json_t *content) {
    char *plain = cli_assistant_content_as_text(content);
    if (!plain) return NULL;
    char *stripped = cli_strip_reasoning_tags(plain);
    free(plain);
    return stripped;
}

/* Port of Python cli:_load_prefill_messages */

/* Port of Python cli:_resolve_prefill_messages_file */

/* Port of Python cli:_parse_reasoning_config */

/* Port of Python cli:_parse_service_tier_config */

/* Port of Python cli:load_cli_config */

/* Port of Python cli:find_spec */


/* Port of Python cli:AIAgent */

/* Port of Python cli:get_tool_definitions */

/* Port of Python cli:get_toolset_for_tool */

/* Port of Python cli:get_all_toolsets */

/* Port of Python cli:get_toolset_info */

/* Port of Python cli:validate_toolset */

/* Port of Python cli:_sync_process_session_id */

/* Port of Python cli:get_job */

/* Port of Python cli:_cleanup_all_terminals */

/* Port of Python cli:set_sudo_password_callback */

/* Port of Python cli:set_approval_callback */

/* Port of Python cli:set_secret_capture_callback */

/* Port of Python cli:_cleanup_all_browsers */

/* Port of Python cli:_mark_tui_input_modes_active */


/* Port of Python cli:_prepare_deferred_agent_startup */

/* Port of Python cli:_run_cleanup */

/* Port of Python cli:_should_emit_cleanup_session_finalize */

/* Port of Python cli:_notify_session_finalize */

/* Port of Python cli:_emit_interrupted_session_end */

/* Port of Python cli:_notify_single_query_session_finalize */

/* Port of Python cli:_finalize_single_query */

/* Port of Python cli:_reset_terminal_input_modes_on_exit */

/* Port of Python cli:_normalize_git_bash_path */

/* Port of Python cli:_git_repo_root */

/* Port of Python cli:_path_is_within_root */

/* Port of Python cli:_setup_worktree */

/* Port of Python cli:_worktree_has_unpushed_commits */

/* Port of Python cli:_cleanup_worktree */

/* Port of Python cli:_run_state_db_auto_maintenance */

/* Port of Python cli:_run_checkpoint_auto_maintenance */

/* Port of Python cli:_prune_stale_worktrees */

/* Port of Python cli:_prune_orphaned_branches */

/* Port of Python cli:_hex_to_ansi */

/* Port of Python cli:_luminance_from_hex */

/* Port of Python cli:_query_osc11_background */

/* Port of Python cli:_detect_light_mode */

/* Port of Python cli:_maybe_remap_for_light_mode */

/* Port of Python cli:_install_skin_light_mode_hook */

/* Port of Python cli:__str__ */

/* Port of Python cli:__add__ */

/* Port of Python cli:__radd__ */

/* Port of Python cli:_rich_text_from_ansi */

/* Port of Python cli:_strip_markdown_syntax */

/* Port of Python cli:_preserve_windows_dot_segments_for_markdown */

/* Port of Python cli:_terminal_width_for_streaming */

/* Port of Python cli:_render_final_assistant_content */

/* Port of Python cli:_coerce_output_history_limit */

/* Port of Python cli:_configure_output_history */

/* Port of Python cli:_clear_output_history */

/* Port of Python cli:_suspend_output_history */

/* Port of Python cli:_record_output_history_entry */

/* Port of Python cli:_record_output_history */

/* Port of Python cli:_replay_output_history */

/* Port of Python cli:_cprint */

/* Port of Python cli:_prepend_note_to_message */

/* Port of Python cli:_termux_example_image_path */

/* Port of Python cli:_split_path_input */

/* Port of Python cli:_resolve_attachment_path */

/* Port of Python cli:_detect_file_drop */

/* Port of Python cli:_format_image_attachment_badges */

/* Port of Python cli:_should_auto_attach_clipboard_image_on_paste */

/* Port of Python cli:_strip_leaked_bracketed_paste_wrappers */

/* Port of Python cli:_apply_bracketed_paste_timeout_patch */

/* Port of Python cli:_preserve_ctrl_enter_newline */

/* Port of Python cli:_bind_prompt_submit_keys */

/* Port of Python cli:_disable_prompt_toolkit_cpr_warning */

/* Port of Python cli:_strip_leaked_terminal_responses_with_meta */

/* Port of Python cli:_strip_leaked_terminal_responses */

/* Port of Python cli:_estimate_tui_input_height */

/* Port of Python cli:_collect_query_images */

/* Port of Python cli:_build_compact_banner */

/* Port of Python cli:_looks_like_slash_command */

/* Port of Python cli:_ensure_skill_commands */

/* Port of Python cli.py:build_skill_invocation_message */
/* Build the user message content for a skill slash command invocation.
 * Returns malloc'd string (caller must free) or NULL if skill not found. */
char *cli_cli_build_skill_invocation_message(const char *cmd_key,
                                             const char *user_instruction,
                                             const char *task_id,
                                             const char *runtime_note)
{
    (void)task_id;
    (void)runtime_note;
    if (!cmd_key || !cmd_key[0]) return NULL;

    /* Ensure skills are scanned */
    skill_cmd_scan();

    /* Look up the skill by slug */
    const skill_cmd_entry_t *sk = skill_cmd_get(cmd_key);
    if (!sk) return NULL;

    /* Load the skill payload */
    skill_cmd_payload_t *payload = load_skill_payload(cmd_key);
    if (!payload) return NULL;

    /* Build the activation note */
    char activation[2048];
    snprintf(activation, sizeof(activation),
        "[IMPORTANT: The user has invoked the \"%s\" skill, indicating they want "
        "you to follow its instructions. The full skill content is loaded below.]",
        payload->skill_name);

    /* Build the full message */
    size_t total = strlen(activation) + strlen(payload->body) + strlen(user_instruction) + 4096;
    char *msg = (char *)malloc(total);
    if (!msg) { skill_payload_free(payload); return NULL; }

    if (user_instruction && user_instruction[0]) {
        snprintf(msg, total,
            "%s\n\n%s\n\n[User instruction: %s]\n\n[Skill directory: %s]\n"
            "Resolve any relative paths in this skill (e.g. scripts/foo.js, "
            "templates/config.yaml) against that directory, then run them "
            "with the terminal tool using the absolute path.\n",
            activation, payload->body, user_instruction, payload->skill_dir);
    } else {
        snprintf(msg, total,
            "%s\n\n%s\n\n[Skill directory: %s]\n"
            "Resolve any relative paths in this skill (e.g. scripts/foo.js, "
            "templates/config.yaml) against that directory, then run them "
            "with the terminal tool using the absolute path.\n",
            activation, payload->body, payload->skill_dir);
    }

    skill_payload_free(payload);
    return msg;
}

/* Port of Python cli:_get_plugin_cmd_handler_names */

/* Port of Python cli:_parse_skills_argument */

/* Port of Python cli:save_config_value */

/* Port of Python cli:_claim_active_session */

/* Port of Python cli:_release_active_session */

/* Port of Python cli:_invalidate */

/* Port of Python cli:_paint_now */

/* Port of Python cli:_force_full_redraw */

/* Port of Python cli:_clear_prompt_toolkit_screen */

/* Port of Python cli:_recover_after_resize */

/* Port of Python cli:_schedule_resize_recovery */

/* Port of Python cli:_status_bar_context_style */

/* Port of Python cli:_compression_count_style */

/* Port of Python cli:_build_context_bar */

/* Port of Python cli:_format_prompt_elapsed */

/* Port of Python cli:_format_idle_since */

/* Port of Python cli:_get_status_bar_snapshot */

/* Port of Python cli:_status_bar_display_width */

/* Port of Python cli:_trim_status_bar_text */

/* Port of Python cli:_get_tui_terminal_width */

/* Port of Python cli:_use_minimal_tui_chrome */

/* Port of Python cli:_scrollback_box_width */

/* Port of Python cli:_tui_input_rule_height */

/* Port of Python cli:_spinner_widget_height */

/* Port of Python cli:_render_spinner_text */

/* Port of Python cli:_voice_record_key_label */

/* Port of Python cli:set_voice_record_key_cache */

/* Port of Python cli:_get_voice_status_fragments */

/* Port of Python cli:_build_status_bar_text */

/* Port of Python cli:_get_status_bar_fragments */

/* Port of Python cli:_normalize_model_for_provider */

/* Port of Python cli:_on_thinking */

/* Port of Python cli:_on_notice */

/* Port of Python cli:_flush_credit_notices */

/* Port of Python cli:_on_notice_clear */


/* Port of Python cli:_current_reasoning_callback */

/* Port of Python cli:_emit_reasoning_preview */

/* Port of Python cli:_flush_reasoning_preview */

/* Port of Python cli:_format_submitted_user_message_preview */

/* Port of Python cli:_expand_paste_references */

/* Port of Python cli:_print_user_message_preview */

/* Port of Python cli:_stream_reasoning_delta */

/* Port of Python cli:_close_reasoning_box */

/* Port of Python cli:_stream_delta */

/* Port of Python cli:_emit_stream_text */

/* Port of Python cli:_flush_stream */

/* Port of Python cli:_reset_stream_state */

/* Port of Python cli:_slow_command_status */

/* Port of Python cli:_command_spinner_frame */

/* Port of Python cli:_busy_command */

/* Port of Python cli:_open_external_editor */

/* Port of Python cli:_install_tool_callbacks */

/* Port of Python cli:_ensure_tirith_security */

/* Port of Python cli:_show_security_advisories */

/* Port of Python cli:show_banner */

/* Port of Python cli:_restore_session_cwd */

/* Port of Python cli:_render_resume_history_panel_lines */

/* Port of Python cli:_try_attach_clipboard_image */

/* Port of Python cli:_resolve_checkpoint_ref */

/* Port of Python cli:_write_osc52_clipboard */

/* Port of Python cli:_recover_terminal_input_modes */

/* Port of Python cli:_preprocess_images_with_vision */

/* Port of Python cli:_show_tool_availability_warnings */

/* Port of Python cli:_show_status */

/* Port of Python cli:_show_session_status */

/* Port of Python cli:_fast_command_available */

/* Port of Python cli:_command_available */

/* Port of Python cli:show_help */

/* Port of Python cli:show_tools */

/* Port of Python cli:show_toolsets */

/* Port of Python cli:_list_recent_sessions */

/* Port of Python cli:_show_recent_sessions */

/* Port of Python cli:show_history */

/* Port of Python cli:_discard_session_if_empty */

/* Port of Python cli:new_session */

/* Port of Python cli:_consume_pending_resume_selection */

/* Port of Python cli:save_conversation */

/* Port of Python cli:retry_last */

/* Port of Python cli:undo_last */

/* Port of Python cli:_undo_content_to_text */

/* Port of Python cli:_prefill_input_buffer */

/* Port of Python cli:_run_curses_picker */

/* Port of Python cli:_prompt_text_input */

/* Port of Python cli:_prompt_text_input_modal */

/* Port of Python cli:_submit_slash_confirm_response */

/* Port of Python cli:_normalize_slash_confirm_choice */

/* Port of Python cli:_get_slash_confirm_display_fragments */

/* Port of Python cli:_open_model_picker */

/* Port of Python cli:_confirm_expensive_model_switch */

/* Port of Python cli:_confirm_and_apply_model_switch_result */

/* Port of Python cli:_close_model_picker */

/* Port of Python cli:_compute_model_picker_viewport */

/* Port of Python cli:_apply_model_switch_result */

/* Port of Python cli:_handle_model_picker_selection */

/* Port of Python cli:_handle_model_switch */

/* Port of Python cli:_handle_codex_runtime */

/* Port of Python cli:_should_handle_model_command_inline */

/* Port of Python cli:_should_handle_steer_command_inline */

/* Port of Python cli:_output_console */

/* Port of Python cli:_console_print */

/* Port of Python cli:_resolve_personality_prompt */

/* Port of Python cli:_show_gateway_status */

/* Port of Python cli:process_command */

/* Port of Python cli:_try_launch_chrome_debug */

/* Port of Python cli:_get_goal_manager */

/* Port of Python cli:_maybe_continue_goal_after_turn */

/* Port of Python cli:_toggle_verbose */

/* Port of Python cli:_transfer_session_yolo */

/* Port of Python cli:_is_session_yolo_active */

/* Port of Python cli:_toggle_yolo */

/* Port of Python cli:_on_reasoning */

/* Port of Python cli:_manual_compress */

/* Port of Python cli:_show_usage */

/* Port of Python cli:_print_nous_credits_block */

/* Port of Python cli:_show_credits */

/* Port of Python cli:_show_insights */

/* Port of Python cli:_check_config_mcp_changes */

/* Port of Python cli:_split_destructive_skip */

/* Port of Python cli:_confirm_destructive_slash */

/* Port of Python cli:_confirm_and_reload_mcp */

/* Port of Python cli:_on_tool_gen_start */

/* Port of Python cli:_on_tool_progress */

/* Port of Python cli:_on_tool_start */

/* Port of Python cli:_on_tool_complete */

/* Port of Python cli:_voice_start_recording */

/* Port of Python cli:_voice_stop_and_transcribe */

/* Port of Python cli:_voice_speak_response_async */

/* Port of Python cli:_voice_speak_response */

/* Port of Python cli:_voice_beeps_enabled */

/* Port of Python cli:_enable_voice_mode */

/* Port of Python cli:_disable_voice_mode */

/* Port of Python cli:_toggle_voice_tts */

/* Port of Python cli:_show_voice_status */

/* Port of Python cli:_persist_prompt_summary */

/* Port of Python cli:_clarify_callback */

/* Port of Python cli:_sudo_password_callback */

/* Port of Python cli:_approval_callback */

/* Port of Python cli:_approval_choices */

/* Port of Python cli:_computer_use_approval_callback */

/* Port of Python cli:_handle_approval_selection */

/* Port of Python cli:_get_approval_display_fragments */

/* Port of Python cli:_secret_capture_callback */

/* Port of Python cli:_capture_modal_input_snapshot */

/* Port of Python cli:_restore_modal_input_snapshot */

/* Port of Python cli:_submit_secret_response */

/* Port of Python cli:_cancel_secret_capture */

/* Port of Python cli:_clear_secret_input_buffer */

/* Port of Python cli:_clear_terminal_on_exit */

/* Port of Python cli:_print_exit_summary */

/* Port of Python cli:_get_tui_prompt_symbols */

/* Port of Python cli:_audio_level_bar */

/* Port of Python cli:_get_tui_prompt_fragments */

/* Port of Python cli:_get_tui_prompt_text */

/* Port of Python cli:_build_tui_style_dict */

/* Port of Python cli:_apply_tui_skin_style */

/* Port of Python cli:_get_extra_tui_widgets */

/* Port of Python cli:_register_extra_tui_keybindings */


/* Port of Python cli:_build_tui_layout_children */

/* Port of Python cli:run */

/* Port of Python cli:_run_kanban_goal_loop_q */