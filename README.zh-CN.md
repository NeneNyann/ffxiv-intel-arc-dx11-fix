# FFXIVIntelDX11Fix

[English](README.md) | 简体中文

`FFXIVIntelDX11Fix` 用于修复 Windows 系统上 Intel Arc 显卡在 FINAL FANTASY XIV 原生 Direct3D 11 路径中出现的顶点异常和三角形拉伸问题。

原始问题：[Intel GPU Issue #1440](https://github.com/IGCIT/Intel-GPU-Community-Issue-Tracker-IGCIT/issues/1440)。

此前已有使用 DXVK 绕过该问题的方案，但在本机 B580 上使用该方案配合 [ReShade](https://reshade.me/)、[OptiScaler](https://github.com/optiscaler/OptiScaler) 时会崩溃。本项目的初衷是保留原生 DX11，修复顶点异常，同时继续使用 [ReShade](https://reshade.me/) 和 [OptiScaler](https://github.com/optiscaler/OptiScaler)。

该问题位于 Intel Arc D3D11 驱动的通用 buffer 复制路径，其他使用原生 D3D11 的游戏也可能遇到类似异常。本补丁也可能适用于这些游戏，但目前仅在 FINAL FANTASY XIV 中验证。

## 与 DXVK 对比

以下为本机 B580 的使用情况。

| 项目 | DXVK 方案 | 本项目 |
| --- | --- | --- |
| 渲染接口 | Vulkan | 原生 DX11 |
| [ReShade](https://reshade.me/) | 6.8.0 的 Vulkan 路径导致游戏无法启动；回退至 6.6.2 后可使用，但会随机崩溃 | 保留 DX11 路径供 [ReShade](https://reshade.me/) 使用 |
| [OptiScaler](https://github.com/optiscaler/OptiScaler) | 崩溃 | 保留 DX11 路径供 [OptiScaler](https://github.com/optiscaler/OptiScaler) 使用 |
| 帧数 | 更高 | 基本与原生 DX11 运行时一致 |

## 使用方法

退出游戏。

如果安装过 DXVK，先移除其 `d3d11.dll` 和 `dxgi.dll`。

将 `winhttp.dll` 放到 `ffxiv_dx11.exe` 所在目录，然后正常启动游戏。

卸载时删除 `winhttp.dll` 即可。

### 理论支持

- Intel Arc A 系列（Alchemist / DG2）。
- Intel Arc B 系列（Battlemage / BMG）。
- 驱动：32.0.101.9034。

仅在 Intel Arc B580 上测试。

## 实现原理

使用 RenderDoc 重复重放同一异常帧时，画面有时恢复正常，有时再次出现顶点异常和三角形拉伸。经过大量重复重放与比对，将异常定位到 `ID3D11DeviceContext::UpdateSubresource` 上传后的 GPU buffer 内容错误。附加 IDA 后，从该函数跟入 Intel 驱动入口 `igd10iumd64.dll`，再进入 `igd10umt64xe.dll`，发现 `ResourceUpdateSubresourceUP` 的内部 buffer 复制路径缺少同步，导致错误数据参与渲染并出现几何拉伸。

修复在该路径的 Arc A、Arc B 两份实现中启用 Render Target Cache Flush 和 Stall Pixel Scoreboard。`winhttp.dll` 代理在 D3D11 设备创建后应用补丁，修改前校验联合签名、调用目标和原始指令。补丁只修改游戏进程内已加载的代码，仍使用原生 D3D11 渲染。

## 待验证

瞬时闪烁和三角形拉伸已在下述测试环境中修复。但对于顶点持续停留在错误位置的现象，目前尚未抓到对应的 RenderDoc 记录，无法进一步调试。它是否与瞬时闪烁同源，以及本补丁对它是否有效，仍待验证。

## 已测试环境

- 游戏：FINAL FANTASY XIV 国服客户端，原生 Direct3D 11。
- 显卡：Intel Arc B580。
- 驱动：Intel 32.0.101.9034。
- [ReShade](https://reshade.me/)：6.8.0 with full add-on support。
- [OptiScaler](https://github.com/optiscaler/OptiScaler)：v0.9.4-final，`winmm.dll` 安装模式。

## 手动构建

需要 CMake 4.2+、Visual Studio 2026 和 Windows SDK。

```powershell
cmake --workflow --preset release
```

## 致谢

- [RenderDoc](https://github.com/baldurk/renderdoc/)
- [Mesa](https://gitlab.freedesktop.org/mesa/mesa)：Intel GPU 同步处理参考。
- [DXVK](https://github.com/doitsujin/dxvk)
