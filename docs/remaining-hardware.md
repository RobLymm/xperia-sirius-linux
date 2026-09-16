# What is left, and what each piece actually costs

Six subsystems are unfinished. This is the evidence for each, so nobody starts
one with the wrong idea of its size.

An earlier version of this file was wrong about two of them. Both corrections
are in the relevant sections below, because the mistake is instructive: the
absence of something in mainline does not mean nobody has written it.

## Needs a device tree node and a test — NFC

The chip is an NXP PN547 at 0x28 on `blsp1_i2c6`. Mainline has the driver
(`drivers/nfc/nxp-nci/i2c.c`) and the binding names `nxp,pn547` explicitly.
GPIOs from Sony's tree: interrupt TLMM 24, firmware download TLMM 57, VEN on
PM8941 MPP 2.

Node, caveats and test procedure in `nfc.md`. One unknown: VEN polarity,
which is a one-line change to try both ways.

## Needs diagnosis on hardware

### Modem: boots, then stalls

Firmware is finished and packaged, and five hypotheses have been ruled out:
region size, missing segments, load addresses, rmtfs and the userspace stack.
The modem brings up its SMD transport and stops before registering services.
Next candidates are lk2nd and the Z2's own ADSP firmware. Detail in
`modem.md`.

### Suspend and resume: comes back without Wi-Fi or touch

Two devices fail independently, suggesting two causes. The MAX1187x touch
driver is out of tree and written against a downstream kernel — whether it has
`suspend`/`resume` callbacks at all is the first thing to check. Separately,
whether the brcmfmac SDIO card keeps power across suspend and is re-probed on
resume. Neither has been investigated; neither needs a new driver.

## Microphones

Headphone playback works. The codec that carries it, a WCD9320 on SLIMbus,
also carries all three microphones, and that half is untouched: no capture
links in the device tree, no ADC or decimator paths, no micbias. The earpiece
is not part of this at all — it is a separate TFA9890 amplifier on MI2S, like
the loudspeakers.

Nothing here needs a new driver. The pieces are:

- Capture links for `SLIMBUS_0_TX` in the codec device tree, alongside the
  playback link that is already there.
- The codec's ADC, decimator and micbias paths, which the driver already
  contains but which nothing has exercised.
- Sony's stock tree supplies the micbias and routing configuration: which
  physical microphone is on which AMIC, and which bias each one uses.

Jack detection is a separate gap. The codec's MBHC hardware is not driven,
and the driver's `set_jack` only stores the pointer. It needs the interrupt
block, which in turn needs `CONFIG_REGMAP_IRQ`.

## Camera — the ISP is nearly solved, the sensors are not

**Correction, twice over.** First described as needing camss support for a SoC
it does not cover. Then revised to "add a variant". Both were wrong in the
same way: nobody looked for existing work.

msm8974 camera exists, on the Nexus 5, in `z3ntu/linux` branch
`flto-msm8974-5.17-camera`. It binds to `qcom,msm8916-camss` — no new variant
was written — plus a ~150 line camss patch, a CCI hack and a hand-written
sensor driver. The addresses in their device tree node are identical to the
ones derived independently from Sony's tree here.

What remains for this phone is the sensors: Sony hides the parts behind
`sony_camera_0` and `sony_camera_1`, and mainline has no driver for a 2014
Sony flagship sensor. Full detail, the hardware table and the device tree node
in `camera.md`.

## FM radio — the tuner answers from userspace

The Z2's tuner is in the Broadcom BCM4335C0 Bluetooth chip and is driven over
HCI vendor command `0xFC15`. From userspace it powers on and tunes, verified on
the phone; reception is untested because it needs headphones as the aerial.
Audio routing is the open question. Detail in `fm-broadcom.md`; tool in
`../tools/bcm-fm.sh`.

An earlier version of this section described a WCNSS driver. The Z2 has no
WCNSS; that driver is for other msm8974 phones.

## Order worth doing them in

1. **NFC** — a node and a flash.
2. **Suspend** — investigate two drivers.
3. **Microphones** — capture links and the codec's ADC paths.
4. **Camera** — forward-port existing msm8974 camss and CCI work, then
   identify and drive the two sensors.
