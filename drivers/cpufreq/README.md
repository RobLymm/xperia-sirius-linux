# CPU frequency scaling on the Xperia Z2

**This works, across the full rated range.** 300 MHz to 2265.6 MHz on
`cpufreq-dt` with the schedutil governor: all four cores reach the top under
load and drop to 300 MHz at idle, at 36-45 C. Measured rather than assumed —
the same timed workload takes 25.2 s at 300 MHz and 4.6 s at 2265.6 MHz, a
5.5-fold difference, so the cores really do run at the rate the files claim.

It needs these patches because nothing in mainline instantiates the Krait
clock controller: no ARM qcom device tree in 6.16 has a `krait-cc` node, a
CPU OPP table or a `cpu-supply`, so an unpatched MSM8974 runs at whatever
rate its bootloader left, which on this phone is 960 MHz.

## What is here

Four patches against 6.16.12 and one extra driver, as carried in
postmarketOS's `linux-postmarketos-qcom-msm8974` aport, where they are
patches 0010 to 0013 alongside a CPU thermal trip and L2 cache scaling.

**`0010-clk-qcom-hfpll-add-msm8974-data.patch`** — the HFPLL binding
documents `qcom,msm8974-hfpll` but the driver has no match entry for it, so
an msm8974 device tree can only use the deprecated compatible, which selects
qcs404's data and would write the wrong config value at the first rate
change. Values come from Sony's device tree and from Qualcomm's downstream
8974 clock driver; the config value reads back live from all five HFPLL
config registers on a running Z2.

**`0011-clk-qcom-krait-cc-fix-v2-aux-name.patch`** — a one-word bug.
`krait_cc_probe()` registers the shared aux clock as `acpu_aux` and
`krait_add_sec_mux()` asks for `apu_aux` on the same path, so the secondary
mux cannot resolve its second parent — which is the path the driver switches
to while reprogramming an HFPLL. Unreachable until now because no in-tree
device tree defines the controller at all.

**`0012-ARM-dts-qcom-msm8974-krait-cpufreq.patch`** — the device tree: five
HFPLLs, the Krait clock controller, the speed/PVS fuse cell, and for each CPU
its clock, supply and OPP table. The fuse decodes to speed bin 1, PVS 13, v1
on this unit, and bin 1's 2265.6 MHz maximum is the part's rating. The OPP
table is generated from Sony's factory tables into
`qcom-msm8974-krait-opp.dtsi`, using the `opp-microvolt-speed<S>-pvs<P>-v<V>`
names `qcom-cpufreq-nvmem` selects at runtime. Each entry is a
`<target target max>` triplet, because four CPUs share one regulator but have
independent policies, so exact single values from different cores would have
no voltage in common. Not `opp-shared`: each core has its own HFPLL and
primary mux.

**`0013-soc-qcom-spm-msm8974-l2-saw-cpu-supply.patch`** — the supply.

**`krait-l2-cache.c`** — L2 cache rate scaling, separate from the CPU OPPs,
with **`0015-soc-qcom-add-krait-l2-cache-scaling.patch`** as its kernel patch
and **`krait-l2-node.dtsi-fragment`** plus
**`0016-ARM-dts-qcom-msm8974-krait-l2-scaling.patch`** for the device tree
side. The L2 voltage corners come from Sony's `qcom,l2-fmax`: up to 576 MHz at
SVS_SOC, 1036.8 MHz at NORMAL, 1728 MHz at SUPER_TURBO.

**`0014-ARM-dts-qcom-msm8974-cpu-trip-80C.patch`** — the CPU thermal trip.
80 °C was chosen here independently, and Sony's `qcom,msm-thermal` node picks
the same figure. What Sony also does and this does not is act on it: throttle
to 422.4 MHz on cores 0 and 3, and take cores 1 and 2 offline at 85 °C. See
`../../docs/whats-left.md`.

**`qcom-msm8974-krait-opp.dtsi`** — the OPP table itself, and
**`gen-krait-opp.py`**, which generates it from the stock device tree on the
device. The table is not hand-written and should not be hand-edited;
regenerate it with a different `--cap-hz` instead.

Its values have been checked against Sony's published kernel, which is an
independent copy of the same data: **1,535 frequency/voltage pairs across 55
tables, with no difference**, including every table carrying the 2265.6 MHz
entry this phone runs at. That is the part where an undervolt would matter and
would not show up immediately. `../../docs/sony-source-audit.md` has the
method.

## The supply is not what the state table used to say

It is worth being explicit, because this repository previously said the
remaining work needed "a driver for the Krait per-core regulators on PM8841
that mainline does not have". That is wrong twice over.

The four cores do not have per-core regulators. They **share one supply**:
ganged PM8841 FTS phases, reached over SPMI by the L2 SAW2, which Qualcomm's
downstream tree marks as the APCS master. Voltage, phase count and PWM/PFM
mode all go through that SAW's VCTL register.

And a driver for it is written, in patch 0013. Mainline has the SPM regulator
framework but only APQ8064's SAW2 v1.1 implements `set_vdd`, the msm8974 L2
compatible has no match entry, and `spm_get_cpu()` rejects any SAW that no
CPU references. The patch adds the msm8974 L2 SAW as a regulator, with
register offsets for SAW2 v2.1 from msm-3.10, the configuration Sony's device
tree writes, and the voltage encoding confirmed against the level echoed back
in `PMIC_STS`.

So the voltage path is neither missing nor per-core. And it works: the cores
would not reach 2265.6 MHz without it, since every operating point above
960 MHz needs more than the boot voltage.

## The frequency cap

The patch in this directory is the first version, which capped the table at
960 MHz with `status = "disabled"` on everything above, so the supply path
could be proven before anything ran faster. That cap has since been lifted:
the phone offers all 28 operating points from 300 MHz to 2265.6 MHz. If you
apply the patch as it stands you will get the capped table, and raising it
means removing the `status` property from the operating points you want.

The patch header refers to `cpufreq/gen-krait-opp.py --cap-hz` for
regenerating the table; that script is not in this repository and was not
found on the device.

## Checking it on a device

None of this is a loadable module, so it needs a kernel built with:

    CONFIG_KRAITCC=y  QCOM_HFPLL=y  ARM_QCOM_CPUFREQ_NVMEM=y
    CONFIG_CPUFREQ_DT=y  QCOM_SPM=y  KRAIT_CLOCKS=y

and a device tree carrying the nodes. Note that a boot image assembled from a
separately built DTB will not have them unless that DTB was built from a
patched tree, which is an easy way to have the drivers present and the
scaling absent.

To check:

    cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies
    cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq

It should sit near the bottom at idle and reach the top under load. Verify
the rate against a timed workload rather than trusting the file: the same
loop should take several times longer pinned low than it does at the top.
