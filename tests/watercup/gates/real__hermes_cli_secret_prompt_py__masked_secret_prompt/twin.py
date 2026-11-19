"""twin for hermes_cli/secret_prompt.py:masked_secret_prompt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/secret_prompt.py"
    PY_FN = "masked_secret_prompt"
    C_FN = "masked_secret_prompt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
