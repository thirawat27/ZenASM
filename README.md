# ZenAsm

ZenAsm is a C++20 compiler pipeline that turns an indentation-based `.zen` language into optimized x86-64 assembly.

## Highlights

- Frontend with lexer, parser, AST, and semantic validation.
- Middle-end IR with constant folding, branch simplification, dead code elimination, and repeat-loop unrolling for constant trip counts.
- Backend with graph-coloring register allocation, spill handling, and assembly emission for `win64` and `sysv64`.
- Inline assembly blocks and extern function declarations for low-level interop.
- Loop control with `elif`, `break`, and `continue`.
- `for` loops with `range(...)`, typed function signatures, and string literals backed by real data sections.
- Inline assembly operand binding via `asm(expr0, expr1, ...):` and `{0}`, `{1}` placeholders.
- Direct object/executable generation and `run` support from the ZenAsm CLI.
- `package` workflow that emits asm, object, executable, debug dumps, source snapshot, and a manifest.
- CMake-based build and CTest smoke coverage.

## Build

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_MAKE_PROGRAM="C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" `
  -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang++.exe" `
  -DCMAKE_RC_COMPILER="C:/Program Files/LLVM/bin/llvm-rc.exe"

cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Usage

```powershell
.\build\zenasm.exe build .\examples\hello.zen -o .\build\hello.asm --target win64 --opt 3
```

Optional debug outputs:

```powershell
.\build\zenasm.exe build .\examples\hello.zen `
  --emit-ast .\build\hello.ast.txt `
  --emit-ir .\build\hello.ir.txt
```

Build an object or executable directly:

```powershell
.\build\zenasm.exe build .\examples\advanced.zen `
  -o .\build\advanced.asm `
  --emit-obj .\build\advanced.obj `
  --emit-exe .\build\advanced.exe `
  --opt 3
```

Compile and run in one step:

```powershell
.\build\zenasm.exe run .\examples\advanced.zen `
  -o .\build\advanced.asm `
  --emit-exe .\build\advanced.exe `
  --opt 3
```

Create a package bundle:

```powershell
.\build\zenasm.exe package .\examples\production.zen `
  --output-dir .\build\dist\production `
  --opt 3
```

## Language Snapshot

```text
extern fn external_add(a, b)

fn main():
    let value = external_add(10, 32)
    if value > 40:
        print(value)
    else:
        print(0)
    return value
```

Typed signatures and inline asm bindings:

```text
fn banner() -> str:
    return "ZenAsm package ready"

fn sum_window(start: i64, finish: i64) -> i64:
    let total = 0
    for i in range(start, finish):
        total = total + i
    return total

fn main() -> i64:
    let total = sum_window(1, 7)
    asm(total):
        "add {0}, 5"
    print(banner())
    print(total)
    return total
```

Supported statements:

- `let name = expr`
- `name = expr`
- `return expr`
- `if cond:` / `elif cond:` / `else:`
- `while cond:`
- `for name in range(end):`
- `for name in range(start, end):`
- `for name in range(start, end, step):`
- `repeat count:`
- `break`
- `continue`
- `asm:` block with raw assembly lines
- `asm(expr0, expr1, ...):` block with `{0}`, `{1}`, ... operand placeholders
- expression statements for function calls such as `print(value)`

Supported types:

- `i64`
- `bool`
- `str`
- `void`

## Notes

- Function parameters are inferred as `i64` by default.
- Function return types can be explicit with `-> type`; if omitted they are inferred when possible.
- Booleans are strict in control-flow expressions and are emitted as `0`/`1`.
- String literals are emitted into real read-only data sections and passed around as native pointers.
- Outbound calls use the target calling convention and support register arguments plus stack arguments when needed.
- `zenasm run` builds the assembly, links an executable through the configured toolchain, and then launches the program.
- `--emit-obj` is useful for integrating ZenAsm into a larger native build pipeline.
- `zenasm package` builds a distributable output directory with asm/object/executable/debug dumps and `manifest.json`.
- Assembly output uses Intel-syntax GNU/LLVM assembler directives so it can be assembled with modern LLVM/GNU toolchains.
