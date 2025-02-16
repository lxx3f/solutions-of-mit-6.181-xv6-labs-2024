# memory allocator

两个优化思路:

1.考虑程序运行的时间相关性,freepage不够的情况很可能连续出现,所以可以在借用其他cpu的freepage的时候多拿一些,我这里设置的是一次至多借用1024个page;

2.引用自[LRL52的博客](https://lrl52.top/1229/6-1810-lab8-lock-parallelism-locking/) : "考虑到 NCPU 的值为 8, 而 xv6 启动默认为 3 核。因此在当前 CPU 内存不够去其它 CPU 窃取空闲页时，可以从当前 cpuid + 3 的 CPU 上窃取，这样可以减少多个 core 同时内存不够用时的锁争用可能。这里还用到了一点数论的技巧, 由于, 因此这么做是可行的, 总会遍历完所有可用的 CPU。" 

[详细代码](https://github.com/lxx3f/solutions-of-mit-6.181-xv6-labs-2024/commit/2b96146ac15092f949816b314fe6b2848de3857f)


# bcache
这里的难点就是注意acquire(lock)和release(lock)的时机，并发导致的bug很难找。

[详细代码](https://github.com/lxx3f/solutions-of-mit-6.181-xv6-labs-2024/commit/a8ed63d8571832f237eb83dbd2556ada222ac131)