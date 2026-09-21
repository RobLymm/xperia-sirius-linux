# Sony's camera `.dat` format, and what it says about the IMX200

Decoded 2026-09-21. The rear sensor of the Xperia Z2 has no driver anywhere —
not in mainline, not in Sony's released kernel, not in Qualcomm's sensor
modules. This is what was found instead, and how far it goes.

## The format is the same struct the camera middleware carries

`/system/lib/libcammw.so` (253 KB, Sony's camera middleware) exports one
function per sensor, each exactly 16 bytes:

    imx132_get_sensor_param   imx134_get_sensor_param
    imx135_get_sensor_param   imx200_get_sensor_param

All four are the same Thumb stub — `ldr r3,[pc,#8]; add r3,pc; str r3,[r0];
movs r0,#1; bx lr` — so each returns a pointer to a static struct. Following
those pointers gives four structs of 3164 bytes in the library's data segment.

**Those structs are the same thing as the `.dat` files** in
`/system/vendor/camera/`. Byte 2 onward of the library's IMX132 struct is
identical to `SEM02BN1_IMX132.dat` for 230 bytes, and the same holds for the
IMX200. Only the first `u16` differs — it is the length, so the library's
3164-byte copy is a shortened version of the 5816-byte (IMX132) and
34460-byte (IMX200) files on disk.

That matters because it gives a **known-answer control**: the IMX132 is a
sensor this project already has a working driver for, so any reading of the
format can be checked against values that are known to be right.

## Header

| offset | size | IMX132 | IMX200 | meaning |
|---|---|---|---|---|
| 0x00 | u16 | 5816 | 34460 | length of the file |
| 0x02 | u16 | 0x0107 | 0x0107 | format version |
| 0x04 | u32 | 0 | 3 | sensor index |
| 0x08 | u16 | **0x36** | **0x10** | I2C address |
| 0x0a | u16 | **0x0132** | **0x0200** | model number |

The I2C addresses and model numbers match what the project independently read
off the hardware over CCI, and the module EEPROMs.

## Mode records, and the IMX200's modes

From 0x00c0 the file carries per-mode records. The IMX132's first record
reads, as `u16`:

    0x00c6:  0, 1975, 0, 1199, 1976, 1200      crop left/right/top/bottom, then size
    0x00dc:  1976, 1200, 0x0101, 0x0101, ...   size, binning x/y, timing

1976x1200 is the IMX132's native array, which is exactly what this project's
`imx132.c` uses for `IMX132_NATIVE_WIDTH`/`_HEIGHT`. That is the control
passing.

The same offsets in `SOI20BS0_IMX200.dat`:

    0x00c6:  0, 5247, 0, 3935, 5248, 3936
    0x00dc:  5248, 3936, 0x0101, 0x0101, ...

**5248 x 3936 is 20.66 MP**, which is the Xperia Z2's advertised rear camera.
Counting repeated size pairs through the file gives the sensor's mode list:

| size | MP | likely use |
|---|---|---|
| 5248 x 3936 | 20.7 | full array |
| 5904 x 2621 | 15.5 | wide |
| 5248 x 2960 | 15.5 | 16:9 |
| 2624 x 1976 | 5.2 | 2x2 binned |
| 2624 x 1504 / 1480 | 3.9 | binned 16:9 |
| 1312 x 988 | 1.3 | 4x4 binned |
| 1312 x 740 | 1.0 | binned 16:9 |
| 656 x 492 | 0.3 | preview |

## What this does not give you

**Register sequences.** They are not in the file in any encoding tried. The
test for that was to take the 66 vendor-init register/value pairs this
project's `imx132.c` is known to need and look for them in the IMX132's own
`.dat`: 25 of 66 addresses appear as bare 16-bit words and only 9 as
address+value, which is the rate you would get by chance in a 5.8 KB file. If
the format held register tables, the control would have matched nearly all of
them.

So the geometry, timing and tuning are here; the register writes that make the
sensor stream are somewhere else in Sony's stack. That is the remaining gap
for a rear camera driver, and it is the hard half — the front sensor needed
Intel's atomisp tables for exactly this, because an imx219's MIPI registers
did not work on it.
