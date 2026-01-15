# ⚡️ ArlECS

![Language](https://img.shields.io/badge/language-C99-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Performance](https://img.shields.io/badge/performance-High-red.svg)

**ArlECS** is a high-performance, ultra-lightweight Entity Component System (ECS) engine written in pure C99.
Built on top of the **Armel** arena allocator, it ensures contiguous memory layout, zero fragmentation, and **zero-malloc runtime** during the game loop.

## 🚀 Key Features

* **Cache-Friendly:** Uses **Sparse Sets** for component storage, allowing linear iteration speed close to raw array processing.
* **Zero-Allocation Runtime:** All memory is pre-allocated in an Arena. No garbage collection, no fragmentation.
* **Multi-Component Views:** Powerful and expressive iterator system (`ArlView`) to query entities with specific component combinations.
* **Type-Safety:** Macros ensure component size safety at registration time.
* **Simple API:** Pure C. No complex templates or class hierarchies.

## 📦 Architecture

ArlECS is designed around **Data-Oriented Design** principles:

* **World:** A container using an external `Armel` arena.
* **Entity:** A simple `uint32_t` ID.
* **Component:** Pure Old Data (POD) structs.
* **System:** Standard functions iterating over component views.

## 🛠 Integration

### Prerequisites
* C99 Compiler (Clang/GCC)
* `libarmel.a` (Memory Allocator)

### Building
The project includes a Makefile to build the static library and run benchmarks.

```bash
# Build the static library (lib/libarlecs.a)
make

# Run Unit Tests (Debug mode + Sanitizers)
make tests

# Run Benchmarks (Release mode -O3)
make bench
```

## ⚡️ Quick Start

```c
#include <Armel/armel.h>
#include <ArmelECS/arlecs.h>

typedef struct { float x, y; } Position;
typedef struct { float vx, vy; } Velocity;

enum { C_POS, C_VEL }; // Component IDs

int main() {
    // 1. Init Memory
    Armel arena;
    arl_new(&arena, 10 * 1024 * 1024); // 10 MB

    // 2. Create World
    ArlEcsWorld* world = arlecs_world_create(&arena);
    
    // 3. Register Components
    arlecs_component_new(world, C_POS, Position, 1000);
    arlecs_component_new(world, C_VEL, Velocity, 1000);

    // 4. Create Entity
    ArlEntity e = arlecs_create_entity(world);
    Position* p = arlecs_add_component(world, e, C_POS);
    p->x = 10; p->y = 10;

    // 5. System Logic (View)
    ArlView view = arlecs_view(world, 1, C_POS);
    while (arlecs_view_next(&view)) {
        Position* pos = (Position*)view.components[0];
        printf("Entity %d is at %.1f\n", view.entity, pos->x);
    }

    // 6. Cleanup
    arl_free(&arena);
    return 0;
}
```

## 📊 Performance

Benchmarks running on Apple M4 (Single Core):

| Operation | Scale | Time (avg) |
| :--- | :--- | :--- |
| **Creation** | 1,000,000 Entities | ~3.40 ms |
| **Iteration** | 1,000,000 Components (Read/Write) | **~0.61 ms** |
| **Complex View** | 1M Entities, 3 Systems, Logic | ~4.55 ms |

## 📜 License

This project is open-source and available under the MIT License.