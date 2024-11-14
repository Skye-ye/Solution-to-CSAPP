#include "cache.h"

static void move_front(cache_t *cache, object_t *obj);

void cache_init(cache_t *cache, size_t max_size) {
  cache->size = 0;
  cache->count = 0;
  cache->max_size = max_size;
  cache->head = NULL;
  cache->tail = NULL;
  pthread_rwlock_init(&cache->rwlock, NULL);
}

object_t *cache_access(cache_t *cache, char *url) {
  pthread_rwlock_rdlock(&cache->rwlock);

  object_t *current = cache->head;
  while (current) {
    if (strcmp(current->url, url) == 0) {
      pthread_rwlock_unlock(&cache->rwlock);
      pthread_rwlock_wrlock(&cache->rwlock);

      if (strcmp(current->url, url) == 0) {
        move_front(cache, current);
        pthread_rwlock_unlock(&cache->rwlock);
        return current;
      }

      pthread_rwlock_unlock(&cache->rwlock);
      return NULL;
    }
    current = current->next;
  }

  pthread_rwlock_unlock(&cache->rwlock);
  return NULL;
}

void cache_insert(cache_t *cache, object_t *obj) {
  pthread_rwlock_wrlock(&cache->rwlock);

  // If cache is full, evict objects until there's space
  while (cache->size + obj->size > cache->max_size && cache->tail) {
    object_t *victim = cache->tail;

    // Remove from tail
    cache->tail = victim->prev;
    if (cache->tail)
      cache->tail->next = NULL;
    else
      cache->head = NULL;

    cache->size -= victim->size;
    cache->count--;

    // Free evicted object
    Free(victim->url);
    Free(victim->data);
    Free(victim);
  }

  // Insert new object at head
  obj->next = cache->head;
  obj->prev = NULL;
  if (cache->head)
    cache->head->prev = obj;
  cache->head = obj;
  if (!cache->tail)
    cache->tail = obj;

  cache->count++;
  cache->size += obj->size;

  pthread_rwlock_unlock(&cache->rwlock);
}

void cache_deinit(cache_t *cache) {
  pthread_rwlock_wrlock(&cache->rwlock);

  object_t *current = cache->head;
  while (current) {
    object_t *next = current->next;
    Free(current->url);
    Free(current->data);
    Free(current);
    current = next;
  }
  cache->head = cache->tail = NULL;
  cache->size = 0;
  cache->count = 0;

  pthread_rwlock_unlock(&cache->rwlock);
  pthread_rwlock_destroy(&cache->rwlock);
}

static void move_front(cache_t *cache, object_t *obj) {
  if (!obj || obj == cache->head)
    return;

  // Remove from current position
  if (obj->prev)
    obj->prev->next = obj->next;
  if (obj->next)
    obj->next->prev = obj->prev;
  if (obj == cache->tail)
    cache->tail = obj->prev;

  // Move to front
  obj->next = cache->head;
  obj->prev = NULL;
  if (cache->head)
    cache->head->prev = obj;
  cache->head = obj;
  if (!cache->tail)
    cache->tail = obj;
}