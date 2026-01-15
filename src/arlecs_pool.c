#include <ArmelECS/arlecs_pool.h>

ArlPool* arlecs_pool_new(Armel* arena, size_t elem_size, uint32_t max_entities) {
	// 1. Alloue la structure de gestion
	ArlPool* pool = arl_make(arena, ArlPool);
	
	pool->elem_size = elem_size;
	pool->capacity  = max_entities;
	pool->count     = 0;

	// 2. Alloue les tableaux (Sparse, Dense, Data)
	// Le Sparse doit être initialisé à "VIDE" (0xFF...)
	pool->sparse = arl_array(arena, uint32_t, max_entities);
	memset(pool->sparse, 0xFF, max_entities * sizeof(uint32_t)); 

	pool->dense = arl_array(arena, ArlEntity, max_entities);
	
	// Data brute : on alloue capacity * taille_du_composant
	pool->data  = (uint8_t*)arl_alloc(arena, max_entities * elem_size);

	return pool;
}

void* arlecs_pool_add(ArlPool* pool, ArlEntity entity) {
	if (entity >= pool->capacity) return NULL;

	// Si déjà présent, on renvoie l'existant
	if (pool->sparse[entity] != ARL_NULL_ID) {
		return pool->data + (pool->sparse[entity] * pool->elem_size);
	}

	// Sinon, on ajoute à la fin du tableau dense
	uint32_t index = pool->count;
	
	pool->sparse[entity] = index; 
	pool->dense[index]   = entity;
	
	pool->count++;

	return pool->data + (index * pool->elem_size);
}


void arlecs_pool_remove(ArlPool* pool, ArlEntity entity) {
	if (entity >= pool->capacity) return;
	
	uint32_t index_removed = pool->sparse[entity];
	if (index_removed == ARL_NULL_ID) return; // Rien à supprimer

	uint32_t index_last = pool->count - 1;

	// SWAP & POP : Si ce n'est pas le dernier, on déplace le dernier dans le trou
	if (index_removed != index_last) {
		ArlEntity entity_last = pool->dense[index_last];

		// 1. Déplacer la DATA brute (memcpy) du dernier vers le trou
		uint8_t* dst = pool->data + (index_removed * pool->elem_size);
		uint8_t* src = pool->data + (index_last * pool->elem_size);
		memcpy(dst, src, pool->elem_size);

		// 2. Mettre à jour les liens
		pool->dense[index_removed] = entity_last;
		pool->sparse[entity_last]  = index_removed;
	}

	// Nettoyage
	pool->sparse[entity] = ARL_NULL_ID;
	pool->count--;
}