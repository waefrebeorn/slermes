"""twin for agent/account_usage.py:AccountUsageSnapshot.available (a PROPERTY).

The shared walker cannot call this one: `available` needs a receiver, and the
harness's input text ("windows,details,reason") is the C struct's field set, not
an argument list. So the twin builds the SAME snapshot the harness builds and
reads the property off it -- the parse below mirrors harness.c field for field.
"""
from watercup_twin_runtime import TwinBase


def _int(text):
    try:
        return max(0, int(str(text).strip() or 0))
    except ValueError:
        return 0


class _Twin(TwinBase):
    PY_MODULE = "agent/account_usage.py"
    PY_FN = "AccountUsageSnapshot.available"
    C_FN = "account_usage_available"

    def call(self, inp, inputs, idx=0):
        cls = self._mod.AccountUsageSnapshot
        parts = str(inp).split(",")
        while len(parts) < 3:
            parts.append("NULL")
        n_windows, n_details, reason = _int(parts[0]), _int(parts[1]), parts[2]
        snap = cls(
            provider="",
            source="",
            fetched_at=self._mod.datetime.now(self._mod.UTC),
            windows=tuple(object() for _ in range(n_windows)),
            details=tuple("" for _ in range(n_details)),
            unavailable_reason=None if reason.strip() == "NULL" else reason,
        )
        return snap.available

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
