#ifndef ARLECS_SYSTEM_H
#define ARLECS_SYSTEM_H

#include <ArmelECS/arlecs.h>

// Signature d'une fonction système standard
// dt = delta time (temps écoulé depuis la dernière frame)
typedef void (*ArlSystemFunc)(ArlEcsWorld* world, float dt);

typedef struct {
    const char* name;      // Pour le debug / profiling
    ArlSystemFunc update;  // La fonction à appeler
    bool active;           // Pour mettre en pause un système (ex: Menu vs Jeu)
} ArlSystem;

// Le Manager (peut être inclus dans ArlEcsWorld ou géré à part)
#define ARLECS_MAX_SYSTEMS 32

typedef struct {
    ArlSystem systems[ARLECS_MAX_SYSTEMS];
    uint32_t count;
} ArlSystemManager;

// --- API Inline (Simple et Efficace) ---

static inline void arlecs_sys_init(ArlSystemManager* mgr) {
    mgr->count = 0;
}

static inline void arlecs_sys_register(ArlSystemManager* mgr, const char* name, ArlSystemFunc func) {
    if (mgr->count >= ARLECS_MAX_SYSTEMS) return;
    mgr->systems[mgr->count].name = name;
    mgr->systems[mgr->count].update = func;
    mgr->systems[mgr->count].active = true;
    mgr->count++;
}

static inline void arlecs_sys_update(ArlSystemManager* mgr, ArlEcsWorld* world, float dt) {
    for (uint32_t i = 0; i < mgr->count; i++) {
        if (mgr->systems[i].active) {
            mgr->systems[i].update(world, dt);
        }
    }
}

#endif