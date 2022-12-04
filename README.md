![Mint branding](https://i.ibb.co/9WV53PD/Banner-New-Dark.png)

# Mint Kernel

_Freshen up your Galaxy. An optimized, One UI-first kernel for Samsung Galaxy devices on the Exynos 9610 platform._

#### Common Mint kernel source for the Exynos 9610 Platform

Supports the following devices:

- Samsung Galaxy A50 (`a50`)

Looking for the Linux kernel readme? [Click here.](https://github.com/TenSeventy7/android_kernel_samsung_exynos9610_mint/blob/android-12.0/README_Kernel)

## Source References and Contributors

Mint Kernel will **never** be possible without the unwavering work of these awesome people. I have tried my best to keep their authorships on the commit history. Thank you very much!

 - [Cruel Kernel for the Galaxy S10/Note10](https://github.com/CruelKernel/samsung-exynos9820/) (@evdenis)
 - [GaltsGulch for the OnePlus 7 series](https://github.com/RealJohnGalt/GaltsGulch-sm8150) (@RealJohnGalt)
 - [DragonHeart Kernel for the OnePlus 7 series](https://github.com/cyberknight777/dragonheart_kernel_oneplus_sm8150) (@cyberknight777)
 - [Sultan Kernel for the Google Pixel 4](https://github.com/kerneltoast/android_kernel_google_floral) (@kerneltoast)
 - [ThunderStorms Kernel for the Galaxy S10/Note10](https://github.com/ThunderStorms21th/Galaxy-S10) (@ThunderStorms21th)
 - [Cosmic Fresh for the Moto G40 Fusion](https://github.com/Dark-Matter7232/CosmicFresh-Hanoip) (@Dark-Matter7232)
 - [Motorola One Action/Vision Kernel Sources](https://github.com/MotorolaMobilityLLC/kernel-slsi)
 - [Quantum Kernel](https://github.com/prashantpaddune/android_kernel_samsung_a50dd) (@prashantpaddune)
 - [Zeus Kernel for the Galaxy Note9](https://github.com/THEBOSS619/Note9-Zeus-Q10.0) (@THEBOSS619)
 - [Custom Galaxy A51 Kernel](https://github.com/ianmacd/a51xx) (@ianmacd)
 - [StormBreaker Kernel](https://github.com/stormbreaker-project/kernel_xiaomi_surya) (@stormbreaker-project) and [Stratosphere Kernel](https://github.com/Stratosphere-Kernel/android_kernel_xiaomi_surya) (@Stratosphere-Kernel) for the POCO X3
 - [Artemis Kernel for the Pixel 4 XL](https://github.com/celtare21/kernel_google_coral) (@celtare21)
 - [GS101/Tensor SoC Kernel Source](https://github.com/AndreiLux/GS101) (@Google/@AndreiLux)

## About

Mint is an optimized kernel source based on Samsung's open-source kernel drops of the Galaxy A50. Additional features include:

 - Built with LLVM/Clang (`proton-clang`) 13
 - Built with Link-Time Optimizations (LTO) enabled
 - A fully-revamped Exynos Mobile Scheduler (EMS) optimized for low-latency tasks.
 - A smarter governor with boost algorithms from the Galaxy S10 as well as 'inferencing' from the S20.
 - Added additional I/O schedulers, `anxiety` is set as default.
 - Various kernel and performance improvements from Linux upstream, and even from Qualcomm devices.
 - Backported changes from multiple devices, including newer Galaxy devices.
 - RAM Plus support (requires ROM support), including support for per-process swap.
 - Disabled basic Samsung hardening (Knox, etc).
 - Added support for WireGuard VPN tunnel.

## How to Install

**The device must have an unlocked bootloader**; as well as TWRP, SHRP, or any recovery of your choice installed.

 1. Download latest available release from GitHub Releases.
 2. Copy the ZIP file to your SD card if necessary.
 3. Reboot to recovery.
 4. Flash downloaded ZIP.
 5. Reboot to **System**  instead of recovery.
 6. ???
 7. Profit
 
## Building Locally

Local and GitHub Action builds of Mint are built using **Ubuntu 22.04 LTS**. The following prerequisites are needed to build with this source:

### Ubuntu/Debian

```
sudo apt-get install libelf-dev bzip2 bc p7zip-full jq git python3 python-is-python3
```

### Fedora

```
sudo dnf install elfutils-libelf-devel bzip2 bc p7zip jq git python3
```

Once you have the prerequisites installed, simply run this on the Terminal.

`./build.sh -d|--device <device> -v|--variant <variant> -a|--android <Android version> [main options]`

**Device options:**

```
- a50 # For Samsung Galaxy A50
```

More options are available on the script by executing `./build.sh --help|-h`. The script will download all it needs (including the toolchain) and builds a new kernel build for you.
