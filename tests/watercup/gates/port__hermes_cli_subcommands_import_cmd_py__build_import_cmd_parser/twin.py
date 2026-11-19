"""twin for hermes_cli/subcommands/import_cmd.py:build_import_cmd_parser (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/subcommands/import_cmd.py"
    PY_FN = "build_import_cmd_parser"
    C_FN = "hermes_cli_subcommands_import__build_import_cmd_parser"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
