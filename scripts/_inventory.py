import os, re, subprocess

ROOT = os.getcwd()
SRC = os.path.join(ROOT, 'src')

def git_ls():
    out = subprocess.run(['git','ls-files'], capture_output=True, text=True).stdout
    return [l.strip() for l in out.splitlines() if (l.strip().endswith('.c') or l.strip().endswith('.h') or l.strip().endswith('.inc'))]

tracked = git_ls()
# restrict to src/
src_files = [f for f in tracked if f.startswith('src/')]

# 1) remaining port_ prefixed files
port_files = [f for f in src_files if '/port_' in f or os.path.basename(f).startswith('port_')]
print("=== remaining 'port_'-prefixed files:", len(port_files))

# 2) src/cli vs src/hermes_cli
cli = sorted([f for f in src_files if f.startswith('src/cli/')])
hcli = sorted([f for f in src_files if f.startswith('src/hermes_cli/')])
cli_base = set(os.path.basename(f) for f in cli)
hcli_base = set(os.path.basename(f) for f in hcli)
overlap = cli_base & hcli_base
print("src/cli files:", len(cli), "| src/hermes_cli files:", len(hcli), "| basename overlap:", len(overlap))
print("  overlap sample:", sorted(overlap)[:20])

# 3) src/ports remaining
ports = sorted([f for f in src_files if f.startswith('src/ports/')])
print("src/ports files still tracked:", len(ports))
for f in ports[:40]: print("   ", f)

# 4) Are src/ports built? check objects.mk
mk = open('build/objects.mk').read()
ports_built = [f for f in ports if (f[:-2]+'.o') in mk]
print("  of those, referenced in objects.mk:", len(ports_built))

print()
print("=== sample remaining port_ files ===")
for f in sorted(port_files)[:60]: print("   ", f)
