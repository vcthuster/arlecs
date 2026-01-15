#include <ArmelECS/arlecs.h>
#include <Armel/armel_test.h>

// --- FIXTURES (Test data) ---

typedef struct {
	float x, y;
	int hp;
} ComponentTest;

// --- TESTS ---

ARMEL_TEST(test_creation) {
	Armel arena;
	arl_new(&arena, 1024);

	ArlPool* pool = arlecs_pool_new(&arena, sizeof(ComponentTest), 10);

	assert(pool != NULL);
	assert(pool->count == 0);
	assert(pool->capacity == 10);
	assert(pool->elem_size == sizeof(ComponentTest));

	arl_free(&arena);
}

ARMEL_TEST(test_add_get) {
	Armel arena;
	arl_new(&arena, 4096);
	ArlPool* pool = arlecs_pool_new(&arena, sizeof(ComponentTest), 100);

	// Ajout d'une entité (ID 42)
	ComponentTest* c = arlecs_pool_add(pool, 42);
	assert(c != NULL);
	c->x = 10.0f; c->y = 20.0f; c->hp = 100;

	assert(pool->count == 1);
	assert(arlecs_pool_has(pool, 42) == true);
	assert(arlecs_pool_has(pool, 99) == false);

	// Récupération
	ComponentTest* get_back = arlecs_pool_get(pool, 42);
	assert(get_back == c); // Doit pointer vers la même adresse
	assert(get_back->hp == 100);

	// Ajout d'une entité lointaine (ID 99)
	ComponentTest* c2 = arlecs_pool_add(pool, 99);
	c2->hp = 50;

	assert(pool->count == 2);
	
	ComponentTest* c99 = (ComponentTest*)arlecs_pool_get(pool, 99);
	assert(c99 != NULL);
	assert(c99->hp == 50);

	arl_free(&arena);
}

ARMEL_TEST(test_remove_swap_pop) {
	Armel arena;
	arl_new(&arena, 4096);
	ArlPool* pool = arlecs_pool_new(&arena, sizeof(int), 100); // On stocke juste des int pour simplifier

	// On remplit : [A(ID 10), B(ID 20), C(ID 30)]
	*(int*)arlecs_pool_add(pool, 10) = 111;
	*(int*)arlecs_pool_add(pool, 20) = 222;
	*(int*)arlecs_pool_add(pool, 30) = 333;

	assert(pool->count == 3);

	// Suppression de celui du milieu (ID 20)
	// C (30) devrait prendre sa place en mémoire dense
	arlecs_pool_remove(pool, 20);

	assert(pool->count == 2);
	assert(arlecs_pool_has(pool, 20) == false);
	assert(arlecs_pool_get(pool, 20) == NULL);

	// Vérification de l'intégrité des restants
	assert(*(int*)arlecs_pool_get(pool, 10) == 111);
	assert(*(int*)arlecs_pool_get(pool, 30) == 333); // C doit toujours être accessible via son ID

	// Vérification structurelle interne (Dense array)
	// Dense[0] doit être 10
	// Dense[1] doit être 30 (car Swap & Pop a déplacé le dernier ici)
	assert(pool->dense[0] == 10);
	assert(pool->dense[1] == 30);

	arl_free(&arena);
}

ARMEL_TEST(test_double_add) {
	Armel arena;
	arl_new(&arena, 1024);
	ArlPool* pool = arlecs_pool_new(&arena, sizeof(int), 10);

	int* p1 = arlecs_pool_add(pool, 5);
	*p1 = 123;

	// Ajouter une entité déjà existante ne doit PAS créer de doublon
	// mais renvoyer le pointeur existant
	int* p2 = arlecs_pool_add(pool, 5);

	assert(p1 == p2);
	assert(pool->count == 1);
	assert(*p2 == 123);

	arl_free(&arena);
}

ARMEL_TEST(test_out_of_bounds) {
	Armel arena;
	arl_new(&arena, 1024);
	// Capacité de 5 entités (IDs 0 à 4)
	ArlPool* pool = arlecs_pool_new(&arena, sizeof(int), 5);

	assert(arlecs_pool_add(pool, 0) != NULL);
	assert(arlecs_pool_add(pool, 4) != NULL);
	
	// ID 5 est hors limite (capacity = 5)
	assert(arlecs_pool_add(pool, 5) == NULL);

	arl_free(&arena);
}


// --- Mocks (Faux composants pour tester) ---
typedef struct { float x, y; } Pos;
typedef struct { float vx, vy; } Vel;
typedef struct { int hp; } Health;

enum { C_POS = 0, C_VEL = 1, C_HP = 2 };

// --- TESTS WORLD ---

ARMEL_TEST(test_world_lifecycle) {
	Armel arena;
	arl_new(&arena, 1024 * 1024); // 1 MB

	ArlEcsWorld* world = arlecs_world_create(&arena);
	assert(world != NULL);
	assert(world->entity_counter == 0);

	// Test création entité
	ArlEntity e1 = arlecs_create_entity(world);
	ArlEntity e2 = arlecs_create_entity(world);

	assert(e1 == 0);
	assert(e2 == 1);
	assert(world->entity_counter == 2);

	arl_free(&arena); // Destruction totale
}

ARMEL_TEST(test_components_data) {
	Armel arena;
	arl_new(&arena, 1024 * 1024);
	ArlEcsWorld* world = arlecs_world_create(&arena);

	// Enregistrement
	arlecs_component_new(world, C_POS, Pos, 100);
	arlecs_component_new(world, C_HP, Health, 100);

	ArlEntity player = arlecs_create_entity(world);

	// Ajout et écriture
	Pos* p = arlecs_add_component(world, player, C_POS);
	p->x = 10.0f; p->y = 20.0f;

	Health* h = arlecs_add_component(world, player, C_HP);
	h->hp = 100;

	// Relecture
	Pos* p_read = arlecs_get_component(world, player, C_POS);
	assert(p_read == p); // Doit être la même adresse
	assert(p_read->x == 10.0f);

	// Test non-possession
	assert(arlecs_get_component(world, player, C_VEL) == NULL); // Pas de Velocity enregistré

	arl_free(&arena);
}

ARMEL_TEST(test_view_filtering) {
	Armel arena;
	arl_new(&arena, 1024 * 1024);
	ArlEcsWorld* world = arlecs_world_create(&arena);

	// Setup: Pos, Vel, HP
	arlecs_component_new(world, C_POS, Pos, 100);
	arlecs_component_new(world, C_VEL, Vel, 100);
	arlecs_component_new(world, C_HP, Health, 100);

	// Création de 3 entités avec des archétypes différents
	
	// E0: Juste POS
	ArlEntity e0 = arlecs_create_entity(world);
	arlecs_add_component(world, e0, C_POS);

	// E1: POS + VEL (Celle qu'on veut trouver !)
	ArlEntity e1 = arlecs_create_entity(world);
	arlecs_add_component(world, e1, C_POS);
	arlecs_add_component(world, e1, C_VEL);

	// E2: POS + HP
	ArlEntity e2 = arlecs_create_entity(world);
	arlecs_add_component(world, e2, C_POS);
	arlecs_add_component(world, e2, C_HP);

	// TEST DE LA VUE : On cherche [POS + VEL]
	// Seul E1 devrait matcher.
	
	int match_count = 0;
	ArlView view = arlecs_view(world, 2, C_POS, C_VEL); // 2 composants demandés

	while (arlecs_view_next(&view)) {
		match_count++;
		
		// Vérification de l'ID
		assert(view.entity == e1); 
		
		// Vérification que les pointeurs ne sont pas NULL
		assert(view.components[0] != NULL); // POS
		assert(view.components[1] != NULL); // VEL
	}

	assert(match_count == 1); // On ne doit avoir trouvé que E1

	arl_free(&arena);
}

ARMEL_TEST(test_view_removal_safety) {
	// Ce test vérifie si supprimer un composant rend la vue invalide (ce qui est bien)
	Armel arena;
	arl_new(&arena, 1024 * 1024);
	ArlEcsWorld* world = arlecs_world_create(&arena);

	arlecs_component_new(world, C_POS, Pos, 10);
	arlecs_component_new(world, C_VEL, Vel, 10);

	ArlEntity e = arlecs_create_entity(world);
	arlecs_add_component(world, e, C_POS);
	arlecs_add_component(world, e, C_VEL);

	// On supprime VEL
	arlecs_remove_component(world, e, C_VEL);

	// La vue [POS + VEL] ne doit plus rien trouver
	ArlView view = arlecs_view(world, 2, C_POS, C_VEL);
	bool found = arlecs_view_next(&view);

	assert(found == false);

	arl_free(&arena);
}




// --- MAIN ---

int main() {
	setbuf(stdout, NULL);

	printf("==========================================\n");
	printf("    🧪  ArlECS (Sparse Set) Tests         \n");
	printf("==========================================\n");

	RUN_TEST(test_creation);
	RUN_TEST(test_add_get);
	RUN_TEST(test_remove_swap_pop);
	RUN_TEST(test_double_add);
	RUN_TEST(test_out_of_bounds);

	RUN_TEST(test_world_lifecycle);
	RUN_TEST(test_components_data);
	RUN_TEST(test_view_filtering);
	RUN_TEST(test_view_removal_safety);

	printf("\n🎉 All tests passed successfully!\n");
	return 0;
}