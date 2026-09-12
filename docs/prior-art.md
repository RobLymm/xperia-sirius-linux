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
  7,100 lines across five files. See `remaining-hardware.md`.
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
  in `modem.md` is a device-specific problem — firmware or memory regions —
  not a missing driver.
- **FM on the WCN3680 is "No driver"** for someone who has been working on this
  SoC for years. That corroborates FM being genuinely unwritten rather than
  merely hard to find.
- Cameras are listed as "Working (WIP), out-of-tree". The msm8974 camss support
  that implies is not in the 6.16 branch of the fork above, so it is somewhere
  else and has not been located yet. Worth finding before anyone starts.

His own patch tree, `z3ntu/linux`, has 60 branches and is where the msm8974
camera work turned out to be: **`flto-msm8974-5.17-camera`**, covering CAMSS
and CCI device tree nodes, a CCI driver hack, Jonathan Marek's camss patch and
a hand-written IMX179 driver, all for the Nexus 5. See `camera.md`. This is
the branch that took three attempts to find, and the reason this page exists.

Also `z3ntu/msm-mainline-status`, a Qualcomm mainline status tracker, and
`z3ntu/linux-mdss-dsi-panel-driver-generator`, the generator mentioned above.

## Where this project is ahead

Worth knowing in the other direction, because it is what is worth contributing
back rather than duplicating. Against the sirius tree in the fork, this project
has working speaker audio, five working sensors with verified mount matrices,
battery percentage, a panel driver that handles all six variants by runtime
detection rather than assuming one, and a device tree built on
`shinano-common.dtsi` in the shape mainline expects.
