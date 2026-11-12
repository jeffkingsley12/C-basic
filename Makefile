# C-basic

Production-grade core type architecture for a high-performance C runtime.

This project provides reusable C building blocks for allocation, containers, string handling, and tokenization.

## Included modules

- `ce_types.h` — core primitive aliases and alignment utilities
- `allocator.h` — shared allocation interface used across the library
- `arena.h` / `arena.c` — arena allocator implementation and validation helpers
- `array.h` / `array.c` — dynamic array abstraction built on a realloc-style allocator
- `vector.h` / `vector.c` — intrusive-header vector implementation
- `list.h` / `list.c` — boxed list container
- `dict.h` / `dict.c` — dictionary implementation
- `string_lib.h` / `string.c` — immutable string API
- `tokenizer.h` / `tokenizer.c` — lexer/tokenizer utilities

## Build

A minimal build is available via:

```bash
make
```

## Notes

The core library now uses `ce_types.h` as the authoritative type layer so the rest of the project shares the same alignment and primitive definitions.
