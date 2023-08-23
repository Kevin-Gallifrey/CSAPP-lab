# Writing Your Own Unix Shell
`eval`函数用于解析并执行命令。输入参数是`char*`，指向`main`函数中的输入缓冲区。首先利用于一个字符串缓冲区保存输入指令，之后所有操作都使用该缓冲区，以免影响`main`中变量。利用`parseline`函数解析命令行，得到`argv`参数列表。如果`argv[0]`为空指针，表示命令行为空，直接返回。  
之后利用`builtin_cmd`函数判断是否为内置命令，若是，由`builtin_cmd`直接运行并返回，若不是，调用`fork`在子进程中运行。为了避免竞争的出现（子进程退出调用`deletejob`在父进程调用`addjob`之前），parent 在`fork`之前就要block `SIGCHLD`信号，并在`addjob`之后unblock该信号。由`sigprocmask`函数实现对信号的block和unblock。如果是 foreground job，parent 需要调用`waitfg`函数等待 child 结束运行。  
在 child 中，调用`execve`执行命令。由于 parent 之前 block `SIGCHLD`信号，所以 child 要在调用`execve`之前unblock这个信号。child还需要在调用`execve`之前使用`setpgid(0, 0)`设置一个不同的process group ID，这样中断信号和停止信号就不会影响到shell本身。

`builtin_cmd`函数判断命令是否为内置命令，根据`argv[0]`的值调用相应的函数，如果不是内置命令返回0，否则执行完相应的命令后返回1。内置命令由shell本身执行，不创建新的进程。

`do_bgfg`函数用于执行`fg`和`bg`内置命令。首先要判断参数是否合法，并得到相应的`pid`或是`jid`值，随后找到相应的`job`。重启job意味着设置job的状态为`FG`或是`BG`，并通过`kill`发送`SIGCONT`信号。每个job对应一个独立的进程组，信号发送给job就是发送给整个进程组，所以`kill`中`pid`参数应为负值。如果job的状态为`FG`，则需要调用`waitfg`等待进程结束。

`waitfg`函数等待 foreground job 运行结束。由于每个child job结束时都会触发`SIGCHLD`信号，`waitfg`只需要等待这个信号，并判断 foreground job 是否还在运行。`fgpid`函数返回 foreground job 的`pid`，如果没有 foreground job，则返回0。用`fgpid`作为循环判断的条件，用`sigsuspend`函数让进程休眠，同时避免出现竞争。在循环前block `SIGCHLD`信号，在`sigsuspend`函数中unblock `SIGCHLD`信号并进入`pause`，这样保证进程能够收到`SIGCHLD`信号，并从`pause`中返回。

`sigchld_handler`处理`SIGCHLD`信号。循环调用`waitpid`回收zombie子进程。利用`status`参数判断child状态。对于正常退出的child，调用`deletejob`删除job信息，对于被信号终止的child，打印信息后再调用`deletejob`。对于暂停的child，打印信息，并将其`job->state`改为`ST`。`sigchld_handler`不等待child终止，且需要处理stopped child，所以在`waitpid`中加入参数`WNOHANG`和`WUNTRACED`。退出循环有两种情况：一是`waitpid`返回-1，此时需要检查错误信息是否为`ECHILD`；二是`waitpid`返回0（`WNOHANG`模式下的立即返回），这是正常的返回，需要和前一种情况区分开来，此时不用检查`errno`。  

handler中使用到了`errno`，为了保证程序的安全，需要在入口处保存原来`errno`的值，并在返回前恢复。同时，在涉及到对全局变量的修改时，如`deletejob`，`addjob`以及对其他`jobs`中数据的操作，需要block所有信号。

`sigint_handler`处理`SIGINT`信号。首先调用`fgpid`获得 foreground job 的`pid`。如果没有foreground job就直接返回，否则调用`kill`函数，对该进程组中的所有进程发送`SIGINT`信号。`kill`中`pid`参数为负值。

`sigtstp_handler`处理`SIGTSTP`信号。与`sigint_handler`类似，只不过`kill`中发送的是`SIGTSTP`信号。

为了使程序安全运行，所有的系统函数的返回值都需要检查，如果返回报错，需要进行处理。