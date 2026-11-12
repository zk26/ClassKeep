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

一句话：**Windows 二进制只能用 MinGW-w64/GCC 编译，MSVC + vcpkg 是死路。**
目前仓库提供三条可立即运行的 GitHub Actions 路线：

| Workflow | 作用 | 当前状态 |
|---|---|---|
| **ClassKeep Build Check** | Linux/Qt6/GCC 全量验证课守插件，产出 `classroom.so` | ✅ 已跑通，两个 job 全绿 |
| **ClassKeep Windows Build** | MSYS2 UCRT64/MinGW 产出真实 Windows `.exe`/`.dll` | ✅ 已跑通 |
| **ClassKeep Release** | 汇总 APK + 教师端/学生端包，自动发 Release | 见 [`CLASSKEEP-RELEASE.md`](CLASSKEEP-RELEASE.md) |

---

## 1. 路线 A（推荐，现在就能做）：GitHub Actions 上真实编译

已写好 [`.github/workflows/classkeep-build.yml`](../../.github/workflows/classkeep-build.yml)。
它做两件事：

| Job | 环境 | 做什么 |
|---|---|---|
| `desktop-linux` | `ubuntu-24.04` + 上游 Ubuntu 24.04 那套 **Qt6**/QCA 依赖 | **全量编译 Veyon + 课守插件**，产出 `classroom.so` 与完整编译日志 |
| `android` | `ubuntu-latest` + JDK 21 | `:app:assembleDebug` + `:app:testDebugUnitTest`（本机已验证可行） |

### 你现在的操作

1. 把本次改动推到 `main`（已推送 / 或你自己 push）；
2. 打开 <https://github.com/zk26/ClassKeep/actions>；
3. 选 **ClassKeep Build Check** → **Run workflow**（`main` 分支）；
4. 看 `desktop-linux` 的 `Build` 日志。**这一步会真正把所有 C++ 文件交给 GCC**，
   静态复核发现不了的错误（缺 include、签名不匹配、模板推导）都会在这里暴露；
5. 把报错日志贴给我，我逐个修，直到 `BUILD SUCCESS`。

### 它用的确切配置

> **Qt6 是正确路线**：Windows 平台插件无条件 `find_package(Qt6 GuiPrivate)`，课守代码也使用 Qt6 API。
> 依赖清单照搬上游 `.ci/linux.ubuntu.24.04/Dockerfile`。
>
> ⚠️ 但**不要跑全量 `cmake --build build`**：这份 4.11.4 上游快照里的 `plugins/filetransfer`
> 在 **Qt5 和 Qt6 下都编不过**（`FileCollection.h` 缺 `#include <QUuid>`、
> `qApp->setQuitOnLastWindowClosed` 在 `QCoreApplication` 上不存在），与课守无关。
> 因此 CI 只编译 `--target classroom`——它会连带编译课守依赖的 `veyon-core` 等，
> 但不会去碰那个损坏的上游插件。

```bash
# 依赖（Ubuntu 24.04 / noble，Qt6）
sudo apt-get install -y build-essential cmake ninja-build git pkg-config \
  qt6-base-dev qt6-5compat-dev qt6-l10n-tools qt6-tools-dev \
  qt6-declarative-dev qt6-httpserver-dev qt6-websockets-dev \
  libqca-qt6-dev libqca-qt6-plugins \
  libssl-dev zlib1g-dev libjpeg-dev libpng-dev liblzo2-dev \
  libpam0g-dev libproc2-dev libldap2-dev libsasl2-dev \
  libfakekey-dev xorg-dev libvncserver-dev \
  libpipewire-0.3-dev libspa-0.2-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev

# 3rdparty（根仓库没有 .gitmodules，必须显式 clone）
# 必须检出上游 veyon/veyon v4.11.4 锁定的 commit，直接 clone HEAD 会拉到不同版本的 kldap 而编译失败。
fetch() {
  git init -q "veyon/3rdparty/$1"
  git -C "veyon/3rdparty/$1" remote add origin "$2"
  git -C "veyon/3rdparty/$1" fetch -q --depth 1 origin "$3"
  git -C "veyon/3rdparty/$1" checkout -q FETCH_HEAD
}
fetch libvncserver https://github.com/veyon/libvncserver.git 30860286c0f55674a846df9cab458da9afd1735b
fetch x11vnc       https://github.com/veyon/x11vnc.git       c1e38369e2c601df7ab894c378df0cc2380a1ede
fetch libfakekey   https://github.com/veyon/libfakekey.git    7ad885912efb2131e80914e964d5e635b0d07b40
fetch kldap        https://invent.kde.org/pim/kldap.git      2002b2c4ec81d05eb0fc5b2106ef112d404f5cea

# SHA 来源：https://api.github.com/repos/veyon/veyon/git/trees/v4.11.4?recursive=1
# Qt5 路线才用 kldap-qt5(27bf9afb...)；qthttpserver / ultravnc 只在 WebAPI(Qt5回退) / Windows 里用。
```

```bash
cmake -S veyon -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_QT6=ON -DWITH_BUNDLED_LIBVNC=ON \
  -DWITH_WEBAPI=OFF -DWITH_TRANSLATIONS=OFF \
  -DWITH_LTO=OFF -DWITH_PCH=OFF -DWITH_UNITY_BUILD=OFF \
  -DWITH_STACK_PROTECTION=OFF -DWITH_TESTS=OFF -DWITH_CORE_ONLY=OFF \
  -DCMAKE_SKIP_INSTALL_RULES=ON \
  -DCMAKE_CXX_FLAGS="-include cmath"
cmake --build build --target classroom --parallel 4
```

`-DCMAKE_CXX_FLAGS="-include cmath"` 是因为上游 `core/src/Toast.cpp` 用了 `floor()` / `ceil()`
却没包含 `<cmath>`，旧 Qt 会间接带入、新 Qt/GCC 不会——属于上游的环境差异，不是课守代码问题。

**Qt5 路线已实测失败，不要再试**：Ubuntu 22.04 + Qt5 会依次遇到
`qsizepolicy.h` 的 `-Werror=deprecated-enum-enum-conversion`、`Toast.cpp` 缺 `<cmath>`、
kldap master 的 Qt6-only 符号。

`plugins/filetransfer` 的失败**与 Qt 版本无关**——Qt5 和 Qt6 下报的是同一批错误，
是这份上游快照自身的缺陷；用 `--target classroom` 绕开即可，不需要（也不应该）为了全量编译去改上游代码。

`-DCMAKE_SKIP_INSTALL_RULES=ON` 是必需的：bundled LibVNC 的 `vncclient` / `vncserver`
把源码目录写进了 `INTERFACE_INCLUDE_DIRECTORIES`，Veyon 的 `install(TARGETS ...)`
会在 generate 阶段直接报 `... is prefixed in the source directory` 并拒绝生成 `build.ninja`。
只做编译验证时跳过 install 规则即可，不需要改上游源码。

> CI 里所有 `... | tee xxx.log` 的步骤都加了 `set -o pipefail`，否则 `tee` 的退出码
> 会掩盖 cmake/ninja 的失败（第一次运行就踩过这个坑，日志里出现
> `ninja: error: loading 'build.ninja'` 却把 Build 步骤标成 success）。

参数解释：

- `WITH_QT6=ON`：**必须**。`plugins/filetransfer` 等上游插件是 Qt6-only；Windows 平台插件还无条件 `find_package(Qt6 GuiPrivate)`。
- `WITH_BUNDLED_LIBVNC=ON`：免掉系统 `libvncserver >= 0.9.14`（发行版自带版本偏低）。
  注意它会连带把 `WITH_TESTS` 置 OFF（`CMakeLists.txt:199`），所以 `ClassroomSelfTest` 在这个配置下不会构建。
- `WITH_WEBAPI=OFF` / `WITH_TRANSLATIONS=OFF`：免 `Qt6::HttpServer` 与 LinguistTools 的额外要求；课守 Bridge 只用 `QTcpServer/QUdpSocket`，不依赖 WebAPI。
- `WITH_UNITY_BUILD=OFF`：让每个 `.cpp` 单独编译，最容易暴露真实错误。
- `WITH_CORE_ONLY=OFF`：**必须**，否则整个 `plugins/`（含课守）都不会构建。

### ⚠️ 路线 A 验证不到什么

`ClassroomSystem.cpp` 里 UWF / Core Audio / WFP / SetupAPI 的实现都在 `#ifdef Q_OS_WIN` 内，
Linux 编译走 `#else` 分支，**不会验证这些 Windows 专属代码**。
它们需要路线 B（本机 MinGW）或路线 D（GitHub Windows 构建）才能真正确认。
其余 30 多个文件（协议、积分、词云、报表、Bridge、学生端 UI、插件分发）都能被路线 A 覆盖。

---

## 路线 D：GitHub Actions 产出 Windows 二进制（推荐用于出包）

workflow：`.github/workflows/classkeep-windows.yml` → **ClassKeep Windows Build**

| 项 | 值 |
|---|---|
| Runner | `windows-latest` + `msys2/setup-msys2@v2` |
| 环境 | **UCRT64**（不是 MINGW64——MSYS2 的 QCA / LibVNC 只发布到 ucrt64 / clang64） |
| 编译器 | `mingw-w64-ucrt-x86_64-gcc` + Ninja |
| Qt | `mingw-w64-ucrt-x86_64-qt6-base` 等，用 `cmake -DCMAKE_PREFIX_PATH=/ucrt64` 配置（MSYS2 没有 `qt-cmake` wrapper） |
| 关键包名 | `qca-qt6`、`openldap`、`cyrus-sasl`、`ntldd`、`nsis`、`python`（**不是** `qca` / `libldap` / `libsasl`） |
| 产物 | `classkeep-windows-setups` artifact：教师端 / 学生端两个 `*-setup.exe`（内含完整 DLL 闭包） |

设计要点：

- 3rdparty 仍按上游 v4.11.4 的 gitlink SHA 精确检出，另加 Windows 需要的 `ultravnc` / `qthttpserver`。
- **不调用上游 `windows-binaries` target**：`WindowsInstaller.cmake` 硬编码了 Linux 交叉编译路径
  （`/usr/lib/gcc/${MINGW_TARGET}/...`、`${MINGW_PREFIX}/bin`），且会触发全量编译撞上损坏的 `filetransfer` 插件。
  这里改为显式 `--target classroom` 并自行收集 DLL。
- `classroom` 目标会连带编译 `veyon-core`，因此 **UWF / Core Audio / WFP 这些 Windows 专属实现会被真正编译**。

实际产出（已验证）：

```
plugins/classroom.dll + 其它 Veyon 插件 DLL
veyon-core.dll
ClassKeep-master.exe / ClassKeep-configurator.exe / ClassKeep-cli.exe / ClassKeep-wcli.exe
ClassKeep-service.exe / ClassKeep-server.exe / ClassKeep-worker.exe
Qt6*.dll / libqca-qt6.dll / libbz2-1.dll / libmd4c.dll / CRT 运行时（递归闭包）
platforms/ styles/ tls/ imageformats/ crypto/               Qt 与 QCA 插件
LICENSE.TXT / COPYING.TXT / README.TXT / ClassKeep.ico
```

运行时 DLL 由 `scripts/collect_mingw_deps.py` **用 `ntldd` 递归解析 PE 导入闭包**收集，
不再使用文件名白名单。旧版白名单漏掉 `libbz2-1.dll`、`libmd4c.dll`、`libbrotli*.dll`，
安装后所有 EXE 都报「找不到 DLL」——这是 `v1.3.0` 修复的核心问题。

`classkeep-release.yml` 内联同一套 Windows 步骤，再用 NSIS 生成
`ClassKeep-Teacher-<ver>-win64-setup.exe` 与 `ClassKeep-Student-<ver>-win64-setup.exe`
（详见 [`CLASSKEEP-RELEASE.md`](CLASSKEEP-RELEASE.md) 与 `scripts/README.md`）。

---

## 2. 路线 B：本机 Windows 编译出真正的教师端 / 学生端

需要 **MSYS2 + MinGW-w64 GCC**（不是 Visual Studio）。
**最简单的做法是照抄路线 D 的 workflow**——那份配置已在 GitHub Actions 上实测跑通，
直接在本机 MSYS2 UCRT64 里执行同样的命令即可。

### 2.1 准备工具链（UCRT64）

```bash
# 用 MSYS2 UCRT64 终端（不是 MINGW64）：QCA / LibVNC 只发布到 ucrt64 / clang64
pacman -Syu
pacman -S --needed \
  git base-devel \
  mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-pkgconf \
  mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-5compat \
  mingw-w64-ucrt-x86_64-qt6-tools mingw-w64-ucrt-x86_64-qt6-httpserver \
  mingw-w64-ucrt-x86_64-qt6-websockets \
  mingw-w64-ucrt-x86_64-qca-qt6 \
  mingw-w64-ucrt-x86_64-libvncserver mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-zlib mingw-w64-ucrt-x86_64-libjpeg-turbo \
  mingw-w64-ucrt-x86_64-libpng mingw-w64-ucrt-x86_64-lzo2 \
  mingw-w64-ucrt-x86_64-openldap mingw-w64-ucrt-x86_64-cyrus-sasl
```

> 实测踩过的坑：包名是 **`qca-qt6` / `openldap` / `cyrus-sasl` / `lzo2`**，
> 不是 `qca` / `libldap` / `libsasl` / `lzo`；MSYS2 也没有 `qt-cmake` wrapper，
> 用普通 `cmake -DCMAKE_PREFIX_PATH=/ucrt64` 即可。

### 2.2 QCA-Qt6 不需要自己编译

早期判断有误：MSYS2 UCRT64 **有现成的 `mingw-w64-ucrt-x86_64-qca-qt6` 包**，
它会提供 `Qca-qt6Config.cmake`、`libqca-qt6.dll` 与 `crypto/libqca-ossl.dll`。
只有在使用非 MSYS2 的 Qt6（例如官方 Qt 安装器）时才需要从源码编 QCA。

### 2.3 补齐 3rdparty 并编译

按路线 A/D 的 `fetch()` 脚本检出上游 v4.11.4 锁定的 3rdparty
（Windows 另需 `ultravnc`、`qthttpserver`），然后在 UCRT64 终端：

```bash
cmake -S veyon -B build -G Ninja \
  -DCMAKE_PREFIX_PATH=/ucrt64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_QT6=ON -DWITH_BUNDLED_LIBVNC=ON \
  -DWITH_WEBAPI=OFF -DWITH_TRANSLATIONS=OFF \
  -DWITH_LTO=OFF -DWITH_PCH=OFF -DWITH_UNITY_BUILD=OFF \
  -DWITH_STACK_PROTECTION=OFF -DWITH_TESTS=OFF -DWITH_CORE_ONLY=OFF \
  -DCMAKE_SKIP_INSTALL_RULES=ON \
  -DCMAKE_CXX_FLAGS="-include cmath"
cmake --build build --parallel 4 --target \
  classroom ClassKeep-master ClassKeep-configurator ClassKeep-cli \
  ClassKeep-service ClassKeep-server ClassKeep-worker
```

> **不要跑全量 `cmake --build build`**（会撞上损坏的上游 `filetransfer` 插件），
> 也不要用 `windows-binaries` / `create-windows-installer` target：
> `WindowsInstaller.cmake` 写死了 Linux 交叉编译路径（`${MINGW_PREFIX}`、
> `/usr/lib/gcc/<target>/<ver>-posix`），原生 MSYS2 下不适用。
> 运行时 DLL 与 Qt 插件的收集由
> [`collect-windows-runtime.sh`](../scripts/collect-windows-runtime.sh) 完成。

### 2.4 生成安装程序

```bash
cd /g/veyon
bash veyon/scripts/collect-windows-runtime.sh
mkdir -p dist
{ printf '\xEF\xBB\xBF'; cat veyon/scripts/classkeep-installer.nsi; } > installer.nsi
makensis -DVERSION=2.1.0 -DROLE=teacher installer.nsi
makensis -DVERSION=2.1.0 -DROLE=student installer.nsi
```

`collect-windows-runtime.sh` 会校验 `plugins/classroom.dll`、`ClassKeep-wcli.exe`、
`platforms/qwindows.dll` 等关键文件，缺一即报错，不产半成品。
详见 [`CLASSKEEP-RELEASE.md`](CLASSKEEP-RELEASE.md) 与 [`../scripts/README.md`](../scripts/README.md)。

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
| `Could not find a package configuration file provided by "Qca-qt6"` | QCA-Qt6 未装 / 不在 `CMAKE_PREFIX_PATH` | 路线 A 装 `libqca-qt6-dev`；路线 B 见第 2.2 步自编 |
| `LibVNCServer or LibVNCClient not found` | 系统 LibVNC < 0.9.14 | `-DWITH_BUNDLED_LIBVNC=ON` + clone `3rdparty/libvncserver` |
| `Cannot find source file .../3rdparty/kldap...` | 子模块为空 | 显式 clone（根仓库没有 `.gitmodules`） |
| `Qt6::GuiPrivate` / `Qt6::HttpServer` 找不到 | Qt6 模块不全 | 装 `qt6-base-private-dev` / `qt6-httpserver-dev`；或 `-DWITH_WEBAPI=OFF`。**不要退回 Qt5** |
| MSVC 报 `D9002 ignoring unknown option -Wno-attributes` | 用了 Visual Studio 生成器 | 必须 `-G Ninja` + MinGW GCC |
| `WITH_BUNDLED_LIBVNC=ON` 后 `ClassroomSelfTest` 不构建 | `CMakeLists.txt:199` 会强制 `WITH_TESTS=OFF` | 想跑自检需 `-DWITH_BUNDLED_LIBVNC=OFF` 且系统 LibVNC ≥ 0.9.14 |
| `INTERFACE_INCLUDE_DIRECTORIES ... is prefixed in the source directory` | bundled LibVNC 的 include 路径与 Veyon `install(TARGETS)` 冲突 | 加 `-DCMAKE_SKIP_INSTALL_RULES=ON`（只编译不安装） |
| `ninja: error: loading 'build.ninja': No such file or directory` | 上一条 CMake 错误导致未生成构建文件 | 看 `configure.log`；同时确认 `tee` 步骤有 `set -o pipefail` |
| `sdkmanager failed with exit code 1`（Android CI） | `android-actions/setup-android@v3` 覆盖 runner 预装的 cmdline-tools 版本 | 去掉该 action，用 runner 自带 SDK + `sdkmanager --licenses`，让 AGP 自动装 platform |
| `classroom.dll` 没生成 | `-DWITH_CORE_ONLY=ON` 跳过了 `plugins/` | 设为 OFF；`plugins/CMakeLists.txt` 是 GLOB，改插件后需重新跑 CMake |
| `plugins/filetransfer`: `QUuid` incomplete / `QCoreApplication::setQuitOnLastWindowClosed` 不存在 | 上游该插件在 **Qt5 与 Qt6 下都编不过**（缺 `#include <QUuid>`、`qApp` 类型不对），与课守无关 | 用 `cmake --build build --target classroom` 只编译课守目标；不要跑全量 `cmake --build build` |
| `Qt::Literals / StringLiterals / _L1 / QLatin1StringView` 未定义（在 `3rdparty/kldap-qt5` 里，仅 Qt5 路线） | clone 了 KDE kldap 最新 master 而非锁定版本 | 按 v4.11.4 的 gitlink SHA 精确检出；或直接用本方案的 Qt6 `kldap` |
| `qsizepolicy.h: bitwise operation between different enumeration types ... [-Werror=deprecated-enum-enum-conversion]` | Ubuntu 22.04 的 GCC 11+ 对 Qt5 头文件产生新警告，被 Veyon 的 `-Werror` 放大；连上游 `AboutDialog.cpp` 都失败 | 加 `-DCMAKE_CXX_FLAGS="-Wno-deprecated-enum-enum-conversion"`，或改用 Debian 11 / GCC 10 容器 |
| 校验步骤说插件没生成，其实生成了 | `find build -name A -o -name B` 的 `-o` 会让 `-print` 只作用于最后一个表达式 | 写成 `find build \( -name A -o -name B \) -print` |
| 安装/卸载弹「ClassKeep-wcli.exe - 快速异常检测失败」，程序也全打不开 | `windows-platform.dll` 没编出来（`-linterception` 只存在于上游私有镜像），`VeyonCore::initPlatformPlugin()` 触发 `qFatal("no platform plugin available!")` → UCRT `__fastfail` | 用自制 `3rdparty/interception/include/interception.h` 桩替代 `-linterception`；把 `windows-platform` 设为**必需**构建目标；`collect-windows-runtime.sh` 硬校验该 DLL |
| 同上但平台插件在，仍崩 | `crypto/libqca-ossl.dll` 缺失或挑到了 qca-qt5 版本，`CryptoCore.cpp:39` → `qFatal("RSA not supported!")` | 从精确路径 `/ucrt64/lib/qca-qt6/crypto/libqca-ossl.dll` 复制，并硬校验 |
| `-linterception` 找不到（MSYS2 UCRT64） | 该库既不在上游 `3rdparty/interception/`（只有安装器 exe + bat），也不在 MSYS2 | 使用上述自制最小兼容头，去掉链接参数；Interception 驱动是可选的，默认不启用 |

---

## 5. 已修正的坑（本轮）

1. 新建仓库根 `.github/workflows/classkeep-build.yml` — **真实编译验证**（Linux/GCC + Android）。
2. 新建仓库根 `.github/workflows/classkeep-release.yml` — 只发布能真实构建的东西。
3. 删除 `veyon/.github/workflows/classkeep-release.yml`（位置错误 + 基于不可行的 `vcpkg install qca` + MSVC）。
4. build workflow 显式 clone 5 个 3rdparty 源码，绕开根仓库缺 `.gitmodules`。
5. 文档不再声称「MSVC 可用」。

> 上游 `veyon/.github/workflows/build.yml` 和 `release.yml` 保留原样，
> 但它们同样位于子目录，在本仓库结构下**不会被触发**；不要依赖它们做验证。
