利用内核中的smp_call_function_single()和smp_call_function_many()函数，来测量IPI性能。

在我的测试环境中，物理机采用的是Intel(R) Xeon(R) Gold 6148 CPU @ 2.40GHz 2路共80超线程的CPU。虚拟机运行在KVM上的72核CentOS系统。


| IPI类型                | 100000次IPI耗时(ns) |
| -----------           | -----------         |
| 发给所有CPU            | 15601355147         |
| 发给同NUMA的一个CPU    | 230063725           |
| 发给跨NUMA的一个CPU    | 665893155           |

为了知道IPI测试时kvm在背后到底进行了什么动作。我们使用perf kvm工具来统计。

perf kvm stat record -a sleep 100

人工掐好时间，在IPI测试时在物理机上运行该命令，测试完成时中断perf工具。

perf kvm stat report

采用该命令来分析数据。

![alt text](../../medias/images_0/IPI中断性能评估_image.png)

虚拟机中IPI的开销基本花在了MSR_WRITE 和 HLT虚拟化上。

HLT是由于CPU进入idle状态时，就会调用该指令把CPU挂起。这样虚拟CPU挂起后就能出让物理CPU给其它进程使用。如果我们不允许虚拟机中CPU挂起会如何呢？可以修改虚拟机启动选项，增加idle=poll选项。
![alt text](../../medias/images_0/IPI中断性能评估_image-1.png)

![alt text](../../medias/images_0/IPI中断性能评估_image-2.png)

在虚拟机角度观察，IPI中断耗时明显减少，特别是跨NUMA的情况。

在物理机角度观察，从VM-EXIT到VM-ENTRY之间经历的时间，从5956389325us减少到了30678080us。数量级的减少。

# 分析
 为何HLT的虚拟化，消耗了那么多时间？谷歌工程师David Matlack的一篇[文章](https://www.linux-kvm.org/images/a/ac/02x03-Davit_Matalack-KVM_Message_passing_Performance.pdf)给了我们答案。这里引用他的文章截图：

![alt text](../../medias/images_0/IPI中断性能评估_image-3.png)
![alt text](../../medias/images_0/IPI中断性能评估_image-4.png)

这篇文章的测试环境，使用的是x2apic中断控制器，虚拟机发起IPI中断通过写入MSR寄存器开始，到目标VCPU调用IPI中断服务程序结束。

从图可见，时间集中消耗在了kvm_vcpu_kick()和schedule()上。当VCPU执行HLT指令挂起自己，陷入VMM处理，VMM知道该VCPU目前不需要使用了，便将该VCPU所在线程挂起进入睡眠状态。当另一个VCPU需要唤醒该挂起的VCPU时，就在虚拟机内发起IPI中断，陷入到VMM中，随后便是执行kvm_vcpu_kick()和schedule()函数，最后注入IPI中断到目标VCPU。

虚拟机使用idle=poll启动选项，能够完全避免VCPU执行HTL指令，因此，该VCPU在物理机上对应的线程将一直占用cpu(除非被中断或者抢占)，那么该线程几乎不会睡眠，那么kvm_vcpu_kick()和schedule()调用开销将大幅下降(假设系统中没有别的高优先级进程和频繁外部中断)（如下图）。这也就是为何虚拟机中IPI测试耗时减少的原因。

# 进一步
 看起来设置虚拟机idle=poll启动选项完全避免了HTL指令执行，可以大幅提升虚拟机中IPI中断性能，这将直接提升网络服务性能。问题真正解决了吗？答案是否定的。
 1. 从虚拟机角度来看，IPI中断性能是提高了，网络，数据库服务性能都能提高。但是从物理机角度来看，由于本该挂起进入睡眠的VCPU，现在不再睡眠，而是持续占有CPU。这对云主机可不是件好事情，因为这部分"空闲"CPU配额本该交给别的虚拟机来执行，现在却被禁止了HTL的虚拟机在空转，实在是在经济上不划算。
 2. 虚拟机中禁止HTL也不是所有情况下都有明显效果。假如虚拟机中的业务场景CPU负载很高，到了100%，该场景中自然不会调用HTL(VCPU没有空闲，自然不会进入idle状态)。因此，修改idle=poll启动选项就失去了作用。

为了在物理机经济效益和虚拟机性能最大化之间取得折中，目前内核的方案是提供了halt_poll_ns机制，即在VCPU HTL之前，先轮询下有没有虚拟中断要来，来的话就马上注入虚拟机，如果超过轮询上限都没有虚拟中断过来，才真正进入睡眠。

 测试halt_poll_ns方案，echo 2000000 > /sys/module/kvm/parameters/halt_poll_ns

 ![alt text](../../medias/images_0/IPI中断性能评估_image-5.png)
 测试结果介于虚拟机缺省配置和设置idle=poll性能之间