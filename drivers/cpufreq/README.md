# CPU frequency scaling on the Xperia Z2

All four Krait cores run at a fixed 960 MHz of a rated 2265.6 MHz, because
nothing in mainline instantiates the Krait clock controller: no ARM qcom
device tree in 6.16 has a `krait-cc` node, a CPU OPP table or a `cpu-supply`,
so every MSM8974 runs at whatever rate its bootloader left.

That is two problems, not one, and they are worth separating because the
useful half is much easier than the other:

- **Clocking down when idle** is what gives battery life. Nothing currently
  drops below 960 MHz, so the phone burns the same power asleep as awake.
- **Clocking up** toward 2265.6 MHz needs more core voltage.

## What is here

Four patches against 6.16.12 and one extra driver. They compile clean; none
has been booted.

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

**`krait-l2-cache.c`** — L2 cache rate scaling, separate from the CPU OPPs
and not wired into the patches above.

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

So the voltage path is written rather than missing. It is untested, which is
a different thing.

## The frequency cap, and raising it

The generated table contains every operating point from 300 MHz to
2457.6 MHz, but everything above 960 MHz carries
`status = "disabled";	/* above the safe rail cap */`. 960 MHz is the rate the
cores already sustain, so the first version proves the supply path without
anything running faster than it does today.

Raising the cap means enabling those entries. The patch header refers to
`cpufreq/gen-krait-opp.py --cap-hz` for regenerating the table; **that script
is not in this repository and was not found on the device**, so for now the
way to raise the cap is to remove the `status` property from the operating
points wanted.

## Testing it

Needs a boot image and a flash; none of it is a loadable module.

    CONFIG_KRAITCC=y  QCOM_HFPLL=y  ARM_QCOM_CPUFREQ_NVMEM=y
    CONFIG_CPUFREQ_DT=y  QCOM_SPM=y  KRAIT_CLOCKS=y

Then, in order:

1. `/sys/devices/system/cpu/cpu0/cpufreq/` exists at all.
2. `scaling_available_frequencies` lists the points up to the cap.
3. `scaling_cur_freq` drops at idle — this is the battery result, and it is
   the one worth having even if nothing else follows.
4. It rises under load, and the core actually runs at the rate claimed:
   check against a timed workload rather than trusting the file.
5. Temperature under sustained load stays sane.

Only then is raising the cap worth trying, because everything above 960 MHz
depends on the supply patch doing what it says.
