"""twin for hermes_cli/skin_engine.py:get_active_prompt_symbol (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/skin_engine.py"
    PY_FN = "get_active_prompt_symbol"
    C_FN = "hermes_cli_skin_engine_get_active_prompt_symbol"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
