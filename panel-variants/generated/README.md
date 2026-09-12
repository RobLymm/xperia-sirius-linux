# Generated DRM panel drivers for all six variants

A DRM panel driver for each of the six panels Sony shipped on the Z2,
generated from the init sequences in `../`. Five of them are for panels
nobody here owns, which is the point: an owner of a Sharp or AUO Z2 has no
way to extract this data from a phone that will not turn its screen on.

| driver | glass | driver IC | lines |
|---|---|---|---|
| panel-novatek-jdi.c | JDI | Novatek | 657 |
| panel-novatek-sharp.c | Sharp | Novatek | 644 |
| panel-novatek-auo.c | AUO | Novatek | 286 |
| panel-renesas-jdi.c | JDI | Renesas | 271 |
| panel-renesas-sharp.c | Sharp | Renesas | 262 |
| panel-renesas-auo.c | AUO | Renesas | 252 |

Each has a matching `.dtsi` with the panel node.

## Why these are worth trusting

Only one of the six can be tested here, and it is the one that provides the
evidence. `panel-novatek-jdi.c` was generated from the same extracted data as
the hand-written, hardware-verified `../../drivers/panel/panel-sony-sirius.c`,
and the init sequences agree byte for byte:

    generated            hand-written (works on this phone)
    0xff, 0x01           0xff, 0x01
    0x75, 0x00           0x75, 0x00
    0x76, 0x30           0x76, 0x30
    0x77, 0x00           0x77, 0x00
    0x78, 0x43           0x78, 0x43
    0x79, 0x00           0x79, 0x00
    0x7a, 0x61           0x7a, 0x61
    0x7b, 0x00           0x7b, 0x00

The tool produces correct output for the variant that can be checked against
working hardware. The other five come from the same extraction and the same
tool, so the failure modes left are in the data rather than the process.

They have still never been run. Nobody here has the hardware.

## State of the code

checkpatch: no errors, one or two warnings each, all the same one — the
generator emits its own `dsi_generic_write_seq` macro and checkpatch objects
to flow control inside macros.

They build against a current API: `drm_panel_init()` with a connector type,
`drm_panel_of_backlight()`, `dev_err_probe()`, `mipi_dsi_attach()`. Verified
against 6.16.12 sources.

Before upstreaming, the one real modernisation is to drop the local macro and
use the core's `mipi_dsi_generic_write_seq_multi()` and the `_multi` error
accumulation pattern, which is what `panel-sony-sirius.c` already does. That
is mechanical.

Also needed: a real compatible string per panel and a binding document.
Generated drivers use placeholder names.

## A caution specific to this device

A driver per panel is not enough on its own. Which panel a given Z2 has is a
property of the individual unit, not the model, and the only way to tell is
the `lcdid_adc` value the bootloader passes — remembering that it is in ADC
units while Sony's selection windows are in microvolts, three times larger.
See `../README.md`.

So either the six drivers coexist and something picks between them at
runtime, as `panel-sony-sirius.c` does internally, or each is selected by a
device tree that was built for one known unit. The second only works if you
already know which panel you have.

## Reproducing

    curl -sSL -o gen.tar.gz https://codeload.github.com/z3ntu/linux-mdss-dsi-panel-driver-generator/tar.gz/refs/heads/master
    mkdir gen && tar xzf gen.tar.gz -C gen --strip-components=1
    python3 -m venv venv && ./venv/bin/pip install pylibfdt fdt

    # the generator needs a DTB; ../../docs/extracting-from-stock.md has the
    # extraction, or convert the decompiled source back:
    ./venv/bin/python -c "import fdt; open('fota0.dtb','wb').write(fdt.parse_dts(open('fota-dtb0.dts').read()).to_dtb(version=17))"

    ./venv/bin/python gen/lmdpdg.py -r vddio fota0.dtb

It writes one directory per panel found, each containing the driver, a
`panel-simple-` variant and the device tree node.
