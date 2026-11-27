"""twin for hermes_cli/codex_runtime_switch.py:check_codex_binary_ok.

The generated twin mixed the whole (ok, message) tuple as one string while the C
harness mixed only the int, so the cups could never agree -- a shape artefact that
read as a parity failure. This twin mixes exactly what harness.c mixes: the flag,
then the message, in that order.

The oracle is the LIVE Python function, which shells out to the binary the same way
the port does, so both sides answer "not found" for a name that is not installed.
"""
from watercup_py import Cup
from watercup_twin_runtime import TwinBase


class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/codex_runtime_switch.py"
    PY_FN = "check_codex_binary_ok"
    C_FN = "hermes_cli_codex_runtime_switc_check_codex_binary_ok"

    def trace(self, inp, inputs, idx=0):
        label = getattr(self, "_cup_label", None) or self.C_FN
        c = Cup(label)
        fn = self._fn
        arg = "" if inp is None else str(inp)
        try:
            res = fn(arg if arg else "codex")
        except Exception as exc:                      # pragma: no cover
            c.valstr("0")
            c.valstr("codex check failed: %s" % exc)
            return c.finalize()
        if isinstance(res, tuple):
            ok, msg = res[0], res[1]
        else:
            ok, msg = bool(res), ""
        c.valstr("1" if ok else "0")
        c.valstr(str(msg))
        return c.finalize()

    def norm(self, c, result):
        if isinstance(result, tuple):
            c.valstr("1" if result[0] else "0")
            c.valstr(str(result[1]))
        else:
            c.valstr("1" if result else "0")


_t = _Twin()
trace = _t.trace
run = _t.run
