# Core facilities for JsonLogic: Track A

Research only. This directory adds no JsonLogic evaluator, public API, or default
build target. It records compatibility-sensitive behavior before choosing an
engine architecture. The baseline is sourcemeta/core commit
`246c0e4f80bb196096d9b044c3c8bc0487987300`.

## Reproduce

From the repository root, with CMake, Ninja, and a C++23 compiler available:

```sh
cmake -S research/jsonlogic-core -B build-jsonlogic-research -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DSOURCEMETA_CORE_USE_SYSTEM_ALLOCATOR=ON \
  -DSOURCEMETA_CORE_CLANG_TIDY=OFF
cmake --build build-jsonlogic-research --target core_jsonlogic_facilities
ctest --test-dir build-jsonlogic-research -R '^research.jsonlogic.facilities$' --output-on-failure
build-jsonlogic-research/core_jsonlogic_facilities --facts
```

The executable runs 26 tests through Core's test runner. It covers numeric
representation and arithmetic, lookup and pointer semantics, allocation-related
ownership helpers, Unicode, errors, limits, JSONPath preparation, regex, and time
utilities. `--negative-control` deliberately introduces one incorrect numeric
expectation and must exit nonzero. It is not registered in ordinary CTest.

For comparison with the pinned JavaScript reference, keep its checkout outside
this repository:

```sh
git clone https://github.com/jwadhams/json-logic-js.git /tmp/jsonlogic-reference
git -C /tmp/jsonlogic-reference checkout c5c73601c90b11e98f6846609bac4dec203d1c18
node research/jsonlogic-core/reference.cjs /tmp/jsonlogic-reference/logic.js
```

That commit is version 2.0.5. The reference script checks 24 observations. Its
optional second argument, `--negative-control`, must exit nonzero. No upstream
JavaScript source is vendored here.

## Findings and boundaries

- Core's JSON parser preserves adjacent integers above 2^53 and uses Decimal for
  inexact decimal tokens. JavaScript JSON.parse and binary arithmetic differ.
- Core equality is structural. JsonLogic's JavaScript strict equality compares
  distinct arrays and objects by identity. Native Core ordering and numeric
  parsers also do not implement JsonLogic coercion.
- Parsed duplicate object members are retained and lookup selects the first;
  JavaScript JSON.parse retains the last. Numeric negative zero also differs.
- Prehashed lookup is available, but object storage is a vector and lookup can
  scan it. JSON Pointer is reusable machinery, not JsonLogic's dot-path syntax.
- OwnedOrReference can borrow inputs or own computed outputs. It does not make
  borrowed values safe after the source is destroyed or mutated.
- JSONPath demonstrates prepared evaluation and explicit root/candidate scope.
  Its language semantics are not JsonLogic semantics.
- UTF-8 byte strings and code-point helpers do not supply JavaScript UTF-16
  substring semantics. Raw malformed UTF-8 requires explicit validation.
- Decimal division is exact for representable terminating results and rounds
  recurring results; arbitrary precision does not mean every division is exact.

Observed verification: Linux/GCC 14.2 Release, system allocator, 26 Core probes
and 24 reference checks passed; both deliberate failure controls exited 1. The
existing JSON, numeric, JSON Pointer, memory, and error CTest suites passed.
Formatting and diff checks were also run. The full Core suite, macOS/Windows,
sanitizers, allocation measurements, and performance comparisons were not run
for this research slice. No performance conclusion follows from these probes.
