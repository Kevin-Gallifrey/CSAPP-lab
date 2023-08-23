#include "lock.h"

sem_t mutex, w;
sem_t rw;  // 实现公平读写，FIFO
int readcnt;

void initRWLock()
{
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);
    Sem_init(&rw, 0, 1);
    readcnt = 0;
}

void lock_reader()
{
    P(&rw);
    P(&mutex);
    readcnt++;
    if (readcnt == 1)  // 第一个读开始，锁住写
        P(&w);
    V(&mutex);
    V(&rw);
}

void unlock_reader()
{
    P(&mutex);
    readcnt--;
    if (readcnt == 0)  // 读结束，可以写
        V(&w);
    V(&mutex);
}

void lock_writer()
{
    P(&rw);
    P(&w);
    V(&rw);
}

void unlock_writer()
{
    V(&w);
}