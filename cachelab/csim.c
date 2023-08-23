#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

typedef struct 
{
    int valid;
    int tag;
    int stamp;
}CacheLine;

struct Cache
{
    int S;
    int E;
    int b;
    CacheLine **lines;
};


int main(int argc, char* argv[])
{
    int s = 0, E = 0, b = 0;
    int opt;
    FILE *fp = NULL;

    // get the input arguments
    while ((opt = getopt(argc, argv, "s:E:b:t:")) != -1) {
        switch (opt) {
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            fp = fopen(optarg, "r");
            if (fp == NULL)
            {
                fprintf(stderr, "Open file failed.\n");
                fclose(fp);
                exit(EXIT_FAILURE);
            }
            break;
        default: /* '?' */
            fprintf(stderr, "Wrong arguments.\n");
            fclose(fp);
            exit(EXIT_FAILURE);
        }
    }

    // initial cache
    struct Cache mycache;
    mycache.S = 1 << s;
    mycache.E = E;
    mycache.b = b;
    mycache.lines = (CacheLine**)malloc(mycache.S * sizeof(CacheLine*));
    for (int i = 0; i < mycache.S; i++)
        mycache.lines[i] = (CacheLine*)malloc(mycache.E * sizeof(CacheLine));
    for (int i = 0; i < mycache.S; i++)
        for (int j = 0; j < mycache.E; j++)
        {
            mycache.lines[i][j].valid = 0;
            mycache.lines[i][j].tag = 0;
            mycache.lines[i][j].stamp = 0;
        }
    
    // run
    int hit = 0, miss = 0, eviction = 0;
    char oper;
    unsigned long addr;
    int size;
    int setMask = (1 << s) - 1, clock = 0, flag = 0;
    while (fscanf(fp," %c %lx,%d", &oper, &addr, &size) != EOF)
    {
        clock++;
        flag = 0;
        if (oper == 'I')
            continue;
        
        int tset = (addr >> b) & setMask, ttag = addr >> (b + s);

        // if hit
        for (int i = 0; i < E; i++)
        {
            if (mycache.lines[tset][i].tag == ttag && mycache.lines[tset][i].valid == 1)
            {
                hit++;
                if (oper == 'M')
                    hit++;
                flag = 1;
                mycache.lines[tset][i].stamp = clock;
                break;
            }
        }
        if (flag == 1)
            continue;

        miss++;
        if (oper == 'M')
            hit++;
        
        // if exist an empty line
        for (int i = 0; i < E; i++)
        {
            if (mycache.lines[tset][i].valid == 0)
            {
                mycache.lines[tset][i].valid = 1;
                mycache.lines[tset][i].tag = ttag;
                mycache.lines[tset][i].stamp = clock;
                flag = 1;
                break;
            }
        }
        if (flag == 1)
            continue;
        
        // eviction using LRU policy
        eviction++;
        int last = clock, idx;
        for (int i = 0; i < E; i++)
        {
            if (mycache.lines[tset][i].stamp < last)
            {
                idx = i;
                last = mycache.lines[tset][i].stamp;
            }
        }
        mycache.lines[tset][idx].tag = ttag;
        mycache.lines[tset][idx].stamp = clock;
        mycache.lines[tset][idx].valid = 1;
    }
    
    // free
    for (int i = 0; i < mycache.S; i++)
        free(mycache.lines[i]);
    free(mycache.lines);

    printSummary(hit, miss, eviction);
    return 0;
}
