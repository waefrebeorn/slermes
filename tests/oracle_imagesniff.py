#!/usr/bin/env python3
"""Oracle for upstream Hermes gateway/platforms/base_image_sniff.py (gate imagesniff)."""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# Faithful copy of agent.image_routing._sniff_mime_from_bytes (the only
# dependency of _looks_like_image). Stubbed so the oracle never imports the
# heavy image_routing module; only the ftyp brand map matters for data[:16].
_FTYP = {
    b"avif": "image/avif", b"avis": "image/avif",
    b"heic": "image/heic", b"heix": "image/heic", b"hevc": "image/heic",
    b"hevx": "image/heic", b"mif1": "image/heic", b"msf1": "image/heic",
    b"heim": "image/heic", b"heis": "image/heic",
}
_MAGIC = [
    (((0, b"\x89PNG\r\n\x1a\n"),), "image/png"),
    (((0, b"\xff\xd8\xff"),), "image/jpeg"),
    (((0, b"GIF87a"),), "image/gif"),
    (((0, b"GIF89a"),), "image/gif"),
    (((0, b"RIFF"), (8, b"WEBP")), "image/webp"),
    (((0, b"BM"),), "image/bmp"),
]
_MAGIC += [(((4, b"ftyp"), (8, brand)), mime) for brand, mime in _FTYP.items()]
_MAGIC += [
    (((0, b"II*\x00"),), "image/tiff"),
    (((0, b"MM\x00*"),), "image/tiff"),
    (((0, b"\x00\x00\x01\x00"),), "image/x-icon"),
]

def _sniff(raw):
    if not raw: return None
    for conds, mime in _MAGIC:
        if all(raw[o:o + len(sig)] == sig for o, sig in conds): return mime
    head = raw[:512].lstrip().lower()
    return "image/svg+xml" if head.startswith((b"<?xml", b"<svg")) and b"<svg" in head else None

_ir = types.ModuleType("agent.image_routing")
_ir._sniff_mime_from_bytes = _sniff
for _n in ("agent",):
    if _n not in sys.modules:
        _p = types.ModuleType(_n); _p.__path__ = []; sys.modules[_n] = _p
sys.modules["agent.image_routing"] = _ir

SRC = "C:/Users/eman5/hermes-agent/gateway/platforms/base_image_sniff.py"
spec = importlib.util.spec_from_file_location("imagesniff_live", SRC)
live = importlib.util.module_from_spec(spec); spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    if r.get("func") != "looks_like_image": continue
    py = live._looks_like_image(bytes.fromhex(r.get("data_hex", "")))
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE imagesniff: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
