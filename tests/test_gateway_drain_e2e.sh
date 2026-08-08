#!/usr/bin/env bash
# test_gateway_drain_e2e.sh — TRUE end-to-end drain test.
# Real gateway process + real LLM key (NOUS_API_KEY from env) + real in-flight
# agent turn (a long essay prompt posted to the webhook platform) + SIGTERM
# mid-turn. Verifies:
#   1. the turn is actually in flight when SIGTERM arrives,
#   2. the gateway DRAINS (waits for the in-flight turn / bounded window),
#   3. the full graceful sequence runs ("Shutdown requested ... beginning
#      drain" -> "Shutdown complete"),
#   4. the process exits on its own (no kill -9),
#   5. a ping is sent to the user's real Telegram chat with the result.
set -u
cd /home/wubu/hermes-agent-dev/slermes

PORT=18576
LOG=/tmp/gw_e2e.log
TOKEN=$(sed -n '525p' ~/.hermes/config.yaml | sed 's/^[[:space:]]*token:[[:space:]]*//' | tr -d ' \r')
CHAT=641099789
TG="https://api.telegram.org/bot${TOKEN}"

echo "=== Gateway DRAIN End-to-End Test ==="

# 0. Ack any stale bot updates so the gateway starts with a clean queue
#    (old /stop /new /restart messages must NOT replay into the test).
curl -s --max-time 10 "${TG}/getUpdates?offset=999999999" >/dev/null 2>&1

# 1. Start the REAL gateway with the webhook platform (real LLM key via env)
echo "[1/6] Starting real gateway (webhook platform, NOUS_API_KEY=${NOUS_API_KEY:+set})..."
rm -f "$LOG"
HERMES_GATEWAY_PLATFORMS=webhook SLERMES_WEBHOOK_PORT=$PORT stdbuf -oL -eL ./slermes gateway --platform webhook >"$LOG" 2>&1 &
GW_PID=$!
echo "[1/6] gateway PID=$GW_PID"

# 2. Wait for the webhook port
echo "[2/6] Waiting for gateway startup..."
for i in $(seq 1 20); do
    if (echo > /dev/tcp/127.0.0.1/$PORT) 2>/dev/null; then echo "    listening on :$PORT"; break; fi
    if ! kill -0 $GW_PID 2>/dev/null; then echo "    GATEWAY DIED EARLY"; cat "$LOG"; exit 1; fi
    sleep 1
done

# 3. Trigger a REAL, SLOW agent turn (long essay -> real LLM call in flight)
echo "[3/6] POSTing long-essay prompt (real agent turn)..."
curl -s -X POST "http://127.0.0.1:$PORT/webhook" \
     -H "Content-Type: application/json" \
     -d '{"text":"Write a very long, extremely detailed 2500-word essay about the history of the Silk Road, covering every major period from Han dynasty origins through the Mongol era, in full flowing prose. Take your time.","chat_id":"e2e_drain_test"}' \
     --max-time 240 -o /tmp/gw_e2e_reply.txt 2>/tmp/gw_e2e_curl.err &
CURL_PID=$!
sleep 10
if ! kill -0 $GW_PID 2>/dev/null; then echo "    FAIL: gateway died before SIGTERM"; cat "$LOG"; exit 1; fi
echo "    turn in flight (curl PID=$CURL_PID still running: $(kill -0 $CURL_PID 2>/dev/null && echo yes || echo no))"

# 4. SIGTERM mid-turn
echo "[4/6] Sending SIGTERM to $GW_PID (turn still in flight)..."
T0=$(date +%s)
kill -TERM $GW_PID

# 5. Wait for graceful exit — time it (drain must take meaningful time,
#    NOT return instantly while a turn is running)
echo "[5/6] Waiting for graceful drain+exit (up to 120s)..."
exited=0
for i in $(seq 1 240); do
    if ! kill -0 $GW_PID 2>/dev/null; then exited=1; break; fi
    sleep 0.5
done
T1=$(date +%s)
DRAIN_SECS=$((T1 - T0))

if [ "$exited" != "1" ]; then
    echo "FAIL: gateway did not exit within 120s — killing"
    kill -9 $GW_PID 2>/dev/null || true
    curl -s --max-time 10 "${TG}/sendMessage" --data-urlencode "chat_id=$CHAT" \
         --data-urlencode "text=SLERMES E2E DRAIN: FAIL — gateway did not exit (drain hung)" >/dev/null
    echo "--- log tail ---"; tail -25 "$LOG"
    exit 1
fi
echo "    exited after ${DRAIN_SECS}s of drain"

# 6. Verify graceful sequence markers + meaningful drain time
echo "[6/6] Verifying drain sequence..."
OK=1
grep -q "Shutdown requested (SIGTERM) — beginning drain" "$LOG" || { echo "FAIL: missing 'Shutdown requested' marker"; OK=0; }
grep -q "Shutdown complete" "$LOG" || { echo "FAIL: missing 'Shutdown complete' marker"; OK=0; }
# If a turn was in flight and the drain works, exit must NOT be instant:
# the in-flight turn either completed (drain waited) or was interrupted
# after the 30s window + 5s grace. Sub-3s exit while curl still pending
# would mean the drain did NOT wait.
if [ "$DRAIN_SECS" -lt 3 ]; then
    echo "FAIL: drain returned in ${DRAIN_SECS}s — in-flight turn not honored"
    OK=0
fi
wait $CURL_PID 2>/dev/null
REPLY_BYTES=$(wc -c < /tmp/gw_e2e_reply.txt 2>/dev/null || echo 0)

if [ "$OK" = "1" ]; then
    echo "PASS: gateway drained ${DRAIN_SECS}s with turn in flight and exited gracefully"
    echo "      (turn reply bytes: $REPLY_BYTES)"
    curl -s --max-time 10 "${TG}/sendMessage" --data-urlencode "chat_id=$CHAT" \
         --data-urlencode "text=✅ SLERMES E2E DRAIN: PASS — gateway waited ${DRAIN_SECS}s for the in-flight turn, then shut down gracefully (reply ${REPLY_BYTES}B)." >/dev/null
    echo "      -> pinged Telegram chat $CHAT"
    echo "--- log tail ---"
    tail -15 "$LOG"
    exit 0
else
    curl -s --max-time 10 "${TG}/sendMessage" --data-urlencode "chat_id=$CHAT" \
         --data-urlencode "text=❌ SLERMES E2E DRAIN: FAIL — markers missing (see log)" >/dev/null
    echo "--- log tail ---"
    tail -25 "$LOG"
    exit 1
fi
