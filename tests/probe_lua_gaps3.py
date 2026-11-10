#!/usr/bin/env python3
"""Third-pass probe: nail down multi-return, varargs and generic-for using only
`local f = function` (so the `local function` parser bug cannot mask them)."""
import glob
import os
import subprocess
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(tempfile.gettempdir(), "slv_probe3.exe")

PROBES = [
    # multiple returns
    ("mr_local3",     "local f = function() return 1, 2, 3 end local a, b, c = f() print(a, b, c)", "1\t2\t3"),
    ("mr_local2",     "local f = function() return 1, 2 end local a, b = f() print(a, b)", "1\t2"),
    ("mr_print",      "local f = function() return 1, 2, 3 end print(f())", "1\t2\t3"),
    ("mr_table",      "local f = function() return 1, 2 end local t = {f()} print(#t)", "2"),
    ("mr_concat",     "local f = function() return 'a', 'b' end print(f())", "a\tb"),
    ("mr_nested",     "local f = function() return 1, 2 end local g = function() return f() end print(g())", "1\t2"),
    ("mr_tail",       "local f = function() return 1, 2 end local a, b = f(), 9 print(a, b)", "1\t9"),
    ("mr_in_call",    "local f = function() return 1, 2 end local function g(x, y) return x + y end print(g(f()))", "3"),
    # varargs
    ("va_local",      "local f = function(...) local a, b = ... return a + b end print(f(3, 4))", "7"),
    ("va_count",      "local f = function(...) return select('#', ...) end print(f(1, 2, 3))", "3"),
    ("va_table",      "local f = function(...) local t = {...} return #t end print(f(1, 2, 3))", "3"),
    ("va_pack",       "local f = function(...) local t = table.pack(...) return t.n end print(f(1, 2))", "2"),
    # generic for
    ("gf_three",      "local it = function(s, i) if i < 3 then return i + 1 end end for i in it, nil, 0 do io.write(i) end", "123"),
    ("gf_ipairs",     "local t = {10, 20} local s = 0 for i, v in ipairs(t) do s = s + v end print(s)", "30"),
    ("gf_pairs",      "local t = {a = 1, b = 2} local s = 0 for k, v in pairs(t) do s = s + v end print(s)", "3"),
    ("gf_break",      "for i = 1, 10 do if i > 3 then break end io.write(i) end", "123"),
    # method / colon
    ("colon_def",     "local t = {v = 5} function t:get() return self.v end print(t:get())", "5"),
    ("colon_call",    "local s = 'abc' print(s:upper())", "ABC"),
    ("colon_def_dot", "local t = {v = 5} function t.get(self) return self.v end print(t.get(t))", "5"),
    ("colon_call_num","local n = 10 print(n:byte and 1 or 2)", "2"),
    # call result as operand (the big one)
    ("call_binop",    "local f = function() return 7 end print(f() + 1)", "8"),
    ("call_binop2",   "local f = function() return 7 end local x = f() + 1 print(x)", "8"),
    ("call_compare",  "local f = function() return 7 end print(f() > 1)", "true"),
    ("call_concat",   "local f = function() return 'x' end print('a' .. f())", "ax"),
    ("call_index",    "local t = {} local f = function() return 1 end t[f()] = 9 print(t[1])", "9"),
    ("call_len",      "local f = function() return 'abc' end print(#f())", "3"),
    ("call_neg",      "local f = function() return 3 end print(-f())", "-3"),
    ("call_paren",    "local f = function() return 3 end print((f()))", "3"),
    ("call_tonum",    "print(tonumber('42') + 1)", "43"),
    ("call_tostr",    "print('x=' .. tostring(9))", "x=9"),
    ("call_strsub",   "print(string.sub('hello', 2, 3))", "el"),
    ("call_strrep",   "print(string.rep('a', 3))", "aaa"),
    ("call_math",     "print(math.abs(-5))", "5"),
    ("call_min",      "print(math.min(3, 1, 2))", "1"),
    # string escapes
    ("esc_n",         "print(#'a\\nb')", "3"),
    ("esc_t",         "print(#'a\\tb')", "3"),
    ("esc_sq",        "print#'a\\'b'", "3"),
    ("esc_dq",        'print#"a\\"b"', "3"),
    ("esc_bs",        "print#'a\\\\b'", "3"),
    ("esc_zero",      "print#'a\\0b'", "3"),
    ("esc_hex",       "print#'a\\x41b'", "4"),
    ("esc_dec",       "print#'a\\65b'", "4"),
]


def build():
    src = os.path.join(REPO, "tests", "slv_probe_main.c")
    lv = os.path.join(REPO, "src", "lv")
    lua = os.path.join(REPO, "src", "lua")
    objs = sorted(glob.glob(os.path.join(lv, "slv_*.c")))
    cmd = (["gcc", "-O2", "-g", "-Wall", "-Werror=implicit-function-declaration",
            "-I", lua, "-I", lv] + objs +
           [os.path.join(lua, "slv_ui.c"), src, "-o", HARNESS, "-lm"])
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("build FAILED:\n" + r.stderr[:2000])
        raise SystemExit(2)


def run(code):
    with tempfile.NamedTemporaryFile("w", suffix=".lua", delete=False) as f:
        f.write(code + "\n")
        path = f.name
    try:
        r = subprocess.run([HARNESS, path], capture_output=True, text=True, timeout=20)
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    finally:
        os.unlink(path)
    out = r.stdout.strip().replace("\r", "")
    if r.returncode != 0:
        lines = (r.stderr or out).strip().splitlines()
        return False, lines[0] if lines else f"rc={r.returncode}"
    return True, out


def main():
    build()
    ok_n = 0
    gaps = []
    for name, code, want in PROBES:
        ok, detail = run(code)
        mark = "ok  " if ok else "GAP "
        print(f"{mark} {name:16s} want={want!r:12s} got={detail!r}")
        if ok:
            ok_n += 1
        else:
            gaps.append((name, code, detail))
    print(f"\n=== {ok_n}/{len(PROBES)} pass, {len(gaps)} gaps ===")


if __name__ == "__main__":
    main()
