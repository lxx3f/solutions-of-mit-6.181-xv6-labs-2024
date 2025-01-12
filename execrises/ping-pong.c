#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define NUM_EXCHANGES 100000

int main()
{
    int pipe1[2]; // 用于父进程到子进程的管道
    int pipe2[2]; // 用于子进程到父进程的管道
    pid_t pid;
    char byte = 'A'; // 要传输的字节
    char buffer;

    // 创建管道
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1)
    {
        perror("pipe error");
        exit(EXIT_FAILURE);
    }

    // 创建子进程
    pid = fork();
    if (pid < 0)
    {
        perror("fork error");
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {                    // 父进程
        close(pipe1[0]); // 关闭读取端
        close(pipe2[1]); // 关闭写入端

        clock_t start = clock(); // 记录开始时间

        for (int i = 0; i < NUM_EXCHANGES; i++)
        {
            write(pipe1[1], &byte, 1);  // 向子进程发送字节
            read(pipe2[0], &buffer, 1); // 从子进程接收字节
        }

        clock_t end = clock();                                      // 记录结束时间
        double time_taken = (double)(end - start) / CLOCKS_PER_SEC; // 计算耗时
        printf("父进程完成 %d 次交换，耗时 %.2f 秒\n", NUM_EXCHANGES, time_taken);
        printf("交换速率：%.2f 次/秒\n", NUM_EXCHANGES / time_taken);

        close(pipe1[1]); // 关闭写入端
        close(pipe2[0]); // 关闭读取端

        wait(NULL); // 等待子进程结束
    }
    else
    {                    // 子进程
        close(pipe1[1]); // 关闭写入端
        close(pipe2[0]); // 关闭读取端

        for (int i = 0; i < NUM_EXCHANGES; i++)
        {
            read(pipe1[0], &buffer, 1); // 从父进程接收字节
            write(pipe2[1], &byte, 1);  // 向父进程发送字节
        }

        close(pipe1[0]); // 关闭读取端
        close(pipe2[1]); // 关闭写入端
        exit(0);
    }

    return 0;
}