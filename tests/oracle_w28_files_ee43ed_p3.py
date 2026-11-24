#!/usr/bin/env python3
"""Differential oracle for hermes_cli/web_routers/files.py — gate
w28_files_ee43ed_p3 (query parsing, coercion, payload decoding).

The live router module is imported BY FILE PATH.  HTTP is the seam: the
proxy's socket, the multipart reader and the response object never exist here.
A ``chain`` of ``{status, location}`` stands in for the proxy's successive
responses, and ``status``/``content_type``/``size`` stand in for the fetched
response, so what is compared is exactly the policy those bytes decide.
"""
import importlib.util
import json
import os
import sys
import urllib.parse
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
GATE = "w28_files_ee43ed_p3"

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


live = _load("w28_live_files_p3", "hermes_cli/web_routers/files.py")

WANTED = {
    "decode_data_url", "chat_image_extension", "decode_chat_image_upload",
    "media_proxy_host_allowed", "media_proxy_target", "media_proxy_content_type_ok",
    "media_proxy_redirect", "proxy_response_verdict", "profile_key",
    "default_cwd_placeholder", "read_text_meta", "media_suffix_verdict",
}


def verdict(ok, status=200, detail=None, **extra):
    out = {"ok": ok, "status": status, "detail": detail}
    out.update(extra)
    return out


def decode_data_url(r):
    max_bytes = float(r.get("max_bytes") or 0)
    try:
        data, mime = live._decode_data_url(r.get("data_url") or "")
    except HTTPException as exc:
        return verdict(False, exc.status_code, exc.detail)
    if len(data) > max_bytes:
        return verdict(False, 413, "File is too large")
    return verdict(True, 200, None, mime_type=mime, data=list(data))


def decode_chat_image_upload(r):
    payload = type("P", (), {"data_url": r.get("data_url") or "", "filename": "a.png"})()
    try:
        data, mime, ext = live._decode_chat_image_upload(payload)
    except HTTPException as exc:
        return verdict(False, exc.status_code, exc.detail)
    if len(data) > float(r.get("cap") or 0):
        mb = int(float(r.get("cap") or 0)) // (1024 * 1024)
        return verdict(False, 413, "Image is too large; cap is %d MB" % mb)
    return verdict(True, 200, None, mime_type=mime, data=list(data), ext=ext)


def media_proxy_target(r):
    url = (r.get("url") or "").strip()
    try:
        parsed = urllib.parse.urlparse(url)
    except ValueError:
        return verdict(False, 400, "A remote image URL is required")
    if parsed.scheme not in ("http", "https") or not parsed.hostname:
        return verdict(False, 400, "A remote image URL is required")
    if not live._media_proxy_host_allowed(parsed.hostname):
        return verdict(False, 403, "Image host not allowed")
    path = parsed.path
    suffix = Path(path).suffix.lower() if path else ""
    if not path or suffix not in live._MEDIA_CONTENT_TYPES:
        if Path(path).suffix and suffix not in live._MEDIA_CONTENT_TYPES:
            return verdict(False, 415, "Unsupported media type")
    return verdict(True, 200, None, host=parsed.hostname,
                   suffix=Path(path).suffix.lower() if path else "")


def media_proxy_redirect(r):
    """_media_proxy_get's hop policy: <=3 follows, each on the allowlist."""
    url = r.get("url") or ""
    hops = 0
    for hop in (r.get("chain") or [])[: live._MEDIA_PROXY_MAX_REDIRECTS]:
        location = hop.get("location") or ""
        if hop.get("status") not in live._MEDIA_PROXY_REDIRECT_CODES or not location:
            break
        url = urllib.parse.urljoin(url, location)
        parsed = urllib.parse.urlparse(url)
        hops += 1
        if parsed.scheme not in ("http", "https") or not live._media_proxy_host_allowed(parsed.hostname or ""):
            return verdict(False, 403, "Image redirect host not allowed", hops=hops)
    return verdict(True, 200, None, hops=hops)


def proxy_response_verdict(r):
    if int(r.get("status", 200)) != 200:
        return verdict(False, 502, "Image fetch returned HTTP %d" % int(r.get("status")))
    content_type = (r.get("content_type") or "").split(";")[0].strip().lower()
    if content_type not in live._MEDIA_CONTENT_TYPES.values():
        return verdict(False, 415, "Unsupported media type")
    if float(r.get("size") or 0) > float(r.get("max_bytes") or 0):
        return verdict(False, 413, "File too large")
    return verdict(True, 200, None)


def read_text_meta(r):
    data = bytes(r.get("data") or [])
    size, preview = float(r.get("size") or 0), float(r.get("preview_max") or 0)
    type_path = Path(r.get("name") or "")
    suffix = type_path.suffix.lower()
    return {
        "binary": live._fs_looks_binary(data[:4096]),
        "byteSize": size,
        "truncated": size > preview,
        "language": live._FS_PREVIEW_LANGUAGE_BY_EXT.get(suffix, "text"),
        "mimeType": live._fs_mime_type(type_path),
    }


def media_suffix_verdict(r):
    suffix = (r.get("suffix") or "").lower()
    if suffix not in live._MEDIA_CONTENT_TYPES:
        return verdict(False, 415, "Unsupported media type")
    if not r.get("under_roots", True):
        return verdict(False, 403, "Path outside media roots")
    if not r.get("is_file", True):
        return verdict(False, 404, "File not found")
    if float(r.get("size") or 0) > float(r.get("max_bytes") or 0):
        return verdict(False, 413, "File too large")
    return verdict(True, 200, None, content_type=live._MEDIA_CONTENT_TYPES[suffix])


def expected(r):
    f = r.get("func")
    if f == "decode_data_url":
        return decode_data_url(r)
    if f == "chat_image_extension":
        return live._chat_image_extension(bytes(r.get("data") or []))
    if f == "decode_chat_image_upload":
        return decode_chat_image_upload(r)
    if f == "media_proxy_host_allowed":
        return live._media_proxy_host_allowed(r.get("host") or "")
    if f == "media_proxy_target":
        return media_proxy_target(r)
    if f == "media_proxy_content_type_ok":
        ct = (r.get("content_type") or "").split(";")[0].strip().lower()
        return ct in live._MEDIA_CONTENT_TYPES.values()
    if f == "media_proxy_redirect":
        return media_proxy_redirect(r)
    if f == "proxy_response_verdict":
        return proxy_response_verdict(r)
    if f == "profile_key":
        return (r.get("profile") or "current").strip().lower() or "current"
    if f == "default_cwd_placeholder":
        return str(r.get("raw") or "").strip() in {"", ".", "auto", "cwd"}
    if f == "read_text_meta":
        return read_text_meta(r)
    if f == "media_suffix_verdict":
        return media_suffix_verdict(r)
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
