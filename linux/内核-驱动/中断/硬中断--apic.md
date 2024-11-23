# 硬件规范
APIC的信息在SDM vol3中有比较详细的介绍
![alt text](../../../../medias/images_0/硬中断--apic_image.png)
```c
  Timer related:

      CCR: Current Count Register
      ICR: Initial Count Register
      DCR: Divide Configuration Register
      Timer: in LVT

  LVT (Local Vector Table):

      Timer
      Local Interrupt
      Performance Monitor Counters
      Thermal Sensor
      Error

  IPI:

      ICR: Interrupt Command Register
      LDR: Logical Destination Register
      DFR: Destination Format Register

  Interrupt State:

      ISR: In-Service Register
      IRR: Interrupt Request Register
      TMR: Trigger Mode Register
    ```