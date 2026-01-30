#include <ArmelECS/arlecs.h>


ArlEcsWorld* arlecs_world_create(Armel* armel, uint32_t max_entities) {
	ArlEcsWorld* w = arl_make(armel, ArlEcsWorld);

	w->arena = armel;
	w->entity_counter = 0;
	w->max_entities = max_entities;
	w->component_counter = 0;

	for (int i = 0; i < ARLECS_MAX_COMPONENT_TYPES; i++) {
		w->pools[i] = NULL;
	}

	return w;
}


ArlEntity arlecs_create_entity(ArlEcsWorld* world) {
	return world->entity_counter++;
}


uint32_t arlecs_register_component(ArlEcsWorld* world, size_t size) {
	assert(world->component_counter < ARLECS_MAX_COMPONENT_TYPES && "ArlECS Error: Component ID out of bounds");

	uint32_t new_id = world->component_counter;

	world->pools[new_id] = arlecs_pool_new(world->arena, size, world->max_entities);
	world->component_counter++;

	return new_id;
}


// Ajoute un composant à une entité
void* arlecs_add_component(ArlEcsWorld* world, ArlEntity entity, uint32_t component_id) {
	assert(world->pools[component_id] != NULL && "ArlEcs Error: Unknown component");
	assert(entity < world->entity_counter && "ArlEcs Error: Unknown entity");

	return arlecs_pool_add(world->pools[component_id], entity);
}


// Récupère un composant
void* arlecs_get_component(ArlEcsWorld* world, ArlEntity entity, uint32_t component_id) {
	assert(entity <= world->entity_counter && "ArlEcs Error: Unknown entity");

	if (component_id >= ARLECS_MAX_COMPONENT_TYPES) return NULL;

	ArlPool* pool = world->pools[component_id];
	if (! pool) return NULL;

	return arlecs_pool_get(pool, entity);
}


// Supprime un composant
void arlecs_remove_component(ArlEcsWorld* world, ArlEntity entity, uint32_t component_id) {
	if (component_id >= ARLECS_MAX_COMPONENT_TYPES) return;

	assert(entity <= world->entity_counter && "ArlEcs Error: Unknown entity");
	ArlPool* pool = world->pools[component_id];
	if (pool) arlecs_pool_remove(world->pools[component_id], entity);
}