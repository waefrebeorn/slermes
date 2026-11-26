"""twin for libcron_next_run — LIVE croniter, same branch ladder as harness.c.

The generated twin could not work at all: `croniter.get_next` is not a module-level
function (it is a method on a croniter instance), and the gate's fixtures were the
generic "hello"/"world" placeholders, so even a working twin would have been
NON-DISCRIMINATING. Inputs are now "expr|from_epoch" probes, and this twin emits
the SAME three WCUP_BRANCH points as the C harness (B1 @-special, B2 parse fail,
B3 next-run found) -- the shared TwinBase.branch() can only emit one.
"""
from datetime import UTC, datetime

from watercup_py import Cup
from watercup_twin_runtime import TwinBase


class _Twin(TwinBase):
    PY_MODULE = "croniter"
    PY_FN = "croniter"
    C_FN = "wc_cron_next"

    def trace(self, inp, inputs, idx=0):
        import croniter as _croniter

        label = getattr(self, "_cup_label", None) or "cron_next_run"
        c = Cup(label)
        text = str(inp)
        expr, _, from_s = text.partition("|")
        try:
            from_t = int(from_s or 0)
        except ValueError:
            from_t = 0
        c.branch(bool(expr.startswith("@") and not expr.startswith("@every ")))

        try:
            it = _croniter.croniter(expr, datetime.fromtimestamp(from_t, UTC))
        except Exception:
            c.branch(True)          # B2: parse failed
            c.valstr("-1")
            return c.finalize()

        c.branch(False)             # B2 = 0
        try:
            nxt = it.get_next(datetime)
            found = True
        except Exception:
            found = False           # impossible date / horizon
        c.branch(found)             # B3
        c.valstr("%d" % int(nxt.timestamp()) if found else "-1")
        return c.finalize()

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
