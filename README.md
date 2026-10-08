# FFXIVIntelDX11Fix

English | [简体中文](README.zh-CN.md)

`FFXIVIntelDX11Fix` fixes vertex glitches and stretched triangles in FINAL FANTASY XIV on Windows with Intel Arc graphics using native Direct3D 11.

Original issue: [Intel GPU Issue #1440](https://github.com/IGCIT/Intel-GPU-Community-Issue-Tracker-IGCIT/issues/1440).

An existing workaround uses DXVK to avoid the issue, but using that workaround with [ReShade](https://reshade.me/) and [OptiScaler](https://github.com/optiscaler/OptiScaler) causes crashes on the author's B580 setup. This project aims to fix the vertex glitches while retaining native DX11 for continued use of [ReShade](https://reshade.me/) and [OptiScaler](https://github.com/optiscaler/OptiScaler).

The issue lies in a general buffer-copy path in the Intel Arc D3D11 driver. Other games using native D3D11 may encounter similar glitches, and this patch may work for them as well. It has only been validated in FINAL FANTASY XIV.

## Comparison with DXVK

The following reflects the author's B580 setup.

| Item | DXVK workaround | This project |
| --- | --- | --- |
| Rendering API | Vulkan | Native DX11 |
| [ReShade](https://reshade.me/) | The Vulkan path in 6.8.0 prevents the game from starting; reverting to 6.6.2 allows it to run, but random crashes occur | Retains the DX11 path for [ReShade](https://reshade.me/) |
| [OptiScaler](https://github.com/optiscaler/OptiScaler) | Crashes | Retains the DX11 path for [OptiScaler](https://github.com/optiscaler/OptiScaler) |
| Frame rate | Higher | Approximately the same as native DX11 |

## Usage

Close the game.

If DXVK is installed, remove its `d3d11.dll` and `dxgi.dll`.

Place `winhttp.dll` in the directory containing `ffxiv_dx11.exe`, then start the game normally.

To uninstall, remove `winhttp.dll`.

### Theoretical Support

- Intel Arc A series (Alchemist / DG2).
- Intel Arc B series (Battlemage / BMG).
- Driver: 32.0.101.9034.

Tested only on Intel Arc B580.

## Implementation

Repeated RenderDoc replays of the same affected frame sometimes produce a normal image and sometimes reproduce the vertex glitches and stretched triangles. Extensive repeated replays and comparisons traced the corruption to GPU buffer contents after `ID3D11DeviceContext::UpdateSubresource`. After attaching IDA, stepping from this function into the Intel driver entry `igd10iumd64.dll` and then into `igd10umt64xe.dll` revealed missing synchronization in the internal buffer-copy path used by `ResourceUpdateSubresourceUP`, causing incorrect data to produce stretched geometry.

The fix enables Render Target Cache Flush and Stall Pixel Scoreboard in the Arc A and Arc B implementations of that path. The `winhttp.dll` proxy applies the patch after D3D11 device creation, verifying joint signatures, call targets and original instructions first. It modifies only loaded code in the game process and keeps rendering on native D3D11.

## Pending Verification

Brief flickering and triangle stretching have been resolved in the tested environment listed below. However, no RenderDoc capture of vertices remaining persistently in the wrong positions has been obtained, preventing further debugging of that behavior. Whether it shares the same cause as the brief flickering, and whether this patch fixes it, remain unverified.

## Tested Environment

- Game: FINAL FANTASY XIV, Chinese client, native Direct3D 11.
- GPU: Intel Arc B580.
- Driver: Intel 32.0.101.9034.
- [ReShade](https://reshade.me/): 6.8.0 with full add-on support.
- [OptiScaler](https://github.com/optiscaler/OptiScaler): v0.9.4-final, installed as `winmm.dll`.

## Manual Build

Requires CMake 4.2+, Visual Studio 2026 and the Windows SDK.

```powershell
cmake --workflow --preset release
```

## Acknowledgments

- [RenderDoc](https://github.com/baldurk/renderdoc/)
- [Mesa](https://gitlab.freedesktop.org/mesa/mesa): Intel GPU synchronization reference.
- [DXVK](https://github.com/doitsujin/dxvk)
