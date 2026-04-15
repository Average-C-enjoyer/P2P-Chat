#include "spsc_queue.h"

int main() {
	int *q = NULL;
	// Test pushing and popping
	int a = 1, b = 2, c = 3, d = 4;
	q_push(q, a);
	q_push(q, b);
	q_push(q, c);
	q_push(q, d);
	if (q_pop_val(q) != a) {
		printf("Failed to pop from queue\n");
		return 1;
	}
	if (q_pop_val(q) != b) {
		printf("Failed to pop from queue\n");
		return 1;
	}
	if (q_pop_val(q) != c) {
		printf("Failed to pop from queue\n");
		return 1;
	}
	if (q_pop_val(q) != d) {
		printf("Failed to pop from queue\n");
		return 1;
	}
	if (q_pop_val(q) != 0) {
		printf("Expected queue to be empty\n");
		return 1;
	}

	printf("All tests passed!\n");
	return 0;
}