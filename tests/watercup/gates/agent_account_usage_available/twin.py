"""Twin for agent_account_usage_available: AccountUsageSnapshot.available.

C harness parses CSV: "windows,details,reason_or_NULL".
Mirrors Python AccountUsageSnapshot.available() logic.
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
from watercup_py import Cup
from agent.account_usage import AccountUsageSnapshot, AccountUsageWindow
from datetime import datetime

def trace(input):
    c = Cup("account_usage_available")
    if input is None or input == "NULL":
        c.valstr("false")
        return c.finalize()
    try:
        parts = [p.strip() for p in input.split(",")]
        w = int(parts[0]) if len(parts) > 0 and parts[0] else 0
        d = int(parts[1]) if len(parts) > 1 and parts[1] else 0
        reason = parts[2] if len(parts) > 2 and parts[2] and parts[2] != "NULL" else None
    except (ValueError, IndexError):
        c.valstr("false")
        return c.finalize()
    windows = tuple(AccountUsageWindow(label=f"w{i}") for i in range(w)) if w > 0 else ()
    details = tuple(f"d{i}" for i in range(d)) if d > 0 else ()
    snap = AccountUsageSnapshot(
        windows=windows,
        details=details,
        unavailable_reason=reason,
        provider="test",
        source="test",
        fetched_at=datetime(2026, 1, 1),
    )
    c.valstr("true" if snap.available else "false")
    return c.finalize()

def run(inputs):
    return [trace(inp) for inp in inputs]
