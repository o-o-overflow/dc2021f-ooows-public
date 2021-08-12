#include <stddef.h>
#include <stdint.h>
#include "malloc.h"

struct block{
 size_t size;
 int free;
 struct block *next;
};

static struct block *freeList;
static size_t HEAPSZ;
static void *memory;

size_t strlen(char *s) {
  size_t i = 0;
  while (s[i]) { i++; }
  return i;
}

int strcmp(char *s1, char *s2) {
  char t = 0;
  while (*s1 && *s2) {
    char t = *s1++ - *s2++;
    if (t) {
      return t;
    }
  }

  return *s1 - *s2;
}

int strncmp(char *s1, char *s2, size_t n) {
  char t = 0;
  while (*s1 && *s2 && n--) {
    char t = *s1++ - *s2++;
    if (t) {
      return t;
    }
  }

  return n;
}

void memcpy(void *ptr, void *data, uint64_t size) {
  int i;
  for (i=0; i < size; i++) {
    ((uint8_t *)ptr)[i] = ((uint8_t *)data)[i];
  }
}

void memset(void *ptr, uint8_t val, uint64_t size) {
  int i;
  for (i=0; i < size; i++) {
    ((uint8_t *)ptr)[i] = val;
  }
}

void malloc_init(void *ptr, size_t sz) {
	memory = ptr;
	freeList = (struct block *)ptr;
	freeList->size = sz - sizeof(struct block);
	freeList->free = 1;
	freeList->next = NULL;
	HEAPSZ = sz;
}

void split(struct block *fitting_slot,size_t size){
	struct block *new=(void*)((void*)fitting_slot+size+sizeof(struct block));
	new->size=(fitting_slot->size)-size-sizeof(struct block);
	new->free=1;
	new->next=fitting_slot->next;
	fitting_slot->size=size;
	fitting_slot->free=0;
	fitting_slot->next=new;
}


void *malloc(size_t noOfBytes){
	struct block *curr;
	void *result;
	if(!(freeList->size)){
		return NULL;
	}
	curr=freeList;
	while((((curr->size)<noOfBytes)||((curr->free)==0))&&(curr->next!=NULL)){
		curr=curr->next;
	}
	if((curr->size)==noOfBytes){
		curr->free=0;
		result=(void*)(++curr);
		return result;
	}
	else if((curr->size)>(noOfBytes+sizeof(struct block))){
		split(curr,noOfBytes);
		result=(void*)(++curr);
		return result;
	}
	else{
		result=NULL;
		return result;
	}
}

void merge(){
	struct block *curr;
	curr=freeList;
	while((curr->next)!=0){
		if((curr->free) && (curr->next->free)){
			curr->size+=(curr->next->size)+sizeof(struct block);
			curr->next=curr->next->next;
		}
		curr=curr->next;
	}
}

void free(void* ptr){
	if(((void*)memory<=ptr)&&(ptr<=(void*)(memory+HEAPSZ))){
		struct block* curr=ptr;
		--curr;
		curr->free=1;
		merge();
	}
}
