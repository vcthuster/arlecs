#ifndef ARMEL_BENCH_H
#define ARMEL_BENCH_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static inline double diff_in_ns(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
}

#define ARL_BENCH_REPEAT 20

static inline uint64_t arl_now_ns(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)(ts.tv_sec * 1e9 + ts.tv_nsec);
}

typedef uint64_t (*arl_bench_func)(void);

void arl_bench_avg(const char *label, arl_bench_func fn) {
	uint64_t results[ARL_BENCH_REPEAT];

	// Run and store results
	for (int i = 0; i < ARL_BENCH_REPEAT; i++) {
		results[i] = fn();
	}

	// Sort to eliminate min/max outliers
	for (int i = 0; i < ARL_BENCH_REPEAT - 1; i++) {
		for (int j = i + 1; j < ARL_BENCH_REPEAT; j++) {
			if (results[i] > results[j]) {
				uint64_t tmp = results[i];
				results[i] = results[j];
				results[j] = tmp;
			}
		}
	}

	// Remove first and last (outliers)
	uint64_t total = 0;
	for (int i = 1; i < ARL_BENCH_REPEAT - 1; i++) {
		total += results[i];
	}

	double avg = total / (double)(ARL_BENCH_REPEAT - 2);
	printf("⏱ %s avg over %d runs: %.2f ns/op\n", label, ARL_BENCH_REPEAT - 2, avg);
}

#endif