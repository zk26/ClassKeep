# 课守 ClassKeep 许可证与衍生作品声明

## 1. 上游许可

本项目基于 [Veyon](https://veyon.io/)（`https://github.com/veyon/veyon`），
上游以 **GNU General Public License v2.0 or later (GPL-2.0-or-later)** 发布，
许可证全文见仓库根目录 [`COPYING`](../COPYING)。

Veyon 官方源码文件头的版权声明：

> Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
> This program is free software; you can redistribute it and/or modify it under
> the terms of the GNU General Public License as published by the Free Software
> Foundation; either version 2 of the License, or (at your option) any later version.

## 2. 本分支的性质

课守 ClassKeep 是 Veyon 的**修改/衍生作品**（fork + 定制），不是 Veyon 官方版本，
与 Veyon 项目及其作者无隶属、背书或合作关系。

按 GPL 第 2 条与第 3 条要求，本仓库**整体**继续以 **GPL-2.0-or-later** 授权：

```
SPDX-License-Identifier: GPL-2.0-or-later
```

该声明覆盖本分支新增与修改的全部文件，包括但不限于：

- `plugins/classroom/` 下所有 C++/Qt 源文件（课守插件）
- `mobile/` 下的 Android Kotlin 工程（教室端 App）
- `core/src/FeatureProviderInterface.h`、`core/src/FeatureManager.h/.cpp`、
  `server/src/ComputerControlServer.cpp`、`master/src/ComputerMonitoringWidget.cpp` 的扩展改动
- `docs/` 下的文档

修改声明（GPL 第 2(a) 条）：本分支相对上游的改动记录在本仓库 git 历史中，
总览见 [`CLASSKEEP.md`](CLASSKEEP.md)，协议见 [`CLASSKEEP-PROTOCOL.md`](CLASSKEEP-PROTOCOL.md)。

## 3. 分发义务

若你分发本分支的二进制或源码，必须：

1. 随附 `COPYING`（GPL v2 全文）与本文档；
2. 提供对应完整源代码，或按 GPL 第 3 条给出有效的书面源码获取方式；
3. 保留所有原有版权与许可证声明；
4. 明确标注文件被修改及修改日期（git 历史 + 本文件即承担该说明）；
5. 不得对本软件附加任何进一步限制，也不提供任何担保（GPL 第 11、12 条）。

桌面端与 Veyon 核心链接为同一程序，因此**整体必须按 GPL-2.0-or-later 提供源码**。
`mobile/` 中的 Android App 通过标准 HTTP/UDP 与教师机通信，属独立进程；
本仓库仍按同一许可证发布以保持一致，若日后单独分发，需自行确认其依赖与分发方式符合相应许可。

## 4. 第三方素材与参考项目

| 项目 | 关系 | 说明 |
|---|---|---|
| [Veyon](https://github.com/veyon/veyon) | **上游代码**，GPL-2.0-or-later | 本仓库是其 fork，保留全部原有版权声明 |
| `G:/UWFTool`（本地参考项目） | **仅参考思路** | 只借鉴 `uwfmgr.exe` 子命令、WMI 类与「关保护→重启→安装→恢复」状态机的设计思路，**未复制任何代码、图标或资源** |
| [Monitoring-Center](https://github.com/candiedoperation/Monitoring-Center) | **仅参考思路** | 只参考「移动端通过 Veyon WebAPI 驱动 Feature」的理念；本分支的手机端是从零编写的 Kotlin 原生工程，**未复制任何代码或界面** |
| Qt / Windows SDK / Android SDK | 构建依赖 | 各自遵循其原有许可证 |
| Ollama / OpenAI 兼容推理服务 | 可选外部服务 | 由使用者自行部署，不在本仓库内 |

本仓库**不包含**任何第三方二进制库、驱动、字体或图片素材；新增图标沿用 Veyon 自带的 `:/core/icon*.png`。

## 5. 商标与名称

- 「Veyon」及其徽标属于原作者/项目，本分支不使用其商标做背书。
- 「课守 / ClassKeep」是本定制分支的名称，不代表 Veyon 官方产品。
- 上游 README 中的徽章、文档链接与 Transifex 链接为上游原样保留，仅用于说明来源。

## 6. 无担保

本软件按 GPL 第 11、12 条「按现状」提供，**无任何明示或暗示担保**。
课堂监控、UWF 还原、断网、远程安装等操作具有数据丢失与设备不可用风险，
部署前请先备份并阅读 [`CLASSKEEP-DEPLOY.md`](CLASSKEEP-DEPLOY.md)。
