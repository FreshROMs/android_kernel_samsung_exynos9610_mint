#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>

#include <soc/samsung/exynos-fsys0-tcxo.h>

#include <linux/notifier.h>

static DEFINE_RWLOCK(exynos_fsys0_tcxo_notifier_lock);
static RAW_NOTIFIER_HEAD(exynos_fsys0_tcxo_notifier_chain);


static int exynos_fsys0_tcxo_notify(enum exynos_fsys0_tcxo_event event, int nr_to_call, int *nr_calls)
{
       int ret;

       ret = __raw_notifier_call_chain(&exynos_fsys0_tcxo_notifier_chain, event, NULL,
               nr_to_call, nr_calls);

       return notifier_to_errno(ret);
}

int exynos_fsys0_tcxo_register_notifier(struct notifier_block *nb)
{
      unsigned long flags;
       int ret;
       write_lock_irqsave(&exynos_fsys0_tcxo_notifier_lock, flags);
       ret = raw_notifier_chain_register(&exynos_fsys0_tcxo_notifier_chain, nb);
       write_unlock_irqrestore(&exynos_fsys0_tcxo_notifier_lock, flags);

       return ret;
}
EXPORT_SYMBOL_GPL(exynos_fsys0_tcxo_register_notifier);


int exynos_fsys0_tcxo_unregister_notifier(struct notifier_block *nb)
{
       unsigned long flags;
       int ret;

       write_lock_irqsave(&exynos_fsys0_tcxo_notifier_lock, flags);
       ret = raw_notifier_chain_unregister(&exynos_fsys0_tcxo_notifier_chain, nb);
       write_unlock_irqrestore(&exynos_fsys0_tcxo_notifier_lock, flags);

       return ret;
}
EXPORT_SYMBOL_GPL(exynos_fsys0_tcxo_unregister_notifier);



int exynos_fsys0_tcxo_sleep_enter(void)
{
       int nr_calls;
       int ret = 0;

       read_lock(&exynos_fsys0_tcxo_notifier_lock);
       ret = exynos_fsys0_tcxo_notify(SLEEP_ENTER, -1, &nr_calls);
       if (ret)
               /*
                * Inform listeners (nr_calls - 1) about failure of LPA
                * entry who are notified earlier to prepare for it.
                */
               exynos_fsys0_tcxo_notify(SLEEP_ENTER_FAIL, nr_calls - 1, NULL);
       read_unlock(&exynos_fsys0_tcxo_notifier_lock);

       return ret;
}
EXPORT_SYMBOL_GPL(exynos_fsys0_tcxo_sleep_enter);



int exynos_fsys0_tcxo_sleep_exit(void)
{
       int ret;

       read_lock(&exynos_fsys0_tcxo_notifier_lock);
       ret = exynos_fsys0_tcxo_notify(SLEEP_EXIT, -1, NULL);
       read_unlock(&exynos_fsys0_tcxo_notifier_lock);

       return ret;
}
EXPORT_SYMBOL_GPL(exynos_fsys0_tcxo_sleep_exit);
