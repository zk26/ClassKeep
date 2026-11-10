# 课守 ClassKeep {VER}

## 本 Release 的产物

- `ClassKeep-Mobile-{VER}.apk` — Android 教室端，CI 实测编译 + 单元测试通过（debug 包，正式分发请自行签名）
- `ClassKeep-Teacher-{VER}-win64-preset.zip` — 教师机部署预设包
- `ClassKeep-Student-{VER}-win64-preset.zip` — 学生机部署预设包
- `SHA256SUMS.txt` — 覆盖全部产物

## 重要说明

两个 `-preset` 包**不含 Veyon / 课守可执行文件或 DLL**：它们提供配置模板、安装辅助脚本、
全套文档与许可文件，需配合 Veyon 4.11.4（含 `classroom` 插件）安装使用。
完整的 Windows 二进制包需要 MinGW-w64/GCC 工具链（见 `veyon/docs/CLASSKEEP-BUILD.md`），
由具备该环境的机器产出后手动附加到 Release，CI 不生成占位或伪造二进制。

想验证代码能否编译，可在 GitHub Actions 运行 **ClassKeep Build Check**（Linux/Qt5/GCC 全量编译）。

## 合规

- 许可证 GPL-2.0-or-later（`LICENSE` / `NOTICE.md`）
- 分发义务见 `veyon/docs/CLASSKEEP-LICENSE.md`
- 第三方依赖见 `veyon/docs/CLASSKEEP-THIRD-PARTY.md`
- 本软件采集学生行为数据，部署前**必须**阅读 `veyon/docs/CLASSKEEP-PRIVACY.md`
