# Implementing a sequential web proxy
参考 tiny.c 实现一个 HTTP 服务器。  
使用`Open_listenfd`创建并打开监听端口，该函数将`socket, bind, listen`函数封装在一起。在`while`循环中，使用`Accept`函数等待来自客户端的连接请求，并返回一个已连接描述符。将这个描述符传入`doit`函数中，对 HTTP 请求进行处理。  
使用`rio`包来处理`socket`，使用有缓冲的输入，可以高效地读取数据。使用`Rio_readinitb`将一个`rio_t`类型的读缓冲区与描述符联系起来。使用`Rio_readlineb`从缓冲区中读取一行到 user buffer 中。HTTP 报文的第一行为 HTTP request，根据报文格式可以提取出`method, uri, version`。使用`parse_uri`对`uri`进行解析，得到`hostname, port, path`。  
使用`Open_clientfd`打开一个客户端`socket`并向服务器进行连接，该函数将`socket, connect`函数封装在一起。将`method, path, version`组成一个 HTTP 请求，使用`Rio_writen`向描述符中写入需要发送的内容。  
使用`forward_requesthdrs`函数处理 HTTP request headers 的转发。使用`Rio_readlineb`读取客户端发送的内容，使用`Rio_writen`向发送`socket`写入内容。请求头以`\r\n`为结束标志。代理服务器需要额外添加一些新的请求头，最后写入`\r\n`表示请求头内容结束。  
使用`forward_response`函数转发来自服务器的响应。逐行读取数据并写入与客户端连接的描述符中。  
完成之后，分别关闭与服务器、客户端相连的描述符。

# Dealing with multiple concurrent requests
在`Accept`之后，使用`Pthread_create`创建一个新的进程用于处理一个新的连接。`Accept`返回的描述符`fd`作为一个参数传入 thread routine 中。在 thread routine 中调用`doit`函数来处理 HTTP 请求。  
需要注意的是，由于线程之间的运行顺序是不确定的，所以可能存在之前得到的描述符`fd`还未传入`doit`就被下一次`Accept`的返回值覆盖。所以采用动态内存开辟的方法，将每次`Accept`的返回值存放到不同的地址中，这样它们之间就不会相互覆盖了。  
在 thread routine 中，需要调用`Pthread_detach`使线程结束后自动回收资源，还需要将主线程中为`fd`开辟的空间释放掉。

## Prethreading 预线程化模型
采用生产者消费者模型，需要一个共享buffer，生产者（主线程）将`Accept`返回的`fd`存入共享buffer中，消费者（子线程）不断地从共享buffer中读取`fd`并处理。  
共享buffer设计为一个循环数组，用两个指针分别指向头和尾。对共享buffer的所有操作都需要加锁，共需要三个信号量：`mutex`互斥锁用于操作buffer；`slots`用于判断是否有空的slot，在`insert`中对`slots`执行P操作，在`remove`中执行V操作；`items`用于判断buffer中是否有元素，在`remove`中执行P操作，在`insert`中执行V操作。

# Caching web objects
使用双向链表构建cache，cache包含一个头节点和一个尾节点作为哨兵，其余节点包含一个key，用于存放HTTP请求，一个value，用于存放HTTP响应，一个size，用于记录这个缓存节点的大小。  
采用LRU cache，越靠近头的节点表示最近被访问过。cache有大小的限制，当cache满了之后，要进行evict。从最后一个节点开始向前依次进行evict，直到cache中剩余的空间大于需要插入的节点大小，最后就将新的节点插入到cache的头部。  
采用读写者模型对cache进行访问。使用三个信号量：`mutex`互斥锁，锁住`readcnt`；`w`互斥锁，锁住写操作，只用`readcnt`为0时，才允许写操作；`rw`互斥锁，实现公平的读写操作，当有reader或是writer出现时，先获得`rw`锁，再去获得其他锁，在对其他锁加锁完成后，立即释放`rw`锁。有了`rw`，后到来的reader会被先到来的writer阻塞，这样也避免了写饥饿。此时，读写者优先级相同，是一个公平的读写者模型。  
在转发HTTP响应时，无法事先知道响应报文大小，需要一行行地进行读取，设置一个计数器记录当前读取了多少内容，当计数器值小于cache允许的最大 object size 时，将读到的内容写入一个buffer中，使用`strncat`进行拼接，之后，将buffer中内容写入cache，当计数器值超过允许的最大大小时，意味着这个响应不进行缓存，就无需再写入buffer。