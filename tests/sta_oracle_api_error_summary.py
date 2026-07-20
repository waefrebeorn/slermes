#!/usr/bin/env python3
"""
sta_oracle_api_error_summary.py — Python oracle for summarize_api_error().

Ground truth: run_agent.AIAgent._summarize_api_error, applied to the SAME raw
string the C harness receives. We model the string-input branches the C port
mirrors (the only branches reachable from a plain string — not an SDK exception
object carrying .body/.response). This matches the ported surface 1:1.

Behavioral parity, not snapshot: we compare the produced one-liner for each
input class (malformed stream, Cloudflare HTML+title+ray+status, JSON error
object, JSON error string, raw truncate).
"""
import sys
import json
import re


def _summarize(raw: str) -> str:
    if not raw:
        return "Unknown API error"
    raw_len = len(raw)
    if raw_len > 8192:
        raw = raw[:8192]

    # malformed streaming
    low = raw.lower()
    if "expected ident at line" in low:
        return f"Malformed provider streaming response: {raw[:300]}"

    # Cloudflare / proxy HTML
    if "<!DOCTYPE" in raw or "<html" in raw or "<HTML" in raw:
        m = re.search(r"<title[^>]*>([^<]+)</title>", raw, re.IGNORECASE)
        title = m.group(1).strip() if m else "HTML error page (title not found)"
        ray = re.search(r"Cloudflare Ray ID:\s*<strong[^>]*>([^<]+)</strong>", raw)
        ray_id = ray.group(1).strip() if ray else None
        status_code = None
        parts = []
        if status_code:
            parts.append(f"HTTP {status_code}")
        parts.append(title)
        if ray_id:
            parts.append(f"Ray {ray_id}")
        return " — ".join(parts)

    # JSON body errors (object with error.message / error.code / error.type,
    # or error as string)
    try:
        body = json.loads(raw)
    except (json.JSONDecodeError, TypeError):
        body = None
    if isinstance(body, dict):
        err = body.get("error")
        if isinstance(err, dict):
            msg = err.get("message")
            code = err.get("code")
            typ = err.get("type")
            if isinstance(code, str):
                if isinstance(msg, str):
                    prefix = f"[{code}] {typ if isinstance(typ, str) else 'error'}: "
                    return (prefix + msg[:400])[:400]
                return "API error (see logs)"
            if isinstance(msg, str):
                return msg[:400]
            return "API error (see logs)"
        if isinstance(err, str):
            return err[:400]

    # raw truncate fallback (500 chars, break at first newline < 400)
    if len(raw) > 400:
        nl = raw.find("\n")
        if 0 <= nl < 400:
            return raw[:nl]
        return raw[:400]
    return raw


def main():
    path = sys.argv[1]
    out = {"results": []}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            out["results"].append({"input": line, "summary": _summarize(line)})
    sys.stdout.write(json.dumps(out, ensure_ascii=False, separators=(",", ":")))


if __name__ == "__main__":
    main()
