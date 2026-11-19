"""twin for hermes_cli/skin_engine.py:get_prompt_toolkit_style_overrides (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/skin_engine.py"
    PY_FN = "get_prompt_toolkit_style_overrides"
    C_FN = "hermes_cli_skin_engine_get_prompt_toolkit_style_overrides"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
