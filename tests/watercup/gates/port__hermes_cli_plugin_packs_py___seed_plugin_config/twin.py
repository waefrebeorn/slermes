"""twin for hermes_cli/plugin_packs.py:_seed_plugin_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/plugin_packs.py"
    PY_FN = "_seed_plugin_config"
    C_FN = "_seed_plugin_config"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
