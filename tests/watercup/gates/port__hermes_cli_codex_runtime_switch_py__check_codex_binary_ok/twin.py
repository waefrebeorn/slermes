"""twin for hermes_cli/codex_runtime_switch.py:check_codex_binary_ok (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/codex_runtime_switch.py"
    PY_FN = "check_codex_binary_ok"
    C_FN = "hermes_cli_codex_runtime_switc_check_codex_binary_ok"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
