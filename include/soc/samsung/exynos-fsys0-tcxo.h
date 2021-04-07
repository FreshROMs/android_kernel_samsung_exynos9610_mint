#include <linux/kernel.h>
#include <linux/notifier.h>


enum exynos_fsys0_tcxo_event {
       /* UFS is entering the SLEEP state */
       SLEEP_ENTER,

       /* UFS failed to enter the SLEEP state */
       SLEEP_ENTER_FAIL,

       /* UFS is exiting the SLEEP state */
       SLEEP_EXIT,
};

int exynos_fsys0_tcxo_register_notifier(struct notifier_block *nb);
int exynos_fsys0_tcxo_unregister_notifier(struct notifier_block *nb);
int exynos_fsys0_tcxo_sleep_enter(void);
int exynos_fsys0_tcxo_sleep_exit(void);
