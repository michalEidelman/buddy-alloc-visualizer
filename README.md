# Buddy Allocator Visualizer

An educational + engineering-grade C++ project that implements a **Buddy Memory Allocator** and exposes:

- a thread-safe allocator library,
- JSON heap snapshots,
- an interactive terminal simulator for teaching split/coalesce,
- and a micro-benchmark for throughput and memory efficiency.

---

## Algorithm overview

The buddy allocator manages power-of-two blocks:

- Minimum block: `128 B` (Order 0)
- Maximum buddy block: `128 KB` (Order 10)
- Initial arena: `32 × 128 KB = 4 MB`

Each request is rounded to the smallest order that can hold:

```
requested_bytes + metadata <= 128 * 2^k
```

### Split logic

If there is no free block in the requested order, the allocator takes the next larger free block and repeatedly splits:

```
Order k+1 block -> two Order k buddies
```

### O(1) XOR buddy lookup

For a block starting at address `addr` with block size `S`, its buddy address is:

```
buddy_addr = addr XOR S
```

This works because buddies differ in exactly one alignment bit (the `S` bit).

### Coalescing

On free, if the buddy is free and the same size, both are merged into the next order. Repeat until no merge is possible.

### Large allocations

Requests larger than buddy max block are serviced by `mmap` (with HugePages attempt when available, then fallback).

---

## Data structures

Metadata per block:

- block size,
- requested payload size,
- free/allocated state,
- doubly-linked list pointers.

Free blocks are kept in per-order sorted linked lists (`Order 0..10`).

```
free_lists[0] -> 128B blocks
free_lists[1] -> 256B blocks
...
free_lists[10] -> 128KB blocks
```

---

## Project layout

```
include/BuddyAllocator.hpp   # public allocator API
src/BuddyAllocator.cpp       # implementation
malloc_3.cpp                 # compatibility wrappers (smalloc/sfree/...)
tools/simulator.cpp          # interactive educational CLI
benchmark.cpp                # throughput + efficiency benchmark
```

---

## Build

```bash
make test3
make simulator
make benchmark
```

Run:

```bash
./test3
./simulator
./benchmark
```

---

## Educational simulator

Interactive commands:

- `alloc <bytes>`
- `free <id>`
- `state`
- `help`
- `quit`

The simulator prints:

1. operation result,
2. split/coalesce event trace (including XOR buddy merge steps),
3. `dumpHeapStateJson()` snapshot for visualization tooling.

---

## JSON API (`dumpHeapStateJson`)

`BuddyAllocator::dumpHeapStateJson()` returns a JSON snapshot containing:

- all blocks grouped by order (`0..10`),
- per-block start address, size, status (`allocated`/`free`), requested bytes,
- per-order and global internal fragmentation metrics.

This JSON is designed for educational dashboards or local web visualizers.

---

## Notes

- Thread-safety is implemented with `std::mutex` and `std::lock_guard`.
- Legacy homework API (`smalloc`, `scalloc`, `sfree`, `srealloc`, stats helpers) is preserved.
