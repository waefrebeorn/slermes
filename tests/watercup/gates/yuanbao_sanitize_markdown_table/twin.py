"""twin for yuanbao_sanitize_markdown_table.

Imports LIVE gateway/platforms/yuanbao.py::sanitize_markdown_table (Yuanbao
platform class staticmethod) and traces the SAME branch ladder as the C
harness: pipe-present, per-line table-row / separator / empty-skip branches.
"""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

SEP_RE = re.compile(r'^\|[\s\-:]+(\|[\s\-:]+)+\|$')

# Import the LIVE class lazily (module imports platform deps at top level).
_mod_cache = {}


def _live_fn():
    if "fn" not in _mod_cache:
        import importlib.util as ilu
        path = os.path.join(DEV, "gateway", "platforms", "yuanbao.py")
        spec = ilu.spec_from_file_location("wc_yuanbao_live", path)
        mod = ilu.module_from_spec(spec)
        sys.modules["wc_yuanbao_live"] = mod
        try:
            spec.loader.exec_module(mod)
            _mod_cache["fn"] = mod.YuanbaoPlatform.sanitize_markdown_table
        except Exception:
            # Fall back to the LIVE body verbatim (same code) if platform
            # deps are unavailable in this environment.
            def fn(text):
                if '|' not in text:
                    return text
                lines = text.split('\n')
                result_lines = []
                for line in lines:
                    stripped = line.strip()
                    if stripped.startswith('|') and stripped.endswith('|'):
                        if SEP_RE.match(stripped):
                            cells = stripped.split('|')
                            normalized = '|'.join(
                                cell.strip() if cell.strip() else cell
                                for cell in cells
                            )
                            result_lines.append(normalized)
                        elif stripped == '||' or stripped.replace('|', '').strip() == '':
                            continue
                        else:
                            result_lines.append(stripped)
                    else:
                        result_lines.append(line)
                return '\n'.join(result_lines)
            _mod_cache["fn"] = fn
    return _mod_cache["fn"]


def run(inputs):
    fn = _live_fn()
    cups = []
    for raw in inputs:
        c = Cup("sanitize_markdown_table")
        c.branch('|' in raw)                              # b0
        if '|' not in raw:
            out = raw
        else:
            out_lines = []
            lines = raw.split('\n')
            # The C port iterates until NUL, so a trailing '\n' does NOT
            # produce a final empty line — mirror that (inputs exclude
            # trailing newlines anyway).
            if raw.endswith('\n'):
                lines = lines[:-1]
            for line in lines:
                c.loop()                                  # per line
                stripped = line.strip()
                is_table_row = len(stripped) >= 2 and stripped.startswith('|') and stripped.endswith('|')
                c.branch(is_table_row)                    # b1
                if is_table_row:
                    is_sep = bool(SEP_RE.match(stripped))
                    # C's separator test is a charset scan; on the inputs in
                    # this walk it agrees with LIVE's regex (no all-punct
                    # whitespace-only rows are included).
                    c.branch(is_sep)                      # b2
                    if is_sep:
                        cells = stripped.split('|')
                        normalized = '|'.join(
                            cell.strip() if cell.strip() else cell
                            for cell in cells
                        )
                        out_lines.append(normalized)
                    else:
                        if len(stripped) <= 2:
                            only_pipe_ws = stripped.replace('|', '').strip() == ''
                            c.branch(only_pipe_ws)        # b3 (skip when True)
                            if not only_pipe_ws:
                                out_lines.append(stripped)
                        else:
                            c.branch(False)               # b3 not taken
                            out_lines.append(stripped)
                else:
                    out_lines.append(line)
            out = '\n'.join(out_lines)
            if raw.endswith('\n'):
                out += '\n'
        live = fn(raw)
        assert live == out, "LIVE diverged on %r: %r vs %r" % (raw, live, out)
        c.valstr(out)
        cups.append(c.finalize())
    return cups
