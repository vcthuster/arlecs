#ifndef ARMEL_SYS_H
#define ARMEL_SYS_H

#include <stddef.h>
#include <string.h>

#ifdef _WIN32
	#include <windows.h>

	/**
	 * @brief Prints a fatal error message to stderr and aborts the program.
	 *
	 * This macro writes the specified message followed by a newline to standard error,
	 * then terminates the program using abort().
	 *
	 * @param msg The error message to display.
	 */
	#define ARL_FATAL(msg) 															\
		do {																		\
			DWORD ignored;															\
			WriteFile(GetStdHandle(STD_ERROR_HANDLE),								\
				msg,																\
				(DWORD)strlen(msg),													\
				&ignored,															\
				NULL);																\
			WriteFile(GetStdHandle(STD_ERROR_HANDLE), "\n", 1, &ignored, NULL);	\
			abort();																\
		} while(0);
#else
	#include <unistd.h>

	/**
	 * @brief Prints a fatal error message to stderr and aborts the program.
	 *
	 * This macro writes the specified message followed by a newline to standard error,
	 * then terminates the program using abort().
	 *
	 * @param msg The error message to display.
	 */
	#define ARL_FATAL(msg) 															\
		do {																		\
			write(STDERR_FILENO, msg, strlen(msg));									\
			write(STDERR_FILENO, "\n", 1);											\
			abort();																\
		} while (0);
#endif

/**
 * @brief Allocates a memory region of the given size using system-specific calls.
 *
 * On UNIX: uses mmap. 
 * On Windows: uses VirtualAlloc.
 * This function is intended for internal use by arl_new().
 *
 * @param size Number of bytes to allocate (must already be aligned)
 * @return Pointer to the allocated memory (never NULL or MAP_FAILED — use ARL_ASSERT_FATAL)
 */
void* arl_sys_alloc(size_t size);

/**
 * @brief Frees a memory region allocated by arl_sys_alloc.
 *
 * On UNIX: uses munmap. 
 * On Windows: uses VirtualFree.
 * This function is intended for internal use by arl_free().
 *
 * @param ptr  Pointer to the memory block to free
 * @param size Original size of the memory block
 */
void  arl_sys_free(void* ptr, size_t size);

#endif // ARMEL_SYS_H