课守 ClassKeep - Windows 安装说明
================================================

本目录包含 Veyon 4.11.4 与课守 ClassKeep 插件的全部可执行文件、
Qt6 运行时 DLL、Qt 插件与 QCA 加密插件，以及 GPL 许可证。

【安装方式】
  沿用 Veyon 官方安装流程：安装器的「服务端」组件为必选，会自动注册并启动
  Veyon Service、放行防火墙、启用 SAS。教师端安装器额外提供可选的
  「教师控制台」组件（veyon-master.exe）；学生端安装器只有服务端组件，
  不会安装也不会出现 Master 选项。

【教师机】
  1. 安装时保持「教师控制台」勾选。
  2. 运行 veyon-configurator.exe 完成 Veyon 基础配置（认证密钥、网络端口等）。
  3. 运行 veyon-master.exe 打开教师端，工具栏点「课守 ClassKeep」进入控制台。
  4. 导入班级名单，点「开始新课堂」生成本堂课密钥，按 IP 登录学生机。

【学生机】（需 Windows 10/11 企业版、教育版或 IoT 企业版，UWF 才可用）
  1. 安装器只装服务端；安装完成后 Service 已自动注册并启动。
  2. 以管理员运行 veyon-configurator.exe 导入教师机导出的认证配置。
  3. 如需 UWF 还原：先添加必要目录排除项，再重启（不能只关机再开机）。
  4. 学生端悬浮条由 Service 自动拉起，学生输入「座位号 + 姓名 + 本堂课密钥」登录。

【手机端】
  Android 教室端 App 见本仓库 Releases。需与教师机在同一局域网，
  并在 App 中输入本堂课密钥与教师机 IP/端口。

【重要】
  - 本软件会采集学生行为数据。部署前必须阅读仓库中的
    veyon/docs/CLASSKEEP-PRIVACY.md 并完成告知、访问控制与保存期限评估。
  - Bridge 端口（默认 11880）仅限受控教学网段，切勿映射到公网。
  - 完整文档：https://github.com/zk26/ClassKeep

许可：GPL-2.0-or-later，见 LICENSE.TXT / COPYING.TXT / NOTICE.md。
