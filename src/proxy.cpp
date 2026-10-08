#include "runtime.h"

#include <d3d11.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;

using CreateDevice = decltype(&D3D11CreateDevice);
using CreateSwapChain = decltype(&D3D11CreateDeviceAndSwapChain);
CreateDevice G_CREATE_DEVICE = nullptr;
CreateSwapChain G_CREATE_SWAPCHAIN = nullptr;
INIT_ONCE G_PATCH_ONCE = INIT_ONCE_STATIC_INIT;
INIT_ONCE G_LOG_ONCE = INIT_ONCE_STATIC_INIT;
HANDLE G_LOG = INVALID_HANDLE_VALUE;
HMODULE G_SELF = nullptr;
SRWLOCK G_LOG_LOCK = SRWLOCK_INIT;
UINT G_IAT_ENTRIES = 0;
ULONGLONG *G_DEVICE_SLOT = nullptr;
ULONGLONG *G_SWAPCHAIN_SLOT = nullptr;

HRESULT WINAPI CreateDeviceGate(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driverType, HMODULE software, UINT flags,
                                const D3D_FEATURE_LEVEL *levels, UINT levelCount, UINT sdkVersion, ID3D11Device **device,
                                D3D_FEATURE_LEVEL *selectedLevel, ID3D11DeviceContext **context);
HRESULT WINAPI CreateSwapChainGate(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driverType, HMODULE software, UINT flags,
                                   const D3D_FEATURE_LEVEL *levels, UINT levelCount, UINT sdkVersion,
                                   const DXGI_SWAP_CHAIN_DESC *description, IDXGISwapChain **swapchain, ID3D11Device **device,
                                   D3D_FEATURE_LEVEL *selectedLevel, ID3D11DeviceContext **context);

BOOL CALLBACK InitializeLog(PINIT_ONCE, PVOID, PVOID *) {
    wchar_t path[32768] = {};
    const DWORD length = GetModuleFileNameW(G_SELF, path, static_cast<DWORD>(_countof(path)));
    if (!length || length >= _countof(path))
        return TRUE;
    const auto name = std::wcsrchr(path, L'\\');
    if (!name)
        return TRUE;
    name[1] = 0;
    if (wcscat_s(path, L"ffxiv-intel-dx11-fix.log") == 0)
        G_LOG = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    return TRUE;
}

BOOL CALLBACK InitializePatch(PINIT_ONCE, PVOID, PVOID *) {
    constexpr const wchar_t *names[] = {L"igd10iumd64.dll", L"igd10umt64xe.dll"};
    for (const auto name : names) {
        HMODULE module = GetModuleHandleW(name);
        wchar_t path[32768] = {};
        if (module && GetModuleFileNameW(module, path, static_cast<DWORD>(_countof(path))))
            FFXIVIntelDX11Fix::LogMessage("Loaded driver module: %ls base=%p", path, module);
    }
    FFXIVIntelDX11Fix::LogMessage("Main IAT hooks=%u; device gate current=%u; swapchain gate current=%u", G_IAT_ENTRIES,
                                  G_DEVICE_SLOT && *G_DEVICE_SLOT == reinterpret_cast<ULONGLONG>(&CreateDeviceGate) ? 1u : 0u,
                                  G_SWAPCHAIN_SLOT && *G_SWAPCHAIN_SLOT == reinterpret_cast<ULONGLONG>(&CreateSwapChainGate) ? 1u : 0u);
    DWORD result = ERROR_NOT_ENOUGH_MEMORY;
    try {
        result = FFXIVIntelDX11Fix::ApplyDriverPatch();
    } catch (...) {
    }
    FFXIVIntelDX11Fix::LogMessage("Intel RT cache flush + Pixel Scoreboard patch: status=%lu", result);
    return result == ERROR_MOD_NOT_FOUND ? FALSE : TRUE;
}

void ObserveSuccessfulCreation(ID3D11Device *device) {
    const DWORD savedError = GetLastError();
    if (device) {
        ComPtr<IDXGIDevice> dxgiDevice;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice)))) {
            ComPtr<IDXGIAdapter> adapter;
            if (SUCCEEDED(dxgiDevice->GetAdapter(&adapter))) {
                DXGI_ADAPTER_DESC description = {};
                if (SUCCEEDED(adapter->GetDesc(&description)))
                    FFXIVIntelDX11Fix::LogMessage("GPU: %ls vendor=%04X device=%04X", description.Description, description.VendorId,
                                                  description.DeviceId);
            }
        }
    }
    InitOnceExecuteOnce(&G_PATCH_ONCE, InitializePatch, nullptr, nullptr);
    SetLastError(savedError);
}

HRESULT WINAPI CreateDeviceGate(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driverType, HMODULE software, UINT flags,
                                const D3D_FEATURE_LEVEL *levels, UINT levelCount, UINT sdkVersion, ID3D11Device **device,
                                D3D_FEATURE_LEVEL *selectedLevel, ID3D11DeviceContext **context) {
    FFXIVIntelDX11Fix::LogMessage("winhttp D3D11CreateDevice gate entered");
    const HRESULT result =
        G_CREATE_DEVICE(adapter, driverType, software, flags, levels, levelCount, sdkVersion, device, selectedLevel, context);
    if (SUCCEEDED(result))
        ObserveSuccessfulCreation(device ? *device : nullptr);
    return result;
}

HRESULT WINAPI CreateSwapChainGate(IDXGIAdapter *adapter, D3D_DRIVER_TYPE driverType, HMODULE software, UINT flags,
                                   const D3D_FEATURE_LEVEL *levels, UINT levelCount, UINT sdkVersion,
                                   const DXGI_SWAP_CHAIN_DESC *description, IDXGISwapChain **swapchain, ID3D11Device **device,
                                   D3D_FEATURE_LEVEL *selectedLevel, ID3D11DeviceContext **context) {
    FFXIVIntelDX11Fix::LogMessage("winhttp D3D11CreateDeviceAndSwapChain gate entered");
    const HRESULT result = G_CREATE_SWAPCHAIN(adapter, driverType, software, flags, levels, levelCount, sdkVersion, description, swapchain,
                                              device, selectedLevel, context);
    if (SUCCEEDED(result))
        ObserveSuccessfulCreation(device ? *device : nullptr);
    return result;
}

bool InImage(SIZE_T rva, SIZE_T bytes, DWORD imageSize) {
    return rva < imageSize && bytes <= imageSize - rva;
}

bool ImageString(const BYTE *base, SIZE_T rva, DWORD imageSize) {
    return InImage(rva, 1, imageSize) && std::memchr(base + rva, 0, imageSize - rva) != nullptr;
}

bool ReplaceImport(ULONGLONG *slot, void *replacement, void **original) {
    const auto previous = reinterpret_cast<void *>(*slot);
    if (!previous || *original)
        return false;
    MEMORY_BASIC_INFORMATION information = {};
    if (!VirtualQuery(previous, &information, sizeof(information)) || information.State != MEM_COMMIT || (information.Protect & 0xf0) == 0)
        return false;
    DWORD protection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection))
        return false;
    *original = previous;
    const auto observed = InterlockedCompareExchangePointer(reinterpret_cast<void *volatile *>(slot), replacement, previous);
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), protection, &ignored);
    if (observed != previous) {
        *original = nullptr;
        return false;
    }
    ++G_IAT_ENTRIES;
    return true;
}

} // namespace

namespace FFXIVIntelDX11Fix {

void LogMessage(const char *format, ...) {
    const DWORD savedError = GetLastError();
    InitOnceExecuteOnce(&G_LOG_ONCE, InitializeLog, nullptr, nullptr);
    if (G_LOG == INVALID_HANDLE_VALUE) {
        SetLastError(savedError);
        return;
    }
    char message[1024] = {};
    va_list arguments;
    va_start(arguments, format);
    const int length = _vsnprintf_s(message, _countof(message), _TRUNCATE, format, arguments);
    va_end(arguments);
    char record[1280] = {};
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    const int recordLength = _snprintf_s(record, _countof(record), _TRUNCATE, "%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu tid=%lu %s\r\n",
                                         time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
                                         GetCurrentProcessId(), GetCurrentThreadId(), length >= 0 ? message : "log truncated");
    if (recordLength > 0) {
        AcquireSRWLockExclusive(&G_LOG_LOCK);
        DWORD written = 0;
        WriteFile(G_LOG, record, static_cast<DWORD>(recordLength), &written, nullptr);
        ReleaseSRWLockExclusive(&G_LOG_LOCK);
    }
    SetLastError(savedError);
}

void StartRuntime(HMODULE self) {
    G_SELF = self;
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(self),
                            &pinned))
        return;
    auto base = reinterpret_cast<BYTE *>(GetModuleHandleW(nullptr));
    if (!base)
        return;
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x100000)
        return;
    const auto pe = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        pe->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT)
        return;
    const DWORD size = pe->OptionalHeader.SizeOfImage;
    const auto directory = pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!directory.VirtualAddress || !InImage(directory.VirtualAddress, directory.Size, size))
        return;
    for (SIZE_T offset = 0; offset + sizeof(IMAGE_IMPORT_DESCRIPTOR) <= directory.Size; offset += sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
        const auto descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR *>(base + directory.VirtualAddress + offset);
        if (!descriptor->Name)
            break;
        if (!ImageString(base, descriptor->Name, size) ||
            _stricmp(reinterpret_cast<const char *>(base + descriptor->Name), "d3d11.dll") != 0 || !descriptor->OriginalFirstThunk ||
            !descriptor->FirstThunk)
            continue;
        for (SIZE_T index = 0;; ++index) {
            const SIZE_T displacement = index * sizeof(IMAGE_THUNK_DATA64);
            const SIZE_T nameRva = descriptor->OriginalFirstThunk + displacement;
            const SIZE_T slotRva = descriptor->FirstThunk + displacement;
            if (!InImage(nameRva, sizeof(IMAGE_THUNK_DATA64), size) || !InImage(slotRva, sizeof(IMAGE_THUNK_DATA64), size))
                break;
            const auto name = reinterpret_cast<const IMAGE_THUNK_DATA64 *>(base + nameRva);
            auto slot = reinterpret_cast<IMAGE_THUNK_DATA64 *>(base + slotRva);
            if (!name->u1.AddressOfData)
                break;
            if (IMAGE_SNAP_BY_ORDINAL64(name->u1.Ordinal) || !InImage(name->u1.AddressOfData, sizeof(IMAGE_IMPORT_BY_NAME), size))
                continue;
            const SIZE_T textRva = name->u1.AddressOfData + offsetof(IMAGE_IMPORT_BY_NAME, Name);
            if (!ImageString(base, textRva, size))
                continue;
            const auto function = reinterpret_cast<const char *>(base + textRva);
            if (std::strcmp(function, "D3D11CreateDevice") == 0) {
                G_DEVICE_SLOT = &slot->u1.Function;
                ReplaceImport(&slot->u1.Function, reinterpret_cast<void *>(&CreateDeviceGate), reinterpret_cast<void **>(&G_CREATE_DEVICE));
            } else if (std::strcmp(function, "D3D11CreateDeviceAndSwapChain") == 0) {
                G_SWAPCHAIN_SLOT = &slot->u1.Function;
                ReplaceImport(&slot->u1.Function, reinterpret_cast<void *>(&CreateSwapChainGate),
                              reinterpret_cast<void **>(&G_CREATE_SWAPCHAIN));
            }
        }
    }
}

} // namespace FFXIVIntelDX11Fix

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        FFXIVIntelDX11Fix::StartRuntime(instance);
    }
    return TRUE;
}
