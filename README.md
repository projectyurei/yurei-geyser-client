# yurei-geyser-client

High-performance Solana Yellowstone (Geyser) gRPC client written in C.  The client subscribes to the `geyser.Geyser/Subscribe` stream, performs SIMD-accelerated protocol detection on the incoming updates, parses PumpFun and Raydium events with zero-copy layouts, and writes the derived events into PostgreSQL without blocking the ingest path.

## Features
- Persistent TLS gRPC connection with automatic reconnection and exponential backoff.
- Uses `protobuf-c` structures generated from the official Yellowstone `geyser.proto` definitions.
- SIMD protocol detector (AVX2/SSE2/NEON + scalar fallback) that matches known program ids directly inside geyser transaction payloads.
- Zero-copy parsers for PumpFun trades and Raydium swaps; the parsers cast instruction data onto packed C structs to avoid `malloc`/`memcpy` hot paths.
- Lock-free-ish bounded queue that decouples the ingest loop from the database writer thread.
- PostgreSQL writer based on `libpq` that batches inserts into dedicated tables.

## Repository layout
```
proto/                    # geyser + solana storage proto files (source of truth)
cmake/                    # additional CMake modules
include/                  # public headers for the modules
src/                      # implementation
scripts/generate_protos.sh# helper for manual proto generation
```

## Dependencies
- `cmake >= 3.20`
- `clang` or `gcc` with C11 support
- [`protobuf-c`](https://github.com/protobuf-c/protobuf-c) runtime headers/libraries (`libprotobuf-c` pkg-config entry)
- [`gRPC`](https://github.com/grpc/grpc) C core (provides `libgrpc` + headers)
- `PostgreSQL` client libraries (`libpq`, headers + pkg-config metadata)
- `pkg-config`, `openssl`, and POSIX threads

> **Note:** `protoc-c` is only required when you need to re-generate protobuf bindings. Generated stubs are checked into `src/proto/` so normal builds work as long as `libprotobuf-c` is installed.

### Optional (development)
- `clang-format`
- `bear` for compilation database generation

## Building
1. Export any custom library locations (only needed when the system does not ship `libprotobuf-c` or `libpq` pkg-config metadata):
   ```bash
   export PKG_CONFIG_PATH=/opt/protobuf-c/lib/pkgconfig:$PKG_CONFIG_PATH
   export PostgreSQL_ROOT=/opt/postgresql
   ```
2. Configure and compile:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ```

If `cmake` still complains about missing dependencies, install `libprotobuf-c-dev`, `libgrpc-dev`, and `libpq-dev` (package names vary per distro) or update the paths above to point at your local builds.

### Regenerating protobuf bindings
`proto/` intentionally uses `proto2` syntax to keep compatibility with `protobuf-c`, which still lacks `proto3 optional` support.  If upstream `geyser.proto` changes, patch it the same way and run the helper script:

```bash
scripts/generate_protos.sh
```

The script writes into `src/proto/` directly (including the bundled `google/protobuf/timestamp.pb-c.*` files).

## Running
1. Provision PostgreSQL (or point to an existing instance) and apply the schema below.
2. Export the runtime configuration and start the binary:
   ```bash
   YUREI_GEYSER_ENDPOINT=solana-yellowstone-grpc.publicnode.com:443 \
   YUREI_DB_URL=postgres://user:password@127.0.0.1:5432/yurei \
   YUREI_PUMPFUN_PROGRAM=6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P \
   YUREI_RAYDIUM_PROGRAM=Hvw5ZJiExampleFake11111111111111111111111111 \
   YUREI_GEYSER_AUTH_TOKEN="Bearer YOUR_ALLNODES_TOKEN" \
   ./build/yurei-geyser-client
   ```
3. Watch the process logs for `yurei geyser client started`, then verify new rows land in `pumpfun_trades` / `raydium_swaps`.

Important environment variables:
- `YUREI_GEYSER_ENDPOINT` — Yellowstone gRPC endpoint (default: `solana-yellowstone-grpc.publicnode.com:443`).
- `YUREI_DB_URL` — PostgreSQL connection string.
- `YUREI_PUMPFUN_PROGRAM` / `YUREI_RAYDIUM_PROGRAM` — base58 program ids that should be detected.
- `YUREI_RESUME_FROM_SLOT` — optional, instructs the subscription to replay from a slot.
- `YUREI_QUEUE_CAPACITY` — overrides the bounded queue length (default 65536).
- `YUREI_GEYSER_AUTH_TOKEN` — **required for production endpoints.** PublicNode/Allnodes issue free Yellowstone credentials; log in to [Allnodes](https://www.allnodes.com/) → *PublicNode* → *Get token* and paste the provided value (usually `Bearer <token>`).

Run the binary under a supervisor (systemd, Docker, etc.) for 24/7 uptime; the geyser client auto-reconnects with exponential backoff.

## Database schema
Example schema expected by the writer:
```sql
CREATE TABLE IF NOT EXISTS pumpfun_trades (
    observed_at TIMESTAMPTZ DEFAULT now(),
    slot BIGINT NOT NULL,
    tx_signature TEXT NOT NULL,
    mint TEXT NOT NULL,
    trader TEXT NOT NULL,
    creator TEXT NOT NULL,
    side TEXT NOT NULL,
    sol_amount NUMERIC NOT NULL,
    token_amount NUMERIC NOT NULL,
    fee_bps NUMERIC NOT NULL,
    fee_lamports NUMERIC NOT NULL,
    creator_fee_bps NUMERIC NOT NULL,
    creator_fee_lamports NUMERIC NOT NULL,
    virtual_sol_reserves NUMERIC NOT NULL,
    virtual_token_reserves NUMERIC NOT NULL,
    real_sol_reserves NUMERIC NOT NULL,
    real_token_reserves NUMERIC NOT NULL
);

CREATE TABLE IF NOT EXISTS raydium_swaps (
    observed_at TIMESTAMPTZ DEFAULT now(),
    slot BIGINT NOT NULL,
    tx_signature TEXT NOT NULL,
    pool TEXT NOT NULL,
    user_owner TEXT NOT NULL,
    amount_in NUMERIC NOT NULL,
    amount_out NUMERIC NOT NULL
);
```

## Scripts
`scripts/generate_protos.sh` re-builds the vendored protobuf stubs under `src/proto/` if you upgrade the `.proto` definitions.

## Architecture overview
1. **Geyser client** — Maintains the TLS channel, replays from the configured slot, and emits `SubscribeUpdate` messages into the ingestion pipeline.
2. **Protocol detector** — SIMD scanner that locates program ids inside account-key payloads and log blobs without leaving L1 cache.
3. **Parsers** — Zero-copy binary overlays for PumpFun & Raydium instructions.  The parser casts instruction bytes onto packed structs, extracting the fields with little-endian helpers only when needed.
4. **Event queue** — Multi-producer/single-consumer bounded ring via futex-friendly `pthread` primitives.
5. **Database writer** — Dedicated thread that builds parameterized `INSERT` statements without blocking ingest.

## Testing
Parser and protocol-detector tests live under `tests/` and compile alongside the main target.  After configuring the build directory:
```bash
cmake --build build --target test
ctest --test-dir build --output-on-failure
```
`test_pumpfun_parser` synthesizes a PumpFun trade layout and verifies the zero-copy parser mirrors every field, while `test_protocol_detector` exercises the SIMD matcher on synthetic pubkeys.  Extend this folder with additional captured fixtures as you add new protocols.

## Production notes
- Use systemd or another supervisor to run the binary 24/7.
- `geyser_client` tracks gRPC status codes and reconnects using exponential backoff to mitigate transient outages.
- SIMD detection automatically falls back to scalar search when the CPU lacks AVX2/NEON features so the binary can run on aarch64 validator nodes.
- Add PostgreSQL connection pooling (pgBouncer) in deployments with multiple ingestion workers.
