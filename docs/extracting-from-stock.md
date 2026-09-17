# Taking what you need off your own phone

This is the background. For the step by step route from a stock phone, see
[from-stock-to-this.md](from-stock-to-this.md), and for the script that does
the extraction, `tools/extract-from-stock.sh`.


Several things this port depends on are proprietary and cannot be
redistributed: the modem, ADSP and GPU firmware. They are on your device
already. This describes how to get at them, and at Sony's own device tree,
which is the single most useful reference for anything not yet working.

Do this before wiping the stock ROM if you can. If the stock system partition
is already gone, the firmware is gone with it, and the phone's own NV storage
partitions are the only thing left that is irreplaceable.

## Sony's device tree, from the FOTA kernel

The FOTAKernel partition holds an ELF with several device trees embedded in
it. One of them matches your board.

    dd if=/dev/mmcblk0p16 of=fota.elf        # FOTAKernel on this device
    # find the embedded DTBs by their d00dfeed magic, carve each one out, then
    dtc -I dtb -O dts -o fota-dtb0.dts fota-dtb0.dtb

Identify the right one by its model string and board id. For the Z2 the match
is model "SoMC Sirius ROW", `qcom,board-id = <0x08 0x00>`.

Check the partition number against your own device rather than trusting
`p16`; `ls -l /dev/disk/by-partlabel/` is the reliable way.

What that file is good for:

- **Panel data.** All six panel variants with timings, init sequences, power
  sequences and colour calibration. Extracted copies are in
  `../panel-variants/`.
- **Audio.** The complete downstream audio tree: codec configuration, routing,
  MI2S pin functions, amplifier addresses.
- **Charging.** The values in the upstream device tree here came from this
  file: `qcom,vddmax-mv`, `vddsafe`, `vinmin`, `ibatmax`, `ibatterm`,
  `vbatdet-delta`, `maxinput-usb-ma`.
- **The modem node**, which is where the remaining modem problem probably
  lies.

## Firmware

From the stock system partition, mounted read-only:

- `modem.*` and `mba.*` — the modem. The Xperia Z3 set that postmarketOS
  ships is rejected by a Z2; you need your own.
- ADSP firmware. The Z3 set appears to work, but the Z2's own is worth having.
- Adreno a330 firmware is packaged by distributions and does not need
  extracting.

Keep a backup somewhere off the phone before you flash anything.

## The modem's NV storage

Three partitions hold the modem's calibration and settings. Copy them out and
keep them:

    modemst1  ->  modem_fs1
    modemst2  ->  modem_fs2
    fsg       ->  modem_fsg

`rmtfs` serves these to the modem at runtime. Without them the modem starts
with no usable state. These are specific to your handset and cannot be
replaced from anywhere else.

## Sensor orientation

The stock system also carries `/etc/sensor_def_qcomdev.conf`, which lists every
sensor as a plain I2C device with its address, interrupt GPIO and axis
orientation.

Treat the orientation values as a hint, not an answer. The mapping was derived
from that file here and was wrong twice: once from misreading the convention,
and once because the sign depends on how the panel is mounted, which the
accelerometer cannot tell you. Deriving the matrix empirically and then
checking it against the screen is the only reliable route.

## What not to publish

Do not commit any of the firmware binaries to a public repository. They are
Sony's and Qualcomm's, they are not redistributable, and a repository that
carries them is a repository that gets taken down. Document the extraction
instead, which is what this file is.
