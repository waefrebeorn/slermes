"""twin for hermes_cli/input_sanitize.py:sanitize_user_prompt_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/input_sanitize.py"
    PY_FN = "sanitize_user_prompt_text"
    C_FN = "sanitize_user_prompt_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
