```c
/**
 * struct msi_domain_ops - MSI interrupt domain callbacks
 * @get_hwirq:		Retrieve the resulting hw irq number
 * @msi_init:		Domain specific init function for MSI interrupts
 * @msi_free:		Domain specific function to free a MSI interrupts
 * @msi_check:		Callback for verification of the domain/info/dev data
 * @msi_prepare:	Prepare the allocation of the interrupts in the domain
 * @msi_finish:		Optional callback to finalize the allocation
 * @set_desc:		Set the msi descriptor for an interrupt
 * @handle_error:	Optional error handler if the allocation fails
 * @domain_alloc_irqs:	Optional function to override the default allocation
 *			function.
 * @domain_free_irqs:	Optional function to override the default free
 *			function.
 *
 * @get_hwirq, @msi_init and @msi_free are callbacks used by
 * msi_create_irq_domain() and related interfaces
 *
 * @msi_check, @msi_prepare, @msi_finish, @set_desc and @handle_error
 * are callbacks used by msi_domain_alloc_irqs() and related
 * interfaces which are based on msi_desc.
 *
 * @domain_alloc_irqs, @domain_free_irqs can be used to override the
 * default allocation/free functions (__msi_domain_alloc/free_irqs). This
 * is initially for a wrapper around XENs seperate MSI universe which can't
 * be wrapped into the regular irq domains concepts by mere mortals.  This
 * allows to universally use msi_domain_alloc/free_irqs without having to
 * special case XEN all over the place.
 *
 * Contrary to other operations @domain_alloc_irqs and @domain_free_irqs
 * are set to the default implementation if NULL and even when
 * MSI_FLAG_USE_DEF_DOM_OPS is not set to avoid breaking existing users and
 * because these callbacks are obviously mandatory.
 *
 * This is NOT meant to be abused, but it can be useful to build wrappers
 * for specialized MSI irq domains which need extra work before and after
 * calling __msi_domain_alloc_irqs()/__msi_domain_free_irqs().
 */

struct msi_domain_ops {
	irq_hw_number_t	(*get_hwirq)(struct msi_domain_info *info,
				     msi_alloc_info_t *arg);
	int		(*msi_init)(struct irq_domain *domain,
				    struct msi_domain_info *info,
				    unsigned int virq, irq_hw_number_t hwirq,
				    msi_alloc_info_t *arg); // 设置了hwirq和virq的映射
	void		(*msi_free)(struct irq_domain *domain,
				    struct msi_domain_info *info,
				    unsigned int virq);
	int		(*msi_check)(struct irq_domain *domain,
				     struct msi_domain_info *info,
				     struct device *dev);
	int		(*msi_prepare)(struct irq_domain *domain,
				       struct device *dev, int nvec,
				       msi_alloc_info_t *arg);
	void		(*msi_finish)(msi_alloc_info_t *arg, int retval);
	void		(*set_desc)(msi_alloc_info_t *arg,
				    struct msi_desc *desc);
	int		(*handle_error)(struct irq_domain *domain,
					struct msi_desc *desc, int error);
	int		(*domain_alloc_irqs)(struct irq_domain *domain,
					     struct device *dev, int nvec);
	void		(*domain_free_irqs)(struct irq_domain *domain,
					    struct device *dev);
};

/**
 * struct msi_domain_info - MSI interrupt domain data
 * @flags:		Flags to decribe features and capabilities
 * @ops:		The callback data structure
 * @chip:		Optional: associated interrupt chip
 * @chip_data:		Optional: associated interrupt chip data
 * @handler:		Optional: associated interrupt flow handler
 * @handler_data:	Optional: associated interrupt flow handler data
 * @handler_name:	Optional: associated interrupt flow handler name
 * @data:		Optional: domain specific data
 */
struct msi_domain_info {
	u32			flags;
	struct msi_domain_ops	*ops;
	struct irq_chip		*chip;
	void			*chip_data;
	irq_flow_handler_t	handler;
	void			*handler_data;
	const char		*handler_name;
	void			*data;
};

static const struct irq_domain_ops msi_domain_ops = {
	.alloc		= msi_domain_alloc,
	.free		= msi_domain_free,
	.activate	= msi_domain_activate,
	.deactivate	= msi_domain_deactivate,
};

static int msi_domain_alloc(struct irq_domain *domain, unsigned int virq,
			    unsigned int nr_irqs, void *arg)
{
	struct msi_domain_info *info = domain->host_data;
	struct msi_domain_ops *ops = info->ops;
	irq_hw_number_t hwirq = ops->get_hwirq(info, arg);
	int i, ret;

	if (irq_find_mapping(domain, hwirq) > 0)
		return -EEXIST;

	if (domain->parent) {
		ret = irq_domain_alloc_irqs_parent(domain, virq, nr_irqs, arg);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < nr_irqs; i++) {
        //
		ret = ops->msi_init(domain, info, virq + i, hwirq + i, arg);
		if (ret < 0) {
			if (ops->msi_free) {
				for (i--; i > 0; i--)
					ops->msi_free(domain, info, virq + i);
			}
			irq_domain_free_irqs_top(domain, virq, nr_irqs);
			return ret;
		}
	}

	return 0;
}
```