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

echo "== Checking node roles =="
./build/kv_client 9101 "STATUS" || true
./build/kv_client 9102 "STATUS" || true
./build/kv_client 9103 "STATUS" || true

echo
echo "== Writing through a follower (client should redirect) =="
./build/kv_client 9101 "PUT demo 123"

echo
echo "== Reading replicated value from multiple nodes =="
./build/kv_client 9101 "GET demo"
./build/kv_client 9102 "GET demo"
./build/kv_client 9103 "GET demo"

echo
echo "== Triggering leader stop manually =="
echo "Use this in another terminal if needed:"
echo 'printf "KILL\n" | nc localhost <leader_port>'

echo
echo "== Cluster logs =="
tail -n 40 /tmp/kv_cluster_demo.log