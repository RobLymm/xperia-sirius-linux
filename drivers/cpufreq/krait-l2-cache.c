// SPDX-License-Identifier: GPL-2.0-only
/*
 * Krait L2 cache clock scaling.
 *
 * On Krait SoCs the four cores share one L2 cache with its own clock (the
 * krait-cc L2 mux, fed by the L2 HFPLL). cpufreq-dt scales the cores only, so
 * the cache stays at whatever rate krait-cc settled at during probe - 729.6
 * MHz on an MSM8974 - while the cores run from 300 MHz to 2265.6 MHz. Work
 * that waits on memory then gains little from a faster core.
 *
 * This driver sets the cache clock from the fastest online core, using a
 * CPU-rate to L2-rate table from the device tree (Qualcomm's downstream
 * msm-cpufreq table for the same SoC). The cache clock is driven through an
 * OPP table, so the OPP core raises the cache's voltage corner (required-opps
 * on the CX power domain) before speeding the clock up and lowers it after
 * slowing down.
 *
 * Device tree:
 *	clocks = <&kraitcc 4>;
 *	clock-names = "l2";
 *	power-domains = <&rpmpd ...>;
 *	operating-points-v2 = <&l2_opp_table>;
 *	qcom,cpu-l2-khz = <cpu_khz l2_khz>, ...;	ascending by cpu_khz
 *
 * The cache takes the L2 rate of the highest table entry whose cpu_khz does
 * not exceed the fastest online core.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

struct krait_l2 {
	struct device *dev;
	struct notifier_block nb;
	struct work_struct work;
	struct mutex lock;
	u32 *map;		/* pairs of cpu kHz, L2 kHz */
	unsigned int map_len;	/* number of pairs */
	unsigned int *khz;	/* last known rate of each CPU, kHz */
	unsigned long cur_hz;
};

static unsigned long krait_l2_target_hz(struct krait_l2 *l2)
{
	unsigned int cpu, khz, max_khz = 0;
	unsigned long target = l2->map[1];
	unsigned int i;

	for_each_online_cpu(cpu) {
		khz = READ_ONCE(l2->khz[cpu]);
		if (khz > max_khz)
			max_khz = khz;
	}

	for (i = 0; i < l2->map_len; i++)
		if (l2->map[2 * i] <= max_khz)
			target = l2->map[2 * i + 1];

	return target * 1000UL;
}

static void krait_l2_update(struct work_struct *work)
{
	struct krait_l2 *l2 = container_of(work, struct krait_l2, work);
	unsigned long hz;
	int ret;

	mutex_lock(&l2->lock);
	hz = krait_l2_target_hz(l2);
	if (hz != l2->cur_hz) {
		ret = dev_pm_opp_set_rate(l2->dev, hz);
		if (ret)
			dev_err_ratelimited(l2->dev, "failed to set L2 to %lu kHz: %d\n",
					    hz / 1000, ret);
		else
			l2->cur_hz = hz;
	}
	mutex_unlock(&l2->lock);
}

/*
 * The core updates policy->cur only after the POSTCHANGE notifiers have run,
 * so record the new rate here rather than reading it back later.
 */
static int krait_l2_cpufreq_notifier(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	struct krait_l2 *l2 = container_of(nb, struct krait_l2, nb);
	struct cpufreq_freqs *freqs = data;
	unsigned int cpu;

	if (event != CPUFREQ_POSTCHANGE)
		return NOTIFY_DONE;

	for_each_cpu(cpu, freqs->policy->cpus)
		WRITE_ONCE(l2->khz[cpu], freqs->new);

	schedule_work(&l2->work);

	return NOTIFY_OK;
}

static int krait_l2_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dev_pm_opp_config config = {
		.clk_names = (const char *[]){ "l2", NULL },
	};
	struct cpufreq_policy *policy;
	struct krait_l2 *l2;
	unsigned int cpu;
	int n, ret;

	/*
	 * Without cpufreq every core reads 0 kHz and the cache would drop to
	 * the lowest table rate while the cores run fast. Wait for it.
	 */
	policy = cpufreq_cpu_get(0);
	if (!policy)
		return -EPROBE_DEFER;
	cpufreq_cpu_put(policy);

	l2 = devm_kzalloc(dev, sizeof(*l2), GFP_KERNEL);
	if (!l2)
		return -ENOMEM;
	l2->dev = dev;

	n = of_property_count_u32_elems(dev->of_node, "qcom,cpu-l2-khz");
	if (n < 2 || n % 2)
		return dev_err_probe(dev, -EINVAL, "invalid qcom,cpu-l2-khz\n");

	l2->map = devm_kcalloc(dev, n, sizeof(*l2->map), GFP_KERNEL);
	l2->khz = devm_kcalloc(dev, nr_cpu_ids, sizeof(*l2->khz), GFP_KERNEL);
	if (!l2->map || !l2->khz)
		return -ENOMEM;

	ret = of_property_read_u32_array(dev->of_node, "qcom,cpu-l2-khz", l2->map, n);
	if (ret)
		return dev_err_probe(dev, ret, "cannot read qcom,cpu-l2-khz\n");
	l2->map_len = n / 2;

	ret = devm_pm_opp_set_config(dev, &config);
	if (ret)
		return dev_err_probe(dev, ret, "cannot configure the L2 clock\n");

	ret = devm_pm_opp_of_add_table(dev);
	if (ret)
		return dev_err_probe(dev, ret, "cannot add the L2 OPP table\n");

	for_each_possible_cpu(cpu)
		l2->khz[cpu] = cpufreq_quick_get(cpu);

	mutex_init(&l2->lock);
	INIT_WORK(&l2->work, krait_l2_update);
	platform_set_drvdata(pdev, l2);

	krait_l2_update(&l2->work);

	l2->nb.notifier_call = krait_l2_cpufreq_notifier;
	ret = cpufreq_register_notifier(&l2->nb, CPUFREQ_TRANSITION_NOTIFIER);
	if (ret)
		return dev_err_probe(dev, ret, "cannot register the cpufreq notifier\n");

	dev_info(dev, "L2 cache at %lu kHz, following %u table entries\n",
		 l2->cur_hz / 1000, l2->map_len);

	return 0;
}

static void krait_l2_remove(struct platform_device *pdev)
{
	struct krait_l2 *l2 = platform_get_drvdata(pdev);

	cpufreq_unregister_notifier(&l2->nb, CPUFREQ_TRANSITION_NOTIFIER);
	cancel_work_sync(&l2->work);
}

static const struct of_device_id krait_l2_match[] = {
	{ .compatible = "qcom,msm8974-krait-l2" },
	{ }
};
MODULE_DEVICE_TABLE(of, krait_l2_match);

static struct platform_driver krait_l2_driver = {
	.probe = krait_l2_probe,
	.remove = krait_l2_remove,
	.driver = {
		.name = "krait-l2-cache",
		.of_match_table = krait_l2_match,
	},
};
module_platform_driver(krait_l2_driver);

MODULE_DESCRIPTION("Qualcomm Krait L2 cache clock scaling");
MODULE_LICENSE("GPL");
