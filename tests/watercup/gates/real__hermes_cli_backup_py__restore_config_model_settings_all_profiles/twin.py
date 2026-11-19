"""twin for hermes_cli/backup.py:restore_config_model_settings_all_profiles (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/backup.py"
    PY_FN = "restore_config_model_settings_all_profiles"
    C_FN = "restore_config_model_settings_all_profiles"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
