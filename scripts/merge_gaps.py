#!/usr/bin/env python3
"""merge_gaps.py v2 — fold *_gaps.c residue into canonical module files.

File-level mapping table (curated by hand after symbol audit), with
per-function splitting for grab-bag files. Conservative dedup: drop a
residue top-level def only if the canonical FILE defines the same name.
"""
import os, re, subprocess, json, sys

ROOT = '/home/wubu/hermes-agent-dev/slermes'
SRC  = os.path.join(ROOT, 'src')
DRY  = '--apply' not in sys.argv

# ---------- curated mapping ----------
# (gap file) -> list of (py_module_regex_or_None_for_rest, canonical_path)
# None regex = default destination for everything not matched.
MAP = {
 'src/agent/final_gaps.c':            [(r'^cli\.py$',                'src/cli/cli.c'),
                                       (None,                        'src/tools/skills_hub.c')],
 'src/agent/final_missing_gaps.c':    [(r'^hermes_cli/auth\.py$',    'src/hermes_cli/auth.c'),
                                       (r'^hermes_cli/kanban_db\.py$','src/hermes_cli/kanban_db.c'),
                                       (r'^hermes_cli/main\.py$',     'src/hermes_cli/main.c'),
                                       (None,                         'src/gateway/channel_directory.c')],
 'src/agent/truly_missing_gaps.c':    [(r'^cron/blueprint_catalog\.py$','src/cron/blueprint_catalog.c'),
                                       (r'^hermes_cli/models\.py$',   'src/hermes_cli/models.c'),
                                       (r'^hermes_cli/backup\.py$',   'src/hermes_cli/backup.c'),
                                       (r'^gateway/drain_control\.py$','src/gateway/drain_control.c'),
                                       (r'^agent/verification_stop\.py$','src/gateway/run.c'),
                                       (r'^tools/terminal_tool\.py$', 'src/tools/terminal_tool.c'),
                                       (r'^gateway/run\.py$',         'src/gateway/run.c')],
 'src/agent/multi_mod_gaps.c':        [(r'^gateway/session\.py$',    'src/gateway/session.c'),
                                       (r'^agent/context_compressor\.py$','src/agent/context_compressor.c'),
                                       (r'^gateway/platforms/weixin\.py$','src/gateway/platforms/weixin.c'),
                                       (r'^tools/process_registry\.py$','src/tools/process_registry.c'),
                                       (None,                         'src/tools/mcp_tool.c')],
 'src/agent/small_mods_gaps.c':       [(r'^hermes_cli/auth\.py$',    'src/hermes_cli/auth.c'),
                                       (r'^gateway/slash_commands\.py$','src/gateway/slash_commands.c'),
                                       (r'^gateway/',                 'src/gateway/session.c'),
                                       (r'^agent/retry_utils\.py$',   'src/agent/retry_utils.c'),
                                       (r'^agent/turn_summary\.py$',  'src/agent/turn_summary.c'),
                                       (r'^agent/credits_tracker\.py$','src/agent/credits_tracker.c'),
                                       (r'^agent/learning_graph_render\.py$','src/agent/insights.c'),
                                       (None,                         'src/agent/delegation_context.c')],
 'src/agent/transports_codex_gaps.c': [(r'^agent/model_metadata\.py$','src/agent/model_metadata.c'),
                                       (None,                         'src/agent/transports/codex_app_server_session.c')],
 'src/agent/real_gaps.c':             [(r'^cli\.py$',                'src/cli/cli.c'),
                                       (r'^agent/markdown_tables\.py$','src/agent/markdown_tables.c'),
                                       (r'^agent/outbound_webhooks\.py$','src/agent/outbound_webhooks.c'),
                                       (r'^agent/delegation_context\.py$','src/agent/delegation_context.c'),
                                       (r'^agent/secret_sources/base\.py$','src/agent/secret_common.c'),
                                       (r'^hermes_cli/approvals_suggest\.py$','src/agent/approvals_suggest_proposals.c'),
                                       (r'^tools/skills_hub\.py$',    'src/tools/skills_hub.c'),
                                       (None,                         'src/agent/verification_evidence.c')],
 'src/agent/coding_ctx_gaps.c':       [(None,                        'src/agent/coding_context.c')],
 'src/agent/hermes_state_gaps.c':     [(None,                        'src/agent/hermes_state/hermes_state_misc.c')],
 'src/hermes_cli/gaps.c':             [(None,                        'src/cli/cli.c')],
 'src/hermes_cli/pure_gaps.c':        [(None,                        'src/cli/cli.c')],
 'src/hermes_cli/parity_gaps.c':      [(r'^hermes_cli/cli_commands_mixin\.py$','src/hermes_cli/main.c'),
                                       (None,                        'src/cli/cli.c')],
 'src/hermes_cli/banner_gaps.c':      [(None,                        'src/hermes_cli/banner_pure.c')],
 'src/hermes_cli/doctor_live_gaps.c': [(None,                        'src/hermes_cli/doctor.c')],
 'src/hermes_cli/plugins_gaps.c':     [(None,                        'src/hermes_cli/plugins_core.c')],
 'src/hermes_cli/profiles_router_gaps.c':[(None,                     'src/hermes_cli/web_server.c')],
 'src/hermes_cli/shared_metrics_gaps.c':[(None,                      'src/hermes_cli/shared_metrics.c')],
 'src/tools/browser_use_cli_gaps.c':  [(None,                        'src/tools/browser_use_cli.c')],  # new canonical
 'src/tools/environment_gaps.c':      [(None,                        'src/tools/env_probe.c')],  # catalog comment file -> fold as doc? handled special
 # ---- clean 1:1 residues (whole file -> canonical) ----
 'src/agent/auxiliary_client_gaps.c': [(None,                        'src/agent/auxiliary_client.c')],
 'src/agent/context_breakdown_gaps.c':[(None,                        'src/agent/context_breakdown.c')],
 'src/agent/markdown_tables_gaps.c':  [(None,                        'src/agent/markdown_tables.c')],
 'src/agent/insights_gaps.c':         [(None,                        'src/agent/insights.c')],
 'src/agent/models_dev_gaps.c':       [(None,                        'src/agent/models_dev.c')],
 'src/cron/scheduler_gaps.c':         [(None,                        'src/cron/scheduler.c')],
 'src/gateway/run_gaps.c':            [(None,                        'src/gateway/run.c')],
 'src/gateway/run_gaps2.c':           [(None,                        'src/gateway/run.c')],
 'src/gateway/session_gaps.c':        [(None,                        'src/gateway/session.c')],
 'src/hermes_cli/goals_gaps.c':       [(None,                        'src/hermes_cli/goals.c')],
 'src/hermes_cli/kanban_db_gaps.c':   [(None,                        'src/hermes_cli/kanban_db.c')],
 'src/hermes_cli/main_gaps.c':        [(None,                        'src/hermes_cli/main.c')],
 'src/hermes_cli/config_gaps.c':      [(None,                        'src/hermes_cli/config.c')],
 'src/hermes_cli/web_server_gaps.c':  [(None,                        'src/hermes_cli/web_server.c')],
 'src/hermes_cli/service_manager_gaps.c':[(None,                     'src/hermes_cli/service_manager.c')],
 'src/hermes_cli/update_cmd_gaps.c':  [(None,                        'src/hermes_cli/update_cmd.c')],
 'src/tools/approval_gaps.c':         [(None,                        'src/tools/approval.c')],
 'src/tools/file_operations_gaps.c':  [(None,                        'src/tools/file_operations.c')],
 'src/tools/mcp_tool_gaps.c':         [(None,                        'src/tools/mcp_tool.c')],
 'src/tools/read_extract_gaps.c':     [(None,                        'src/tools/read_extract.c')],
 'src/tools/skills_hub_gaps.c':       [(None,                        'src/tools/skills_hub.c')],
 'src/tools/terminal_tool_gaps.c':    [(None,                        'src/tools/terminal_tool.c')],
 'src/tools/tts_tool_gaps.c':         [(None,                        'src/tools/tts_tool.c')],
 'src/tools/wake_word_gaps.c':        [(None,                        'src/tools/wake_word.c')],
}

def dest_for(gap_rel, py_mod):
    for pat, dst in MAP.get(gap_rel, []):
        if pat is None or re.search(pat, py_mod):
            return dst
    return None

# ---------- C parsing ----------
def defs_in(text):
    out=set()
    for m in re.finditer(r'^[A-Za-z_][A-Za-z0-9_ \*]*?[\* ]([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{)]*\)\s*\{', text, re.M):
        out.add(m.group(1))
    for m in re.finditer(r'typedef\s+(?:struct|union|enum)[^;{]*\{[^}]*\}\s*([A-Za-z_][A-Za-z0-9_]*)\s*;', text):
        out.add(m.group(1))
    for m in re.finditer(r'^typedef[^;{]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*;', text, re.M):
        out.add(m.group(1))
    return out

POP=re.compile(r'/\* PoP: ([A-Za-z0-9_]+) @ ([A-Za-z0-9_/.]+\.py):[A-Za-z0-9_]+ \*/')
HDR=re.compile(r'/\* ── ([a-zA-Z0-9_]+) \((L\d+)\) ── \*/')

def split_functions(text):
    """Split into top-level blocks keyed by preceding PoP annotation if any."""
    lines=text.split('\n'); n=len(lines); blocks=[]; i=0
    while i<n:
        l=lines[i]
        # find a function start at col 0
        m=re.match(r'[A-Za-z_].*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{]*$', l)
        if m and not l.startswith('typedef'):
            j=i; depth=0; started=False
            while j<n:
                depth+=lines[j].count('{')-lines[j].count('}')
                if '{' in lines[j]: started=True
                if started and depth<=0: break
                if ';' in lines[j] and not started: break
                j+=1
            if not started: i+=1; continue
            # walk back to capture leading comments / PoP annotations
            s=i
            while s>0 and (lines[s-1].strip().startswith(('/*','*','//')) or
                           (lines[s-1].strip().startswith('*/') and 'PoP' not in lines[s-1]) or
                           POP.search(lines[s-1]) or lines[s-1].strip()=='' ):
                if lines[s-1].strip()=='' : 
                    # only swallow blank if next above is a comment block end
                    if s-2>=0 and lines[s-2].strip().endswith('*/'): s-=1; continue
                    break
                s-=1
            blocks.append({'name':m.group(1),'start':s,'end':j,'text':'\n'.join(lines[s:j+1])})
            i=j+1; continue
        i+=1
    return blocks

def pop_of(block_text):
    m=POP.search(block_text)
    return m.group(2) if m else None

def merge_into(canon_abs, chunk):
    cc=open(canon_abs).read()
    cdef=defs_in(cc)
    dup=[b['name'] for b in chunks_defs(chunk) if b in cdef]
    return dup

def chunks_defs(chunk):  # names defined inside a text blob (rough)
    out=[]
    for m in re.finditer(r'^[A-Za-z_][A-Za-z0-9_ \*]*?[\* ]([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{)]*\)\s*\{', chunk, re.M):
        out.append(m.group(1))
    return out

INCLUDE=re.compile(r'#include\s+"([^"]+)"')

def strip_conflicting_includes(text, canon_defset, roots):
    keep=[]
    for l in text.split('\n'):
        m=re.match(r'\s*'+INCLUDE.pattern+r'\s*$', l)
        if m:
            base=os.path.basename(m.group(1))
            hp=None
            for root in roots:
                cand=os.path.join(root,m.group(1))
                if os.path.exists(cand): hp=cand; break
            if hp:
                hd=defs_in(open(hp,errors='ignore').read())
                if hd and (hd & canon_defset):
                    continue  # conflicting include dropped
            elif base.endswith('.h'):
                pass
        keep.append(l)
    return '\n'.join(keep)

def main():
    todo={k:v for k,v in MAP.items() if os.path.exists(os.path.join(ROOT,k))}
    report={'merged':{}, 'skipped':[], 'dry':DRY}
    # group gap files per destination so we append once
    appends={}  # dst -> list of (gapfile, text_chunks)
    for gap_rel,rules in sorted(todo.items()):
        gap_abs=os.path.join(ROOT,gap_rel)
        txt=open(gap_abs,errors='ignore').read()
        if len(rules)==1 and rules[0][0] is None:
            # whole-file merge
            dst=rules[0][1]
            appends.setdefault(dst,[]).append((gap_rel,txt))
        else:
            blocks=split_functions(txt)
            # leading includes/header stay with first destination
            for b in blocks:
                py=pop_of(b['text'])
                dst=dest_for(gap_rel,py) if py else None
                if dst is None:
                    dst=rules[-1][1]
                appends.setdefault(dst,[]).append((gap_rel,b['text']))
    for dst,chunks in sorted(appends.items()):
        dst_abs=os.path.join(ROOT,dst)
        if not os.path.exists(dst_abs):
            print(f"NO CANONICAL {dst} — creating from merged content")
            parent=os.path.dirname(dst_abs)
            os.makedirs(parent,exist_ok=True)
            header=('#include "hermes_json.h"\n#include "hermes_logger.h"\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\n')
            body=''
            for g,c in chunks: body+=f"/* === merged from {os.path.basename(g)} === */\n"+c.strip('\n')+"\n\n"
            if not DRY:
                open(dst_abs,'w').write(header+body)
                subprocess.run(['git','-C',ROOT,'add',dst],check=False)
            report['merged'][dst]=report['merged'].get(dst,[])+[g for g,_ in chunks]
            continue
        cc=open(dst_abs,errors='ignore').read()
        cdef=defs_in(cc)
        roots=[SRC,os.path.join(ROOT,'include'),ROOT]
        parts=[]
        dropped=[]
        for g,c in chunks:
            names=chunks_defs(c)
            dups=[nm for nm in names if nm in cdef]
            if dups: dropped.append((g,dups))
            c2=strip_conflicting_includes(c,cdef,roots)
            if c2.strip():
                parts.append(f"/* === merged from {os.path.basename(g)} === */\n"+c2.strip('\n'))
        merged=cc.rstrip('\n')+"\n\n"+"\n\n".join(parts)+"\n"
        if not DRY:
            open(dst_abs,'w').write(merged)
        for g,_ in chunks:
            report['merged'].setdefault(dst,[]).append(g)
        if dropped:
            print(f"NOTE {dst}: duplicate names NOT dropped automatically — check: {dropped}")
        print(f"MERGED -> {dst}: {[g for g,_ in chunks]}")
    if DRY:
        print("\nDRY-RUN (use --apply)")
    print(json.dumps({k:[os.path.basename(x) for x in v] for k,v in report['merged'].items()},indent=1))

main()
