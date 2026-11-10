#!/usr/bin/env bash
# Restore LF endings on files this session converted to CRLF.
# Python cannot spawn git.exe on this box (WinError 5), so the comparison runs
# entirely in the shell.
set -uo pipefail
cd "C:/Users/eman5/slermes" || exit 1
REF="${1:-e6edcde}"

fixed=0
for f in $(git diff --name-only "$REF"..HEAD); do
    case "$f" in
        *.c|*.h|*.py|*.sh|*.md|*.mk|*.txt) ;;
        *) continue ;;
    esac
    [ -f "$f" ] || continue
    # current file must contain CRLF
    grep -q $'\r' "$f" || continue
    # reference version must be LF-only
    if git show "$REF:$f" 2>/dev/null | grep -q $'\r'; then
        continue   # originally CRLF, leave alone
    fi
    # strip CRLF -> LF
    sed -i 's/\r$//' "$f"
    fixed=$((fixed+1))
    echo "  LF restored: $f"
done
echo "total restored: $fixed"
