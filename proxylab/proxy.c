#include <stdio.h>
#include "csapp.h"
#include "cache.h"
#include "sbuf.h"

#define NTHREADS 4
#define SBUFSIZE 16
#define PRETHREAD

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void* thread(void *vargp);
void doit(int fd);
int parse_uri(char *uri, char *hostname, char *port, char *path);
void forward_requesthdrs(rio_t *rp, int fd, char *hostname);
size_t forward_response(rio_t *rp, int fd, char *cbuf);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/* global variables*/
Cache proxyCache;
sbuf_t sbuf;  /* Shared buffer of connected descriptors */

int main(int argc, char **argv)
{
    int listenfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* Initialize cache */
    initCache(&proxyCache);

    #ifdef PRETHREAD
    /* Create worker threads */
    sbuf_init(&sbuf, SBUFSIZE);
    for (int i = 0; i < NTHREADS; i++)
    {
        Pthread_create(&tid, NULL, thread, NULL);
    }
    #endif

    listenfd = Open_listenfd(argv[1]);

    #ifdef PRETHREAD
    int connfd;
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        sbuf_insert(&sbuf, connfd);  /* Insert connfd in buffer */
    }
    
    #else
    int *connfdp;
    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        Pthread_create(&tid, NULL, thread, connfdp);
    }
    #endif

    return 0;
}

/* Thread routine */
#ifdef PRETHREAD
void* thread(void *vargp)
{
    Pthread_detach(pthread_self());
    while (1)
    {
        int connfd = sbuf_remove(&sbuf);  /* Remove connfd from buffer */
        doit(connfd);
        Close(connfd);
    }
    return NULL;
}
#else
void* thread(void *vargp)
{
    int fd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(fd);
    Close(fd);
    return NULL;
}
#endif

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    char sbuf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t rio_server;
    char cacheBuf[MAX_OBJECT_SIZE];

    /* Read request line */
    Rio_readinitb(&rio_server, fd);
    if (!Rio_readlineb(&rio_server, sbuf, MAXLINE)) {  //line:netp:doit:readrequest
        return;
    }
    printf("%s", sbuf);

    /* Check whether the request is cached and read if cached */
    if (readCache(&proxyCache, sbuf, cacheBuf) >= 0)
    {
        printf("Cache hit. Read from cache.\n");
        Rio_writen(fd, cacheBuf, strlen(cacheBuf));
        return;
    }

    sscanf(sbuf, "%s %s %s", method, uri, version);      //line:netp:doit:parserequest
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not implement this method");
        return;
    }                                                    //line:netp:doit:endrequesterr

    /* Parse uri and get hostname */
    char hostname[MAXLINE], port[MAXLINE], path[MAXLINE];
    if (parse_uri(uri, hostname, port, path) < 0) {
        clienterror(fd, uri, "501", "Not Implemented",
                    "Proxy does not implement this uri");
        return;
    }

    /* Forward request line */
    int clientfd;
    char cbuf[MAXLINE];
    rio_t rio_client;
    clientfd = Open_clientfd(hostname, port);
    Rio_readinitb(&rio_client, clientfd);
    sprintf(cbuf, "%s %s %s", method, path, "HTTP/1.0\r\n");
    Rio_writen(clientfd, cbuf, strlen(cbuf));

    /* Read and forward requst headers */
    forward_requesthdrs(&rio_server, clientfd, hostname);

    /* Read and forward response */
    if (forward_response(&rio_client, fd, cacheBuf) <= MAX_OBJECT_SIZE - strlen(sbuf))
        writeCache(&proxyCache, sbuf, cacheBuf);
    
    Close(clientfd);
}
/* $end doit */

/*
 * parse_uri - parse URI into hostname and path
 *             return 0 if success, -1 if fail (not http)
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *hostname, char *port, char *path) 
{
    char* ptr, *q;
    char buf[MAXLINE];
    if ((ptr = strstr(uri, "http://")) == NULL)
        return -1;

    ptr += 7;
    int i = 0;
    while (*ptr != '/') {
        buf[i] = *ptr;
        i++;
        ptr++;
    }
    buf[i] = '\0';

    if (q = strchr(buf, ':')) {
        // 不能直接使用 %s:%s 格式化，':'会被当作字符串的一部分，需要使用空白字符分隔字符串
        *q = ' ';
        sscanf(buf, "%s %s", hostname, port);
    }
    else {
        strcpy(hostname, buf);
        strcpy(port, "80");
    }
        
    strcpy(path, ptr);
	return 0;
}
/* $end parse_uri */

/*
 * forward_requesthdrs - read and forward HTTP request headers
 */
/* $begin forward_requesthdrs */
void forward_requesthdrs(rio_t *rp, int fd, char *hostname) 
{
    char buf[MAXLINE], header[MAXLINE], temp[MAXLINE];
    int hasHost = 0;

    Rio_readlineb(rp, buf, MAXLINE);
    while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
        sscanf(buf, "%s:%s", header, temp);
        if (!strcasecmp(header, "Host"))
            hasHost = 1;
        Rio_writen(fd, buf, strlen(buf));
        Rio_readlineb(rp, buf, MAXLINE);
    }
    
    /* Add headers */
    if (!hasHost) {
        sprintf(buf, "Host: %s\r\n", hostname);
        Rio_writen(fd, buf, strlen(buf));
    }
    sprintf(buf, user_agent_hdr);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Connection: close\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Proxy-Connection: close\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "\r\n");
    Rio_writen(fd, buf, strlen(buf));
    return;
}
/* $end forward_requesthdrs */

/*
 * forward_response - read and forward HTTP response, write response to cacheBuf
 */
/* $begin forward_response */
size_t forward_response(rio_t *rp, int fd, char *cbuf) 
{
    char buf[MAXLINE];
    size_t n, count = 0;

    // 计算 buf 中内容大小时，可以直接使用 n
    while ((n = Rio_readlineb(rp, buf, MAXLINE)) > 0) {
        count += n;
        if (count <= MAX_OBJECT_SIZE)
            strncat(cbuf, buf, n);
        Rio_writen(fd, buf, n);
    }

    return count;
}
/* $end forward_response */

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Proxy Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Proxy Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */
