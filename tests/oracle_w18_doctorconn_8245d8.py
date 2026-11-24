#!/usr/bin/env python3
"""Differential oracle for hermes_cli/doctor_connectivity.py (gate w18_doctorconn_8245d8)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _seed(name, path=None, **attrs):
    m = types.ModuleType(name)
    if path is not None:
        m.__path__ = [path]
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = ".".join(parts[:i])
        if parent in sys.modules and not hasattr(sys.modules[parent], parts[i]):
            setattr(sys.modules[parent], parts[i], m)
    return m


_seed("hermes_cli", Q + "/hermes_cli")
_seed("pm", install_hint=lambda s: s)
_seed("hermes_cli.colors",
      Colors=types.SimpleNamespace(GREEN="G", YELLOW="Y", RED="R", DIM="D"),
      color=lambda s, c: s)
_seed("hermes_cli.version_info", hermes_user_agent=lambda: "ua")
_seed("hermes_constants", OPENROUTER_MODELS_URL="https://openrouter.ai/api/v1/models")
_seed("utils", base_url_host_matches=lambda *a, **k: False,
      normalize_proxy_env_vars=lambda: None)

spec = importlib.util.spec_from_file_location(
    "w18_doctorconn_live", Q + "/hermes_cli/doctor_connectivity.py")
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)


def _pr(res):
    return {"label": res.label,
            "lines": [list(x) for x in res.lines],
            "issues": list(res.issues)}


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
    try:
        if func == "row":
            py = _pr(live._row(r.get("name") or "", r.get("status") or "ok",
                               r.get("detail") or "", label=r.get("label")))
        elif func == "openrouter_status":
            code = int(r.get("status_code", 0))
            detail, issue = live._OPENROUTER_STATUS.get(code, (f"(HTTP {code})", None))
            py = [detail, issue]
        elif func == "probe_openrouter":
            key = r.get("key") or ""
            err = r.get("error") or ""
            code = int(r.get("status_code", 0))
            live.os = types.SimpleNamespace(
                getenv=lambda n, d="": key if n == "OPENROUTER_API_KEY" else d)

            class Resp:
                status_code = code

            def fake_get(url, **kw):
                if err:
                    raise Exception(err)
                return Resp()
            sys.modules["httpx"] = types.SimpleNamespace(get=fake_get)
            py = _pr(live._probe_openrouter())
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_doctorconn_8245d8: {cases} cases, {mism} mismatches")
sys.exit(1 if mism or cases == 0 else 0)
