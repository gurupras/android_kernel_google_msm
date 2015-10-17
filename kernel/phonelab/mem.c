#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/printk.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif


inline void *alloc(size_t size)
{
	void *buffer = NULL;
#ifdef __KERNEL__
	buffer = kmalloc(size, GFP_KERNEL);
#else
	buffer = malloc(size);
#endif
	return buffer;
}

inline void release(void *ptr)
{
#ifdef __KERNEL__
	kfree(ptr);
#else
	free(ptr);
#endif
}

inline char *stringdup(const char *str)
{
#ifdef __KERNEL__
	return kstrdup(str, GFP_KERNEL);
#else
	return strdup(str);
#endif
}

inline void *grealloc(void *ptr, size_t size)
{
#ifdef __KERNEL__
	return krealloc(ptr, size, GFP_KERNEL);
#else
	return realloc(ptr, size);
#endif
}
