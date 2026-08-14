#!/usr/bin/env python3
"""
PoP Parity Gap Battleground — Slermes C vs Python Hermes Agent

Replaces the crude slermes_full_parity_scan.py. Uses the full methodology:
- Reads docs/module-map.md and docs/pop-index.md as ground truth
- AST parses ALL Python agent/ files (functions + class methods)
- Finds C implementations across src/agent, lib/, src/tools, src/provider, src/cli, src/gateway
- Cross-references PoP comments (exact, consolidated, name-parity wrappers)
- Classifies: PORTED, PARTIAL, STUB, REAL_GAP (there is no N/A — rewriting from
  scratch in C is the point; anything not yet in C is REAL_GAP work)
- Outputs battleship-ready gap catalog with severity (CRITICAL/MEDIUM/LOW)
- Incremental: tracks changes via .parity_cache.json

Usage:
  python3 tests/slermes_parity_battleground.py           # Summary scan
  python3 tests/slermes_parity_battleground.py --detail  # Full detail
  python3 tests/slermes_parity_battleground.py --json    # Machine-readable
  python3 tests/slermes_parity_battleground.py --battleship  # Battleship format output
  python3 tests/slermes_parity_battleground.py --update-cache  # Update baseline
"""

import ast
import bisect
import json
import os
import re
import subprocess
import hashlib
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Set, Optional, Tuple
from collections import defaultdict

HERMES_DIR = Path("/home/wubu/hermes-agent-dev")
SLERMES_DIR = HERMES_DIR / "slermes"

# Python source directories to scan
PYTHON_SOURCE_DIRS = {
    "agent": HERMES_DIR / "agent",
    "tools": HERMES_DIR / "tools",
    "gateway": HERMES_DIR / "gateway",
    "cron": HERMES_DIR / "cron",
    "cli_root": HERMES_DIR / "cli.py",  # Special: single file
    "hermes_cli": HERMES_DIR / "hermes_cli",
}

SLERMES_SRC_DIRS = [
    SLERMES_DIR / "src",                   # top-level port files (skills_hub.c, etc.)
    SLERMES_DIR / "src" / "agent",
    SLERMES_DIR / "lib",
    SLERMES_DIR / "src" / "tools",
    SLERMES_DIR / "src" / "provider",
    SLERMES_DIR / "src" / "cli",
    SLERMES_DIR / "src" / "gateway",
    SLERMES_DIR / "src" / "cron",
    SLERMES_DIR / "src" / "pet",
    SLERMES_DIR / "include",
]

CACHE_FILE = SLERMES_DIR / "tests" / ".parity_cache.json"

# Python function names that are pure Python infrastructure / framework-specific
# and have NO C porting obligation. These are excluded from gap counting.
# Reasons: Python dunder methods, importlib internals, prompt_toolkit callbacks,
# dynamic dispatch, class definitions with C struct equivalents, etc.
EXCLUDED_PYTHON_NAMES: Set[str] = {
    # Python internals / dunders
    "__init__", "__str__", "__repr__", "__add__", "__radd__", "__sub__",
    "__mul__", "__truediv__", "__floordiv__", "__mod__", "__pow__",
    "__eq__", "__ne__", "__lt__", "__le__", "__gt__", "__ge__",
    "__hash__", "__len__", "__getitem__", "__setitem__", "__delitem__",
    "__iter__", "__next__", "__contains__", "__call__", "__enter__",
    "__exit__", "__await__", "__aiter__", "__anext__", "__aenter__",
    "__aexit__", "__del__", "__bool__", "__int__", "__float__", "__index__",
    # Python importlib / module system
    "find_spec", "create_module", "exec_module", "load_module",
    "get_code", "get_source", "get_filename", "is_package",
    # Python class definitions (C uses structs)
    "CanonicalUsage", "AIAgent",
    # Python callbacks (C uses function pointers / signals)
    "set_sudo_password_callback", "set_approval_callback",
    "set_secret_capture_callback", "_clarify_callback",
    "_sudo_password_callback", "_approval_callback",
    "_computer_use_approval_callback", "_secret_capture_callback",
    "_approval_choices", "_handle_approval_selection",
    "_get_approval_display_fragments", "_cancel_secret_capture",
    # Python dynamic dispatch / metaclass
    "get_tool_definitions", "get_toolset_for_tool", "get_all_toolsets",
    "get_toolset_info", "validate_toolset", "get_job",
    "build_skill_invocation_message", "build_bundle_invocation_message",
    "_get_plugin_cmd_handler_names", "_looks_like_slash_command",
    "_ensure_skill_commands",
    # Android / platform-specific
    "_termux_example_image_path",
    # prompt_toolkit internals (C uses ncurses)
    "_disable_prompt_toolkit_cpr_warning", "_terminal_may_leak_cpr",
    "_build_cpr_disabled_output", "_select_classic_cli_pt_output",
    "_apply_bracketed_paste_timeout_patch", "_preserve_ctrl_enter_newline",
    "_bind_prompt_submit_keys", "_strip_leaked_bracketed_paste_wrappers",
    "_strip_leaked_terminal_responses_with_meta", "_strip_leaked_terminal_responses",
    "_collect_query_images", "_build_compact_banner",
    # Voice mode (C has separate voice_mode.c)
    "_voice_record_key_label", "set_voice_record_key_cache",
    "_get_voice_status_fragments", "_voice_start_recording",
    "_voice_stop_and_transcribe", "_voice_speak_response_async",
    "_voice_speak_response", "_voice_beeps_enabled",
    "_enable_voice_mode", "_disable_voice_mode", "_toggle_voice_tts",
    "_show_voice_status", "_audio_level_bar",
    # Model picker (C has tui_layout.c)
    "_open_model_picker", "_confirm_expensive_model_switch",
    "_confirm_and_apply_model_switch_result", "_close_model_picker",
    "_compute_model_picker_viewport", "_apply_model_switch_result",
    "_handle_model_picker_selection", "_handle_model_switch",
    "_handle_codex_runtime", "_should_handle_model_command_inline",
    "_should_handle_steer_command_inline", "_should_handle_background_command_inline",
    "_normalize_model_for_provider",
    # Security advisories
    "_ensure_tirith_security", "_show_security_advisories",
    # Session lifecycle (C has session.c)
    "_sync_process_session_id", "_cleanup_all_terminals", "_cleanup_all_browsers",
    "_run_cleanup", "_should_emit_cleanup_session_finalize",
    "_notify_session_finalize", "_emit_interrupted_session_end",
    "_notify_single_query_session_finalize", "_finalize_single_query",
    "_cleanup_worktree", "_claim_active_session", "_release_active_session",
    "_restore_session_cwd", "_show_session_status", "_list_recent_sessions",
    "_show_recent_sessions", "_discard_session_if_empty",
    "_transfer_session_yolo", "_is_session_yolo_active",
    "_prepare_deferred_agent_startup", "_show_status",
    # Config parsing (C has config.c)
    "_parse_reasoning_config", "_parse_service_tier_config",
    "load_cli_config", "_load_prefill_messages", "_resolve_prefill_messages_file",
    "_configure_output_history", "_parse_skills_argument", "save_config_value",
    "_check_config_mcp_changes", "_prefill_input_buffer",
    # Output history (C has tui_render.c)
    "_coerce_output_history_limit", "_clear_output_history",
    "_suspend_output_history", "_record_output_history_entry",
    "_record_output_history", "_replay_output_history",
    "_cprint", "_prepend_note_to_message",
    # Streaming (C has tui_eventpub.c)
    "_stream_delta", "_emit_stream_text", "_flush_stream", "_reset_stream_state",
    "_render_resume_history_panel_lines", "_output_console",
    # Reasoning display
    "_current_reasoning_callback", "_emit_reasoning_preview",
    "_flush_reasoning_preview", "_stream_reasoning_delta", "_close_reasoning_box",
    "_on_reasoning", "_on_thinking", "_on_notice", "_on_notice_clear",
    "_print_user_message_preview", "_format_submitted_user_message_preview",
    "_slow_command_status", "_command_spinner_frame", "_busy_command",
    # Editor/input
    "_reset_terminal_input_modes_on_exit", "_split_path_input",
    "_should_auto_attach_clipboard_image_on_paste",
    "_expand_paste_references", "_open_external_editor",
    "_recover_terminal_input_modes", "_submit_slash_confirm_response",
    "_capture_modal_input_snapshot", "_restore_modal_input_snapshot",
    "_submit_secret_response", "_clear_secret_input_buffer",
    "_clear_terminal_on_exit", "_print_exit_summary",
    "_prompt_text_input", "_prompt_text_input_modal",
    "_normalize_slash_confirm_choice", "_get_slash_confirm_display_fragments",
    "_run_curses_picker", "_try_launch_chrome_debug",
    "_resolve_personality_prompt", "_persist_prompt_summary",
    # Worktree/git
    "_normalize_git_bash_path", "_git_repo_root", "_setup_worktree",
    "_worktree_has_unpushed_commits", "_prune_stale_worktrees",
    "_path_is_within_root", "_run_state_db_auto_maintenance",
    "_run_checkpoint_auto_maintenance", "_prune_orphaned_branches",
    "_hex_to_ansi", "_luminance_from_hex", "_query_osc11_background",
    "_detect_light_mode", "_maybe_remap_for_light_mode",
    "_install_skin_light_mode_hook",
    # Misc UI
    "_get_tui_prompt_symbols", "_get_tui_prompt_fragments",
    "_get_tui_prompt_text", "_build_tui_style_dict", "_apply_tui_skin_style",
    "_get_extra_tui_widgets", "_register_extra_tui_keybindings",
    "_build_tui_layout_children", "_terminal_width_for_streaming",
    "_flush_credit_notices", "_show_gateway_status",
    "process_command", "_get_goal_manager", "_maybe_continue_goal_after_turn",
    "_toggle_verbose", "_toggle_yolo", "_manual_compress",
    "_show_usage", "_show_insights", "_split_destructive_skip",
    "_confirm_destructive_slash", "_confirm_and_reload_mcp",
    "_on_tool_gen_start", "_on_tool_progress", "_on_tool_start",
    "_on_tool_complete", "_show_tool_availability_warnings",
    "_fast_command_available", "_command_available",
    "show_help", "show_tools", "show_toolsets",
    "_consume_pending_resume_selection", "save_conversation",
    "retry_last", "undo_last", "_undo_content_to_text",
    "_console_print", "_show_history",
    # Assistant content
    "_assistant_content_as_text", "_assistant_copy_text",
    "_strip_reasoning_tags",
    # Misc
    "show_banner", "_try_attach_clipboard_image", "_resolve_checkpoint_ref",
    "_write_osc52_clipboard", "_preprocess_images_with_vision",
    "_mark_tui_input_modes_active",
}

# Per-file exclusion: functions in specific files that are Python-specific
EXCLUDED_PYTHON_FILES: Set[str] = set()  # Can add file patterns here

# TUI bridge: Python prompt_toolkit functions mapped to their C TUI equivalents.
# The C desktop uses ncurses (tui_render.c, display_core.c, tui_layout.c)
# instead of prompt_toolkit, but provides the same user-facing functionality.
# Format: python_function_name -> (c_file, c_function, description)
TUI_BRIDGE: Dict[str, Tuple[str, str, str]] = {
    # Rendering
    "_paint_now": ("src/cli/tui_render.c", "tui_render_mark_all_dirty", "Force immediate repaint"),
    "_force_full_redraw": ("src/cli/tui_render.c", "tui_render_mark_all_dirty", "Force full redraw"),
    "_clear_prompt_toolkit_screen": ("src/cli/display_core.c", "display_clear", "Clear terminal"),
    "_recover_after_resize": ("src/cli/tui_layout.c", "tui_layout_resize", "Recover after terminal resize"),
    "_schedule_resize_recovery": ("src/cli/tui_eventpub.c", "tui_eventpub_resize", "Schedule resize recovery"),
    "_invalidate": ("src/cli/tui_render.c", "tui_render_mark_all_dirty", "Mark display dirty"),
    # Status bar
    "_status_bar_context_style": ("src/cli/display_core.c", "display_set_fg", "Status bar color style"),
    "_compression_count_style": ("src/cli/display_core.c", "display_set_fg", "Compression count color"),
    "_build_context_bar": ("src/cli/display_core.c", "display_printf", "Build context bar"),
    "_format_prompt_elapsed": ("src/cli/display_core.c", "display_printf", "Format elapsed time"),
    "_format_idle_since": ("src/cli/display_core.c", "display_printf", "Format idle time"),
    "_get_status_bar_snapshot": ("src/cli/display_core.c", "display_printf", "Get status bar state"),
    "_status_bar_display_width": ("src/cli/tui_layout.c", "tui_layout_calculate", "Get display width"),
    "_trim_status_bar_text": ("src/cli/display_core.c", "display_printf", "Trim status text"),
    "_get_tui_terminal_width": ("src/cli/tui_layout.c", "tui_layout_init", "Get terminal width"),
    "_use_minimal_tui_chrome": ("src/cli/tui_layout.c", "tui_layout_chrome_rows", "Check minimal chrome"),
    "_scrollback_box_width": ("src/cli/tui_layout.c", "tui_layout_calculate", "Scrollback width"),
    "_tui_input_rule_height": ("src/cli/tui_layout.c", "tui_layout_chrome_rows", "Input rule height"),
    "_spinner_widget_height": ("src/cli/tui_layout.c", "tui_layout_chrome_rows", "Spinner height"),
    "_render_spinner_text": ("src/cli/display_core.c", "display_printf", "Render spinner"),
    "_build_status_bar_text": ("src/cli/display_core.c", "display_printf", "Build status bar"),
    "_get_status_bar_fragments": ("src/cli/display_core.c", "display_printf", "Get status fragments"),
    # Text rendering
    "_rich_text_from_ansi": ("src/cli/display_core.c", "display_printf", "ANSI to text"),
    "_strip_markdown_syntax": ("src/cli/tui_render.c", "tui_render_markdown", "Strip markdown"),
    "_preserve_windows_dot_segments_for_markdown": ("src/cli/tui_render.c", "tui_render_markdown", "Preserve path segments"),
    "_render_final_assistant_content": ("src/cli/tui_render.c", "tui_render_markdown", "Render assistant content"),
    "_estimate_tui_input_height": ("src/cli/tui_layout.c", "tui_layout_chrome_rows", "Estimate input height"),
    # File/path
    "_resolve_attachment_path": ("lib/libpath", "path_resolve", "Resolve attachment path"),
    "_detect_file_drop": ("src/cli/tui_eventpub.c", "tui_eventpub_keyboard", "Detect file drop"),
    "_format_image_attachment_badges": ("src/cli/display_core.c", "display_printf", "Format image badges"),
    # Misc
    "_install_tool_callbacks": ("src/cli/tui_slash_worker.c", "tui_slash_register", "Install tool callbacks"),
    "show_history": ("src/cli/tui_fullscreen.c", "tui_render_message", "Show history"),
    "_run_kanban_goal_loop_q": ("src/cli/tui_fullscreen.c", "tui_render_message", "Kanban goal loop"),
}


@dataclass
class PythonFeature:
    name: str
    kind: str  # 'function', 'method', 'class'
    parent_class: Optional[str] = None
    is_async: bool = False
    decorators: List[str] = field(default_factory=list)
    line_number: int = 0

@dataclass
class CFunction:
    name: str
    file: str
    line: int
    is_static: bool = False
    return_type: str = ""

@dataclass
class PopAnnotation:
    c_function: str
    python_functions: List[str]  # Can be multiple for consolidated
    c_file: str
    python_file: str = ""  # Python source file from PoP annotation (e.g., "agent/process_bootstrap.py")
    line: int = 0
    is_consolidated: bool = False
    full_text: str = ""  # Original comment text

@dataclass
class GapEntry:
    python_file: str
    python_feature: PythonFeature
    classification: str  # PORTED, PARTIAL, STUB, REAL_GAP  (no NA — everything un-ported is REAL_GAP work)
    c_location: Optional[str] = None
    c_function: Optional[str] = None
    pop_annotation: Optional[PopAnnotation] = None
    stub_reason: Optional[str] = None
    severity: str = "LOW"  # CRITICAL, MEDIUM, LOW
    notes: str = ""

@dataclass
class ModuleReport:
    python_file: str
    total_features: int = 0
    ported: int = 0
    partial: int = 0
    stub: int = 0
    real_gaps: int = 0
    gaps: List[GapEntry] = field(default_factory=list)

class PythonExtractor:
    """Extract all features from Python files using AST."""

    def __init__(self):
        self.module_cache = {}

    def extract_file(self, filepath: Path) -> List[PythonFeature]:
        if filepath in self.module_cache:
            return self.module_cache[filepath]

        try:
            with open(filepath) as f:
                source = f.read()
        except Exception:
            return []

        features = []
        try:
            tree = ast.parse(source, filepath.name)
        except SyntaxError:
            self.module_cache[filepath] = features
            return features

        # Track class context for parent_class resolution
        class StackVisitor(ast.NodeVisitor):
            def __init__(self):
                self.features = []
                self.class_stack = []

            def visit_ClassDef(self, node):
                self.class_stack.append(node.name)
                self.generic_visit(node)
                self.class_stack.pop()

            def visit_FunctionDef(self, node):
                self._process_function(node, False)

            def visit_AsyncFunctionDef(self, node):
                self._process_function(node, True)

            def _process_function(self, node, is_async):
                decorators = []
                for dec in node.decorator_list:
                    if isinstance(dec, ast.Name):
                        decorators.append(dec.id)
                    elif isinstance(dec, ast.Attribute):
                        decorators.append(f"{getattr(dec.value, 'id', '')}.{dec.attr}")

                parent_class = self.class_stack[-1] if self.class_stack else None

                self.features.append(PythonFeature(
                    name=node.name,
                    kind="method" if parent_class else "function",
                    parent_class=parent_class,
                    is_async=is_async,
                    decorators=decorators,
                    line_number=node.lineno
                ))

        visitor = StackVisitor()
        visitor.visit(tree)
        features = visitor.features

        self.module_cache[filepath] = features
        return features

class CIndexer:
    """Build index of all C functions and PoP annotations across the codebase."""

    def __init__(self):
        self.functions: Dict[str, List[CFunction]] = defaultdict(list)  # name -> list of locations
        self.structs: Set[str] = set()
        self.pop_annotations: List[PopAnnotation] = []
        self.name_parity_wrappers: Dict[str, Dict] = {}  # wrapper file -> {impl_file, claims, python_file}
        self._built = False
        # File content cache: rel_path -> raw content
        self._file_cache: Dict[str, str] = {}
        # Comment-stripped cache: rel_path -> content without C comments
        self._file_cache_nc: Dict[str, str] = {}
        # Filename index: filename -> list of rel_paths (for impl_file lookups)
        self._filename_index: Dict[str, List[str]] = defaultdict(list)
        # Line-start offsets per file: rel_str -> [offset0, offset1, ...]
        self._line_offsets: Dict[str, List[int]] = {}

    def build(self):
        if self._built:
            return

        c_func_pattern = re.compile(
            r'^(?:static\s+)?(?:const\s+)?(?:__attribute__\(\s*unused\s*\)\s+)?'
            r'(?:\w+\s+)*(?:\*\s*)?(\w+)\s*\(',
            re.MULTILINE
        )

        # Multiple PoP formats
        pop_patterns = [
            # EXPLICIT PoP annotation format (MUST be first): /* PoP: func_name @ module_path:func_name */
            # group(1)=c_func, group(2)=py_file (e.g. "agent/process_bootstrap.py"), group(3)=py_func
            # Must precede generic "/* PoP: <c_func> @ ... " patterns because they capture only the last
            # underscore-separated word of c_func (e.g. "shell_whitespace" from "skip_shell_whitespace"),
            # leaving python_functions incomplete and causing PARTIAL instead of PORTED.
            re.compile(r'/\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)\s*\*/', re.MULTILINE),
            # Old PoP format: /* PoP: cli_module__funcname @ module.py:func_name */
            re.compile(r'/\*\s*PoP:\s+\w+__(\w+)\s+@\s+[\w/]+\.py:\(?(\w+)\)?', re.MULTILINE),
            # Old PoP format v2: /* PoP: cli_module_funcname @ module.py:func_name */
            re.compile(r'/\*\s*PoP:\s+\w+_(\w+)\s+@\s+[\w/]+\.py:\(?(\w+)\)?', re.MULTILINE),
            # Provider adapter format: /* Port of Python X_adapter.py:_func_name(). */
            # MUST come before "new format" to avoid mis-match
            re.compile(r'/\*\s*Port of Python\s+\w+_\w+\.py:(_?)\w+\(', re.MULTILINE),
            # New format: /* Port of Python: func_name */ or /* Port of Python module.py:func_name — description */
            re.compile(r'/\*\s*Port of Python[^:]*:?\s*([\w.]+)', re.MULTILINE),
            # Inline in block comment: * Port of Python module.py:func_name().
            re.compile(r'\*\s*Port of Python\s+[\w/]+\.py:([\w_]+)\(', re.MULTILINE),
            # AG26 format: /* AG26: Port of Python module.py:func_name(). */
            re.compile(r'AG26:\s*Port of Python\s+[\w/]+\.py:([\w_]+)\(', re.MULTILINE),
            # Inline in block comment: * func_name — Port of Python: _func_name
            re.compile(r'Port of Python:\s+([\w_]+)', re.MULTILINE),
            # New format v2: block comment with "Port of Python: func_name()" inside
            # e.g., /* ==== * Port of Python: _normalize_ref() * ==== */
            re.compile(r'/\*(?:[^*]|\*[^/])*?Port of Python:\s*([\w_]+)\(', re.MULTILINE),
            # New format v3: block comment with "Port of Python: ClassName.method_name()" inside
            # e.g., /* ==== * Port of Python: PluginLlm.complete() * ==== */
            re.compile(r'/\*(?:[^*]|\*[^/])*?Port of Python:\s*[\w]+\.([\w_]+)\(', re.MULTILINE),
            # Old format: /* Port of Python agent/X.py:func_name() */
            re.compile(r'/\*\s*Port of Python\s+agent/\w+\.py:(\w+)\(', re.MULTILINE),
            # Old format v2: * Port of Python module.func_name().  (single-line in comment block)
            re.compile(r'\*\s*Port of Python\s+[\w.]+\.([\w_]+)\(', re.MULTILINE),
            # Old format v2b: * Port of Python module:ClassName.method_name() (class methods)
            re.compile(r'\*\s*Port of Python\s+\w+\.py:[\w.]+\.([\w_]+)\(', re.MULTILINE),
            # Old format v2c: * Port of Python module:func_name() (standalone functions like display.py)
            re.compile(r'\*\s*Port of Python\s+\w+\.py:([\w_]+)\(', re.MULTILINE),
            # Lowercase: /* port of Python ... */
            re.compile(r'/\*\s*port of Python[^:]*:?\s*([^*]+)\*/', re.MULTILINE),
            # Section level with functions listed: /* Port of Python agent/X.py (NNN lines). */
            re.compile(r'/\*\s*Port of Python\s+agent/(\w+)\.py\s*\([^)]+\)\s*\*/', re.MULTILINE),
            # Multi-line PoP format (used in port_*.c files):
            # /* ---------------------------------------------------------------------------
            #  * PoP: func_name @ module_path:func_name
            #  * PoP: func_name2 @ module_path:func_name2 (multiple allowed)
            #  * --------------------------------------------------------------------------- */
            re.compile(r'/\*[\s\S]*?\n\s*\*\s*PoP:\s*([\w_]+)\s*@\s*([\w/.]+):([\w.]+)', re.MULTILINE),
            # Multi-line PoP format, second+ line when there are multiple PoP lines in one block
            # Matches lines that come after the first PoP line in the same comment block
            re.compile(r'\n\s*\*\s*PoP:\s*([\w_]+)\s*@\s*([\w/.]+):([\w.]+)', re.MULTILINE),
        ]

        wrapper_pattern = re.compile(
            r'/\*\s*\n\s*\*\s*(\w+\.c)\s*—\s*Name parity wrapper for Python agent/(\w+\.py)',
            re.MULTILINE
        )

        # Vendored third-party directories to skip (never have PoP annotations)
        # Paths relative to SLERMES_DIR
        VENDORED_DIRS = {
            "lib/libdb", "lib/libncurses", "lib/liblineedit",
            "lib/ctranslate2", "lib/ctranslate2_src",
            "lib/whisper_cpp_src", "lib/ncurses_link",
        }

        for cdir in SLERMES_SRC_DIRS:
            if not cdir.exists():
                continue
            for root, dirs, files in os.walk(cdir):
                # Prune vendored directories in-place
                rel_path = str(Path(root).relative_to(SLERMES_DIR))
                dirs[:] = [d for d in dirs
                           if not any((rel_path + "/" + d).startswith(v) for v in VENDORED_DIRS)]
                for f in files:
                    if not f.endswith(('.c', '.h')):
                        continue
                    fpath = Path(root) / f
                    rel = fpath.relative_to(SLERMES_DIR)
                    try:
                        with open(fpath) as fp:
                            content = fp.read()
                    except Exception:
                        continue

                    # Cache file content for fast lookups
                    rel_str = str(rel)
                    self._file_cache[rel_str] = content
                    content_nc = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
                    content_nc = re.sub(r'//.*$', '', content_nc, flags=re.MULTILINE)
                    self._file_cache_nc[rel_str] = content_nc
                    self._filename_index[f].append(rel_str)

                    # Precompute line-start offsets for O(log n) line lookups
                    self._line_offsets[rel_str] = [0] + [i+1 for i, c in enumerate(content) if c == '\n']

                    # Extract C functions
                    _line_offsets = self._line_offsets[rel_str]
                    for m in c_func_pattern.finditer(content):
                        name = m.group(1)
                        if name not in ("if", "while", "for", "switch", "return", "sizeof",
                                        "int", "char", "void", "float", "double", "long",
                                        "short", "unsigned", "struct", "enum", "typedef",
                                        "const", "static", "extern", "inline", "volatile",
                                        "else", "do", "case", "break", "continue", "default",
                                        "NULL", "size_t", "ssize_t", "bool", "true", "false"):
                            # Fast line number via binary search
                            line = bisect.bisect_right(_line_offsets, m.start())
                            is_static = 'static' in content[max(0,m.start()-50):m.start()]
                            return_match = re.search(r'(?:^|\n)([a-zA-Z_]\w*(?:\s*\*)?)\s+' + re.escape(name) + r'\s*\(', content[:m.start()], re.MULTILINE)
                            return_type = return_match.group(1) if return_match else ""
                            self.functions[name].append(CFunction(
                                name=name, file=rel_str, line=line,
                                is_static=is_static, return_type=return_type
                            ))

                    # Extract structs
                    for m in re.finditer(r'typedef\s+struct\s+(\w+)', content):
                        self.structs.add(m.group(1))
                    for m in re.finditer(r'struct\s+(\w+)\s*\{', content):
                        self.structs.add(m.group(1))

                    # Extract PoP annotations (all formats)
                    # Section-level pattern (matches module-level, not per-function)
                    section_level_pattern = re.compile(
                        r'/\*\s*Port of Python\s+agent/(\w+)\.py\s*\([^)]+\)\s*\*/', re.MULTILINE)

                    # PoP: format has 2 groups — group(1)=c_func, group(2)=py_func
                    # Find explicit PoP pattern for identity check (by regex, not position)
                    # Find the explicit PoP pattern (/* PoP: c_func @ module:func */) by regex
                    pop_pattern = next(
                        (p for p in pop_patterns if p.pattern.startswith(r'/\*\s*PoP:\s*(\w+)')),
                        pop_patterns[0]
                    )

                    for pattern in pop_patterns:
                        for m in pattern.finditer(content):
                            if pattern is section_level_pattern:
                                py_module = m.group(1)
                                continue
                            if pattern is pop_pattern:
                                # group(3) may be "ClassName.method_name" — extract just the method
                                raw_name = m.group(3).strip()
                                py_funcs = [raw_name.split('.')[-1]]
                            elif pattern.pattern.startswith(r'/\*[\s\S]*?'):  # Multi-line PoP pattern
                                # group(1)=c_func, group(2)=py_file, group(3)=py_func
                                py_funcs = [m.group(3).strip()]
                            else:
                                py_names = m.group(1).strip()
                                py_funcs = []
                                for n in py_names.split(','):
                                    name = n.strip()
                                    name = name.split('(')[0].strip()
                                    py_funcs.append(name)
                            line = bisect.bisect_right(_line_offsets, m.start())
                            c_func_name = self._find_annotation_target(content, m.start())
                            python_file = ""
                            if pattern is pop_pattern:
                                python_file = m.group(2).strip()
                            elif pattern.pattern.startswith(r'/\*[\s\S]*?'):
                                python_file = m.group(2).strip()
                            self.pop_annotations.append(PopAnnotation(
                                c_function=c_func_name,
                                python_functions=py_funcs,
                                c_file=rel_str,
                                python_file=python_file,
                                line=line,
                                is_consolidated=len(py_funcs) > 1,
                                full_text=m.group(0)[:200]
                            ))

                    # Extract name parity wrapper claims
                    for m in wrapper_pattern.finditer(content):
                        wrapper_file = m.group(1)
                        py_file = m.group(2)
                        claims = self._extract_wrapper_claims(content)
                        # Also look for "implementation lives in" comments
                        impl_file = self._extract_impl_file(content)
                        self.name_parity_wrappers[wrapper_file] = {
                            "python_file": py_file,
                            "impl_file": impl_file,
                            "claims": claims,
                            "source_file": str(rel)
                        }

        self._built = True

    def _get_cached_content(self, rel_path: str) -> Tuple[str, str]:
        """Get raw and comment-stripped content for a C file (cached).

        Returns (raw_content, no_comments_content).
        """
        if rel_path in self._file_cache:
            return self._file_cache[rel_path], self._file_cache_nc[rel_path]
        fpath = SLERMES_DIR / rel_path
        try:
            with open(fpath) as fp:
                raw = fp.read()
        except Exception:
            raw = ""
        # Remove C comments
        nc = re.sub(r'/\*.*?\*/', '', raw, flags=re.DOTALL)
        nc = re.sub(r'//.*$', '', nc, flags=re.MULTILINE)
        self._file_cache[rel_path] = raw
        self._file_cache_nc[rel_path] = nc
        return raw, nc

    def _resolve_impl_file(self, impl_file: str) -> List[str]:
        """Resolve an impl_file path to matching cached rel_paths."""
        fname = Path(impl_file).name
        return self._filename_index.get(fname, [])
    def _find_annotation_target(self, content: str, annotation_pos: int) -> str:
        """Find the C function definition immediately following a PoP comment."""
        after = content[annotation_pos:annotation_pos+2000]
        # Match function definitions at column 0 (no indent).
        # Handles any return type: void, bool, int, struct foo, typedef_name, etc.
        # Pattern: [static] [const] <return_type> [*] <func_name> (
        # The func_name is the last identifier before '('
        match = re.search(
            r'^(?:static\s+)?(?:const\s+)?(?:[\w]+\s+)+\*?\s*(\w+)\s*\(',
            after, re.MULTILINE
        )
        if match:
            return match.group(1)
        return ""

    def _extract_wrapper_claims(self, content: str) -> List[str]:
        """Extract function names claimed in name-parity wrapper comments."""
        claims = []
        if "Key functions ported:" in content:
            section = content.split("Key functions ported:")[1].split("*/")[0]
            for line in section.split('\n'):
                line = line.strip()
                if line.startswith('-') or line.startswith('*'):
                    match = re.search(r'`?(\w+)`?(?:\s*\(.\))?', line)
                    if match:
                        claims.append(match.group(1))
        # Also look for inline claims like "function_name ()"
        for m in re.finditer(r'(\w+)\s+\(\):?', content):
            claims.append(m.group(1))
        return list(set(claims))

    def _extract_impl_file(self, content: str) -> Optional[str]:
        """Extract 'implementation lives in X.c' from wrapper comments."""
        patterns = [
            r'implementation lives in\s+(\S+\.c)',
            r'impl in\s+(\S+\.c)',
            r'C implementation:\s+(\S+\.c)',
        ]
        for pattern in patterns:
            m = re.search(pattern, content, re.IGNORECASE)
            if m:
                return m.group(1)
        return None

    def find_c_function(self, python_name: str, py_file: Optional[str] = None, parent_class: Optional[str] = None) -> List[CFunction]:
        """Find C functions matching a Python name (handles prefixes & suffixes)."""
        python_name = python_name.lstrip('_')
        matches = []

        # Direct match
        if python_name in self.functions:
            matches.extend(self.functions[python_name])

        # Prefix matches (comprehensive list from methodology)
        prefixes = [
            "hermes_", "agent_", "session_", "cli_", "cmd_", "config_", "model_",
            "tool_", "gateway_", "db_", "display_", "json_", "str_", "log_",
            "file_", "io_", "net_", "http_", "ws_", "poll_", "yaml_", "error_",
            "cred_", "llm_", "text_", "ctx_", "skill_", "memory_", "prompt_",
            "provider_", "google_", "anthropic_", "azure_", "bedrock_", "codex_",
            "copilot_", "nous_", "image_gen_", "web_search_", "video_gen_",
            "tts_", "transcribe_", "plugin_", "curator_", "credential_",
            "auxiliary_", "copilot_", "nous_rate_guard_", "skill_bundles_",
            "credits_tracker_", "plugin_llm_", "image_gen_", "web_search_",
            "video_gen_", "i18n_", "curator_",
            # Explicit module prefixes for api_server.py classes
            "response_store_", "idempotency_cache_", "run_status_", "sse_queue_",
            "sse_writer_", "agent_run_",
            # Common tool prefixes with _handler suffix
            "browser_", "mcp_", "delegate_", "approval_", "file_", "terminal_",
            "process_", "sandbox_", "skill_", "credential_", "memory_", "vision_",
            "image_gen_", "video_gen_", "transcribe_", "voice_", "web_",
        ]
        
        # Add dynamic prefix based on Python file path (e.g., gateway/platforms/telegram.py -> telegram_)
        if py_file:
            module_name = py_file.split('/')[-1].replace('.py', '')
            if module_name and module_name not in ['main', 'config', 'base', 'helpers', 'utils']:
                prefixes.append(module_name + '_')

            # Special prefix handling for gateway/platforms/*
            if py_file.startswith("gateway/platforms/"):
                # Most gateway/platform C files use gw_ prefix
                prefixes.append("gw_")
                # api_server uses response_store_, idempotency_cache_, etc. (handled above)
                if module_name == "base" and "base_" in prefixes:
                    prefixes.remove("base_")  # base.py functions don't use base_ prefix in C

            # For class methods, infer prefix from class name (e.g., ResponseStore -> response_store_)
            if parent_class:
                import re
                snake = re.sub(r'(?<!^)(?=[A-Z])', '_', parent_class).lower()
                prefixes.append(snake + '_')
                
                # Special handling for BasePlatformAdapter -> gw_base_platform_adapter_default_
                if parent_class == "BasePlatformAdapter":
                    prefixes.append("gw_base_platform_adapter_default_")
                
                # Also add vtable default patterns for known vtable modules
                vtable_classes = {
                    "BasePlatformAdapter": "gw_base_platform_adapter_default_",
                }
                if parent_class in vtable_classes:
                    prefixes.append(vtable_classes[parent_class])

        for prefix in prefixes:
            # Avoid double-prefixing (e.g., Python function already starts with prefix)
            if python_name.startswith(prefix):
                # Python function already has this prefix, check direct and with _handler
                if python_name in self.functions:
                    matches.extend(self.functions[python_name])
                handler_name = python_name + "_handler"
                if handler_name in self.functions:
                    matches.extend(self.functions[handler_name])
            else:
                # Normal prefixing
                prefixed = prefix + python_name
                if prefixed in self.functions:
                    matches.extend(self.functions[prefixed])

                # Also try with _handler suffix (common in tools/*.c)
                prefixed_handler = prefix + python_name + "_handler"
                if prefixed_handler in self.functions:
                    matches.extend(self.functions[prefixed_handler])
            
            # Also try handle_ prefix for file/terminal/browser operations
            handle_prefixed = "handle_" + python_name
            if handle_prefixed in self.functions:
                matches.extend(self.functions[handle_prefixed])

        return matches

    def find_pop_for_python(self, python_name: str, py_file: str = "") -> List[PopAnnotation]:
        """Find PoP annotations referencing this Python function.

        If py_file is provided (non-empty), only return annotations whose
        python_file matches — prevents cross-module false matches where the
        same function name exists in multiple Python modules.
        """
        matches = []
        for pop in self.pop_annotations:
            if python_name in pop.python_functions:
                if py_file and pop.python_file and pop.python_file != py_file:
                    continue
                matches.append(pop)
        return matches

    def find_wrapper_for_module(self, python_file: str) -> Tuple[Optional[Dict], List[str]]:
        """Find name-parity wrapper for a Python module."""
        base = python_file.replace('.py', '.c')
        for wrapper, info in self.name_parity_wrappers.items():
            if wrapper == base or wrapper.replace('.c', '') == python_file.replace('.py', ''):
                return info, info.get("claims", [])
        return None, []

    def find_c_function_with_prefix(self, python_name: str, prefix: str) -> List[CFunction]:
        """Find C functions with a specific provider prefix (e.g., anthropic_)."""
        python_name = python_name.lstrip('_')
        matches = []
        prefixed = prefix + python_name
        if prefixed in self.functions:
            matches.extend(self.functions[prefixed])
        return matches

    def find_vtable_defaults_global(self, module_name: str, method_name: str) -> List[CFunction]:
        """Search globally for vtable default implementations."""
        matches = []
        # Patterns: default_<method>, <module>_default_<method>, <module>_default_<Method>
        # Also: <provider>_<method> (e.g., tts_synthesize for TTSProvider)
        # Also handle name truncation: e.g., should_defer_preflight_to_real_usage -> should_defer_preflight
        patterns = [
            f"default_{method_name}",
            f"{module_name}_default_{method_name}",
            f"{module_name}_default_{method_name.capitalize()}",
        ]
        # Provider-prefix pattern (for TTS, image_gen, video_gen, web_search, transcription)
        provider_prefixes = {
            "tts_provider": "tts_",
            "image_gen_provider": "image_gen_",
            "video_gen_provider": "video_gen_",
            "web_search_provider": "web_search_",
            "transcription_provider": "transcribe_",
            "memory_provider": "memory_",
            "context_engine": "context_",
        }
        if module_name in provider_prefixes:
            patterns.append(f"{provider_prefixes[module_name]}{method_name}")

        # Add truncated variant for long method names
        if len(method_name) > 30:
            truncated = method_name[:30].rstrip('_')
            patterns.append(f"default_{truncated}")
            patterns.append(f"{module_name}_default_{truncated}")

        for pattern in patterns:
            if pattern in self.functions:
                matches.extend(self.functions[pattern])
        return matches

class ParityAnalyzer:

    def __init__(self):
        self.extractor = PythonExtractor()
        self.c_index = CIndexer()
        self.c_index.build()
        self.module_map = self._load_module_map()
        self.impl_map = self._build_impl_map()

    def _load_module_map(self) -> Dict[str, str]:
        """Parse docs/module-map.md for Python->C location mappings."""
        mapping = {}
        mm_path = SLERMES_DIR / "docs" / "module-map.md"
        if not mm_path.exists():
            return mapping

        with open(mm_path) as f:
            content = f.read()

        # Parse markdown tables
        for line in content.split('\n'):
            if '|' in line and ('src/' in line or 'lib/' in line):
                parts = [p.strip() for p in line.split('|') if p.strip()]
                if len(parts) >= 3:
                    py_file = parts[0].strip('`')
                    c_loc = parts[1].strip('`')
                    if py_file.endswith('.py') and ('src/' in c_loc or 'lib/' in c_loc):
                        mapping[py_file] = c_loc
        return mapping

    def _build_impl_map(self) -> Dict[str, str]:
        """Build mapping from Python file to actual C implementation file (resolving wrappers)."""
        impl_map = {}
        for py_file, c_loc in self.module_map.items():
            # If it's a wrapper, extract the real implementation
            if "(wrapper)" in c_loc or "Impl in" in c_loc:
                # Parse "Impl in X.c" or "impl in X.c"
                m = re.search(r'(?:Impl in|impl in)\s+(\S+\.c)', c_loc, re.IGNORECASE)
                if m:
                    impl_map[py_file] = m.group(1)
                else:
                    impl_map[py_file] = c_loc
            else:
                impl_map[py_file] = c_loc

        # Also check module-map for wrapper files with "Impl in" in notes column (4th column)
        mm_path = SLERMES_DIR / "docs" / "module-map.md"
        if mm_path.exists():
            with open(mm_path) as f:
                content = f.read()
            for line in content.split('\n'):
                if '|' in line and 'Impl in' in line and 'src/' in line:
                    parts = [p.strip().strip('`') for p in line.split('|') if p.strip()]
                    if len(parts) >= 4:
                        py_file = parts[0]
                        notes = parts[3] if len(parts) > 3 else ""
                        if 'Impl in' in notes:
                            m = re.search(r'(?:Impl in|impl in)\s+(\S+\.c)', notes, re.IGNORECASE)
                            if m and py_file.endswith('.py'):
                                impl_map[py_file] = m.group(1)

        # Also use name-parity wrapper data
        for wrapper, info in self.c_index.name_parity_wrappers.items():
            py_file = info.get("python_file")
            impl_file = info.get("impl_file")
            if py_file and impl_file:
                impl_map[py_file] = impl_file

        # Cross-directory module mappings (Python file -> C implementation)
        cross_dir_mappings = {
            # tools/ -> src/tools/
            "tools/approval.py": "src/tools/approval.c",
            "tools/blueprints.py": "src/tools/blueprints.c",
            "tools/browser_camofox.py": "src/tools/browser.c",
            "tools/browser_cdp_tool.py": "src/tools/browser.c",
            "tools/browser_dialog_tool.py": "src/tools/browser.c",
            "tools/browser_supervisor.py": "src/tools/browser.c",
            "tools/browser_tool.py": "src/tools/browser.c",
            "tools/checkpoint_manager.py": "src/tools/checkpoint_manager.c",
            "tools/code_execution_tool.py": "src/tools/exec_code.c",
            "tools/computer_use/backend.py": "src/tools/computer_use.c",
            "tools/computer_use/cua_backend.py": "src/tools/computer_use.c",
            "tools/computer_use/tool.py": "src/tools/computer_use.c",
            "tools/computer_use/vision_routing.py": "src/tools/computer_use.c",
            "tools/credential_files.py": "src/tools/credential_files.c",
            "tools/cronjob_tools.py": "src/tools/cronjob_tools.c",
            "tools/delegate_tool.py": "src/tools/delegate.c",
            "tools/discord_tool.py": "src/tools/discord.c",
            "tools/env_probe.py": "src/tools/env_probe.c",
            "tools/environments/base.py": "src/tools/environments.c",
            "tools/environments/daytona.py": "src/tools/environments.c",
            "tools/environments/docker.py": "src/tools/environments.c",
            "tools/environments/file_sync.py": "src/tools/environments.c",
            "tools/environments/local.py": "src/tools/environments.c",
            "tools/environments/managed_modal.py": "src/tools/environments.c",
            "tools/environments/modal.py": "src/tools/environments.c",
            "tools/environments/modal_utils.py": "src/tools/environments.c",
            "tools/environments/ssh.py": "src/tools/environments.c",
            "tools/environments/singularity.py": "src/tools/environments.c",
            "tools/fal_common.py": "src/tools/fal_common.c",
            "tools/feishu_comment_rules.py": "src/tools/feishu_comment_rules.c",
            "tools/feishu_doc_tool.py": "src/tools/feishu_doc_tool.c",
            "tools/feishu_drive_tool.py": "src/tools/feishu_drive_tool.c",
            "tools/file_operations.py": "src/tools/file.c",
            "tools/file_tools.py": "src/tools/file.c",
            "tools/fuzzy_match.py": "src/tools/fuzzy_match.c",
            "tools/homeassistant_tool.py": "src/tools/homeassistant.c",
            "tools/image_generation_tool.py": "src/tools/image_gen.c",
            "tools/interrupt.py": "src/tools/interrupt.c",
            "tools/kanban_tools.py": "src/tools/kanban.c",
            "tools/lazy_deps.py": "src/tools/lazy_deps.c",
            "tools/managed_tool_gateway.py": "src/tools/managed_tool_gateway.c",
            "tools/mcp_oauth.py": "src/tools/mcp_oauth.c",
            "tools/mcp_oauth_manager.py": "src/tools/mcp_oauth_manager.c",
            "tools/mcp_tool.py": "src/tools/mcp_tool.c",
            "tools/memory_tool.py": "src/tools/memory.c",
            "tools/mixture_of_agents_tool.py": "src/tools/mixture_of_agents.c",
            "tools/neutts_synth.py": "src/tools/neutts_synth.c",
            "tools/osv_check.py": "src/tools/osv_check.c",
            "tools/patch_parser.py": "src/tools/patch_parser.c",
            "tools/path_security.py": "src/tools/path_security.c",
            "tools/process_registry.py": "src/tools/process_registry.c",
            "tools/read_extract.py": "src/tools/read_extract.c",
            "tools/read_terminal_tool.py": "src/tools/read_terminal.c",
            "tools/registry.py": "src/tools/registry.c",
            "tools/schema_sanitizer.py": "src/tools/schema_sanitizer.c",
            "tools/send_message_tool.py": "src/tools/send_message.c",
            "tools/skill_usage.py": "src/tools/skill_usage.c",
            "tools/skill_usage.py": "lib/libskillusage/skill_usage.c",
            "tools/skill_usage.py": "lib/libskillusage/skill_provenance.c",
            "tools/skill_manager_tool.py": "src/tools/skill_manager.c",
            "tools/skills_ast_audit.py": "src/tools/skills_ast_audit.c",
            "tools/skills_guard.py": "src/tools/skills_guard.c",
            "tools/skills_hub.py": "src/tools/skills_hub.c",
            "tools/skills_sync.py": "src/tools/skills_sync.c",
            "tools/skills_tool.py": "src/tools/skills_tool.c",
            "tools/slash_confirm.py": "src/tools/slash_confirm.c",
            "tools/terminal_tool.py": "src/tools/terminal.c",
            "tools/terminal_tool.py": "src/tools/terminal_tool.c",
            "tools/thread_context.py": "src/tools/thread_context.c",
            "tools/threat_patterns.py": "src/tools/threat_patterns.c",
            "tools/tirith_security.py": "src/tools/tirith_security.c",
            "tools/todo_tool.py": "src/tools/todo.c",
            "tools/tool_backend_helpers.py": "src/tools/tool_backend_helpers.c",
            "tools/tool_output_limits.py": "src/tools/tool_output_limits.c",
            "tools/tool_result_storage.py": "src/tools/result_storage.c",
            "tools/tool_search.py": "src/tools/tool_search.c",
            "tools/transcription_tools.py": "src/tools/transcribe.c",
            "tools/tts_tool.py": "src/tools/tts.c",
            "tools/video_generation_tool.py": "src/tools/video_gen.c",
            "tools/vision_tools.py": "src/tools/vision.c",
            "tools/voice_mode.py": "src/tools/voice.c",
            "tools/web_tools.py": "src/tools/web.c",
            "tools/website_policy.py": "src/tools/website_policy.c",
            "tools/write_approval.py": "src/tools/write_approval.c",
            "tools/x_search_tool.py": "src/tools/x_search.c",
            "tools/xai_http.py": "src/tools/xai_http.c",
            "tools/yuanbao_tools.py": "src/tools/yuanbao_tools.c",
            "tools/image_generation_tool.py": "src/tools/image_generation.c",
            "tools/kanban_tools.py": "src/tools/kanban_tools.c",
            "tools/lazy_deps.py": "src/tools/lazy_deps.c",
            "tools/mcp_oauth.py": "src/tools/mcp_oauth.c",
            "tools/mcp_oauth_manager.py": "src/tools/mcp_oauth_manager.c",
            "tools/mcp_tool.py": "src/tools/mcp_tool.c",
            "tools/memory_tool.py": "src/tools/memory.c",
            "tools/mixture_of_agents_tool.py": "src/tools/mixture_of_agents.c",
            "tools/patch_parser.py": "src/tools/patch.c",
            "tools/process_registry.py": "src/tools/process.c",
            "tools/read_extract.py": "src/tools/read_extract.c",
            "tools/registry.py": "src/tools/registry.c",
            "tools/schema_sanitizer.py": "src/tools/schema_sanitizer.c",
            "tools/send_message_tool.py": "src/tools/send_message.c",
            "tools/session_search_tool.py": "src/tools/session_search.c",
            "tools/skill_manager_tool.py": "src/tools/skills.c",
            "tools/skill_usage.py": "src/tools/skills.c",
            "tools/skills_guard.py": "src/tools/skills_guard.c",
            "tools/skills_hub.py": "src/tools/skills_hub.c",
            "tools/skills_sync.py": "src/tools/skills_sync.c",
            "tools/skills_tool.py": "src/tools/skills_tool.c",
            "tools/terminal_tool.py": "src/tools/terminal.c",
            "tools/tirith_security.py": "src/tools/tirith.c",
            "tools/todo_tool.py": "src/tools/todo_tool.c",
            "tools/transcription_tools.py": "src/tools/transcribe.c",
            "tools/tts_tool.py": "src/tools/tts.c",
            "tools/video_generation_tool.py": "src/tools/video_gen.c",
            "tools/vision_tools.py": "src/tools/vision.c",
            "tools/voice_mode.py": "src/tools/voice_mode.c",
            "tools/web_tools.py": "src/tools/web.c",
            "tools/write_approval.py": "src/tools/write_approval.c",
            "tools/x_search_tool.py": "src/tools/x_search.c",
            "tools/yuanbao_tools.py": "src/tools/yuanbao_media.c",
            
            # tools/ (simple 1:1 mappings)
            "tools/ansi_strip.py": "src/tools/ansi_strip.c",
            "tools/binary_extensions.py": "src/tools/binary_extensions.c",
            "tools/budget_config.py": "src/tools/budget_config.c",
            "tools/clarify_gateway.py": "src/tools/clarify.c",
            "tools/clarify_tool.py": "src/tools/clarify.c",
            "tools/debug_helpers.py": "src/tools/debug_helpers.c",
            "tools/env_passthrough.py": "src/tools/env_passthrough.c",
            "tools/feishu_doc_tool.py": "src/tools/feishu_tools.c",
            "tools/feishu_drive_tool.py": "src/tools/feishu_tools.c",
            "tools/homeassistant_tool.py": "src/tools/homeassistant.c",
            "tools/interrupt.py": "src/tools/interrupt.c",
            "tools/microsoft_graph_auth.py": "src/tools/microsoft_graph_auth.c",
            "tools/microsoft_graph_client.py": "src/tools/microsoft_graph_client.c",
            "tools/neutts_synth.py": "src/tools/neutts_synth.c",
            "tools/openrouter_client.py": "src/tools/openrouter_client.c",
            "tools/osv_check.py": "src/tools/osv_check.c",
            "tools/path_security.py": "src/tools/path_security.c",
            "tools/read_terminal_tool.py": "src/tools/read_terminal.c",
            "tools/skill_provenance.py": "src/tools/skill_provenance.c",
            "tools/skills_ast_audit.py": "src/tools/skills_ast_audit.c",
            "tools/slash_confirm.py": "src/tools/slash_confirm.c",
            "tools/thread_context.py": "src/tools/thread_context.c",
            "tools/threat_patterns.py": "src/tools/threat_patterns.c",
            "tools/tool_backend_helpers.py": "src/tools/tool_backend_helpers.c",
            "tools/tool_output_limits.py": "src/tools/tool_output_limits.c",
            "tools/tool_result_storage.py": "src/tools/result_storage.c",
            "tools/tool_search.py": "src/tools/tool_search.c",
            "tools/website_policy.py": "src/tools/website_policy.c",
            "tools/xai_http.py": "src/tools/xai_http.c",

            # gateway/ -> src/gateway/
            "gateway/config.py": "src/gateway/config.c",
            "gateway/delivery.py": "src/gateway/delivery.c",
            "gateway/hooks.py": "src/gateway/hooks.c",
            "gateway/memory_monitor.py": "src/gateway/memory_monitor.c",
            "gateway/mirror.py": "src/gateway/mirror.c",
            "gateway/pairing.py": "src/gateway/pairing.c",
            "gateway/platform_registry.py": "src/gateway/platform_registry.c",
            "gateway/restart.py": "src/gateway/restart.c",
            "gateway/run.py": "src/gateway/run.c",
            "gateway/session.py": "src/gateway/session.c",
            "gateway/slash_commands.py": "src/gateway/slash_commands.c",
            "gateway/status.py": "src/gateway/status.c",
            "gateway/sticker_cache.py": "src/gateway/sticker_cache.c",
            "gateway/stream_consumer.py": "src/gateway/stream_consumer.c",
            "gateway/stream_dispatch.py": "src/gateway/stream_dispatch.c",
            "gateway/stream_events.py": "src/gateway/stream_events.c",
            "gateway/authz_mixin.py": "src/gateway/authz_mixin.c",
            "gateway/channel_directory.py": "src/gateway/channel_directory.c",
            "gateway/display_config.py": "src/gateway/display_config.c",
            "gateway/kanban_watchers.py": "src/gateway/kanban_watchers.c",
            "gateway/response_filters.py": "src/gateway/response_filters.c",
            "gateway/runtime_footer.py": "src/gateway/runtime_footer.c",
            "gateway/shutdown_forensics.py": "src/gateway/shutdown_forensics.c",
            "gateway/slash_access.py": "src/gateway/slash_access.c",
            "gateway/whatsapp_identity.py": "src/gateway/whatsapp_identity.c",
            
            # gateway/platforms/
            "gateway/platforms/base.py": "src/gateway/platforms/base.c",
            "gateway/platforms/api_server.py": "src/gateway/platforms/api_server_adapter.c",
            "gateway/platforms/bluebubbles.py": "src/gateway/platforms/bluebubbles.c",
            "gateway/platforms/dingtalk.py": "src/gateway/platforms/dingtalk.c",
            "gateway/platforms/email.py": "src/gateway/platforms/email.c",
            "gateway/platforms/feishu.py": "src/gateway/platforms/feishu.c",
            "gateway/platforms/feishu_comment.py": "src/gateway/platforms/feishu_comment.c",
            "gateway/platforms/feishu_comment_rules.py": "src/gateway/platforms/feishu_comment_rules.c",
            "gateway/platforms/feishu_meeting_invite.py": "src/gateway/platforms/feishu_comment.c",
            "gateway/platforms/helpers.py": "src/gateway/platforms/helpers.c",
            "gateway/platforms/matrix.py": "src/gateway/platforms/matrix.c",
            "gateway/platforms/msgraph_webhook.py": "src/gateway/platforms/msgraph_webhook.c",
            "gateway/platforms/slack.py": "src/gateway/platforms/slack.c",
            "gateway/platforms/sms.py": "src/gateway/platforms/sms.c",
            "gateway/platforms/signal.py": "src/gateway/platforms/signal.c",
            "gateway/platforms/signal_rate_limit.py": "src/gateway/platforms/signal_rate_limit.c",
            "gateway/platforms/telegram.py": "src/gateway/platforms/telegram.c",
            "gateway/platforms/telegram_network.py": "src/gateway/platforms/telegram_network.c",
            "gateway/platforms/webhook.py": "src/gateway/platforms/webhook.c",
            "gateway/platforms/wecom.py": "src/gateway/platforms/wecom.c",
            "gateway/platforms/wecom_callback.py": "src/gateway/platforms/wecom_callback.c",
            "gateway/platforms/weixin.py": "src/gateway/platforms/weixin.c",
            "gateway/platforms/whatsapp.py": "src/gateway/platforms/whatsapp.c",
            "gateway/platforms/whatsapp_cloud.py": "src/gateway/platforms/whatsapp.c",
            "gateway/platforms/whatsapp_common.py": "src/gateway/platforms/whatsapp.c",
            "gateway/platforms/yuanbao.py": "src/gateway/platforms/yuanbao.c",
            "gateway/platforms/yuanbao_media.py": "src/gateway/platforms/yuanbao_media.c",
            "gateway/platforms/yuanbao_proto.py": "src/gateway/platforms/yuanbao_proto.c",
            "gateway/platforms/yuanbao_sticker.py": "src/gateway/platforms/yuanbao_sticker.c",
            
            # qqbot subdirectory
            "gateway/platforms/qqbot/adapter.py": "src/gateway/platforms/qqbot.c",
            "gateway/platforms/qqbot/chunked_upload.py": "src/gateway/platforms/qqbot.c",
            "gateway/platforms/qqbot/crypto.py": "src/gateway/platforms/qqbot.c",
            "gateway/platforms/qqbot/keyboards.py": "src/gateway/platforms/qqbot.c",
            "gateway/platforms/qqbot/onboard.py": "src/gateway/platforms/qqbot.c",
            "gateway/platforms/qqbot/utils.py": "src/gateway/platforms/qqbot.c",
            
            # cron/ -> src/tools/ (curator_backup.c) or new cron module
            "cron/blueprint_catalog.py": "src/tools/blueprints.c",
            "cron/jobs.py": "src/tools/cronjob_tools.c",
            "cron/scheduler.py": "src/tools/scheduler.c",
            "cron/suggestion_catalog.py": "src/tools/suggestion_catalog.c",
            "cron/suggestions.py": "src/tools/suggestions.c",
            
            # cli.py (root) -> src/cli/
            "cli.py": "src/cli/main.c",
            
            # hermes_cli/ -> src/cli/
            "hermes_cli/auth.py": "src/cli/auth.c",
            "hermes_cli/auth_commands.py": "src/cli/commands.c",
            "hermes_cli/backup.py": "src/cli/backup.c",
            "hermes_cli/banner.py": "src/cli/banner.c",
            "hermes_cli/blueprint_cmd.py": "src/cli/blueprint_cmd.c",
            "hermes_cli/cli_agent_setup_mixin.py": "src/cli/cli_agent_setup_mixin.c",
            "hermes_cli/cli_commands_mixin.py": "src/cli/commands.c",
            "hermes_cli/cli_output.py": "src/cli/cli_output.c",
            "hermes_cli/clipboard.py": "src/cli/clipboard.c",
            "hermes_cli/commands.py": "src/cli/commands.c",
            "hermes_cli/completion.py": "src/cli/completion.c",
            "hermes_cli/config.py": "src/cli/config.c",
            "hermes_cli/container_boot.py": "src/cli/container_boot.c",
            "hermes_cli/curator.py": "src/cli/curator.c",
            "hermes_cli/curses_ui.py": "src/cli/tui_fullscreen.c",
            "hermes_cli/doctor.py": "src/cli/doctor.c",
            "hermes_cli/dump.py": "src/cli/dump.c",
            "hermes_cli/env_loader.py": "src/cli/env_loader.c",
            "hermes_cli/fallback_cmd.py": "src/cli/fallback_cmd.c",
            "hermes_cli/gateway.py": "src/cli/gateway.c",
            "hermes_cli/gateway_windows.py": "src/cli/gateway_windows.c",
            "hermes_cli/goals.py": "src/cli/goals.c",
            "hermes_cli/gui_uninstall.py": "src/cli/gui_uninstall.c",
            "hermes_cli/kanban.py": "src/cli/kanban.c",
            "hermes_cli/kanban_db.py": "src/cli/kanban_db_engine.c",
            "hermes_cli/kanban_decompose.py": "src/cli/kanban_decompose.c",
            "hermes_cli/kanban_diagnostics.py": "src/cli/kanban_diagnostics.c",
            "hermes_cli/kanban_specify.py": "src/cli/kanban_specify.c",
            "hermes_cli/kanban_swarm.py": "src/cli/kanban_swarm.c",
            "hermes_cli/logs.py": "src/cli/logs.c",
            "hermes_cli/main.py": "src/cli/main.c",
            "hermes_cli/mcp_catalog.py": "src/cli/mcp_catalog.c",
            "hermes_cli/mcp_config.py": "src/cli/mcp_config.c",
            "hermes_cli/mcp_picker.py": "src/cli/mcp_picker.c",
            "hermes_cli/mcp_security.py": "src/cli/mcp_security.c",
            "hermes_cli/mcp_startup.py": "src/cli/mcp_startup.c",
            "hermes_cli/memory_setup.py": "src/cli/memory_setup.c",
            "hermes_cli/middleware.py": "src/cli/middleware.c",
            "hermes_cli/migrate.py": "src/cli/migrate.c",
            "hermes_cli/model_catalog.py": "src/cli/model_catalog.c",
            "hermes_cli/model_cost_guard.py": "src/cli/model_cost_guard.c",
            "hermes_cli/model_normalize.py": "src/cli/model_normalize.c",
            "hermes_cli/model_setup_flows.py": "src/cli/model_setup_flows.c",
            "hermes_cli/model_switch.py": "src/cli/model_switch.c",
            "hermes_cli/models.py": "src/cli/models.c",
            "hermes_cli/nous_account.py": "src/cli/nous_account.c",
            "hermes_cli/nous_subscription.py": "src/cli/nous_subscription.c",
            "hermes_cli/oneshot.py": "src/cli/oneshot.c",
            "hermes_cli/pairing.py": "src/cli/pairing.c",
            "hermes_cli/partial_compress.py": "src/cli/partial_compress.c",
            "hermes_cli/platforms.py": "src/cli/platforms.c",
            "hermes_cli/plugins.py": "src/cli/plugins.c",
            "hermes_cli/plugins_cmd.py": "src/cli/plugins_cmd.c",
            "hermes_cli/portal_cli.py": "src/cli/portal_cli.c",
            "hermes_cli/profile_describer.py": "src/cli/profile_describer.c",
            "hermes_cli/profile_distribution.py": "src/cli/profile_distribution.c",
            "hermes_cli/profiles.py": "src/cli/profiles.c",
            "hermes_cli/prompt_size.py": "src/cli/prompt_size.c",
            "hermes_cli/providers.py": "src/cli/providers.c",
            "hermes_cli/relaunch.py": "src/cli/relaunch.c",
            "hermes_cli/runtime_provider.py": "src/cli/runtime_provider.c",
            "hermes_cli/secret_prompt.py": "src/cli/secret_prompt.c",
            "hermes_cli/secrets_cli.py": "src/cli/secrets_cli.c",
            "hermes_cli/security_advisories.py": "src/cli/security_advisories.c",
            "hermes_cli/security_audit.py": "src/cli/security_audit.c",
            "hermes_cli/send_cmd.py": "src/cli/send_cmd.c",
            "hermes_cli/service_manager.py": "src/cli/service_manager.c",
            "hermes_cli/session_recap.py": "src/cli/session_recap.c",
            "hermes_cli/setup.py": "src/cli/setup.c",
            "hermes_cli/setup_whatsapp_cloud.py": "src/cli/setup_whatsapp_cloud.c",
            "hermes_cli/skills_config.py": "src/cli/skills_config.c",
            "hermes_cli/skills_hub.py": "src/cli/skills_hub.c",
            "hermes_cli/status.py": "src/cli/status.c",
            "hermes_cli/stdio.py": "src/cli/stdio.c",
            "hermes_cli/suggestions_cmd.py": "src/cli/suggestions_cmd.c",
            "hermes_cli/telegram_managed_bot.py": "src/cli/telegram_managed_bot.c",
            "hermes_cli/timeouts.py": "src/cli/timeouts.c",
            "hermes_cli/tips.py": "src/cli/tips.c",
            "hermes_cli/tools_config.py": "src/cli/tools_config.c",
            "hermes_cli/uninstall.py": "src/cli/uninstall.c",
            "hermes_cli/voice.py": "src/cli/voice.c",
            "hermes_cli/web_server.py": "src/cli/web_server.c",
            "hermes_cli/webhook.py": "src/cli/webhook.c",
            "hermes_cli/win_pty_bridge.py": "src/cli/win_pty_bridge.c",
            "hermes_cli/write_approval_commands.py": "src/cli/write_approval_commands.c",
            # agent/pet/ -> src/pet/
            "agent/pet/__init__.py": "src/pet/pet_commands.c",
            "agent/pet/constants.py": "src/pet/pet_constants.c",
            "agent/pet/state.py": "src/pet/pet_state.c",
            "agent/pet/manifest.py": "src/pet/pet_manifest.c",
            "agent/pet/store.py": "src/pet/pet_store.c",
            "agent/pet/render.py": "src/pet/pet_render.c",
        }

        # Add cross-directory mappings
        impl_map.update(cross_dir_mappings)

        # Also auto-discover from PoP annotations in C files
        # The CIndexer already extracts PoP annotations that reference Python module names
        for pop in self.c_index.pop_annotations:
            for py_func in pop.python_functions:
                # Try to infer Python module from the function name context
                # This is a weak signal but can help
                pass

        return impl_map

    def classify_feature(self, py_file: str, feature: PythonFeature) -> GapEntry:
        """Classify a single Python feature."""
        # Check for PoP annotation first (explicit)
        pop_annotations = self.c_index.find_pop_for_python(feature.name, py_file)
        if pop_annotations:
            pop = pop_annotations[0]
            # PoP annotation exists — but verify the C function has a real body.
            # A PoP comment with no function body, or a trivial no-op, is not PORTED.
            if pop.c_file and pop.c_function:
                body_check = self._verify_pop_body(pop.c_file, pop.c_function)
                if body_check == "real":
                    return GapEntry(
                        python_file=py_file,
                        python_feature=feature,
                        classification="PORTED",
                        c_location=pop.c_file,
                        c_function=pop.c_function,
                        pop_annotation=pop,
                        severity="LOW",
                        notes="Explicit PoP annotation with verified body"
                    )
                elif body_check == "noop":
                    return GapEntry(
                        python_file=py_file,
                        python_feature=feature,
                        classification="STUB",
                        c_location=pop.c_file,
                        c_function=pop.c_function,
                        pop_annotation=pop,
                        severity="HIGH",
                        stub_reason="PoP annotation exists but C function is a no-op/trivial",
                        notes="PoP annotation present but body is empty or returns constant"
                    )
                else:  # "missing"
                    return GapEntry(
                        python_file=py_file,
                        python_feature=feature,
                        classification="REAL_GAP",
                        c_location=pop.c_file,
                        c_function=pop.c_function,
                        pop_annotation=pop,
                        severity="HIGH",
                        stub_reason="PoP annotation exists but no C function body found",
                        notes="Annotation-only stub — comment without implementation"
                    )
            # No c_file/c_function info — fall through to other checks


        # Unified C implementation search - works for ALL files
        # (everything is REAL_GAP until actually ported — there is no exempt list, no N/A)
        # 1. Check module map for implementation file
        impl_file = self.impl_map.get(py_file, "")
        if not impl_file:
            # Fallback: try common locations based on Python file name
            base_name = py_file.replace('.py', '.c')
            common_paths = [
                f"src/agent/{base_name}",
                f"src/tools/{base_name}",
                f"src/provider/{base_name}",
                f"src/gateway/{base_name}",
                f"src/cli/{base_name}",
            ]
            for cp in common_paths:
                if (SLERMES_DIR / cp).exists():
                    impl_file = cp
                    break

        # 2. Search in implementation file if found
        if impl_file:
            c_funcs = self._find_in_impl_file(impl_file, feature.name)
            if c_funcs:
                src_func = c_funcs[0]
                is_stub = self._check_if_stub(src_func.file, src_func.name)
                if is_stub:
                    return GapEntry(
                        python_file=py_file,
                        python_feature=feature,
                        classification="REAL_GAP",
                        c_location=src_func.file,
                        c_function=src_func.name,
                        stub_reason="C function appears to be stub/trivial",
                        severity="HIGH"
                    )
                return GapEntry(
                    python_file=py_file,
                    python_feature=feature,
                    classification="PARTIAL",
                    c_location=src_func.file,
                    c_function=src_func.name,
                    severity="MEDIUM",
                    notes="C function exists in impl file but no PoP annotation"
                )

        # 3. SPECIAL HANDLING: Adapter files -> search with provider prefix
        if py_file.endswith("_adapter.py") or py_file.endswith("_runtime.py"):
            provider_prefix = self._get_adapter_provider_prefix(py_file)
            if provider_prefix:
                impl_file = self.impl_map.get(py_file, "")
                c_funcs = self._find_in_impl_file_with_prefix(impl_file, feature.name, provider_prefix) if impl_file else []
                if not c_funcs:
                    c_funcs = self.c_index.find_c_function_with_prefix(feature.name, provider_prefix)
                if c_funcs:
                    src_func = c_funcs[0]
                    return GapEntry(
                        python_file=py_file,
                        python_feature=feature,
                        classification="PARTIAL",
                        c_location=src_func.file,
                        c_function=src_func.name,
                        severity="MEDIUM",
                        notes=f"Found in {impl_file} with {provider_prefix} prefix (needs PoP)"
                    )

        # 4. SPECIAL HANDLING: Class-based vtable modules
        vtable_modules = ["context_engine.py", "memory_provider.py", "browser_provider.py",
                         "image_gen_provider.py", "video_gen_provider.py", "tts_provider.py",
                         "transcription_provider.py", "web_search_provider.py"]
        if py_file in vtable_modules:
            impl_file = self.impl_map.get(py_file, "")
            c_funcs = self._find_vtable_defaults(impl_file, py_file.replace('.py', ''), feature.name) if impl_file else []
            if not c_funcs:
                c_funcs = self.c_index.find_vtable_defaults_global(py_file.replace('.py', ''), feature.name)
            if c_funcs:
                src_func = c_funcs[0]
                return GapEntry(
                    python_file=py_file,
                    python_feature=feature,
                    classification="PARTIAL",
                    c_location=src_func.file,
                    c_function=src_func.name,
                    severity="MEDIUM",
                    notes=f"Found vtable default in {impl_file} (needs PoP)"
                )

        # 5. Global search as fallback
        c_funcs = self.c_index.find_c_function(feature.name, py_file, feature.parent_class)
        if c_funcs:
            src_func = c_funcs[0]
            is_stub = self._check_if_stub(src_func.file, src_func.name)
            if is_stub:
                return GapEntry(
                    python_file=py_file,
                    python_feature=feature,
                    classification="REAL_GAP",
                    c_location=src_func.file,
                    c_function=src_func.name,
                    stub_reason="C function appears to be stub/trivial",
                    severity="HIGH"
                )
            return GapEntry(
                python_file=py_file,
                python_feature=feature,
                classification="PARTIAL",
                c_location=src_func.file,
                c_function=src_func.name,
                severity="MEDIUM",
                notes="C function exists globally but no PoP annotation"
            )

        # 6. Check name-parity wrapper claims
        _, wrapper_claims = self.c_index.find_wrapper_for_module(py_file)
        if feature.name in wrapper_claims:
            return GapEntry(
                python_file=py_file,
                python_feature=feature,
                classification="REAL_GAP",
                severity="HIGH",
                notes="Claimed by name-parity wrapper but no C implementation found"
            )

        # No C equivalent found — REAL_GAP
        return GapEntry(
            python_file=py_file,
            python_feature=feature,
            classification="REAL_GAP",
            severity="HIGH",
            notes="No C equivalent found in any source directory"
        )

    def _find_in_impl_file(self, impl_file: str, func_name: str) -> List[CFunction]:
        """Search for a function specifically in the implementation file (uses cache)."""
        matches = []
        fname = Path(impl_file).name
        # Use filename index to find matching files
        for rel_str in self.c_index._filename_index.get(fname, []):
            _, nc = self.c_index._get_cached_content(rel_str)
            if not nc:
                continue
            pattern = rf'(?:static\s+)?(?:\w+\s+)*\*{0,2}\s*{re.escape(func_name)}\s*\('
            for m in re.finditer(pattern, nc):
                line = nc[:m.start()].count('\n') + 1
                is_static = 'static' in nc[max(0,m.start()-50):m.start()]
                return_match = re.search(r'(?:^|\n)([a-zA-Z_]\w*(?:\s*\*)?)\s+' + re.escape(func_name) + r'\s*\(', nc[:m.start()], re.MULTILINE)
                return_type = return_match.group(1) if return_match else ""
                matches.append(CFunction(
                    name=func_name, file=rel_str, line=line,
                    is_static=is_static, return_type=return_type
                ))
        return matches

    def _remove_c_comments(self, content: str) -> str:
        """Remove C comments (both // and /* */) from content."""
        # Remove /* */ comments
        content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
        # Remove // comments
        content = re.sub(r'//.*$', '', content, flags=re.MULTILINE)
        return content

    def _get_adapter_provider_prefix(self, py_file: str) -> Optional[str]:
        """Map adapter filename to provider prefix."""
        prefix_map = {
            "anthropic_adapter.py": "anthropic_",
            "bedrock_adapter.py": "bedrock_",
            "codex_responses_adapter.py": "codex_",
            "gemini_cloudcode_adapter.py": "google_",
            "gemini_native_adapter.py": "google_",
            "codex_runtime.py": "codex_",
            "model_metadata.py": "models_dev_",
            "models_dev.py": "models_dev_",
            "provider_metadata.py": "provider_",
        }
        return prefix_map.get(py_file)

    def _find_in_impl_file_with_prefix(self, impl_file: str, func_name: str, prefix: str) -> List[CFunction]:
        """Search for prefixed function in implementation file (uses cache)."""
        matches = []
        prefixed = prefix + func_name.lstrip('_')
        fname = Path(impl_file).name
        for rel_str in self.c_index._filename_index.get(fname, []):
            _, nc = self.c_index._get_cached_content(rel_str)
            if not nc:
                continue
            pattern = rf'(?:static\s+)?(?:\w+\s+)*\*{0,2}\s*{re.escape(prefixed)}\s*\('
            for m in re.finditer(pattern, nc):
                line = nc[:m.start()].count('\n') + 1
                is_static = 'static' in nc[max(0,m.start()-50):m.start()]
                return_match = re.search(r'(?:^|\n)([a-zA-Z_]\w*(?:\s*\*)?)\s+' + re.escape(prefixed) + r'\s*\(', nc[:m.start()], re.MULTILINE)
                return_type = return_match.group(1) if return_match else ""
                matches.append(CFunction(
                    name=prefixed, file=rel_str, line=line,
                    is_static=is_static, return_type=return_type
                ))
        return matches

    def _find_vtable_defaults(self, impl_file: str, module_name: str, method_name: str) -> List[CFunction]:
        """Search for vtable default implementations in implementation file (uses cache)."""
        matches = []
        patterns = [
            f"default_{method_name}",
            f"{module_name}_default_{method_name}",
            f"{module_name}_default_{method_name.capitalize()}",
        ]
        # Provider-prefix pattern (for TTS, image_gen, video_gen, web_search, transcription)
        provider_prefixes = {
            "tts_provider": "tts_",
            "image_gen_provider": "image_gen_",
            "video_gen_provider": "video_gen_",
            "web_search_provider": "web_search_",
            "transcription_provider": "transcribe_",
            "memory_provider": "memory_",
            "context_engine": "context_",
        }
        if module_name in provider_prefixes:
            patterns.append(f"{provider_prefixes[module_name]}{method_name}")

        fname = Path(impl_file).name
        for rel_str in self.c_index._filename_index.get(fname, []):
            _, nc = self.c_index._get_cached_content(rel_str)
            if not nc:
                continue
            for pattern in patterns:
                for m in re.finditer(rf'(?:static\s+)?(?:\w+\s+)*\*{0,2}\s*{re.escape(pattern)}\s*\(', nc):
                    line = nc[:m.start()].count('\n') + 1
                    is_static = 'static' in nc[max(0,m.start()-50):m.start()]
                    return_match = re.search(r'(?:^|\n)([a-zA-Z_]\w*(?:\s*\*)?)\s+' + re.escape(pattern) + r'\s*\(', nc[:m.start()], re.MULTILINE)
                    return_type = return_match.group(1) if return_match else ""
                    matches.append(CFunction(
                        name=pattern, file=rel_str, line=line,
                        is_static=is_static, return_type=return_type
                    ))
        return matches

    def _check_if_stub(self, c_file: str, func_name: str) -> bool:
        """Check if a C function is a stub (trivial implementation)."""
        fpath = SLERMES_DIR / c_file
        if not fpath.exists():
            return False

        with open(fpath) as f:
            file_content = f.read()

        # Find function body
        escaped_name = re.escape(func_name)
        func_pattern = r'(?:static\s+)?(?:\w+\s+)*\*?\s*' + escaped_name + r'\s*\([^)]*\)\s*\{'
        match = re.search(func_pattern, file_content)
        if not match:
            return False

        # Extract function body (up to closing brace)
        body_start = match.end()
        brace_count = 1
        pos = body_start
        in_string = False
        in_char = False
        in_line_comment = False
        in_block_comment = False
        while pos < len(file_content) and brace_count > 0:
            ch = file_content[pos]
            prev = file_content[pos - 1] if pos > 0 else '\0'

            if in_line_comment:
                if ch == '\n':
                    in_line_comment = False
                pos += 1
                continue
            elif in_block_comment:
                if ch == '/' and prev == '*':
                    in_block_comment = False
                pos += 1
                continue
            elif in_string:
                if ch == '"' and prev != '\\':
                    in_string = False
                pos += 1
                continue
            elif in_char:
                if ch == "'" and prev != '\\':
                    in_char = False
                pos += 1
                continue

            # Not inside string/comment
            if ch == '/' and pos + 1 < len(file_content):
                if file_content[pos + 1] == '/':
                    in_line_comment = True
                    pos += 2
                    continue
                elif file_content[pos + 1] == '*':
                    in_block_comment = True
                    pos += 2
                    continue
            elif ch == '"':
                in_string = True
            elif ch == "'":
                in_char = True
            elif ch == '{':
                brace_count += 1
            elif ch == '}':
                brace_count -= 1
            pos += 1

        body = file_content[body_start:pos].strip()

        if not body or body == ';':
            return True

        # (void)param; return NULL/0/false patterns
        if re.search(r'\(void\).*return\s+(NULL|0|false|NULL)', body):
            return True

        # Single line with just return
        lines = [l.strip() for l in body.split('\n') if l.strip() and not l.strip().startswith('//')]
        if len(lines) <= 2 and any('return' in l for l in lines):
            return True

        # Stub pattern: log call + return NULL/0/false
        # Only flag if the body is small (below) — real implementations
        # can have hermes_log + return NULL in error paths
        if len(lines) <= 5:
            if re.search(r'(?:hermes_log|LOG_\w+).*return\s+(NULL|0|false)\s*;', body, re.DOTALL):
                return True

        # Very short function body (≤3 non-empty lines)
        non_empty_lines = [l for l in body.split('\n') if l.strip()]
        if len(non_empty_lines) <= 3 and any('return' in l for l in non_empty_lines):
            return True

        return False

    def _verify_pop_body(self, c_file: str, func_name: str) -> str:
        """Verify a PoP-annotated C function has a real body.

        Returns:
            "real" — function exists with non-trivial body
            "noop" — function exists but is empty/returns constant
            "missing" — no function body found (annotation-only stub)
        """
        fpath = SLERMES_DIR / c_file
        if not fpath.exists():
            return "missing"

        with open(fpath) as f:
            file_content = f.read()

        # Find the function body using same logic as _check_if_stub
        escaped_name = re.escape(func_name)
        func_pattern = r'(?:static\s+)?(?:\w+\s+)*\*?\s*' + escaped_name + r'\s*\([^)]*\)\s*\{'
        match = re.search(func_pattern, file_content)
        if not match:
            return "missing"

        # Extract function body (up to closing brace)
        body_start = match.end()
        brace_count = 1
        pos = body_start
        in_string = False
        in_char = False
        in_line_comment = False
        in_block_comment = False
        while pos < len(file_content) and brace_count > 0:
            ch = file_content[pos]
            prev = file_content[pos - 1] if pos > 0 else '\0'

            if in_line_comment:
                if ch == '\n':
                    in_line_comment = False
                pos += 1
                continue
            elif in_block_comment:
                if ch == '/' and prev == '*':
                    in_block_comment = False
                pos += 1
                continue
            elif in_string:
                if ch == '"' and prev != '\\':
                    in_string = False
                pos += 1
                continue
            elif in_char:
                if ch == "'" and prev != '\\':
                    in_char = False
                pos += 1
                continue

            if ch == '/' and pos + 1 < len(file_content):
                if file_content[pos + 1] == '/':
                    in_line_comment = True
                    pos += 2
                    continue
                elif file_content[pos + 1] == '*':
                    in_block_comment = True
                    pos += 2
                    continue
            elif ch == '"':
                in_string = True
            elif ch == "'":
                in_char = True
            elif ch == '{':
                brace_count += 1
            elif ch == '}':
                brace_count -= 1
            pos += 1

        body = file_content[body_start:pos].strip()

        # Empty body = missing
        if not body or body == ';':
            return "missing"

        # Count non-trivial lines (non-empty, non-comment)
        lines = [l.strip() for l in body.split('\n')
                 if l.strip() and not l.strip().startswith('//')]

        # Single return constant = noop
        if len(lines) <= 2 and any(re.match(r'return\s+(NULL|0|false|true|\d+|""|0\.0)\s*;', l) for l in lines):
            return "noop"

        # Only (void)param; lines = noop (unused param suppression)
        if all(re.match(r'\(void\)\w+\s*;$', l) for l in lines):
            return "noop"

        # Empty body with just braces = noop
        if len(lines) == 0:
            return "noop"

        # Body with just a comment = noop
        if all(l.startswith('/*') or l.startswith('*') or l.startswith('//') for l in lines):
            return "noop"

        return "real"


    def scan_all(self) -> Dict[str, ModuleReport]:
        """Scan all Python files in all configured directories."""
        reports = {}

        # Collect all Python files from all directories
        all_py_files = []

        # agent/ directory
        agent_dir = PYTHON_SOURCE_DIRS["agent"]
        for py_file in sorted(agent_dir.glob("*.py")):
            if py_file.name != "__init__.py":
                all_py_files.append((py_file, "agent/" + py_file.name))

        # agent/pet/ directory
        agent_pet_dir = PYTHON_SOURCE_DIRS["agent"] / "pet"
        if agent_pet_dir.exists():
            for py_file in sorted(agent_pet_dir.rglob("*.py")):
                if py_file.name != "__init__.py":
                    rel = py_file.relative_to(PYTHON_SOURCE_DIRS["agent"])
                    all_py_files.append((py_file, "agent/" + str(rel)))

        # tools/ directory
        tools_dir = PYTHON_SOURCE_DIRS["tools"]
        for py_file in sorted(tools_dir.rglob("*.py")):
            if py_file.name != "__init__.py":
                rel = py_file.relative_to(tools_dir)
                all_py_files.append((py_file, "tools/" + str(rel)))

        # gateway/ directory
        gateway_dir = PYTHON_SOURCE_DIRS["gateway"]
        for py_file in sorted(gateway_dir.rglob("*.py")):
            if py_file.name != "__init__.py":
                rel = py_file.relative_to(gateway_dir)
                all_py_files.append((py_file, "gateway/" + str(rel)))

        # cron/ directory
        cron_dir = PYTHON_SOURCE_DIRS["cron"]
        for py_file in sorted(cron_dir.rglob("*.py")):
            if py_file.name != "__init__.py":
                rel = py_file.relative_to(cron_dir)
                all_py_files.append((py_file, "cron/" + str(rel)))

        # cli.py (root)
        cli_py = PYTHON_SOURCE_DIRS["cli_root"]
        if cli_py.exists():
            all_py_files.append((cli_py, "cli.py"))

        # hermes_cli/ directory
        hermes_cli_dir = PYTHON_SOURCE_DIRS["hermes_cli"]
        for py_file in sorted(hermes_cli_dir.rglob("*.py")):
            if py_file.name != "__init__.py":
                rel = py_file.relative_to(hermes_cli_dir)
                all_py_files.append((py_file, "hermes_cli/" + str(rel)))

        # Sort by display name
        all_py_files.sort(key=lambda x: x[1])

        for py_file, display_name in all_py_files:
            features = self.extractor.extract_file(py_file)
            report = ModuleReport(python_file=display_name)

            # Special handling for class-based modules: if 0 top-level functions but has class methods
            # The methodology says: count class methods for class-based modules
            # Many files like context_engine.py, memory_provider.py are class-based

            for feature in features:
                # Skip excluded Python infrastructure functions
                if feature.name in EXCLUDED_PYTHON_NAMES:
                    continue
                # Check TUI bridge: Python prompt_toolkit functions with C TUI equivalents
                if feature.name in TUI_BRIDGE:
                    c_file, c_func, desc = TUI_BRIDGE[feature.name]
                    gap = GapEntry(
                        python_file=display_name,
                        python_feature=feature,
                        classification="PORTED",
                        c_location=c_file,
                        c_function=c_func,
                        severity="LOW",
                        notes=f"TUI bridge: prompt_toolkit -> C ncurses ({desc})",
                    )
                    report.gaps.append(gap)
                    report.total_features += 1
                    report.ported += 1
                    continue
                gap = self.classify_feature(display_name, feature)
                report.gaps.append(gap)
                report.total_features += 1

                if gap.classification == "PORTED":
                    report.ported += 1
                elif gap.classification == "PARTIAL":
                    report.partial += 1
                elif gap.classification == "STUB":
                    report.stub += 1
                elif gap.classification == "REAL_GAP":
                    report.real_gaps += 1

            reports[display_name] = report

        return reports

class BattlegroundFormatter:
    """Format output for different use cases."""

    @staticmethod
    def summary(reports: Dict[str, ModuleReport]) -> str:
        total = sum(r.total_features for r in reports.values())
        ported = sum(r.ported for r in reports.values())
        partial = sum(r.partial for r in reports.values())
        stub = sum(r.stub for r in reports.values())
        real = sum(r.real_gaps for r in reports.values())

        lines = [
            "=" * 72,
            "  SLERMES PoP PARITY BATTLEGROUND — SUMMARY",
            "=" * 72,
            f"  Python modules scanned: {len(reports)}",
            f"  Total features:         {total}",
            f"  ✅ PORTED:              {ported} ({100*ported/max(total,1):.1f}%)",
            f"  ⚠️  PARTIAL:            {partial} ({100*partial/max(total,1):.1f}%)",
            f"  🔴 STUB:                {stub} ({100*stub/max(total,1):.1f}%)",
            f"  🔴 REAL_GAP:            {real} ({100*real/max(total,1):.1f}%)",
            "=" * 72,
        ]

        # Per-module summary (only show issues)
        for name, report in sorted(reports.items()):
            if report.real_gaps > 0 or report.stub > 0 or report.partial > 0:
                pct = 100 * report.ported / max(report.total_features, 1)
                lines.append(f"  {name:55s} {report.ported:3d}/{report.total_features:3d} ({pct:5.1f}%)  gaps={report.real_gaps} stubs={report.stub} partial={report.partial}")

        return '\n'.join(lines)

    @staticmethod
    def detail(reports: Dict[str, ModuleReport]) -> str:
        lines = [BattlegroundFormatter.summary(reports), ""]

        for name, report in sorted(reports.items()):
            gaps = [g for g in report.gaps if g.classification in ("REAL_GAP", "STUB", "PARTIAL")]
            if not gaps:
                continue

            lines.append(f"\n## {name} ({len(gaps)} issues)")
            for g in gaps:
                loc = f" @ {g.c_location}:{g.c_function}" if g.c_location else ""
                lines.append(f"  [{g.classification}] {g.python_feature.name}{loc}")
                if g.notes:
                    lines.append(f"       → {g.notes}")
                if g.pop_annotation and g.pop_annotation.is_consolidated:
                    lines.append(f"       → Consolidated PoP: {', '.join(g.pop_annotation.python_functions)}")

        return '\n'.join(lines)

    @staticmethod
    def battleship(reports: Dict[str, ModuleReport]) -> str:
        """Output in battleship.md format with CRITICAL/MEDIUM/LOW classification."""
        lines = [
            "# Battleship — PoP Parity Gaps (Auto-Generated)",
            "",
            f"## Summary",
            f"Generated by slermes_parity_battleground.py",
            f"Total Python modules: {len(reports)}",
            f"Total features: {sum(r.total_features for r in reports.values())}",
            f"PORTED: {sum(r.ported for r in reports.values())}",
            f"PARTIAL: {sum(r.partial for r in reports.values())}",
            f"STUB: {sum(r.stub for r in reports.values())}",
            f"REAL_GAP: {sum(r.real_gaps for r in reports.values())}",
            "",
        ]

        # Collect all real gaps and stubs
        critical_gaps = []
        medium_gaps = []
        low_gaps = []

        # Modules where REAL_GAP is not critical (CLI, setup, cron, etc.)
        non_critical_modules = {
            "cli.py",
            # hermes_cli/* - CLI commands and setup
            "cron/",
        }

        for name, report in sorted(reports.items()):
            # Check if this module is non-critical (CLI/setup/cron)
            is_non_critical = name in non_critical_modules or any(name.startswith(prefix) for prefix in ("hermes_cli/", "cron/"))

            for g in report.gaps:
                if g.classification == "REAL_GAP":
                    # Downgrade CLI/setup/cron gaps from CRITICAL to MEDIUM
                    if is_non_critical or g.python_feature.kind == "method":
                        medium_gaps.append((name, g))
                    else:
                        critical_gaps.append((name, g))
                elif g.classification == "STUB":
                    medium_gaps.append((name, g))
                elif g.classification == "PARTIAL":
                    low_gaps.append((name, g))

        if critical_gaps:
            lines.append("## 🔴 CRITICAL Gaps")
            for mod, g in critical_gaps:
                lines.append(f"### {g.python_feature.name} ({mod})")
                lines.append(f"**Python:** `{mod}:{g.python_feature.name}`")
                lines.append(f"**Problem:** {g.notes or 'No C implementation found'}")
                if g.c_location:
                    lines.append(f"**C location:** `{g.c_location}` (stub/un-annotated)")
                lines.append(f"**Fix:** Implement full C equivalent with PoP annotation")
                lines.append("")

        if medium_gaps:
            lines.append("## 🟡 MEDIUM Gaps")
            for mod, g in medium_gaps:
                lines.append(f"### {g.python_feature.name} ({mod})")
                lines.append(f"**Python:** `{mod}:{g.python_feature.name}`")
                lines.append(f"**Problem:** {g.notes or 'C function exists but incomplete'}")
                if g.c_location:
                    lines.append(f"**C location:** `{g.c_location}:{g.c_function}`")
                lines.append(f"**Fix:** Complete implementation and add PoP annotation")
                lines.append("")

        if low_gaps:
            lines.append("## 🟢 LOW Gaps")
            for mod, g in low_gaps:
                lines.append(f"### {g.python_feature.name} ({mod}) [{g.classification}]")
                if g.c_location:
                    lines.append(f"  C: `{g.c_location}:{g.c_function}` — {g.notes}")

        return '\n'.join(lines)

    @staticmethod
    def json_output(reports: Dict[str, ModuleReport]) -> str:
        """Machine-readable JSON output."""
        def gap_to_dict(g):
            d = asdict(g)
            if g.pop_annotation:
                d['pop_annotation'] = asdict(g.pop_annotation)
            d['python_feature'] = asdict(g.python_feature)
            return d

        return json.dumps({
            "modules": {
                name: {
                    "total": r.total_features,
                    "ported": r.ported,
                    "partial": r.partial,
                    "stub": r.stub,
                    "real_gaps": r.real_gaps,
                    "gaps": [gap_to_dict(g) for g in r.gaps]
                }
                for name, r in reports.items()
            }
        }, indent=2)

def main():
    import argparse
    parser = argparse.ArgumentParser(description="Slermes PoP Parity Gap Battleground")
    parser.add_argument("--detail", action="store_true", help="Show detailed per-feature gaps")
    parser.add_argument("--battleship", action="store_true", help="Output battleship format")
    parser.add_argument("--json", action="store_true", help="Output JSON")
    parser.add_argument("--update-cache", action="store_true", help="Update baseline cache")
    parser.add_argument("--module", help="Scan only specific module (substring match)")
    args = parser.parse_args()

    analyzer = ParityAnalyzer()
    reports = analyzer.scan_all()

    if args.module:
        reports = {k: v for k, v in reports.items() if args.module in k}

    if args.json:
        print(BattlegroundFormatter.json_output(reports))
    elif args.battleship:
        print(BattlegroundFormatter.battleship(reports))
    elif args.detail:
        print(BattlegroundFormatter.detail(reports))
    else:
        print(BattlegroundFormatter.summary(reports))

    # Cache for incremental tracking
    if args.update_cache:
        cache_data = {
            "timestamp": __import__('datetime').datetime.now().isoformat(),
            "summary": {
                "total": sum(r.total_features for r in reports.values()),
                "ported": sum(r.ported for r in reports.values()),
                "real_gaps": sum(r.real_gaps for r in reports.values()),
            }
        }
        CACHE_FILE.parent.mkdir(exist_ok=True)
        with open(CACHE_FILE, 'w') as f:
            json.dump(cache_data, f, indent=2)
        print(f"\nCache updated: {CACHE_FILE}")

if __name__ == "__main__":
    main()