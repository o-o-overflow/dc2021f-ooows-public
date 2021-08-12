#ifndef __MALLOOOC
#define __MALLOOOC
#include<stddef.h>

size_t strlen(char *);
int strcmp(char *, char *);
void memcpy(void *, void *, uint64_t);
void memset(void *, uint8_t, uint64_t);

void malloc_init(void *ptr, size_t size);
void *malloc(size_t noOfBytes);
void merge();
void free(void* ptr);
#endif
