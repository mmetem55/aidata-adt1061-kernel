# Aidata ADT1061 Kernel

Linux Kernel **4.14.133** source code for the **Aidata ADT1061** tablet.

## Device Information ##

| Item | Value |
|------|------|
| Device | Aidata ADT1061 |
| Kernel Version | 4.14.133 |
| Platform | Unisoc UMS512 |

## Building ##

Use Ubuntu 20.04 LTS for the best result

```bash
git clone https://github.com/mmetem55/aidata-adt1061-kernel.git
cd aidata-adt1061-kernel
```

```bash
chmod +x build-aidata-adt1061.sh
chmod +x scripts/*
chmod +x arch/arm64/kernel/vdso/gen_vdso_offsets.sh
```

```bash
./build-aidata-adt1061.sh
```

## Status ##

🟢 Active development

## Credits ##

- Linux Kernel Developers
- Spreadtrum/Unisoc Develop
- strongtz/linux-sprd
- mmetem55

## License ##

This project follows the original Linux Kernel (GPL-2.0) licensing.
