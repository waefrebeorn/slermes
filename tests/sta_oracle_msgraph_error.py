#!/usr/bin/env python3
"""
sta_oracle_msgraph_error.py — oracle for t_port_msgraph_error.c.

Reference implementation of the Graph error-extraction contract:
  - parse the JSON body;
  - return error_description if present;
  - else error if present;
  - else the raw body;
  - empty body -> "unknown error".
Emits one JSON line per case (same shape as the C harness). The runner diffs
them. There is no single canonical Python def for this helper, so this is a
behavior-contract oracle (the C function is verified against the documented
extraction contract, not a frozen value).
"""
import sys
import json as _json

def extract_error(body):
    if not body or not body.strip():
        return "unknown error"
    try:
        obj = _json.loads(body)
    except Exception:
        return body
    if isinstance(obj, dict):
        d = obj.get("error_description")
        if isinstance(d, str) and d:
            return d
        e = obj.get("error")
        if isinstance(e, str) and e:
            return e
    return body

def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: sta_oracle_msgraph_error.py <bodies.txt>\n")
        return 2
    with open(sys.argv[1], "rb") as f:
        raw = f.read()
    text = raw.decode("utf-8")

    docs = []
    cur = []
    for line in text.split("\n"):
        if line.strip() == "===":
            docs.append("\n".join(cur)); cur = []
        else:
            cur.append(line)
    docs.append("\n".join(cur))

    for doc in docs:
        doc = doc.strip()
        if not doc:
            continue
        out = extract_error(doc)
        sys.stdout.write(
            '{"body":%s,"out":%s}\n' % (_json.dumps(doc), _json.dumps(out))
        )
    return 0

if __name__ == "__main__":
    sys.exit(main())
