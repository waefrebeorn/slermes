#!/usr/bin/env python3
"""Oracle for upstream Hermes pm/publication.py (w5_publication_1af7eb gate).

Compares _metadata_records against the live module. The heavy pm.* imports are
stubbed with plain namespaces so the module loads without touching the
filesystem; _metadata_records itself uses only json and is fully exercised.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


p = "C:/x"
stub("pm.environments",
     dependency_home_root=lambda: __import__("pathlib").Path(p),
     install_state_dir=lambda proj: __import__("pathlib").Path(p),
     runtime_facts_path=lambda proj: __import__("pathlib").Path(p))
stub("pm.filesystem",
     durable_write_bytes=lambda *a, **k: None,
     file_digest=lambda path: None,
     inherit_parent_acl=lambda path: False,
     read_bytes_or_none=lambda path: None)
stub("pm.workspace",
     enabled_plugin_dirs=lambda *a, **k: [],
     _is_member_candidate=lambda path: True)

spec = importlib.util.spec_from_file_location("w5_publication_live", Q + "/pm/publication.py")
live = importlib.util.module_from_spec(spec)
sys.modules["w5_publication_live"] = live
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
    if func != "metadata_records":
        continue
    raw = r.get("data")
    data = None if raw is None else raw.encode("utf-8")
    try:
        py = {"result": live._metadata_records(data)}
    except Exception as exc:
        py = {"error": str(exc)}
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w5_publication_1af7eb: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
