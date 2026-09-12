# Sony Xperia Z2 panel driver

`panel-sony-sirius.c` is a DRM panel driver for all six panel configurations
Sony shipped on this device: two driver ICs (Renesas, Novatek) by three glass
vendors (Sharp, JDI, AUO).

A hardcoded panel compatible is wrong for most of these phones, because which
panel you have is a property of the individual unit, not the model. The driver
picks at runtime from the `lcdid_adc` value the bootloader passes on the
kernel command line.

**The units trap:** `lcdid_adc` is in ADC units, and Sony's selection windows
are in microvolts, three times larger. Comparing them directly gives a
confident wrong answer — a JDI-on-Novatek unit matches the JDI-on-Renesas
window. Multiply by three first. This mistake was made here and is easy to
repeat; see `../../docs/panel-identification.md`.

The per-variant data — timings, init sequences, power sequences and Sony's
colour calibration tables — is in `../../panel-variants/`, extracted from the
stock kernel's device tree, with a comparison of what differs between them.
The short version: the glass vendor determines the display mode, the driver IC
determines the init sequence.

## Before this can go upstream

- A DT binding document. There is none yet, and no patch will be accepted
  without one.
- Mainline prefers one driver per controller IC, with the panel identified by
  compatible string, rather than one driver covering six variants. How to
  reconcile that with runtime selection by ADC value is a question worth
  asking on dri-devel before writing the patch rather than after.
- `Signed-off-by:` lines. The patch in this directory has none.
