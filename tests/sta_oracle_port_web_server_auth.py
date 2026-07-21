#!/usr/bin/env python3
"""
Oracle for port_web_server_auth — runs the C harness and the Python reference
over the shared fixture (tests/oracle/fixtures/port_web_server_auth/cases.in)
and emits one JSON envelope per case on stdout, mirroring the canonical
run_pure oracle pattern so run_oracle.sh can diff the two outputs line-by-line.

Fixture line grammar (one op per line, '|' field separator, leading '#' or
blank ignored):
  should_require_auth|<host>|<allow_public:bool>
  is_accepted_host|<host_header>|<bound_host>
  has_valid_session_token|<headers>      (headers may be ""  quoted; literal
                                          \r \n \\ \" escapes are honoured)
"""
import json
import sys
from pathlib import Path

_HERE = Path(__file__).parent
PY_REF = _HERE / "port_web_server_auth_ref.py"


def _import_ref():
    import importlib.util
    spec = importlib.util.spec_from_file_location("port_web_server_auth_ref", PY_REF)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _strip_quotes(s: str) -> str:
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        return s[1:-1]
    return s


def _unescape(s: str) -> str:
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            n = s[i + 1]
            if n == 'r':
                out.append('\r'); i += 2; continue
            if n == 'n':
                out.append('\n'); i += 2; continue
            if n == '\\':
                out.append('\\'); i += 2; continue
            if n == '"':
                out.append('"'); i += 2; continue
        out.append(c)
        i += 1
    return ''.join(out)


def _json_string(s: str) -> str:
    """Mirror the C harness's emit_json_string byte-for-byte so the runner
    can diff the two outputs without parse-then-compare re-quoting. Both
    sides escape control bytes (< 0x20) as \\u00XX, double quotes and
    backslashes with a single backslash quote. This is RFC 8259 §7."""
    out = ['"']
    for c in s:
        cp = ord(c)
        if cp == ord('"'):
            out.append('\\"')
        elif cp == ord('\\'):
            out.append('\\\\')
        elif cp < 0x20:
            out.append('\\u%04x' % cp)
        else:
            out.append(c)
    out.append('"')
    return ''.join(out)


_NONE_SENTINEL = "NONE"


def _coerce_str_arg(raw: str) -> str:
    """Map the fixture's NONE sentinel back to the empty string the way
    the C harness does (only meaningful for the two string-typed args)."""
    if raw == _NONE_SENTINEL:
        return ""
    return raw


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: %s <cases.txt>\n" % sys.argv[0])
        return 2
    ref = _import_ref()
    ref._set_session_token("test-token-123")

    def emit_op(op, args, result):
        # Build the byte string manually so it matches the C harness
        # byte-for-byte; json.dumps() would choose a different escape form
        # for control chars and unicode chars.
        parts = ['{"op":"', op, '","args":[', _json_string(args[0]),
                 ',', _json_string(args[1]) if len(args) > 1 else '""',
                 '],"result":', "true" if result else "false", "}"]
        sys.stdout.write(''.join(parts) + "\n")

    with open(sys.argv[1], "r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            op, _, rest = line.partition("|")
            if op == "should_require_auth":
                host, _, tail = rest.partition("|")
                allow_public = (tail.strip() == "true")
                host = _coerce_str_arg(host)
                result = ref.ws_should_require_auth(host, allow_public)
                emit_op(op, [host, tail.strip()], result)
            elif op == "is_accepted_host":
                host_header, _, bound_host = rest.partition("|")
                host_header = _coerce_str_arg(host_header)
                result = ref.ws_is_accepted_host(host_header, bound_host)
                emit_op(op, [host_header, bound_host], result)
            elif op == "has_valid_session_token":
                headers_raw = rest
                headers = _unescape(_strip_quotes(headers_raw))
                result = ref.ws_has_valid_session_token(headers)
                emit_op(op, [headers, ""], result)
            else:
                sys.stderr.write("unknown op: %s\n" % op)
                return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
