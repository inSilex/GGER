# syntax=docker/dockerfile:1

# Build environment for the GGER (Gamos Game Engine Reanimated) release targets.
#
# The image contains the Linux, MinGW, Android, and PS5 host tools plus the
# Python dependencies.  The default netwalk-ps5 target bundles a pinned
# ps5-payload-dev SDK, LLVM 20, and prebuilt native converter/runtime/makefs
# tools.  Those are separately licensed dependencies; see docker/README.md
# before redistributing the image.  Build --target netwalk-base for the lean
# Linux/Windows/Android image without PS5 support.
#
# Build from the repository root:
#
#   docker build -f docker/Dockerfile -t netwalk-build:latest .
#   docker build --target netwalk-base -f docker/Dockerfile -t netwalk-build:base .
#
# The image target is linux/amd64 because the Windows and PS5 toolchains are
# x86-64 host tools.  The release wrapper passes --platform; do the same when
# building the image directly.  Ubuntu 24.04 is the oldest practical baseline:
# its glibc 2.39 provides the 2.38 __isoc23_* entry points used by the checked-in
# static PulseAudio archives and matches the common WSL2/Ubuntu 24.04 runtime.
# The Linux executable links glibc dynamically, so its runtime glibc must be at
# least this image's glibc.

FROM ubuntu:24.04 AS netwalk-base

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        file \
        g++-mingw-w64-x86-64 \
        gcc-mingw-w64-x86-64 \
        git \
        libasound2-dev \
        libedit2 \
        libffi8 \
        libfreetype-dev \
        libjpeg-dev \
        libpulse-dev \
        libx11-dev \
        libxcursor-dev \
        libxext-dev \
        libxi-dev \
        libxinerama-dev \
        libxkbcommon-dev \
        libxrandr-dev \
        libxss-dev \
        libxtst-dev \
        libxml2 \
        libzstd1 \
        ninja-build \
        openjdk-17-jdk-headless \
        patch \
        pkg-config \
        python3 \
        python3-dev \
        python3-pip \
        python3-venv \
        tar \
        unzip \
        wget \
        xz-utils \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Android application packaging.  Keep the SDK/NDK versions in sync with
# platforms/android/app/build.gradle and docs/BUILD.md.  The SDK is part of the
# image so the release wrapper can assemble a signed APK in the same container
# as the desktop targets.
ARG ANDROID_SDK_ROOT=/opt/android-sdk
ARG ANDROID_CMDLINE_TOOLS_URL=https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
ENV ANDROID_HOME="${ANDROID_SDK_ROOT}" \
    ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT}" \
    ANDROID_NDK_HOME="${ANDROID_SDK_ROOT}/ndk/27.2.12479018" \
    JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64

RUN mkdir -p "${ANDROID_SDK_ROOT}/cmdline-tools" \
    && wget -q "${ANDROID_CMDLINE_TOOLS_URL}" -O /tmp/android-cmdline-tools.zip \
    && unzip -q /tmp/android-cmdline-tools.zip -d "${ANDROID_SDK_ROOT}/cmdline-tools" \
    && mv "${ANDROID_SDK_ROOT}/cmdline-tools/cmdline-tools" \
          "${ANDROID_SDK_ROOT}/cmdline-tools/latest" \
    && yes | "${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin/sdkmanager" \
          --licenses >/dev/null \
    && "${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin/sdkmanager" \
          --install "platforms;android-35" "build-tools;35.0.0" \
          "ndk;27.2.12479018" "cmake;3.22.1" \
    && rm -rf /tmp/android-cmdline-tools.zip \
          "${ANDROID_SDK_ROOT}/.temp" "${ANDROID_SDK_ROOT}/.downloadIntermediates"

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    PATH="${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin:${ANDROID_SDK_ROOT}/platform-tools:/opt/venv/bin:${PATH}" \
    PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PYTHONDONTWRITEBYTECODE=1

COPY tools/requirements.txt /tmp/netwalk-requirements.txt

RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir --upgrade pip \
    && /opt/venv/bin/pip install --no-cache-dir -r /tmp/netwalk-requirements.txt

COPY docker/container-build.sh /usr/local/bin/netwalk-build-releases
RUN chmod 0755 /usr/local/bin/netwalk-build-releases

WORKDIR /work
ENTRYPOINT ["/usr/local/bin/netwalk-build-releases"]
CMD ["--help"]

# Native PS5 toolchain.  Keep the SDK version and digest pinned so a rebuilt
# image does not silently change the title ABI.  The apt.llvm.org package
# repository supplies the LLVM 20 host tools required by prospero-clang.
# The SDK, generated libc.prx, and makefs are GPL/BSD build dependencies; the
# source revisions are retained under /opt for license compliance.
FROM netwalk-base AS netwalk-ps5

ARG PS5_SDK_VERSION=v0.45
ARG PS5_SDK_SHA256=5fac64093d0e3e69f2584ce7935f24f703f622d9681f1495c85b9f00d1291a2c
ARG PS5_LLVM_APT_SUITE=noble
ARG PS5_LLVM_VERSION=20

RUN wget -qO /etc/apt/trusted.gpg.d/llvm.asc \
        https://apt.llvm.org/llvm-snapshot.gpg.key \
    && printf '%s\n' \
        "deb [signed-by=/etc/apt/trusted.gpg.d/llvm.asc] https://apt.llvm.org/${PS5_LLVM_APT_SUITE}/ llvm-toolchain-${PS5_LLVM_APT_SUITE}-${PS5_LLVM_VERSION} main" \
        > /etc/apt/sources.list.d/llvm-${PS5_LLVM_VERSION}.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        "clang-${PS5_LLVM_VERSION}" \
        "lld-${PS5_LLVM_VERSION}" \
        "llvm-${PS5_LLVM_VERSION}" \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /opt/llvm20/bin /opt/llvm20/root/usr/bin \
        /opt/llvm20/root/usr/lib \
    && ln -s /usr/bin/llvm-config-20 /opt/llvm20/bin/llvm-config-20 \
    && ln -s /usr/bin/llvm-config-20 /opt/llvm20/root/usr/bin/llvm-config-20 \
    && ln -s /usr/lib/x86_64-linux-gnu \
        /opt/llvm20/root/usr/lib/x86_64-linux-gnu \
    && ln -s /usr/lib/llvm-20 /opt/llvm20/root/usr/lib/llvm-20

RUN wget -q \
        "https://github.com/ps5-payload-dev/sdk/releases/download/${PS5_SDK_VERSION}/ps5-payload-sdk.zip" \
        -O /tmp/ps5-payload-sdk.zip \
    && echo "${PS5_SDK_SHA256}  /tmp/ps5-payload-sdk.zip" | sha256sum -c - \
    && unzip -q /tmp/ps5-payload-sdk.zip -d /opt \
    && rm -f /tmp/ps5-payload-sdk.zip

ENV PS5_SDK=/opt/ps5-payload-sdk \
    PS5_LLVM=/opt/llvm20

COPY platforms/ps5/tools/setup-native.sh \
    /usr/local/lib/netwalk/platforms/ps5/tools/setup-native.sh
COPY platforms/ps5/tools/setup-packaging.sh \
    /usr/local/lib/netwalk/platforms/ps5/tools/setup-packaging.sh
COPY platforms/ps5/tools/patches/makefs-no-crypto.patch \
    /usr/local/lib/netwalk/platforms/ps5/tools/patches/makefs-no-crypto.patch

RUN chmod 0755 \
        /usr/local/lib/netwalk/platforms/ps5/tools/setup-native.sh \
        /usr/local/lib/netwalk/platforms/ps5/tools/setup-packaging.sh \
    && /usr/local/lib/netwalk/platforms/ps5/tools/setup-native.sh --out /opt/ps5-native \
    && /usr/local/lib/netwalk/platforms/ps5/tools/setup-packaging.sh --out /opt/ps5-packaging

ENV NW_PS5_NATIVE_TOOL=/opt/ps5-native/tool/ps5-native-tool \
    NW_PS5_NATIVE_LIBC=/opt/ps5-native/tool/libc.prx \
    NW_PS5_MAKEFS=/opt/ps5-packaging/makefs
