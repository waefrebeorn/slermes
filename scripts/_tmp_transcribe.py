import re
from pathlib import Path
t = Path('lib/libtranscribe/transcribe.c').read_text(errors='ignore')
idents = set(re.findall(r'\b([a-z_][a-z0-9_]*)\b', t))
CAND = ['hermes_core_types','hermes_json','hermes_http','hermes_crypto','hermes_agent','hermes_memory','hermes_cli','hermes_skills','hermes_cron','hermes_plugin','registry','hermes_config','hermes_tool_helpers','hermes_media_cache','hermes_credits_tracker','hermes_tirith','hermes_display','hermes_skin']
for h in CAND:
    p = Path(f'include/{h}.h')
    if not p.exists():
        continue
    syms = set(re.findall(r'\b([a-z_][a-z0-9_]*)\b', p.read_text(errors='ignore')))
    used = syms & idents
    if used:
        print(f"{h}.h: {len(used)} syms -> {sorted(used)[:12]}")
