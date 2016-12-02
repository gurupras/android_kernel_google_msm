#ifndef __HEAP__H_
#define __HEAP_H_

struct heap {
	int size;
	int count;
	int *data;
	int (*comparator)(int, int);
};

struct heap *create_min_heap(int limit);
struct heap *create_max_heap(int limit);
int heap_pop(struct heap *h);
int heap_push(struct heap *h, int value);
int heap_peek(struct heap *h);
void heapify(struct heap *h, int *values, int count);
#endif	/* __HEAP_H_ */
