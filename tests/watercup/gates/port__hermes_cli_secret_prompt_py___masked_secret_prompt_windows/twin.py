"""twin for hermes_cli/secret_prompt.py:_masked_secret_prompt_windows (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/secret_prompt.py"
    PY_FN = "_masked_secret_prompt_windows"
    C_FN = "hermes_cli_secret_prompt_u_masked_secret_prompt_windows"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
