"""DEBT, not policy: oracle references that no longer resolve.

Generated from tests/verify_oracle_drift.py. Each entry is a name, a
module path, or a call arity the oracle uses that the live module no
longer has. Disposition is per-entry: repoint (moved), drop the case
(gone), fix the call, or declare the port incomplete. Regenerate, then
re-run the drift gate -- do not hand-edit to swallow a NEW finding.
"""
ALLOW = {
    "sta_oracle_install_repair.py:109": "upstream helper deleted; the C port still implements it",
    "sta_oracle_install_repair.py:80": "upstream helper deleted; the C port still implements it",
    "sta_oracle_install_repair.py:81": "upstream helper deleted; the C port still implements it",
    "sta_oracle_read_extract_pure.py:27": "upstream helper deleted; the C port still implements it",
    "sta_oracle_read_extract_pure.py:31": "upstream helper deleted; the C port still implements it",
    "sta_oracle_turn_summary.py:41": "upstream helper deleted; the C port still implements it",
    "sta_oracle_video_gen_cache_dir.py:37": "upstream helper deleted; the C port still implements it",
    "sta_oracle_agent_estop.py:23": "agent.estop",
    "sta_oracle_agent_estop.py:35": "agent.estop",
    "sta_oracle_dashboard_pure.py:11": "hermes_cli.dashboard_procs._valid_lockfile_payload",
    "sta_oracle_file_search.py:22": "tools.file_operations._split_tool_diagnostics",
    "sta_oracle_github_provider.py:19": "tools.skills_hub.github_provider_for",
    "sta_oracle_hermes_cli_shared_metrics_contract.py:17": "hermes_cli.observability.shared_metrics_contract.tool_approval_outcome",
    "sta_oracle_managed_tool_gateway.py:14": "tools.managed_tool_gateway.managed_gateway_auth_headers",
    "sta_oracle_mcp_identity.py:15": "tools.mcp_tool._apply_identity_header",
    "sta_oracle_message_sanitization_pure.py:9": "agent.message_sanitization._family_rule",
    "sta_oracle_process_bootstrap_pure.py:15": "agent.process_bootstrap._interleave_addrinfos",
    "sta_oracle_search_context.py:22": "tools.file_operations._parse_search_context_line",
    "sta_oracle_skills_guard.py:24": "tools.skills_guard._finding_dict",
    "sta_oracle_skills_hub_filter.py:25": "tools.skills_hub._filter_results_by_provider",
    "sta_oracle_skills_hub_path.py:20": "tools.skills_hub._validate_install_parent_path",
    "sta_oracle_tool_search.py:24": "tools.tool_search._tokenize",
    "sta_oracle_tools_approval.py:5": "tools.approval.unregister_gateway_notify",
    "sta_oracle_tools_interrupt.py:5": "tools.interrupt._ThreadAwareEventProxy",
    "sta_oracle_tools_mcp_oauth_manager.py:5": "tools.mcp_oauth_manager._make_hermes_provider_class",
    "sta_oracle_tools_microsoft_graph_auth.py:5": "tools.microsoft_graph_auth.clear_cache",
    "sta_oracle_tools_read_terminal_tool.py:5": "tools.read_terminal_tool.check_read_terminal_requirements",
    "sta_oracle_tools_skill_usage.py:5": "tools.skills_sync.is_bundled_skills_opt_out",
    "sta_oracle_tools_skills_hub.py:5": "tools.skills_hub.GitHubAuth",
    "sta_oracle_tools_skills_sync.py:5": "tools.skills_sync.is_bundled_skills_opt_out",
    "sta_oracle_tools_web_tools.py:5": "tools.web_tools._get_extract_char_limit",
    "sta_oracle_tools_website_policy.py:5": "tools.website_policy.invalidate_cache",
    "sta_oracle_tools_write_approval.py:5": "tools.write_approval.is_background",
    "sta_oracle_update_completion.py:11": "hermes_cli.update_cmd._print_update_completion",
    "sta_oracle_url_safety.py:91": "tools.url_safety._safe_connect_scheme",
    "sta_oracle_web_git_base.py:10": "hermes_cli.web_git",
}
