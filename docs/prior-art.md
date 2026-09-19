# Prior art: look here before writing anything

This page exists because of a mistake. Four subsystems on this phone were
assessed by checking whether a driver was in mainline, finding it absent, and
concluding it would have to be written. For two of them that was wrong, and a
single search would have shown it. "Not in mainline" answers whether something
is upstream. It does not answer whether it exists.

Check these before estimating any piece of work on this hardware.

## msm8974-mainline

<https://github.com/msm8974-mainline/linux>

A kernel fork for MSM8x74 devices with a branch per kernel version, currently
up to `qcom-msm8974-6.16.y` — the same version this phone runs. Also
`qcom-msm8974-next-stable`. Several older WIP branches carry work that never
made it forward, and they are where the useful surprises are.

What it already contains that matters here:

- **`arch/arm/boot/dts/qcom/qcom-msm8974-sony-xperia-sirius.dts`** on the 6.16
  branch. A maintained Xperia Z2 device tree already exists. It is a
  standalone tree including `qcom-msm8974pro.dtsi` directly rather than
  building on `shinano-common.dtsi`, and it covers less than this project's
  tree does — camera buttons and charging, but no audio, sensors or NFC. It
  also declares the panel as `sharp,ls052t3sx02`, which matches none of the six
  variants this device actually ships; see `panel-identification.md`.
- **A complete WCD9320 codec driver** on `old-4.18.0/qcom-audio-wip`, about
  7,100 lines across five files. A second one, easier to forward-port because
  it is four years newer, is on `flto-msm8974-5.11` in `z3ntu/linux`; that is
  the one this project uses. See `../drivers/audio/wcd9320/`.
- **`qcom-msm8974-5.6.y-sirius-nfc`**, a branch whose single commit adds NFC to
  the Z2. See `nfc.md`.
- `old-4.18.0/qcom-tfa-audio-wip`, TFA amplifier work, the same amplifiers this
  phone uses.

## msm8974-mainline/linux-panel-drivers

<https://github.com/msm8974-mainline/linux-panel-drivers>

Panel configurations for the fork above, built with
`linux-mdss-dsi-panel-driver-generator` — a tool that generates a DRM panel
driver from a downstream MDSS DSI panel device tree node. Given that this
project has extracted all six of the Z2's panel nodes into `panel-variants/`,
that generator is the obvious route to drivers for the five variants that
cannot be tested here.

## Luca Weiss (z3ntu)

<https://github.com/z3ntu/linux-mainline-files>

Maintains `device-sony-sirius` in pmaports, and keeps a component support
table for the Fairphone 2, another msm8974 device. That table is the fastest
way to find out whether something is possible on this SoC at all. Two things
read off it directly:

- **The modem works on msm8974 mainline, since v5.6.** So the stall documented
  in `modem.md` is a device-specific problem, not a missing driver. Firmware
  and memory regions have since been ruled out.
- **FM on the WCN3680 is "No driver"** for someone who has been working on this
  SoC for years. That corroborates FM being genuinely unwritten rather than
  merely hard to find.
- Cameras are listed as "Working (WIP), out-of-tree". That work is real and
  was eventually found, in his own patch tree rather than the shared fork —
  see below.

His own patch tree, `z3ntu/linux`, has 60 branches and is where the msm8974
camera work turned out to be: **`flto-msm8974-5.17-camera`**, covering CAMSS
and CCI device tree nodes, a CCI driver hack, Jonathan Marek's camss patch and
a hand-written IMX179 driver, all for the Nexus 5. See `camera.md`. This is
the branch that took three attempts to find, and the reason this page exists.

Also `z3ntu/msm-mainline-status`, a Qualcomm mainline status tracker, and
`z3ntu/linux-mdss-dsi-panel-driver-generator`, the generator mentioned above.

## Sony's own Open Devices sources — not yet used, and should be

<https://github.com/sonyxperiadev/kernel> and
<https://github.com/sonyxperiadev/device-sony-sirius>

Sony runs an Open Devices programme and publishes kernel sources and AOSP
device configurations for its unlocked Xperias, sirius among them. This
project has used the stock firmware *on the device* — decompiled device
trees, tuning files, blobs — but not Sony's published source, and that is a
gap rather than a decision.

It was read on 2026-09-19. What it does and does not contain is now known,
and the answer for the cameras is half of what this page used to claim.

**The camera sensors: power sequences yes, register sequences no.** The Z2's
camera driver is `sony_camera_v4l2.c`, and it is on branch
`aosp/LNX.LA.3.5.1-01110-8x74.0` — not on the newest 8x74 branch, which drops
it. It is a power-sequencing and I2C-passthrough driver: rails, GPIOs, clock,
one `I2C_WRITE` for streaming off, and nothing else. Its
`struct sony_camera_seq` has exactly nine commands, none of which programs a
mode. The generic Qualcomm sensor drivers in the same tree are no different —
`imx135.c` is 236 lines and is all power sequencing.

That is how this generation of Qualcomm camera software worked: sensor mode
programming lived in the userspace HAL, which sent register arrays down
through ioctls. **No Qualcomm-era kernel contains IMX200 or IMX132 mode
tables, and Sony's does not either.** They have to come from the stock camera
HAL on the device; see `extracting-from-stock.md`.

What the published source *is* authoritative for, and what was taken from it:

- **The camera power sequences**, exactly: rail order, voltages, load
  currents and per-step delays for both sensors, in
  `arch/arm/boot/dts/msm8974pro-ab-shinano_sirius_common.dtsi`. In
  `camera.md`.
- **The ISP hardware map** — CSIPHY, CSID, ISPIF, VFE addresses, sizes and
  interrupt numbers, in `msm8974-camera.dtsi`. It agrees exactly with the
  table already derived from the stock device tree on the device, which makes
  three independent derivations.
- **The autofocus actuator**, a Rohm BU64296G, in the actuator directory.
- **The touchscreen.** The MAX1187x driver this port runs came from Sony
  downstream; the published source is its origin.
- **The WCD9320 codec**, for comparison against the forward-port here.

Worth checking before writing any new driver for this phone — and worth
checking *which branch*, because the sirius-specific files are not all on the
newest one.

## The stock camera stack, and why there is no table to lift

Read on 2026-09-19, because the register sequences had to be somewhere and it
was not the kernel. The stock Android 6.0.1 system partition is still intact
on the test phone — see `extracting-from-stock.md` for how to mount it.

**`/system/lib/libcammw.so` is Sony's sensor driver.** 253 KB, ARM 32-bit,
stripped, and it exports exactly what the name suggests:

    imx132_get_sensor_param        imx200_get_sensor_param
    imx134_get_sensor_param        imx135_get_sensor_param
    sony_camera_get_sensor_driver  sony_camera_get_focus_driver
    sony_camera_get_eeprom_driver  sony_camera_platform_i2c_write
    cam_load_tables                sony_camera_platform_i2c_read

Nothing else in the stock system knows these sensors by name: `camera.qcom.so`,
`mm-qcamera-daemon` and `libmmcamera_interface.so` do not mention them, and
`libmmcamera2_sensor_modules.so` refers only to `sony_camera_0`, the kernel
subdev name.

**And it contains no register tables.** Its `.rodata` is 25 KB and 90%
printable strings; scanning for arrays of Sony IMX register addresses finds no
run longer than two entries in any plausible encoding. `imx200_get_sensor_param`
disassembles to arithmetic on a context structure, not a table lookup. The
`.dat` files in `/vendor/camera/` are tuning and module parameters — under 2%
of their 16-bit words look like sensor registers, and they are mostly zeros.

So Sony's HAL **computes** the register writes at run time from the module
parameters and the sensor's own state. There is no table to extract, and the
search should not be repeated.

What that leaves, and why it is not as bad as it sounds: these are ordinary
Sony IMX parts with the standard register map, and their power-on defaults can
simply be read over CCI. The first read confirms it — the IMX132 reports
`0x0340 = 0x04b0`, frame length 1200 lines, which is exactly the active height
Sony's device tree gives it. `camera.md` records what a full dump found.

## Where this project is ahead

Worth knowing in the other direction, because it is what is worth contributing
back rather than duplicating. Against the sirius tree in the fork, this project
has working speaker audio, five working sensors with verified mount matrices,
battery percentage, a panel driver that handles all six variants by runtime
detection rather than assuming one, and a device tree built on
`shinano-common.dtsi` in the shape mainline expects.
