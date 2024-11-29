![alt text](../../medias/images_0/kvm_image.png)
回到虚拟化中：虚拟I/O APIC接收到Guest内核I/O APIC填充中断重定向向量表的请求后，将中断向量、目标CPU、触发模式等中断信息记录到中断重定向表中。

```c
file: arch/x86/kvm/ioapic.c

static int ioapic_mmio_write(struct kvm_vcpu *vcpu, struct kvm_io_device *this, gpa_t addr, int len, const void *val)

{
...
    addr &= 0xff;
    spin_lock(&ioapic->lock);
    switch (addr) {
    case IOAPIC_REG_SELECT:
        ioapic->ioregsel = data & 0xFF; /* 8-bit register */
        break;
    case IOAPIC_REG_WINDOW:
        ioapic_write_indirect(ioapic, data);
        break;
    default:
        break;
    }
    spin_unlock(&ioapic->lock);
    return 0;
}

static void ioapic_write_indirect(struct kvm_ioapic *ioapic, u32 val)
{
    unsigned index;
    bool mask_before, mask_after;
    int old_remote_irr, old_delivery_status;
    union kvm_ioapic_redirect_entry *e;

    switch (ioapic->ioregsel) {
    case IOAPIC_REG_VERSION:
        /* Writes are ignored. */
        break;
    case IOAPIC_REG_APIC_ID:
        ioapic->id = (val >> 24) & 0xf;
        break;
    case IOAPIC_REG_ARB_ID:
        break;
    default:
        index = (ioapic->ioregsel - 0x10) >> 1;
        if (index >= IOAPIC_NUM_PINS)
            return;
        index = array_index_nospec(index, IOAPIC_NUM_PINS);
        e = &ioapic->redirtbl[index];
        mask_before = e->fields.mask;
        /* Preserve read-only fields */
        old_remote_irr = e->fields.remote_irr;
        old_delivery_status = e->fields.delivery_status;
        if (ioapic->ioregsel & 1) {
            e->bits &= 0xffffffff;
            e->bits |= (u64) val << 32;
        } else {
            e->bits &= ~0xffffffffULL;
            e->bits |= (u32) val;
        }

        e->fields.remote_irr = old_remote_irr;
        e->fields.delivery_status = old_delivery_status;
		...
    }
}
```