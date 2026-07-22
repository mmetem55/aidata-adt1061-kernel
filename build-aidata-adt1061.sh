#!/usr/bin/env bash
set -e

# Android 10 / Linux 4.14.133
# Aidata ADT1061 (UMS512 - ARM64)

export ARCH=arm64
export SUBARCH=arm64

export BRANCH=android-4.14

export DEFCONFIG=aidata_adt1061_sharkl5Pro_defconfig

export KERNEL_DIR="$(pwd)"

export FILES="
arch/arm64/boot/Image.gz-dtb
arch/arm64/boot/Image
vmlinux
System.map
"

export STOP_SHIP_TRACEPRINTK=1

export EXTRA_CMDS=""

export POST_DEFCONFIG_CMDS="check_defconfig"

# --- Toolchains ---
export CLANG_PREBUILT_BIN="$(pwd)/prebuilts/clang/host/linux-x86/clang-r353983/bin"
export LINUX_GCC_CROSS_COMPILE_PREBUILTS_BIN="$(pwd)/prebuilts/gcc/arm-linux-androideabi-4.9/bin"
export LINUX_GCC_CROSS_COMPILE_ARM32_PREBUILTS_BIN="$(pwd)/prebuilts/gcc/arm-linux-androideabi-4.9/bin"

# --- Compilers & LLVM Binutils ---
export CC=clang
export HOSTCC=clang
export HOSTCFLAGS="-fcf-protection=none"

export LD=ld.lld
export AR=llvm-ar
export NM=llvm-nm
export OBJCOPY=llvm-objcopy
export OBJDUMP=llvm-objdump
export STRIP=llvm-strip

export CLANG_TRIPLE=aarch64-linux-gnu-
export CROSS_COMPILE=aarch64-linux-android-
export CROSS_COMPILE_ARM32=arm-linux-androideabi-

# --- PATH ---
export PATH="$CLANG_PREBUILT_BIN:$LINUX_GCC_CROSS_COMPILE_PREBUILTS_BIN:$LINUX_GCC_CROSS_COMPILE_ARM32_PREBUILTS_BIN:$PATH"

chmod +x scripts/* 2>/dev/null || true
chmod +x arch/arm64/kernel/vdso/gen_vdso_offsets.sh 2>/dev/null || true


filter_gcc_warning() {
    grep -v -E "Android GCC has been deprecated|GCC_4_9_DEPRECATION" >&2 || true
}

# --- Configure ---
make CC=clang HOSTCC=clang HOSTCFLAGS="-fcf-protection=none" "$DEFCONFIG" 2> >(filter_gcc_warning)

# defconfig check
if command -v check_defconfig >/dev/null 2>&1; then
    check_defconfig
fi

# --- Build ---
make CC=clang HOSTCC=clang HOSTCFLAGS="-fcf-protection=none" -j"$(nproc)" 2> >(filter_gcc_warning)
