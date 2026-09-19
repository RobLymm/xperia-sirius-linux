# Auditing this port against Sony's published source

Sony runs an Open Devices programme and publishes the kernel its Xperias
shipped with. For this phone that source is the authority on values, sequences
and orderings: it is what the hardware was built and tested against. It is
**not** a model for structure — it is 3.4-era vendor code and the drivers here
target 6.16 — so every entry asks "is our number, order or timing the same as
Sony's, and if not, why", never "should we look more like this".

`prior-art.md` says how to read the source and which branch to read. Every
finding below cites a file, a branch and a commit, so it can be re-checked:

    tools/sony-src.sh drivers/misc/pn547.c

Outstanding work that comes out of these findings is in `whats-left.md`;
confirmed faults are in `known-problems.md`. This file is the evidence.

## Status of each finding

| | |
|---|---|
| **Confirmed on device** | reproduced by a measurement, before and after |
| **From source** | read in Sony's tree, not yet tested here |
| **Agrees** | checked and the same — recorded so it is not re-checked or "fixed" |

---

## NFC — the PN547

Audited 2026-09-19 against `drivers/misc/pn547.c` and
`arch/arm/boot/dts/msm8974pro-ab-shinano_sirius.dtsi`, branch
`aosp/LNX.LA.3.5.2.2-03010-8x74.0` at `ba9f9c5d`. Ours is the proposed node in
`nfc.md`, which has never been flashed.

### VEN polarity is the opposite of what our node says — From source

`nfc.md` records this as "the one thing to verify", and Sony's driver settles
it. The device tree says `nxp,ven = <&pm8941_mpps 2 0x01>`, and `0x01` is
`OF_GPIO_ACTIVE_LOW`. The driver reads that flag into `ven_gpio_flags` and then
**never uses it**: every access goes through the legacy `gpio_set_value()` and
`gpio_set_value_cansleep()`, which are raw and apply no polarity.

    gpio_direction_output(pn547_dev->ven_gpio, 0);   /* probe: held in reset */
    ...
    gpio_set_value(pn547_dev->ven_gpio, 1);          /* enable, then scan I2C */
    usleep_range(10000, 11000);
    for (addr = 0x28; addr < 0x2C; addr++) { ... }
    gpio_set_value(pn547_dev->ven_gpio, 0);          /* back to reset */

and the same in the `PN547_SET_PWR` ioctl: `1` to power on, `0` to power off.
So electrically **the line is driven high to enable the chip**, which is the
usual arrangement for a PN547 and the opposite of what the active-low flag
suggests. The flag is dead code.

Our node has `enable-gpios = <&pm8941_mpps 2 GPIO_ACTIVE_LOW>`, taken from
Sony's third cell. Under the gpiod API that inverts the line, so asserting the
descriptor would drive it low and hold the chip in reset.

**Change** `GPIO_ACTIVE_LOW` to `GPIO_ACTIVE_HIGH` before flashing. `nfc.md`
already says to try the inversion first if no chip appears; this says which way
round to start, and why.

### PVDD has no home in our node at all — From source

Sony's node has `nxp,pvdd_en = <&pm8941_gpios 34 0x01>`, and the driver
requests that GPIO. Our proposed node drops it, because the mainline
`nxp,nci.yaml` binding has no property for it — the binding has `enable-gpios`
and `firmware-gpios` and nothing else.

Dropping it leaves the chip with no supply. Sony's base configuration makes
this worse rather than better: in the family file
`msm8974pro-ab-shinano_common.dtsi`, PM8941 **GPIO_33 and GPIO_34 are both
commented `NC` and set `qcom,master-en = <0>`** — disabled. Only GPIO_35,
`NFC_CLK_REQ`, is enabled there. The pins are brought up per-variant.

**Write it as a regulator**, which is the mainline-shaped answer and gives the
node something to reference:

```dts
nfc_pvdd: regulator-nfc-pvdd {
	compatible = "regulator-fixed";
	regulator-name = "nfc_pvdd";
	gpio = <&pm8941_gpios 34 GPIO_ACTIVE_HIGH>;
	enable-active-high;
	regulator-always-on;
};
```

Polarity here is a guess and should be checked the same way VEN was, by reading
what drives the line rather than what the flag says.

### There are two board revisions, wired differently — From source

`dynamic_config` in Sony's node sends probe through
`board_nfc_hw_lag_check()`, and `configure_gpio = <&pm8941_gpios 33 ...>` and
`configure_mpp = <&pm8941_mpps 2 ...>` are the alternate wiring it selects.
So some Z2 hardware revisions drive NFC differently, and a node that works on
one unit is not proof for all of them. Worth knowing before this is published
as working.

---

## Camera ISP — the msm8974 CAMSS resource tables

Audited 2026-09-19 against `msm8974-camera.dtsi` at `ba9f9c5d` and
`sony_camera_v4l2.c` at `38838c6e`, against
`drivers/camera/0002-ARM-dts-qcom-msm8974-add-the-camss-node.patch`.

### Every address and every interrupt agrees — Agrees

A third independent derivation, after the stock device tree on the phone and
the Nexus 5 patch. Ten interrupts and fourteen register ranges, all identical:

| Block | Address | Size | IRQ |
|---|---|---|---|
| CSIPHY 0/1/2 | `fda0ac00` / `fda0b000` / `fda0b400` | `0x200` | 78 / 79 / 80 |
| CSIPHY clock mux 0/1/2 | `fda00030` / `fda00038` / `fda00040` | `0x4` | — |
| CSID 0-3 | `fda08000` / `08400` / `08800` / `08c00` | `0x100` | 51 / 52 / 53 / 54 |
| ISPIF | `fda0a000` | `0x500` | 55 |
| ISPIF CSI clock mux | `fda00020` | `0x10` | — |
| VFE 0/1 | `fda10000` / `fda14000` | `0x1000` | 57 / 58 |

Sony's compatibles corroborate the hardware-version choices the driver README
argues for from first principles: `qcom,ispif-v3.0` for the ISPIF and
`qcom,vfe40` for the VFEs.

Nothing to change. Recorded because the capture so far has only exercised VFE0
RDI0 at 1920x1080, so an error in the dual-VFE or PIX paths would still be
sitting unhit — and this says there is not one in the addresses.

### The 8 MHz sensor clock is not a contradiction — From source

`drivers/camera/README.md` records, as unexplained, that Sony's driver sets an
8 MHz sensor MCLK while this board supplies 19.2 MHz and the IMX132 streams at
19.2. Sony's driver explains it:

```c
#define SENSOR_MCLK_DEFAULT	8000000
...
if (value == 0)
	cam_clk_info[0].clk_rate = SENSOR_MCLK_DEFAULT;
else
	cam_clk_info[0].clk_rate = (long)value;
```

The Sirius power sequence passes `CAM_CLK` with a value of 0, so 8 MHz is the
driver's fallback constant taken because the device tree asked for the default —
not a measured requirement of the board. Stock did run the sensors at 8 MHz;
this port runs them at 19.2 MHz with Intel's PLL pair, and both are consistent
because the sensor's PLL is programmed to suit whatever clock arrives.

**This matters for the rear IMX200.** Any register or mode table recovered from
the stock camera HAL was computed for an 8 MHz MCLK, and D-PHY timings are only
valid at the rate they were derived for — which is exactly the trap the IMX132
fell into and took three attempts to escape. Take Sony's power sequence for the
IMX200; do not take Sony's clock with it without deciding which rate the
register table belongs to.

### The CCI bus timing override is not carried — From source

Sony overrides the bus timing for **both** CCI masters, in the Sirius file
rather than the family one, so it is specific to this phone:

```dts
qcom,cci@fda0C000 {
	qcom,cci-master0 { qcom,hw-thigh = <22>; qcom,hw-tlow = <33>; };
	qcom,cci-master1 { qcom,hw-thigh = <22>; qcom,hw-tlow = <33>; };
};
```

Our `&cci` node sets clocks and nothing else, and mainline's `i2c-qcom-cci`
carries its timing in per-variant tables in the driver rather than reading it
from the device tree, so this cannot simply be copied across.

Not urgent: the bus enumerates, both sensors answer, and the front camera
streams. Worth knowing because Sony thought the generic timing needed changing
for this board, and a marginal I2C bus is the kind of fault that appears later
as an occasional failed sensor read.

### The autofocus actuator has a part number and no driver — From source

A Rohm **BU64296G** at `0x0c` on the rear camera bus, in Sony's actuator
directory. It only matters once the IMX200 works, and it is the smallest of the
remaining camera parts. In `whats-left.md`.

---

## Touchscreen — the MAX1187x

Audited 2026-09-19 against `drivers/input/touchscreen/max1187x.c`, branch
`aosp/LNX.LA.3.5.2.2-03010-8x74.0` at `ba9f9c5d`, against
`drivers/touch/max1187x.c` here. Sony's file **is the origin of ours**, so this
is a true diff and not an analogy: 3,135 lines against 3,115, 32 hunks.

Most of the difference is the forward-port to 6.16 and is correct:
`regulator_set_optimum_mode` became `regulator_set_load`, `kzfree` became
`kfree_sensitive`, `probe()` lost its `id` argument, `__devinit` went away,
`bin_attribute` became `const`, and the regulator failure path returns
`-EPROBE_DEFER` rather than `-ENODEV`, which is an improvement on the original.
Two hunks are not.

### The reset GPIO is never claimed, and the chip can never be reset — Confirmed on device

Sony reads the reset line as a plain number:

```c
if (of_property_read_u32(devnode, "gpio_reset", &pdata->gpio_reset))
	dev_info(dev, "unused gpio_reset should be set to zero\n");
```

against `gpio_reset = <85>` in their device tree. The port here changed the
device tree to a normal GPIO specifier, `reset-gpio = <&tlmm 85 0x2>`, and the
parse to match — but asks for the wrong element:

```c
pdata->gpio_tirq  = of_get_named_gpio(devnode, "tirq-gpio", 0);
pdata->gpio_reset = of_get_named_gpio(devnode, "reset-gpio", 1);
```

`reset-gpio` holds one specifier, so index 1 does not exist and
`of_get_named_gpio()` returns `-ENOENT`. `gpio_reset` is a `u32`, so the error
becomes a very large positive number, which means every `if (ts->pdata->
gpio_reset)` guard in the driver passes and every operation on the line then
fails. `tirq-gpio`, on index 0, is correct — which is why touch works at all.

Confirmed in the boot journal on the device:

    max1187x 0-0048: (INIT): chip init OK
    max1187x 0-0048: GPIO request failed for gpio reset (-2)
    max1187x 0-0048: (INIT): Input touch device OK

Probe continues, because `max1187x_gpio_init()` only warns. **The reset line is
never driven for the life of the driver.**

This is the explanation for the touchscreen **resume** fault in
`known-problems.md` — not necessarily for every report of a dead touchscreen,
since that file also records a GEM shrinker crash that wedges the compositor
and produces an identical symptom without the touchscreen being involved. The
chain for the resume fault is short. Our device tree sets `enable_resume_por = <1>`, copied from
Sony, so `set_resume_mode()` takes the reset branch:

```c
if (ts->pdata->enable_resume_por) {
	disable_irq(ts->client->irq);
	reset_power(ts);
	enable_irq(ts->client->irq);
}
```

and `reset_power()` does nothing but toggle `gpio_reset` low then high. With an
invalid line, `gpio_direction_output()` fails on the first call and the
function returns an error having reset nothing. So on resume the controller is
never power-on-reset, and it stays in the sleep mode `set_suspend_mode()` put
it in — which is exactly the reported symptom: *"the chip answers I2C
(chip_id 0x78, fw 1.30.60, config_id 0x048D) but does not scan"*. It also
explains why wiring `set_resume_mode()` into resume was *necessary and not
sufficient*: the call now happens, and the work it does is a no-op.

**Fixed the same day**, index `1` to `0`, rebuilt and reloaded on the running
phone. The probe message is gone:

    max1187x 0-0048: (INIT): chip init OK
    max1187x 0-0048: (INIT): Input touch device OK      <- no GPIO failure
    max1187x 0-0048: (INIT): fw_ver (1.30.60) chip_id (0x78)

and the operation that used to fail now works. Writing to the `por` sysfs entry
runs `reset_power()` under the same semaphore the resume path uses. Before, it
reported `irq reset timeout`. After:

    # echo 1 > /sys/bus/i2c/devices/0-0048/por
    max1187x 0-0048: hw reset occured

with the interrupt count moving 1098 to 1100 across the reset — the chip
raising its reset interrupt, which is the handshake that releases `reset_sem`
and that had never run. The chip answers afterwards: `chip_id 0x78`,
`fw_ver 1.30.60`, `config_id 0x048D`.

So the reset path is repaired and verified. **What is not yet verified is
suspend and resume**, because waking this phone needs the power key and so
needs someone at it. The test is: suspend, wake on the power key, and check
that the touch interrupt count in `/proc/interrupts` moves when the screen is
touched. `set_resume_mode()` calls exactly the `reset_power()` that now works,
so the mechanism is in place; whether it is sufficient is the open question,
and it was "necessary and not sufficient" only because the call did nothing.

One thing to watch: `reset_l2h = <1>` means the line idles high and a reset
pulses it low. GPIO 85 read `out high` before the fix and still does, so the
driver now owns a line that was already in the right state.

### Double-tap to wake was removed from the driver and left in the device tree — From source

Sony's driver supports a wakeup gesture. Ours had it taken out, in four places:

| Sony | Ours |
|---|---|
| `#include <linux/input/evdt_helper.h>` | removed |
| `struct device_node *evdt_node` in `struct data` | removed |
| `report_wakeup_gesture()` | removed |
| the `MXM_RPT_ID_POWER_MODE` branch in `process_report()` | removed |
| `evdt_initialize()` + `device_init_wakeup(dev, 1)` at probe | removed |

The reason is visible in the first line: it depends on `evdt_helper`, Sony's
device-tree-driven event table, which was not ported.

**Our device tree still describes the gesture.** The touchscreen node carries
`wakeup_gesture_support = <1>`, `wakeup_gesture_timeout = <2000>` and a
`wakeup_gesture/double_tap` subtree that maps gesture `0x0102` to a `KEY_POWER`
press and release. None of it is read by anything. They are dead properties,
and they read as a working feature.

This matters more than a missing convenience. `known-problems.md` records that
**nothing but the power key can wake this phone** — not Wi-Fi, not an incoming
call, not the RTC. Double tap to wake is a wake source the hardware has, that
Sony shipped, that our device tree already describes, and that the driver drops
on the floor. It also means `device_init_wakeup()` is never called for the
touchscreen, so it is not registered as a wake source at all.

Restoring it does not need `evdt_helper`. That helper exists to run an
arbitrary table of events out of the device tree, and our tree describes
exactly one gesture producing one key. Reading `gesture_code` and emitting the
key directly is perhaps thirty lines, with no new dependency — or port
`evdt_helper.c` and keep Sony's binding intact, which is the better option if
the node is to stay as written.

### The supply name property is now dead — From source

Sony looks the regulator up by a name from the device tree:

```c
ts->vreg_touch_vdd = regulator_get(dev, ts->pdata->vdd_supply_name);
```

Ours hardcodes it:

```c
ts->vreg_touch_vdd = regulator_get(dev, "vdd");
```

That is consistent with our node using `vdd-supply`, and it works. But the node
also still carries `touch_vdd-supply_name = "vdd"`, which nothing reads any
more. Harmless, and worth deleting so the node stops describing a mechanism
that is gone.

---

## CPU frequency and voltage — the Krait OPP table

Audited 2026-09-19 against `msm8974.dtsi` and `msm8974pro.dtsi` at `ba9f9c5d`,
against `cpufreq/qcom-msm8974-krait-opp.dtsi`, which is generated by
`cpufreq/gen-krait-opp.py` from the stock device tree **on the device**. Sony's
published tree is therefore an independent copy of the same data, which is
exactly what a generated table needs.

### Every frequency and voltage matches — Agrees

Both trees hold the plan as `qcom,speedX-pvsY-bin-vZ` properties: one table per
speed bin, process bin and characterisation version, each a list of frequency,
microvolts and microamps.

| | Tables | Frequency/voltage pairs | Differences |
|---|---|---|---|
| `msm8974.dtsi` | 21 | 532 | **0** |
| `msm8974pro.dtsi` | 34 | 1003 | **0** |

**1,535 pairs across 55 tables, none of them different**, and no table Sony has
that our generated file lacks. All 34 Pro tables carry the 2265.6 MHz entry the
phone actually runs at, so the top of the range — the part where an undervolt
would matter and would not show up immediately — is covered rather than
inferred.

This is the audit that was most likely to find something and did not. The
generated table is Sony's plan, and `gen-krait-opp.py` reproduces it exactly.

The L2 voltage corners agree too: Sony's `qcom,l2-fmax` gives 576 MHz at
SVS_SOC, 1036.8 MHz at NORMAL and 1728 MHz at SUPER_TURBO, which is what
`cpufreq/krait-l2-node.dtsi-fragment` already cites in its own comment.

One thing worth recording and not acting on: some of Sony's tables run to
**2457.6 MHz**, above the 2265.6 MHz this phone uses. Those are other bins of
the part. The phone runs the frequency its own fuses select, and that is
correct.

---

## Speaker amplifiers — the two TFA9890s

Audited 2026-09-19 against `msm8974pro-ab-shinano_common.dtsi` at `ba9f9c5d`,
against `devicetree/qcom-msm8974pro-sony-xperia-shinano-sirius.dts`.

### The I2C addresses agree — Agrees

Sony has `tfa98xx_top@68` and `tfa98xx_bottom@6A` on `i2c@f9967000`; ours has
`amplifier@34` and `amplifier@35` on `blsp2_i2c5`. These are the same two
parts: Sony writes the 8-bit address including the read/write bit, mainline
writes the 7-bit address. `0x68 >> 1 = 0x34` and `0x6A >> 1 = 0x35`.

Recorded because the numbers look like a disagreement and are not. Do not
"fix" this.

### Sony calls them top and bottom; we call them left and right — From source

`0x34` is Sony's **top** and our `speaker_left`; `0x35` is Sony's **bottom**
and our `speaker_right`. The README already says the earpiece is the top
TFA9890, which is `0x34`.

Top-to-left is the right mapping for a phone rotated anticlockwise into
landscape, so this is probably correct, but it has never been checked by ear.
One stereo file with a known channel settles it.

---

## Voice — q6voice and the silent uplink

Audited 2026-09-19 against `sound/soc/msm/qdsp6v2/q6voice.c` (5,996 lines) and
`audio_acdb.c` at `ba9f9c5d`, against `drivers/audio/q6voice/` here.

### The topology choice already matches Sony — Agrees

When the calibration database supplies nothing, Sony falls back to:

```c
tx_id = get_voice_tx_topology();
if (tx_id == 0)
	tx_id = VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS;     /* 0x00010F71 */
rx_id = get_voice_rx_topology();
if (rx_id == 0)
	rx_id = VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT;     /* 0x00010F77 */
```

`q6cvp.c` here already defaults to exactly those two. Sony's header also
confirms what the topology scan in `todo.md` concluded by experiment:
`0x00010F77` is `RX_DEFAULT`, a receive topology, which is why the DSP rejects
it for transmit.

### Sony sends three commands between create and enable; we send none — From source

This is the difference. Sony's `voice_setup_vocproc()`:

```c
/* 3091 */ cvp_session_cmd.hdr.opcode = VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2;
...
/* 3139 */ voice_send_cvp_register_dev_cfg_cmd(v);
/* 3140 */ voice_send_cvp_register_cal_cmd(v);
/* 3141 */ voice_send_cvp_register_vol_cal_cmd(v);
...
/* 3228 */ cvp_enable_cmd.opcode = VSS_IVOCPROC_CMD_ENABLE;
```

Ours goes from `CREATE_FULL_CONTROL_SESSION_V2` to `TOPOLOGY_COMMIT` to
`ENABLE`, with none of the three in between. There is no `cal_mem_handle`, no
`VSS_IMEMORY_CMD_MAP_PHYSICAL`, and no ACDB.

**The first of the three is not tuning.**
`VSS_IVOCPROC_CMD_REGISTER_DEVICE_CONFIG` carries the vocproc's *device
configuration*, not its sound quality settings — and it comes out of the same
calibration memory as the rest:

```c
if (!common.cal_mem_handle) { ... ret = -EPERM; goto done; }
get_vocproc_dev_cfg_cal(&cal_block);
if (cal_block.cal_size == 0) { ... ret = -EPERM; goto done; }
```

### What this means for the README's conclusion — From source

`README.md` says of the voice driver that "it needs no calibration data, which
was the surprise". Sony's code says something narrower: a call **enables**
without calibration, because all three registrations bail out early when the
calibration memory is unmapped and **the caller ignores their return values**.
Nothing aborts. The session is created, the vocproc is enabled, and the call
connects.

So a call that connects, carries downlink audio, and transmits nothing is not
evidence that calibration is unnecessary. It is the exact behaviour Sony's own
driver produces when the calibration database is absent.

That reverses the priority in `todo.md`, which lists the ACDB work as the
expensive fallback and "four transmit topologies never tried for audio"
(`0x10F72`–`0x10F75`) as the cheap thing to try first. Sony never selects any
of those four except when `get_voice_tx_topology()` returns them from ACDB, and
a processing topology without the calibration it expects is what the comment
already in `q6cvp.c` suspected would not work. The cheap experiment is worth an
hour because it is an hour; it is not the likely answer.

**The likely answer is the calibration path**, and it is now well defined:
map memory with `VSS_IMEMORY_CMD_MAP_PHYSICAL`, then send the three
registrations in Sony's order before `ENABLE`. `audio_acdb.c` shows the
memory-map and block-fetch side in full, and the blocks themselves come off the
stock partition — `extracting-from-stock.md` covers getting at it.

---

## Codec — the microphone routing, and four routes Sony has that we do not

Audited 2026-09-19 against the `sound` node in
`msm8974pro-ab-shinano_sirius_common.dtsi` at `ba9f9c5d`, against the live
device tree on the phone (`/proc/device-tree/soc/sound@fe02f000/audio-routing`)
rather than against a source file, because the question was what the running
system does.

### The microphone routing already matches Sony — Agrees

Sony's routing, and ours, both say:

    AMIC1  <- MIC BIAS1 External <- Secondary Mic
    AMIC2  <- MIC BIAS2 External <- Headset Mic
    AMIC4  <- MIC BIAS1 External <- Handset Mic

including the detail that is easy to get wrong: **AMIC4 takes MIC BIAS1, not
MIC BIAS4**, so the handset and secondary microphones share a bias. Sony's
`taiko_codec` node agrees from the other direction — `micbias1-cfilt-sel` and
`micbias4-cfilt-sel` are both `0x0`.

This was worth checking because the secondary microphone reads nothing and
wrong routing was the cheap explanation. It is not the explanation. It also
means a shared bias cannot be the reason either: the handset microphone on the
same bias does work.

### The bias configuration — From source

**From source.** `taiko_codec` under `&slim_msm`:

| Property | Value |
|---|---|
| `qcom,cdc-micbias-ldoh-v` | `0x3` |
| `qcom,cdc-micbias-cfilt1-mv` | 2700 |
| `qcom,cdc-micbias-cfilt2-mv` | 2700 |
| `qcom,cdc-micbias-cfilt3-mv` | 2700 |
| `qcom,cdc-micbias1-cfilt-sel` | `0x0` |
| `qcom,cdc-micbias2-cfilt-sel` | `0x1` |
| `qcom,cdc-micbias3-cfilt-sel` | `0x2` |
| `qcom,cdc-micbias4-cfilt-sel` | `0x0` |

All three filters at 2700 mV, and each of the four biases assigned to a
filter. Read against the WCD9320 audit when it is done: the secondary
microphone is on AMIC1, whose bias takes cfilt 0, shared with AMIC4 — the
handset microphone that does work. Shared bias is worth knowing about before
concluding that a silent microphone is unpowered.

### Four routes are missing, and two of them supply every microphone — From source

Sony has these; the live tree does not:

    "RX_BIAS", "MCLK"
    "LDO_H",   "MCLK"
    "AMIC5",   "MCLK"
    "AMIC6",   "MCLK"
    "AMIC3", "MIC BIAS3 External" / "MIC BIAS3 External", "ANCLeft Headset Mic"
    "MIC BIAS2 External", "ANCRight Headset Mic"

The ANC pair is a feature nothing here uses yet. The `MCLK` pair is not a
feature: **`LDO_H` is the supply behind every microphone bias on this codec**,
and our own driver says so —

```c
{"MIC BIAS1 External", NULL, "LDO_H"},
{"MIC BIAS2 External", NULL, "LDO_H"},
...
```

In Sony's graph `LDO_H` is itself sourced from `MCLK`, so DAPM powers the clock
whenever any bias is brought up. In ours `LDO_H` has no source, and `MCLK`
sits in the graph with nothing attached to it. On the idle phone all three read
`Off  in 0 out 0`, which proves nothing by itself — everything is off when
nothing is capturing.

**Tested on the device the same day, and disproved.** The reasoning was that a
bias whose supply is not powered on demand would give a microphone that reads
nothing, and a supply up from a previous stream and down on the next would give
the alternating capture in `known-problems.md`. Neither survives the
measurement.

Four two-second captures from AMIC4 and four from AMIC1, with the DAPM widget
states read one second into each:

| | LDO_H | MIC BIAS1 External | capture rms |
|---|---|---|---|
| idle | Off | Off | — |
| AMIC4 try 1 | **On** | **On** | 2083 |
| AMIC4 try 2 | **On** | **On** | 0 |
| AMIC4 try 3 | **On** | **On** | 95 |
| AMIC4 try 4 | **On** | **On** | 0 |
| AMIC1 try 1-4 | **On** | **On** | 0, 0, 0, 0 (peaks 6, 0, 4, 0) |

`LDO_H` and `MIC BIAS1 External` come up for **every** capture, on both
microphones, despite `LDO_H` having no source route. DAPM powers it through the
codec's own `{"MIC BIAS1 External", NULL, "LDO_H"}` link, which does not need
Sony's board-level route to work. `MCLK` stays `Off` throughout and the bias
powers anyway, so that widget is not gating anything either.

So **adding those two routes would not have fixed either fault**, and the
device tree change they implied is not worth making. Recorded because a
plausible, cheap, wrong explanation is worth closing properly.

Two things the measurement did establish:

- **The alternating capture reproduced exactly**, on AMIC4: 2083, 0, 95, 0.
  Whatever alternates is below DAPM — the bias, the route and the widget states
  are identical on a good capture and a silent one, which is the same result
  the driver and SLIMbus logs gave in `known-problems.md`.
- **AMIC1 is silent with everything above it correct.** Its route matches
  Sony's, its bias is powered, its widget is on, and it returns peaks of 4 and 6
  against 2083 rms on AMIC4 in the same room a second earlier. That is not a
  quiet microphone, it is a path that carries nothing. The fault is below the
  routing: the ADC, the decimator, or the microphone itself. Sony's source has
  now been exhausted on this question — the routing was the only thing it had
  to say, and it agrees.

---

## Panel — the six variants, of which four are handled

Audited 2026-09-19 against `dsi-panel-sirius.dtsi` at `ba9f9c5d` (1,980 lines),
against `drivers/panel/panel-sony-sirius.c`.

### The four implemented ADC ranges are exact — Agrees

| Downstream name | Sony's `somc,lcd-id-adc` | Ours |
|---|---|---|
| sharp renesas 1080p video | 0 – 57000 | same |
| auo renesas 1080p video | 215000 – 256000 | same |
| jdi renesas 1080p video | 353000 – 414000 | same |
| jdi novatek 1080p video | 1087000 – 1231000 | same |

`somc,mul-channel-scaling = <3>` matches the 1:3 divider the driver applies to
`lcdid_adc`, and the driver header already cites the branch it came from.

### Where the variant identifier is read from — From source

**From source.** `msm8974pro-ab-shinano_common.dtsi` configures PM8941 MPP 6:

    /* MPP_6: LCD_ID_ADC */
    mpp@a500 {
    	qcom,mode = <4>;         /* AIN */
    	qcom,ain-route = <1>;    /* AMUX 6 */
    	qcom,master-en = <1>;
    };

The panel driver here selects between six variants from an `lcdid_adc=` value
on the kernel command line, put there by the bootloader. This is the hardware
it is read from: an analogue input on PM8941 MPP 6, routed to AMUX 6.

That matters for two reasons. It is an independent confirmation that the
variant really is identified by an ADC reading rather than by anything else,
and it is the route to reading the value in-kernel through
`qcom-spmi-vadc` instead of trusting a command line the bootloader writes.
Not a defect; a better foundation, if the command line ever proves unreliable.

### Two variants are not handled, and the README says they are — Confirmed

Sony has six. The driver has four. Missing:

    sharp novatek 1080p video    1236000 - 1395000
    auo novatek 1080p video      1420000 - 1594000

The driver is honest about it — its header says outright that those two "are
not included here", and the `enum sirius_variant` has four entries. A phone
whose ADC falls in either range gets:

```c
dev_err(dev, "lcdid_adc=%u (%u uV) matches no known panel\n", adc, uv);
return ERR_PTR(-ENODEV);
```

which is no display at all.

**The documentation is what is wrong.** `README.md` describes
`drivers/panel/` as "DRM panel driver, all six Z2 panel variants" and the state
table as "Six panel variants, selected at runtime, with a generated driver for
each". Six generated standalone drivers do exist, in `panel-variants/generated/`
— `panel-novatek-sharp.c` and `panel-novatek-auo.c` among them — but they are
not wired into the driver that runs, and runtime detection covers four.

This unit is JDI glass on a Novatek controller, which is in range, so the
phone this was developed on could never have shown the gap. Corrected in
`README.md`; the work of folding the other two in is in `whats-left.md`.

---

## MHL — a SiI8620, and mainline has a driver for it

**From source.** `msm8974pro-ab-shinano_common.dtsi` at `ba9f9c5d`:

```dts
sii8620@72 {
	compatible = "qcom,mhl-sii8620";
	reg = <0x72>;
	mhl-intr-gpio = <&msmgpio 64 0>;
	mhl-pwr-gpio  = <&msmgpio 23 0>;
	mhl-rst-gpio  = <&msmgpio 16 0>;
	mhl-switch-sel-1-gpio = <&msmgpio 10 0>;
	mhl-switch-sel-2-gpio = <&msmgpio 11 0>;
	mhl-fw-wake-gpio = <&msmgpio 31 0>;
	qcom,hdmi-tx-map = <&mdss_hdmi_tx>;
};
```

on the same I2C bus as the amplifiers, with `&pm8941_mvs2` commented
`VREG_HDMI`.

This identifies the part, which was the thing blocking the item. Mainline has
a **Silicon Image SiI8620 DRM bridge driver**, `sil-sii8620`, so video out over
the micro-USB port is a device tree and bridge-plumbing job rather than a new
driver. Still large — it has to attach to the DSI output — but no longer
unknown. In `whats-left.md`.

---

## Machine driver — the MI2S pins, and the LINEOUT question settled

Audited 2026-09-19 against `sound/soc/msm/msm8974.c` (3,460 lines) at
`ba9f9c5d`, against `drivers/audio/msm8974-sndcard.c` and the `&sound` node.

### The Quaternary MI2S pins agree — Agrees

Sony's machine driver names them directly:

```c
#define GPIO_QUAT_MI2S_SCK   58
#define GPIO_QUAT_MI2S_WS    59
#define GPIO_QUAT_MI2S_DATA0 60
#define GPIO_QUAT_MI2S_DATA1 61
```

Our `quat_mi2s_default` pinctrl state is `"gpio58", "gpio59", "gpio60",
"gpio61"` with function `qua_mi2s`. Identical. The amplifier I2C bus agrees
too: Sony's `i2c@f9967000` is mainline's `blsp2_i2c5`, where our two
`tfa989x` amplifiers sit.

### Sony's LINEOUT routing is not the speaker path — Agrees

`sirius_common.dtsi` routes `Ext Spk Bottom Pos/Neg` to `LINEOUT1`/`LINEOUT3`
and `Ext Spk Top Pos/Neg` to `LINEOUT2`/`LINEOUT4`, which looks like it
contradicts this port driving the amplifiers digitally over Quaternary MI2S.

It does not. Sony's machine driver carries **both** paths — the `Ext Spk`
widgets with their `msm_ext_spkramp_event` callback appear twice, in two
board-variant widget lists, alongside the Quaternary MI2S plumbing — and the
Shinano family tree declares the two `nxp,tfa98xx` amplifiers on I2C. The
LINEOUT routing is inherited from the Qualcomm MTP reference board that the
Sirius file overrides, and is left in place unused.

So the digital MI2S path used here is the right one, and there is no louder
analogue route being missed. That closes the question raised in `whats-left.md`
about which is real.

---

## Battery current — the hardware is already configured

Audited 2026-09-19 against `&pm8941_lsid0/iadc@3600` and `&pm8941_chg` at
`ba9f9c5d`, against the live device tree and sysfs on the phone.

### The sense resistor already matches Sony — Agrees

Sony sets `qcom,rsense = <10000000>`, which is nano-ohms in their driver
(`QPNP_IADC_INTERNAL_RSENSE_N_OHMS_FACTOR` is 10000000, and the internal
defaults are 7800000, 9000000 and 9700000) — so **10 mΩ**.

The live tree on the phone already carries
`qcom,external-resistor-micro-ohms = <10000>` on `adc@3600`, from mainline's
own `pm8941.dtsi`. Same 10 mΩ. Nothing to add.

### The current channels exist and nothing consumes them — Confirmed

`qcom_spmi_iadc` is loaded and bound, and the phone has:

    /sys/bus/iio/devices/iio:device5   fc4cf000.spmi:pm8941@0:adc@3600
      in_current0_raw  in_current0_scale
      in_current1_raw  in_current1_scale

while `/sys/class/power_supply/fuel-gauge/` offers `capacity`, `voltage_now`,
`status` and no `current_now`.

So the correction to `BACKLOG.md:510-518`, which stops for want of the sense
resistor value, is that the value was never missing. The ADC is present,
bound, and correctly scaled. What is missing is only the plumbing from the IIO
channel into the battery power supply. That is a much smaller job than it was
recorded as, and it needs no flash.

### The charging protection is still absent — From source

Unchanged from the reading in `whats-left.md`: Sony's `&pm8941_chg` carries a
thermal mitigation ladder `1600 1600 1100 900 700 500 300 200 100 0` mA, a
warm-battery limit of 4200 mV at 900 mA, a cool-battery limit of 4350 mV at
900 mA, `vbatweak` 3200 mV and a 512-minute timeout. Our `&smbb` node has the
four headline limits and none of these. How much of it mainline's `qcom_smbb`
implements has not been checked.

---

## Thermal — Sony's trip points, and one that already agrees

**From source.** `qcom,msm-thermal` in `msm8974pro-ab-shinano_common.dtsi`:

| Sony | Value | Ours |
|---|---|---|
| `qcom,limit-temp` | 80 °C | 80 °C CPU trip, in `0014-ARM-dts-qcom-msm8974-cpu-trip-80C.patch` — **agrees** |
| `qcom,core-limit-temp` | 85 °C | nothing |
| `qcom,freq-mitigation-value` | 422400 kHz | nothing |
| `qcom,core-control-mask` | `0x6` — cores 1 and 2 | nothing |
| `qcom,freq-mitigation-control-mask` | `0x09` — cores 0 and 3 | nothing |

The 80 °C figure was chosen here independently and Sony picked the same one,
which is a useful confirmation. What is missing is everything that happens
*after* the trip: Sony throttles to 422.4 MHz on cores 0 and 3, and takes
cores 1 and 2 offline at 85 °C. `todo.md` leaves "whether thermal trips are
sensible under sustained load" open; this is the vendor's answer.

---

## FM — Sony's tree confirms it is not Qualcomm's, and has a driver for what it is

**From source.** Two separate confirmations, on two branches.

On `ba9f9c5d`, `msm8974pro-ab-shinano_common.dtsi` deletes the entire Qualcomm
wireless connectivity subsystem:

    /delete-node/ qcom,iris-fm;
    /delete-node/ qcom,pronto@fb21b000;
    /delete-node/ qcom,wcnss-wlan@fb000000;
    /delete-node/ &smdtty_apps_fm;

`qcom,iris-fm` is the Qualcomm FM tuner. Sony deleted it because this phone
does not have one — Wi-Fi and Bluetooth are Broadcom here, and so is FM. That
settles a question `fm-broadcom.md` answered by experiment: the device tree
agrees. It also confirms that `drivers/fm/radio-wcnss-fm.c` in this repository
is for other msm8974 phones and can never work on this one, which its own
README already says.

On `aosp/LA.UM.5.5.r1` at `4bc2f4cd` there is a complete Broadcom FM stack:

    drivers/bluetooth/broadcom/v4l2_fm_driver/fmdrv_main.c
    drivers/bluetooth/broadcom/v4l2_fm_driver/fmdrv_v4l2.c
    drivers/bluetooth/broadcom/v4l2_fm_driver/fmdrv_rx.c
    drivers/bluetooth/broadcom/include/fm.h
    drivers/bluetooth/broadcom/line_discipline_driver/brcm_hci.c

This is the vendor implementation of the tuner this phone actually has, driven
over HCI exactly as `fm-broadcom.md` describes doing from userspace. It has not
been read yet. The reason to read it is the 41.6 Hz sign-bit corruption on the
I2S capture, which is currently repaired by the `fmrepair` ALSA plugin rather
than fixed: if Sony's stack sets a clock role or a PCM configuration the
userspace HCI sequence here does not, the cause goes away. In `whats-left.md`.

---

## Device tree sweep — what Sony configures that we do not

Audited 2026-09-19 against `msm8974pro-ab-shinano_sirius_common.dtsi` (846
lines), `..._sirius.dtsi` and `msm8974pro-ab-shinano_common.dtsi` (813 lines)
at `ba9f9c5d`, against
`devicetree/qcom-msm8974pro-sony-xperia-shinano-sirius.dts` (589 lines) and the
live tree on the phone.

The individual audits above cover most of it. This is what the node-by-node
pass adds, and the consolidated list for the single boot image.

### Things that agree and should not be re-examined

- **Panel power.** Sony enables the panel DCDC from PM8941 GPIO 20 with
  `somc,keep_high_at_init`. Mainline's `shinano-common.dtsi` already provides
  the equivalent: a `regulator-fixed` node, `lcd-dcdc-regulator`, named
  `vreg_vsp`, `enable-active-high`, on that GPIO — present in the live tree and
  referenced by the panel's `vsp-supply`.
- **Charger headline limits.** Already taken from Sony and documented in the
  board DTS comment: 4350 mV, 4400 mV safe, 1600 mA, 1600 mA safe, 4200 mV
  minimum input.
- **Backlight current limit.** Sony's WLED runs 2 strings at 20 mA; our
  `&pm8941_wled` sets `qcom,current-limit = <20>`.
- **Camera buttons.** Both stages, on PM8941 GPIOs 3 and 4, match.
- **Amplifier and MI2S wiring**, **Krait OPP tables**, **CAMSS addresses**,
  **panel ADC ranges**, **microphone routing** — each covered above, all
  agreeing.

### Things Sony has that we do not

Ordered by whether they need a flash.

**No flash — driver or userspace only:**

| | Where |
|---|---|
| Battery current: plumb the IIO channel into the power supply | audit above |
| Double tap to wake: restore the gesture path in the touch driver | audit above |
| Two panel variants: fold Sharp-Novatek and AUO-Novatek into the driver | audit above |
| CCI bus timing `hw-thigh = 22`, `hw-tlow = 33` — mainline holds this in driver tables, not the device tree | audit above |

**Needs the flash:**

| | Detail |
|---|---|
| **NFC node** | with `enable-gpios` **active high**, plus a `regulator-fixed` for PVDD on PM8941 GPIO 34 |
| **SIM tray detect** | `msmgpio 9`, `EV_SW`, wakeup, 10 ms debounce. Sony reports code 7 (`SW_JACK_PHYSICAL_INSERT`); the kernel has no SIM-tray code, so what to report needs deciding |
| **Camera flash and torch** | `somc,leds@d300` and the `FLASH_LED_NOW` strobe pin, PM8941 GPIO 27 alternate function 1 — needs a driver too |
| **Vibrator drive level** | 2900 mV |
| **Notification LED maxima** | red 200 on PWM 6, green 511 on PWM 5, blue 341 on PWM 4, 12 mA, 1000 µs, `rgb_sync` |
| **Thermal mitigation** | throttle to 422.4 MHz on cores 0 and 3 at 80 °C, cores 1 and 2 offline at 85 °C |
| **Charging protection** | thermal ladder, warm and cool battery limits, weak-battery threshold, charge timeout — subject to what `qcom_smbb` implements |
| **ANC microphone routes** | `AMIC3` to `MIC BIAS3 External` to `ANCLeft Headset Mic`, and `MIC BIAS2 External` to `ANCRight Headset Mic` |
| **MHL** | the SiI8620 node, if the bridge work is taken on |

### Things Sony has that are deliberately not wanted

- The `Ext Spk` LINEOUT routing — inherited from the Qualcomm reference board
  and unused, see the machine driver audit.
- `qcom,msm-pcm-bit` and `qcom,msm-pcm-dsee`, Sony's hi-res and upscaling DSP
  paths.
- The `sony,vj180` ISDB-T tuner, which is on the Japanese Samba variant only.
- The 100 MB ION camera heap — this port uses CMA.
