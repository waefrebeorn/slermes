import glob, os, subprocess, tempfile
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS = os.path.join(tempfile.gettempdir(), "slv_probe5.exe")
PROBES = [
    ("for_write",     "for i = 1, 3 do io.write(i) end", "123"),
    ("for_type",      "for i = 1, 1 do print(math.type(i)) end", "integer"),
    ("for_print",     "for i = 1, 3 do print(i) end", "1"),
    ("for_arith",     "local s = 0 for i = 1, 3 do s = s + i end print(s)", "6"),
    ("for_div",       "for i = 2, 2 do print(i // 2) end", "1"),
    ("for_index",     "local t = {10, 20} local s = 0 for i = 1, 2 do s = s + t[i] end print(s)", "30"),
    ("for_frac",      "for i = 1, 2 do print(i / 2) end", "0.5"),
    ("for_neg",       "for i = 3, 1, -1 do io.write(i) end", "321"),
    ("for_float_lim", "for x = 1, 2 do io.write(x) end", "12"),
    ("while_type",    "local n = 0 while n < 1 do print(math.type(n)) n = n + 1 end", "integer"),
    ("global_int",    "g = 5 print(math.type(g))", "integer"),
    ("global_float",  "g = 5.0 print(math.type(g))", "float"),
    ("arith_int",     "print(math.type(1 + 2))", "integer"),
    ("arith_div",     "print(math.type(4 / 2))", "float"),
    ("concat_int",    "print('' .. 5)", "5"),
    ("tostring_for",  "for i = 1, 2 do io.write(tostring(i)) end", "12"),
]

def build():
    src = os.path.join(REPO, "tests", "slv_probe_main.c")
    lv = os.path.join(REPO, "src", "lv"); lua = os.path.join(REPO, "src", "lua")
    objs = sorted(glob.glob(os.path.join(lv, "slv_*.c")))
    cmd = (["gcc","-O2","-g","-Wall","-Werror=implicit-function-declaration","-I",lua,"-I",lv]
           + objs + [os.path.join(lua,"slv_ui.c"), src, "-o", HARNESS, "-lm"])
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode: print(r.stderr[:1500]); raise SystemExit(2)

def run(code):
    with tempfile.NamedTemporaryFile("w", suffix=".lua", delete=False) as f:
        f.write(code+"\n"); path=f.name
    try:
        r = subprocess.run([HARNESS, path], capture_output=True, text=True, timeout=20)
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    finally:
        os.unlink(path)
    out = r.stdout.strip().replace("\r","")
    if r.returncode:
        lines=(r.stderr or out).strip().splitlines()
        return False, lines[0] if lines else f"rc={r.returncode}"
    return True, out

build()
for name, code, want in PROBES:
    good, detail = run(code)
    mark = "ok  " if (good and detail == want) else ("WRONG" if good else "GAP ")
    print(f"{mark} {name:16s} want={want!r:10s} got={detail!r}")
