# sleep
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(2, "Usage: sleep <ticks>\n"); // 参数数量错误,报错
    }
    int n = 0; // 休眠时间
    n = atoi(argv[1]);
    sleep(n);
    exit(0);
}

```

从运行结果来看,sleep 10大约停顿了2秒,即qemu模拟的一个时钟周期大约是0.2秒。

# pingpong
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        fprintf(2, "Usage: pingpong\n"); // 参数错误,报错
    }
    int pipe1[2]; // 父写子读
    int pipe2[2]; // 子写父读
    char byte = 'a';
    char buffer;

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1)
    { // 创建管道
        fprintf(2, "create pipe error\n");
        exit(0);
    }

    int pid = fork();
    if (pid < 0)
    { // 创建子进程出错
        fprintf(2, "fork error\n");
    }
    else if (pid == 0)
    {
        // 子进程
        pid = getpid();
        close(pipe1[1]);
        close(pipe2[0]);
        if (read(pipe1[0], &buffer, 1) != 1)
        {
            fprintf(2, "child process : read pipe error\n");
        }
        fprintf(1, "%d: received ping\n", pid);
        if (write(pipe2[1], &byte, 1) != 1)
        {
            fprintf(2, "child process : write pipe error\n");
        }
        exit(0);
    }
    else
    {
        // 父进程
        pid = getpid();
        close(pipe1[0]);
        close(pipe2[1]);
        if (write(pipe1[1], &byte, 1) != 1)
        {
            fprintf(2, "parent process : write pipe error\n");
        }
        if (read(pipe2[0], &buffer, 1) != 1)
        {
            fprintf(2, "parent process : read pipe error\n");
        }
        fprintf(1, "%d: received pong\n", pid);
        exit(0);
    }

    exit(0);
}

```

思考:为什么要close父进程和子进程中的pipe端口,一方面是符合pipe单向通信的原则,更重要的是,我们把pipe看作一个特殊的文件,关闭写端口相当于给文件末尾提供了结束符号。

# primes
质数筛

难点是及时关闭端口回收管道。
```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NUM_START 2
#define NUM_END 280

void prime(int p[]) __attribute__((noreturn));

void prime(int p[])
{
    int n;
    close(p[1]); // 关闭写管道
    if (read(p[0], &n, 4) != 4)
    {
        close(p[0]);
        exit(0);
    }
    fprintf(1, "prime %d\n", n);

    int buf;
    int newp[2];
    if (pipe(newp) == -1)
    {
        int pid = getpid();
        fprintf(2, "process %d: create pipe error\n", pid);
        exit(-1);
    }

    int pid = fork();
    if (pid < 0)
    {
        pid = getpid();
        fprintf(2, "process %d: fork error\n", pid);
        exit(-1);
    }
    else if (pid == 0)
    {
        close(p[0]);
        prime(newp);
    }
    else
    {
        close(newp[0]); // 关闭读管道
        while (read(p[0], &buf, 4))
        {
            if (buf % n)
            {
                if (write(newp[1], &buf, 4) != 4)
                {
                    pid = getpid();
                    fprintf(2, "process %d: write pipe %d error\n", pid, newp[1]);
                    exit(-1);
                }
            }
        }
        close(p[0]);
        close(newp[1]); // 写完成
        wait(0);
        exit(0);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        fprintf(2, "Usage: primes\n");
        exit(-1);
    }
    int p[2];
    if (pipe(p) == -1)
    {
        int pid = getpid();
        fprintf(2, "process %d: create pipe error\n", pid);
        exit(-1);
    }
    int pid = fork();
    if (pid < 0)
    {
        pid = getpid();
        fprintf(2, "process %d: fork error\n", pid);
        exit(-1);
    }
    else if (pid == 0)
    {
        // 子进程
        prime(p);
    }
    else
    {
        // 父进程
        close(p[0]);
        for (int buf = NUM_START; buf <= NUM_END; buf++)
        {
            if (write(p[1], &buf, 4) != 4)
            {
                pid = getpid();
                fprintf(2, "process %d: write pipe %d error\n", pid, p[1]);
                exit(-1);
            }
        }
        close(p[1]); // 写完成
        while (wait(0) > 0)
            ;
        exit(0);
    }
}
```

# find

采用深度优先搜索策略

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// 查找文件
void find(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, O_RDONLY)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        fprintf(2, "Usage: find dir file\n");
        exit(0);
    case T_DIR:
        // 检查路径长度是否合适
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
        {
            fprintf(2, "find: path too long\n");
            break;
        }

        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';

        // 读取目录项
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue;

            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;

            if (stat(buf, &st) < 0)
            {
                fprintf(2, "find: cannot stat %s\n", buf);
                continue;
            }
            if (st.type == T_FILE)
            {
                if (strcmp(de.name, filename) == 0)
                {
                    printf("%s\n", buf);
                }
            }
            else if (st.type == T_DIR)
            {
                find(buf, filename);
            }
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        fprintf(2, "Usage: find <directory> <filename>\n");
        exit(0);
    }
    find(argv[1], argv[2]);
    exit(0);
}
```

# xargs

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int readline(char *new_argv[32], int curr_argc)
{
    static char buf[1024];
    int n = 0;
    while (read(0, buf + n, 1))
    {
        if (n == 1023)
        {
            fprintf(2, "argument is too long\n");
            exit(1);
        }
        if (buf[n] == '\n')
        {
            break;
        }
        n++;
    }
    buf[n] = 0;
    if (n == 0)
        return 0;
    int offset = 0;
    while (offset < n)
    {
        new_argv[curr_argc++] = buf + offset;
        while (buf[offset] != ' ' && offset < n)
        {
            offset++;
        }
        while (buf[offset] == ' ' && offset < n)
        {
            buf[offset++] = 0;
        }
    }
    return curr_argc;
}

int main(int argc, char const *argv[])
{
    if (argc <= 1)
    {
        fprintf(2, "Usage: xargs command (arg ...)\n");
        exit(1);
    }
    char *command = malloc(strlen(argv[1]) + 1);
    char *new_argv[MAXARG];
    strcpy(command, argv[1]);
    for (int i = 1; i < argc; ++i)
    {
        new_argv[i - 1] = malloc(strlen(argv[i]) + 1);
        strcpy(new_argv[i - 1], argv[i]);
    }

    int curr_argc;
    while ((curr_argc = readline(new_argv, argc - 1)) != 0)
    {
        new_argv[curr_argc] = 0;
        if (fork() == 0)
        {
            exec(command, new_argv);
            fprintf(2, "exec failed\n");
            exit(1);
        }
        wait(0);
    }
    exit(0);
}
```

执行 `sh < xargstest.sh` 的输出结果如[实验文档](https://pdos.csail.mit.edu/6.828/2024/labs/util.html)所料,有多个`$`,但是没想明白怎么改find.c。 // 留个TODO

# summary
耗时大约8小时