/*
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _MMC_EXYNOS_FMP_H_
#define _MMC_EXYNOS_FMP_H_

#if defined(CONFIG_MMC_DW_EXYNOS_FMP)
int exynos_mmc_fmp_cfg(struct dw_mci *host,
				void *desc,
				struct mmc_data *mmc_data,
				struct page *page,
				int sector_offfset,
				bool cmdq_enabled);
int exynos_mmc_fmp_clear(struct dw_mci *host, void *desc,
				bool cmdq_enabled);

int exynos_mmc_fmp_sec_cfg(struct dw_mci *host);
#else
inline int exynos_mmc_fmp_cfg(struct dw_mci *host,
		       void *desc,
		       struct mmc_data *mmc_data,
		       struct page *page, int sector_offset, bool cmdq_enabled)
{
	return 0;
}

inline int exynos_mmc_fmp_clear(struct dw_mci *host, void *desc, bool cmdq_enabled)
{
	return 0;
}

inline int exynos_mmc_fmp_sec_cfg(struct dw_mci *host)
{
	return 0;
}
#endif
#if defined(CONFIG_MMC_DW_EXYNOS_SMU)
int exynos_mmc_smu_init(struct dw_mci *host);
int exynos_mmc_smu_resume(struct dw_mci *host);
int exynos_mmc_smu_abort(struct dw_mci *host);
#else
inline int exynos_mmc_smu_init(struct dw_mci *host)
{
	/* smu entry0 init */
	mci_writel(host, MPSBEGIN0, 0);
	mci_writel(host, MPSEND0, 0xffffffff);
	mci_writel(host, MPSLUN0, 0xff);
	mci_writel(host, MPSCTRL0, DWMCI_MPSCTRL_BYPASS);
	return 0;
}

inline int exynos_mmc_smu_resume(struct dw_mci *host)
{
	exynos_mmc_smu_init(host);
	return 0;
}

inline int exynos_mmc_smu_abort(struct dw_mci *host)
{
	return 0;
}
#endif
#endif /* _MMC_EXYNOS_FMP_H_ */
