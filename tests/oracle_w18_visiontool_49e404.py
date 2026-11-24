#!/usr/bin/env python3
"""Oracle for gate w18_visiontool_49e404 (tools/vision_tools_image_prep.py)."""
import importlib.util
import json
import sys
import types
import contextlib
import io

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

QUARRY = "C:/Users/eman5/hermes-agent"
sys.path.insert(0, QUARRY)


def _load(dotted, rel):
    parts = dotted.split(".")
    for i in range(1, len(parts)):
        pn = ".".join(parts[:i])
        if pn not in sys.modules:
            pkg = types.ModuleType(pn)
            pkg.__path__ = [QUARRY + "/" + "/".join(parts[:i])]
            sys.modules[pn] = pkg
    spec = importlib.util.spec_from_file_location(dotted, QUARRY + "/" + rel)
    m = importlib.util.module_from_spec(spec)
    sys.modules[dotted] = m
    with contextlib.redirect_stderr(io.StringIO()):
        spec.loader.exec_module(m)
    return m


live = _load("tools.vision_tools_image_prep", "tools/vision_tools_image_prep.py")


def install_pil(png_ok):
    class _FakeImg:
        def __enter__(self):
            return self

        def __exit__(self, *a):
            return False

        def verify(self):
            if not png_ok:
                raise ValueError("corrupt png")

    pil = types.ModuleType("PIL")
    pil.Image = types.SimpleNamespace(open=lambda b: _FakeImg())
    sys.modules["PIL"] = pil


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
        if func == "unsupported_inline_image_media_type":
            py = live.unsupported_inline_image_media_type(r.get("url", ""))
        elif func == "detect_image_mime_type_from_bytes":
            install_pil(r.get("png_verify_ok", True))
            py = live._detect_image_mime_type_from_bytes(bytes.fromhex(r.get("data_hex", "")))
        else:
            continue
    except Exception as exc:
        print(f"SKIP {func}: {exc}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} in={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w18_visiontool_49e404: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
