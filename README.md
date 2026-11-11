# netSwitch

一个快捷切换网络优先的软件，只在系统托盘留存。

Windows 10 / 11（64 位）原生网络切换托盘工具，当前版本 **1.0.1 测试版**。

![程序图标](docs/icon.png)

支持 Wi-Fi / 有线网络优先级调整和独占切换，提供安装器、开机自启与恢复原设置功能。使用 C++ / Win32 实现，运行不依赖 PowerShell、Python、.NET 或开发工具。

> 当前完成了构建、模拟恢复验收和安全的只读/界面测试。真实网卡切换、管理员安装卸载、服务异常恢复等完整系统集成验收尚未完成。此项目不是已完成全部验收的稳定版。

## 使用

下载安装 EXE，确认 Windows 管理员授权；安装后点击托盘图标，进入“设置”指定一张 Wi-Fi 和一张有线网卡。未选择网卡时不会切换。

- **优先模式（默认）**：保留两张网卡启用，调整 IPv4 / IPv6 接口优先级。VPN、专用路由和已有连接仍可能使用其他路径。
- **独占模式（主动开启）**：先启用并等待目标网卡连接与 IP 地址就绪，再停用另一张指定网卡；会中断部分连接。
- **恢复原设置**：恢复仍与程序写入值匹配的配置，遇到设备移除或外部修改时保留恢复记录并提示。
- **重新启用网卡**：对指定网卡执行明确的启用修复操作，保留启用状态。

退出默认恢复程序持有的修改。安装或更新不会自动重新应用上次的网络偏好。完整操作说明见 [使用说明](docs/USAGE.md)。

## 构建

在 Windows 上安装带 MinGW-w64 工具链的 Dev-C++。默认工具链路径：

```
C:\Program Files (x86)\Dev-Cpp\MinGW64\bin
```

使用命令提示符进入项目目录并执行：

```bat
build.cmd
```

生成：

| 文件 | 用途 |
| --- | --- |
| `NetSwitch.exe` | 普通权限托盘程序 |
| `NetSwitchService.exe` | 受限本地指令的后台服务 |
| `Setup.exe` | 包含前两者的独立安装/卸载器 |

构建脚本当前使用上述 Dev-C++ 默认路径；工具链安装位置不同需调整脚本中的 PATH。运行安装包不需要保留编译器。

只有可选的图标重新生成脚本 `build_icon.py` 需要 Python 和 Pillow；正常构建直接使用已包含的 `app.ico`。

## 测试

在 Windows 命令提示符中：

```bat
build-tests.cmd
acceptance.exe
build-dpi-tests.cmd
dpi_acceptance.exe
build-inspect.cmd
inspect.exe
build-smoke.cmd
smoke.exe
```

- `acceptance.exe`：直接测试正式服务逻辑，使用模拟网卡和真实恢复文件读写。23 项场景通过；不改动真实网络。
- `dpi_acceptance.exe`：使用正式窗口过程，模拟 100%、125%、150%、200% DPI 变化，检查字体、布局、图标尺寸与 GDI 资源稳定性。不更改显示设置。
- `inspect.exe`：只读识别真实网卡和 IP 参数。
- `smoke.exe`：短暂创建托盘图标并退出，不连接服务或改动网络。
- `integration.exe`（由 `build-integration.cmd` 构建）：仅检查管理员令牌，不执行服务安装；它不是系统集成验收工具。

完整测试范围和限制见 [验收报告](docs/ACCEPTANCE.md)。

## 结构

| 文件 | 职责 |
| --- | --- |
| `tray.cpp` | 托盘、设置、异步指令与开机自启 |
| `service.cpp` | 用户授权、网络操作、控制进程生命周期与恢复 |
| `network.hpp` | WMI / IP Helper、磁盘恢复记录与事务 |
| `common.hpp` | 本地协议和通用 Windows 工具 |
| `dpi.hpp` | Per-Monitor V2、字体和界面重排 |
| `setup.cpp` | 安装、升级、卸载及资源解包 |
| `app.manifest` | DPI 感知与运行权限声明 |

## 版本与发布

1.0.1 增加高 DPI 适配，详见 [更新说明](docs/CHANGELOG-1.0.1.md)。当前提交包含源码和构建说明，尚未上传预编译安装器。后续发布安装器时建议使用 GitHub Release 附件，构建产物不提交到源码历史。

尚未选择开源许可证；本仓库不会自动授予开源许可。
