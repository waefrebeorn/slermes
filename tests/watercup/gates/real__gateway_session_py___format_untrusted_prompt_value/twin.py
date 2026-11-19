"""twin for gateway/session.py:_format_untrusted_prompt_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "_format_untrusted_prompt_value"
    C_FN = "_format_untrusted_prompt_value"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
