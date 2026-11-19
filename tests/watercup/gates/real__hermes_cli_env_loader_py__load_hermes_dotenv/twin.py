"""twin for hermes_cli/env_loader.py:load_hermes_dotenv (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/env_loader.py"
    PY_FN = "load_hermes_dotenv"
    C_FN = "load_hermes_dotenv"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
