#ifndef CACHE_H
#define CACHE_H

#include "csapp.h"

typedef struct {
  size_t size;
  char *data;
  int cacheable;
} buffer_t;

typedef struct object_node {
  size_t size;
  char *url;
  char *data;
  struct object_node *next;
  struct object_node *prev;
} object_t;

typedef struct {
  size_t size;
  size_t count;
  size_t max_size;
  object_t *head;
  object_t *tail;
  pthread_rwlock_t rwlock;
} cache_t;

void cache_init(cache_t *cache, size_t max_size);
object_t *cache_access(cache_t *cache, char *url);
void cache_insert(cache_t *cache, object_t *object);
void cache_deinit(cache_t *cache);

#endif /* CACHE_H */