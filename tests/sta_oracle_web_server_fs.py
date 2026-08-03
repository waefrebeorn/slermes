#!/usr/bin/env python3
"""
sta_oracle_web_server_fs.py — LIVE-Python oracle for the /api/fs cluster.
Extracts the real function sources + constants from hermes_cli/web_server.py
(module import is FastAPI-heavy) and execs them, then answers the same
fixture ops as the C harness against the same real filesystem sandbox.
"""
import ast
import base64
import json
import os
import stat as stat_mod
import sys
from pathlib import Path

WEB_SERVER = Path("/home/wubu/hermes-agent-dev/hermes_cli/web_server.py")

WANT_FUNCS = {"_fs_path", "_fs_mime_type", "_fs_looks_binary",
              "_fs_regular_file"}
WANT_CONSTS = {"_FS_READDIR_HIDDEN", "_FS_PREVIEW_LANGUAGE_BY_EXT",
               "_FS_MIME_TYPES", "_FS_DATA_URL_MAX_BYTES",
               "_FS_TEXT_SOURCE_MAX_BYTES", "_FS_TEXT_PREVIEW_MAX_BYTES",
               "_FS_TEXT_WRITE_MAX_BYTES"}


class HTTPException(Exception):
    def __init__(self, status_code, detail=""):
        self.status_code = status_code
        self.detail = detail


def load_live():
    src = WEB_SERVER.read_text(encoding="utf-8")
    tree = ast.parse(src)
    pieces = []
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name in WANT_FUNCS:
            pieces.append(ast.get_source_segment(src, node))
        elif isinstance(node, ast.Assign):
            for t in node.targets:
                if isinstance(t, ast.Name) and t.id in WANT_CONSTS:
                    pieces.append(ast.get_source_segment(src, node))
        elif isinstance(node, ast.AnnAssign):
            if isinstance(node.target, ast.Name) and node.target.id in WANT_CONSTS:
                pieces.append(ast.get_source_segment(src, node))
    import mimetypes
    import urllib.parse
    import urllib.request
    ns = {
        "Path": Path, "os": os, "stat": stat_mod, "mimetypes": mimetypes,
        "urllib": urllib, "HTTPException": HTTPException, "tuple": tuple,
    }
    exec("\n\n".join(pieces), ns)
    missing = (WANT_FUNCS | WANT_CONSTS) - set(ns)
    if missing:
        raise SystemExit(f"oracle failed to extract: {missing}")
    return ns


def main():
    fx = json.loads(Path(sys.argv[1]).read_text())
    ns = load_live()
    op = fx["op"]

    if op == "list":
        # replicate fs_list route body with live constants/helpers
        try:
            target = ns["_fs_path"](fx["path"])
        except HTTPException:
            print(json.dumps({"entries": [], "error": "read-error"},
                             separators=(",", ":")))
            return
        try:
            entries = []
            with os.scandir(target) as scan:
                for entry in scan:
                    if entry.name in ns["_FS_READDIR_HIDDEN"]:
                        continue
                    entries.append({
                        "name": entry.name,
                        "isDirectory": entry.is_dir(follow_symlinks=False),
                    })
            entries.sort(key=lambda i: (not i["isDirectory"],
                                        i["name"].lower(), i["name"]))
            print(json.dumps({"entries": entries}, separators=(",", ":")))
        except FileNotFoundError:
            print(json.dumps({"entries": [], "error": "ENOENT"}, separators=(",", ":")))
        except NotADirectoryError:
            print(json.dumps({"entries": [], "error": "ENOTDIR"}, separators=(",", ":")))
        except PermissionError:
            print(json.dumps({"entries": [], "error": "EACCES"}, separators=(",", ":")))
        except OSError as exc:
            print(json.dumps({"entries": [],
                              "error": getattr(exc, "strerror", None) or "read-error"},
                             separators=(",", ":")))

    elif op == "read_text":
        try:
            target, st = ns["_fs_regular_file"](ns["_fs_path"](fx["path"]))
            if st.st_size > ns["_FS_TEXT_SOURCE_MAX_BYTES"]:
                raise HTTPException(413)
            to_read = min(st.st_size, ns["_FS_TEXT_PREVIEW_MAX_BYTES"])
            with open(target, "rb") as h:
                data = h.read(to_read)
            print(json.dumps({
                "binary": ns["_fs_looks_binary"](data[:4096]),
                "byteSize": st.st_size,
                "language": ns["_FS_PREVIEW_LANGUAGE_BY_EXT"].get(
                    target.suffix.lower(), "text"),
                "mimeType": ns["_fs_mime_type"](target),
                "text": data.decode("utf-8", errors="replace"),
                "truncated": st.st_size > ns["_FS_TEXT_PREVIEW_MAX_BYTES"],
            }, separators=(",", ":"), ensure_ascii=False))
        except HTTPException as e:
            print(json.dumps({"status": e.status_code}, separators=(",", ":")))

    elif op == "write_text":
        try:
            target = ns["_fs_path"](fx["path"])
            text = fx.get("content") or ""
            if len(text.encode("utf-8")) > ns["_FS_TEXT_WRITE_MAX_BYTES"]:
                raise HTTPException(413)
            try:
                st = target.stat()
            except FileNotFoundError:
                st = None
            except PermissionError:
                raise HTTPException(403)
            if st is not None and stat_mod.S_ISDIR(st.st_mode):
                raise HTTPException(400)
            if st is not None and not stat_mod.S_ISREG(st.st_mode):
                raise HTTPException(400)
            if not target.parent.is_dir():
                raise HTTPException(400)
            tmp = target.with_name(f".{target.name}.hermes-tmp-{os.getpid()}")
            try:
                tmp.write_text(text, encoding="utf-8")
                os.replace(tmp, target)
            except PermissionError:
                tmp.unlink(missing_ok=True)
                raise HTTPException(403)
            except OSError:
                tmp.unlink(missing_ok=True)
                raise HTTPException(500)
            print(json.dumps({"status": 0,
                              "byteSize": len(text.encode("utf-8"))},
                             separators=(",", ":")))
        except HTTPException as e:
            print(json.dumps({"status": e.status_code}, separators=(",", ":")))

    elif op == "read_data_url":
        try:
            target, st = ns["_fs_regular_file"](ns["_fs_path"](fx["path"]))
            if st.st_size > ns["_FS_DATA_URL_MAX_BYTES"]:
                raise HTTPException(413)
            encoded = base64.b64encode(target.read_bytes()).decode("ascii")
            print(json.dumps(
                {"dataUrl": f"data:{ns['_fs_mime_type'](target)};base64,{encoded}"},
                separators=(",", ":")))
        except HTTPException as e:
            print(json.dumps({"status": e.status_code}, separators=(",", ":")))

    elif op == "preview_language":
        p = Path(fx["path"])
        print(json.dumps({"language": ns["_FS_PREVIEW_LANGUAGE_BY_EXT"].get(
            p.suffix.lower(), "text")}, separators=(",", ":")))

    elif op == "readdir_hidden":
        print(json.dumps({"hidden": fx["name"] in ns["_FS_READDIR_HIDDEN"]},
                         separators=(",", ":")))
    else:
        raise SystemExit(f"unknown op {op}")


if __name__ == "__main__":
    main()
