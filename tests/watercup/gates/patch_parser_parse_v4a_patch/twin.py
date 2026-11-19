"""twin for tools/patch_parser.py:parse_v4a_patch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/patch_parser.py"
    PY_FN = "parse_v4a_patch"
    C_FN = "patch_parser_parse_v4a"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
