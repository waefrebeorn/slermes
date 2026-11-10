#!/bin/bash
# gate_github_provider.sh — drive the stdin-consuming github_provider harness
# (reads repo lines from stdin; the no-stdin pipe runner can't drive it), feed
# a curated fixture set, pipe the JSONL into the oracle.
set -e
cd "$(dirname "$0")/.."
mkdir -p .testbin
PYBIN=$(for cand in python python3 \
  /c/Users/*/AppData/Local/hermes/tools/python-*/python.exe; do
  [ -x "$cand" ] && "$cand" -c "import hashlib;hashlib.sha256(b'x').hexdigest()" >/dev/null 2>&1 && { echo "$cand"; break; }
done)
if [ -z "$PYBIN" ]; then echo "no python with working hashlib" >&2; exit 1; fi
EXE=.testbin/tt_github_provider
# ensure the port object is fresh
make src/skills_hub.o >/dev/null 2>&1 || true
if [ ! -x "$EXE" ]; then
  # skills_hub.o drags http/net siblings; link the full object set via a
  # response file (newline-joined, as run_one_oracle does) like the FULL sentinel.
  objset=.testbin/ghp_objs.rsp
  ( find src lib -name '*.o' ! -name 'main.o' | tr '\n' ' '; echo ) > "$objset"
  gcc -w -O2 -std=gnu11 -D_GNU_SOURCE -I include/compat -I include -I src -I lib/libjson \
    tests/t_port_github_provider.c @"$objset" \
    -lstdc++ -lm -lws2_32 -lgdi32 -lcrypt32 -ladvapi32 -luser32 -lshell32 \
    -lole32 -luuid -lz -lssl -lcrypto -lSDL2main -lSDL2 -lSDL2_ttf -ltre -lpcre2-8 \
    -Wl,--allow-multiple-definition -o "$EXE"
fi
cat > ".testbin/ghp_fixtures.txt" <<'EOF'
openai/skills
anthropics/skills
huggingface/skills
nvidia/skills
voltagent/awesome-agent-skills
garrytan/gstack
minimax-ai/cli
wandb/skills
qdrant/skills
pinecone-io/skills
OPENAI/SKILLS
  openai/skills  
openai/skills 
unknown/repo
some-user/other-repo
empty
Xx/SkIlLs
EOF
SLERMES_HOME="$(pwd)/.testbin/ghp_tmp" HOME="$(pwd)/.testbin/ghp_tmp" \
  "$EXE" < ".testbin/ghp_fixtures.txt" > ".testbin/ghp_lines.txt" 2>&1
"$PYBIN" "tests/oracle_github_provider.py" < ".testbin/ghp_lines.txt"
RC=$?
rm -rf .testbin/ghp_tmp
exit $RC
