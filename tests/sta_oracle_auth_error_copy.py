#!/usr/bin/env python3
"""
sta_oracle_auth_error_copy.py — Oracle for hermes_cli/auth_error_copy.py.

Rebuilds the equivalent exception for each harness case, replays the LIVE
module, and diffs the printed lines / device-flow copy / removed-provider hint.
"""
import json
import subprocess
import sys

sys.path.insert(0, "C:/Users/eman5/hermes-agent")
import hermes_cli.auth_error_copy as AEC  # noqa: E402


class ConnectError(Exception):
    pass


CASES = {
    "cancelled": KeyboardInterrupt(),
    "eof": EOFError(),
    "sysexit130": SystemExit(130),
    "sysexit_none": SystemExit(None),
    "sysexit3": SystemExit(3),
    "network": ConnectError("connection refused"),
    "network_stdlib": ConnectionResetError("reset by peer"),
    "generic": RuntimeError("boom"),
    "generic_empty": RuntimeError(""),
    "signin_copy": AEC.SignInCopyError(
        "The sign-in code expired.\n  Details: expired_token: gone",
        oauth_error_code="expired_token"),
    "signin_copy_single": AEC.SignInCopyError("Sign-in did not complete."),
    "device_expired": AEC.DeviceCodeExpired("device code timed out"),
    "device_expired_code2": type("AuthError", (Exception,), {"code": "timeout"})("timed out"),
}

REMOVED = {"opencode-free", "free", "opencode_free"}


def main() -> int:
    try:
        sys.stdin.reconfigure(encoding="utf-8", errors="replace")
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

    cases = []
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            cases.append(json.loads(line))
        except Exception:
            pass
    if not cases:
        print("ORACLE auth_error_copy: 0 cases (link failure in disguise)")
        return 1

    mism = 0
    checked = 0
    for c in cases:
        fam = c["family"]
        # note_sign_in_failure touches shared metrics; stub it so the oracle
        # measures copy, not observability side effects.
        if fam == "signin":
            AEC.note_sign_in_failure = None  # replaced below
            import hermes_cli.observability.shared_metrics_setup as SMS
            SMS.note_sign_in_failure = lambda exc: None
            host = "example.test" if c["case"] == "custom_host" else "portal.nousresearch.com"
            retry = "hermes login" if c["case"] == "custom_host" else "hermes portal"
            exc = CASES["network"] if c["case"] == "custom_host" else CASES[c["case"]]
            want = AEC.sign_in_failure_lines(exc, service_host=host, retry_command=retry)
        elif fam == "provider":
            exc = CASES[c["case"]]
            want = AEC.provider_setup_failure_lines(exc, retry_command="hermes model")
        elif fam == "device":
            code, desc = {
                "device_expired_token": ("expired_token", "code expired"),
                "device_access_denied": ("access_denied", "user said no"),
                "device_invalid_grant": ("invalid_grant", "bad grant"),
                "device_invalid_client": ("invalid_client", "unknown app"),
                "device_unknown_code": ("weird_code", "server exploded"),
                "device_empty_code": ("", "no code given"),
            }[c["case"]]
            want = str(AEC.device_flow_error(code, desc, retry_command="hermes portal"))
        else:  # hint
            provider = c["provider"]
            if provider is None:
                continue
            if provider.strip().lower() not in REMOVED:
                continue
            want = AEC.unknown_provider_hint(provider)

        got = c.get("lines") if "lines" in c else c.get("text")
        checked += 1
        if got != want:
            mism += 1
            print(f"MISMATCH {c['case']} [{fam}]:\n  C ={got}\n  PY={want}")

    print(f"ORACLE auth_error_copy: {checked} cases checked, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
