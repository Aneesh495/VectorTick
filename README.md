# VectorTick

VectorTick is a C++20 prototype for synthetic market-event ingestion, columnar
storage, query parsing, and IR-based execution research.

## Current status

The repository has source for VTP1 and PCAP decoding, VTS1 segment I/O and
codecs, a query lexer and parser, an SSA IR builder, a reference interpreter,
and x86-64 and AArch64 assembler/code-generation work. These parts have not
been validated as a complete query-to-native-code pipeline.

A clean CMake configure succeeds on macOS arm64 with AppleClang 21. The build
then fails in `include/vectortick/jit/x86_assembler.hpp` because `X86Reg`
values are passed to `modrm(u8, u8, u8)`. The tests and benchmark executable
cannot run from that build. Other platforms are not verified by this audit.

`tests/test_main.cpp` defines six basic checks for types, byte order, CRC32C,
the canonical event size, and checked arithmetic. There are no dedicated
protocol, storage, query, IR, or JIT tests yet. The repo has no fuzz targets,
and it does not publish reproducible performance results.

## Source map

| Path | Contents |
| --- | --- |
| `include/vectortick/common/` | Types, status/result, endian helpers, hashing, checked math |
| `include/vectortick/memory/`, `concurrency/` | Buffers, arenas, mapped files, queues, worker pool |
| `include/vectortick/protocol/`, `src/protocol/` | VTP1 decoding and PCAP reading |
| `include/vectortick/codec/`, `storage/`, `src/storage/` | Codecs and VTS1 segment I/O |
| `include/vectortick/query/`, `src/query/` | Query tokens, lexer, parser, AST |
| `include/vectortick/ir/`, `src/ir/` | IR types and builder |
| `include/vectortick/execution/`, `src/execution/` | Reference interpreter |
| `include/vectortick/jit/`, `src/jit/` | x86-64 and AArch64 assembler/code-generation source |
| `apps/`, `tests/`, `bench/` | CLI entry points, six basic tests, benchmark harness |
| `docs/`, `cmake/` | Design notes and build support |

The presence of a source module in this table does not imply that it has an
integration test or a measured performance result.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The build currently stops at the assembler error described above on the
tested macOS arm64 toolchain. Once the build passes, CMake defines targets for
five apps, `vectortick_tests`, and `vectortick_bench`. The benchmark source is
present, but its results should be measured and recorded before citing them.

## Documentation

- [Implementation status and gaps](docs/BUILD_SPEC.md)
- [Architecture design](docs/ARCHITECTURE.md)
- [Architecture decision records](docs/adr/README.md)

The architecture documents describe both implemented source and intended
integration. Use the status document for the current validation boundary.

## Data and license

The examples use deterministic synthetic market data. The project does not
connect to a live exchange or place trades.

MIT. See [LICENSE](LICENSE).
