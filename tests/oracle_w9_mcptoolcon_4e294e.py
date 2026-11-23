#!/usr/bin/env python3
"""Differential oracle for tools/mcp_tool_content.py (gate w9_mcptoolcon_4e294e)."""
import importlib.util, json, sys, types
sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m

tools = _mod("tools"); tools.__path__ = [Q + "/tools"]
_mod("tools.ansi_strip", strip_unicode_tags=lambda s: s)
_mod("tools.mcp_tool_common", exact_mcp_identifier=lambda s, w: s, mcp_field=lambda b, *a: None)
_mod("tools.mcp_tool_schema", mcp_prefixed_tool_name=lambda s, n: n)
_mod("tools.tool_output_truncate", truncate_head_tail=lambda *a, **k: (a[0] if a else ""))

NAME = "w9_mcptoolcon_4e294e_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/mcp_tool_content.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    if f == "is_reserved_meta_key":
        return live._is_reserved_mcp_meta_key(r.get("key", ""))
    if f == "strip_reserved_meta":
        return live._strip_reserved_meta_keys(r.get("meta"))
    if f == "base_mime":
        return live._base_mime(r.get("mime"))
    if f in ("image_extension", "resource_filename"):
        old = live.mimetypes.guess_extension
        ext = r.get("ext", "")
        live.mimetypes.guess_extension = lambda m: ext or None
        try:
            if f == "image_extension":
                return live._mcp_image_extension_for_mime_type(r.get("mime", ""))
            return live._mcp_resource_filename(r.get("uri", ""), r.get("mime", ""))
        finally:
            live.mimetypes.guess_extension = old
    return None

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        print(f"ORACLE w9_mcptoolcon_4e294e: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w9_mcptoolcon_4e294e {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_mcptoolcon_4e294e: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
