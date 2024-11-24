extern struct iommu_table_entry __iommu_table[], __iommu_table_end[];

void __init pci_iommu_alloc(void)
{
	struct iommu_table_entry *p;

	sort_iommu_table(__iommu_table, __iommu_table_end);
	check_iommu_entries(__iommu_table, __iommu_table_end);

	for (p = __iommu_table; p < __iommu_table_end; p++) {
		if (p && p->detect && p->detect() > 0) {
			p->flags |= IOMMU_DETECTED;
			if (p->early_init)
				p->early_init();
			if (p->flags & IOMMU_FINISH_IF_DETECTED)
				break;
		}
	}
}

/*
 * See <Documentation/x86/x86_64/boot-options.rst> for the iommu kernel
 * parameter documentation.
 */
static __init int iommu_setup(char *p)
{
	iommu_merge = 1;

	if (!p)
		return -EINVAL;

	while (*p) {
		if (!strncmp(p, "off", 3))
			no_iommu = 1;
		/* gart_parse_options has more force support */
		if (!strncmp(p, "force", 5))
			force_iommu = 1;
		if (!strncmp(p, "noforce", 7)) {
			iommu_merge = 0;
			force_iommu = 0;
		}

		if (!strncmp(p, "biomerge", 8)) {
			iommu_merge = 1;
			force_iommu = 1;
		}
		if (!strncmp(p, "panic", 5))
			panic_on_overflow = 1;
		if (!strncmp(p, "nopanic", 7))
			panic_on_overflow = 0;
		if (!strncmp(p, "merge", 5)) {
			iommu_merge = 1;
			force_iommu = 1;
		}
		if (!strncmp(p, "nomerge", 7))
			iommu_merge = 0;
		if (!strncmp(p, "forcesac", 8))
			pr_warn("forcesac option ignored.\n");
		if (!strncmp(p, "allowdac", 8))
			pr_warn("allowdac option ignored.\n");
		if (!strncmp(p, "nodac", 5))
			pr_warn("nodac option ignored.\n");
		if (!strncmp(p, "usedac", 6)) {
			disable_dac_quirk = true;
			return 1;
		}
#ifdef CONFIG_SWIOTLB
		if (!strncmp(p, "soft", 4))
			swiotlb = 1;
#endif
		if (!strncmp(p, "pt", 2))
			iommu_set_default_passthrough(true);
		if (!strncmp(p, "nopt", 4))
			iommu_set_default_translated(true);

		gart_parse_options(p);

		p += strcspn(p, ",");
		if (*p == ',')
			++p;
	}
	return 0;
}
early_param("iommu", iommu_setup);

static int __init pci_iommu_init(void)
{
	struct iommu_table_entry *p;

	x86_init.iommu.iommu_init();

	for (p = __iommu_table; p < __iommu_table_end; p++) {
		if (p && (p->flags & IOMMU_DETECTED) && p->late_init)
			p->late_init();
	}

	return 0;
}
/* Must execute after PCI subsystem */
rootfs_initcall(pci_iommu_init);

#ifdef CONFIG_PCI
/* Many VIA bridges seem to corrupt data for DAC. Disable it here */

static int via_no_dac_cb(struct pci_dev *pdev, void *data)
{
	pdev->dev.bus_dma_limit = DMA_BIT_MASK(32);
	return 0;
}

static void via_no_dac(struct pci_dev *dev)
{
	if (!disable_dac_quirk) {
		dev_info(&dev->dev, "disabling DAC on VIA PCI bridge\n");
		pci_walk_bus(dev->subordinate, via_no_dac_cb, NULL);
	}
}
DECLARE_PCI_FIXUP_CLASS_FINAL(PCI_VENDOR_ID_VIA, PCI_ANY_ID,
				PCI_CLASS_BRIDGE_PCI, 8, via_no_dac);
#endif


int __init detect_intel_iommu(void)
{
	int ret;
	struct dmar_res_callback validate_drhd_cb = {
		.cb[ACPI_DMAR_TYPE_HARDWARE_UNIT] = &dmar_validate_one_drhd,
		.ignore_unhandled = true,
	};

	down_write(&dmar_global_lock);
	ret = dmar_table_detect();
	if (!ret)
		ret = dmar_walk_dmar_table((struct acpi_table_dmar *)dmar_tbl,
					   &validate_drhd_cb);
	if (!ret && !no_iommu && !iommu_detected &&
	    (!dmar_disabled || dmar_platform_optin())) {
		iommu_detected = 1;
		/* Make sure ACS will be enabled */
		pci_request_acs();
	}

#ifdef CONFIG_X86
	if (!ret) {
		x86_init.iommu.iommu_init = intel_iommu_init;
		x86_platform.iommu_shutdown = intel_iommu_shutdown;
	}

#endif

	if (dmar_tbl) {
		acpi_put_table(dmar_tbl);
		dmar_tbl = NULL;
	}
	up_write(&dmar_global_lock);

	return ret ? ret : 1;
}