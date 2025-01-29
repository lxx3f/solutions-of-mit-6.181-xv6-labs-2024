# speed up system calls

难点在于如何确定申请syscallpage和mappages以及取消映射和释放内存的时机,我第一次出错就是因为没注意到allocproc函数内部调用了proc_pagetable函数,把kalloc()放在了proc_pagetable之后,导致出错,实际应在页表映射创建之前申请页空间。

# print page table
note:输出`printf("%d: pte %p pa %p\n", i, pte, PTE2PA(pte));`时出现参数格式错误,需要类型转换`(void*)pte`。

# use superpages
函数调用路径:`sys_sbrk -> growproc -> uvmalloc -> kalloc`,因此修改kalloc.c实现超级页的分配和释放机制,然后修改uvmalloc()的内存分配逻辑实现使用超级页。

这里我增加了一个`PTE_SUPER`的pte标志位。