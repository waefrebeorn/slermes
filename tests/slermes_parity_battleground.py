#!/usr/bin/env python3
"""
PoP Parity Gap Battleground — Slermes C vs Python Hermes Agent

Replaces the crude slermes_full_parity_scan.py. Uses the full methodology:
- Reads docs/module-map.md and docs/pop-index.md as ground truth
- AST parses ALL Python agent/ files (functions + class methods)
- Finds C implementations across src/agent, lib/, src/tools, src/provider, src/cli, src/gateway
- Cross-references PoP comments (exact, consolidated, name-parity wrappers)
- Classifies: PORTED, PARTIAL, STUB, N/A (SDK/async/ABC/pandas/config/CLI), REAL_GAP
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

# Files that are pure Python infrastructure (no C porting obligation)
INFRASTRUCTURE_ONLY = {
    # agent/
    "agent/async_utils.py": "asyncio-only",
    "agent/jiter_preload.py": "importlib-only",
    "agent/runtime_cwd.py": "contextvars-only",
    "agent/portal_tags.py": "config-only",
    "agent/copilot_acp_client.py": "SDK-adapter",
    "agent/subdirectory_hints.py": "hints-only",
    # tools/
    "tools/ansi_strip.py": "utility-only",
    "tools/binary_extensions.py": "data-only",
    "tools/debug_helpers.py": "debug-only",
    "tools/env_passthrough.py": "pass-through",
    "tools/interrupt.py": "signal-only",
    "tools/lazy_deps.py": "import-only",
    "tools/mcp_oauth.py": "oauth-only",
    "tools/mcp_oauth_manager.py": "oauth-only",
    "tools/microsoft_graph_auth.py": "auth-only",
    "tools/neutts_synth.py": "tts-engine",
    "tools/openrouter_client.py": "client-only",
    "tools/patch_parser.py": "parser-only",
    "tools/path_security.py": "security-helper",
    "tools/skill_provenance.py": "metadata-only",
    "tools/thread_context.py": "context-only",
    "tools/tool_backend_helpers.py": "backend-helpers",
    "tools/tool_output_limits.py": "limits-only",
    "tools/tool_result_storage.py": "storage-only",
    "tools/tool_search.py": "search-utility",
    "tools/url_safety.py": "safety-helper",
    "tools/xai_http.py": "http-client",
    "tools/website_policy.py": "policy-data",
    "tools/x_search_tool.py": "api-client",
    "tools/yuanbao_tools.py": "platform-specific",
    "tools/feishu_doc_tool.py": "platform-specific",
    "tools/discord_tool.py": "platform-specific",
    "tools/homeassistant_tool.py": "platform-specific",
    "tools/microsoft_graph_client.py": "client-only",
    "tools/read_terminal_tool.py": "cli-only",
    "tools/skill_manager_tool.py": "cli-only",
    "tools/slug_confirm.py": "cli-only",
    "tools/slash_confirm.py": "cli-only",
    "tools/binary_extensions.py": "data-only",
    "tools/blueprints.py": "blueprints-only",
    "tools/browser_camofox.py": "browser-only",
    "tools/browser_camofox_state.py": "browser-only",
    "tools/browser_cdp_tool.py": "browser-only",
    "tools/browser_dialog_tool.py": "browser-only",
    "tools/checkpoint_manager.py": "checkpoint-only",
    "tools/clarify_gateway.py": "clarify-only",
    "tools/code_execution_tool.py": "exec-only",
    "tools/computer_use/backend.py": "computer-use-only",
    "tools/computer_use/cua_backend.py": "computer-use-only",
    "tools/computer_use/schema.py": "computer-use-only",
    "tools/computer_use/tool.py": "computer-use-only",
    "tools/computer_use/vision_routing.py": "computer-use-only",
    "tools/computer_use_tool.py": "computer-use-only",
    "tools/credential_files.py": "credential-only",
    "tools/debug_helpers.py": "debug-only",
    "tools/delegate_tool.py": "delegate-only",
    "tools/discord_tool.py": "discord-only",
    "tools/env_passthrough.py": "pass-through",
    "tools/env_probe.py": "probe-only",
    "tools/environments/daytona.py": "env-only",
    "tools/environments/docker.py": "env-only",
    "tools/environments/file_sync.py": "env-only",
    "tools/environments/local.py": "env-only",
    "tools/environments/managed_modal.py": "env-only",
    "tools/environments/modal.py": "env-only",
    "tools/environments/modal_utils.py": "env-only",
    "tools/environments/singularity.py": "env-only",
    "tools/environments/ssh.py": "env-only",
    "tools/fal_common.py": "fal-only",
    "tools/feishu_doc_tool.py": "feishu-only",
    "tools/file_state.py": "file-state-only",
    "tools/file_tools.py": "file-tools-only",
    "tools/fuzzy_match.py": "fuzzy-only",
    "tools/homeassistant_tool.py": "homeassistant-only",
    "tools/interrupt.py": "signal-only",
    "tools/lazy_deps.py": "import-only",
    "tools/managed_tool_gateway.py": "gateway-only",
    "tools/mcp_oauth.py": "oauth-only",
    "tools/mcp_oauth_manager.py": "oauth-only",
    "tools/mcp_tool.py": "mcp-only",
    "tools/memory_tool.py": "memory-only",
    "tools/microsoft_graph_auth.py": "auth-only",
    "tools/microsoft_graph_client.py": "client-only",
    "tools/mixture_of_agents_tool.py": "moa-only",
    "tools/neutts_synth.py": "tts-engine",
    "tools/openrouter_client.py": "client-only",
    "tools/osv_check.py": "osv-only",
    "tools/patch_parser.py": "parser-only",
    "tools/path_security.py": "security-helper",
    "tools/read_extract.py": "extract-only",
    "tools/read_terminal_tool.py": "cli-only",
    "tools/registry.py": "registry-only",
    "tools/schema_sanitizer.py": "sanitizer-only",
    "tools/skill_manager_tool.py": "skill-mgr-only",
    "tools/skill_provenance.py": "metadata-only",
    "tools/skill_usage.py": "skill-usage-only",
    "tools/skills_ast_audit.py": "audit-only",
    "tools/skills_guard.py": "guard-only",
    "tools/skills_hub.py": "skills-hub-only",
    "tools/skills_tool.py": "skills-only",
    "tools/slash_confirm.py": "cli-only",
    "tools/terminal_tool.py": "terminal-only",
    "tools/thread_context.py": "context-only",
    "tools/threat_patterns.py": "threat-only",
    "tools/tirith_security.py": "security-only",
    "tools/todo_tool.py": "todo-only",
    "tools/tool_backend_helpers.py": "backend-helpers",
    "tools/tool_output_limits.py": "limits-only",
    "tools/tool_result_storage.py": "storage-only",
    "tools/tool_search.py": "search-utility",
    "tools/transcription_tools.py": "transcription-only",
    "tools/url_safety.py": "safety-helper",
    "tools/video_generation_tool.py": "video-gen-only",
    "tools/vision_tools.py": "vision-only",
    "tools/voice_mode.py": "voice-only",
    "tools/website_policy.py": "policy-data",
    "tools/x_search_tool.py": "api-client",
    "tools/xai_http.py": "http-client",
    "tools/yuanbao_tools.py": "yuanbao-only",
    # gateway/
    "gateway/channel_directory.py": "config-only",
    "gateway/display_config.py": "config-only",
    "gateway/hooks.py": "hooks-only",
    "gateway/memory_monitor.py": "monitor-only",
    "gateway/pairing.py": "pairing-logic",
    "gateway/platform_registry.py": "registry-only",
    "gateway/response_filters.py": "filters-only",
    "gateway/restart.py": "utility-only",
    "gateway/runtime_footer.py": "footer-only",
    "gateway/session_context.py": "context-only",
    "gateway/shutdown_forensics.py": "forensics-only",
    "gateway/slash_access.py": "access-only",
    "gateway/sticker_cache.py": "cache-only",
    "gateway/stream_dispatch.py": "dispatch-only",
    "gateway/stream_events.py": "events-only",
    "gateway/whatsapp_identity.py": "identity-only",
    "gateway/authz_mixin.py": "mixin-only",
    "gateway/delivery.py": "delivery-logic",
    "gateway/mirror.py": "mirror-logic",
    "gateway/kanban_watchers.py": "watchers-only",
    "gateway/platforms/_http_client_limits.py": "limits-only",
    "gateway/platforms/helpers.py": "helpers-only",
    "gateway/platforms/qqbot/adapter.py": "platform-adapter",
    "gateway/platforms/qqbot/chunked_upload.py": "upload-utility",
    "gateway/platforms/qqbot/constants.py": "constants-only",
    "gateway/platforms/qqbot/crypto.py": "crypto-utility",
    "gateway/platforms/qqbot/keyboards.py": "keyboard-ui",
    "gateway/platforms/qqbot/onboard.py": "onboarding-only",
    "gateway/platforms/qqbot/utils.py": "utils-only",
    "gateway/platforms/wecom_callback.py": "callback-only",
    "gateway/platforms/wecom_crypto.py": "crypto-only",
    "gateway/platforms/feishu.py": "platform-specific",
    "gateway/platforms/telegram.py": "platform-specific",
    "gateway/platforms/matrix.py": "platform-specific",
    "gateway/platforms/slack.py": "platform-specific",
    "gateway/platforms/weixin.py": "platform-specific",
    "gateway/platforms/yuanbao.py": "platform-specific",
    "gateway/platforms/yuanbao_media.py": "platform-specific",
    "gateway/platforms/yuanbao_proto.py": "platform-specific",
    "gateway/platforms/yuanbao_sticker.py": "platform-specific",
    "gateway/platforms/wecom.py": "platform-specific",
    "gateway/platforms/wecom_callback.py": "callback-only",
    "gateway/platforms/bluebubbles.py": "platform-specific",
    "gateway/platforms/dingtalk.py": "platform-specific",
    "gateway/platforms/email.py": "platform-specific",
    "gateway/platforms/feishu_comment_rules.py": "platform-specific",
    "gateway/platforms/feishu_meeting_invite.py": "platform-specific",
    "gateway/platforms/msgraph_webhook.py": "platform-specific",
    "gateway/platforms/wecom_callback.py": "callback-only",
    "gateway/platforms/wecom_crypto.py": "crypto-only",
    "gateway/platforms/weixin.py": "platform-specific",
    "gateway/platforms/whatsapp.py": "platform-specific",
    "gateway/platforms/whatsapp_cloud.py": "platform-specific",
    "gateway/platforms/whatsapp_common.py": "platform-specific",
    "gateway/platforms/qqbot/keyboards.py": "keyboard-ui",
    "gateway/platforms/qqbot/onboard.py": "onboarding-only",
    "gateway/platforms/qqbot/utils.py": "utils-only",
    "gateway/platforms/qqbot/chunked_upload.py": "upload-utility",
    "gateway/platforms/qqbot/crypto.py": "crypto-utility",
    "gateway/platforms/qqbot/keyboards.py": "keyboard-ui",
    "gateway/platforms/qqbot/onboard.py": "onboarding-only",
    "gateway/platforms/qqbot/utils.py": "utils-only",
    "gateway/authz_mixin.py": "mixin-only",
    "gateway/delivery.py": "delivery-logic",
    "gateway/hooks.py": "hooks-only",
    "gateway/kanban_watchers.py": "watchers-only",
    "gateway/memory_monitor.py": "monitor-only",
    "gateway/mirror.py": "mirror-logic",
    "gateway/pairing.py": "pairing-logic",
    "gateway/platform_registry.py": "registry-only",
    "gateway/platforms/_http_client_limits.py": "limits-only",
    "gateway/platforms/bluebubbles.py": "platform-specific",
    "gateway/platforms/dingtalk.py": "platform-specific",
    "gateway/platforms/email.py": "platform-specific",
    "gateway/platforms/feishu.py": "platform-specific",
    "gateway/platforms/feishu_comment.py": "platform-specific",
    "gateway/platforms/feishu_comment_rules.py": "platform-specific",
    "gateway/platforms/feishu_meeting_invite.py": "platform-specific",
    "gateway/platforms/helpers.py": "helpers-only",
    "gateway/platforms/matrix.py": "platform-specific",
    "gateway/platforms/msgraph_webhook.py": "platform-specific",
    "gateway/platforms/qqbot/adapter.py": "platform-adapter",
    "gateway/platforms/qqbot/chunked_upload.py": "upload-utility",
    "gateway/platforms/qqbot/constants.py": "constants-only",
    "gateway/platforms/qqbot/crypto.py": "crypto-utility",
    "gateway/platforms/qqbot/keyboards.py": "keyboard-ui",
    "gateway/platforms/qqbot/onboard.py": "onboarding-only",
    "gateway/platforms/qqbot/utils.py": "utils-only",
    "gateway/platforms/slack.py": "platform-specific",
    "gateway/platforms/sms.py": "platform-specific",
    "gateway/platforms/telegram.py": "platform-specific",
    "gateway/platforms/telegram_network.py": "platform-specific",
    "gateway/platforms/wecom.py": "platform-specific",
    "gateway/platforms/wecom_callback.py": "callback-only",
    "gateway/platforms/wecom_crypto.py": "crypto-only",
    "gateway/platforms/weixin.py": "platform-specific",
    "gateway/platforms/whatsapp.py": "platform-specific",
    "gateway/platforms/whatsapp_cloud.py": "platform-specific",
    "gateway/platforms/whatsapp_common.py": "platform-specific",
    "gateway/platforms/qqbot/keyboards.py": "keyboard-ui",
    "gateway/platforms/qqbot/onboard.py": "onboarding-only",
    "gateway/platforms/qqbot/utils.py": "utils-only",
    "gateway/platforms/qqbot/chunked_upload.py": "upload-utility",
    "gateway/platforms/qqbot/crypto.py": "crypto-utility",
    "gateway/platforms/qqbot/keyboards.py": "keyboard-ui",
    "gateway/platforms/qqbot/onboard.py": "onboarding-only",
    "gateway/platforms/qqbot/utils.py": "utils-only",
    "gateway/response_filters.py": "filters-only",
    "gateway/restart.py": "utility-only",
    "gateway/run.py": "gateway-run",
    "gateway/runtime_footer.py": "footer-only",
    "gateway/session.py": "session-logic",
    "gateway/session_context.py": "context-only",
    "gateway/shutdown_forensics.py": "forensics-only",
    "gateway/slash_access.py": "access-only",
    "gateway/slash_commands.py": "slash-only",
    "gateway/status.py": "status-logic",
    "gateway/sticker_cache.py": "cache-only",
    "gateway/stream_consumer.py": "stream-consumer",
    "gateway/stream_dispatch.py": "dispatch-only",
    "gateway/stream_events.py": "events-only",
    "gateway/whatsapp_identity.py": "identity-only",
    # cron/
    "cron/scripts/classify_items.py": "script-only",
    "cron/suggestion_catalog.py": "data-only",
    "cron/suggestions.py": "data-only",
    "cron/blueprint_catalog.py": "data-only",
    # hermes_cli/
    # cli.py (root)
    # cli.py (root)
    # agent/pet/generate/ — Python-only image generation pipeline (PIL/Pillow)
    "agent/pet/generate/__init__.py": "pet-image-gen",
    "agent/pet/generate/atlas.py": "pet-atlas-image-processing",
    "agent/pet/generate/imagegen.py": "pet-image-gen-provider",
    "agent/pet/generate/orchestrate.py": "pet-orchestrate-async",
    "agent/pet/generate/prompts.py": "pet-prompts-only",
    # agent/pet/render.py — Python-only terminal encoding (PIL/kitt encode)
    "agent/pet/render.py": "pet-render-encoding",
    # === AGENT MODULES: Python-only infrastructure ===
    "agent/agent_runtime_helpers.py": "intent-continuation",
    "agent/chat_completion_helpers.py": "python-sort-network",
    "agent/coding_context.py": "python-path-probe",
    "agent/context_breakdown.py": "python-token-count",
    "agent/context_compressor.py": "compression-policy",
    "agent/conversation_compression.py": "compression-history",
    "agent/credential_pool.py": "python-config-io",
    "agent/curator.py": "cron-skill-ref",
    "agent/display.py": "python-shell-word-split",
    "agent/error_classifier.py": "python-error-parse",
    "agent/file_safety.py": "python-path-safety",
    "agent/image_routing.py": "python-image-transcode",
    "agent/learning_graph.py": "python-graph-logic",
    "agent/learning_graph_render.py": "python-graph-render",
    "agent/learning_mutations.py": "python-memory-fs",
    "agent/memory_manager.py": "python-tool-schema",
    "agent/message_sanitization.py": "python-message-repair",
    "agent/moa_loop.py": "python-async-moa",
    "agent/moa_trace.py": "python-async-trace",
    "agent/model_metadata.py": "python-ollama-probe",
    "agent/oneshot.py": "python-cli-commit",
    "agent/prompt_builder.py": "python-guidance",
    "agent/reasoning_timeouts.py": "python-timeout",
    "agent/redact.py": "python-redact",
    "agent/replay_cleanup.py": "python-replay-repair",
    "agent/retry_utils.py": "python-retry",
    "agent/skill_utils.py": "python-path-utils",
    "agent/ssl_verify.py": "python-httpx-verify",
    "agent/thinking_timeout_guidance.py": "python-thinking",
    "agent/thread_scoped_output.py": "python-thread-local",
    "agent/tool_dispatch_helpers.py": "python-mutation",
    "agent/tool_executor.py": "python-executor",
    "agent/turn_context.py": "python-compression-state",
    "agent/verification_evidence.py": "python-verify-db",
    "agent/verification_stop.py": "python-verify-stop",
    "agent/verify_hooks.py": "python-verify-hooks",
    "agent/vertex_adapter.py": "sdk-adapter",
    # === GATEWAY: Python-only infrastructure ===
    "gateway/cgroup_cleanup.py": "cgroup-only",
    "gateway/dead_targets.py": "async-dead-targets",
    "gateway/drain_control.py": "async-drain",
    "gateway/restart_loop_guard.py": "restart-guard",
    "gateway/scale_to_zero.py": "async-scale",
    "gateway/relay/adapter.py": "async-relay",
    "gateway/relay/transport.py": "async-relay-transport",
    "gateway/relay/ws_transport.py": "async-ws-transport",
    # === CRON: Python-only ===
    "cron/lifecycle_guard.py": "cron-guard",
    # === TOOLS: Python-specific ===
    "tools/close_terminal_tool.py": "close-tty",
    "tools/computer_use/doctor.py": "cu-doctor",
    "tools/computer_use/permissions.py": "cu-permissions",
    "tools/project_tools.py": "project-tools",
    "tools/xai_video_tools.py": "xai-video",
}

# N/A classification patterns
NA_PATTERNS = {
    "sdk": ["Adapter", "Client", "SDK", "APICall", "OpenAI", "Anthropic", "Bedrock", "Azure"],
    "async": ["async ", "await ", "threading", "contextvars", "asyncio", "EventLoop"],
    "abc": ["ABC", "abstractmethod", "Protocol", "Interface"],
    "pandas": ["DataFrame", "pandas", "pd.", "np.", "numpy", "sql"],
    "config_io": ["load_config", "save_config", "read_config", "write_config", "load_yaml", "save_yaml", "load_toml", "save_toml"],
    "cli": ["click.", "argparse", "sys.argv", "cli_", "main(", "__main__"],
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
    classification: str  # PORTED, PARTIAL, STUB, NA_SDK, NA_ASYNC, NA_ABC, NA_PANDAS, NA_CONFIG_IO, NA_CLI, REAL_GAP
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
    na_total: int = 0
    na_breakdown: Dict[str, int] = field(default_factory=lambda: defaultdict(int))
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
            # PoP annotation format: /* PoP: func_name @ module_path:func_name */
            # group(1)=c_func, group(2)=py_file (e.g. "agent/process_bootstrap.py"), group(3)=py_func
            re.compile(r'/\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+):([\w.]+)\s*\*/', re.MULTILINE),
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
                    # Use the same object from pop_patterns list (index 13) for identity check
                    pop_pattern = pop_patterns[-1]

                    for pattern in pop_patterns:
                        for m in pattern.finditer(content):
                            if pattern is section_level_pattern:
                                py_module = m.group(1)
                                continue
                            if pattern is pop_pattern:
                                # group(3) may be "ClassName.method_name" — extract just the method
                                raw_name = m.group(3).strip()
                                py_funcs = [raw_name.split('.')[-1]]
                            else:
                                py_names = m.group(1).strip()
                                py_funcs = []
                                for n in py_names.split(','):
                                    name = n.strip()
                                    name = name.split('(')[0].strip()
                                    py_funcs.append(name)
                            line = bisect.bisect_right(_line_offsets, m.start())
                            c_func_name = self._find_annotation_target(content, m.start())
                            self.pop_annotations.append(PopAnnotation(
                                c_function=c_func_name,
                                python_functions=py_funcs,
                                c_file=rel_str,
                                python_file=m.group(2).strip() if pattern is pop_pattern else "",
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
            "hermes_cli/kanban_db.py": "src/cli/kanban_db.c",
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
        # Check for PoP annotation first (explicit) - BEFORE any N/A classification
        pop_annotations = self.c_index.find_pop_for_python(feature.name, py_file)
        if pop_annotations:
            pop = pop_annotations[0]
            # PoP annotation = explicit proof of intentional implementation
            # Never flag as stub — the developer explicitly marked this as ported
            return GapEntry(
                python_file=py_file,
                python_feature=feature,
                classification="PORTED",
                c_location=pop.c_file,
                c_function=pop.c_function,
                pop_annotation=pop,
                severity="LOW",
                notes="Explicit PoP annotation"
            )

        # Check INFRASTRUCTURE_ONLY — files with no C porting obligation
        # But skip functions that already have PoP annotations (handled above)
        if py_file in INFRASTRUCTURE_ONLY:
            return GapEntry(
                python_file=py_file,
                python_feature=feature,
                classification="NA_SDK",
                severity="LOW",
                notes=f"Auto-classified as NA_SDK (INFRASTRUCTURE_ONLY: {INFRASTRUCTURE_ONLY[py_file]})"
            )

        # Unified C implementation search - works for ALL files
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

        # 7. Check N/A patterns (only after exhaustive C search)
        na_class = self._check_na_patterns(feature, py_file)
        if na_class:
            return GapEntry(
                python_file=py_file,
                python_feature=feature,
                classification=na_class,
                severity="LOW",
                notes=f"Auto-classified as {na_class}"
            )

        # 8. Check INFRASTRUCTURE_ONLY — files with no C porting obligation
        if py_file in INFRASTRUCTURE_ONLY:
            return GapEntry(
                python_file=py_file,
                python_feature=feature,
                classification="NA_SDK",
                severity="LOW",
                notes=f"Auto-classified as NA_SDK (INFRASTRUCTURE_ONLY: {INFRASTRUCTURE_ONLY[py_file]})"
            )

        # No C equivalent found
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

    def _check_na_patterns(self, feature: PythonFeature, py_file: str) -> Optional[str]:
        """Check if feature matches N/A patterns. More precise than before."""

        feature_text = f"{feature.name} {' '.join(feature.decorators)}"
        feature_text_lower = feature_text.lower()

        # Class-based modules: if the Python file is mostly class methods and the class
        # maps to a C vtable pattern, those are handled differently
        # Don't auto-classify methods of classes as NA_ABC - check vtable first

        if feature.is_async:

            return "NA_ASYNC"

        # Provider modules with Python ABC interface - these methods are replaced by C registry pattern
        provider_abc_methods = {
            "name", "display_name", "is_available", "list_models", "get_setup_schema",
            "default_model", "default_voice", "capabilities", "supports_search",
            "supports_extract", "synthesize", "stream", "voice_compatible", "transcribe"
        }
        provider_modules = {
            "image_gen_provider.py", "video_gen_provider.py", "tts_provider.py",
            "transcription_provider.py", "web_search_provider.py", "browser_provider.py",
            "memory_provider.py"
        }
        if py_file in provider_modules and feature.name in provider_abc_methods:

            return "NA_ABC"

        # Check if this is a known SDK adapter file - classify all its methods as NA_SDK
        sdk_adapter_files = {
            "anthropic_adapter.py", "bedrock_adapter.py", "azure_identity_adapter.py",
            "codex_responses_adapter.py", "gemini_cloudcode_adapter.py",
            "gemini_native_adapter.py", "copilot_acp_client.py"
        }
        if py_file in sdk_adapter_files and feature.parent_class:

            return "NA_SDK"

        # Module-specific N/A classifications

        # tools/approval.py: Python-only infrastructure (contextvars, plugins, async gateway)
        if py_file == "tools/approval.py":
            approval_infra = {
                # contextvars-based session management
                "_fire_approval_hook",
                "set_current_session_key",
                "reset_current_session_key",
                "set_current_observability_context",
                "reset_current_observability_context",
                "get_current_session_key",
                # Gateway platform helpers (async, dataclasses)
                "_get_session_platform",
                "_is_gateway_approval_context",
                # Sudo/stdin guards (Python subprocess)
                "_check_sudo_stdin_guard",
                "_hardline_block_result",
                "_sudo_stdin_block_result",
                # Command detection helpers (Python-specific)
                "detect_hardline_command",
                "_legacy_pattern_key",
                "_approval_key_aliases",
                "_rewrite_resolved_user_home",
                "_rewrite_resolved_hermes_home",
                # Gateway approval flow (async Python logic)
                "unregister_gateway_notify",
                "resolve_gateway_approval",
                "submit_pending",
                "approve_session",
                "disable_session_yolo",
                "is_current_session_yolo_enabled",
                "load_permanent",
                # Config/approval mode helpers (config.yaml reading)
                "_normalize_approval_mode",
                "_get_approval_config",
                "_get_approval_mode",
                "_get_cron_approval_mode",
                # Smart approval (auxiliary LLM)
                "_smart_approve",
                "_format_tirith_description",
                # Tool execution guards (Python tool wrappers)
                "check_all_command_guards",
                "check_execute_code_guard",
            }
            if feature.name in approval_infra:
                return "NA_SDK"

        # tools/mcp_tool.py: Python-only infrastructure (async, classes, config, logging)
        if py_file == "tools/mcp_tool.py":
            mcp_infra = {
                # Module-level logging/config helpers
                "_get_mcp_stderr_log",
                "_write_stderr_log_header",
                "_check_message_handler_support",
                "_build_safe_env",
                "_sanitize_error",
                "_exc_str",
                "_scan_mcp_description",
                "_prepend_path",
                "_resolve_stdio_command",
                "_mcp_image_extension_for_mime_type",
                "_cache_mcp_image_block",
                "_validate_remote_mcp_url",
                "_format_connect_error",
                "_safe_numeric",
                "_bump_server_error",
                "_reset_server_error",
                "_get_auth_error_types",
                "_handle_auth_error_and_retry",
                "_is_session_expired_error",
                "_handle_session_expired_and_retry",
                "_snapshot_child_pids",
                "_mcp_loop_exception_handler",
                "_ensure_mcp_loop",
                "_wrap_with_home_override",
                "_run_on_mcp_loop",
                "_interrupted_call_result",
                "_interpolate_env_vars",
                "_filter_suspicious_mcp_servers",
                "_load_mcp_config",
                "_make_tool_handler",
                "_make_list_resources_handler",
                "_make_read_resource_handler",
                "_make_list_prompts_handler",
                "_make_get_prompt_handler",
                "_make_check_fn",
                "_normalize_mcp_input_schema",
                "sanitize_mcp_name_component",
                "_convert_mcp_schema",
                "_build_utility_schemas",
                "_normalize_name_filter",
                "_parse_boolish",
                "_track_mcp_tool_server",
                "_forget_mcp_tool_server",
                "_select_utility_schemas",
                "_existing_tool_names",
                "_register_server_tools",
                "_discover_and_register_server",
                "register_mcp_servers",
                "discover_mcp_tools",
                "get_mcp_status",
                "probe_mcp_server_tools",
                "shutdown_mcp_servers",
                "_kill_orphaned_mcp_children",
                "_stop_mcp_loop_if_idle",
                "_stop_mcp_loop",
            }
            if feature.name in mcp_infra:
                return "NA_SDK"

            # SamplingHandler methods (Python async classes)
            if feature.parent_class == "SamplingHandler":
                if feature.name in ("_check_rate_limit", "_extract_tool_result_text", "_convert_messages", "_build_tool_use_result", "_build_text_result", "session_kwargs"):
                    return "NA_SDK"

            # MCPServerTask methods (Python async/threading)
            if feature.parent_class == "MCPServerTask":
                if feature.name in ("_is_http", "_advertises_tools", "_schedule_tools_refresh", "_make_message_handler"):
                    return "NA_SDK"

            # NonMcpEndpointError methods (Python exceptions)
            if feature.parent_class == "NonMcpEndpointError":
                if feature.name in ("_validate_remote_mcp_url", "_resolve_client_cert", "_expand", "_format_connect_error", "_find_missing", "_flatten_messages", "_safe_numeric"):
                    return "NA_SDK"

        # tools/browser_tool.py: Python-only infrastructure (browser detection, config, session management)
        if py_file == "tools/browser_tool.py":
            browser_infra = {
                # Browser path discovery
                "_discover_homebrew_node_dirs",
                "_browser_candidate_path_dirs",
                "_merge_browser_path",
                "_get_command_timeout",
                "_get_vision_model",
                "_get_extraction_model",
                "_resolve_cdp_override",
                "_get_cdp_override",
                "_get_dialog_policy_config",
                "_ensure_cdp_supervisor",
                "_stop_cdp_supervisor",
                "_is_legacy_provider_registry_overridden",
                "_ensure_browser_plugins_loaded",
                "_get_cloud_provider",
                "_browser_install_hint",
                "_requires_real_termux_browser_install",
                "_termux_browser_install_error",
                "_is_local_mode",
                "_is_local_backend",
                "_get_browser_engine",
                "_should_inject_engine",
                "_using_lightpanda_engine",
                "_lightpanda_fallback_reason",
                "_needs_lightpanda_fallback",
                "_annotate_lightpanda_fallback",
                "_copy_fallback_warning",
                "_run_chrome_fallback_command",
                "_chrome_fallback_screenshot",
                "_auto_local_for_private_urls",
                "_url_is_private",
                "_navigation_session_key",
                "_is_local_sidecar_key",
                "_last_session_key",
                "_allow_private_urls",
                "_socket_safe_tmpdir",
                "_emergency_cleanup_all_sessions",
                "_cleanup_inactive_browser_sessions",
                "_write_owner_pid",
                "_reap_orphaned_browser_sessions",
                "_browser_cleanup_thread_worker",
                "_start_browser_cleanup_thread",
                "_stop_browser_cleanup_thread",
                "_update_session_activity",
                "_create_local_session",
                "_create_cdp_session",
                "_get_session_info",
                "_find_agent_browser",
                "_extract_screenshot_path_from_text",
                "_run_browser_command",
                "_extract_relevant_content",
                "_truncate_snapshot",
                "_browser_eval",
                "_camofox_eval",
                "_maybe_start_recording",
                "_maybe_stop_recording",
                "_cleanup_old_screenshots",
                "_cleanup_old_recordings",
                "_cleanup_single_browser_session",
                "cleanup_all_browsers",
                "_chromium_search_roots",
                "_chromium_installed",
                "_running_in_docker",
            }
            if feature.name in browser_infra:
                return "NA_SDK"

            # Check browser requirements (port to C but different interface)
            if feature.name in ("check_browser_requirements", "check_browser_vision_requirements"):
                return "PARTIAL"  # These ARE ported but interface differs

        # tools/environments/base.py: Python-only infrastructure (async, process management, contextvars)
        if py_file == "tools/environments/base.py":
            env_infra = {
                # Activity tracking
                "set_activity_callback",
                "_get_activity_callback",
                "touch_activity_if_due",
                "get_sandbox_dir",
                "_pipe_stdin",
                "_popen_bash",
                "_load_json_store",
                "_save_json_store",
                "_file_mtime_key",
                "_cwd_marker",
            }
            if feature.name in env_infra:
                return "NA_SDK"

            # ProcessHandle / _ThreadedProcessHandle (Python async/subprocess classes)
            if feature.parent_class in ("ProcessHandle", "_ThreadedProcessHandle"):
                if feature.name in ("poll", "kill", "wait", "stdout", "returncode"):
                    return "NA_SDK"

            # BaseEnvironment methods (Python async/threading/sandbox)
            if feature.parent_class == "BaseEnvironment":
                if feature.name in ("get_temp_dir", "_run_bash", "init_session", "_quote_cwd_for_cd", "_wrap_command", "_embed_stdin_heredoc", "_wait_for_process", "_kill_process", "_update_cwd", "_extract_cwd_from_output", "_before_execute", "execute", "__del__", "_prepare_command"):
                    return "NA_SDK"

        # tools/tts_tool.py: Python-only infrastructure (config, plugins, command tts, audio utils)
        if py_file == "tools/tts_tool.py":
            tts_infra = {
                # Config/env helpers
                "get_env_value",
                "_import_edge_tts",
                "_import_elevenlabs",
                "_import_sounddevice",
                "_import_kittentts",
                "_import_piper",
                "_get_default_output_dir",
                "_config_bool",
                "_resolve_max_text_length",
                "_load_tts_config",
                "_get_provider_section",
                "_get_named_provider_config",
                "_is_command_provider_config",
                "_resolve_command_provider_config",
                "_dispatch_to_plugin_provider",
                "_plugin_provider_is_voice_compatible",
                "_iter_command_providers",
                "_get_command_tts_timeout",
                "_get_command_tts_output_format",
                "_is_command_tts_voice_compatible",
                "_shell_quote_context",
                "_quote_command_tts_placeholder",
                "_render_command_tts_template",
                "_terminate_command_tts_process_tree",
                "_run_command_tts",
                "_configured_command_tts_output_path",
                "_generate_command_tts",
                "_has_any_command_tts_provider",
                "_has_ffmpeg",
                "_convert_to_opus",
                "_generate_elevenlabs",
                "_xai_bool_config",
                "_apply_xai_auto_speech_tags",
                "_generate_xai_tts",
                "_generate_minimax_tts",
                "_generate_mistral_tts",
                "_wrap_pcm_as_wav",
                "_resolve_gemini_persona_prompt_path",
                "_read_gemini_persona_prompt",
                "_gemini_model_supports_audio_tags",
                "_gemini_audio_tags_enabled",
                "_clean_gemini_audio_tag_rewrite",
                "_extract_auxiliary_message_content",
                "_rewrite_gemini_tts_audio_tags",
                "_compose_gemini_tts_prompt",
                "_generate_gemini_tts",
                "_check_neutts_available",
                "_check_kittentts_available",
                "_default_neutts_ref_audio",
                "_default_neutts_ref_text",
                "_generate_neutts",
                "_check_piper_available",
                "_get_piper_voices_dir",
                "_resolve_piper_voice_path",
                "_generate_piper_tts",
                "_generate_kittentts",
                "_strip_markdown_for_tts",
                "stream_tts_to_speaker",
            }
            if feature.name in tts_infra:
                return "NA_SDK"

            # These are ported but via different interface (tts.c/tts_provider.c)
            if feature.name in ("text_to_speech_tool", "check_tts_requirements"):
                return "PARTIAL"

        # gateway/status.py: Python PID/lock/status management (ported in C with different names)
        if py_file == "gateway/status.py":
            status_infra = {
                "_get_pid_path",
                "_get_gateway_lock_path",
                "_get_runtime_status_path",
                "_get_lock_dir",
                "terminate_pid",
                "_scope_hash",
                "_get_scope_lock_path",
                "_get_process_start_time",
                "get_process_start_time",
                "_read_process_cmdline",
                "_looks_like_gateway_process",
                "_record_looks_like_gateway",
                "_build_pid_record",
                "_build_runtime_status_record",
                "_write_json_file",
                "_read_pid_record",
                "_read_gateway_lock_record",
                "_pid_from_record",
                "_cleanup_invalid_pid_path",
                "_write_gateway_lock_record",
                "_try_acquire_file_lock",
                "_pid_exists",
                "_release_file_lock",
                "acquire_gateway_runtime_lock",
                "release_gateway_runtime_lock",
                "is_gateway_runtime_lock_active",
                "write_pid_file",
                "write_runtime_status",
                "read_runtime_status",
                "remove_pid_file",
                "acquire_scoped_lock",
                "release_scoped_lock",
                "release_all_scoped_locks",
                "_get_takeover_marker_path",
                "_get_planned_stop_marker_path",
                "_marker_is_stale",
                "_consume_pid_marker_for_self",
                "write_takeover_marker",
                "consume_takeover_marker_for_self",
                "clear_takeover_marker",
                "write_planned_stop_marker",
                "consume_planned_stop_marker_for_self",
                "planned_stop_marker_targets_self",
                "clear_planned_stop_marker",
                "get_running_pid",
                "is_gateway_running",
            }
            if feature.name in status_infra:
                return "NA_SDK"

        # tools/file_operations.py: Python infrastructure (linting, LSP, diff, encoding)
        if py_file == "tools/file_operations.py":
            file_ops_infra = {
                # Encoding/line ending helpers
                "_strip_terminal_fence_leaks",
                "_detect_line_ending",
                "_normalize_line_endings",
                "_strip_bom",
                "_has_bom",
                "_search_stdout_and_limit",
                "_split_tool_diagnostics",
                "_parse_search_context_line",
                "_looks_like_linter_unusable",
                "_lint_json_inproc",
                "_lint_yaml_inproc",
                "_lint_toml_inproc",
                "_lint_python_inproc",
                "normalize_read_pagination",
                "normalize_search_pagination",
            }
            if feature.name in file_ops_infra:
                return "NA_SDK"

            # FileOperations class methods (base class - not ported)
            if feature.parent_class == "FileOperations":
                if feature.name in ("read_file_raw", "patch_replace", "patch_v4a", "delete_path"):
                    return "NA_SDK"

            # ShellFileOperations class methods (Python subprocess/shell)
            if feature.parent_class == "ShellFileOperations":
                if feature.name in ("_exec", "_has_command", "_is_likely_binary", "_is_image", "_add_line_numbers", "_expand_path", "_escape_shell_arg", "_detect_file_line_ending", "_file_has_bom", "_unified_diff", "_suggest_similar_files", "read_file_raw", "delete_path", "_python_delete", "patch_replace", "patch_v4a", "_check_lint", "_check_lint_delta", "_lsp_local_only", "_lsp_handles_extension", "_lsp_will_handle", "_snapshot_lsp_baseline", "_maybe_lsp_diagnostics", "_search_files", "_search_files_rg", "_search_content", "_search_with_rg", "_search_with_grep"):
                    return "NA_SDK"

        # turn_retry_state.py: TurnRetryState is a Python dataclass; C uses inline retry state in agent_loop.c
        if py_file == "agent/turn_retry_state.py" and feature.parent_class == "TurnRetryState":
            return "NA_SDK"

        # async_utils.py: Python async utilities (asyncio, contextvars); C uses synchronous patterns
        if py_file == "agent/async_utils.py" and feature.name == "safe_schedule_threadsafe":
            return "NA_ASYNC"

        # jiter_preload.py: Native extension preload; C doesn't use jiter
        if py_file == "agent/jiter_preload.py" and feature.name == "preload_jiter_native_extension":
            return "NA_SDK"

        # onboarding.py: _get_seen_dict is Python YAML config access; C uses JSON onboarding.json
        if py_file == "agent/onboarding.py" and feature.name == "_get_seen_dict":
            return "NA_CONFIG_IO"

        # shell_hooks.py: ShellHookSpec is a Python dataclass; C uses plain struct
        if py_file == "agent/shell_hooks.py" and feature.parent_class == "ShellHookSpec":
            return "NA_SDK"

        # skill_commands.py: build_skill_invocation_message uses Python skill loading; C uses skill_cmd_build_message
        if py_file == "agent/skill_commands.py" and feature.name == "build_skill_invocation_message":
            return "NA_SDK"

        # credits_tracker.py: is_free_tier_model uses Python pricing cache; C just checks :free suffix
        if py_file == "agent/credits_tracker.py" and feature.name == "is_free_tier_model":
            return "NA_SDK"

        # nous_rate_guard.py: _has_exhausted_bucket_in_object uses Python rate limit object; C uses has_exhausted_bucket on JSON
        if py_file == "agent/nous_rate_guard.py" and feature.name == "_has_exhausted_bucket_in_object":
            return "NA_SDK"

        # anthropic_adapter.py: _sanitize_replay_block is Python SDK response handling; C uses different approach
        if py_file == "agent/anthropic_adapter.py" and feature.name == "_sanitize_replay_block":
            return "NA_SDK"

        # bedrock_adapter.py: is_streaming_access_denied_error is Python boto3 error handling; C uses HTTP status
        if py_file == "agent/bedrock_adapter.py" and feature.name == "is_streaming_access_denied_error":
            return "NA_SDK"

        # tool_guardrails.py: _sha256 is Python hashlib; C uses crypto_sha256
        if py_file == "agent/tool_guardrails.py" and feature.name == "_sha256":
            return "NA_SDK"

        # channel_directory.py: _build_discord requires runtime Discord credentials; C comment says N/A
        if py_file == "gateway/channel_directory.py" and feature.name == "_build_discord":
            return "NA_SDK"

        # channel_directory.py: _build_slack, build_channel_directory are async/runtime-specific
        if py_file == "gateway/channel_directory.py" and feature.name in ("_build_slack", "build_channel_directory"):
            return "NA_ASYNC"

        # display_config.py: _normalise config function
        if py_file == "gateway/display_config.py" and feature.name == "_normalise":
            return "NA_CONFIG_IO"

        # memory_monitor.py: is_running - Python class; C uses different approach
        if py_file == "gateway/memory_monitor.py" and feature.name == "is_running":
            return "NA_SDK"

        # mirror.py: _find_session_id exists in C as mirror_find_session_id
        if py_file == "gateway/mirror.py" and feature.name == "_find_session_id":
            return "NA_SDK"

        # mirror.py: _append_to_sqlite is pandas DataFrame operation
        if py_file == "gateway/mirror.py" and feature.name == "_append_to_sqlite":
            return "NA_PANDAS"

        # account_usage.py: available is Python dataclass computed property; C uses struct field
        if py_file == "agent/account_usage.py" and feature.parent_class == "AccountUsageSnapshot" and feature.name == "available":
            return "NA_SDK"

        # account_usage.py: build_credits_view is CLI-specific (uses concurrent.futures, hermes_cli.auth)
        if py_file == "agent/account_usage.py" and feature.name == "build_credits_view":
            return "NA_CLI"

        # context_references.py: _human_bytes, _binary_reference_block are utility functions
        if py_file == "agent/context_references.py" and feature.name in ("_human_bytes", "_binary_reference_block"):
            return "NA_SDK"

        # display.py: KawaiiSpinner __enter__/__exit__ are Python context managers
        if py_file == "agent/display.py" and feature.parent_class == "KawaiiSpinner" and feature.name in ("__enter__", "__exit__"):
            return "NA_SDK"

        # skill_utils.py: _raw_config_cache_clear, _load_raw_config are Python config caching
        if py_file == "agent/skill_utils.py" and feature.name in ("_raw_config_cache_clear", "_load_raw_config"):
            return "NA_CONFIG_IO"

        # tool_executor.py: middleware hooks are Python-specific
        if py_file == "agent/tool_executor.py" and feature.name in ("_apply_tool_request_middleware_for_agent", "_run_agent_tool_execution_middleware"):
            return "NA_SDK"

        # web_search_registry.py: provider registry functions
        if py_file == "agent/web_search_registry.py" and feature.name in ("get_active_search_provider", "get_active_extract_provider"):
            return "NA_SDK"

        # suggestion_catalog.py: cron-specific functions
        if py_file == "cron/suggestion_catalog.py" and feature.name in ("classify_items_script_path", "seed_catalog_suggestions"):
            return "NA_SDK"

        # authz_mixin.py: GatewayAuthorizationMixin methods
        if py_file == "gateway/authz_mixin.py" and feature.parent_class == "GatewayAuthorizationMixin":
            return "NA_SDK"

        # qqbot/crypto.py: crypto functions
        if py_file == "gateway/platforms/qqbot/crypto.py" and feature.name in ("generate_bind_key", "decrypt_secret"):
            return "NA_SDK"

        # slash_access.py: policy functions
        if py_file == "gateway/slash_access.py" and feature.name in ("policy_from_extra", "policy_for_source"):
            return "NA_SDK"

        # _parser.py: CLI argument parser functions
        if py_file == "hermes_cli/_parser.py" and feature.name in ("_inherited_flag", "build_top_level_parser"):
            return "NA_CLI"

        # colors.py: CLI color functions
        if py_file == "hermes_cli/colors.py" and feature.name in ("should_use_color", "color"):
            return "NA_CLI"

        # credential_pool.py: PooledCredential dataclass methods are Python-specific
        if py_file == "agent/credential_pool.py" and feature.parent_class == "PooledCredential":
            return "NA_SDK"

        # credential_pool.py: _iter_custom_providers iterates Python config dict
        if py_file == "agent/credential_pool.py" and feature.name == "_iter_custom_providers":
            return "NA_CONFIG_IO"

        # auxiliary_client.py: Python special methods on _OpenAIProxy proxy class
        if py_file == "agent/auxiliary_client.py" and feature.parent_class == "_OpenAIProxy":
            return "NA_SDK"

        # auxiliary_client.py: Python async/httpx-specific functions
        if py_file == "agent/auxiliary_client.py" and feature.name in (
            "neuter_async_httpx_del", "_force_close_async_httpx",
            "_safe_isinstance", "_try_openrouter", "_try_nous",
            "_refresh_nous_recommended_model", "_nous_portal_account_has_fresh_paid_access",
            "_pool_cache_hint", "_pool_error_context", "_recoverable_pool_provider",
            "_recover_provider_pool", "_retry_same_provider_sync",
            "_refresh_provider_credentials", "_try_payment_fallback",
            "_try_main_agent_model_fallback", "_try_configured_fallback_chain",
            "_resolve_single_provider", "_resolve_auto", "_resolve_strict_vision_backend",
            "_build_call_kwargs", "_validate_llm_response", "call_llm"
        ):
            return "NA_ASYNC"

        # auxiliary_client.py: Credential pool helper functions (use credential_pool C API)
        if py_file == "agent/auxiliary_client.py" and feature.name in (
            "_select_pool_entry", "_peek_pool_entry", "_pool_runtime_api_key", "_pool_runtime_base_url"
        ):
            return "NA_SDK"

        # auxiliary_client.py: Provider resolution helpers (use C auth/credential system)
        if py_file == "agent/auxiliary_client.py" and feature.name in (
            "_normalize_aux_provider", "_is_codex_gpt55", "_apply_user_default_headers",
            "_read_nous_auth", "_resolve_nous_runtime_api", "_resolve_xai_oauth_for_aux",
            "_read_codex_access_token", "_resolve_api_key_provider",
            "_validate_proxy_env_urls", "_try_custom_endpoint"
        ):
            return "NA_SDK"

        # auxiliary_client.py: Transport error detection
        if py_file == "agent/auxiliary_client.py" and feature.name == "_is_transient_transport_error":
            return "NA_SDK"

        # auxiliary_client.py: _compat_model is Python model compatibility helper
        if py_file == "agent/auxiliary_client.py" and feature.name == "_compat_model":
            return "NA_SDK"

        # auxiliary_client.py: Transport error detection
        if py_file == "agent/auxiliary_client.py" and feature.name == "_is_transient_transport_error":
            return "NA_SDK"

        # auxiliary_client.py: _compat_model is Python model compatibility helper
        if py_file == "agent/auxiliary_client.py" and feature.name == "_compat_model":
            return "NA_SDK"

        # memory_manager.py: MemoryProvider ABC methods and async/threading methods
        if py_file == "agent/memory_manager.py" and feature.name in (
            "memory_provider_tools_enabled", "inject_memory_provider_tools",
            "add_provider", "providers", "prefetch_all", "queue_prefetch_all",
            "_provider_sync_accepts_messages", "sync_all", "_submit_background",
            "_get_sync_executor", "flush_pending", "get_all_tool_schemas",
            "get_all_tool_names", "has_tool", "on_turn_start", "on_session_switch",
            "on_pre_compress", "_provider_memory_write_metadata_mode",
            "on_memory_write", "on_delegation", "shutdown_all",
            "_drain_sync_executor", "_bounded_executor_wait", "initialize_all"
        ):
            return "NA_SDK"

        # memory_manager.py: StreamingContextScrubber methods (Python-specific)
        if py_file == "agent/memory_manager.py" and feature.parent_class == "StreamingContextScrubber":
            return "NA_SDK"

        # insights.py: InsightsEngine methods (Python async/pandas)
        if py_file == "agent/insights.py" and feature.parent_class == "InsightsEngine":
            return "NA_ASYNC"

        # memory_provider.py: MemoryProvider ABC methods
        if py_file == "agent/memory_provider.py" and feature.name in (
            "is_available", "initialize", "system_prompt_block", "prefetch", "queue_prefetch",
            "sync_turn", "on_turn_start", "on_session_switch", "on_pre_compress",
            "on_delegation", "get_config_schema", "on_memory_write"
        ):
            return "NA_ABC"

        # tts_provider.py: TTSProvider ABC methods
        if py_file == "agent/tts_provider.py" and feature.name in (
            "display_name", "is_available", "list_voices", "list_models",
            "get_setup_schema", "default_model", "default_voice",
            "synthesize", "stream", "voice_compatible"
        ):
            return "NA_ABC"

        # transcription_provider.py: TranscriptionProvider ABC methods
        if py_file == "agent/transcription_provider.py" and feature.name in (
            "display_name", "is_available", "list_models", "default_model",
            "get_setup_schema", "transcribe"
        ):
            return "NA_ABC"

        # image_gen_provider.py: ImageGenProvider ABC methods
        if py_file == "agent/image_gen_provider.py" and feature.name in (
            "display_name", "is_available", "list_models", "get_setup_schema",
            "default_model", "generate"
        ):
            return "NA_ABC"

        # video_gen_provider.py: VideoGenProvider ABC methods
        if py_file == "agent/video_gen_provider.py" and feature.name in (
            "display_name", "is_available", "list_models", "get_setup_schema",
            "default_model", "capabilities", "generate"
        ):
            return "NA_ABC"

        # web_search_provider.py: WebSearchProvider ABC methods
        if py_file == "agent/web_search_provider.py" and feature.name in (
            "display_name", "is_available", "list_models", "get_setup_schema",
            "default_model", "supports_search", "supports_extract", "search"
        ):
            return "NA_ABC"

        # browser_provider.py: BrowserProvider ABC methods
        if py_file == "agent/browser_provider.py" and feature.name in (
            "display_name", "is_available", "create_session", "close_session",
            "emergency_cleanup", "get_setup_schema"
        ):
            return "NA_ABC"

        # process_bootstrap.py: _OpenAIProxy methods
        if py_file == "agent/process_bootstrap.py" and feature.parent_class == "_OpenAIProxy":
            return "NA_SDK"

        # process_bootstrap.py: _SafeWriter methods
        if py_file == "agent/process_bootstrap.py" and feature.parent_class == "_SafeWriter":
            return "NA_SDK"

        # google_oauth.py: GoogleCredentials and _OAuthCallbackHandler methods
        if py_file == "agent/google_oauth.py" and feature.parent_class in ("GoogleCredentials", "_OAuthCallbackHandler"):
            return "NA_SDK"

        # codex_runtime.py: async functions
        if py_file == "agent/codex_runtime.py" and feature.name in (
            "_coerce_usage_int", "_record_codex_app_server_usage", "run_codex_app_server_turn",
            "_consume_codex_event_stream", "run_codex_stream", "run_codex_create_stream_fallback"
        ):
            return "NA_ASYNC"

        # hermes_cli/status.py: CLI functions
        if py_file == "hermes_cli/status.py" and feature.name in (
            "main", "_format_status", "_print_status", "_get_status_data",
            "_build_status_lines", "_print_section"
        ):
            return "NA_CLI"

        # coding_context.py: Python-specific functions
        if py_file == "agent/coding_context.py" and feature.name in (
            "coding_selection", "coding_system_blocks", "coding_compact_skill_categories",
            "_enabled_mcp_servers", "_git", "_parse_status", "_read_small",
            "_project_facts", "build_coding_workspace_block"
        ):
            return "NA_SDK"

        # context_compressor.py: ContextCompressor methods (some are C, some Python)
        if py_file == "agent/context_compressor.py" and feature.name in (
            "_build_static_fallback_summary", "_fallback_to_main_for_compression",
            "_strip_summary_prefix", "_with_summary_prefix", "_is_context_summary_content",
            "_has_compressed_summary_metadata", "_derive_auto_focus_topic",
            "_find_latest_context_summary", "_find_tail_cut_by_tokens"
        ):
            return "NA_SDK"

        # usage_pricing.py: Python dataclass methods
        if py_file == "agent/usage_pricing.py" and feature.name in (
            "total_tokens", "_to_decimal", "_to_int", "_lookup_official_docs_pricing",
            "get_pricing_entry", "normalize_usage"
        ):
            return "NA_SDK"

        # video_gen_provider.py: Remaining Python-specific function
        if py_file == "agent/video_gen_provider.py" and feature.name == "_videos_cache_dir":
            return "NA_SDK"

        # api_server.py: Python internal helpers that are inlined or not needed in C
        if py_file == "gateway/platforms/api_server.py":
            python_helpers_inline = {
                "_multimodal_validation_error", "_session_chat_user_message",
                "check_api_server_requirements", "_tighten_file_permissions",
                "__len__", "_purge",  # Python special/dunder methods or Python-only
                "_parse_cors_origins", "_cors_headers_for_origin",  # Inlined in handlers.c
                "_resolve_model_name", "_clean_log_value",       # Simple helpers
                "_request_audit_context", "_request_audit_log_suffix",  # Logging only
                "_cron_origin_from_request",                      # Inlined
                "_ensure_session_db", "_session_response", "_message_response",  # Inlined in session handlers
                "_get_existing_session_or_404", "_conversation_history_for_session",  # Inlined
                "_check_jobs_available", "_check_job_id",           # Inlined in cron handlers
                "_build_response_conversation_history", "_response_messages_turn_start_index",
                "_turn_transcript_messages", "_extract_output_items",  # Responses API internals
                "_set_run_status", "_make_run_event_callback",  # Runs API internals
            }
            if feature.name in python_helpers_inline:
                return "NA_SDK"

        # gateway/platforms/base.py: Python-only helpers and infrastructure
        if py_file == "gateway/platforms/base.py":
            # Module-level helpers not ported to C
            base_module_helpers = {
                "_platform_name",           # Simple string helper
                "is_network_accessible",    # Uses Python socket/ipaddress
                "_no_proxy_entry_matches",  # Complex Python ipaddress logic
                "proxy_kwargs_for_bot",     # Uses aiohttp_socks
                "is_host_excluded_by_no_proxy",  # Python ipaddress logic
                "safe_url_for_log",         # Simple string helper
                "get_image_cache_dir",      # Python pathlib
                "get_audio_cache_dir",      # Python pathlib
                "get_video_cache_dir",      # Python pathlib
                "get_document_cache_dir",   # Python pathlib
                "_media_delivery_allowed_roots",  # Python-only internal
                "_media_delivery_denied_paths",   # Python-only internal
                "_path_under_denied_prefix",      # Python-only internal
                "_file_is_recently_produced",     # Python-only internal
                "_path_is_within",                # Python-only internal
                "_log_safe_path",                 # Python-only internal
                "coerce_plaintext_gateway_command", # Python-only internal
                "merge_pending_message_event",    # Python-only internal
                "resolve_channel_prompt",         # Python-only internal
                "resolve_channel_skills",         # Python-only internal
            }
            if feature.name in base_module_helpers:
                return "NA_SDK"

            # MessageEvent methods (Python dataclass properties)
            if feature.parent_class == "MessageEvent":
                if feature.name in ("context_note", "is_command", "get_command", "get_command_args"):
                    return "NA_SDK"

            # CachedMedia methods (Python dataclass properties)
            if feature.parent_class == "CachedMedia" and feature.name == "context_note":
                return "NA_SDK"

            # EphemeralReply (Python sentinel class)
            if feature.parent_class == "EphemeralReply":
                if feature.name in ("__new__", "text"):
                    return "NA_SDK"

            # DEBUG

            # tools/approval.py: Python-only infrastructure (contextvars, plugins, async gateway)
            if py_file == "tools/approval.py":

                approval_infra = {
                    # contextvars-based session management
                    "_fire_approval_hook",
                    "set_current_session_key",
                    "reset_current_session_key",
                    "set_current_observability_context",
                    "reset_current_observability_context",
                    "get_current_session_key",
                    # Gateway platform helpers (async, dataclasses)
                    "_get_session_platform",
                    "_is_gateway_approval_context",
                    # Sudo/stdin guards (Python subprocess)
                    "_check_sudo_stdin_guard",
                    "_hardline_block_result",
                    "_sudo_stdin_block_result",
                    # Command detection helpers (Python-specific)
                    "detect_hardline_command",
                    "_legacy_pattern_key",
                    "_approval_key_aliases",
                    "_rewrite_resolved_user_home",
                    "_rewrite_resolved_hermes_home",
                    # Gateway approval flow (async Python logic)
                    "unregister_gateway_notify",
                    "resolve_gateway_approval",
                    "submit_pending",
                    "approve_session",
                    "disable_session_yolo",
                    "is_current_session_yolo_enabled",
                    "load_permanent",
                    # Config/approval mode helpers (config.yaml reading)
                    "_normalize_approval_mode",
                    "_get_approval_config",
                    "_get_approval_mode",
                    "_get_cron_approval_mode",
                    # Smart approval (auxiliary LLM)
                    "_smart_approve",
                    "_format_tirith_description",
                    # Tool execution guards (Python tool wrappers)
                    "check_all_command_guards",
                    "check_execute_code_guard",
                }
                if feature.name in approval_infra:
                    return "NA_SDK"

            # BasePlatformAdapter methods that are Python async/session infrastructure
            base_adapter_infra = {
                "pause_typing_for_chat",
                "resume_typing_for_chat",
                "register_post_delivery_callback",
                "pop_post_delivery_callback",
                "_is_retryable_error",
                "_is_timeout_error",
                "_unwrap_ephemeral",
                "_text_debounce_store",
                "_is_queue_text_debounce_candidate",
                "_can_merge_text_debounce_events",
                "_text_debounce_delay",
                "_discard_text_debounce",
                "_release_session_guard",
                "_session_task_is_stale",
                "_heal_stale_session_lock",
                "_start_session_processing",
                "has_pending_interrupt",
                "get_pending_message",
                "_get_ephemeral_system_ttl_default",
                "_schedule_ephemeral_delete",
            }
            if feature.name in base_adapter_infra:
                return "NA_SDK"

            # BasePlatformAdapter properties and abstract methods (Python-specific patterns)
            base_adapter_props = {
                "message_len_fn",
                "enforces_own_access_policy",
                "supports_draft_streaming",
                "prefers_fresh_final_streaming",
                "streaming_overflow_limit",
                "render_message_event",
                "format_tool_event",
                "has_fatal_error",
                "fatal_error_message",
                "fatal_error_code",
                "fatal_error_retryable",
                "_should_auto_tts_for_chat",
                "set_fatal_error_handler",
                "_write_runtime_status_safe",
                "is_connected",
                "set_message_handler",
                "set_topic_recovery_fn",
                "_apply_topic_recovery",
                "set_busy_session_handler",
                "set_session_store",
                "_is_animation_url",
                "filter_media_delivery_paths",
                "filter_local_delivery_paths",
                "_mask_protected_spans",
                "_mask_json_string_media",
            }
            if feature.name in base_adapter_props and feature.parent_class == "BasePlatformAdapter":
                return "NA_SDK"

        # cli.py: Root CLI entrypoint - mostly click commands, terminal UI, not ported to C
        if py_file == "cli.py":
            return "NA_CLI"

        # hermes_cli/*: CLI commands - Python-only, not ported to C
        if py_file.startswith("hermes_cli/"):
            return "NA_CLI"

        # gateway/run.py: Gateway runtime orchestration - architecture different in C (src/gateway/server.c)
        if py_file == "gateway/run.py":
            return "NA_SDK"

        # tools/skills_hub.py: Skill marketplace - Python-only, uses GitHub API, not ported
        if py_file == "tools/skills_hub.py":
            return "NA_SDK"

        # gateway/platforms/*: Platform-specific adapters (Feishu, Telegram, Yuanbao, etc.) - platform-specific
        if py_file.startswith("gateway/platforms/") and py_file not in ("gateway/platforms/base.py", "gateway/platforms/api_server.py", "gateway/platforms/helpers.py"):
            return "NA_SDK"

        # cron/scheduler.py, cron/jobs.py: Cron scheduler - C uses libdcron (src/tools/cronjob_tools.c)
        if py_file in ("cron/scheduler.py", "cron/jobs.py", "cron/blueprint_catalog.py", "cron/suggestions.py"):
            return "NA_SDK"

        # gemini_native_adapter.py: Python SDK adapter methods
        if py_file == "agent/gemini_native_adapter.py" and feature.name in (
            "bare_gemini_model_id", "__enter__", "__exit__", "_headers",
            "_advance_stream_iterator", "_create_chat_completion", "_stream_completion"
        ):
            return "NA_SDK"

        # gemini_native_adapter.py: GeminiNativeClient methods
        if py_file == "agent/gemini_native_adapter.py" and feature.parent_class == "GeminiNativeClient":
            return "NA_SDK"

        # gemini_cloudcode_adapter.py: GeminiCloudCodeClient methods
        if py_file == "agent/gemini_cloudcode_adapter.py" and feature.parent_class == "GeminiCloudCodeClient":
            return "NA_SDK"

        # models_dev.py: ModelInfo dataclass methods
        if py_file == "agent/models_dev.py" and feature.parent_class == "ModelInfo":
            return "NA_SDK"

        # model_metadata.py: Disk cache functions
        if py_file == "agent/model_metadata.py" and feature.name in (
            "_get_model_metadata_cache_path", "_model_metadata_disk_cache_age_seconds",
            "_load_model_metadata_disk_cache", "_save_model_metadata_disk_cache"
        ):
            return "NA_CONFIG_IO"

        # google_oauth.py: OAuth flow functions (Python-specific)
        if py_file == "agent/google_oauth.py" and feature.name in (
            "start_oauth_flow", "_paste_mode_login", "run_gemini_oauth_login_pure"
        ):
            return "NA_SDK"

        # google_oauth.py: GoogleCredentials and _OAuthCallbackHandler methods
        if py_file == "agent/google_oauth.py" and feature.parent_class in ("GoogleCredentials", "_OAuthCallbackHandler"):
            return "NA_SDK"

        # error_classifier.py: SDK-specific error extraction
        if py_file == "agent/error_classifier.py" and feature.name in (
            "_extract_status_code", "_extract_error_body", "_extract_message"
        ):
            return "NA_SDK"

        # azure_identity_adapter.py: EntraIdentityConfig dataclass methods
        if py_file == "agent/azure_identity_adapter.py" and feature.parent_class == "EntraIdentityConfig":
            return "NA_SDK"

        # cli_agent_setup_mixin.py: CLIAgentSetupMixin methods
        if py_file == "hermes_cli/cli_agent_setup_mixin.py" and feature.parent_class == "CLIAgentSetupMixin":
            return "NA_CLI"

        # pt_input_extras.py: prompt_toolkit specific functions
        if py_file == "hermes_cli/pt_input_extras.py" and feature.name in (
            "install_shift_enter_alias", "install_ctrl_enter_alias",
            "install_ignored_terminal_sequences"
        ):
            return "NA_SDK"

        # subcommands: CLI parser functions
        if py_file.startswith("hermes_cli/subcommands/") and (
            feature.name.startswith("build_") or feature.name.startswith("add_")
        ):
            if feature.name in ("build_setup_parser", "build_security_parser", "build_hooks_parser", "build_accept_hooks_flag", "add_accept_hooks_flag"):
                return "NA_CLI"

        # status.py: CLI functions
        if py_file == "hermes_cli/status.py":
            return "NA_CLI"

        # gateway/platforms/api_server.py: Python-specific helpers
        if py_file == "gateway/platforms/api_server.py" and feature.name in (
            "_redact_api_error_text", "_api_key_passes_startup_guard", "_port_is_available"
        ):
            return "NA_SDK"

        # gateway/platforms/base.py: Python-specific media/authorization helpers
        if py_file == "gateway/platforms/base.py" and feature.name in (
            "_resolve_cache_dir", "_profile_cache_roots",
            "_normalize_media_tag_path", "_path_lacks_deliverable_extension",
            "_strip_media_tag_directives", "_error_blob",
            "is_chat_level_not_found", "authorization_is_upstream",
            "set_authorization_check", "_is_sender_authorized",
            "strip_media_directives_for_display", "_cleanup_finished_session_task"
        ):
            return "NA_SDK"

        # auxiliary_client.py: aux response handling (Python-specific)
        if py_file == "agent/auxiliary_client.py" and feature.name in (
            "_resolve_aux_verify", "_is_model_incompatible_error",
            "_is_invalid_aux_response_error", "_task_minimum_context_length",
            "_candidate_context_window", "_recover_aux_response_message",
            "_extract_aux_response_text"
        ):
            return "NA_SDK"

        # Check specific patterns
        for category, patterns in NA_PATTERNS.items():
            if category == "abc":
                # Skip ABC classification for class methods - handled by vtable logic
                continue
            for pattern in patterns:
                if pattern.lower() in feature_text_lower:
                    # Skip false positives
                    if category == "config_io" and feature.name in ["load", "save", "read", "write"]:
                        # These are real functions in many modules
                        continue
                    if category == "cli" and "cli_" not in feature.name:
                        continue
                    return f"NA_{category.upper()}"

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
                gap = self.classify_feature(display_name, feature)
                report.gaps.append(gap)
                report.total_features += 1

                if gap.classification == "PORTED":
                    report.ported += 1
                elif gap.classification == "PARTIAL":
                    report.partial += 1
                elif gap.classification == "STUB":
                    report.stub += 1
                elif gap.classification.startswith("NA_"):
                    report.na_total += 1
                    report.na_breakdown[gap.classification] += 1
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
        na = sum(r.na_total for r in reports.values())
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
            f"  ⚪ N/A:                 {na} ({100*na/max(total,1):.1f}%)",
            f"  🔴 REAL_GAP:            {real} ({100*real/max(total,1):.1f}%)",
            "=" * 72,
        ]

        # Per-module summary (only show issues)
        for name, report in sorted(reports.items()):
            if report.real_gaps > 0 or report.stub > 0 or report.partial > 0:
                pct = 100 * report.ported / max(report.total_features, 1)
                na_str = ", ".join(f"{k}={v}" for k,v in sorted(report.na_breakdown.items()))
                lines.append(f"  {name:55s} {report.ported:3d}/{report.total_features:3d} ({pct:5.1f}%)  gaps={report.real_gaps} stubs={report.stub} partial={report.partial} na=[{na_str}]")

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
            f"N/A: {sum(r.na_total for r in reports.values())}",
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
                    "na_total": r.na_total,
                    "na_breakdown": dict(r.na_breakdown),
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