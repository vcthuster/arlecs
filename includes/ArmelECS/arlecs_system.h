#ifndef ARLECS_SYSTEM_H
#define ARLECS_SYSTEM_H

#include <string.h>
#include <ArmelECS/arlecs.h>


typedef enum {
    ARL_PHASE_STARTUP = 0,
    ARL_PHASE_UPDATE,
    ARL_PHASE_RENDER,
    ARL_PHASE_MANUAL,
    ARL_PHASE_MAX
} ArlSystemPhase;


typedef void (*ArlSystemFunc)(ArlEcsWorld* world, void* ctx);


typedef struct {
    const char* name;      // debug / profiling
    ArlSystemFunc update;  // Function to call
    ArlSystemPhase phase;  // Phase (moment where the function will be called)
    bool active;           // Sets the system as callable or paused
} ArlSystem;


#define ARLECS_MAX_SYSTEMS 64

typedef struct {
    ArlSystem systems[ARLECS_MAX_SYSTEMS];
    uint32_t count;
} ArlSystemManager;

// ----- API -----

/**
 * @brief Initializes the ArlSystemManager.
 * @param mgr 
 */
static inline void arlecs_sys_init(ArlSystemManager* mgr) {
    mgr->count = 0;
}


/**
 * @brief Registers a system identified by its name. 
 * The callback function func is the system and will be called at the phase phase.
 * @param mgr ArlSystemManager
 * @param name The name of the system
 * @param phase The phase of the system (see ArlSystemPhase)
 * @param func The function of the system, (see ArlSystemFunc : void (*ArlSystemFunc)(ArlEcsWorld* world, void* ctx))
 */
static inline void arlecs_sys_register (ArlSystemManager* mgr, const char* name, ArlSystemPhase phase, ArlSystemFunc func) {
    if (mgr->count >= ARLECS_MAX_SYSTEMS) return;
    mgr->systems[mgr->count].name   = name;
    mgr->systems[mgr->count].update = func;
    mgr->systems[mgr->count].phase  = phase;
    mgr->systems[mgr->count].active = true;
    mgr->count++;
}


/**
 * @brief Runs all active systems.
 * @param mgr 
 * @param world 
 * @param ctx 
 */
static inline void arlecs_sys_run_all (ArlSystemManager* mgr, ArlEcsWorld* world, void* ctx) {
    for (uint32_t i = 0; i < mgr->count; i++) {
        ArlSystem* s = &mgr->systems[i];
        if (s->active) {
            s->update(world, ctx);
        }
    }
}


/**
 * @brief Runs the system belonging to the phase phase.
 * @param mgr 
 * @param world 
 * @param phase 
 * @param ctx 
 */
static inline void arlecs_sys_run_phase (ArlSystemManager* mgr, ArlEcsWorld* world, ArlSystemPhase phase, void* ctx) {
    for (uint32_t i = 0; i < mgr->count; i++) {
        ArlSystem* s = &mgr->systems[i];
        if (s->phase == phase && s->active) {
            s->update(world, ctx);
        }
    }
}


/**
 * @brief (De)Activates the system identified by its name.
 * @param mgr 
 * @param name 
 * @param is_active True or False
 */
static inline void arlecs_sys_set_active (ArlSystemManager* mgr, const char* name, bool is_active) {
    for (uint32_t i = 0; i < mgr->count; i++) {
        ArlSystem* s = &mgr->systems[i];
        if (strcmp(s->name, name) == 0) {
            s->active = is_active;
        }
    }
}

#endif