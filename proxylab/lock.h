#include "csapp.h"

extern sem_t mutex, w;
extern sem_t rw;
extern int readcnt;

void initRWLock();
void lock_reader();
void unlock_reader();
void lock_writer();
void unlock_writer();