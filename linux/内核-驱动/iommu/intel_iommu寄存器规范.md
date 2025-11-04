# Intel IOMMU register specification
```c
/*
 * Intel IOMMU register specification per version 1.0 public spec.
 */
#define	DMAR_VER_REG	0x0	/* Arch version supported by this IOMMU */
#define	DMAR_CAP_REG	0x8	/* Hardware supported capabilities */
#define	DMAR_ECAP_REG	0x10	/* Extended capabilities supported */
#define	DMAR_GCMD_REG	0x18	/* Global command register */
#define	DMAR_GSTS_REG	0x1c	/* Global status register */
#define	DMAR_RTADDR_REG	0x20	/* Root entry table */
#define	DMAR_CCMD_REG	0x28	/* Context command reg */
#define	DMAR_FSTS_REG	0x34	/* Fault Status register */
#define	DMAR_FECTL_REG	0x38	/* Fault control register */
#define	DMAR_FEDATA_REG	0x3c	/* Fault event interrupt data register */
#define	DMAR_FEADDR_REG	0x40	/* Fault event interrupt addr register */
#define	DMAR_FEUADDR_REG 0x44	/* Upper address register */
#define	DMAR_AFLOG_REG	0x58	/* Advanced Fault control */
#define	DMAR_PMEN_REG	0x64	/* Enable Protected Memory Region */
#define	DMAR_PLMBASE_REG 0x68	/* PMRR Low addr */
#define	DMAR_PLMLIMIT_REG 0x6c	/* PMRR low limit */
#define	DMAR_PHMBASE_REG 0x70	/* pmrr high base addr */
#define	DMAR_PHMLIMIT_REG 0x78	/* pmrr high limit */
#define DMAR_IQH_REG	0x80	/* Invalidation queue head register */
#define DMAR_IQT_REG	0x88	/* Invalidation queue tail register */
#define DMAR_IQ_SHIFT	4	/* Invalidation queue head/tail shift */
#define DMAR_IQA_REG	0x90	/* Invalidation queue addr register */
#define DMAR_ICS_REG	0x9c	/* Invalidation complete status register */
#define DMAR_IQER_REG	0xb0	/* Invalidation queue error record register */
#define DMAR_IRTA_REG	0xb8    /* Interrupt remapping table addr register */
#define DMAR_PQH_REG	0xc0	/* Page request queue head register */
#define DMAR_PQT_REG	0xc8	/* Page request queue tail register */
#define DMAR_PQA_REG	0xd0	/* Page request queue address register */
#define DMAR_PRS_REG	0xdc	/* Page request status register */
#define DMAR_PECTL_REG	0xe0	/* Page request event control register */
#define	DMAR_PEDATA_REG	0xe4	/* Page request event interrupt data register */
#define	DMAR_PEADDR_REG	0xe8	/* Page request event interrupt addr register */
#define	DMAR_PEUADDR_REG 0xec	/* Page request event Upper address register */
#define DMAR_MTRRCAP_REG 0x100	/* MTRR capability register */
#define DMAR_MTRRDEF_REG 0x108	/* MTRR default type register */
#define DMAR_MTRR_FIX64K_00000_REG 0x120 /* MTRR Fixed range registers */
#define DMAR_MTRR_FIX16K_80000_REG 0x128
#define DMAR_MTRR_FIX16K_A0000_REG 0x130
#define DMAR_MTRR_FIX4K_C0000_REG 0x138
#define DMAR_MTRR_FIX4K_C8000_REG 0x140
#define DMAR_MTRR_FIX4K_D0000_REG 0x148
#define DMAR_MTRR_FIX4K_D8000_REG 0x150
#define DMAR_MTRR_FIX4K_E0000_REG 0x158
#define DMAR_MTRR_FIX4K_E8000_REG 0x160
#define DMAR_MTRR_FIX4K_F0000_REG 0x168
#define DMAR_MTRR_FIX4K_F8000_REG 0x170
#define DMAR_MTRR_PHYSBASE0_REG 0x180 /* MTRR Variable range registers */
#define DMAR_MTRR_PHYSMASK0_REG 0x188
#define DMAR_MTRR_PHYSBASE1_REG 0x190
#define DMAR_MTRR_PHYSMASK1_REG 0x198
#define DMAR_MTRR_PHYSBASE2_REG 0x1a0
#define DMAR_MTRR_PHYSMASK2_REG 0x1a8
#define DMAR_MTRR_PHYSBASE3_REG 0x1b0
#define DMAR_MTRR_PHYSMASK3_REG 0x1b8
#define DMAR_MTRR_PHYSBASE4_REG 0x1c0
#define DMAR_MTRR_PHYSMASK4_REG 0x1c8
#define DMAR_MTRR_PHYSBASE5_REG 0x1d0
#define DMAR_MTRR_PHYSMASK5_REG 0x1d8
#define DMAR_MTRR_PHYSBASE6_REG 0x1e0
#define DMAR_MTRR_PHYSMASK6_REG 0x1e8
#define DMAR_MTRR_PHYSBASE7_REG 0x1f0
#define DMAR_MTRR_PHYSMASK7_REG 0x1f8
#define DMAR_MTRR_PHYSBASE8_REG 0x200
#define DMAR_MTRR_PHYSMASK8_REG 0x208
#define DMAR_MTRR_PHYSBASE9_REG 0x210
#define DMAR_MTRR_PHYSMASK9_REG 0x218
#define DMAR_VCCAP_REG		0xe30 /* Virtual command capability register */
#define DMAR_VCMD_REG		0xe00 /* Virtual command register */
#define DMAR_VCRSP_REG		0xe10 /* Virtual command response register */

#define DMAR_IQER_REG_IQEI(reg)		FIELD_GET(GENMASK_ULL(3, 0), reg)
#define DMAR_IQER_REG_ITESID(reg)	FIELD_GET(GENMASK_ULL(47, 32), reg)
#define DMAR_IQER_REG_ICESID(reg)	FIELD_GET(GENMASK_ULL(63, 48), reg)

```