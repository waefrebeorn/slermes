"""Python reference for web_server auth helpers (port of port_web_server_auth.c).

This module mirrors the C port exactly so the oracle runner can verify
behavioural parity. The C port and this reference are both derived from
the canonical Python logic in hermes_cli/web_server.py.
"""

import hmac
from typing import List, Tuple, Any, Optional

# Shared session token (mirrors g_session_token in web_dashboard.c)
_session_token = ""

def _set_session_token(token: str) -> None:
    """Set the session token for validation (called by C test setup)."""
    global _session_token
    _session_token = token


def ws_has_valid_session_token(headers: str) -> bool:
    """Port of _has_valid_session_token(request).

    Validates the dedicated session header (X-Hermes-Session-Token) with
    constant-time compare, then falls back to the legacy "Bearer <token>"
    Authorization form. Mirrors Python's hmac.compare_digest, which compares
    the ENTIRE header value — so trailing junk (e.g. "token extra") is
    rejected, not just whitespace-trimmed.
    """
    global _session_token
    if not headers or not _session_token:
        return False
    toklen = len(_session_token)
    if toklen == 0:
        return False

    # Dedicated session header (case-insensitive name).
    hdr_start = headers.lower().find("x-hermes-session-token:")
    if hdr_start != -1:
        hdr = headers[hdr_start:]
        colon = hdr.find(':')
        if colon != -1:
            hdr = hdr[colon + 1:]
            while hdr and hdr[0] == ' ':
                hdr = hdr[1:]
            # Value runs to end-of-line; reject any trailing junk.
            end = 0
            while end < len(hdr) and hdr[end] not in '\r\n':
                end += 1
            vlen = end
            if vlen == toklen and hmac.compare_digest(hdr[:vlen], _session_token):
                return True

    # Legacy Bearer form in Authorization.
    auth_start = headers.lower().find("authorization:")
    if auth_start != -1:
        auth = headers[auth_start:]
        colon = auth.find(':')
        if colon != -1:
            auth = auth[colon + 1:]
            while auth and auth[0] == ' ':
                auth = auth[1:]
            if auth.startswith("Bearer "):
                auth = auth[7:]
                end = 0
                while end < len(auth) and auth[end] not in '\r\n':
                    end += 1
                vlen = end
                if vlen == toklen and hmac.compare_digest(auth[:vlen], _session_token):
                    return True

    return False


def ws_should_require_auth(host: str, allow_public: bool) -> bool:
    """Port of should_require_auth(host, allow_public).

    Loopback binds are trusted (no gate); any non-loopback bind always
    requires an auth provider. `allow_public` is accepted for backward-
    compat but intentionally ignored — a non-loopback bind ALWAYS engages
    the gate.
    """
    if not host:
        return True
    loopback = {"localhost", "127.0.0.1", "::1"}
    return host.lower() not in loopback


def ws_is_accepted_host(host_header: str, bound_host: str) -> bool:
    """Port of _is_accepted_host(host_header, bound_host).

    DNS-rebinding defence (GHSA-ppp5-vxwm-4cf7): reject Host headers that
    don't target the bound interface. 0.0.0.0 / :: binds opt into
    all-interfaces (no Host-layer defence possible); loopback binds accept
    the loopback aliases; explicit non-loopback binds require exact host
    match.
    """
    if not host_header or not bound_host:
        return False

    # Strip port suffix, handling IPv6 bracket notation ([::1]:9119).
    h = host_header.strip()
    if h.startswith('['):
        # IPv6 bracketed — port (if any) follows "]:"
        close = h.find(']')
        if close != -1:
            host_only = h[1:close]
        else:
            host_only = h.strip('[]')
    else:
        host_only = h.rsplit(':', 1)[0] if ':' in h else h
    host_only = host_only.lower()

    # 0.0.0.0 / :: -> operator opted into all-interfaces.
    if bound_host.lower() in {"0.0.0.0", "::"}:
        return True

    # Loopback bind: accept the loopback aliases.
    bound_lc = bound_host.lower()
    loopback = {"localhost", "127.0.0.1", "::1"}
    if bound_lc in loopback:
        return host_only in loopback

    # Explicit non-loopback bind: require exact host match.
    return host_only == bound_lc


if __name__ == "__main__":
    # Quick smoke test
    _set_session_token("test-token-123")
    assert ws_should_require_auth("localhost", False) == False
    assert ws_should_require_auth("127.0.0.1", False) == False
    assert ws_should_require_auth("::1", False) == False
    assert ws_should_require_auth("0.0.0.0", False) == True
    assert ws_should_require_auth("192.168.1.5", False) == True
    assert ws_should_require_auth("example.com", True) == True

    assert ws_is_accepted_host("localhost", "localhost") == True
    assert ws_is_accepted_host("localhost:9119", "localhost") == True
    assert ws_is_accepted_host("127.0.0.1", "localhost") == True
    assert ws_is_accepted_host("evil.test", "localhost") == False
    assert ws_is_accepted_host("0.0.0.0", "0.0.0.0") == True
    assert ws_is_accepted_host("evil.test", "0.0.0.0") == True
    assert ws_is_accepted_host("10.0.0.5", "10.0.0.5") == True
    assert ws_is_accepted_host("10.0.0.6", "10.0.0.5") == False
    assert ws_is_accepted_host("[::1]:9119", "localhost") == True
    assert ws_is_accepted_host("[::1]", "localhost") == True
    assert ws_is_accepted_host("", "localhost") == False

    assert ws_has_valid_session_token(
        "X-Hermes-Session-Token: test-token-123\r\n"
    ) == True
    assert ws_has_valid_session_token(
        "x-hermes-session-token: test-token-123\r\n"
    ) == True
    assert ws_has_valid_session_token(
        "X-Hermes-Session-Token: test-token-123 extra\r\n"
    ) == False
    assert ws_has_valid_session_token(
        "Authorization: Bearer test-token-123\r\n"
    ) == True
    assert ws_has_valid_session_token(
        "Authorization: Bearer ***"
    ) == False
    assert ws_has_valid_session_token(
        "X-Hermes-Session-Token: other\r\n"
    ) == False

    print("All smoke tests passed!")