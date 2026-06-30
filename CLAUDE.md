# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

KomodoOcean (komodo-qt) is a native Qt5 graphical wallet and daemon for the Komodo (KMD) ecosystem — a ZCash fork. It produces four binaries: `komodod` (daemon), `komodo-qt` (GUI wallet), `komodo-cli` (CLI tool), and `komodo-tx` (transaction utility). The `static` branch is the main production branch.

## Build System

Uses autotools (autoconf/automake). To regenerate the build system after modifying `configure.ac` or `Makefile.am`:
```shell
./autogen.sh
```

Before building for the first time, download ZCash zkSNARK parameters (~911MB):
```shell
./zcutil/fetch-params.sh
```

### Build Commands

**Linux (native, with Qt GUI):**
```shell
./zcutil/build.sh -j8
```

**Linux (daemon only, no Qt):**
```shell
./zcutil/build-no-qt.sh -j8
```

**Linux (aarch64 cross-compile):**
```shell
./zcutil/build-aarch64-cross.sh -j8
```

**macOS (native):**
```shell
./zcutil/build-mac.sh -j8       # Intel
./zcutil/build-mac-arm.sh -j8   # Apple Silicon
```

**macOS (cross-compile from Linux):**
Requires `Xcode-13.2.1-13C100-extracted-SDK-with-libcxx-headers.tar.gz` in repo root.
```shell
./zcutil/build-mac-cross.sh -j8
```

**Windows (cross-compile from Linux):**
Requires mingw-w64 with POSIX variants configured.
```shell
./zcutil/build-win.sh -j8
```

### Key Configure Flags

The build scripts call `./configure` internally. Notable options:
- `--with-gui=no` — daemon-only build
- `--enable-tests` / `--disable-tests`
- `--enable-lcov` — code coverage
- `--disable-bip70` — disable payment protocol (used in non-Linux builds)
- `--enable-rust` — Rust support (enabled by default)

## Running Tests

**Unit tests (Boost framework):** `src/test/`

**Unit tests (Google Test framework):** `src/gtest/` and `src/wallet/gtest/`

**Run the full test suite:**
```shell
qa/zcash/full-test-suite.sh
```

**Run RPC tests (Python):**
```shell
qa/pull-tester/rpc-tests.sh
```

**Run a single Boost test binary** (after build):
```shell
src/test/test_bitcoin --run_test=<test_suite_name>
```

**Run a single GTest binary** (after build):
```shell
src/zcash-gtest --gtest_filter=<TestCase>.<TestName>
```

## Code Architecture

### Core Layers

- **`src/main.cpp` / `src/net.cpp`** — Block validation, chain state management, P2P networking
- **`src/chainparams.cpp`** — Chain parameters, checkpoints, DNS seeds for KMD and all assetchains
- **`src/consensus/`** — Consensus rules (upgrades, params)
- **`src/zcash/`** — ZCash-specific logic: Sapling/Sprout proofs, note encryption, incremental Merkle trees
- **`src/cc/`** — CryptoConditions / Custom Contracts (Komodo-specific smart contract layer)
- **`src/wallet/`** — Wallet logic including z-address (shielded) support
- **`src/rpc/`** — RPC server and all RPC call implementations
- **`src/qt/`** — Qt5 GUI application
- **`src/script/`** — Script interpreter and standard scripts
- **`src/crypto/`** — Hash functions, ECDSA, and ZK-proof cryptography

### Komodo-Specific Extensions

- **`src/cc/`** — Custom Contracts: on-chain DEX, tokens, gateways, oracles, etc. Each contract is a `.cpp` file implementing eval code dispatch.
- **`src/ac/`** — Assetchain support: `komodo_assetchain.h` defines assetchain parameters loaded at startup via `-ac_name`, `-ac_supply`, etc. flags.
- **`src/chainparams.cpp`** — Contains a large static table of assetchain definitions in addition to mainnet/testnet params.

### Threading Model

Key threads (described in `doc/developer-notes.md`):
- `ScriptCheck` — parallel script validation
- `Import` — block import
- `DNSAddressSeed` — DNS peer discovery
- `SocketHandler` — P2P socket I/O
- `ProcessMessage` — incoming P2P message handling

Use `DEBUG_LOCKORDER=1` when debugging potential deadlocks.

### Database

- **LevelDB** (`src/leveldb/`) — Block index and UTXO set (chainstate)
- **BerkeleyDB** — Wallet storage (via `src/wallet/db.cpp`)

## Version

Current version is defined in `configure.ac`:
```
_CLIENT_VERSION_MAJOR = 0
_CLIENT_VERSION_MINOR = 9
_CLIENT_VERSION_REVISION = 2
```

## Debugging

Enable verbose logging by running `komodod` or `komodo-qt` with `-debug=<category>`. Categories include: `net`, `mempool`, `rpc`, `wallet`, `zrpc`, `addrman`, etc. Logs go to `debug.log` in the data directory.

For regtest/local testing:
```shell
komodod -regtest -daemon
komodo-cli -regtest <command>
```
