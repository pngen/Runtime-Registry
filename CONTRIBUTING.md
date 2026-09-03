# Contributing to Runtime Registry

Thank you for your interest in Runtime Registry.

## Scope

Runtime Registry is a vendor-neutral C++20 runtime discovery and authority
layer for heterogeneous AI infrastructure. It owns a single systems boundary:
stating *what runtime services, nodes, devices, backends, protocols, and
capabilities exist right now*, which instances are current and healthy, what
versions and contracts they expose, and which registry state is authoritative
enough to drive discovery and binding.

It is deliberately not a resource allocator, not a state store with ownership
semantics, not a cache directory, and not a generic DNS / Consul / etcd clone.

## Conventions

- C++20, clean under MSVC with /W4 /WX.
- Strong, non-interchangeable identity and generation types.
- Deterministic behavior. Never silently downgrade an UNKNOWN to compatible.
- Avoid holding global registry locks during blocking network I/O.
- Every mutation carries explicit authority: epoch, WorkerBootId, generations.

## Getting started

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ctest --test-dir build

## Adding code

- Run `cmake --build build --target format-check` where available, and keep
  `/W4 /WX` clean.
- Add a test under tests/, and register it in tests/CMakeLists.txt.
- Add an example under examples/, and register it in examples/CMakeLists.txt.
- Keep commit messages neutral and public-facing.

## License

By contributing you agree that your contributions are licensed under the
Apache License, Version 2.0. See LICENSE and NOTICE.
