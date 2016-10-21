#ifndef __KERNEL__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#else
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#endif  /* __KERNEL__ */

#ifdef __KERNEL__
#define printf(...) printk(KERN_DEBUG __VA_ARGS__);
#define PRId64 "lld"
#endif  /* __KERNEL__ */

#ifdef __KERNEL__
#define printf(...) printk(KERN_DEBUG __VA_ARGS__);
#endif

#ifdef __KERNEL__
#include <linux/heap.h>
#else
#include "heap.h"
#endif  /* __KERNEL__ */

static void *alloc(size_t size)
{
#ifdef __KERNEL__
	return kmalloc(size, GFP_KERNEL);
#else
	return malloc(size);
#endif  /* __KERNEL__ */
}

static void *zalloc(size_t size)
{
	void *ptr = alloc(size);
	memset(ptr, 0, size);
	return ptr;
}

static void release(void *ptr)
{
#ifdef __KERNEL__
	kfree(ptr);
#else
	free(ptr);
#endif  /* __KERNEL__ */
}



static inline int min_heap_comparator(int a, int b)
{
	return ((a) <= (b));
}

static inline int max_heap_comparator(int a, int b)
{
	return ((a) >= (b));
}

static inline int get_parent_idx(int idx)
{
	return (idx - 1) / 2;
}

static inline void heap_swap(struct heap *h, int idx1, int idx2)
{
	int tmp;
	tmp = h->data[idx2];
	h->data[idx2] = h->data[idx1];
	h->data[idx1] = tmp;
}

static void bubble_up(struct heap *h, int child_idx)
{
	int parent_idx = -1;
	while(child_idx > 0) {
		parent_idx = get_parent_idx(child_idx);
		if(h->comparator(h->data[child_idx], h->data[parent_idx])) {
			heap_swap(h, child_idx, parent_idx);
			child_idx = parent_idx;

		} else {
			break;
		}
	}
}

#define LCHILD(parent)	(((parent) * 2) + 1)
#define RCHILD(parent)	(((parent) * 2) + 2)

static void bubble_down(struct heap *h)
{
	int parent_idx = 0;
	int lchild_idx = LCHILD(parent_idx);
	int rchild_idx;
	int swap_child_idx;

	while(lchild_idx <= h->count) {
		lchild_idx = LCHILD(parent_idx);
		rchild_idx = RCHILD(parent_idx);
		swap_child_idx = lchild_idx;
		if(rchild_idx <= h->count) {
			if(h->comparator(h->data[rchild_idx], h->data[lchild_idx])) {
				swap_child_idx = rchild_idx;
			}
		}
		if(h->comparator(h->data[swap_child_idx], h->data[parent_idx])) {
			// Do the heap_swap
			heap_swap(h, swap_child_idx, parent_idx);
			parent_idx = swap_child_idx;
		} else {
			break;
		}
	}
}

int heap_pop(struct heap *h)
{
	int val;
	if(h->count < 0) {
		printf("Cannot pop from empty heap\n");
		val = -1;
		goto out;
	}
	val = h->data[0];
	// Now do the actual pop
	h->data[0] = h->data[h->count];
	h->count--;
	bubble_down(h);
out:
	return val;
}

int heap_push(struct heap *h, int value)
{
	h->count++;

	if(h->count >= h->size)
	{
		printf("Heap size exceeded\n");
		h->count--;
		return -1;
	}
	// Insert in next slot
	h->data[h->count] = value;
	bubble_up(h, h->count);
	return 0;
}

int heap_peek(struct heap *h)
{
	if(h->count >= 0)
		return h->data[0];
	else
		return -1;
}

static inline struct heap *create_heap(int limit)
{
	struct heap *h = alloc(sizeof(struct heap));
	if(h == NULL) {
		goto out;
	}
	h->size = limit;
	h->count = -1;
	h->data = zalloc(sizeof(int) * limit);
	if(h->data == NULL) {
		release(h);
		h = NULL;
		goto out;
	}
out:
	return h;
}

inline void heapify(struct heap *h, int *values, int count)
{
	int i;
	for(i = 0; i < count; i++) {
		heap_push(h,values[i]);
	}
}

struct heap *create_min_heap(int limit)
{
	struct heap *h = create_heap(limit);
	h->comparator = min_heap_comparator;
	return h;
}

struct heap *create_max_heap(int limit)
{
	struct heap *h = create_heap(limit);
	h->comparator = max_heap_comparator;
	return h;
}
