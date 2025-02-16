# mmap
最后一个lab，综合了前面的很多内容，代码量很大。

[详细代码](https://github.com/lxx3f/solutions-of-mit-6.181-xv6-labs-2024/commit/a9271ac80ec0b00c096b21144a8ab18a0d3ab5e8)

## 印象最深的一个bug
在这个测试点卡了很久:
```c
if(read(fd, buf, PGSIZE) != PGSIZE/2)
    err("dirty read #2");
```

一开始没有去翻mmaptest文件, 一直在检查自己的代码是不是有什么逻辑漏洞, 最后发现问题出在脏页写回这一步, 写回时不能改变file的size, 这个测试点就是因为fd剩余的大小是半页, 但写回时写了一整页, 导致read读出了一整页。

关键修改如下：
```c
// kernel/file.c: writeback()
    int n = nr_page * PGSIZE;
+   int len = f->ip->size - off;
+   if(n > len){
+     n = len;
  }
```

