#include <ArmelECS/arlecs.h>


ArlEcsWorld* arlecs_world_create(Armel* armel) {
	ArlEcsWorld* w = arl_make(armel, ArlEcsWorld);

	w->arena = armel;
	w->entity_counter = 0;

	for (int i = 0; i < ARLECS_MAX_COMPONENT_TYPES; i++) {
		w->pools[i] = NULL;
	}

	return w;
}


ArlEntity arlecs_create_entity(ArlEcsWorld* world) {
	return world->entity_counter++;
}


void arlecs_register_component(ArlEcsWorld* world, uint32_t component_id, size_t size, uint32_t max_entities) {
	assert(component_id < ARLECS_MAX_COMPONENT_TYPES && "ArlECS Error: Component ID out of bounds");
	assert(world->pools[component_id] == NULL && "ArlECS Error: Component already in use");

	world->pools[component_id] = arlecs_pool_new(world->arena, size, max_entities);
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