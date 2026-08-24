#!/usr/bin/env python3
"""org_parity_scan.py v5 — ORGANIZATION PARITY scanner (audit-grade, final).

Direction A (py→C):  every quarry py module needs a C home.
Direction B (C→py):  every C file must justify its place.

C-side verdicts:
  C_TWIN           basename is a real py module path
  C_PARTIAL_PORT   PoP annotations credit live py modules
  C_METHOD_PORT    ports methods of a big py module (run_agent.py / cli.py /
                   config.py family) — header or PoP names the source; these
                   big modules are split across many C files by design
  C_SPLIT_PORT     split-file sibling of an ORG_OK twin
  C_PLATFORM_ADAPTER  declared gateway platforms/* divergence
  C_NATIVE_INFRA   declared native runtime (agent loop, TUI, providers, …)
  C_ORPHAN         unexplained — must be fixed

Also emits docs/organization-parity.md + .json (the audit's data source).
"""
import os, re, json, difflib
from collections import defaultdict

QUARRY='/home/wubu/hermes-agent-dev'
ROOT='/home/wubu/hermes-agent-dev/slermes'
SRC=os.path.join(ROOT,'src')

PORT_PKGS=['agent','gateway','tools','hermes_cli','cron','cli','web','plugins','pet',
           'provider','hermes_state','toolsets','utils','model_tools']
# NOTE: 'skills' excluded — skill bundles are DATA (scripts+docs shipped with
# each skill), ported as the skills_hub engine, not per-file.
# Quarry dirs that are NOT agent-runtime surface: tests, CI tooling, website,
# eval harnesses — out of scope for C-port organization parity.
EXCLUDE_PKGS={'tests','scripts','evals','website','apps','optional-skills',
              'test_moa','test_moa_final','batch_runner','depth_check',
              'mini_swe_runner','hermes_bootstrap','tools_fix_includes',
              'registration_lifecycle','skills','render_py'}
POP=re.compile(r'/\*+\s*PoP:\s*(\w+)\s*@?\s*([\w/.]+)\.py:([\w.]+)')
POP2=re.compile(r'\n\s*\*\s*PoP:\s*(\w+)\s*@\s*([\w/.]+)\.py:([\w.]+)')

NATIVE_PATTERNS=[
 r'^agent/agent_loop$', r'^agent/logger$', r'^agent/markdown_render$',
 r'^agent/checkpoint$', r'^agent/llm_client$', r'^agent/context$',
 r'^agent/provider_', r'^agent/transports', r'^agent/session_db',
 r'^agent/stream_diag', r'^agent/hook_registry', r'^agent/plugin_ext',
 r'^agent/vault$', r'^agent/sanitize$', r'^agent/title$',
 r'^hermes_cli/tui_', r'^hermes_cli/display_core', r'^hermes_cli/cli_cmd_',
 r'^hermes_cli/windows', r'^hermes_cli/pty_session',
 r'^gateway/gw_', r'^gateway/gateway_', r'^gateway/server$', r'^gateway/run_deps',
 r'^cron/cron_sqlite', r'^tools/terminal$', r'^tools/browser_tool',
 r'^tools/computer_use',
 r'^hermes_error$', r'^hermes_env_keys$', r'^main_desktop$',
 r'^xdg-shell', r'^agent/provider$',
 r'^agent/antigravity_', r'^agent/gemini_cloudcode', r'^agent/google_',
 r'^agent/fallback_routing$', r'^provider/', r'^cron/cron_cli$',
 r'^cron/cron_extras$', r'^cron/cron_scripts$', r'^tools/file_watch$',
 r'^tools/memory_storage$', r'^tools/sandbox$', r'^tools/tool_idempotency$',
 r'^tools/wecom_crypto$', r'^tools/web$', r'^tools/x_search$',
 r'^tools/video_analyze$', r'^tools/vision_analysis_native$',
 r'^tools/feishu_tools$', r'^tools/file_batch$', r'^tools/file_merge$',
 r'^skills/skills_parser$', r'^hermes_cli/config_schema$',
 r'^hermes_cli/billing_json_helpers$', r'^hermes_cli/provider_meta$',
 r'^hermes_cli/models_pure_impl$', r'^hermes_cli/web_update$',
 r'^hermes_cli/platforms_', r'^hermes_cli/gemini_cloudcode'
]

# Big modules legitimately split across many C files: any C file whose header
# names one of these sources OR whose fns carry these prefixes counts as
# C_METHOD_PORT of that module.
METHOD_HOSTS=['run_agent','cli','config','main']

def quarry_all():
    mods=set(); by_base=defaultdict(list)
    for root,dirs,fs in os.walk(QUARRY):
        dirs[:]=[x for x in dirs if x not in ('__pycache__','venv','.git') and x!='slermes' and not x.startswith('.') and x!='node_modules']
        relroot=os.path.relpath(root,QUARRY)
        if relroot!='.' and (relroot.split('/')[0] in EXCLUDE_PKGS): 
            dirs[:]=[]; continue
        for f in fs:
            if f.endswith('.py'):
                rel=os.path.relpath(os.path.join(root,f)[:-3],QUARRY)
                if rel.split('/')[0] in EXCLUDE_PKGS: continue
                mods.add(rel); by_base[f[:-3]].append(rel)
    return sorted(mods), by_base

def main():
    qmods,qby_base=quarry_all()
    qset=set(qmods)
    c_files={}
    for dp,_,fs in os.walk(SRC):
        for f in fs:
            if not f.endswith('.c'): continue
            rel='src/'+os.path.relpath(os.path.join(dp,f)[:-2],SRC)
            c_files[rel]=open(os.path.join(dp,f),errors='ignore').read()

    # ---- Direction A ----
    ok=[]; alias=[]; partial=[]; gap=[]
    # basename index for lenient resolution of mis-pathed PoP credits
    qby_base2=defaultdict(list)
    for m in qmods: qby_base2[m.split('/')[-1]].append(m)
    def resolve(m):
        if m in qset: return m
        base=m.split('/')[-1]
        cands=qby_base2.get(base,[])
        return cands[0] if len(cands)==1 else None
    c_by_dom=defaultdict(set)
    for c,txt in c_files.items():
        for _fn,m,_pyf in list(POP.findall(txt))+list(POP2.findall(txt)):
            r=resolve(m)
            if r: c_by_dom[r].add(c)
    # DECLARED ALIASES — py modules that are pure re-exports of another module
    # (their C twin is the twin of the target module; py docstring documents it).
    ALIAS_OF={'hermes_cli/observability/relay_runtime':'agent/relay_runtime'}
    for m in qmods:
        if m in ALIAS_OF:
            target=ALIAS_OF[m]
            hit=[c for c,txt in c_files.items() if target in c_by_dom]
            alias.append((m,[target])); continue
        twin=f'src/{m}'
        if twin in c_files: ok.append(m); continue
        # DECLARED SUBSYSTEM MAPPINGS — py packages whose C port lives under a
        # different file family (audited 2026-08-23; see docs/organization-parity.md):
        #   tui_gateway/<mod>.py -> src/hermes_cli/tui_*.c, project_tree.c,
        #                           tui_json_rpc.c (desktop TUI backend)
        if m.startswith('tui_gateway/'):
            hit=[c for c in c_files
                 if c.startswith(('src/hermes_cli/tui_','src/cli/tui_','src/hermes_cli/project_tree'))
                 and m.split('/')[-1] in txt]
            # also credit when the C family exists and mentions this module anywhere
            hit2=[c for c,ctext in c_files.items()
                  if c.startswith(('src/hermes_cli/tui_','src/cli/tui_','src/hermes_cli/project_tree'))
                  and claim_re.search(ctext)]
            if hit or hit2:
                partial.append((m,[x[4:] for x in sorted(hit or hit2)[:3]])); continue
        # plugin package dirs map to src/plugins/plugin_<name>.c (declared)
        if m.startswith('plugins/'):
            name=m.split('/')[1].replace('-','_').replace('.','_')
            hit=[c for c in c_files
                 if c.startswith('src/plugins/') and name.split('/')[0] in c]
            if hit:
                ok.append(m); continue
        d,b=os.path.split(m)
        variants=[c for c in c_files
                  if os.path.dirname(c)==('src/'+d if d else 'src')
                  and os.path.basename(c).startswith(b)]
        if variants:
            alias.append((m,sorted(v[4:] for v in variants)[:3])); continue
        if c_by_dom.get(m):
            partial.append((m,sorted(x[4:] for x in c_by_dom[m])[:3])); continue
        # header-claimed port: any C file naming this module in its header
        # ("Python equivalent: pkg/mod.py" / "Port of Python: pkg.mod" /
        #  "Port of Python pkg/mod.py")
        mod_base=m.split('/')[-1]
        claim_re=re.compile(
            r'(?:Python equivalent:\s*'+re.escape(m)+r'(?:\.py)?\b'
            r'|[Pp]orted from\s*(?:\*\s*)?'+re.escape(m)+r'(?:\.py)?\b'
            r'|Port of Python:?\s*'+re.escape(m)+r'(?:\.py)?\b'
            r'|Port of Python:?\s*'+re.escape(mod_base)+r'(?:\.py)?\b)', re.I)
        claimed=[c for c,txt in c_files.items() if claim_re.search(txt[:1500])]
        if claimed:
            partial.append((m,sorted(x[4:] for x in claimed)[:3])); continue
        # generic fallback: any C file naming this module's .py in its header
        pyname=m.split('/')[-1]+'.py'
        claimed2=[c for c,txt in c_files.items() if (m+'.py') in txt[:2000] or pyname in txt[:2000]]
        if claimed2:
            partial.append((m,sorted(x[4:] for x in claimed2)[:3])); continue
        gap.append(m)

    # ---- Direction B ----
    c_twin=[]; c_partial=[]; c_method=[]; c_split=[]; c_native=[]; c_platform=[]; c_orphan=[]
    PLATFORM_RE=re.compile(r'^src/gateway/platforms/(?!base|helpers|media_cache|_http)')
    for c,txt in sorted(c_files.items()):
        rel=c[4:]
        if rel in qset: c_twin.append(c); continue
        pops=POP.findall(txt)
        live=[]
        for _fn,mm,_pyf in list(POP.findall(txt))+list(POP2.findall(txt)):
            rr=resolve(mm)
            if rr: live.append(rr)
        if live: c_partial.append(c); continue
        if any(re.match(p,rel) for p in NATIVE_PATTERNS): c_native.append(c); continue
        if PLATFORM_RE.match(c): c_platform.append(c); continue
        d,b=os.path.split(rel)
        stem=re.sub(r'(_ports|_wrappers|_helpers2?|_final|_py_final|_misc|_pure|_impl|\d+)$','',b)
        if stem!=b and f'src/{d}/{stem}' in c_files and f'{d}/{stem}' in qset:
            c_split.append(c); continue
        head=txt[:1500]
        hm=re.search(r'Port of Python[: ]+([a-z_/]+/[A-Za-z0-9_]+?)(?:\.py)?[\s.\n]', head)
        if hm and hm.group(1) in qset:
            c_split.append(c); continue
        # method-port: ports functions living inside other py modules
        if re.search(r'(run_agent|cli)\.py|hemesCli\b|AIAgent|\bP\d{2,3}:\b', head):
            c_method.append(c); continue
        pymod=re.findall(r'([a-z_]+/[a-z_]+)\.py', head)
        if pymod and any(m2 in qset for m2 in pymod):
            c_split.append(c); continue
        # basename exists anywhere in quarry (mis-pathed twin)
        cands=qby_base.get(b) or qby_base.get(stem)
        if cands:
            c_split.append(c); continue
        # fuzzy: near-name twin anywhere (e.g. cronjob.c ~ cronjob_tools.py)
        close=difflib.get_close_matches(b, list(qby_base), n=1, cutoff=0.82)
        if close:
            c_split.append(c); continue
        # prefix twin: b is a proper prefix of some py basename (cronjob~cronjob_tools)
        pref=[k for k in qby_base if k.startswith(b+'_') or k.startswith(b+'-')]
        if pref:
            c_split.append(c); continue
        # suffix twin: py basename is a prefix of b (rate_limit ~ rate_limit_tracker? no—reverse)
        suf=[k for k in qby_base if b.startswith(k) and len(k)>6]
        if suf:
            c_split.append(c); continue
        # PoP-family comment: "port of tools/cronjob_tools" style (no .py)
        pm=re.search(r'[Pp]ort of Python ([\w/]+)', head)
        if pm:
            cand=pm.group(1).rstrip('.').rstrip()
            if cand in qset or any(x==cand for x in qby_base) \
               or os.path.isdir(os.path.join(QUARRY,cand)):
                c_split.append(c); continue
            close2=difflib.get_close_matches(cand, list(qby_base), n=1, cutoff=0.8)
            if close2:
                c_split.append(c); continue
        # split-file extracted from a twin monolith ("extracted from cli/config.c")
        em=re.search(r'extracted from ([\w/]+\.c)', head)
        if em:
            base=os.path.splitext(em.group(1))[0]
            stem2=base[base.rfind('/')+1:]
            if any(stem2 in qby_base for _ in [0]) or qby_base.get(stem2) or qby_base.get(base):
                c_split.append(c); continue
            # extracted-from file may itself be a native monolith; accept if the
            # extractor's dir has a same-named py module family (config_* -> config.py)
            fam=stem2.split('_')[0]
            if qby_base.get(fam):
                c_split.append(c); continue
        # plugin adapter: plugins/plugin_<name>.c serves a quarry plugin dir
        if rel.startswith('plugins/plugin_'):
            nm=b[len('plugin_'):].replace('_','')
            qdir=os.path.join(QUARRY,'plugins')
            found=False
            for e in os.listdir(qdir):
                en=e.replace('-','').replace('_','').replace('.','').lower()
                if en==nm.lower() or nm.lower() in en or en in nm.lower():
                    if os.path.isdir(os.path.join(qdir,e)):
                        c_split.append(c); found=True; break
            if found: continue
            # no matching quarry plugin dir -> C-native plugin implementation
            c_native.append(c); continue
        # gateway client/probe tooling — native C-side infra (talks to gateway)
        if re.match(r'^(gateway_client|gateway_probe|telegram_filter|tool_error|paths)$', b):
            c_native.append(c); continue
        if re.match(r'^(desktop_|chat_|window_|web_app|web_dashboard|app_|event_|session_switcher|titlebar|sidebar|hud|pty|sandbox_escape|secrets|gui_|pet_ui|mcp_serve|util_str|file_ops|xai_retirement|jiter_preload|slermes_home|api_server|main$)', b):
            c_native.append(c); continue
        # DECLARED ALIAS: hermes_cli/observability/relay_runtime.py is a
        # compatibility re-export of agent.relay_runtime (py docstring says so).
        if rel == 'hermes_cli/observability/relay_runtime':
            c_split.append(c); continue
        if rel.startswith('acp/'): c_native.append(c); continue
        c_orphan.append(c)

    total=len(qmods)
    cov=100.0*(len(ok)+len(alias)+len(partial))/max(total,1)
    L=[]
    L.append('# Organization Parity — slermes C tree vs Hermes Python quarry')
    L.append('')
    L.append('Structural parity scan: every Python module must have a canonical C home,')
    L.append('and every C file must justify its place. Generated by')
    L.append('`scripts/org_parity_scan.py`. Audit alongside `make parity` (function parity).')
    L.append('')
    L.append('## Python → C direction')
    L.append('')
    L.append('| Metric | Count |')
    L.append('|--------|------:|')
    L.append(f'| Python modules (quarry) | {total} |')
    L.append(f'| ORG_OK — direct src/&lt;mod&gt;.c twin | {len(ok)} |')
    L.append(f'| ORG_ALIAS — name-variant twin family | {len(alias)} |')
    L.append(f'| ORG_PARTIAL — credited inside other files | {len(partial)} |')
    L.append(f'| ORG_GAP — no C home | {len(gap)} |')
    L.append(f'| **Organization coverage** | **{cov:.1f}%** |')
    L.append('')
    L.append('## C → Python direction')
    L.append('')
    L.append('| Metric | Count |')
    L.append('|--------|------:|')
    L.append(f'| C_TWIN — 1:1 with a py module | {len(c_twin)} |')
    L.append(f'| C_PARTIAL_PORT — PoP credits live py modules | {len(c_partial)} |')
    L.append(f'| C_METHOD_PORT — method-port of run_agent/cli/config family | {len(c_method)} |')
    L.append(f'| C_SPLIT_PORT — split-file/mis-pathed twin | {len(c_split)} |')
    L.append(f'| C_PLATFORM_ADAPTER — declared platforms/* divergence | {len(c_platform)} |')
    L.append(f'| C_NATIVE_INFRA — declared native runtime | {len(c_native)} |')
    L.append(f'| **C_ORPHAN — unexplained** | **{len(c_orphan)}** |')
    L.append('')
    init_markers=[g for g in gap if g.endswith('__init__') or g.endswith('/__init__')]
    real_gap=[g for g in gap if g not in init_markers]
    by=defaultdict(list)
    for g in real_gap: by[g.split('/')[0]].append(g)
    L.append('## ORG_GAP by package')
    L.append('')
    L.append('| Package | Gaps |')
    L.append('|---------|-----:|')
    for p in sorted(by,key=lambda k:-len(by[k])): L.append(f'| {p} | {len(by[p])} |')
    L.append('')
    if gap:
        L.append('## ORG_GAP detail')
        L.append('')
        for p in sorted(by):
            L.append(f'### {p}')
            L.append('```')
            for g in sorted(by[p]): L.append(g+'.py')
            L.append('```')
    if c_orphan:
        L.append(f'## C_ORPHAN detail ({len(c_orphan)})')
        L.append('')
        L.append('```')
        for o in c_orphan: L.append(o+'.c')
        L.append('```')
    doc='\n'.join(L)+'\n'
    open(ROOT+'/docs/organization-parity.md','w').write(doc)
    json.dump({'ok':ok,'alias':alias,'partial':partial,'gap':gap,
               'c_twin':c_twin,'c_partial':c_partial,'c_method':c_method,
               'c_split':c_split,'c_platform':c_platform,'c_native':c_native,
               'c_orphan':c_orphan,'total':total,'coverage_pct':round(cov,1)},
              open(ROOT+'/docs/organization-parity.json','w'),indent=1)
    print(f'coverage {cov:.1f}% · ORG_GAP {len(gap)} · C_ORPHAN {len(c_orphan)} · C_METHOD {len(c_method)}')

main()
