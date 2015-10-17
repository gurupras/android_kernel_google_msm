#ifndef __MEM_H_
#define __MEM_H_

void *alloc(size_t size);
void release(void *ptr);
char *stringdup(const char *str);
void *grealloc(void *ptr, size_t size);


#endif	/* __MEM_H_ */
