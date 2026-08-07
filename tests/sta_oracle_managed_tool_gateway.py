"""Oracle for tools/managed_tool_gateway.py pure helpers.

Tests managed_vendor_base_path, managed_vendor_upload_path,
is_managed_nous_gateway_url, managed_gateway_auth_headers,
_read_user_token_override — against LIVE Python source.
"""
import json, os, sys

DEV_ROOT = os.environ.get("HERMES_DEV_ROOT") or os.path.expanduser("~/.hermes/hermes-agent")
if DEV_ROOT not in sys.path:
    sys.path.insert(0, DEV_ROOT)

from tools.managed_tool_gateway import (
    managed_vendor_base_path,
    managed_vendor_upload_path,
    is_managed_nous_gateway_url,
    managed_gateway_auth_headers,
    _read_user_token_override,
)


def run(c):
    op = c.get("op")
    if op == "managed_vendor_base_path":
        return managed_vendor_base_path(c.get("vendor", ""))
    if op == "managed_vendor_upload_path":
        return managed_vendor_upload_path(c.get("vendor", ""))
    if op == "is_managed_nous_gateway_url":
        url = c.get("url", "")
        # Gateway builder defaults to build_vendor_gateway_url
        return is_managed_nous_gateway_url(url)
    if op == "managed_gateway_auth_headers":
        url = c.get("url", "")
        headers = managed_gateway_auth_headers(url)
        if headers and "Authorization" in headers:
            return headers["Authorization"]
        return None
    if op == "_read_user_token_override":
        r = _read_user_token_override()
        return r if r else None
    return None


def main():
    with open(sys.argv[1]) as f:
        cases = json.load(f)
    for c in cases:
        r = run(c)
        if r is None:
            print("none")
        elif isinstance(r, bool):
            print("true" if r else "false")
        elif isinstance(r, (int, float)):
            if isinstance(r, float) and r == int(r):
                print(int(r))
            else:
                print(r)
        elif isinstance(r, str):
            print(r)
        else:
            print(json.dumps(r, sort_keys=True))


if __name__ == "__main__":
    main()
