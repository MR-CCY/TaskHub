#!/usr/bin/env bash
set -euo pipefail

MASTER="http://localhost:8082"
WORKER_ID="worker-1"
WORKER_HOST="127.0.0.1"
WORKER_PORT=8083
QUEUE="default"

echo "Using master: ${MASTER}"
echo "Worker: id=${WORKER_ID}, host=${WORKER_HOST}, port=${WORKER_PORT}, queue=${QUEUE}"
echo "Assume: TTL=10s, prune_after=60s (adjust if your code differs)"
echo

jq_or_cat() {
  if command -v jq >/dev/null 2>&1; then jq .; else cat; fi
}

print_workers() {
  curl -s "${MASTER}/api/workers" | jq_or_cat
}

register_worker() {
  curl -s -X POST "${MASTER}/api/workers/register" \
    -H "Content-Type: application/json" \
    -d "{
      \"id\":\"${WORKER_ID}\",
      \"host\":\"${WORKER_HOST}\",
      \"port\":${WORKER_PORT},
      \"queues\":[\"${QUEUE}\"],
      \"labels\":[\"shell\"],
      \"running_tasks\":0
    }" | jq_or_cat
}

heartbeat_once() {
  curl -s -X POST "${MASTER}/api/workers/heartbeat" \
    -H "Content-Type: application/json" \
    -d "{
      \"id\":\"${WORKER_ID}\",
      \"running_tasks\":0
    }" | jq_or_cat
}

echo "=============================="
echo "▶ CASE 1: Register worker, expect alive=true"
echo "------------------------------"
register_worker
print_workers
echo

echo "=============================="
echo "▶ CASE 2: Wait 11s (>TTL=10s), expect alive=false (still listed)"
echo "------------------------------"
sleep 11
print_workers
echo

echo "=============================="
echo "▶ CASE 3: Wait 55s more (total > ~66s), expect pruned (removed) if prune_after=60s"
echo "------------------------------"
sleep 55
print_workers
echo

echo "=============================="
echo "▶ CASE 4: Register again, then keep sending heartbeat every 2s for 20s -> expect alive=true always"
echo "------------------------------"
register_worker
for i in {1..10}; do
  heartbeat_once >/dev/null
  sleep 2
done
print_workers
echo

echo "=============================="
echo "▶ CASE 5: Stop heartbeat, wait 11s -> alive=false; wait extra 55s -> pruned"
echo "------------------------------"
sleep 11
print_workers
sleep 55
print_workers
echo

echo "✅ M11.4 test flow done."