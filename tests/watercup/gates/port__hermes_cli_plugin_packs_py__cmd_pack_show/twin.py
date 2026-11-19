"""twin for hermes_cli/plugin_packs.py:cmd_pack_show (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_packs.py"
    PY_FN = "cmd_pack_show"
    C_FN = "cmd_pack_show"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
