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