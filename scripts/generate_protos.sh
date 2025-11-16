#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROTO_DIR="$ROOT_DIR/proto"
OUT_DIR="$ROOT_DIR/src/proto"

mkdir -p "$OUT_DIR" "$OUT_DIR/google/protobuf"

if ! command -v protoc-c >/dev/null 2>&1; then
  echo "protoc-c not found. Install protobuf-c >=1.4" >&2
  exit 1
fi

protoc-c --c_out="$OUT_DIR" -I"$PROTO_DIR" \
  "$PROTO_DIR"/solana-storage.proto \
  "$PROTO_DIR"/geyser.proto
protoc-c --c_out="$OUT_DIR" -I"$PROTO_DIR" "$PROTO_DIR"/google/protobuf/timestamp.proto

echo "Generated sources in $OUT_DIR"
