#!/usr/bin/env python3
"""Oracle for pm/download_state.py (w6_downloadsta_5c6fdc).
Reconstructs the fixture's partial files in a temp dir, runs the LIVE
collect_partials() against them, and reports the surviving partial stems.
"""
import importlib.util, json, sys, os, pathlib, tempfile, time, shutil
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = r"C:\Users\eman5\hermes-agent"
sys.path.insert(0, QUARRY)

def load(name, rel):
    parent = name.rsplit(".", 1)[0]
    if parent not in sys.modules:
        __import__(parent)
    spec = importlib.util.spec_from_file_location(name, os.path.join(QUARRY, *rel))
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m

live = load("pm.download_state", ["pm", "download_state.py"])

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func != "collect":
            continue
        d = pathlib.Path(tempfile.mkdtemp())
        try:
            for f in r["files"]:
                p = d / f["name"]
                p.write_text("x")
                age = f.get("age")
                if age is not None:
                    t = time.time() - float(age)
                    os.utime(p, (t, t))
            live.collect_partials(d, grace_seconds=r["grace"])
            py = sorted({pathlib.Path(p).stem for p in d.iterdir()
                         if pathlib.Path(p).suffix in {".part", ".ranges"}})
        finally:
            shutil.rmtree(d, ignore_errors=True)
    except Exception as e:
        mism += 1; cases += 1
        print(f"ORACLE w6_downloadsta_5c6fdc ERROR case={r.get('case')} {e!r}", file=sys.stderr)
        continue
    got = r.get("out"); cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH w6_downloadsta_5c6fdc case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_downloadsta_5c6fdc: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
