"""twin for cli.py:_submit_slash_confirm_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_submit_slash_confirm_response"
    C_FN = "_submit_slash_confirm_response"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
