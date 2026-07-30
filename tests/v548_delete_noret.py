#!/usr/bin/env python3
"""v548 no-return facade deleter. Mirrors v548_delete_facades.py but targets the
22 unreferenced NORET_SHAPE functions (honest REAL_GAP: delete PoP + no-op body).
The 3 referenced no-return fns are NOT in this list (handled manually).
Usage: python3 tests/v548_delete_noret.py [--apply]
"""
import json, re, sys
from pathlib import Path

SLERMES = Path("/home/wubu/hermes-agent-dev/slermes")
# 22 unreferenced no-return facade names (verified by reference scan)
TARGET = {
 "src/tools/port_browser_tool.c": [
    "_annotate_lightpanda_fallback","_cleanup_inactive_browser_sessions",
    "_cleanup_old_recordings","_cleanup_old_screenshots","_cleanup_single_browser_session",
    "_emergency_cleanup_all_sessions","_ensure_browser_plugins_loaded","_maybe_start_recording",
    "_maybe_stop_recording","_reap_orphaned_browser_sessions","_stop_browser_cleanup_thread",
    "_update_session_activity",
 ],
 "src/tools/port_mcp_tool.c": [
    "mcp_tool_reinject_post_build_tools","mcp_tool_schedule_tools_refresh",
    "mcp_tool_stop_mcp_loop","mcp_tool_stop_mcp_loop_if_idle",
 ],
 "src/cli/port_agent_skill_utils.c": ["cli_agent_skill_utils__raw_config_cache_clear"],
 "src/cli/port_gateway_platforms_signal_rate_limit.c": ["cli_gateway_platforms_signal_rate_limit_report_rpc_duration"],
 "src/cli/port_hermes_cli_skills_config.c": ["cli_hermes_cli_skills_config_skills_command"],
 "src/cli/port_hermes_cli_voice.c": ["cli_hermes_cli_voice_start_recording"],
 "src/cli/port_tools_microsoft_graph_auth.c": ["cli_tools_microsoft_graph_auth_clear_cache"],
 "src/cli/port_tools_website_policy.c": ["cli_tools_website_policy_invalidate_cache"],
 "src/cli/port_hermes_cli_memory_setup.c": ["cli_hermes_cli_memory_setup_cmd_setup"],
 "src/cli/port_agent_think_scrubber.c": ["cli_agent_think_scrubber_reset"],
 "src/tools/port_web_tools.c": ["web_ensure_web_plugins_loaded"],
}
POP_RE = re.compile(r"/\* PoP:\s*(\w+)\s*@\s*([^:*]+?):(\w+)\s*\*/")
defname_re = re.compile(r"(?<![A-Za-z0-9_.>])([A-Za-z_]\w*)\s*\(")
APPLY = "--apply" in sys.argv

def find_func_end(text, start):
    assert text[start] == "{"
    depth=1; i=start+1; n=len(text)
    while i<n and depth>0:
        c=text[i]
        if c=="'":
            i+=1
            while i<n and text[i]!="'":
                if text[i]=="\\": i+=2
                else: i+=1
            i+=1; continue
        if c=='"':
            i+=1
            while i<n and text[i]!='"':
                if text[i]=="\\": i+=2
                else: i+=1
            i+=1; continue
        if c=="/" and i+1<n and text[i+1]=="*":
            i+=2
            while i+1<n and not (text[i]=="*" and text[i+1]=="/"): i+=1
            i+=2; continue
        if c=="/" and i+1<n and text[i+1]=="/":
            while i<n and text[i]!="\n": i+=1
            continue
        if c=="{": depth+=1
        elif c=="}": depth-=1
        i+=1
    return i

total=0
for frel, names in TARGET.items():
    fpath=SLERMES/frel
    text=fpath.read_text()
    pops=[(m.start(),m.group(1)) for m in POP_RE.finditer(text)]
    ranges=[]
    for pos,name in pops:
        if name not in names: continue
        rest=text[pos:]
        mf=defname_re.search(rest)
        if not mf: continue
        def_start=pos+mf.start()
        p=def_start+len(mf.group(1))
        while p<len(text) and text[p]!="(": p+=1
        if p>=len(text): continue
        depth=0;i=p
        while i<len(text):
            if text[i]=="(": depth+=1
            elif text[i]==")":
                depth-=1
                if depth==0: break
            i+=1
        j=i+1
        while j<len(text) and text[j] in " \t\r\n": j+=1
        if j>=len(text) or text[j]!="{": continue
        ranges.append((pos, find_func_end(text,j)))
    new=text
    for a,b in sorted(ranges,key=lambda t:-t[0]):
        new=new[:a]+new[b:]
    if ranges:
        if APPLY: fpath.write_text(new)
        print(f"[{'APPLY' if APPLY else 'DRY'}] {frel}: removed {len(ranges)} fn(s)")
        total+=len(ranges)
print(f"\nTotal no-return facades removed (plan): {total}")
