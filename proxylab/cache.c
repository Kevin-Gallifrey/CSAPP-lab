#include "cache.h"
#include "lock.h"

void initCache(Cache *cache)
{
    cache->head = malloc(sizeof(Node));
    cache->tail = malloc(sizeof(Node));
    cache->head->next = cache->tail;
    cache->head->pre = NULL;
    cache->head->key = NULL;
    cache->head->value = NULL;
    cache->head->size = 0;
    cache->tail->pre = cache->head;
    cache->tail->next = NULL;
    cache->tail->key = NULL;
    cache->tail->value = NULL;
    cache->tail->size = 0;
    cache->size = 0;

    /* Initialize semaphores */
    initRWLock();
}

Node* isCached(Cache *cache, const char *request)
{
    Node *p = cache->head->next;
    while (p != cache->tail)
    {
        if (strcmp(p->key, request) == 0)
            return p;
        p = p->next;
    }
        
    return NULL;
}

int readCache(Cache *cache, const char *request, char *buf)
{
    lock_reader();
    Node *node;
    if ((node = isCached(cache, request)) != NULL)
    {
        strcpy(buf, node->value);
        unlock_reader();

        /* Update LRU cache */
        lock_writer();
        node->pre->next = node->next;
        node->next->pre = node->pre;
        node->next = cache->head->next;
        node->pre = cache->head;
        cache->head->next->pre = node;
        cache->head->next = node;
        unlock_writer();
        return 0;
    }
    else
    {
        unlock_reader();
        return -1;
    }
}

void writeCache(Cache *cache, const char *key, const char *value)
{
    Node *node = malloc(sizeof(Node));
    node->key = malloc(strlen(key));
    strcpy(node->key, key);
    node->value = malloc(strlen(value));
    strcpy(node->value, value);
    node->size = strlen(key) + strlen(value);

    /* Adopt LRU policy */
    lock_writer();
    while (cache->size + node->size > MAX_CACHE_SIZE)
        evict(cache);

    node->next = cache->head->next;
    node->pre = cache->head;
    cache->head->next->pre = node;
    cache->head->next = node;
    cache->size += node->size;
    unlock_writer();
}

void evict(Cache *cache)
{
    Node *last = cache->tail->pre;
    last->pre->next = cache->tail;
    cache->tail->pre = last->pre;
    cache->size -= last->size;
    free(last);
}