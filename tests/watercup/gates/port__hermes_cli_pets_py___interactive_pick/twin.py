"""twin for hermes_cli/pets.py:_interactive_pick (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pets.py"
    PY_FN = "_interactive_pick"
    C_FN = "hermes_cli_pets_u_interactive_pick"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
