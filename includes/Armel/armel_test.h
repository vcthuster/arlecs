#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>

#ifdef _WIN32
    #define SKIP_EXPECT_ABORT 1
#else
    #define SKIP_EXPECT_ABORT 0
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#include <Armel/armel.h>

typedef void (*fn) (void);

#define ARMEL_TEST(name) void name(void)

#define RUN_TEST(test) 									\
	do { 												\
		printf("\n⚡️ Running %-30s ... ", #test); 		\
		test(); 										\
		printf("PASSED ✅\n"); 							\
	} while (0)


/**
 * @brief Executes a function and checks if it triggers a SIGABRT.
 *
 * This helper forks the current process, runs the given function in the child,
 * and verifies that it causes an abort (SIGABRT). If not, it prints an error message.
 *
 * @param fn    Function pointer expected to abort.
 * @param label Description label for the test (used in error messages).
 * @return 0 if SIGABRT was correctly raised, 1 otherwise.
 */
static inline int expect_abort(void (*fn)(void), const char* label) {
#if SKIP_EXPECT_ABORT
    printf("\n\t⚠️  Skipping %s (expect_abort not supported on Windows for now)\n", label);
    return 0;
#else
	pid_t pid = fork();

	if (pid == -1) {
		perror("fork failed");
		exit(1);
	}

	if (pid == 0) { // Child : execute fn supposed to crash
		fn();
		_exit(0);
	}

	int status;
	waitpid(pid, &status, 0);

	if (WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT) {
		return 0;
	} else {
		fprintf(stderr, "\n\t❌ %s: expected abort (SIGABRT), but got exit code %d\n", label, status);
		return 1;
	}
#endif
}