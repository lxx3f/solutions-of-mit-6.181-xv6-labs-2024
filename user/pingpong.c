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
