# Coding Style Guide

All new code should match the style of the existing `hw::`, `os::`, and
`debug::` namespaces.

## Formatting

- **Two-space indent**, no tabs.
- **Minimal whitespace**. No blank lines between closely related statements.
  One blank line between functions.
- **Compact expressions**: no spaces around operators in assignments and
  comparisons: `x=y+1;`, `if(x==0)`, `a=b|c;`, `n=static_cast<int>(v);`.
  No spaces inside parens or angle brackets.
- **Braces on same line** as the statement: `if(...) {`, `for(...) {`.
  Single-statement bodies may omit braces.
- **No space before parens** in control flow: `if(x)`, `for(...)`, `while(...)`, `switch(...)`.
- **Compact initializer lists**: `for(int i=0; i<8; i++)`.

## Declarations

- **Variables declared at the top of the function**, not inline.
- **No `auto`**. Always use explicit types.
- **Use `int32_t` / `uint32_t`** for values that interact with the
  emulator or MV-32 data. Use `int` / `size_t` for loop counters
  and standard C++ APIs.

## Naming

- **Classes**: `PascalCase` (e.g., `VaryingString`, `NativeRegistry`).
- **Methods and functions**: `snake_case` (e.g., `read_word`, `arg_addr`).
- **Member variables**: `snake_case`, no prefix (e.g., `word_addr`, not `m_word_addr`).
- **Constants**: `UPPER_CASE` (e.g., `SEGMENT_BASE`, `ERR_CONVERSION`).
- **Namespaces**: `lower_case` (e.g., `hw`, `os`, `emu_rt`, `types`).
- **rt functions**: suffixed with arg count (e.g., `char_to_unsigned_1`,
  `write_screen_5`). The suffix counts the native C++ arguments
  (excluding Context), which normally matches the LCALL arg count.
  Exception: functions with non-stack calling conventions (e.g.,
  SQR31?3 passes input via FPAC0) use a suffix reflecting the native
  interface (`sqr31_1`), and the `emu_rt` wrapper handles the
  calling convention translation.
- **emu_rt functions**: no suffix (e.g., `char_to_unsigned`). They
  determine arg count internally.

## File Structure

- **Headers**: `#pragma once`, includes, forward declarations, namespace,
  class definition, close namespace.
- **Source files**: includes, blank lines, namespace, implementations,
  close namespace.
- **Include order**: own header first, then project headers (relative
  paths with `../`), then system headers.

## Class Layout

```cpp
class Foo {
public:
  // constants
  // member variables (public fields are fine — match existing style)
  // constructors
  // methods

private:
  // private members
  // private methods
};
```

## Unused Parameters

- **No `(void)param;` casts**. If a parameter is unused, leave it.
  The compiler flag `-Wno-unused-parameter` suppresses the warning.
- **Unused rt arguments**: name them `ignoreN` where N is the argument
  position (e.g., `ignore3` for the third PL/I argument). Use the
  correct type even for ignored args.

## PL/I String Conventions

- PL/I CHAR VARYING strings are length-prefixed (word 0 = current length).
  They do NOT use null termination internally.
- However, when PL/I code passes strings to AOS/VS system calls (which
  expect null-terminated strings), the null terminator IS included in
  the varying length.  So a string like "QUEST" has varying length 6
  (5 chars + null).
- When converting `types::String` to `std::string` for OS operations
  (file names, service names, etc.), always truncate at the first
  null byte: `sname.resize(sname.find('\0'))`.

## Error Handling

- `types::OperatingSystem` methods always return `int32_t`: 0 on success,
  non-zero error code on failure. They never throw.
- `rt::` library functions (called via LCALL, routed through `?LIB_ERROR`)
  check the OS return and throw `types::PLIError` on error.
- `rt::` SYSCALL functions (called via SYSCALL, error returned in ac0)
  return the error code directly, never throw.
- `emu_rt::` LCALL wrappers catch `PLIError` and route via `throw_lib_error()`.
- `emu_rt::syscall_handler` returns error codes directly to the emulator.
- Use `std::runtime_error` for internal/unexpected errors only.

## Comments

- Brief comments where the code isn't self-explanatory.
- No boilerplate or redundant comments (e.g., don't comment `// destructor`
  above a destructor).
