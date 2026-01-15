/*
 * ArlECS - A lightweight ECS based on Armel allocator.
 * Copyright (c) 2025 Vincent Huster
 * Licensed under the zlib License (see LICENSE file).
 */

#ifndef ARLECS_H
#define ARLECS_H

#include <ArmelECS/arlecs_pool.h>

/** Maximum number of distinct component types (IDs) allowed in the world. */
#define ARLECS_MAX_COMPONENT_TYPES 32

/**
 * @brief The main container for the ECS.
 * Holds the memory arena, the entity counter, and pointers to component pools.
 */
typedef struct {
	Armel* arena; ///< Pointer to the memory arena used for allocations.

	uint32_t entity_counter; ///< The next available entity ID.
	
	// Recyclage des IDs (Optionnel pour la v1, mais à prévoir)
	// On utilisera un tableau dynamique pour stocker les IDs des entités mortes
	// afin de les réattribuer avant d'augmenter entity_counter.
	// (On verra ça dans la v2 pour ne pas complexifier tout de suite)

	ArlPool* pools[ARLECS_MAX_COMPONENT_TYPES]; ///< Sparse sets for each component type.

} ArlEcsWorld;


// --- API ---

/**
 * @brief Creates a new ECS World within the given memory arena.
 * @param armel Pointer to an initialized Armel arena.
 * @return A pointer to the created World.
 */
ArlEcsWorld* arlecs_world_create(Armel* armel);

/**
 * @brief Creates a new entity.
 * @return A unique Entity ID (uint32_t).
 */
ArlEntity arlecs_create_entity(ArlEcsWorld* world);

/**
 * @brief Registers a component type in the world.
 * Use the macro arlecs_component_new() instead for type safety.
 * @param component_id The unique ID (enum) for this component type.
 * @param size The size of the struct in bytes.
 * @param max_entities The capacity of the pool (cannot be resized).
 */
void arlecs_register_component(ArlEcsWorld* world, uint32_t component_id, size_t size, uint32_t max_entities);

/**
 * @brief Helper macro to register a component safely.
 * Usage: arlecs_component_new(world, COMP_POS, Position, 1000);
 */
#define arlecs_component_new(WORLD,ID,TYPE,NB) \
	arlecs_register_component(WORLD, ID, sizeof(TYPE), NB);

/**
 * @brief Adds a component to an entity.
 * @return A pointer to the newly allocated component memory (zero-initialized or undefined).
 */
void* arlecs_add_component(ArlEcsWorld* world, ArlEntity entity, uint32_t component_id);

/**
 * @brief Retrieves a component for a given entity.
 * @return A pointer to the component data, or NULL if the entity does not have it.
 */
void* arlecs_get_component(ArlEcsWorld* world, ArlEntity entity, uint32_t component_id);

/**
 * @brief Removes a component from an entity.
 */
void arlecs_remove_component(ArlEcsWorld* world, ArlEntity entity, uint32_t component_id);

// Include Views at the end to ensure World definition is known
#include <ArmelECS/arlecs_view.h>

#endif