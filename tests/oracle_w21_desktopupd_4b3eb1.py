#!/usr/bin/env python3
"""Differential oracle for hermes_cli/desktop_update_verify.py (gate w21_desktopupd_4b3eb1).

The live module is imported from the quarry and `_verify_packaged_entry` runs
for real, byte for byte: the filesystem is served by in-memory stand-ins (an
archive with `open`/`stat`, an `app.asar.unpacked` tree, and `dist/index.html`)
so every number the fixture names is the number the live reader sees.

  * `asar_header_check` -- the four `struct.unpack("<4I")` fields plus the file
    size drive a real archive; the live verdict is "invalid ASAR header" or
    "the header was accepted" (it then fails later, on the entry JSON).
  * `asar_entry_path_check` / `asar_entry_check` -- a consistent archive
    declares `package.json` and, as its `main`, the fixture's entry name/size/
    offset/unpacked flag, so the failure the live reader raises is the rule
    under test.
  * `renderer_has_module_entry` -- a valid archive plus the fixture's
    index.html: success, or the live "renderer has no local module entry".
  * `root_to_verify` / `verify` -- `_desktop_packaged_executable`,
    `_desktop_exe_integrity_error`, `_desktop_build_needed` and
    `_verify_packaged_entry` are the stubs (each answers from the fixture), and
    `checkout_root()` is the imported checkout the fixture names.
"""
import importlib.util
import io
import json
import struct
import sys
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "desktop_update_verify.py"
GATE = "w21_desktopupd_4b3eb1"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

_spec = importlib.util.spec_from_file_location("desktop_update_verify_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["desktop_update_verify_live"] = live
_spec.loader.exec_module(live)


# ---- in-memory filesystem the live reader walks ---------------------------

class _Stat:
    def __init__(self, size):
        self.st_size = size


class _Unpacked:
    """`resources / "app.asar.unpacked" / <parts>`: a file that may be absent."""

    def __init__(self, files, parts=()):
        self._files = files
        self._parts = tuple(parts)

    def __truediv__(self, name):
        return _Unpacked(self._files, self._parts + (str(name),))

    def _data(self):
        return self._files.get("/".join(self._parts))

    def read_bytes(self):
        data = self._data()
        if data is None:
            raise FileNotFoundError("unpacked entry missing: " + "/".join(self._parts))
        return data

    def read_text(self, encoding="utf-8"):
        data = self._data()
        if data is None:
            raise FileNotFoundError("renderer index missing: " + "/".join(self._parts))
        return data.decode(encoding)


class _Resources:
    """`resources` for one packaged app: the archive plus its unpacked tree."""

    def __init__(self, blob, size, unpacked=None, index_html=None):
        self.blob = blob
        self.size = size
        self.unpacked = dict(unpacked or {})
        if index_html is not None:
            self.unpacked["dist/index.html"] = index_html.encode("utf-8")

    def __truediv__(self, name):
        if str(name) == "app.asar":
            return _Archive(self.blob, self.size)
        if str(name) == "app.asar.unpacked":
            return _Unpacked(self.unpacked)
        return _Unpacked(self.unpacked, (str(name),))


class _Archive:
    def __init__(self, blob, size):
        self.blob = blob
        self.size = size

    def open(self, mode="rb"):
        return io.BytesIO(self.blob)

    def stat(self):
        return _Stat(self.size)


# ---- archive builders -----------------------------------------------------

def _layout(header, header_size=None):
    """Header JSON padded so the Pickle header's own fields hold together.

    With no target, the natural sizes are used. With a target, the JSON is
    padded with spaces (json.loads tolerates them) so `header_size` is exactly
    the number the fixture names.
    """
    text = json.dumps(header).encode("utf-8")
    if header_size is None:
        json_size = len(text)
    else:
        payload_size = header_size - 4
        json_size = payload_size - 4
        if json_size < len(text) or (json_size % 4) != 0:
            raise AssertionError("header_size %r cannot hold this header" % (header_size,))
    payload_size = 4 + ((json_size + 3) // 4) * 4
    header_size = payload_size + 4
    head = struct.pack("<4I", 4, header_size, payload_size, json_size)
    return head + text.ljust(payload_size - 4, b" "), header_size


def _archive_with(header, data, size, header_size=None):
    head_and_json, real_header_size = _layout(header, header_size)
    blob = head_and_json + data
    if size is None:
        size = len(blob)
    blob = blob.ljust(size, b"\0")[:size]
    return _Resources(blob, size, ), real_header_size


PKG = b'{"main":"main.js"}'


def _nest(header, name, node):
    """Declare *name* the way ASAR nests it: `dist/app.js` -> files.dist.files['app.js']."""
    parts = [p for p in name.split("/") if p and p != "."]
    if not parts or parts[0] == "..":
        return
    files = header["files"]
    for part in parts[:-1]:
        slot = files.setdefault(part, {"files": {}})
        files = slot.setdefault("files", {})
    files[parts[-1]] = node


def _entry_archive(name, length, offset, unpacked, file_size, data_blank, header_size=None):
    node = {"size": length, "offset": offset}
    if unpacked:
        node["unpacked"] = True
    pkg = json.dumps({"main": name}).encode("utf-8")
    header = {"files": {"package.json": {"size": len(pkg), "offset": 0}}}
    _nest(header, name, node)
    head_and_json, real_header_size = _layout(header, header_size)
    base = len(head_and_json)             # 8 + header_size
    data = (b" " * length) if data_blank else (b"x" * length)
    need = base + max(offset + length, len(pkg), 1)
    blob = bytearray(need)
    blob[:len(head_and_json)] = head_and_json
    blob[base:base + len(pkg)] = pkg
    if not unpacked and offset >= len(pkg):
        blob[base + offset:base + offset + length] = data
    size = file_size if file_size is not None else len(blob)
    if size < len(blob):
        blob = blob[:max(size, 0)]
    else:
        blob = blob.ljust(size, b"\0")
    unpacked_files = {name: data} if unpacked else {}
    return _Resources(bytes(blob), size, unpacked=unpacked_files), real_header_size


def _renderer_resources(html):
    header = {"files": {"package.json": {"size": len(PKG), "offset": 0},
                        "main.js": {"size": 8, "offset": len(PKG)}}}
    head_and_json, _hs = _layout(header)
    blob = head_and_json + PKG + b"main();\n"
    return _Resources(blob, len(blob), index_html=html)


def _packaged_entry_error(resources):
    try:
        _REAL_VERIFY_ENTRY(resources)
    except RuntimeError as exc:
        return str(exc)
    return ""


# ---- stub env for root_to_verify / verify ---------------------------------

STUB = {"packaged": {}, "integrity": {}, "entry_error": "", "build_needed": {}}
_checkout = {"root": ""}


def _stub_packaged_executable(desktop):
    value = STUB["packaged"].get(str(desktop))
    return None if value is None else Path(str(value))


def _stub_exe_integrity_error(executable):
    return STUB["integrity"].get(str(executable)) or None


def _stub_build_needed(desktop, project_root, source_mode=False):
    return bool(STUB["build_needed"].get(str(project_root)))


def _stub_verify_packaged_entry(resources):
    if STUB["entry_error"]:
        raise RuntimeError(STUB["entry_error"])


_REAL_VERIFY_ENTRY = live._verify_packaged_entry


def _stub_checkout_root():
    return Path(_checkout["root"])


live._desktop_packaged_executable = _stub_packaged_executable
live._desktop_exe_integrity_error = _stub_exe_integrity_error
live._desktop_build_needed = _stub_build_needed
live._verify_packaged_entry = _stub_verify_packaged_entry
live.checkout_root = _stub_checkout_root


def _desktop(root):
    return str(Path(root) / "apps" / "desktop")


def expected(r):
    f = r.get("func")
    if f == "asar_header_check":
        fields = [int(r.get(k) or 0) for k in ("size", "header_size", "payload_size",
                                               "json_size", "file_size")]
        size, hs, ps, js, fs = fields
        blob = struct.pack("<4I", size, hs, ps, js)
        json_bytes = b"{}".ljust(max(js, 2), b" ")[:max(js, 0)] if js > 0 else b""
        blob += json_bytes
        blob = blob.ljust(max(fs, 0), b"\0")[:max(fs, 0)]
        message = _packaged_entry_error(_Resources(blob, fs))
        return "invalid ASAR header" if "invalid ASAR header" in message else ""
    if f == "asar_entry_path_check":
        resources, _hs = _entry_archive(str(r.get("name", "")), 4, 0, False, None, False)
        message = _packaged_entry_error(resources)
        if "invalid ASAR entry path" in message:
            return "invalid ASAR entry path"
        return ""
    if f == "asar_entry_check":
        name = str(r.get("name", ""))
        resources, _hs = _entry_archive(name, int(r.get("length") or 0), int(r.get("offset") or 0),
                                        bool(r.get("unpacked")), r.get("file_size"),
                                        bool(r.get("data_blank")), r.get("header_size"))
        message = _packaged_entry_error(resources)
        for verdict in ("invalid ASAR entry path", "empty ASAR entry: " + name,
                        "truncated ASAR entry: " + name, "incomplete ASAR entry: " + name):
            if verdict in message:
                return verdict
        return ""
    if f == "renderer_has_module_entry":
        message = _packaged_entry_error(_renderer_resources(r.get("html") or ""))
        if "renderer has no local module entry" in message:
            return 0
        if message:
            raise AssertionError("unexpected archive error: %s" % (message,))
        return 1
    if f == "root_to_verify":
        root = r.get("project_root")
        STUB["packaged"] = {_desktop(root or ""): (root if r.get("has_packaged") else None)}
        _checkout["root"] = r.get("checkout_root") or ""
        return str(live._root_to_verify(None if root is None else Path(root)))
    if f == "verify":
        packaged = dict(r.get("packaged") or {})
        STUB["packaged"] = {_desktop(root): exe for root, exe in packaged.items()}
        STUB["integrity"] = {}
        for root, exe in packaged.items():
            if exe and r.get("integrity_error"):
                STUB["integrity"][str(Path(exe))] = r["integrity_error"]
        STUB["entry_error"] = r.get("entry_error") or ""
        STUB["build_needed"] = {str(root): bool(r.get("build_needed")) for root in packaged}
        _checkout["root"] = r.get("checkout_root") or ""
        project_root = r.get("project_root")
        try:
            live.verify_windows_desktop_update(None if project_root is None else Path(project_root))
        except Exception as exc:  # noqa: BLE001
            return {"status": "error", "type": type(exc).__name__, "message": str(exc)}
        return {"status": "ok"}
    raise AssertionError("unknown func %r" % (f,))


def main():
    cases = mism = 0
    wanted = {"asar_header_check", "asar_entry_path_check", "asar_entry_check",
              "renderer_has_module_entry", "root_to_verify", "verify"}
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") not in wanted:
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
