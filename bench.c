#include <stdio.h>
#include <Armel/armel.h>
#include <ArmelECS/arlecs.h>
#include <ArmelECS/arlecs_view.h>
#include <Armel/armel_bench.h>

// --- SETUP ---

#define ENTITY_COUNT 1000000 // 1 Million d'entités !
#define MEMORY_SIZE (256 * 1024 * 1024) // 256 MB pour être large

typedef struct { float x, y; } Position;
typedef struct { float vx, vy; } Velocity;
typedef struct { int dummy[4]; } HeavyData; // Pour polluer le cache si on l'utilisait mal

typedef struct { float life; float max_life; } Life;
typedef struct { float density; } Mass; // Juste pour le tag "Étoile Lourde"

enum { C_POS, C_VEL, C_LIFE, C_HEAVY, C_MASS };

// --- FONCTIONS DE BENCHMARK ---

// 1. Test de Création pure
// Combien de temps pour créer 1M d'entités et leur attacher un composant ?
uint64_t bench_creation(void) {
    Armel arena;
    arl_new(&arena, MEMORY_SIZE);
    ArlEcsWorld* world = arlecs_world_create(&arena);
    
    arlecs_component_new(world, C_POS, Position, ENTITY_COUNT);

    uint64_t start = arl_now_ns();

    for (int i = 0; i < ENTITY_COUNT; i++) {
        ArlEntity e = arlecs_create_entity(world);
        // On attache directement (simulation d'un chargement de niveau)
        arlecs_add_component(world, e, C_POS);
    }

    uint64_t end = arl_now_ns();

    arl_free(&arena);
    return end - start;
}

// 2. Test d'Itération Simple (Le cas le plus favorable)
// Itérer sur 1M de positions pour écrire dedans.
uint64_t bench_iterate_single(void) {
    // A. Setup (Hors chrono)
    Armel arena;
    arl_new(&arena, MEMORY_SIZE);
    ArlEcsWorld* world = arlecs_world_create(&arena);
    arlecs_component_new(world, C_POS, Position, ENTITY_COUNT);

    for (int i = 0; i < ENTITY_COUNT; i++) {
        ArlEntity e = arlecs_create_entity(world);
        Position* p = arlecs_add_component(world, e, C_POS);
        p->x = 0; p->y = 0;
    }

    // B. Chrono
    uint64_t start = arl_now_ns();

    ArlView view = arlecs_view(world, 1, C_POS);
    float total_x = 0; // Checksum

    while (arlecs_view_next(&view)) {
        Position* p = (Position*)view.components[0];
        p->x += 1.0f;
        total_x += p->x; // Force le calcul
    }

    uint64_t end = arl_now_ns();

    // Empêche le compilateur de supprimer la boucle
    if (total_x < 0) printf("Impossible"); 

    arl_free(&arena);
    return end - start;
}

// 3. Test "Physique" (Pos + Vel)
// C'est le vrai test de la Vue : regarder 2 pools en même temps.
uint64_t bench_iterate_physics(void) {
    Armel arena;
    arl_new(&arena, MEMORY_SIZE);
    ArlEcsWorld* world = arlecs_world_create(&arena);
    arlecs_component_new(world, C_POS, Position, ENTITY_COUNT);
    arlecs_component_new(world, C_VEL, Velocity, ENTITY_COUNT);

    for (int i = 0; i < ENTITY_COUNT; i++) {
        ArlEntity e = arlecs_create_entity(world);
        arlecs_add_component(world, e, C_POS);
        Velocity* v = arlecs_add_component(world, e, C_VEL);
        v->vx = 1.0f; v->vy = 1.0f;
    }

    uint64_t start = arl_now_ns();

    // On itère sur VEL (le plus petit idéalement, ici égaux) et on check POS
    ArlView view = arlecs_view(world, 2, C_VEL, C_POS);
    while (arlecs_view_next(&view)) {
        Velocity* v = (Velocity*)view.components[0];
        Position* p = (Position*)view.components[1];
        p->x += v->vx;
        p->y += v->vy;
    }

    uint64_t end = arl_now_ns();

    arl_free(&arena);
    return end - start;
}

// 4. Test "Fragmentation" (Sparse Set Power)
// On a 1M d'entités avec POS.
// Seulement 1 sur 10 (100k) a une VELOCITY.
// Le test : Est-ce que la boucle saute efficacement les 900k inutiles ?
uint64_t bench_iterate_sparse(void) {
    Armel arena;
    arl_new(&arena, MEMORY_SIZE);
    ArlEcsWorld* world = arlecs_world_create(&arena);
    
    arlecs_component_new(world, C_POS, Position, ENTITY_COUNT);
    arlecs_component_new(world, C_VEL, Velocity, ENTITY_COUNT);

    for (int i = 0; i < ENTITY_COUNT; i++) {
        ArlEntity e = arlecs_create_entity(world);
        arlecs_add_component(world, e, C_POS);
        
        // Seulement 10% ont une vitesse
        if (i % 10 == 0) {
            arlecs_add_component(world, e, C_VEL);
        }
    }

    uint64_t start = arl_now_ns();

    // Astuce ArlECS : Mettre le composant le plus rare (VEL) en PREMIER
    // Comme ça, on boucle sur 100k éléments, et on check POS (qui est présent).
    // Si on faisait l'inverse, on bouclerait sur 1M pour rien.
    ArlView view = arlecs_view(world, 2, C_VEL, C_POS);
    
    int count = 0;
    while (arlecs_view_next(&view)) {
        Velocity* v = (Velocity*)view.components[0];
        Position* p = (Position*)view.components[1];
        p->x += v->vx;
        count++;
    }

    uint64_t end = arl_now_ns();

    if (count != ENTITY_COUNT / 10) printf("⚠️ Error in sparse count\n");

    arl_free(&arena);
    return end - start;
}


// --- BENCHMARK : STELLAR COLLAPSE // 

// 1. Système Gravité : N'affecte QUE les objets ayant une MASSE (10%)
// Tire les étoiles lourdes vers le centre (0,0)
static inline void sys_gravity(ArlEcsWorld* world, float dt) {
    // Vue sur 3 composants : Mass, Vel, Pos
    // On met MASS en premier car c'est le plus rare (100k vs 1M) -> Optimisation cruciale
    ArlView v = arlecs_view(world, 3, C_MASS, C_VEL, C_POS);

    while (arlecs_view_next(&v)) {
        Velocity* vel = (Velocity*)v.components[1];
        Position* pos = (Position*)v.components[2];

        // Gravité simplifiée vers le centre
        // Si on est loin, on est tiré fort.
        float dist_sq = (pos->x * pos->x) + (pos->y * pos->y);
        if (dist_sq > 1.0f) {
            float force = 100.0f / dist_sq; 
            vel->vx -= pos->x * force * dt;
            vel->vy -= pos->y * force * dt;
        }
    }
}

// 2. Système Cinématique : Tout le monde bouge
static inline void sys_kinematics(ArlEcsWorld* world, float dt) {
    ArlView v = arlecs_view(world, 2, C_VEL, C_POS);

    while (arlecs_view_next(&v)) {
        Velocity* vel = (Velocity*)v.components[0];
        Position* pos = (Position*)v.components[1];

        pos->x += vel->vx * dt;
        pos->y += vel->vy * dt;

        // Amortissement (Friction de l'espace)
        vel->vx *= 0.99f;
        vel->vy *= 0.99f;
    }
}

// 3. Système de Vie : Vieillissement et Respawn
static inline void sys_life_cycle(ArlEcsWorld* world, float dt) {
    // On a besoin de POS et VEL pour resetter l'étoile si elle meurt
    ArlView v = arlecs_view(world, 3, C_LIFE, C_POS, C_VEL);

    while (arlecs_view_next(&v)) {
        Life* l = (Life*)v.components[0];
        
        l->life -= dt;

        if (l->life <= 0) {
            // REBIRTH ! (Reset des composants)
            l->life = l->max_life;
            
            Position* p = (Position*)v.components[1];
            p->x = (float)(rand() % 100 - 50); // Reset au centre
            p->y = (float)(rand() % 100 - 50);

            Velocity* vel = (Velocity*)v.components[2];
            vel->vx = (float)(rand() % 10 - 5); // Explosion
            vel->vy = (float)(rand() % 10 - 5);
        }
    }
}


uint64_t run_game_loop_bench(void) {
    srand(42); // Seed fixe pour reproductibilité

    Armel arena;
    arl_new(&arena, MEMORY_SIZE);
    ArlEcsWorld* world = arlecs_world_create(&arena);

    // Register
    arlecs_component_new(world, C_POS, Position, ENTITY_COUNT);
    arlecs_component_new(world, C_VEL, Velocity, ENTITY_COUNT);
    arlecs_component_new(world, C_LIFE, Life, ENTITY_COUNT);
    arlecs_component_new(world, C_MASS, Mass, ENTITY_COUNT);

    // Populate
    printf("    ... Spawning %d stars ...\n", ENTITY_COUNT);
    for (int i = 0; i < ENTITY_COUNT; i++) {
        ArlEntity e = arlecs_create_entity(world);
        
        Position* p = arlecs_add_component(world, e, C_POS);
        p->x = (float)(rand() % 2000 - 1000);
        p->y = (float)(rand() % 2000 - 1000);

        Velocity* v = arlecs_add_component(world, e, C_VEL);
        v->vx = 0; v->vy = 0;

        Life* l = arlecs_add_component(world, e, C_LIFE);
        l->max_life = 10.0f + (rand() % 10);
        l->life = l->max_life; // Random start life to desync deaths

        // 10% sont des étoiles à neutrons (Massives)
        if (i % 10 == 0) {
            arlecs_add_component(world, e, C_MASS);
        }
    }

    printf("    ... Running Simulation (1 Frame logic) ...\n");

    uint64_t start = arl_now_ns();

    // Simulation d'une frame à dt = 0.016 (60 FPS)
    float dt = 0.016f;

    // L'ordre des systèmes compte !
    sys_gravity(world, dt);    // 1. Appliquer les forces (Sparse)
    sys_kinematics(world, dt); // 2. Intégrer le mouvement (Dense)
    sys_life_cycle(world, dt); // 3. Gérer la logique de jeu (Branching)

    uint64_t end = arl_now_ns();

    arl_free(&arena);
    return end - start;
}

// --- ENDOF BENCHMARK : STELLAR COLLAPSE // 


// --- MAIN ---

int main() {
    // On désactive le buffer pour voir la progression
    setbuf(stdout, NULL);

    printf("==========================================\n");
    printf("    🔥 ArlECS HARDCORE BENCHMARKS 🔥      \n");
    printf("    Entities: %d | Runs: %d \n", ENTITY_COUNT, ARL_BENCH_REPEAT);
    printf("==========================================\n");

    arl_bench_avg("Creation (1M entities + Comp)", bench_creation);
    arl_bench_avg("Iterate Single (1M Pos)", bench_iterate_single);
    arl_bench_avg("Iterate Dual (1M Pos + Vel)", bench_iterate_physics);
    arl_bench_avg("Iterate Sparse (100k active / 1M)", bench_iterate_sparse);

	printf("\n==========================================\n");
    printf(" 🌌 GALAXY COLLAPSE : FULL SYSTEM TEST 🌌 \n");
    printf("    Entities: %d \n", ENTITY_COUNT);
    printf("==========================================\n");

    arl_bench_avg("Full Game Loop (3 Systems)", run_game_loop_bench);

    printf("\n✅ Benchmarks finished.\n");
    return 0;
}