#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct ListNode {
    char *key;
    char *value;
    struct ListNode *pre;
    struct ListNode *next;
    size_t size;
} Node;

/* Use list to construct the cache */
typedef struct {
    Node *head;
    Node *tail;
    size_t size;
} Cache;

void initCache(Cache *cache);
Node* isCached(Cache *cache, const char *request);
int readCache(Cache *cache, const char *request, char *buf);
void writeCache(Cache *cache, const char *key, const char *value);
void evict(Cache *cache);

