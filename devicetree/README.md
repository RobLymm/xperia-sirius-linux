# The device tree that runs

`sirius-working.dts` is the tree the phone actually boots. It is a decompiled
Xperia Z3 (leo) device tree with the Z2's differences added, which is why it
uses numeric phandles instead of labels and is hard to read. It is kept
because it is known to boot and is the reference for checking anything against
real hardware.

It carries, beyond what mainline's shinano-common.dtsi has: the panel and DSI
nodes, the Adreno 330 GPU node, the Maxim MAX1187x touchscreen (replacing the
Z3's Synaptics), Bluetooth on UART, the audio path (Quaternary MI2S, the
amplifier I2C bus and two TFA9890 nodes, the APR and q6 service nodes), five
sensors on blsp2_i2c6, the battery OCV table, and a ramoops region.

For the readable, mainline-shaped version of the same device, see
`../upstream/`. That one has never been booted and omits everything that
depends on out-of-tree drivers.

## Kernel command line

The GPU needs a VRAM carveout, because msm8974 has no GPU IOMMU support in
mainline:

    cma=768M msm.vram=512m msm.allow_vram_carveout=1

With a smaller carveout the screen freezes once it fills. Note that
applications rendering on the GPU currently hang it; the compositor on the GPU
is stable, applications are not.

## Building and flashing

`../tools/phone-build-img.sh` packs a kernel and a compiled device tree into a
boot image with `mkbootimg-osm0sis`. `../tools/update-latest.sh` compiles a
`.dts` on the phone itself, which has `dtc`, and brings the results back.

One thing that wastes an afternoon if you miss it: the phone boots the image
that was flashed to the boot partition with fastboot. Installing a kernel
package updates `/boot/vmlinuz` and regenerates `/boot/boot.img`, and changes
nothing about what the phone actually boots. Rebuild and reflash after any
kernel update.
