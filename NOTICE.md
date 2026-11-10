# 课守 ClassKeep — 版权、许可与声明 / NOTICE

本文件用于明确本仓库的权利归属、授权范围与责任边界，避免任何归属或授权上的歧义。
若本文件与其他说明有冲突，**以 `LICENSE`（GPL v2 全文）与 `veyon/docs/CLASSKEEP-LICENSE.md` 为准**。

---

## 1. 项目性质

- 名称：中文「课守」，英文 `ClassKeep`。
- 性质：基于开源项目 [Veyon](https://veyon.io/) 4.x 的**修改 / 衍生作品**（derivative work），
  用于课堂教学管理场景的定制分支。
- 仓库：<https://github.com/zk26/ClassKeep>
- **本仓库不是 Veyon 官方版本**，不由 Veyon 项目、其作者或 Veyon Solutions 维护、认可或背书；
  与其无任何隶属、代理、合作或商业关联。

## 2. 版权归属

| 部分 | 版权 |
|---|---|
| Veyon 上游源码（`veyon/` 中除本分支新增/修改文件外的全部内容） | Copyright (c) 2017-2026 Tobias Junghans 及 Veyon 贡献者，按各文件头声明 |
| 本分支新增与修改内容（`veyon/plugins/classroom/`、`mobile/`、`docs/`、`scripts/`、根级文件、核心扩展点改动） | Copyright (c) 2026 zk26 |
| 上游许可证全文 | `LICENSE` / `veyon/COPYING`（GNU GPL v2） |

「本分支新增与修改内容」的版权声明**不削弱也不替换**上游权利人的权利；
两部分的权利均受 GPL 约束，且**整体作为一个程序**按 GPL-2.0-or-later 授权。

## 3. 许可

```
SPDX-License-Identifier: GPL-2.0-or-later
```

- 上游 Veyon 以 GNU General Public License **v2 或（由其选择）任何更新版本**发布。
- 因桌面端与 Veyon 核心静态/动态链接构成同一程序，**本仓库整体**继续以 GPL-2.0-or-later 授权。
- `mobile/` 下的 Android 应用为独立进程，仅通过标准 HTTP/UDP 与教师机通信；
  本仓库为保持一致，同样按 GPL-2.0-or-later 发布。
- 不得对从本仓库获得的软件附加任何额外限制，也不得主张专利或其他排他权利限制其再分发。

完整的分发义务、修改声明与第三方清单见：

- [`veyon/docs/CLASSKEEP-LICENSE.md`](veyon/docs/CLASSKEEP-LICENSE.md)
- [`veyon/docs/CLASSKEEP-THIRD-PARTY.md`](veyon/docs/CLASSKEEP-THIRD-PARTY.md)

## 4. 修改声明（GPL 第 2(a) 条）

本仓库相对上游 Veyon 做了修改，包括但不限于：新增 `plugins/classroom/` 插件、
新增 `mobile/` Android 工程、新增 `docs/` 与 `scripts/`、新增发布工作流，
以及对 `core/src/FeatureProviderInterface.h`、`core/src/FeatureManager.h/.cpp`、
`server/src/ComputerControlServer.cpp`、`master/src/ComputerMonitoringWidget.cpp` 的扩展。

- 修改日期与逐次变更：**本仓库 git 提交历史**（含作者、时间、diff）。
- 修改内容总览：[`veyon/docs/CLASSKEEP.md`](veyon/docs/CLASSKEEP.md)。
- 通信协议（命令字、路由、认证、数据格式）：[`veyon/docs/CLASSKEEP-PROTOCOL.md`](veyon/docs/CLASSKEEP-PROTOCOL.md)。

## 5. 第三方与参考项目（无代码复制声明）

| 名称 | 关系 | 说明 |
|---|---|---|
| Veyon (`veyon/veyon`) | **上游源码** | 本仓库由其 fork/派生而来，保留全部原有版权与许可证声明 |
| `G:/UWFTool` | **仅参考设计思路** | 借鉴 `uwfmgr.exe` 子命令、WMI 类与「关保护→重启→安装→恢复」流程概念；**未复制任何源代码、脚本、图标、文档或资源** |
| Monitoring-Center (`candiedoperation/Monitoring-Center`) | **仅参考设计思路** | 借鉴「移动端驱动 Veyon」理念；手机端为本仓库从零编写的 Kotlin 工程，**未复制任何源代码、界面或资源** |
| Qt / QCA / LibVNC / OpenSSL / zlib / libjpeg / LZO | 构建期依赖 | 各自遵循其原有许可证，本仓库不附带其二进制 |
| AndroidX / Material Components / Gradle / JDK / Android SDK | 构建期依赖 | 各自遵循其原有许可证，本仓库不附带其二进制 |
| Ollama 或其他 OpenAI 兼容推理服务 | 可选外部服务 | 由使用者自行部署与授权，与本仓库无关 |

本仓库**不包含**任何第三方预编译库、内核驱动、字体或第三方图片素材。
Android 图标为 Android Studio 模板生成的默认资源；桌面端图标沿用 Veyon 自带资源。

## 6. 商标与名称

- 「Veyon」名称、徽标及相关标识归其各自权利人所有；本仓库保留它们仅为说明来源，不构成商标授权或背书。
- 「课守 / ClassKeep」是本定制分支的名称，不主张对「Veyon」商标的任何权利。
- 上游 README 中保留的徽章、文档链接与 Transifex 链接为原样引用，仅用于标示上游来源，不代表本分支由上游维护。

## 7. 数据与隐私

本软件具备**课堂行为采集、上网行为记录、积分评价与远程控制**能力，属于可能处理个人信息的工具。
使用者须自行确保其使用符合所在学校制度与适用法律（包括但不限于知情告知、最小必要、
访问授权、保存期限与删除要求），并承担相应责任。

具体范围、默认配置、日志保留与访问控制见
[`veyon/docs/CLASSKEEP-PRIVACY.md`](veyon/docs/CLASSKEEP-PRIVACY.md)。
**项目作者不对使用者的部署方式、数据处理行为或由此产生的任何纠纷承担责任。**

## 8. 无担保与责任限制

按 GPL 第 11、12 条，本软件「按现状」提供，**不提供任何明示或暗示担保**，
包括但不限于适销性、特定用途适用性、不侵权以及运行不中断。
作者/贡献者不对任何直接、间接、附带或后果性损失承担责任。

UWF 还原、断网、远程安装、锁定输入等操作存在**数据丢失或设备不可用风险**，
部署前请备份并阅读 [`veyon/docs/CLASSKEEP-DEPLOY.md`](veyon/docs/CLASSKEEP-DEPLOY.md)。

## 9. 联系

许可、归属或合规相关问题请通过仓库 Issues 提出：<https://github.com/zk26/ClassKeep/issues>。
本文件不授予任何超出 GPL-2.0-or-later 的额外权利。
