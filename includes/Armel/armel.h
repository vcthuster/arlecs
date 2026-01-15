/**
 * @file armel.h
 * @brief Fast linear memory allocator (arena/bump allocator) for C projects.
 *
 * This library provides a lightweight, low-overhead memory allocation system
 * based on the arena (a.k.a. bump pointer) strategy.
 *
 * Memory is allocated linearly from a pre-allocated buffer using a cursor.
 * This allows extremely fast allocations, predictable performance, and
 * minimal overhead compared to malloc/free.
 *
 * Key features:
 *   - Fast linear allocation with optional zeroing
 *   - Support for alignment-aware allocation
 *   - Reset and rewind capabilities
 *   - Optional soft-fail mode instead of abort()
 *   - Static and dynamic arena support
 *   - Cross-platform support (macOS, Linux, Windows)
 *
 * Use cases:
 *   - Temporary memory (e.g. per-frame or per-task)
 *   - Memory pools for performance-critical code
 *   - Allocation of short-lived objects or arrays
 *
 * The arena does not track individual allocations: memory is released all at once
 * using arl_reset(), arl_rewind_to(), or arl_free().
 *
 * The API is minimal by design and meant to be embedded directly into your project.
 *
 * Author: Vincent Huster
 * License: Zlib
 */

#ifndef ARMEL_H
#define ARMEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Armel/armel_sys.h>

/**
 * @def ARL_KB
 * @brief Number of bytes in a kilobyte (1 KB = 1024 bytes)
 */
#ifndef ARL_KB
    #define ARL_KB ((size_t)1024)
#endif

/**
 * @def ARL_MB
 * @brief Number of bytes in a megabyte (1 MB = 1024 * 1024 bytes)
 */
#ifndef ARL_MB
    #define ARL_MB (ARL_KB * 1024)
#endif

/**
 * @def ARL_GB
 * @brief Number of bytes in a gigabyte (1 GB = 1024 * 1024 * 1024 bytes)
 */
#ifndef ARL_GB
    #define ARL_GB (ARL_MB * 1024)
#endif

/**
 * @def ARL_MAX_MEMORY
 * @brief Number of bytes the Arena is able to use on the system memory, no matter what
 */
#ifndef ARL_MAX_MEMORY
	#define ARL_MAX_MEMORY 2UL * ARL_GB
#endif

/**
 * @def ARL_ALIGN
 * @brief Default memory alignment used if none is specified.
 *
 * Automatically adapts to the architecture for optimal performance.
 * 16 bytes on ARM64/x86_64, 8 bytes otherwise.
 */
#ifndef ARL_ALIGN
	#if defined(__aarch64__) || defined(__arm64__)
		#define ARL_ALIGN 16
	#elif defined(__x86_64__)
		#define ARL_ALIGN 16
	#else
		#define ARL_ALIGN 8
	#endif
#endif

/**
 * @def ARL_DEFAULT_ALIGNMENT
 * @brief Internal alias to ARL_ALIGN.
 */
#define ARL_DEFAULT_ALIGNMENT ARL_ALIGN

/**
 * @def ARL_NOFLAG
 * @brief No special behavior (default)
 */
#define ARL_NOFLAG 0

/**
 * @def ARL_SOFTFAIL
 * @brief Instead of aborting on allocation failure, return NULL.
 *
 * Useful if you want to handle out-of-memory cases manually.
 */
#define ARL_SOFTFAIL 0x01

/**
 * @def ARL_ZEROS
 * @brief Zero-fill memory during allocation (like calloc).
 *
 * Slightly slower, but ensures memory safety for uninitialized reads.
 */
#define ARL_ZEROS 0x02

/**
 * @def ARL_ALLOW_CHAIN
 * @brief instead of failing when the capacity is exceeded, chain a new Armel Arena
 */
#define ARL_ALLOW_CHAIN 0x04

/**
 * @brief Computes the total size needed to allocate N items of type T with alignment.
 *
 * This macro is useful to calculate the buffer size required before creating an arena,
 * especially when using arl_local or preallocating memory manually.
 *
 * @param T     Type of the element
 * @param N     Number of elements to allocate
 * @param ALIGN Desired memory alignment (typically ARL_ALIGN)
 *
 * @return Size in bytes to allocate
 *
 * Example:
 *     size_t size = arl_size(MyStruct, 32, ARL_ALIGN);
 */
#define arl_size(T, N, ALIGN) \
	(arl_align_up(sizeof(T), (ALIGN)) * (N))

/**
 * @brief Allocates and returns a pointer to a single instance of type T.
 *
 * This is a shorthand macro to allocate one object of type T from the arena.
 * The returned pointer is cast to (T*).
 *
 * @param A Pointer to the Arena
 * @param T Type of the object to allocate
 * @return Pointer to a newly allocated object of type T
 *
 * Example:
 *     MyStruct* obj = arl_make(&arl, MyStruct);
 */
#define arl_make(A, T) (T*)arl_alloc(A, sizeof(T))

/**
 * @brief Allocates and returns a pointer to an array of N items of type T.
 *
 * This macro is used to allocate a contiguous block for an array of type T
 * with the correct alignment. The returned pointer is cast to (T*).
 *
 * @param A  Pointer to the Arena
 * @param T  Type of the array elements
 * @param N  Number of elements to allocate
 * @return Pointer to the start of the allocated array
 *
 * Example:
 *     int* numbers = arl_array(&arl, int, 64);
 */
#define arl_array(A, T, NB) (T*)arl_alloc(A, sizeof(T)*(size_t)NB)

/**
 * @def ARL_CHECK
 * @brief Internal safety check. Aborts the program with a message if condition fails.
 *
 * Can be disabled by defining `ARL_NO_CHECKS` before including Armel.
 *
 * Example:
 * ```c
 * ARL_CHECK(ptr != NULL, "allocation failed");
 * ```
 */
#ifndef ARL_NO_CHECKS
    #define ARL_CHECK(cond, msg)                                       \
        do {                                                           \
            if (!(cond)) {                                             \
                fprintf(stderr, "Armel error: %s (%s:%d)\n",           \
                        (msg), __FILE__, __LINE__);                    \
                abort();                                               \
            }                                                          \
        } while (0)
#else
    #define ARL_CHECK(cond, msg) ((void)0)
#endif

/**
 * @brief Always-on fatal assertion. Crashes the program if the condition is false.
 *
 * Unlike ARL_CHECK, this macro is never disabled (even with ARL_NO_CHECKS).
 * Used for critical failures (e.g. system allocation errors).
 *
 * @param cond Condition that must be true
 * @param msg  Message to print if the condition fails
 *
 * Example:
 *     ARL_ASSERT_FATAL(ptr != NULL, "VirtualAlloc failed");
 */
#define ARL_ASSERT_FATAL(cond, msg)                                          \
    ((cond) ? (void)0 :                                                      \
     (fprintf(stderr, "Fatal Armel error: %s\n", msg), abort(), (void)0))

/**
 * @def ARL_ALIGNAS(x)
 * @brief Platform-independent macro to enforce alignment of variables or structures.
 * Ensures that a variable or structure is aligned to `x` bytes.
 *
 * Example :
 * ARL_ALIGNAS(16) static uint8_t buffer[1024];
 * Armel armel;
 * arl_new_static(&armel, buffer, sizeof(buffer), 16, 0);
 */
#if defined(_MSC_VER)
    #define ARL_ALIGNAS(x) __declspec(align(x))
#elif defined(__GNUC__) || defined(__clang__)
    #define ARL_ALIGNAS(x) __attribute__((aligned(x)))
#else
    #define ARL_ALIGNAS(x) /* fallback: no alignment */
#endif
    
/**
 * @struct Armel
 * @brief A linear (bump) memory allocator.
 *
 * This structure holds all state needed to manage a linear memory region
 * allocated with mmap (or platform equivalent). Users allocate memory using
 * arl_alloc() and related macros. Memory is released in bulk via reset or free.
 *
 * Fields:
 *   - base:    Start of the allocated memory buffer (read-only)
 *   - cursor:  Current allocation pointer (moves forward)
 *   - end:     End of the buffer (used for overflow checks)
 *   - alignment: Alignment in bytes (power of 2, typically 8 or 16)
 *   - flags:   Configuration flags (ARL_ZEROS, SOFTFAIL, etc.)
 *
 * Do not modify fields manually unless you know what you're doing.
 */
typedef struct Armel Armel;

struct Armel {
    void* base;
    void* cursor;
    void* end;
	size_t alignment;
    size_t mask;
	uint8_t flags;

	Armel* prev;
	size_t grow_size;
	size_t capacity;
};

/**
 * @brief Initializes an arena using a user-provided memory buffer.
 *
 * This function allows you to create an arena without dynamic allocation (e.g., mmap).
 * You must provide a buffer and its size. This is useful for embedding arenas into
 * structures, using stack memory, or avoiding system-level allocations.
 *
 * @param armel     Pointer to the Armel struct to initialize
 * @param buffer    Pointer to the memory block to use
 * @param size      Size of the memory block (in bytes)
 * @param alignment Alignment in bytes (must be a power of 2, e.g. 8 or 16)
 * @param flags     Arena flags (e.g. ARL_ZEROS, ARL_SOFTFAIL)
 *
 * Example:
 *     uint8_t buffer[1024];
 *     Armel armel;
 *     arl_new_local(&armel, buffer, sizeof(buffer), 8, ARL_NOFLAG);
 * 
 * @note The alignment must be a power of 2 (e.g. 8, 16, 32). If not, the program will abort.
 * @note Do not attempt to use `arl_free()` on a local arena.
 *       Use `arl_reset()` if you need to reuse it.
 */
static inline void arl_new_local (Armel *armel, void *buffer, size_t size, size_t alignment, uint8_t flags) {
	ARL_ASSERT_FATAL(buffer != NULL, "arl_new_local: buffer is NULL");

	if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
		ARL_FATAL("arl_new_local: alignment must be a non-zero power of 2");
	}

	armel->base = buffer;
	armel->cursor = buffer;
	armel->end = (uint8_t*)buffer + size;
	armel->alignment = alignment;
    armel->mask = alignment - 1;
	armel->flags = flags;

	armel->prev = NULL;
	armel->grow_size = size;
	armel->capacity = 0; // not used here
}

/**
 * @brief Declares and initializes a local arena with a statically-sized aligned buffer.
 *
 * Useful for temporary, zero-cost memory arenas with no malloc, on stack or in function scope.
 *
 * Example:
 *     ARL_LOCAL(tmp, 4096);
 *     char* text = arl_array(&tmp, char, 256);
 * 
 * @note Do not call `arl_free()` on this arena.
 */
#define ARL_LOCAL(name, size) 				     		            \
    ARL_ALIGNAS(ARL_ALIGN) uint8_t name##_buffer[size]; 			\
	Armel name; 									                \
	arl_new_local(&name, name##_buffer, size, ARL_ALIGN, ARL_NOFLAG)

/**
 * @brief Rounds a size up to the next multiple of the given alignment.
 *
 * Ensures that allocations respect the required alignment.
 * The alignment must be a power of 2.
 *
 * @param size      The original size in bytes
 * @param alignment Desired alignment (e.g. 8, 16)
 * @return Aligned size (≥ size)
 *
 * Example:
 *     arl_align_up(13, 8) => 16
 */
static inline size_t arl_align_up(size_t size, size_t align) {
    size_t a = align - 1;
    return (size + a) & ~a;
}

/**
 * @brief Create a new Arena with size bytes capacity.
 * @param armel Pointer to an arena to initialise
 * @param size Capacity of the Arena in bytes
 * @param alignment The alignment to be applied, must be a power of 2
 */
void arl_new_custom (Armel* armel, size_t size, size_t alignment, uint8_t flags);

/**
 * @brief Create a new Arena with size bytes capacity.
 * @param armel Pointer to an arena to initialise
 * @param size Capacity of the Arena in bytes
 */
static inline void arl_new (Armel* armel, size_t size) {
	size_t padded_size = arl_align_up(size, ARL_ALIGN);

	ARL_ASSERT_FATAL(padded_size <= ARL_MAX_MEMORY, "arl_new: Size exceeds ARL_MAX_MEMORY");

    void* ptr = arl_sys_alloc(padded_size);

    armel->base = ptr;
    armel->cursor = ptr;
    armel->end = (char*)ptr + padded_size;
	armel->alignment = ARL_ALIGN;
	armel->mask = ARL_ALIGN - 1;
	armel->flags = ARL_NOFLAG;

	armel->prev = NULL;
	armel->grow_size = 0;
	armel->capacity = padded_size;
}

/**
 * @brief Releases the memory used by the arena.
 *
 * Frees the memory allocated by arl_new() via system-level deallocation
 * (e.g., munmap or VirtualFree). Also clears the arena's internal pointers and flags.
 *
 * Do not use the arena after calling this function unless reinitialized.
 *
 * If the arena was initialized via arl_new_static(), calling this is invalid.
 *
 * @param armel Pointer to the arena to free
 *
 * Example:
 *     arl_free(&armel);
 */
void arl_free (Armel *armel);

/**
 * @brief Releases all chained arenas to free up memory.
 * 
 * Use this only after a huge spike in memory usage that won't happen again soon.
 * 
 * @param armel Pointer to the main arena
 */
void arl_trim (Armel *armel);

/**
 * @brief Resets the arena by moving the cursor back to the beginning.
 *
 * This function effectively "frees" all memory allocated so far,
 * without releasing the underlying memory buffer.
 * Use it to reuse the arena for new allocations without any overhead.
 *
 * This is a constant-time operation.
 *
 * @param armel Pointer to the arena to reset
 *
 * Example:
 *     arl_reset(&armel);
 */
static inline void arl_reset (Armel *armel) {
	if (armel->prev == NULL) {
		armel->cursor = armel->base;
		return;
	}

	arl_trim(armel);
}

/**
 * @brief Expands the Armel arena armel and allocates size accordingly.
 * 
 * This function is called by arl_alloc if a new allocation exceeds the 
 * current capacity of the Armel arena armel and if its flag ARL_ALLOW_CHAIN is enabled.
 * The chained arena will be available in the pointer "next" (armel->next).
 * 
 * @param armel Pointer to the parent arena
 * @param size Size of the new arena
 */
void* arl_alloc_expand (Armel *armel, size_t size);


/**
 * @brief Request an allocation of size bytes in the arena armel, 
 * with the specified alignment. Flags allow the caller to 
 * customize the behavior of this allocation request (see flags above)
 * @param armel The arena where to request memory allocation
 * @param size Size in bytes of the allocation request
 * @return A pointer to the allocated memory
 */
static inline void* arl_alloc (Armel *armel, size_t size) {
	if (armel->base == NULL) {
		if (armel->flags & ARL_SOFTFAIL) return NULL;
		ARL_FATAL("Armel arena is not initialized or has been freed");
	}

	uintptr_t cursor    = (uintptr_t)armel->cursor;
	uintptr_t start     = (cursor + armel->mask) & ~armel->mask;
	uintptr_t stop      = start + size;
	const uintptr_t end = (uintptr_t)armel->end;

	if (stop > end) {
		// slow path : chained arena and expansion required
		if (armel->flags & ARL_ALLOW_CHAIN) {
			return arl_alloc_expand(armel, size);
		}

		// If softfail enabled, returns gracefully
		if (armel->flags & ARL_SOFTFAIL) {
			return NULL;
		}

		fprintf(stderr, "Armel arena error: out of memory.\n"
						"  Requested : %zu bytes\n"
						"  Remaining : %zu bytes\n"
						"  Cursor    : %p\n"
						"  End       : %p\n",
					size, (uintptr_t)armel->end - start, armel->cursor, armel->end);
		abort();
	}

	// fast path : no need to expand
	void* ptr = (void*)start;

	if (armel->flags & ARL_ZEROS) {
		memset(ptr, 0, size);
	}

	armel->cursor = (void*)stop;
	return ptr;
}

/**
 * @brief Allocates zero-initialized memory from the arena.
 *
 * This function is equivalent to `arl_alloc`, but it ensures that the allocated
 * memory is cleared (filled with zeroes). It does not require setting the
 * `ARL_ZEROS` flag, and is useful when you only want zeroed memory for some
 * allocations, without affecting the whole arena's behavior.
 *
 * @param armel Pointer to the arena.
 * @param size  Number of bytes to allocate.
 * @return A pointer to a zero-initialized block of memory, or NULL if allocation fails.
 *
 * Example:
 * ```c
 * int* arr = (int*)arl_alloc_zeroed(&arena, 4 * sizeof(int));
 * ```
 */
static inline void* arl_alloc_zeroed (Armel *armel, size_t size) {
    void* ptr = arl_alloc(armel, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/**
 * @brief Returns the current cursor position in the arena, as an offset in bytes from the base.
 *
 * Use this function to mark a point in the arena's memory, allowing you to rewind to it later
 * using arl_rewind_to(). This is useful for temporary allocations or controlled rollback.
 *
 * @param armel Pointer to the arena.
 * @return Offset (in bytes) from armel->base to armel->cursor.
 *
 * @note This is especially useful for temporary memory scopes, simulated sub-arenas,
 *       or canceling allocations without resetting the entire arena.
 */
static inline uintptr_t arl_offset (Armel *armel) {
	return (uintptr_t)armel->cursor - (uintptr_t)armel->base;
}

/**
 * @brief Rewinds the arena cursor to a previously saved offset.
 *
 * This effectively "frees" all allocations made after the given offset.
 * Typically used in conjunction with arl_offset().
 *
 * @param armel  Pointer to the arena.
 * @param offset Offset (in bytes) from armel->base.
 *
 * @note The offset must not exceed the arena's total size.
 *       If the offset is invalid, this will trigger ARL_CHECK().
 */
static inline void arl_rewind_to (Armel *armel, uintptr_t offset) {
	uintptr_t limit = (uintptr_t)armel->end - (uintptr_t)armel->base;
    ARL_CHECK(offset <= limit, "arl_rewind_to : offset out of bounds");

	armel->cursor = (uint8_t*)armel->base + offset;
}

/**
 * @brief Returns the number of bytes currently used in the arena.
 *
 * This includes any internal padding due to alignment. Useful for
 * tracking memory usage or debugging.
 *
 * @param armel Pointer to the arena
 * @return Number of bytes allocated so far
 */
static inline size_t arl_used (Armel *armel) {
	return (uintptr_t)armel->cursor - (uintptr_t)armel->base;
}

/**
 * @brief Returns the number of bytes available for allocation.
 *
 * This accounts for internal alignment and padding, and gives
 * the maximum number of bytes you can still allocate.
 *
 * @param armel Pointer to the arena
 * @return Number of bytes left
 */
static inline size_t arl_remaining (Armel *armel) {
	return (uintptr_t)armel->end - arl_align_up((uintptr_t)armel->cursor, armel->alignment);
}

/**
 * @brief Prints the internal state of the arena to stdout.
 *
 * Useful for debugging memory usage, capacity, and current offset.
 * Output includes the memory range, used/remaining bytes, alignment, and flags.
 *
 * @param armel Pointer to the arena to inspect
 */
void arl_print_info (Armel *armel);

#endif