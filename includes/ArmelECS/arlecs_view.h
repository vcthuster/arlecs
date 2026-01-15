#ifndef ARLECS_VIEW
#define ARLECS_VIEW

#include <stdarg.h>
#include <ArmelECS/arlecs_pool.h>
#include <ArmelECS/arlecs.h> // Required for ArlEcsWorld definition

/** Maximum number of components queryable in a single view. */
#define ARLECS_VIEW_MAX_COMPONENTS 8

/**
 * @brief Multi-Component Iterator (View).
 * * Allows iterating over entities that possess ALL specified components.
 * * Performance Note:
 * The iteration speed depends on the FIRST component passed (The "Master").
 * Always put the component with the FEWEST active entities first.
 */
typedef struct {
	// [Internal State]
	ArlEcsWorld* world;
	ArlPool* pools[ARLECS_VIEW_MAX_COMPONENTS]; ///< Pointers to the required pools.
	uint32_t pools_count;                       ///< Number of components requested.
	uint32_t current_index;                     ///< Cursor on the Master pool.
	
	// [Output] - Publicly accessible in the loop
	ArlEntity entity;                             ///< The current Entity ID.
	void* components[ARLECS_VIEW_MAX_COMPONENTS]; ///< Pointers to component data (typeless).
} ArlView;

/**
 * @brief Initializes a view to iterate over entities with specific components.
 * @param world The ECS world.
 * @param count Number of components to query.
 * @param ... Variadic list of Component IDs (e.g., COMP_POS, COMP_VEL).
 * @return An initialized ArlView ready for iteration.
 */
static inline ArlView arlecs_view(ArlEcsWorld* world, uint32_t count, ...) {
	ArlView view;
	view.world = world;
	view.pools_count = count > ARLECS_VIEW_MAX_COMPONENTS ? ARLECS_VIEW_MAX_COMPONENTS : count;
	view.current_index = 0;
	view.entity = ARL_NULL_ID;

	// Retrieve variadic arguments
	va_list args;
	va_start(args, count);

	for (uint32_t i = 0; i < view.pools_count; i++) {
		uint32_t comp_id = va_arg(args, uint32_t);
		
		// Store pool pointer if ID is valid
		view.pools[i] = comp_id < ARLECS_MAX_COMPONENT_TYPES
			? world->pools[comp_id]
			: NULL;
	}

	va_end(args);
	return view;
}

/**
 * @brief Advances the iterator to the next matching entity.
 * @param view Pointer to the view.
 * @return true if a match was found (loop continues), false if finished.
 */
static inline bool arlecs_view_next(ArlView* view) {
	// Master Pool Strategy: We iterate linearly on the first pool
	ArlPool* master = view->pools[0];
	if (!master) return false;

	while (view->current_index < master->count) {
		
		// 1. Candidate Selection (Dense array access = Fast)
		ArlEntity candidate = master->dense[view->current_index];
		bool match = true;

		// 2. Intersection Check: Do other pools have this entity?
		for (uint32_t i = 1; i < view->pools_count; i++) {
			ArlPool* p = view->pools[i];
			if (!p || ! arlecs_pool_has(p, candidate)) {
				match = false;
				break; // Missing a component, skip this entity
			}
		}

		if (match) {
			// Found a valid entity! Fill the output data.
			view->entity = candidate;
			
			// Master component: Direct calculation (No lookup needed)
			view->components[0] = master->data + (view->current_index * master->elem_size);

			// Other components: Sparse lookup
			for (uint32_t i = 1; i < view->pools_count; i++) {
				view->components[i] = arlecs_pool_get(view->pools[i], candidate);
			}

			// Prepare index for next call
			view->current_index++;
			return true;
		}

		// No match, try next entity in master pool
		view->current_index++;
	}

	return false;
}

#endif