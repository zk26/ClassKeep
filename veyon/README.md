# Veyon - Virtual Eye On Networks

> ## 课守 ClassKeep — 桌面端总入口
>
> 本目录是仓库 <https://github.com/zk26/ClassKeep> 的**桌面端子目录** `veyon/`。
> 仓库根（总 README、LICENSE、NOTICE、Android 工程）在上一级，见 [`../README.md`](../README.md)。
>
> 本分支是 **Veyon 4.x 的教学管理定制分支**，在保留 Veyon 原有监控/控屏/演示能力的基础上，
> 增加了班级与 IP 绑定、课堂密钥防退出、全量行为日志与兴趣词云、积分制与期末报表、
> UWF 还原与远程装软件、本地 AI 助手、Android 教室端 App 等能力。
>
> ### 目录结构
>
> | 路径 | 内容 |
> |---|---|
> | `plugins/classroom/` | 课守插件：教师端控制台、学生端悬浮客户端、日志/积分/词云、Bridge 服务 |
> | `mobile/` | Android 教室端 App（Kotlin，零第三方网络库） |
> | `docs/` | 协议、部署、发布、许可证全套说明 |
> | `.github/workflows/` | 发布构建流水线 |
> | `scripts/` | 发布包打包脚本 |
> | `core/`、`master/`、`server/` | 对上游的最小扩展点 |
>
> ### 三类发布产物
>
> | 产物 | 运行位置 |
> |---|---|
> | **教师端** | 教师机 Windows（Master + Configurator + 课守插件 + Bridge） |
> | **学生端** | 学生机 Windows（Service/Server/Worker + 课守插件，UWF 还原环境） |
> | **APP** | Android 11+ 教室端 |
>
> 产物清单、命名、校验与安装见 [docs/CLASSKEEP-RELEASE.md](docs/CLASSKEEP-RELEASE.md)。
> 当前仓库**不含预编译二进制**，三类产物由发布工作流在推送 `v*` tag 时自动构建并上传到 Releases。
>
> ### 文档索引
>
> | 文档 | 内容 |
> |---|---|
> | [`../README.md`](../README.md) | **仓库总入口**（整个项目的目录结构与全部说明索引） |
> | [`../NOTICE.md`](../NOTICE.md) | 版权归属、修改声明、商标、隐私与无担保声明 |
> | [`../LICENSE`](../LICENSE) | GNU GPL v2 全文 |
> | [docs/CLASSKEEP-FEATURES.md](docs/CLASSKEEP-FEATURES.md) | **全部功能验收总表**（逐条对照需求 / 实现位置 / 入口 / 未完成项） |
> | [docs/CLASSKEEP.md](docs/CLASSKEEP.md) | 功能总览、架构、目录结构与构建 |
> | [docs/CLASSKEEP-BUILD.md](docs/CLASSKEEP-BUILD.md) | **真实编译方案**（GitHub Actions / MinGW / 排错） |
> | [docs/CLASSKEEP-PROTOCOL.md](docs/CLASSKEEP-PROTOCOL.md) | FeatureMessage 命令表、Bridge HTTP/UDP 协议、认证算法、数据文件格式 |
> | [docs/CLASSKEEP-DEPLOY.md](docs/CLASSKEEP-DEPLOY.md) | 教师机/学生机/手机端部署、UWF 与 Defender 注意事项 |
> | [docs/CLASSKEEP-RELEASE.md](docs/CLASSKEEP-RELEASE.md) | 发布产物清单与打包规范 |
> | [docs/CLASSKEEP-UI.md](docs/CLASSKEEP-UI.md) | APP 交互与视觉规范、平板适配验收清单 |
> | [docs/CLASSKEEP-PRIVACY.md](docs/CLASSKEEP-PRIVACY.md) | **数据采集范围与合规义务**（部署前必读） |
> | [docs/CLASSKEEP-THIRD-PARTY.md](docs/CLASSKEEP-THIRD-PARTY.md) | 第三方组件与许可清单 |
> | [docs/CLASSKEEP-LICENSE.md](docs/CLASSKEEP-LICENSE.md) | 许可证与衍生作品声明 |
> | [plugins/classroom/README.md](plugins/classroom/README.md) | 课守插件源码结构 |
> | [scripts/README.md](scripts/README.md) | 依赖闭包收集、NSIS 安装器与图标生成脚本 |
> | [`../mobile/README.md`](../mobile/README.md) | Android 教室端工程说明 |
> | [`../.github/workflows/`](../.github/workflows/) | 编译验证 / Windows 构建 / 发布流水线 |
>
> 本项目继承上游 **GPL-2.0-or-later**（见 `COPYING` 与 `../LICENSE`）。
> 上游项目主页：https://veyon.io/ — 本分支与上游 Veyon 项目无隶属、背书或合作关系。
>
> Android 工程在仓库根 [`../mobile/`](../mobile/)（唯一一份）；桌面端图标由根目录
> [`../ClassKeep_logo.png`](../ClassKeep_logo.png) 生成，产品名统一为「课守 ClassKeep」。

[![.github/workflows/build.yml](https://github.com/veyon/veyon/actions/workflows/build.yml/badge.svg?branch=4.5)](https://github.com/veyon/veyon/actions/workflows/build.yml)
[![Latest stable release](https://img.shields.io/github/release/veyon/veyon.svg?maxAge=3600)](https://github.com/veyon/veyon/releases)
[![Overall downloads on Github](https://img.shields.io/github/downloads/veyon/veyon/total.svg?maxAge=3600)](https://github.com/veyon/veyon/releases)
[![Documentation Status](https://readthedocs.org/projects/veyon/badge/?version=latest)](https://docs.veyon.io/)
[![Localise on Transifex](https://img.shields.io/badge/localise-on_transifex-green.svg)](https://app.transifex.com/veyon-solutions/veyon/)
[![license](https://img.shields.io/badge/license-GPLv2-green.svg)](LICENSE)


## What is Veyon?

Veyon is a free and open source software for monitoring and controlling
computers across multiple platforms. Veyon supports you in teaching in digital
learning environments, performing virtual trainings or giving remote support.

The following features are available in Veyon:

  * Overview: monitor all computers in one or multiple locations or classrooms
  * Remote access: view or control computers to watch and support users
  * Demo: broadcast the teacher's screen in realtime (fullscreen/window)
  * Screen lock: draw attention to what matters right now
  * Communication: send text messages to students
  * Start and end lessons: log in and log out users all at once
  * Screenshots: record learning progress and document infringements
  * Programs & websites: launch programs and open website URLs remotely
  * Teaching material: distribute and open documents, images and videos easily
  * Administration: power on/off and reboot computers remotely


## License

Copyright (c) 2004-2026 Tobias Junghans / Veyon Solutions.

See the file COPYING for the GNU GENERAL PUBLIC LICENSE.


## Installation and configuration

Please refer to the official Veyon Administrator Manual at https://docs.veyon.io/en/latest/admin/index.html
for information on the installation and configuration of Veyon.


## Usage

Please refer to the official Veyon User Manual at https://docs.veyon.io/en/latest/user/index.html
for information on how to use Veyon.


## Veyon on Linux

### Downloading sources

First grab the latest sources by cloning the Git repository and fetching all submodules:

	git clone --recursive https://github.com/veyon/veyon.git && cd veyon


### Installing dependencies

Requirements for Debian-based distributions:

- Build tools: g++ libc6-dev make cmake dpkg-dev
- Qt5: qtbase5-dev qtbase5-private-dev qtbase5-dev-tools qttools5-dev qttools5-dev-tools
- X11: xorg-dev libxtst-dev libfakekey-dev
- libjpeg: libjpeg-dev provided by libjpeg-turbo8-dev or libjpeg62-turbo-dev
- zlib: zlib1g-dev
- OpenSSL: libssl-dev
- PAM: libpam0g-dev
- procps: libprocps-dev
- LZO: liblzo2-dev
- QCA: libqca-qt5-2-dev
- LDAP: libldap2-dev
- SASL: libsasl2-dev

As root you can run

	apt install g++ libc6-dev make cmake qtbase5-dev qtbase5-private-dev \
	            qtbase5-dev-tools qttools5-dev qttools5-dev-tools \
	            xorg-dev libxtst-dev libfakekey-dev libjpeg-dev zlib1g-dev libssl-dev libpam0g-dev \
	            libprocps-dev liblzo2-dev libqca-qt5-2-dev libldap2-dev \
	            libsasl2-dev



Requirements for RedHat-based distributions:

- Build tools: gcc-c++ make cmake rpm-build
- Qt5: qt5-devel qt5-qtbase-private-devel
- X11: libXtst-devel libXrandr-devel libXinerama-devel libXcursor-devel libXrandr-devel libXdamage-devel libXcomposite-devel libXfixes-devel libfakekey-devel
- libjpeg: libjpeg-turbo-devel
- zlib: zlib-devel
- OpenSSL: openssl-devel
- PAM: pam-devel
- procps: procps-devel
- LZO: lzo-devel
- QCA: qca-devel qca-qt5-devel
- LDAP: openldap-devel
- SASL: cyrus-sasl-devel

As root you can run

	dnf install gcc-c++ make cmake rpm-build qt5-devel libXtst-devel libXrandr-devel libXinerama-devel libXcursor-devel \
             libXrandr-devel libXdamage-devel libXcomposite-devel libXfixes-devel libjpeg-turbo-devel zlib-devel \
             openssl-devel pam-devel procps-devel lzo-devel qca-devel qca-qt5-devel openldap-devel cyrus-sasl-devel


### Configuring and building sources

Run the following commands:

	mkdir build
	cd build
	cmake ..
	make -j4

NOTE: If you want to build a .deb or .rpm package for this software, instead of the provided cmake command, you should use:

	cmake -DCMAKE_INSTALL_PREFIX=/usr ..

to install package files in /usr instead of /usr/local.

If some requirements are not fullfilled, CMake will inform you about it and
you will have to install the missing software before continuing.

You can now generate a package (.deb or .rpm depending what system you are in).

For generating a package you can run

	fakeroot make package

Then you'll get something like veyon_x.y.z_arch.deb or veyon-x.y.z.arch.rpm

Alternatively you can install the built binaries directly (not recommended for
production systems) by running the following command as root:

	make install

### Arch linux

A PKGBUILD can be found in the [AUR](https://aur.archlinux.org/packages/veyon/).

### PPA

This PPA contains official Veyon packages for Ubuntu suitable for use both on desktop computers and ARM boards (e.g. Raspberry Pi). Even though only packages for LTS releases are available they should work for subsequent non-LTS releases as well.

	sudo add-apt-repository ppa:veyon/stable
	sudo apt-get update

## Join development

If you are interested in Veyon, its programming, artwork, testing or something like that, you're welcome to participate in the development of Veyon!

Before starting the implementation of a new feature, please always open an issue at https://github.com/veyon/veyon/issues to start a discussion about your intended implementation. There may be different ideas, improvements, hints or maybe an already ongoing work on this feature.

## Join translation team

Veyon and its documentation are translated at the Transifex platform. Please go to https://app.transifex.com/veyon-solutions/veyon and join the corresponding translation team. Please DO NOT submit pull requests for modified translation files since this would require manual Transifex synchronizations on our side.

## More information

* https://veyon.io/
* https://docs.veyon.io/
