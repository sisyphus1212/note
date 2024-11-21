irq_desc 数组有 NR_IRQS 个元素, NR_IRQS 并不是 256-32(！！！), 实际上, 虽然中断描述符表中一共有 256 项(前 32 项用作异常和 intel 保留), 但并不是所有中断向量都会使用到, 所以中断描述符数组也不一定是 256-32 项, CPU 可以使用多少个中断是由中断控制器(PIC、APIC)或者内核配置决定的, 我们看看 NR_IRQS 的定义
```c
/* IOAPIC 为外部中断控制器 */
#ifdef CONFIG_X86_IO_APIC
#define CPU_VECTOR_LIMIT        (64 * NR_CPUS)
#define NR_IRQS                    \
    (CPU_VECTOR_LIMIT > IO_APIC_VECTOR_LIMIT ?    \
        (NR_VECTORS + CPU_VECTOR_LIMIT)  :    \
        (NR_VECTORS + IO_APIC_VECTOR_LIMIT))
#else /* !CONFIG_X86_IO_APIC: NR_IRQS_LEGACY = 16 */
#define NR_IRQS            NR_IRQS_LEGACY
#endif
```