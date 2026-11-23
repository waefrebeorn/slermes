#!/usr/bin/env python3
"""Oracle for hermes_cli/webapp.py (w6_webapp_879137)."""
import importlib.util, json, sys, os, pathlib, tempfile
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

live = load("hermes_cli.webapp", ["hermes_cli", "webapp.py"])

def norm(p):
    return str(p).replace("\\", "/")

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "dist_dir":
            env = r.get("dist_env")
            if env is None: os.environ.pop(live.DIST_ENV, None)
            else: os.environ[live.DIST_ENV] = env
            py = norm(live.webapp_dist_dir(pathlib.Path(r["project_root"])))
        elif func == "mark_stale":
            if r.get("packaged"):
                pkg = pathlib.Path(tempfile.mkdtemp()); (pkg / "index.html").write_text("x")
                os.environ[live.DIST_ENV] = str(pkg)
            else:
                os.environ.pop(live.DIST_ENV, None)
            root = pathlib.Path(tempfile.mkdtemp())
            dist = root / "apps" / "desktop" / "dist-webapp"
            if r.get("index_exists"):
                dist.mkdir(parents=True, exist_ok=True); (dist / "index.html").write_text("x")
            py = bool(live.mark_webapp_stale(root))
        elif func == "bundle_stale":
            d = pathlib.Path(tempfile.mkdtemp())
            if r.get("marker_exists"): (d / live.STALE_MARKER).write_text("x")
            py = bool(live.webapp_bundle_stale(d))
        else:
            continue
    except Exception as e:
        mism += 1; cases += 1
        print(f"ORACLE w6_webapp_879137 ERROR case={r.get('case')} {e!r}", file=sys.stderr)
        continue
    got = r.get("out"); cases += 1
    if py != got:
        mism += 1
        print(f"MISMATCH w6_webapp_879137 case={r.get('case')} func={func} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w6_webapp_879137: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
