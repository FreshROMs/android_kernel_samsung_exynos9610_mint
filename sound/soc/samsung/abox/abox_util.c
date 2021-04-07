#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <sound/pcm.h>

#include "abox_util.h"

void __iomem *devm_not_request_and_map(struct platform_device *pdev,
		const char *name, unsigned int num, phys_addr_t *phys_addr,
		size_t *size)
{
	struct resource *res;
	void __iomem *ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, num);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}
	if (phys_addr)
		*phys_addr = res->start;
	if (size)
		*size = resource_size(res);

	ret = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(ret)) {
		dev_err(&pdev->dev, "Failed to map %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	return ret;
}

void __iomem *devm_request_and_map(struct platform_device *pdev,
		const char *name, unsigned int num, phys_addr_t *phys_addr,
		size_t *size)
{
	struct resource *res;
	void __iomem *ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, num);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}
	if (phys_addr)
		*phys_addr = res->start;
	if (size)
		*size = resource_size(res);

	res = devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), name);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to request %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	ret = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(ret)) {
		dev_err(&pdev->dev, "Failed to map %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	return ret;
}

void __iomem *devm_request_and_map_byname(struct platform_device *pdev,
		const char *name, phys_addr_t *phys_addr, size_t *size)
{
	struct resource *res;
	void __iomem *ret;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, name);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to get %s\n", name);
		return ERR_PTR(-EINVAL);
	}
	if (phys_addr)
		*phys_addr = res->start;
	if (size)
		*size = resource_size(res);

	res = devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), name);
	if (IS_ERR_OR_NULL(res)) {
		dev_err(&pdev->dev, "Failed to request %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	ret = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(ret)) {
		dev_err(&pdev->dev, "Failed to map %s\n", name);
		return ERR_PTR(-EFAULT);
	}

	return ret;
}

struct clk *devm_clk_get_and_prepare(struct platform_device *pdev,
		const char *name)
{
	struct device *dev = &pdev->dev;
	struct clk *clk;
	int ret;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		dev_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	ret = clk_prepare(clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

u32 readl_phys(phys_addr_t addr)
{
	u32 ret;
	void __iomem *virt = ioremap(addr, 0x4);

	ret = readl(virt);
	pr_debug("%pa = %08x\n", &addr, ret);
	iounmap(virt);

	return ret;
}

void writel_phys(unsigned int val, phys_addr_t addr)
{
	void __iomem *virt = ioremap(addr, 0x4);

	writel(val, virt);
	pr_debug("%pa <= %08x\n", &addr, val);
	iounmap(virt);
}

bool is_secure_gic(void)
{
	pr_debug("%s: %08x, %08x\n", __func__, readl_phys(0x10000000),
			readl_phys(0x10000010));
	return (readl_phys(0x10000000) == 0xE8895000) &&
			(readl_phys(0x10000010) == 0x0);
}

u64 width_range_to_bits(unsigned int width_min, unsigned int width_max)
{
	static const struct {
		unsigned int width;
		u64 format;
	} map[] = {
		{  8, SNDRV_PCM_FMTBIT_S8  },
		{ 16, SNDRV_PCM_FMTBIT_S16 },
		{ 24, SNDRV_PCM_FMTBIT_S24 },
		{ 32, SNDRV_PCM_FMTBIT_S32 },
	};

	int i;
	u64 fmt = 0;

	for (i = 0; i < ARRAY_SIZE(map); i++) {
		if (map[i].width >= width_min && map[i].width <= width_max)
			fmt |= map[i].format;
	}

	return fmt;
}

char substream_to_char(struct snd_pcm_substream *substream)
{
	return (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 'p' : 'c';
}