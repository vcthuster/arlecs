#ifndef ARL_POOL_H
#define ARL_POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <Armel/armel.h>


/**
 * @brief Unique identifier for an entity.
 * An entity is just an index. It contains no data itself.
 */
typedef uint32_t ArlEntity;

/**
 * @brief Sentinel value representing an invalid or null entity ID.
 * Equivalent to (uint32_t)-1.
 */
#define ARL_NULL_ID 0xFFFFFFFF

/**
 * @brief A Generic Sparse Set implementation.
 * * Stores ONE type of component (e.g., Position) for entities.
 * It uses a dual-array system (Sparse + Dense) to provide:
 * 1. O(1) Lookup: sparse[entity_id] -> index
 * 2. O(1) Iteration: dense[0...count] are packed contiguously
 */
typedef struct {
	size_t elem_size;      ///< Size of a single component in bytes.
	uint32_t count;        ///< Number of active components.
	uint32_t capacity;     ///< Maximum number of entities supported (Fixed).

	uint32_t* sparse;      ///< [EntityID] -> Index in 'dense' array.
	ArlEntity* dense;      ///< [Index] -> EntityID (Reverse map).
	uint8_t* data;         ///< [Index] -> Packed component data.
} ArlPool;

// --- API ---

/**
 * @brief Allocates and initializes a new component pool.
 * @param arena The memory arena to use.
 * @param elem_size Size of the component struct (sizeof(T)).
 * @param max_entities Hard limit on the number of entities this pool can track.
 * @return A pointer to the new pool.
 */
ArlPool* arlecs_pool_new(Armel* arena, size_t elem_size, uint32_t max_entities);

/**
 * @brief Adds a component to an entity.
 * If the entity already has this component, it returns the existing data.
 * @return A pointer to the memory where data should be written.
 */
void* arlecs_pool_add(ArlPool* pool, ArlEntity entity);

/**
 * @brief Removes a component from an entity using "Swap & Pop".
 * @warning This moves the last element of the array to fill the hole.
 * Any pointers to components of this type held externally may become invalid.
 */
void arlecs_pool_remove(ArlPool* pool, ArlEntity entity);

/**
 * @brief Retrieves a component for an entity (Inline for performance).
 * @return Pointer to the data, or NULL if not present.
 */
static inline void* arlecs_pool_get(ArlPool* pool, ArlEntity entity) {
	if (entity >= pool->capacity) return NULL;
	
	uint32_t index = pool->sparse[entity];

	// Check if the index points to a valid entry in the dense array
	// (Double check required for sparse set validity)
	if (index >= pool->count || pool->dense[index] != entity) return NULL;

	return pool->data + (index * pool->elem_size);
}

/**
 * @brief Checks if an entity possesses this component (Inline).
 * @return true if present, false otherwise.
 */
static inline bool arlecs_pool_has(ArlPool* pool, ArlEntity entity) {
	if (entity >= pool->capacity) return false;
	
	uint32_t index = pool->sparse[entity];
	// We confirm validity by checking if the dense array points back to us
	return index < pool->count && pool->dense[index] == entity;
}

#endif