# From a stock Xperia Z2 to this phone

This is the whole route, start to finish, for someone who has a Sony Xperia
Z2 running the Android it came with and wants it running mainline Linux the
way the phone described in this repository does. It assumes you have nothing
installed and know nothing about postmarketOS or Sony's tooling. Every
command is written out. Wherever you have to supply a piece of information,
it says where to get that information.

Read the two warnings before you start. One of them is irreversible.

## What you get

Verified working on the phone this was developed on:

| | |
|---|---|
| Display, touch, GPU | 1080p, Maxim touchscreen, Adreno 330 running the compositor |
| Calls and SMS | The modem registers and a call has been made and answered |
| Mobile data | Untested, but the modem attaches to the packet service |
| GNSS | Streams NMEA and tracks satellites |
| Wi-Fi and Bluetooth | Broadcom, both in-tree drivers |
| Speakers and earpiece | Two TFA9890 amplifiers |
| Headphones | WCD9320 codec over SLIMbus |
| Handset microphone | Same codec |
| FM radio | The tuner inside the Bluetooth chip, with its own app |
| Battery | Percentage and charging |
| Sensors | Accelerometer, gyroscope, magnetometer, barometer |

Not working: camera, NFC, suspend and resume, CPU frequency scaling above
960 MHz, the secondary and headset microphones, headphone jack detection.
`known-problems.md` and `remaining-hardware.md` have the detail.

## Two warnings

**Unlocking the bootloader is irreversible and it erases keys that cannot be
put back.** Sony's DRM keys live in a protected area of the phone. Unlocking
destroys them. You lose Sony's camera post-processing and some DRM video
playback permanently, on this phone, whatever you install afterwards.
Relocking does not bring them back. If that matters to you, stop here.

**Unlocking wipes the phone.** Everything in the user area goes. Copy your
photos and anything else off it first.

## What you need

**The phone.** An Xperia Z2: model D6502, D6503 or D6543. To check, on the
stock Android: Settings, then About phone, then Model number. The phone this
port was developed on is a D6503.

**A computer running Linux.** Anything current. The instructions use Alpine,
Debian and Fedora package names where they differ.

**A USB cable** that does data, not only charging. Sony's own cable is fine.

**An email address**, for Sony's unlock code.

**About 8 GB free** on the computer for the build.

## Step 1: find your IMEI

Sony needs it to give you an unlock code.

On the stock Android, open the dialler and type:

    *#06#

The IMEI appears on screen. There is no need to press call. Write it down.

If that does not work, Settings, then About phone, then Status, then IMEI
information.

Sony's unlock page asks for the IMEI **without its last digit**. The last
digit is a check digit. So a 15 digit IMEI becomes the first 14 digits. The
page states what it wants; follow what it says if it differs.

## Step 2: allow unlocking, on the phone

The phone has to be told to permit it before the bootloader will listen.

1. Settings, then About phone. Tap **Build number** seven times. It tells you
   developer mode is on.
2. Go back to Settings, then Developer options.
3. Turn on **OEM unlocking**.
4. Turn on **USB debugging** as well, which is useful later.

If there is no OEM unlocking entry, this phone's bootloader is one Sony does
not allow unlocking (some carrier models). Check on Sony's page in step 3:
it lists which models are supported.

## Step 3: get the unlock code from Sony

Go to:

    https://developer.sony.com/develop/open-devices/get-started/unlock-bootloader/

On that page:

1. Choose your model from the list of supported devices. The Xperia Z2 is
   there.
2. Enter your email address and accept the terms.
3. Sony emails you a link. Open it.
4. Enter the IMEI from step 1, in the form the page asks for.
5. Sony shows you an unlock code: a long hexadecimal string. **Copy it
   somewhere safe.** You need it in step 6 and you cannot get it again
   without repeating this.

## Step 4: install the tools on your computer

**fastboot**, which talks to the phone's bootloader:

    # Alpine
    sudo apk add android-tools
    # Debian or Ubuntu
    sudo apt install android-tools-fastboot android-tools-adb
    # Fedora
    sudo dnf install android-tools

**pmbootstrap**, which builds postmarketOS:

    # Alpine
    sudo apk add pmbootstrap
    # anything else
    pip install --user pmbootstrap

Check both:

    fastboot --version
    pmbootstrap --version

**Let your user talk to the phone over USB.** Without this every fastboot
command needs sudo, and sudo sometimes cannot see the device:

    sudo tee /etc/udev/rules.d/51-android.rules >/dev/null <<'EOF'
    SUBSYSTEM=="usb", ATTR{idVendor}=="0fce", MODE="0666", GROUP="plugdev"
    EOF
    sudo udevadm control --reload-rules
    sudo usermod -aG plugdev "$USER"

`0fce` is Sony's USB vendor identifier. Log out and back in for the group to
take effect.

## Step 5: get the phone into fastboot

There is no menu for this. It is a key held while the cable goes in.

1. Power the phone **off** completely. Hold power until it shuts down, and
   wait for the screen to go dark.
2. Unplug the USB cable.
3. Hold **Volume Up**, and keep holding it.
4. With Volume Up still held, plug the USB cable into the computer.
5. The notification LED at the top of the phone turns **blue**. The screen
   stays black. That is fastboot mode. Let go of Volume Up.

Check the computer can see it:

    fastboot devices

You should get one line with a serial number and the word `fastboot`. If you
get nothing, the LED is not blue, or the cable does not carry data, or the
udev rule in step 4 has not taken effect yet (try `sudo fastboot devices` to
tell those apart).

To leave fastboot at any point: `fastboot reboot`, or hold power for ten
seconds.

## Step 6: unlock the bootloader

This is the irreversible step. With the phone in fastboot:

    fastboot -i 0x0fce oem unlock 0xYOURCODEHERE

Replace `YOURCODEHERE` with the code Sony emailed you in step 3. Keep the
`0x` in front of it. So if Sony gave you `1A2B3C4D5E6F7788`, you type
`0x1A2B3C4D5E6F7788`.

It should answer `OKAY`. The phone wipes itself and reboots.

If it says `FAILED (remote: Command not allowed)`, OEM unlocking in step 2 is
not on, or the model does not allow it.

## Step 7: install postmarketOS, as an Xperia Z3

This is the part that looks wrong and is not. **Install it as the Xperia Z3,
codename `sony-leo`, not as the Z2.**

The reason: postmarketOS does have a Z2 device package, but this port is
built and tested on the Z3's, because the Z3 packages carry firmware the Z2
needs. The audio DSP firmware in particular: there is no Z2 package for it,
and the Z3's is what the audio in this port actually runs on. The Z2's own
device tree is layered on top in step 9, which is what makes the phone a Z2
again.

Set up pmbootstrap. It asks a series of questions:

    pmbootstrap init

Answer:

- channel: **edge**
- vendor: **sony**
- device: **leo**
- kernel: the default
- user interface: **phosh** (this port's userspace expects it)
- a username and a password, which you choose
- everything else: the default is fine

Build the image. This takes a while the first time, tens of minutes:

    pmbootstrap install

Put the phone into fastboot again, exactly as in step 5, then:

    pmbootstrap flasher flash_rootfs
    pmbootstrap flasher flash_kernel
    fastboot reboot

The phone boots postmarketOS. Wi-Fi works, the screen works, there is no
audio yet and no modem. Connect it to your Wi-Fi and find its address, so you
can reach it over SSH from the computer. On the phone, in Settings, or:

    ssh YOURUSERNAME@PHONE-ADDRESS

Everything from here happens over that connection.

## Step 8: take what you need off your own phone

Installing postmarketOS does not touch the stock Android **system**
partition, which is where the proprietary firmware lives. It is still there.
Do not reformat it.

One file is not packaged by anyone and has to come off your phone: the
Bluetooth chip's firmware. That chip is also the FM radio, so this file is
what makes both work.

Copy this repository to the phone and run the extraction script:

    git clone https://github.com/RobLymm/xperia-sirius-linux.git
    cd xperia-sirius-linux
    sudo sh tools/extract-from-stock.sh

It mounts the stock system partition read only, installs the Bluetooth
firmware where the kernel looks for it, and backs up to
`~/stock-backup`:

- the whole stock firmware directory, in case you need something else later
- **the modem's calibration partitions**, `modemst1`, `modemst2`, `fsg` and
  `TA`. These are unique to your phone and cannot be downloaded from
  anywhere. If you lose them you lose the modem. Copy that directory to your
  computer and keep it.
- the FOTA kernel image, which holds Sony's own device trees. Those are the
  best reference for anything not yet working:

      sh tools/split-fota-dtbs.sh ~/stock-backup/fota/FOTAKernel.img

## Step 9: add this port's pieces

This is the step that is not yet one command, and it is worth being plain
about that. The pieces are all here, and the phone they came from is built
from them, but they are not packaged: they were built on the device itself as
the work went along. What follows is what each piece is and where its own
instructions live.

**The device tree** is the one thing you certainly need, because the
packaged one describes an Xperia Z3.

    devicetree/qcom-msm8974pro-sony-xperia-shinano-sirius.dts

`devicetree/README.md` says what it contains. Build it against a kernel
source tree with:

    tools/build-board-dtb.sh <kernel-source-tree> sirius

**The boot image** puts that device tree together with the packaged kernel
and, importantly, the command line the GPU needs:

    sudo sh tools/phone-build-img.sh /path/to/sirius.dtb \
        "cma=768M msm.vram=512m msm.allow_vram_carveout=1" \
        /tmp/boot-sirius.img

Without `msm.vram=512m` the display comes up and then stops waking, because
this SoC has no GPU memory management unit in mainline and the GPU needs a
reserved carveout. If you ever see `could not get pages: -28` in the kernel
log, that is this.

**The drivers**, each with its own README explaining what it is and how to
build it:

| | |
|---|---|
| `drivers/panel/` | The display panel. Six variants exist; the right one is chosen at runtime |
| `drivers/audio/msm8974-sndcard.c` | The sound card: speakers, FM capture, and the codec links |
| `drivers/audio/wcd9320/` | The headphone and microphone codec |
| `drivers/clk/pmic-clkdiv/` | The codec's master clock. Without it the codec is silent |
| `drivers/audio/fmrepair/` | Repairs the FM tuner's capture at the audio device layer |
| `drivers/battery/` | Battery percentage from the voltage sense channel |

**Userspace**:

| | |
|---|---|
| `userspace/ucm2/` | The audio profile. Without it there is no named speaker output |
| `modem/ta-service/` | What gets the modem through its startup. See below |
| `apps/` and the radio app | The FM radio application |

**The modem needs one fix that is not in any package.** Sony's modem firmware
waits for the application processor to answer requests about the phone's trim
area, and stops with its own watchdog after about forty seconds if nobody
does. The daemon that answers them is `ta-service`, and it needs a one line
change to read the whole partition rather than the first block.
`modem/README.md` has the patch and the service file. This is not Z2
specific: every Sony msm8974 phone needs it.

## Step 10: flash the boot image

Put the phone into fastboot as in step 5, then, from the computer:

    fastboot flash boot /path/to/boot-sirius.img
    fastboot reboot

`fastboot flash boot` only ever writes the boot partition. The rootfs you
installed in step 7 is untouched, so this is the safe command to repeat as
often as you like while working on the device tree or the kernel.

## Step 11: check each piece

The repository has test scripts that report what the hardware did, not merely
whether a command succeeded:

    sh tools/headphone-test.sh      # plays a tone and reads the amplifier status
    sh tools/mic-test.sh            # records, and measures a tone it should hear
    sh tools/bcm-fm.sh on           # powers the FM tuner
    sh tools/bcm-fm.sh tune 98.9
    sh tools/bcm-fm.sh sweep        # finds stations

For the modem, `mmcli -L` should list one modem, and `mmcli -m 0` should show
it registered with an operator name once a SIM is in.

## If the phone will not boot

`fastboot flash boot` is always recoverable, because the bootloader does not
depend on the boot partition. Get back into fastboot with step 5 and flash a
boot image you know works. Keep one.

If the screen stays dark but the phone is alive, it is almost always the GPU
carveout missing from the command line: see step 9.

If it boots but hangs before the desktop appears, check whether it is
reachable over SSH. If it is, the kernel is fine and something in userspace
is stuck; `systemctl list-jobs` says what is waiting.

## Where the pieces came from

Nothing here is a rewrite. The device tree builds on what mainline already
has for the other three phones on this platform. The codec driver is a
forward-port of one written for a different phone on the same platform. The
panel driver is generated from Sony's own panel data. What this port
contributes is mostly the finding out: which of Sony's numbers matter, which
mainline drivers already fit, and the handful of real defects in between.
`prior-art.md` credits the work this is built on.
