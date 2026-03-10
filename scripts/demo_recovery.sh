#!/usr/bin/env bash
set -euo pipefail

echo "== Building project =="
cmake -S . -B build >/dev/null
cmake --build build >/dev/null

echo "== Starting cluster demo =="
./build/kv_cluster_demo > /tmp/kv_cluster_demo.log 2>&1 &
CLUSTER_PID=$!

cleanup() {
  echo
  echo "== Stopping cluster demo =="
  kill $CLUSTER_PID 2>/dev/null || true
}
trap cleanup EXIT

sleep 2

echo "== Checking cluster roles =="
./build/kv_client 9101 "STATUS" || true
./build/kv_client 9102 "STATUS" || true
./build/kv_client 9103 "STATUS" || true

echo
echo "== Writing initial replicated value =="
./build/kv_client 9101 "PUT recovery 111"

echo
echo "== Reading from all nodes =="
./build/kv_client 9101 "GET recovery"
./build/kv_client 9102 "GET recovery"
./build/kv_client 9103 "GET recovery"

echo
echo "== Recovery demo requires restart sequence from cluster_demo output =="
echo "Watch cluster logs below to verify restart/catch-up behavior."

echo
echo "== Cluster logs =="
tail -n 80 /tmp/kv_cluster_demo.log