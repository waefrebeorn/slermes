#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/files.py — gate
w28_files_ee43ed_p2 (listing / entry shaping / size & mime derivation).

Both live modules are imported BY FILE PATH.  What the C side receives as a
fixture flag is supplied here instead:

  * ``kind`` (dir/file/broken/missing/outside) stands in for the stat behind
    ``_managed_file_entry`` — the entry text is built by the live rules;
  * ``size``/``mtime`` are the stat fields the caller would have read;
  * ``exists``-driven policy is projected to ``{root_kind, locked,
    can_change_path}`` because the default path's text is a Path, i.e. host
    normalisation, not a decision;
  * the entry ``path`` key is dropped for the same reason (it is ``str(Path)``).

``_fs_mime_type``'s ``mimetypes.guess_type`` fallback is a machine seam; cases
stay on the module's own table or on an extension the database does not know,
which is exactly where the ported table and the octet fallback answer.
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
GATE = "w28_files_ee43ed_p2"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# Path.home() needs USERPROFILE on Windows; a piped/msys run can lose it. The
# repo's parent is the real home here, so the guard below never invents one.
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


live = _load("w28_live_files_p2", "hermes_cli/web_routers/files.py")
wsf = _load("w28_live_wsf_p2", "hermes_cli/web_server_files.py")
# Imported once, up front: a later import would run after HERMES_HOME has been
# pointed at a fixture home by managed_files_policy's env probe.
from hermes_cli.ssh_workspace_fs import SshWorkspaceFsError  # noqa: E402

NT = os.name == "nt"

WANTED = {
    "fs_mime_type", "preview_governing", "looks_binary", "managed_response_meta",
    "managed_files_policy", "managed_file_size", "managed_file_entry",
    "fs_list_sort", "managed_list_sort", "list_has_parent", "is_readdir_hidden",
    "backend_error", "media_extension_verdict", "sandbox_not_found_detail",
}


def verdict(ok, status=200, detail=None, **extra):
    out = {"ok": ok, "status": status, "detail": detail}
    out.update(extra)
    return out


def preview_governing(r):
    requested = str(r.get("requested") or "").strip()
    target = Path(r.get("target") or "")
    governing = live._fs_preview_type_path(r.get("requested"), target)
    origin = PurePath(requested if not requested.lower().startswith("file:") else target)
    return {
        "source": "requested" if governing == origin else "target",
        "suffix": governing.suffix.lower(),
        "language": live._FS_PREVIEW_LANGUAGE_BY_EXT.get(governing.suffix.lower(), "text"),
    }


def managed_response_meta(r):
    policy = wsf.ManagedFilesPolicy(
        default_path=Path("/"),
        locked_root=Path("/data") if r.get("locked") else None,
        can_change_path=bool(r.get("can_change_path", True)),
    )
    meta = wsf._managed_response_meta(policy)
    return {
        "root_locked": meta["root"] is not None,
        "locked_root_present": meta["locked_root"] is not None,
        "can_change_path": meta["can_change_path"],
    }


def managed_files_policy(r):
    forced = str(r.get("forced_root") or "").strip()
    if forced:
        return {"root_kind": "forced", "locked": True, "can_change_path": False}
    home = str(r.get("hermes_home") or "").strip()
    if home:
        saved = os.environ.get("HERMES_HOME")
        os.environ["HERMES_HOME"] = home
        try:
            from hermes_constants import get_default_hermes_root

            try:
                resolved = get_default_hermes_root().expanduser().resolve(strict=False)
            except (OSError, RuntimeError):
                resolved = Path(home).expanduser().resolve(strict=False)
            if resolved == wsf._HOSTED_MANAGED_FILES_ROOT:
                return {"root_kind": "opt_data", "locked": True, "can_change_path": False}
        finally:
            if saved is None:
                os.environ.pop("HERMES_HOME", None)
            else:
                os.environ["HERMES_HOME"] = saved
    return {"root_kind": "home", "locked": False, "can_change_path": True}


def managed_file_entry(r):
    kind = r.get("kind")
    name = r.get("name") or ""
    locked = bool(r.get("locked"))
    if kind == "outside":
        return verdict(False, 403, "Path outside managed files root")
    if kind == "missing":
        return None
    if kind == "broken":
        if locked and not r.get("inside", True):
            return verdict(False, 403, "Path outside managed files root")
        return {"name": name, "is_directory": False, "broken_link": True,
                "size": None, "mtime": None, "mime_type": None}
    if kind == "dir":
        return {"name": name, "is_directory": True, "size": None,
                "mtime": float(r.get("mtime") or 0), "mime_type": None}
    return {"name": name, "is_directory": False, "size": float(r.get("size") or 0),
            "mtime": float(r.get("mtime") or 0),
            "mime_type": live._fs_mime_type(PurePath(name))}


def backend_error(code):
    try:
        live._raise_fs_backend_error(SshWorkspaceFsError(code or ""))
    except HTTPException as exc:
        return verdict(False, exc.status_code, exc.detail)
    raise AssertionError("no refusal")


def expected(r):
    f = r.get("func")
    if f == "fs_mime_type":
        return live._fs_mime_type(Path(r.get("path") or ""))
    if f == "preview_governing":
        return preview_governing(r)
    if f == "looks_binary":
        return live._fs_looks_binary(bytes(r.get("data") or []))
    if f == "managed_response_meta":
        return managed_response_meta(r)
    if f == "managed_files_policy":
        return managed_files_policy(r)
    if f == "managed_file_size":
        size, cap = float(r.get("size") or 0), float(r.get("max_bytes") or 0)
        if size > cap:
            return verdict(False, 413, "File is too large")
        return verdict(True, 200, None, size=size)
    if f == "managed_file_entry":
        return managed_file_entry(r)
    if f == "fs_list_sort":
        return sorted(r.get("entries") or [],
                      key=lambda i: (not i["isDirectory"], i["name"].lower(), i["name"]))
    if f == "managed_list_sort":
        return sorted(r.get("entries") or [],
                      key=lambda i: (not i["is_directory"], i["name"].lower()))
    if f == "list_has_parent":
        target = Path(r.get("target") or "")
        locked = Path(r["locked_root"]) if r.get("locked_root") else None
        return bool(target.parent != target and (locked is None or target != locked))
    if f == "is_readdir_hidden":
        return (r.get("name") or "") in live._FS_READDIR_HIDDEN
    if f == "backend_error":
        return backend_error(r.get("code"))
    if f == "media_extension_verdict":
        if not r.get("media_only"):
            return verdict(True, 200, None)
        suffix = (r.get("suffix") or "").lower()
        if suffix not in live._STREAMABLE_MEDIA_EXTENSIONS:
            return verdict(False, 415, "Unsupported media type")
        return verdict(True, 200, None)
    if f == "sandbox_not_found_detail":
        return live.SANDBOX_NOT_FOUND if r.get("unmapped") else "File not found"
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
