#!/usr/bin/env python3
"""Oracle for the UNCLAIMED-remainder FLOW port of
agent/pet/generate/atlas.py (src/agent/pet/generate/pet_atlas_flow2.c, pa2_*).

A CRLF-aware PoP grep of the 42 gaps found 33 claimed (src/pet/atlas.c +
src/agent/pet/generate/atlas.c); the 16 defs below are UNCLAIMED. Every PIL
operation and claimed sibling is a SCRIPTED seam fed the SAME io scripts the C
harness feeds pa2_call; the oracle drives the LIVE defs (AST-extracted from the
READ-ONLY quarry) under those scripts and diffs the ordered trace, the canonical
return and the state projection. Fail-closed: 0 cases => exit 1; a BaseException
per case rides as {"__raise__": cls}.

Representation contract (both sides, see the C port header):
- None -> null; image/channel handles are opaque JSON objects whose scalar
  fields (size, width, height, handle, bbox) are read directly on BOTH sides
  (free reads, NOT trace events). An ImgDouble projects to its whole dict.
- scripts: absent key -> None; {"$seq":[...]} per-call queue (exhaustion raises
  RuntimeError("script exhausted: <name>")); {"__raise__":cls,"msg":m} raises
  (trace ret is the WHOLE marker); any other value is reused. None-valued seam
  arg keys are dropped on BOTH sides ("" and 0 are kept).
- FULL builtin exception hierarchy: except Exception never catches the trio
  SystemExit/KeyboardInterrupt/GeneratorExit; builtins carry the real tree.
- Python round() == banker's rounding (C uses nearbyint()).
Summary: 'ORACLE pet_atlas_flow2: N cases, M mismatches'.
"""
import __future__
import ast
import builtins
import json
import sys
from collections import deque
from itertools import groupby
from pathlib import Path

try:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    if hasattr(sys.stdin, "reconfigure"):
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
except (ValueError, OSError):
    pass

_HERE = Path(__file__).resolve()
QUARRY = _HERE.parents[2] / "hermes-agent"
if not (QUARRY / "agent" / "pet" / "generate" / "atlas.py").is_file():
    QUARRY = Path.home() / "hermes-agent"
SRC = str(QUARRY / "agent" / "pet" / "generate" / "atlas.py")

NAME = "pet_atlas_flow2"
PORTED = [
    "_median", "_median_box_size", "_blank", "_place", "_clear_region",
    "_load_rgba", "_flood", "_border_flood", "_unvisited_components",
    "_remove_masked", "_thin_groups", "_has_margin",
    "_is_multi_pose_outlier", "_clean_slot", "_salvage_frames",
    "_check_atlas_cells",
]
WANTED = set(PORTED)

# ── AST extraction (live defs) ─────────────────────────────────────────────
_SRC_TEXT = Path(SRC).read_text(encoding="utf-8")
_TREE = ast.parse(_SRC_TEXT)
_FLAGS = __future__.annotations.compiler_flag


def _compile_node(node, src):
    return compile(ast.Module(body=[node], type_ignores=[]), src, "exec",
                   flags=_FLAGS, dont_inherit=True)


DEF_CODE = {}
for _node in _TREE.body:
    if isinstance(_node, (ast.FunctionDef, ast.AsyncFunctionDef)) and _node.name in WANTED:
        DEF_CODE[_node.name] = _compile_node(_node, SRC)
_missing = WANTED - set(DEF_CODE)
if _missing:
    print(f"ORACLE {NAME}: FATAL missing live defs: {sorted(_missing)}", file=sys.stderr)
    sys.exit(1)

# ── canonical projection ───────────────────────────────────────────────────
class ImgDouble:
    """Opaque PIL image/channel double. Scalar fields are free attribute reads;
    methods are scripted seams. norm() projects it to its whole backing dict."""
    def __init__(self, d):
        self._d = d if isinstance(d, dict) else {}
        for k, v in self._d.items():
            if not k.startswith("_") and k not in _METHOD_NAMES:
                setattr(self, k, v)

    def convert(self, mode):
        return record("convert", {"image": self, "mode": mode})
    def getchannel(self, ch):
        return record("getchannel", {"image": self, "channel": ch})
    def getbbox(self):
        return record("getbbox", {"image": self})
    def crop(self, box):
        return record("crop", {"image": self, "box": list(box)})
    def resize(self, size, resample=None):
        a = {"image": self, "size": list(size)}
        if resample is not None:
            a["resample"] = "NEAREST" if getattr(resample, "_pa2", "NEAREST") == "NEAREST" else "BILINEAR"
        return record("resize", a)
    def alpha_composite(self, sprite, offset=(0, 0)):
        return record("alpha_composite", {"canvas": self, "sprite": sprite, "offset": list(offset)})
    def paste(self, src, box):
        return record("paste", {"image": self, "src": src, "box": list(box)})
    def putalpha(self, alpha):
        return record("putalpha", {"image": self, "alpha": alpha})
    def getdata(self):
        return record("getdata", {"image": self})
    def tobytes(self):
        return record("tobytes", {"image": self})
    def getextrema(self):
        return record("getextrema", {"image": self})
    def histogram(self):
        return record("histogram", {"image": self})
    def getpixel(self, xy):
        return record("getpixel", {"image": self, "xy": list(xy)})
    def split(self):
        return record("split", {"image": self})
    def point(self, fn):
        return record("point", {"image": self})
    def filter(self, f):
        return record("filter", {"image": self, "filter": getattr(f, "_pa2", str(f))})
    def transpose(self, op):
        return record("transpose", {"image": self, "op": getattr(op, "_pa2", str(op))})


_METHOD_NAMES = frozenset(
    n for n in dir(ImgDouble) if not n.startswith("__")
)


class OpenDouble:
    """Image.open(...) -> context manager; __enter__ yields the opened double."""
    def __init__(self, d):
        self._d = d if isinstance(d, dict) else {}
    def __enter__(self):
        return ImgDouble(self._d)
    def __exit__(self, *exc):
        return False


def norm(v):
    if isinstance(v, ImgDouble):
        return norm(v._d)
    if isinstance(v, OpenDouble):
        return norm(v._d)
    if isinstance(v, BaseException):
        return {"__raise__": type(v).__name__, "msg": str(v)}
    if isinstance(v, (list, tuple)):
        return [norm(x) for x in v]
    if isinstance(v, (set, frozenset)):
        return sorted((norm(x) for x in v), key=lambda s: str(s))
    if isinstance(v, dict):
        return {str(k): norm(x) for k, x in v.items()}
    if isinstance(v, float) and v.is_integer():
        return int(v)
    return v


def make_exc(marker):
    cls_name = marker.get("__raise__", "RuntimeError")
    msg = marker.get("msg", "")
    cls = getattr(builtins, cls_name, None)
    if not (isinstance(cls, type) and issubclass(cls, BaseException)):
        cls = type(cls_name, (Exception,), {})
    return cls(msg)


# ── scripted seams ─────────────────────────────────────────────────────────
_SCRIPTS = None


class Scripts:
    def __init__(self, io, trace):
        self.io = io or {}
        self.trace = trace
        self.cursors = {}

    def next(self, name):
        if name not in self.io:
            return None
        script = self.io[name]
        if isinstance(script, dict) and "$seq" in script:
            seq = script["$seq"]
            i = self.cursors.get(name, 0)
            if i >= len(seq):
                return {"__raise__": "RuntimeError", "msg": f"script exhausted: {name}"}
            self.cursors[name] = i + 1
            return seq[i]
        return script


_MATERIALIZE = {
    "Image.new": lambda s: ImgDouble(s),
    "Image.open": lambda s: OpenDouble(s),
    "composite": lambda s: ImgDouble(s),
    "convert": lambda s: ImgDouble(s),
    "getchannel": lambda s: ImgDouble(s),
    "filter": lambda s: ImgDouble(s),
    "crop": lambda s: ImgDouble(s),
    "resize": lambda s: ImgDouble(s),
    "alpha_composite": lambda s: ImgDouble(s),
    "paste": lambda s: ImgDouble(s),
    "putalpha": lambda s: ImgDouble(s),
    "darker": lambda s: ImgDouble(s),
    "frombytes": lambda s: ImgDouble(s),
    "transpose": lambda s: ImgDouble(s),
    "_isolate_slot_subject": lambda s: ImgDouble(s),
    "_drop_side_bleed": lambda s: ImgDouble(s),
    "_sever_expected_gutters": lambda s: ImgDouble(s),
}


def record(name, args):
    scripts = _SCRIPTS
    args = {k: norm(v) for k, v in args.items() if v is not None}
    script = scripts.next(name)
    if isinstance(script, dict) and isinstance(script.get("__raise__"), str):
        marker = norm(script)
        scripts.trace.append({"ev": name, "args": args, "ret": marker})
        raise make_exc(marker)
    scripts.trace.append({"ev": name, "args": args, "ret": norm(script)})
    if script is None:
        return None
    mat = _MATERIALIZE.get(name)
    return mat(script) if mat else script


class SeamFn:
    def __init__(self, name):
        self.name = name

    def __call__(self, *a, **kw):
        return record(self.name, SEAM_ARGS[self.name](a, kw))


# seam arg projections — MUST match the C pa2_call key sets exactly.
SEAM_ARGS = {
    "accept": lambda a, k: {"x": a[0], "y": a[1]},
    "_isolate_slot_subject": lambda a, k: {"image": a[0]},
    "_drop_side_bleed": lambda a, k: {"image": a[0]},
    "_frame_x_ranges": lambda a, k: {"strip": a[0], "frame_count": a[1]},
    "_sever_expected_gutters": lambda a, k: {"strip": a[0], "frame_count": a[1]},
    "_slot_crops": lambda a, k: {"strip": a[0], "frame_count": a[1],
                                  "require_padding": k.get("require_padding", False)},
    "Image.open": lambda a, k: {"path": a[0]},
    "Image.new": lambda a, k: {"mode": a[0], "size": list(a[1]), "color": list(a[2])},
    "composite": lambda a, k: {"image1": a[0], "image2": a[1], "mask": a[2]},
}


class _Resample:
    def __init__(self, name):
        self._pa2 = name


class _Filter:
    def __init__(self, name):
        self._pa2 = name


class _Transpose:
    def __init__(self, name):
        self._pa2 = name


class ImageModule:
    @staticmethod
    def new(mode, size, color):
        return record("Image.new", {"mode": mode, "size": list(size), "color": list(color)})
    @staticmethod
    def open(path):
        return record("Image.open", {"path": path})
    @staticmethod
    def composite(a, b, mask):
        return record("composite", {"image1": a, "image2": b, "mask": mask})
    NEAREST = _Resample("NEAREST")
    BILINEAR = _Resample("BILINEAR")
    Transpose = type("Transpose", (), {"FLIP_LEFT_RIGHT": _Transpose("FLIP_LEFT_RIGHT")})


class ImageFilterModule:
    @staticmethod
    def MinFilter(n):
        return _Filter("MinFilter%d" % n)


# ── base namespace ─────────────────────────────────────────────────────────
CELL_WIDTH, CELL_HEIGHT = 192, 208
ROW_SPECS = [
    ("idle", 0, 6), ("running-right", 1, 8), ("running-left", 2, 8),
    ("waving", 3, 4), ("jumping", 4, 5), ("failed", 5, 8),
    ("waiting", 6, 6), ("running", 7, 6), ("review", 8, 6),
]
_NEIGHBOURS = ((1, 0), (-1, 0), (0, 1), (0, -1))

_BASE_NS = {
    "__name__": "pa2_live_atlas",
    "__builtins__": builtins,
    "deque": deque, "groupby": groupby, "Path": Path,
    "Image": ImageModule, "ImageFilter": ImageFilterModule,
    "_NEIGHBOURS": _NEIGHBOURS,
    "CELL_WIDTH": CELL_WIDTH, "CELL_HEIGHT": CELL_HEIGHT,
    "ROW_SPECS": ROW_SPECS,
    "FRAME_W": CELL_WIDTH, "FRAME_H": CELL_HEIGHT,
    "_ALPHA_FLOOR": 16, "_CELL_PAD": 10, "_NORMALIZE_PAD": 14,
    "_SIDE_LOBE_RATIO": 0.18,
    # claimed siblings seamed
    "_drop_side_bleed": SeamFn("_drop_side_bleed"),
    "_isolate_slot_subject": SeamFn("_isolate_slot_subject"),
    "_frame_x_ranges": SeamFn("_frame_x_ranges"),
    "_sever_expected_gutters": SeamFn("_sever_expected_gutters"),
    "_slot_crops": SeamFn("_slot_crops"),
    # accept callback seam (installed per-case by the driver)
    "accept": SeamFn("accept"),
}


def build_ns(rec):
    global _SCRIPTS
    inp = rec["in"]
    trace = []
    scripts = Scripts(inp.get("io") or {}, trace)
    _SCRIPTS = scripts
    ns = dict(_BASE_NS)
    for _nm, _code in DEF_CODE.items():
        exec(_code, ns)
    return ns, trace


# ── live driver ────────────────────────────────────────────────────────────
def as_img(x):
    """Materialize a scripted image-handle dict into an ImgDouble double."""
    return ImgDouble(x) if isinstance(x, dict) else x


def drive(rec, ns):
    f = rec["func"]
    p = rec["in"]
    if f == "_median":
        return ns[f](p.get("values"))
    if f == "_median_box_size":
        return ns[f](p.get("boxes"))
    if f == "_blank":
        return ns[f](tuple(p.get("size", [0, 0])))
    if f == "_place":
        return ns[f](as_img(p.get("sprite")), tuple(p.get("size", [0, 0])),
                     tuple(p.get("offset", [0, 0])))
    if f == "_clear_region":
        return ns[f](as_img(p.get("image")), tuple(p.get("box", [0, 0, 0, 0])))
    if f == "_load_rgba":
        return ns[f](as_img(p.get("image")))
    if f == "_flood":
        return ns[f](p.get("w", 0), p.get("h", 0),
                     bytearray(p.get("visited", [])), p.get("seeds", []), ns["accept"])
    if f == "_border_flood":
        return ns[f](p.get("w", 0), p.get("h", 0),
                     bytearray(p.get("visited", [])), ns["accept"])
    if f == "_unvisited_components":
        return list(ns[f](p.get("w", 0), p.get("h", 0),
                          bytearray(p.get("visited", [])), ns["accept"]))
    if f == "_remove_masked":
        return ns[f](as_img(p.get("rgba")), as_img(p.get("mask")))
    if f == "_thin_groups":
        return ns[f](p.get("indices"))
    if f == "_has_margin":
        return ns[f](tuple(p.get("size", [0, 0])), tuple(p.get("box", [0, 0, 0, 0])),
                     p.get("fx", 0.0), p.get("fy", 0.0))
    if f == "_is_multi_pose_outlier":
        return ns[f](p.get("width", 0), p.get("height", 0),
                     p.get("med_w", 0), p.get("med_h", 0))
    if f == "_clean_slot":
        return ns[f](as_img(p.get("image")))
    if f == "_salvage_frames":
        return ns[f](as_img(p.get("strip")), p.get("frame_count", 0))
    if f == "_check_atlas_cells":
        return ns[f](as_img(p.get("atlas")))
    raise KeyError(f"unknown func {f}")


# ── main ───────────────────────────────────────────────────────────────────
def main():
    n = mism = shown = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        n += 1
        try:
            ns, trace = build_ns(rec)
            try:
                ret = drive(rec, ns)
                out = {"ret": norm(ret), "trace": trace, "state": {}}
            except BaseException as e:
                out = {"ret": {"__raise__": type(e).__name__}, "trace": trace, "state": {}}
        except BaseException as e:
            out = {"ret": {"__raise__": type(e).__name__}, "trace": [], "state": {}}
        got = rec.get("out")
        if got != out:
            mism += 1
            if shown < 12:
                shown += 1
                print(f"MISMATCH {rec.get('func')} case {n}:")
                print(f"  C={json.dumps(got, ensure_ascii=False)[:1200]}")
                print(f"  P={json.dumps(out, ensure_ascii=False)[:1200]}")
    print(f"ORACLE {NAME}: {n} cases, {mism} mismatches")
    sys.exit(1 if (mism or n == 0) else 0)


def selftest(fixtures):
    bad = 0
    for i, rec in enumerate(fixtures):
        try:
            ns, trace = build_ns(rec)
            drive(rec, ns)
        except BaseException as e:
            msg = str(e)
            if isinstance(e, AttributeError) and any(
                    f"'{nm}'" in msg for nm in PORTED):
                bad += 1
                print(f"SELFTEST case {i} {rec['func']}: method-named AttributeError: {msg}")
    print(f"SELFTEST {NAME}: {len(fixtures)} cases, {bad} suspicious")
    return bad


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        fx = Path(__file__).resolve().parent / "fixtures" / "pet_atlas_flow2.jsonl"
        recs = [json.loads(l) for l in fx.read_text(encoding="utf-8").split("\n") if l.strip()]
        sys.exit(1 if selftest(recs) else 0)
    main()
