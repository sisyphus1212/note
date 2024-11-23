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

# 初始化
```c
    /**
 * setup_local_APIC - setup the local APIC
 *
 * Used to setup local APIC while initializing BSP or bringing up APs.
 * Always called with preemption disabled.
 */
static void setup_local_APIC(void)
{
	int cpu = smp_processor_id();
	unsigned int value;

	if (disable_apic) {
		disable_ioapic_support();
		return;
	}

	/*
	 * If this comes from kexec/kcrash the APIC might be enabled in
	 * SPIV. Soft disable it before doing further initialization.
	 */
	value = apic_read(APIC_SPIV);
	value &= ~APIC_SPIV_APIC_ENABLED;
	apic_write(APIC_SPIV, value);

#ifdef CONFIG_X86_32
	/* Pound the ESR really hard over the head with a big hammer - mbligh */
	if (lapic_is_integrated() && apic->disable_esr) {
		apic_write(APIC_ESR, 0);
		apic_write(APIC_ESR, 0);
		apic_write(APIC_ESR, 0);
		apic_write(APIC_ESR, 0);
	}
#endif
	/*
	 * Double-check whether this APIC is really registered.
	 * This is meaningless in clustered apic mode, so we skip it.
	 */
	BUG_ON(!apic->apic_id_registered());

	/*
	 * Intel recommends to set DFR, LDR and TPR before enabling
	 * an APIC.  See e.g. "AP-388 82489DX User's Manual" (Intel
	 * document number 292116).  So here it goes...
	 */
	apic->init_apic_ldr();

#ifdef CONFIG_X86_32
	if (apic->dest_mode_logical) {
		int logical_apicid, ldr_apicid;

		/*
		 * APIC LDR is initialized.  If logical_apicid mapping was
		 * initialized during get_smp_config(), make sure it matches
		 * the actual value.
		 */
		logical_apicid = early_per_cpu(x86_cpu_to_logical_apicid, cpu);
		ldr_apicid = GET_APIC_LOGICAL_ID(apic_read(APIC_LDR));
		if (logical_apicid != BAD_APICID)
			WARN_ON(logical_apicid != ldr_apicid);
		/* Always use the value from LDR. */
		early_per_cpu(x86_cpu_to_logical_apicid, cpu) = ldr_apicid;
	}
#endif

	/*
	 * Set Task Priority to 'accept all except vectors 0-31'.  An APIC
	 * vector in the 16-31 range could be delivered if TPR == 0, but we
	 * would think it's an exception and terrible things will happen.  We
	 * never change this later on.
	 */
	value = apic_read(APIC_TASKPRI);
	value &= ~APIC_TPRI_MASK;
	value |= 0x10;
	apic_write(APIC_TASKPRI, value);

	/* Clear eventually stale ISR/IRR bits */
	apic_pending_intr_clear();

	/*
	 * Now that we are all set up, enable the APIC
	 */
	value = apic_read(APIC_SPIV);
	value &= ~APIC_VECTOR_MASK;
	/*
	 * Enable APIC
	 */
	value |= APIC_SPIV_APIC_ENABLED;

#ifdef CONFIG_X86_32
	/*
	 * Some unknown Intel IO/APIC (or APIC) errata is biting us with
	 * certain networking cards. If high frequency interrupts are
	 * happening on a particular IOAPIC pin, plus the IOAPIC routing
	 * entry is masked/unmasked at a high rate as well then sooner or
	 * later IOAPIC line gets 'stuck', no more interrupts are received
	 * from the device. If focus CPU is disabled then the hang goes
	 * away, oh well :-(
	 *
	 * [ This bug can be reproduced easily with a level-triggered
	 *   PCI Ne2000 networking cards and PII/PIII processors, dual
	 *   BX chipset. ]
	 */
	/*
	 * Actually disabling the focus CPU check just makes the hang less
	 * frequent as it makes the interrupt distribution model be more
	 * like LRU than MRU (the short-term load is more even across CPUs).
	 */

	/*
	 * - enable focus processor (bit==0)
	 * - 64bit mode always use processor focus
	 *   so no need to set it
	 */
	value &= ~APIC_SPIV_FOCUS_DISABLED;
#endif

	/*
	 * Set spurious IRQ vector
	 */
	value |= SPURIOUS_APIC_VECTOR;
	apic_write(APIC_SPIV, value);

	perf_events_lapic_init();

	/*
	 * Set up LVT0, LVT1:
	 *
	 * set up through-local-APIC on the boot CPU's LINT0. This is not
	 * strictly necessary in pure symmetric-IO mode, but sometimes
	 * we delegate interrupts to the 8259A.
	 */
	/*
	 * TODO: set up through-local-APIC from through-I/O-APIC? --macro
	 */
	value = apic_read(APIC_LVT0) & APIC_LVT_MASKED;
	if (!cpu && (pic_mode || !value || skip_ioapic_setup)) {
		value = APIC_DM_EXTINT;
		apic_printk(APIC_VERBOSE, "enabled ExtINT on CPU#%d\n", cpu);
	} else {
		value = APIC_DM_EXTINT | APIC_LVT_MASKED;
		apic_printk(APIC_VERBOSE, "masked ExtINT on CPU#%d\n", cpu);
	}
	apic_write(APIC_LVT0, value);

	/*
	 * Only the BSP sees the LINT1 NMI signal by default. This can be
	 * modified by apic_extnmi= boot option.
	 */
	if ((!cpu && apic_extnmi != APIC_EXTNMI_NONE) ||
	    apic_extnmi == APIC_EXTNMI_ALL)
		value = APIC_DM_NMI;
	else
		value = APIC_DM_NMI | APIC_LVT_MASKED;

	/* Is 82489DX ? */
	if (!lapic_is_integrated())
		value |= APIC_LVT_LEVEL_TRIGGER;
	apic_write(APIC_LVT1, value);

#ifdef CONFIG_X86_MCE_INTEL
	/* Recheck CMCI information after local APIC is up on CPU #0 */
	if (!cpu)
		cmci_recheck();
#endif
}
```