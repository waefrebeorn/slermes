#!/usr/bin/env python3
"""Oracle for upstream Hermes utils/urls.py (gate w5_urls_e1571a)."""
import importlib.util, json, sys
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path: sys.path.insert(0, Q)

# utils/urls.py is dependency-free; load it under a flat synthetic name.
spec = importlib.util.spec_from_file_location("live_urls_w5", Q + "/utils/urls.py")
live = importlib.util.module_from_spec(spec)
sys.modules["live_urls_w5"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    url = r.get("url", "")
    if func == "hostname":
        py = live.base_url_hostname(url)
    elif func == "path":
        py = live.base_url_path(url)
    elif func == "origin":
        py = list(live.base_url_origin(url))
    elif func == "same":
        py = live.same_base_url_origin(url, r.get("b", ""))
    elif func == "match":
        py = live.base_url_host_matches(url, r.get("domain", ""))
    else:
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} url={url!r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w5_urls_e1571a: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
