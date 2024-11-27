Remapping硬件将来自于 集成在root-complex中 或 挂载在PCIE bus上的 设备的memory request分成两类：

Requests-without-PASID：这是来自于endpoint devices的普通memory requests。它们一般指明了访问类型(read/write/atomics)、DMA目的地的地址与大小、源设备标识。

"Atomics" 是一种访问类型，表示对内存执行原子操作。这些操作确保多个处理器或设备能够并发访问共享内存而不引起数据竞争或一致性问题。常见的原子操作包括：

原子读/写：确保数据读取或写入操作在执行期间不被打断。
原子比较和交换（Compare-and-Swap, CAS）：允许在单个操作中比较内存值并根据条件更新它。
原子加/减：对内存中的计数器或标志位进行增减操作，同时保证其他线程无法修改这些数据。