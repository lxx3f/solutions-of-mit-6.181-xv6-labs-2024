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
