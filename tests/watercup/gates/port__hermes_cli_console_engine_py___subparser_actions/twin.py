"""twin for hermes_cli/console_engine.py:_subparser_actions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/console_engine.py"
    PY_FN = "_subparser_actions"
    C_FN = "ce_subparser_actions"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
