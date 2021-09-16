![Mint icon](https://i.ibb.co/QmqbngH/core-readme-icon.png)

# Mint

_Freshen up your Galaxy. An optimized, One UI-first kernel for Galaxy devices on the Exynos 9610 platform._

#### Common Mint kernel source for the Exynos 9610 Platform

Supports the following devices:

- Samsung Galaxy A50 (`a50`)
- Samsung Galaxy A50s (`a50s`)

Looking for the Linux kernel readme? [Click here.](https://github.com/TenSeventy7/android_kernel_samsung_exynos9610_mint/blob/android-11.0/README_Kernel)

## Source References and Contributors

Shadow will **never** be possible without the unwavering work of these awesome people. I have tried my best to keep their authorships on the commit history. Thank you very much!

 - [Cruel Kernel for the Galaxy S10/Note10](https://github.com/CruelKernel/samsung-exynos9820/) (@evdenis)
 - [Motorola One Action/Vision Kernel Sources](https://github.com/MotorolaMobilityLLC/kernel-slsi)
 - [ThunderStorms Kernel for the Galaxy S10/Note10](https://github.com/ThunderStorms21th/Galaxy-S10) (@ThunderStorms21th)
 - [Destrictize Project](https://github.com/DestrictizeProject/Destrictize_9611) (@DestrictizeProject)
 - [Quantum Kernel](https://github.com/prashantpaddune/android_kernel_samsung_a50dd) (@prashantpaddune)
 - [Zeus Kernel for the Galaxy Note9](https://github.com/THEBOSS619/Note9-Zeus-Q10.0) (@THEBOSS619)
 - [Custom Galaxy A51 Kernel](https://github.com/ianmacd/a51xx) (@ianmacd)
 - [StormBreaker Kernel for the POCO X3](https://github.com/stormbreaker-project/kernel_xiaomi_surya) (@stormbreaker-project)
 - [Artemis Kernel for the Pixel 4 XL](https://github.com/celtare21/kernel_google_coral) (@celtare21)

## About

Mint is an optimized kernel source based on Samsung's open-source kernel drops of the Galaxy A50. Additional features include:

 - Built with cutting-edge LLVM/Clang (`proton-clang`)
 - Built with Link-Time Optimizations (LTO) enabled
 - Added additional I/O schedulers, `maple` is set as default.
 - Various kernel and performance improvements from upstream, and even Qualcomm devices.
 - Backported changes from the Galaxy A51, and Galaxy S10/Note10.
 - Replaced kernel RNG (HWRandom) with SRandom.
 - State notifier support for various kernel drivers.
 - Disabled basic Samsung hardening (Knox, etc).
 - DriveDroid support.
 - WireGuard support.

## How to Install

**The device must have an unlocked bootloader**; as well as TWRP, OrangeFox Recovery, or any recovery of your choice installed.

 1. Download latest available release from GitHub Releases.
 2. Copy the ZIP file to your SD card if necessary.
 3. Reboot to recovery.
 4. Flash downloaded ZIP.
 5. Reboot to **System**  instead of recovery.
 6. ???
 7. Profit

If you ever experience issues with lock screen and/or Samsung Account, see below.

## Device Locked Out?

If you ever experience being locked out after installing any build, with SystemUI restarting after entering your password/PIN, and Samsung Account showing a `Samsung Account logged out` notification, you may have been experiencing the "Pin Problem".

Several builds of TWRP and OrangeFox have a security patch level (SPL) of 2099-12 (December 2099), but this kernel follows the latest SPL available to the device. This causes the device to act up once booted into the kernel.

Please note that it is **intended behavior** by the system and is **not** a bug.

You can flash PassReset, or wipe your device to fix it.

See [here](https://github.com/CruelKernel/samsung-exynos9820/#pin-problem-cant-login) for more information.

 
## Building Locally

Local builds of Mint are built using **Ubuntu 20.04 LTS**.  These prerequisites are needed to build with this source:

 - libelf-dev
 - bzip2
 - lib32stdc++6
 - libc6-dev-i386
 - git

Once you have the prerequisites installed, simply run this on the Terminal.

`./build.sh -d|--device <device> [main options] [variant options]`

**Device options:**

```
- a50 # For Samsung Galaxy A50
- a50s # For Samsung Galaxy A50s
```

More options are available on the script by executing `./build.sh --help|-h`. The script will download all it needs (including the toolchain) and builds a new kernel build for you.
