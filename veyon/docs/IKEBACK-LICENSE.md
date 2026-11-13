# ikeback — 许可证与衍生作品声明

> 本文件与仓库根 [`NOTICE.md`](../../NOTICE.md)、[`LICENSE`](../../LICENSE) 共同构成本项目的完整许可声明。
> 如有不一致，**以 `LICENSE`（GNU GPL v2 全文）为准**；本文件负责解释与应用说明。
> 仓库：<https://github.com/zk26/ikeback>

---

## 1. 上游

本项目基于 [Veyon](https://veyon.io/)（`https://github.com/veyon/veyon`）4.x。
上游以 **GNU General Public License v2.0 or later（GPL-2.0-or-later）** 发布，
全文见仓库根 [`LICENSE`](../../LICENSE) 与本目录上级 [`COPYING`](../COPYING)（两者内容相同）。

上游源码文件头声明：

> Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
> This program is free software; you can redistribute it and/or modify it under
> the terms of the GNU General Public License as published by the Free Software
> Foundation; either version 2 of the License, or (at your option) any later version.

**所有上游文件头、版权与许可证声明均原样保留，不得删除或改写。**

## 2. 本仓库的性质与授权

ikeback 是 Veyon 的**修改 / 衍生作品**，不是 Veyon 官方版本，
与 Veyon 项目、其作者及 Veyon Solutions 无隶属、代理、背书或商业合作关系。

按 GPL 第 2 条与第 3 条，本仓库**整体作为一个程序**继续以 GPL-2.0-or-later 授权：

```
SPDX-License-Identifier: GPL-2.0-or-later
```

覆盖范围（不限于）：

- `veyon/plugins/classroom/` — ikeback 插件全部 C++/Qt 源文件
- `mobile/`（仓库根，唯一一份）— Android Kotlin 工程与图标生成脚本
- `veyon/docs/`、`veyon/scripts/`（含 NSIS 安装器模板）、根级 `README.md`、`NOTICE.md`、`LICENSE`、`.gitignore`
- `.github/workflows/` — 编译验证 / Windows 构建 / 发布三条工作流
- `veyon/core/src/FeatureProviderInterface.h`、`veyon/core/src/FeatureManager.h/.cpp`、
  `veyon/server/src/ComputerControlServer.cpp`、`veyon/master/src/ComputerMonitoringWidget.cpp`、
  `veyon/cmake/modules/WindowsBuildHelpers.cmake` 的扩展改动
- `ikeback_logo.png` 及由其生成的 `.ico` / `mipmap` 图标（由仓库所有者提供，见 `NOTICE.md`）

### 关于 Android 应用

`mobile/` 中的 App 是独立进程，仅通过标准 HTTP/UDP 与教师机通信，不链接 Veyon 核心库。
本仓库为保持一致，**同样按 GPL-2.0-or-later 发布**（见 [`NOTICE.md`](../../NOTICE.md) 第 3 节）。
若日后单独分发该 App，分发方仍需自行核对其依赖（AndroidX、Material、Gradle 等）的许可与归属义务，
清单见 [`IKEBACK-THIRD-PARTY.md`](IKEBACK-THIRD-PARTY.md)。

## 3. 修改声明（GPL 第 2(a) 条）

本仓库相对上游做了修改，修改文件清单与逐次 diff 完整记录在 **git 提交历史**中
（含作者、时间与变更内容），并满足「标明文件被修改及修改日期」的要求。
概览见 [`IKEBACK.md`](IKEBACK.md)，协议见 [`IKEBACK-PROTOCOL.md`](IKEBACK-PROTOCOL.md)。

历史说明：本项目的早期提交曾推送到 fork 仓库 `zk26/veyon`；
现独立为 `zk26/ikeback` 并保留原有提交历史。**两个仓库均由同一权利人 zk26 控制**，
不存在第三方权利冲突。

## 4. 分发义务（GPL 第 1–3、6、11、12 条）

分发本项目的源码或二进制时，必须：

1. 随附 `LICENSE`（GPL v2 全文）与 `NOTICE.md`；
2. 提供对应**完整源代码**，或按 GPL 第 3 条给出有效的书面源码获取方式
   （GitHub 自动生成的源码归档 + 本仓库 commit 记录满足该要求）；
3. 保留所有原有版权、许可证与无担保声明，不得删除文件头；
4. 保留本文件的修改声明，并明确标注衍生关系；
5. 不得附加任何与 GPL 冲突的限制（禁止再分发、禁止修改、保密义务、额外授权费等）；
6. 若捆绑第三方 DLL/SO 或 Android 依赖，逐一核对其许可证是否允许该分发方式，
   并随附相应许可文本与归属声明（见 [`IKEBACK-THIRD-PARTY.md`](IKEBACK-THIRD-PARTY.md) 第 7 节）；
7. 不得要求接收方为本软件本身支付许可费；可对介质、支持或保修另行收费。

**离线分发**（如 U 盘拷给其他学校）同样适用上述全部义务，不能只给二进制。

## 5. 第三方素材与参考项目（无代码复制声明）

| 项目 | 关系 | 说明 |
|---|---|---|
| [Veyon](https://github.com/veyon/veyon) | **上游代码** | 本仓库由其派生，保留全部原有声明 |
| `G:/UWFTool`（本地参考项目） | **仅参考设计思路** | 借鉴 `uwfmgr.exe` 子命令、UWF WMI 类与「关保护→重启→安装→恢复」流程概念；**未复制任何源代码、脚本、图标、文档或资源** |
| [Monitoring-Center](https://github.com/candiedoperation/Monitoring-Center) | **仅参考设计思路** | 借鉴「移动端驱动 Veyon」理念；手机端为从零编写的 Kotlin 工程，**未复制任何代码、界面或资源** |
| Qt / QCA / LibVNC / OpenSSL / zlib / AndroidX / Material / Gradle 等 | 构建期依赖 | 各自遵循原有许可证，本仓库**不附带其二进制** |

本仓库**不包含**任何第三方预编译库、内核驱动、字体、第三方图片或音频素材。
Android 图标为 Android Studio 模板默认资源；桌面端图标沿用 Veyon 自带资源。
完整清单见 [`IKEBACK-THIRD-PARTY.md`](IKEBACK-THIRD-PARTY.md)。

## 6. 商标与名称

- 「Veyon」名称、徽标及相关标识归其各自权利人所有；本仓库保留它们仅为说明来源，
  **不构成商标授权、背书或隶属关系**。
- 「ikeback」是本定制分支的名称，不主张对「Veyon」商标的任何权利。
- 上游 README 中保留的徽章、文档链接与 Transifex 链接为原样引用，仅用于标示上游来源。

## 7. 数据与隐私（非许可条款，但为部署前置条件）

本软件具备课堂行为采集与监控能力。许可授权**不等于**对数据处理行为的授权或背书。
部署方须自行履行告知同意、最小必要、访问控制、保存期限与删除等义务，
详见 [`IKEBACK-PRIVACY.md`](IKEBACK-PRIVACY.md)。
项目作者不接触、不控制、不存储任何学校或学生的实际数据。

## 8. 无担保与责任限制

按 GPL 第 11、12 条，本软件「按现状」提供，**不提供任何明示或暗示担保**，
包括但不限于适销性、特定用途适用性、不侵权以及运行不中断。
在任何情况下，作者或贡献者均不对直接、间接、附带、特殊或后果性损失承担责任，
即使已被告知此类损失的可能性。

课堂监控、UWF 还原、断网、远程安装、锁定输入等操作存在数据丢失与设备不可用风险，
部署前请备份并阅读 [`IKEBACK-DEPLOY.md`](IKEBACK-DEPLOY.md)。

## 9. 疑问与联系

许可、归属、商标或合规相关问题请通过
<https://github.com/zk26/ikeback/issues> 提出。
本文件**不授予任何超出 GPL-2.0-or-later 的额外权利**，也不构成法律意见。
