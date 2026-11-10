# 课守 ClassKeep — 真实编译方案

> 目标：把「只做过静态复核」变成**真正编译过**，并搞清楚怎样才能产出 Windows 教师端 / 学生端二进制。
> 仓库：<https://github.com/zk26/ClassKeep> · 上游 Veyon **4.11.4**

---

## 0. 先讲清楚之前为什么编不了

| 障碍 | 事实 | 证据 |
|---|---|---|
| 本机没有工具链 | 无 `cmake`、无 Qt、无 QCA、无 LibVNC | 本机 `cmake --version` 失败；Android 侧有 JDK 23 + SDK，已实测编译成功 |
| **workflow 放错位置** | 仓库根是 `G:\veyon`，但所有 workflow 都在 `veyon/.github/workflows/`；GitHub Actions **只读取仓库根** `.github/workflows/*.yml` → 从来没被触发过 | 已修：根下新建 `classkeep-build.yml` / `classkeep-release.yml` |
| **子模块全空** | 仓库根没有 `.gitmodules`（只有 `veyon/.gitmodules`），7 个 `3rdparty/*` 目录都是 0 文件；`actions/checkout` 的 `submodules: recursive` 拉不到 | 已在 build workflow 里显式 clone |
| **MSVC 路线不可行** | Veyon 的 CMake 硬编码 GCC 参数：`-Wno-attributes`、`-fno-exceptions`、`-Wl,-no-undefined`、`-fPIE/-pie`、`-lws2_32`、`-mwindows`、`-Wno-*`（UltraVNC）；`windows-binaries` target 全是 `cp/strip/mv/find/sed` Unix 命令并依赖 `${MINGW_PREFIX}` | `CMakeLists.txt:106-107,256-273`、`SetDefaultTargetProperties.cmake:16,30`、`WindowsInstaller.cmake:3-6,17-69` |
| **QCA 没有 vcpkg port** | `find_package(Qca-qt6 REQUIRED)` 要 `Qca-qt6Config.cmake`（CONFIG 模式），仓库无 `FindQCA.cmake`；vcpkg 无 `qca` port，且 MSVC ABI 与 MinGW 不兼容 | `CMakeLists.txt:234`、`core/CMakeLists.txt:37-43` |
| **官方 Windows 构建是 MinGW 交叉编译** | 上游 `.ci/windows/build.sh` 用 `/usr/x86_64-w64-mingw32/bin/qt-cmake -G Ninja` → `ninja windows-binaries`；GitLab 镜像 `ci-mingw-w64:main` **私有且不提供 Dockerfile**，外部无法重建 | `.gitlab-ci.yml:43-53`、`.ci/windows/build.sh:14-26` |

一句话：**Windows 二进制只能用 MinGW-w64/GCC 编译，MSVC + vcpkg 是死路；但「验证代码能否编译」可以立刻在 Linux/GCC 上做到。**

---

## 1. 路线 A（推荐，现在就能做）：GitHub Actions 上真实编译

已写好 [`.github/workflows/classkeep-build.yml`](../../.github/workflows/classkeep-build.yml)。
它做两件事：

| Job | 环境 | 做什么 |
|---|---|---|
| `desktop-linux` | `ubuntu-22.04` + 上游 Debian 11 那套 Qt5/QCA 依赖 | **全量编译 Veyon + 课守插件**，产出 `classroom.so` 与完整编译日志 |
| `android` | `ubuntu-latest` + JDK 21 | `:app:assembleDebug` + `:app:testDebugUnitTest`（本机已验证可行） |

### 你现在的操作

1. 把本次改动推到 `main`（已推送 / 或你自己 push）；
2. 打开 <https://github.com/zk26/ClassKeep/actions>；
3. 选 **ClassKeep Build Check** → **Run workflow**（`main` 分支）；
4. 看 `desktop-linux` 的 `Build` 日志。**这一步会真正把所有 C++ 文件交给 GCC**，
   静态复核发现不了的错误（缺 include、签名不匹配、模板推导）都会在这里暴露；
5. 把报错日志贴给我，我逐个修，直到 `BUILD SUCCESS`。

### 它用的确切配置

```bash
# 依赖（与上游 .ci/linux.debian.11/Dockerfile 一致）
sudo apt-get install -y build-essential cmake ninja-build git pkg-config \
  qtbase5-dev qtbase5-private-dev qtbase5-dev-tools qttools5-dev qttools5-dev-tools \
  libqca-qt5-2-dev libqca-qt5-2-plugins \
  libssl-dev zlib1g-dev libjpeg-dev libpng-dev liblzo2-dev \
  libpam0g-dev libprocps-dev libldap2-dev libsasl2-dev \
  libfakekey-dev xorg-dev libvncserver-dev \
  libpipewire-0.3-dev libspa-0.2-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev

# 3rdparty（根仓库没有 .gitmodules，必须显式 clone）
# 必须检出上游 veyon/veyon v4.11.4 锁定的 commit，直接 clone HEAD 会编译失败：
# 当前 KDE kldap master 是 Qt6-only，Qt5 构建会报 `Qt::Literals` / `_L1` / `QLatin1StringView` 未定义。
fetch() {
  git init -q "veyon/3rdparty/$1"
  git -C "veyon/3rdparty/$1" remote add origin "$2"
  git -C "veyon/3rdparty/$1" fetch -q --depth 1 origin "$3"
  git -C "veyon/3rdparty/$1" checkout -q FETCH_HEAD
}
fetch libvncserver https://github.com/veyon/libvncserver.git 30860286c0f55674a846df9cab458da9afd1735b
fetch x11vnc       https://github.com/veyon/x11vnc.git       c1e38369e2c601df7ab894c378df0cc2380a1ede
fetch libfakekey   https://github.com/veyon/libfakekey.git    7ad885912efb2131e80914e964d5e635b0d07b40
fetch kldap-qt5    https://invent.kde.org/pim/kldap.git      27bf9afbf5fb0bc587e7d707c3ea10845dcfdf34

# 这些 SHA 来自 https://api.github.com/repos/veyon/veyon/git/trees/v4.11.4?recursive=1
# 其余三个（kldap / qthttpserver / ultravnc）只在 Qt6 / WebAPI / Windows 构建里用到。
```

cmake -S veyon -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_QT6=OFF -DWITH_BUNDLED_LIBVNC=ON \
  -DWITH_WEBAPI=OFF -DWITH_TRANSLATIONS=OFF \
  -DWITH_LTO=OFF -DWITH_PCH=OFF -DWITH_UNITY_BUILD=OFF \
  -DWITH_STACK_PROTECTION=OFF -DWITH_TESTS=OFF -DWITH_CORE_ONLY=OFF \
  -DCMAKE_SKIP_INSTALL_RULES=ON \
  -DCMAKE_CXX_FLAGS="-Wno-deprecated-enum-enum-conversion"
cmake --build build --parallel 4
```

`-DCMAKE_CXX_FLAGS="-Wno-deprecated-enum-enum-conversion"` 用于 Ubuntu 22.04 的 GCC 11+：
Qt5 的 `qsizepolicy.h` 会触发 `deprecated-enum-enum-conversion` 警告，
而 Veyon 全局开了 `-Wall -Werror`（`SetDefaultTargetProperties.cmake:16`），
导致连**上游自己的 `AboutDialog.cpp`** 都编不过。Debian 11 的 GCC 10 没有这个警告，
所以上游 CI 不需要它。这是环境差异，不是课守代码的问题。

`-DCMAKE_SKIP_INSTALL_RULES=ON` 是必需的：bundled LibVNC 的 `vncclient` / `vncserver`
把源码目录写进了 `INTERFACE_INCLUDE_DIRECTORIES`，Veyon 的 `install(TARGETS ...)`
会在 generate 阶段直接报 `... is prefixed in the source directory` 并拒绝生成 `build.ninja`。
只做编译验证时跳过 install 规则即可，不需要改上游源码。

> CI 里所有 `... | tee xxx.log` 的步骤都加了 `set -o pipefail`，否则 `tee` 的退出码
> 会掩盖 cmake/ninja 的失败（第一次运行就踩过这个坑，日志里出现
> `ninja: error: loading 'build.ninja'` 却把 Build 步骤标成 success）。

参数解释：

- `WITH_QT6=OFF`：Windows 平台插件强制 Qt6，但**Linux 构建不进入该插件**，Qt5 依赖在 Ubuntu 上最齐全（上游 debian.11 CI 就是 Qt5）。
- `WITH_BUNDLED_LIBVNC=ON`：免掉系统 `libvncserver >= 0.9.14`（Ubuntu 22.04 版本偏低）。
  注意它会连带把 `WITH_TESTS` 置 OFF（`CMakeLists.txt:199`），所以 `ClassroomSelfTest` 在这个配置下不会构建。
- `WITH_WEBAPI=OFF` / `WITH_TRANSLATIONS=OFF`：免 `Qt5HttpServer` 与 LinguistTools；课守 Bridge 只用 `QTcpServer/QUdpSocket`，不依赖 WebAPI。
- `WITH_UNITY_BUILD=OFF`：让每个 `.cpp` 单独编译，最容易暴露真实错误。
- `WITH_CORE_ONLY=OFF`：**必须**，否则整个 `plugins/`（含课守）都不会构建。

### ⚠️ 路线 A 验证不到什么

`ClassroomSystem.cpp` 里 UWF / Core Audio / WFP / SetupAPI 的实现都在 `#ifdef Q_OS_WIN` 内，
Linux 编译走 `#else` 分支，**不会验证这些 Windows 专属代码**。
它们需要用路线 B 的 MinGW 编译才能真正确认。其余 30 多个文件（协议、积分、词云、报表、Bridge、学生端 UI、插件分发）都能被路线 A 覆盖。

---

## 2. 路线 B：本机 Windows 编译出真正的教师端 / 学生端

需要 **MSYS2 + MinGW-w64 GCC**（不是 Visual Studio）。

### 2.1 准备工具链

```powershell
# 1) 安装 MSYS2（https://www.msys2.org/），然后：
C:\msys64\usr\bin\pacman.exe -Syu
C:\msys64\usr\bin\pacman.exe -S --needed `
  mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja `
  mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-5compat `
  mingw-w64-x86_64-qt6-tools mingw-w64-x86_64-qt6-translations `
  mingw-w64-x86_64-qt6-httpserver mingw-w64-x86_64-qt6-websockets `
  mingw-w64-x86_64-openssl mingw-w64-x86_64-zlib `
  mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-libpng mingw-w64-x86_64-lzo `
  mingw-w64-x86_64-libvncserver mingw-w64-x86_64-cyrus-sasl `
  mingw-w64-x86_64-openldap mingw-w64-x86_64-pkgconf `
  git make nsis
```

### 2.2 QCA-Qt6（最麻烦的一步，必须自己做）

Veyon 要 `find_package(Qca-qt6 REQUIRED)` + `target_link_libraries(... qca-qt6)`，
**MSYS2 没有现成的 `mingw-w64-x86_64-qca-qt6` 包**。需要用同一套 Qt6/MinGW 从源码编译 QCA：

```bash
git clone https://invent.kde.org/utilities/qca.git
cd qca && mkdir build && cd build
/mingw64/bin/qt-cmake .. -G Ninja -DQT6=ON -DBUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/mingw64
ninja && ninja install
# 确认生成 /mingw64/lib/cmake/Qca-qt6/Qca-qt6Config.cmake
#        以及 /mingw64/bin/libqca-qt6.dll、/mingw64/lib/qca-qt6/crypto/libqca-ossl.dll
```

### 2.3 补齐 3rdparty 并编译

```bash
cd /g/veyon
# 与路线 A 相同，另加 Windows 专用：
git clone --depth 1 https://github.com/veyon/ultravnc.git veyon/3rdparty/ultravnc
# ... 其余同路线 A

cmake -S veyon -B build -G Ninja \
  -DCMAKE_C_COMPILER=/mingw64/bin/gcc -DCMAKE_CXX_COMPILER=/mingw64/bin/g++ \
  -DCMAKE_PREFIX_PATH=/mingw64 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DWITH_QT6=ON -DWITH_BUNDLED_LIBVNC=OFF \
  -DWITH_WEBAPI=OFF -DWITH_TRANSLATIONS=OFF \
  -DWITH_LTO=OFF -DWITH_PCH=OFF -DWITH_UNITY_BUILD=OFF \
  -DWITH_TESTS=OFF -DWITH_CORE_ONLY=OFF
cmake --build build --parallel 4
```

> `windows-binaries` / `create-windows-installer` 这两个 target 依赖 `${MINGW_PREFIX}`、
> `/usr/lib/gcc/<target>/<ver>-posix` 等写死的交叉编译路径（`WindowsInstaller.cmake:6`），
> 在原生 MSYS2 下路径不同，**建议先只做 `cmake --build build` 拿到 `classroom.dll`**，
> 运行时 DLL 按 `WindowsInstaller.cmake:29-62` 的清单手工收集，再用
> `scripts/package-classkeep.ps1` 组装教师端/学生端 ZIP。

### 2.4 组装产物

```powershell
cd G:\veyon
.\veyon\scripts\package-classkeep.ps1 -Version 1.0.1 `
    -TeacherDir <教师端运行时目录> -StudentDir <学生端运行时目录> `
    -ApkPath <APK路径> -OutputDir dist
```

脚本会校验两个目录都含 `classroom.dll`，缺一即报错，不产半成品。
详见 [`CLASSKEEP-RELEASE.md`](CLASSKEEP-RELEASE.md)。

---

## 3. 路线 C：在 GitHub 上自动出 Windows 产物

上游的做法是 Linux 容器里用 `x86_64-w64-mingw32` 交叉编译（`.ci/windows/build.sh`），
但它依赖的镜像 `ci-mingw-w64:main` 是 **GitLab 私有镜像且没有 Dockerfile**，外部无法复现。

要在 GitHub Actions 上做，需要二选一：

| 方式 | 成本 | 说明 |
|---|---|---|
| **自建 MinGW Docker 镜像** | 高 | 在 Dockerfile 里装 MinGW-w64、Qt6-MinGW、自编 QCA-Qt6、LibVNC/OpenSSL/LZO/JPEG/SASL/LDAP/interception、NSIS；推到 GHCR，再让 workflow 用它。相当于重建上游私有镜像 |
| **自托管 runner** | 中 | 一台按路线 B 配好的 Windows/MSYS2 机器注册为 self-hosted runner，workflow 里 `runs-on: self-hosted` 直接调现成工具链 |

**建议顺序**：先跑路线 A 把 30 多个 C++ 文件的编译错误全部消灭 → 再决定是否为 Windows 产物投入路线 C。
`classkeep-release.yml` 目前已改成「只发布能真实构建的东西」（Android APK + 两个部署预设包 + 校验），
**不再假装能编译 Windows，也不上传占位二进制**。

---

## 4. 常见错误对照

| 报错 | 原因 | 处理 |
|---|---|---|
| `Could not find a package configuration file provided by "Qca-qt6"` | QCA-Qt6 未装 / 不在 `CMAKE_PREFIX_PATH` | 路线 B 第 2.2 步；或路线 A 用 Qt5 的 `libqca-qt5-2-dev` |
| `LibVNCServer or LibVNCClient not found` | 系统 LibVNC < 0.9.14 | `-DWITH_BUNDLED_LIBVNC=ON` + clone `3rdparty/libvncserver` |
| `Cannot find source file .../3rdparty/kldap...` | 子模块为空 | 显式 clone（根仓库没有 `.gitmodules`） |
| `Qt6::GuiPrivate` / `Qt6::HttpServer` 找不到 | Qt 模块不全，或 Windows 下用了 Qt5 | Linux 用 `WITH_QT6=OFF`；Windows 装 `qt6-httpserver` 与 qtbase 私有头 |
| MSVC 报 `D9002 ignoring unknown option -Wno-attributes` | 用了 Visual Studio 生成器 | 必须 `-G Ninja` + MinGW GCC |
| `WITH_BUNDLED_LIBVNC=ON` 后 `ClassroomSelfTest` 不构建 | `CMakeLists.txt:199` 会强制 `WITH_TESTS=OFF` | 想跑自检需 `-DWITH_BUNDLED_LIBVNC=OFF` 且系统 LibVNC ≥ 0.9.14 |
| `INTERFACE_INCLUDE_DIRECTORIES ... is prefixed in the source directory` | bundled LibVNC 的 include 路径与 Veyon `install(TARGETS)` 冲突 | 加 `-DCMAKE_SKIP_INSTALL_RULES=ON`（只编译不安装） |
| `ninja: error: loading 'build.ninja': No such file or directory` | 上一条 CMake 错误导致未生成构建文件 | 看 `configure.log`；同时确认 `tee` 步骤有 `set -o pipefail` |
| `sdkmanager failed with exit code 1`（Android CI） | `android-actions/setup-android@v3` 覆盖 runner 预装的 cmdline-tools 版本 | 去掉该 action，用 runner 自带 SDK + `sdkmanager --licenses`，让 AGP 自动装 platform |
| `classroom.dll` 没生成 | `-DWITH_CORE_ONLY=ON` 跳过了 `plugins/` | 设为 OFF；`plugins/CMakeLists.txt` 是 GLOB，改插件后需重新跑 CMake |
| `Qt::Literals / StringLiterals / _L1 / QLatin1StringView` 未定义（在 `3rdparty/kldap-qt5` 里） | clone 了 KDE kldap 最新 master（Qt6-only），但 `WITH_QT6=OFF` 需要 Qt5 版本 | 按上游 v4.11.4 的 gitlink SHA 精确检出 `kldap-qt5`（见上文 fetch 脚本） |
| `qsizepolicy.h: bitwise operation between different enumeration types ... [-Werror=deprecated-enum-enum-conversion]` | Ubuntu 22.04 的 GCC 11+ 对 Qt5 头文件产生新警告，被 Veyon 的 `-Werror` 放大；连上游 `AboutDialog.cpp` 都失败 | 加 `-DCMAKE_CXX_FLAGS="-Wno-deprecated-enum-enum-conversion"`，或改用 Debian 11 / GCC 10 容器 |
| 校验步骤说插件没生成，其实生成了 | `find build -name A -o -name B` 的 `-o` 会让 `-print` 只作用于最后一个表达式 | 写成 `find build \( -name A -o -name B \) -print` |

---

## 5. 已修正的坑（本轮）

1. 新建仓库根 `.github/workflows/classkeep-build.yml` — **真实编译验证**（Linux/GCC + Android）。
2. 新建仓库根 `.github/workflows/classkeep-release.yml` — 只发布能真实构建的东西。
3. 删除 `veyon/.github/workflows/classkeep-release.yml`（位置错误 + 基于不可行的 `vcpkg install qca` + MSVC）。
4. build workflow 显式 clone 5 个 3rdparty 源码，绕开根仓库缺 `.gitmodules`。
5. 文档不再声称「MSVC 可用」。

> 上游 `veyon/.github/workflows/build.yml` 和 `release.yml` 保留原样，
> 但它们同样位于子目录，在本仓库结构下**不会被触发**；不要依赖它们做验证。
