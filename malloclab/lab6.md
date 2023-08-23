# Malloc
## Implicit free list
隐式空闲列表方案，每个 block 有一个 header 和一个 footer，其中记录了整个block的大小（包括header和footer）以及这个block是否被分配。利用block的size,可以遍历所有的block,从而查找可用的空闲的block。free时需要检查前后相邻的block是否空闲，若是，则进行合并。  
remalloc中，涉及到对空闲block的split，需要根据大小判断是否需要split操作，否则可能会出现大小为0的block，这会造成错误。因为 epilogue block 是用size为0进行标记的，它标记了heap的末尾。  
初始化时创建序言块（prologue block）和结尾块（epilogue block），用于消除合并时的边界条件判断。  
隐式空闲列表中分配的时间是和 heap 中总的 block 数成正比的。

## Explicit free list
显式空闲列表方案，显式空闲列表由一个双向列表构成，每个 block 有一个 header 和一个 footer，每个 free block 在 payload 中有一个 predecessor 指针和一个 successor 指针。由于需要存放两个指针，所以每个block的大小至少为 2 * DSIZE。  
在分配时，需要将分配的块从空闲链表中取出。在释放时，需要将空闲块插入链表，可以选择插入到链表最前面，这样 free 可以在常数时间内完成。  
显式空闲列表中分配的时间是和 heap 中总的 free block 数成正比的，因为查找空闲块时，只需遍历 freelist 即可。  

由于编译优化，会改变代码执行顺序，所以仅用 printf 来调试可能无法明确判断出程序进行到哪里。

## Segregated free list
分离空闲列表方案，根据 block 的大小划分多个 size class，每个 size class 维护一个显式空闲链表。分配和释放的操作和显式空闲链表方案相同，只是在从空闲链表中取出和插入块时，需要根据块的大小判断在哪一个 class 中，操作对应的空闲链表。  
在使用 first-fit 进行查找时，也是先确定对应的 class，若其中无法找到可以分配的块，则向 size 更大的 class 中寻找。  
class 一般可以按2的幂次方来进行划分，`(2^i, 2^(i+1)]`，对于小的块可以单独划分一个class，这样可以提高空间利用率。在查找对应的class时，可以采用二分查找的方法加速查找。