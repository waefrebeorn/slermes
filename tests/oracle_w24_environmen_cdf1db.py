#!/usr/bin/env python3
"""Differential oracle for agent/verify/environment.py
(gate w24_environmen_cdf1db).

The module reads and writes ``<project>/.hermes/environment.json``.  Here the
file's text is the case's input: ``manifest_path`` is exercised on a ``Path``,
``load_manifest``'s body (utf-8-sig decode, JSON parse, dict shape, then
``Recipe.from_dict``) is exercised by feeding that text to the live parser
chain, and the save payload is built from the live ``Recipe.to_dict``.  No file
is opened, no directory is created, and the clock is the case's timestamp.
"""
import json
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w24_environmen_cdf1db"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

from agent.verify import environment as live  # noqa: E402
from agent.verify.recipes import Recipe  # noqa: E402

WANTED = {"manifest_path", "recipe", "parse_manifest", "source", "save_payload", "constants"}


def slash(path):
    return str(path).replace("\\", "/")


def manifest_path(req):
    return slash(live.manifest_path(Path(req.get("root") or "")))


def _to_dict(raw):
    built = Recipe.from_dict(raw)
    return built.to_dict() if built is not None else None


def recipe(req):
    return _to_dict(req.get("raw"))


def _manifest_recipe(manifest):
    if not isinstance(manifest, dict):
        return None
    return _to_dict(manifest.get("recipe", manifest))


def parse_manifest(req):
    text = req.get("text")
    if not isinstance(text, str):
        return None  # an unreadable manifest
    if text.startswith("\ufeff"):
        text = text[1:]  # read_text(encoding="utf-8-sig")
    try:
        manifest = json.loads(text)
    except ValueError:  # includes JSONDecodeError, and the empty file
        return None
    return _manifest_recipe(manifest)


def source(req):
    return "manifest" if req.get("saved_present") else "detected"


def save_payload(req):
    built = Recipe.from_dict(req.get("recipe"))
    if built is None:
        return {"error": "attribute"}  # None.to_dict()
    return {
        "version": live.MANIFEST_VERSION,
        "recipe": built.to_dict(),
        "updatedAt": req.get("updated_at", ""),
    }


def constants():
    return {"manifest_version": live.MANIFEST_VERSION, "manifest_relpath": slash(live._MANIFEST_RELPATH)}


def expected(r):
    f = r.get("func")
    if f == "manifest_path":
        return manifest_path(r)
    if f == "recipe":
        return recipe(r)
    if f == "parse_manifest":
        return parse_manifest(r)
    if f == "source":
        return source(r)
    if f == "save_payload":
        return save_payload(r)
    if f == "constants":
        return constants()
    raise AssertionError("unknown func %r" % (f,))


def eq(a, b):
    """Python equality: absent key == null == None, True == 1, 1 == 1.0."""
    if isinstance(a, bool) or isinstance(b, bool):
        return isinstance(a, (bool, int, float)) and isinstance(b, (bool, int, float)) and a == b
    if isinstance(a, dict) and isinstance(b, dict):
        return set(a) == set(b) and all(eq(a[k], b[k]) for k in a)
    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(eq(x, y) for x, y in zip(a, b))
    if a is None or b is None:
        return a is b
    return a == b


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in WANTED:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc), file=sys.stderr)
            continue
        got = r.get("out")
        if not eq(py, got):
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
