"""twin for tools/write_approval.py:_frontmatter_description (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/write_approval.py"
    PY_FN = "_frontmatter_description"
    C_FN = "_frontmatter_description"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
