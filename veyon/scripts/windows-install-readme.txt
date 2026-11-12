课守 ClassKeep - Windows 安装说明
================================================

本目录包含 Veyon 4.11.4 与课守 ClassKeep 插件的全部可执行文件、
Qt6 运行时 DLL、Qt 插件与 QCA 加密插件，以及 GPL 许可证。

【安装方式】
  沿用 Veyon 官方的组件式安装：「服务端」组件为必选，教师端额外提供可选的
  「教师控制台」组件（ClassKeep-master.exe）；学生端只有服务端组件，
  不会安装也不会出现 Master 选项。

  服务注册沿用 Veyon 原有的 ClassKeep-wcli.exe（service register / config set /
  service start），卸载时同样由它注销服务。安装目录内必须带有
  plugins/windows-platform.dll 与 crypto/libqca-ossl.dll，否则该 CLI 启动时
  会 qFatal / __fastfail；发布流水线已对这两个文件做硬校验。

  安装器会捕获每一步的返回码写入本目录的 install.log，并用 sc query 确认
  ClassKeepService 真的处于 RUNNING；未启动时会用系统服务控制兜底并弹出中文提示。
  若看到「服务未能启动」，请把 install.log 交给管理员排查。

【中文界面】
  默认简体中文（translations/ 下的 .qm 文件）。要切换语言，打开
  ClassKeep-configurator.exe → General → Language，选择后重新打开程序。

【卸载保护】
  教师端控制台「System & devices」页可开启学生端卸载保护。开启后运行本目录的
  uninstall.exe 会提示「教师端已开启卸载保护」并中止，必须由教师端点「解除」
  后才能卸载。标志写在本机注册表，不依赖网络。

【教师机】
  1. 安装时保持「教师控制台」勾选。
  2. 运行 ClassKeep-configurator.exe 完成 Veyon 基础配置（认证密钥、网络端口等）。
  3. 运行 ClassKeep-master.exe 打开教师端，工具栏点「课守 ClassKeep」进入控制台。
  4. 导入班级名单，点「开始新课堂」生成本堂课密钥，按 IP 登录学生机。

【学生机】（需 Windows 10/11 企业版、教育版或 IoT 企业版，UWF 才可用）
  1. 安装器只装服务端；安装完成后 Service 已自动注册并启动。
  2. 以管理员运行 ClassKeep-configurator.exe 导入教师机导出的认证配置。
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
