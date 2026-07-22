# Aidata ADT1061 Kernel

Linux Kernel **4.14.133** source code for the **Aidata ADT1061** tablet.

> This repository contains the Android/Linux kernel source used for the Aidata ADT1061 device and serves as a base for development, research, customization and kernel modifications.

## Purpose ##

The purpose of this repository is to:

- Preserve the original kernel source.
- Make future kernel modifications easier.
- Develop custom features.
- Help developers studying the Aidata ADT1061 platform.

## Device Information ##

| Item | Value |
|------|------|
| Device | Aidata ADT1061 |
| Kernel Version | 4.14.133 |
| Platform | Unisoc UMS512 |

## Building ##

```bash
git clone https://github.com/mmetem55/aidata-adt1061-kernel.git
cd aidata-adt1061-kernel
```

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-android-
```

```bash
chmod +x scripts/*
chmod +x arch/arm64/kernel/vdso/gen_vdso_offsets.sh
```

```bash
make aidata_adt1061_sharkl5Pro_defconfig
make -j$(nproc)
```

## Status ##

🟢 Active development

This project is intended for developers interested in Android kernel development, reverse engineering, and device customization.

## Credits ##

- Linux Kernel Developers
- Spreadtrum/Unisoc Develop```bashers
- strongtz/linux-sprd
- mmetem55

## License ##

This project follows the original Linux Kernel (GPL-2.0) licensing.
