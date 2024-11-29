逻辑处理器在VMX操作时使用虚拟机控制数据结构（virtual-machine control data structures，VMCSs）。它们管理进入和退出VMX non-root(虚拟机入口和虚拟机出口)的过渡，以及VMX non-root中的处理器行为。这个结构由新的指令VMCLEAR、VMPTRLD、VMREAD和VMWRITE操作。
VMX需要为每个虚拟机分配一个VMCS结构，将虚拟机信息保存在一个4KB对齐的内存页上（称作VMCS region），这块内存的第11:0位必须为0。
同一时刻，可以存在多个active VMCS，但最多只能存在一个current VMCS。

VMCLEAR：初始化VMCS
VMPTRLD：选中一个VMCS作为current VMCS，需要提供其物理地址作为参数，执行之后，其他VMCS切换到active状态
VMWRITE：用于设置VMCS的各个字段，可以理解为填入VMCS的基本信息。由于在不同版本系统中相同字段的位置可能不同，因此没有采用直接对内存进行读写的方式来设置字段。

# Fields
描述：VMCS能够大致分为以下几个域（具体可参考Intel开发手册卷3第24.4~24.9小节）

Guest-State Area
Host-State Area
VM-Control Fields
VM-Exit Information Fields
其中，控制域又可以分为三个部分：

VM-Execution Control Fields
VM-Exit Control Fields
VM-Entry Information Fields

## Host-State Area
描述：Host-State域用于记录部分CPU的状态信息，因为CPU是在VMM与虚拟机中来回切换运行的，所以在VMCS中既要记录主机CPU的状态，也要记录虚拟机CPU的状态，这样才能保证虚拟机正常运行。

根据Intel手册，目前Host-State域需要设置的字段：

控制寄存器：Cr0、Cr3、Cr4
段寄存器：CS、SS、DS、ES、FS、GS、TR
基址段寄存器：FS、GS、TR、GDTR、IDTR
MSR寄存器：IA32_SYSENTER_CS、IA32_SYSENTER_ESP、IA32_SYSENTER_EIP
VM-Control Fields
描述：用于控制虚拟机行为，例如中断方式，是否启用EPT，启用I/O端口，MSR访问限制等等。

控制域相关字段不能够随意设置，需要参考MSR寄存器。

以Pin-Based VM-Execution Control Fields为例，对应PIN_BASED_VM_EXEC_CONTROL字段，它的值参考IA32_VMX_PINBASED_CTLS MSR寄存器（偏移为481H，可参考Intel开发手册卷3附录A.3.1），前32位表示哪些比特位位可以为1，后32位表示哪些比特位必须为1（各比特位含义可参考Intel开发手册卷3第24.6小节）。

因此，在设置PIN_BASED_VM_EXEC_CONTROL字段时，必须将该字段与IA32_VMX_PINBASED_CTLS MSR的前32位进行与运算，与其后32位进行或运算，以保证关键的比特位不出错，其他相关字段同理。

![alt text](../../medias/images_0/vmcs_image-4.png)

# 设置字段
VMCS字段列表如下（不同版本，各字段的位置可能不同），也可以参考Intel开发手册卷3附录B
![alt text](../../medias/images_0/vmcs_image.png)
![alt text](../../medias/images_0/vmcs_image-1.png)
![alt text](../../medias/images_0/vmcs_image-2.png)

各错误码含义可参考Intel开发手册卷3第30.4小节
![alt text](../../medias/images_0/vmcs_image-3.png)
以错误码7为例，含义是「VM entry with invalid control field(s)」，就是说控制域字段存在问题。
