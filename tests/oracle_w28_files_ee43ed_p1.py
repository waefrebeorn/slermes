#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/files.py — gate
w28_files_ee43ed_p1 (path scoping & sanitisation).

The live router module is imported BY FILE PATH (no package import of the
router, whose FastAPI decorators would otherwise drag the whole web_server
graph in).  Only the pure helpers are called; every filesystem answer the C
side receives as a fixture flag is supplied here instead:

  * ``real``  stands in for ``os.path.realpath`` in ``_is_local_sensitive_path``;
  * ``nul``   stands in for a U+0000 in the raw path (a C string cannot carry
    one through the JSON-lines transport);
  * ``exists`` stands in for the stat behind ``_resolve_managed_path``.

Status codes are what the route would have raised, returned as data:
``{"ok": bool, "status": int, "detail": str|None}`` — the projection the C
functions emit.
"""
import importlib.util
import json
import os
import sys
from pathlib import Path, PurePath

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_files_ee43ed_p1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

if os.name == "nt" and not os.environ.get("USERPROFILE"):
    os.environ["USERPROFILE"] = str(QUARRY.parent)

sys.path.append(str(Path(__file__).resolve().parent))  # stub helper only; must NOT shadow the quarry packages
from hermes_seam_stubs import install as _install_seams  # noqa: E402

# This host blocks pydantic's native extension, and the router package
# imports fastapi at module scope: those are seams, not logic under test.
HTTPException = _install_seams()


def _load(name, rel):
    spec = importlib.util.spec_from_file_location(name, QUARRY / rel)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


live = _load("w28_live_files_p1", "hermes_cli/web_routers/files.py")
wsf = _load("w28_live_wsf_p1", "hermes_cli/web_server_files.py")

NT = os.name == "nt"

WANTED = {
    "is_sensitive_filename", "is_sensitive_path", "deny_path_under",
    "is_local_sensitive_path", "sanitize_filename", "filename_stem",
    "path_text", "path_is_under", "file_url_path",
    "agent_container_translatable", "unmapped_container_path",
    "managed_path_verdict", "if_none_match", "if_none_match_hit",
}


def verdict(ok, status=200, detail=None, **extra):
    out = {"ok": ok, "status": status, "detail": detail}
    out.update(extra)
    return out


def from_http(fn, *args):
    """Run the live helper; project an HTTPException to the verdict shape."""
    try:
        return True, fn(*args)
    except HTTPException as exc:
        return False, verdict(False, exc.status_code, exc.detail)


def sanitize_filename(r):
    return live._sanitize_chat_image_filename(r.get("name"))


def filename_stem(r):
    """upload_chat_image: the stem the written name carries."""
    import re

    stem = PurePath(live._sanitize_chat_image_filename(r.get("name"))).stem or "pasted-image"
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", stem).strip("._-") or "pasted-image"


def path_text(r):
    if r.get("nul"):
        return verdict(False, 400, "Invalid path")
    try:
        return verdict(True, 200, None, text=wsf._path_text(r.get("raw")))
    except HTTPException as exc:
        return verdict(False, exc.status_code, exc.detail)


def path_is_under(r):
    return wsf._path_is_under(Path(r.get("root") or ""), Path(r.get("target") or ""))


def file_url_path(r):
    try:
        value = wsf._file_url_path(r.get("url") or "")
    except HTTPException as exc:
        return verdict(False, exc.status_code, exc.detail)
    except ValueError as exc:  # the helper's own refusal (encoded separator / foreign host)
        return verdict(False, 400, str(exc))
    return verdict(True, 200, None, path=str(value))


def agent_container_translatable(r):
    raw = r.get("raw") or ""
    return not (not raw.startswith("/") or raw.startswith("//") or ".." in raw.split("/"))


def unmapped_container_path(r):
    raw = r.get("raw") or ""
    if not raw.startswith("/") or raw.startswith("//"):
        return False
    if not r.get("docker_active"):
        return False
    if not r.get("msys_same", True):
        return False
    roots = {"/workspace", "/root", *r.get("roots", [])}
    return any(raw == root or raw.startswith(root.rstrip("/") + "/") for root in roots)


def managed_path_verdict(r):
    """_resolve_managed_path's refusals, with existence supplied by the fixture."""
    text = wsf._path_text(r.get("text"))
    root = Path(r["root"]) if r.get("root") else None
    locked = bool(r.get("locked"))
    for_write = bool(r.get("for_write"))
    exists = bool(r.get("exists", True))

    if locked and (not text or text in {".", "/"}):
        candidate = root
    elif not text:
        candidate = Path(r.get("default_path") or ".")
    else:
        candidate = Path(text).expanduser()
        if locked and not candidate.is_absolute():
            if any(part == ".." for part in candidate.parts):
                return verdict(False, 400, "Path cannot contain '..'")
            candidate = root / candidate
        elif not candidate.is_absolute():
            return verdict(False, 400, "Path must be absolute")
    if ".." in candidate.parts:
        return verdict(False, 400, "Path cannot contain '..'")
    if not for_write and not exists:
        return verdict(False, 404, "Path not found")
    resolved = candidate if for_write and not exists else candidate.resolve(strict=False)
    under = root is not None and wsf._path_is_under(root, Path(resolved))
    if locked and not under:
        return verdict(False, 403, "Path outside managed files root")
    return verdict(True, 200, None, under_root=bool(under))


def expected(r):
    f = r.get("func")
    if f == "is_sensitive_filename":
        return live._is_sensitive_filename(r.get("name") or "")
    if f == "is_sensitive_path":
        return live._is_sensitive_path(Path(r.get("path") or ""))
    if f == "deny_path_under":
        from agent.file_safety import deny_path_under

        return deny_path_under(r.get("resolved") or "", r.get("base") or "")
    if f == "is_local_sensitive_path":
        from agent.file_safety import deny_key, deny_path_under

        path = Path(r.get("path") or "")
        if live._is_sensitive_path(path):
            return True
        real = r.get("real") or ""
        files, dirs = r.get("files") or [], r.get("dirs") or []
        return deny_key(real) in {deny_key(x) for x in files} or any(deny_path_under(real, d) for d in dirs)
    if f == "sanitize_filename":
        return sanitize_filename(r)
    if f == "filename_stem":
        return filename_stem(r)
    if f == "path_text":
        return path_text(r)
    if f == "path_is_under":
        return path_is_under(r)
    if f == "file_url_path":
        return file_url_path(r)
    if f == "agent_container_translatable":
        return agent_container_translatable(r)
    if f == "unmapped_container_path":
        return unmapped_container_path(r)
    if f == "managed_path_verdict":
        return managed_path_verdict(r)
    if f == "if_none_match":
        class _R:
            headers = {"if-none-match": r.get("header") or ""}

        return sorted(live._if_none_match(_R()))
    if f == "if_none_match_hit":
        class _R:
            headers = {"if-none-match": r.get("header") or ""}

        return bool({r.get("etag"), "*"} & live._if_none_match(_R()))
    raise AssertionError("unknown func %r" % (f,))


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
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
