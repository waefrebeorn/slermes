"""twin: _parse_single_entry(event, index, raw) -> ShellHookSpec | None.

Cup projection: event|command|matcher(-=None)|timeout|fail_closed, or "None"."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/shell_hooks.py"; PY_FN = "_parse_single_entry"
    C_FN = "agent_shell_hooks_u_parse_single_entry"
    NORM_NONE = True   # C returns the literal "None" sentinel for invalid entries
    def call(self, inp, inputs, idx=0):
        d = _j.loads(inp)
        return self._fn(d.get("event", ""), 0, d.get("raw"))
    def norm(self, c, result):
        if result is None:
            c.valstr("None"); return
        m = result.matcher
        c.valstr("%s|%s|%s|%s|%d" % (result.event, result.command, "-" if m is None else m,
                                     result.timeout, int(bool(result.fail_closed))))
_t = _Twin(); trace = _t.trace; run = _t.run
