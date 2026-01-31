# ⚡️ ArlECS

![Language](https://img.shields.io/badge/language-C99-blue.svg)
![License](https://img.shields.io/badge/license-zlib-green.svg)
![Performance](https://img.shields.io/badge/performance-High-red.svg)

**ArlECS** is a high-performance, ultra-lightweight Entity Component System (ECS) engine written in pure C.
Built on top of the **Armel** arena allocator, it ensures contiguous memory layout, zero fragmentation, and **zero-malloc runtime** during the game loop.

## 🚀 Key Features

* **Cache-Friendly:** Uses **Sparse Sets** for component storage, allowing linear iteration speed close to raw array processing.
* **Modular Architecture:** Dynamic component registration allows libraries and plugins to define their own components independently.
* **System Manager:** Built-in phased execution system (`Startup`, `Update`, `Render`, even `Manual`) with context passing.
* **Zero-Allocation Runtime:** All memory is pre-allocated in an Arena. No garbage collection, no fragmentation.
* **Multi-Component Views:** Powerful and expressive iterator system (`ArlView`) to query entities with specific component combinations.
* **Simple API:** Pure C. No complex templates or class hierarchies.

## 📦 Architecture

ArlECS is designed around **Data-Oriented Design** principles:

* **World:** A container using an external `Armel` arena.
* **Entity:** A simple ID managed by the world.
* **Component:** Pure Old Data (POD) structs, registered dynamically.
* **System:** Functions iterating over views, organized by execution phases.

## 🛠 Integration

### Prerequisites
* C Compiler (Clang/GCC)
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
#include <ArmelECS/arlecs_system.h>

// 1. Define Components
typedef struct { float x, y; } Position;
typedef struct { float vx, vy; } Velocity;

// 2. Define Global IDs (Dynamic Registration)
uint32_t C_POS = 0;
uint32_t C_VEL = 0;

// 3. Define Context
typedef struct { float dt; /* + all you need in your systems */ } GameContext;

// 4. Define System
void sys_movement(ArlEcsWorld* world, void* raw_ctx) {
    GameContext* ctx = (GameContext*)raw_ctx;
    
    // Iterate over entities with both Position and Velocity
    ArlView view = arlecs_view(world, 2, C_VEL, C_POS);
    while (arlecs_view_next(&view)) {
        Velocity* v = (Velocity*)view.components[0];
        Position* p = (Position*)view.components[1];
        
        p->x += v->vx * ctx->dt;
        p->y += v->vy * ctx->dt;
    }
}

int main() {
    // Init Memory & World (Max 1M entities)
    Armel arena;
    arl_new(&arena, 64 * 1024 * 1024);
    ArlEcsWorld* world = arlecs_world_create(&arena, 1000000);
    
    // Register Components (Assigns IDs dynamically)
    C_POS = arlecs_component_new(world, Position);
    C_VEL = arlecs_component_new(world, Velocity);

    // Create Entity
    ArlEntity e = arlecs_create_entity(world);
    arlecs_add_component(world, e, C_POS);
    Velocity* v = arlecs_add_component(world, e, C_VEL);
    v->vx = 10.0f; v->vy = 5.0f;

    // Setup Systems
    ArlSystemManager sys_mgr;
    arlecs_sys_init(&sys_mgr);
    arlecs_sys_register(&sys_mgr, "Movement", ARL_PHASE_UPDATE, sys_movement);

    // Game Loop
    GameContext ctx = { .dt = 0.016f };
    arlecs_sys_run_phase(&sys_mgr, world, ARL_PHASE_UPDATE, &ctx);

    // Cleanup
    arl_free(&arena);
    return 0;
}
```

## 📊 Performance

Benchmarks running on Apple M4 (Single Core):

| Operation | Scale | Time (avg) |
| :--- | :--- | :--- |
| **Creation** | 1,000,000 Entities | ~3.04 ms |
| **Iteration** | 1,000,000 Components (Read/Write) | **~0.60 ms** |
| **Complex View** | 1M Entities, 3 Systems, Logic | ~6.60 ms |

## 📜 License

zlib License – do whatever you want, just don't forget to give credit 😉

---

## ✍️ Author

Made with ❤️ by Vincent Huster