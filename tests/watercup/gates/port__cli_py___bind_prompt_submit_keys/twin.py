"""twin for cli.py:_bind_prompt_submit_keys (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_bind_prompt_submit_keys"
    C_FN = "_bind_prompt_submit_keys"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
