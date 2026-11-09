#!/usr/bin/env python3
"""Second-pass probe: re-test the features whose first-pass failure was caused
by `local function` not parsing, using `local f = function` instead.

This separates the ONE root-cause bug from the features that are genuinely
broken on their own.
"""
import glob
import os
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(tempfile.gettempdir(), "slv_probe2.exe")

PROBES = [
    ("multi_return",     "local f = function() return 1, 2, 3 end local a, b, c = f() print(a + b + c)", "6"),
    ("multi_return_trunc","local f = function() return 1, 2 end print(f())", "1\t2"),
    ("varargs",          "local f = function(...) local a, b = ... return a + b end print(f(3, 4))", "7"),
    ("varargs_select",   "local f = function(...) return select('#', ...) end print(f(1, 2, 3))", "3"),
    ("closure",          "local counter = function() local n = 0 return function() n = n + 1 return n end end local c = counter() c() print(c())", "2"),
    ("nested_call",      "local f = function(x) return x + 1 end print(f(f(f(0))))", "3"),
    ("call_in_concat",   "local f = function() return 'z' end print('a' .. f())", "az"),
    ("fndef_local_kw",   "local function f(x) return x * 2 end print(f(21))", "42"),
    # method definition / call
    ("method_def",       "local t = {v = 5} function t.get(self) return self.v end print(t:get())", "5"),
    ("method_def_colon", "local t = {v = 5} function t:get() return self.v end print(t:get())", "5"),
    ("method_call_paren","local t = {v = 5} function t.get(self) return self.v end print(t.get(t))", "5"),
    ("method_on_paren",  "print(('abc'):upper())", "ABC"),
    ("method_on_local",  "local s = 'abc' print(s:upper())", "ABC"),
    ("method_on_literal","print(('abc').upper('abc'))", "ABC"),
    # ipairs / generic for
    ("ipairs",           "local t = {10, 20} local s = 0 for i, v in ipairs(t) do s = s + v end print(s)", "30"),
    ("ipairs_manual",    "local t = {10, 20} local s = 0 for i = 1, #t do s = s + t[i] end print(s)", "30"),
    ("generic_for",      "local function it(s, i) if i < 3 then return i + 1 end end for i in it, nil, 0 do io.write(i) end", "123"),
    ("pairs",            "local t = {a = 1} local n = 0 for k, v in pairs(t) do n = n + v end print(n)", "1"),
    # tostring / tonumber
    ("tonumber_str",     "print(tonumber('42') + 1)", "43"),
    ("tonumber_direct",  "print(tonumber('42'))", "42"),
    ("tonumber_float",   "print(tonumber('3.5'))", "3.5"),
    ("tonumber_bad",     "print(tonumber('xx'))", "nil"),
    ("tostring_int",     "print(tostring(7))", "7"),
    ("tostring_in_concat","print('x=' .. tostring(9))", "x=9"),
    ("tostring_call_var","local s = tostring(9) print('x=' .. s)", "x=9"),
    # string escapes
    ("str_escape_quote", "print#'a\\'b'", "3"),
    ("str_escape_dq",    'print#"a\\"b"', "3"),
    ("str_escape_backslash", "print#'a\\\\b'", "3"),
    ("str_escape_0",     "print#'a\\0b'", "3"),
    ("str_escape_hex",   "print#'a\\x41b'", "4"),
    # more operators / forms
    ("length_concat",    "local s = 'ab' print(#s .. 'cd')", "2cd"),
    ("concat_num",       "print(1 .. 2)", "12"),
    ("concat_call_chain","local f = function() return 'x' end print('a' .. f() .. 'b')", "axb"),
    ("call_in_call",     "local f = function(x) return x end local g = function() return 5 end print(f(g()))", "5"),
    ("call_in_index",    "local t = {} local f = function() return 1 end t[f()] = 9 print(t[1])", "9"),
    ("call_in_table",    "local f = function() return 7 end local t = {f()} print(t[1])", "7"),
    ("call_in_binop",    "local f = function() return 7 end print(f() + 1)", "8"),
    ("call_in_compare",  "local f = function() return 7 end print(f() > 1)", "true"),
    ("call_in_assign",   "local f = function() return 7 end local x = f() print(x)", "7"),
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
        sys.exit(2)


def run(code):
    with tempfile.NamedTemporaryFile("w", suffix=".lua", delete=False) as f:
        f.write(code + "\n")
        path = f.name
    try:
        r = subprocess.run([HARNESS, path],
                           capture_output=True, text=True, timeout=20)
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
        if ok:
            ok_n += 1
            print(f"ok   {name:22s} -> {detail}")
        else:
            gaps.append((name, code, detail))
            print(f"GAP  {name:22s} -> {detail}")
    print()
    print(f"=== {ok_n}/{len(PROBES)} pass, {len(gaps)} gaps ===")
    if gaps:
        print("\nTRUE GAPS (independent of `local function`):")
        for i, (name, code, detail) in enumerate(gaps, 1):
            print(f"  {i:2d}. {name}\n      lua: {code}\n      ->  {detail}")


if __name__ == "__main__":
    main()
