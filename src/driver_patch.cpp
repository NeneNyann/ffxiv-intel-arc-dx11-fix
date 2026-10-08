#include "runtime.h"

#include "driver_signatures.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

struct CopyPatch {
    DWORD copy = 0;
    DWORD emitter = 0;
    DWORD call = 0;
    DWORD early = 0;
    DWORD earlyTarget = 0;
    DWORD late = 0;
    DWORD lateTarget = 0;
    const BYTE *prologue;
    SIZE_T length;
};

INT64 RelativeTarget(const BYTE *base, DWORD rva, SIZE_T length) {
    INT32 displacement = 0;
    std::memcpy(&displacement, base + rva + length - sizeof(displacement), sizeof(displacement));
    return static_cast<INT64>(rva) + static_cast<INT64>(length) + displacement;
}

DWORD FindCopy(const BYTE *base, const SignatureWindow *windows, SIZE_T count, CopyPatch &result) {
    struct Pattern {
        SignatureRole role;
        SIZE_T marker = 0;
        std::vector<short> bytes;
    };
    struct Hit {
        DWORD function;
        DWORD point;
    };
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x100000)
        return ERROR_BAD_EXE_FORMAT;
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXCEPTION)
        return ERROR_BAD_EXE_FORMAT;
    const DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    const auto inImage = [imageSize](SIZE_T rva, SIZE_T bytes) { return rva < imageSize && bytes <= imageSize - rva; };
    const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (!directory.VirtualAddress || !inImage(directory.VirtualAddress, directory.Size) || directory.Size % sizeof(RUNTIME_FUNCTION))
        return ERROR_BAD_EXE_FORMAT;
    const auto functions = reinterpret_cast<const RUNTIME_FUNCTION *>(base + directory.VirtualAddress);
    const SIZE_T functionCount = directory.Size / sizeof(RUNTIME_FUNCTION);
    const auto functionRoot = [&](DWORD address) -> DWORD {
        auto entry = std::upper_bound(functions, functions + functionCount, address,
                                      [](DWORD value, const RUNTIME_FUNCTION &function) { return value < function.BeginAddress; });
        if (entry == functions)
            return 0;
        auto function = *--entry;
        if (address >= function.EndAddress)
            return 0;
        for (unsigned depth = 0; depth < 32; ++depth) {
            if (!inImage(function.BeginAddress, function.EndAddress - function.BeginAddress) || !inImage(function.UnwindData, 4))
                return 0;
            const auto unwind = base + function.UnwindData;
            if ((unwind[0] & 7) != 1 && (unwind[0] & 7) != 2)
                return 0;
            if (!(unwind[0] & 0x20))
                return function.BeginAddress;
            const SIZE_T chain = function.UnwindData + 4 + ((static_cast<SIZE_T>(unwind[2]) + 1) & ~SIZE_T(1)) * 2;
            if (!inImage(chain, sizeof(function)))
                return 0;
            std::memcpy(&function, base + chain, sizeof(function));
        }
        return 0;
    };
    std::vector<Pattern> patterns;
    patterns.reserve(count);
    for (SIZE_T i = 0; i < count; ++i) {
        Pattern pattern{windows[i].role, 0, {}};
        for (auto text = windows[i].pattern; *text;) {
            if (*text == ' ') {
                ++text;
            } else if (*text == '|') {
                pattern.marker = pattern.bytes.size();
                ++text;
            } else if (*text == '?') {
                pattern.bytes.push_back(-1);
                text += 2;
            } else {
                char *end = nullptr;
                pattern.bytes.push_back(static_cast<short>(std::strtol(text, &end, 16)));
                text = end;
            }
        }
        patterns.push_back(std::move(pattern));
    }
    const auto matches = [](BYTE byte, short value) { return value < 0 || byte == value; };
    const auto matchesAt = [&](DWORD address, const Pattern &pattern) {
        return inImage(address, pattern.bytes.size()) && std::equal(pattern.bytes.begin(), pattern.bytes.end(), base + address,
                                                                    [&](short value, BYTE byte) { return matches(byte, value); });
    };
    std::array<std::vector<Hit>, 5> hits;
    const auto sections = IMAGE_FIRST_SECTION(nt);
    for (const auto &pattern : patterns) {
        const auto role = static_cast<SIZE_T>(pattern.role);
        if (role >= hits.size())
            continue;
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            const auto &section = sections[i];
            if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE))
                continue;
            if (!inImage(section.VirtualAddress, section.Misc.VirtualSize))
                return ERROR_BAD_EXE_FORMAT;
            const auto end = base + section.VirtualAddress + section.Misc.VirtualSize;
            for (auto candidate = base + section.VirtualAddress; candidate < end; ++candidate) {
                candidate = std::search(candidate, end, pattern.bytes.begin(), pattern.bytes.end(), matches);
                if (candidate == end)
                    break;
                const DWORD address = static_cast<DWORD>(candidate - base);
                const DWORD function = functionRoot(address);
                if (function && functionRoot(address + static_cast<DWORD>(pattern.bytes.size() - 1)) == function)
                    hits[role].push_back({function, address + static_cast<DWORD>(pattern.marker)});
            }
        }
    }
    unsigned found = 0;
    for (const auto &entry : hits[static_cast<SIZE_T>(SignatureRole::Entry)]) {
        if (entry.function != entry.point)
            continue;
        std::array<DWORD, 5> points = {};
        bool unique = true;
        for (const auto &pattern : patterns) {
            const auto role = static_cast<SIZE_T>(pattern.role);
            if (role >= hits.size())
                continue;
            unsigned matched = 0;
            for (const auto &hit : hits[role]) {
                if (hit.function == entry.function) {
                    points[role] = hit.point;
                    ++matched;
                }
            }
            if (matched != 1)
                unique = false;
        }
        if (!unique)
            continue;
        auto patch = result;
        patch.copy = entry.function;
        patch.early = points[static_cast<SIZE_T>(SignatureRole::Early)];
        patch.late = points[static_cast<SIZE_T>(SignatureRole::Late)];
        patch.call = points[static_cast<SIZE_T>(SignatureRole::Call)];
        if (!(patch.copy < patch.early && patch.early < patch.late && patch.late < patch.call) || !inImage(patch.early, 6) ||
            !inImage(patch.late, 6) || !inImage(patch.call, 5) || base[patch.early] != 0x0f || base[patch.early + 1] != 0x84 ||
            base[patch.late] != 0x0f || base[patch.late + 1] != 0x84 || base[patch.call] != 0xe8)
            continue;
        const auto earlyTarget = RelativeTarget(base, patch.early, 6);
        const auto lateTarget = RelativeTarget(base, patch.late, 6);
        const auto emitter = RelativeTarget(base, patch.call, 5);
        if (earlyTarget < 0 || earlyTarget >= imageSize || lateTarget < 0 || lateTarget >= imageSize || emitter < 0 ||
            emitter >= imageSize || !inImage(static_cast<DWORD>(emitter), patch.length))
            continue;
        patch.earlyTarget = static_cast<DWORD>(earlyTarget);
        patch.lateTarget = static_cast<DWORD>(lateTarget);
        patch.emitter = static_cast<DWORD>(emitter);
        if (patch.call + 5 >= patch.lateTarget || patch.lateTarget > patch.earlyTarget || functionRoot(patch.earlyTarget) != patch.copy ||
            functionRoot(patch.lateTarget) != patch.copy || functionRoot(patch.emitter) != patch.emitter ||
            std::memcmp(base + patch.emitter, patch.prologue, patch.length))
            continue;
        if (!std::all_of(patterns.begin(), patterns.end(), [&](const Pattern &pattern) {
                if (pattern.role == SignatureRole::EarlyTarget)
                    return matchesAt(patch.earlyTarget, pattern);
                if (pattern.role == SignatureRole::LateTarget)
                    return matchesAt(patch.lateTarget, pattern);
                return true;
            }))
            continue;
        result = patch;
        if (++found > 1)
            return ERROR_DUP_NAME;
    }
    return found == 1 ? ERROR_SUCCESS : ERROR_NOT_FOUND;
}

void AbsoluteJump(BYTE *destination, const void *target) {
    constexpr BYTE jump[] = {
        0xff, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp qword ptr [rip+0x00]
    };
    std::memcpy(destination, jump, sizeof(jump));
    std::memcpy(destination + sizeof(jump), &target, sizeof(target));
}

} // namespace

namespace FFXIVIntelDX11Fix {

DWORD ApplyDriverPatch() {
    constexpr BYTE standard[] = {
        0x48, 0x89, 0x4c, 0x24, 0x08,                         // mov qword ptr [rsp+0x08], rcx
        0x56,                                                 // push rsi
        0x57,                                                 // push rdi
        0x41, 0x56,                                           // push r14
        0x48, 0x83, 0xec, 0x30,                               // sub rsp, 0x30
        0x44, 0x0f, 0xb6, 0xb4, 0x24, 0xa0, 0x00, 0x00, 0x00, // movzx r14d, byte ptr [rsp+0xa0]
    };
    constexpr BYTE first[] = {
        0x40, 0x53,                   // push rbx
        0x55,                         // push rbp
        0x41, 0x56,                   // push r14
        0x48, 0x83, 0xec, 0x40,       // sub rsp, 0x40
        0x48, 0x89, 0x74, 0x24, 0x68, // mov qword ptr [rsp+0x68], rsi
    };
    CopyPatch patches[] = {
        {0, 0, 0, 0, 0, 0, 0, first, sizeof(first)},
        {0, 0, 0, 0, 0, 0, 0, standard, sizeof(standard)},
    };
    HMODULE module = GetModuleHandleW(L"igd10umt64xe.dll");
    if (!module)
        return ERROR_MOD_NOT_FOUND;
    auto base = reinterpret_cast<BYTE *>(module);
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(base),
                            &pinned))
        return GetLastError();
    const SignatureWindow *signatures[] = {ArcASignature, ArcBSignature};
    const SIZE_T counts[] = {_countof(ArcASignature), _countof(ArcBSignature)};
    for (SIZE_T i = 0; i < _countof(patches); ++i) {
        auto &patch = patches[i];
        const DWORD status = FindCopy(base, signatures[i], counts[i], patch);
        if (status != ERROR_SUCCESS) {
            LogMessage("Arc %s signature rejected: status=%lu; no code modified", i == 0 ? "A" : "B", status);
            return status;
        }
    }
    auto valid = [&](const CopyPatch &patch) {
        return std::memcmp(base + patch.emitter, patch.prologue, patch.length) == 0 && base[patch.call] == 0xe8 &&
               RelativeTarget(base, patch.call, 5) == patch.emitter && base[patch.early] == 0x0f && base[patch.early + 1] == 0x84 &&
               RelativeTarget(base, patch.early, 6) == patch.earlyTarget && base[patch.late] == 0x0f && base[patch.late + 1] == 0x84 &&
               RelativeTarget(base, patch.late, 6) == patch.lateTarget;
    };
    for (SIZE_T i = 0; i < _countof(patches); ++i) {
        LogMessage("Copy %06lX: emitter=%06lX call=%06lX", patches[i].copy, patches[i].emitter, patches[i].call);
        if (!valid(patches[i])) {
            LogMessage("Copy %06lX: original instructions mismatch; no code modified", patches[i].copy);
            return ERROR_REVISION_MISMATCH;
        }
    }
    struct Page {
        BYTE *address;
        DWORD protection = 0;
        bool acquired = false;
    };
    std::vector<Page> pages;
    pages.reserve(32);
    SYSTEM_INFO system = {};
    GetSystemInfo(&system);
    auto addPages = [&](BYTE *address, SIZE_T length) {
        const auto firstPage = reinterpret_cast<UINT_PTR>(address) / system.dwPageSize * system.dwPageSize;
        const auto lastPage = reinterpret_cast<UINT_PTR>(address + length - 1) / system.dwPageSize * system.dwPageSize;
        for (auto page = firstPage; page <= lastPage; page += system.dwPageSize) {
            const auto pointer = reinterpret_cast<BYTE *>(page);
            if (std::none_of(pages.begin(), pages.end(), [pointer](const Page &page) { return page.address == pointer; }))
                pages.push_back({pointer, 0, false});
        }
    };
    for (SIZE_T i = 0; i < _countof(patches); ++i) {
        addPages(base + patches[i].emitter, patches[i].length);
        addPages(base + patches[i].early, 6);
        addPages(base + patches[i].late, 6);
    }
    auto restorePages = [&]() {
        bool success = true;
        for (auto it = pages.rbegin(); it != pages.rend(); ++it) {
            if (!it->acquired)
                continue;
            DWORD ignored = 0;
            if (VirtualProtect(it->address, system.dwPageSize, it->protection, &ignored))
                it->acquired = false;
            else
                success = false;
        }
        return success;
    };
    auto code = static_cast<BYTE *>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!code)
        return GetLastError();
    for (SIZE_T i = 0; i < _countof(patches); ++i) {
        BYTE thunk[] = {
            0x50,                                                       // push rax
            0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, imm64 (returnAddress)
            0x48, 0x39, 0x44, 0x24, 0x08,                               // cmp qword ptr [rsp+0x08], rax
            0x75, 0x09,                                                 // jne +0x09 (pop rax)
            0x41, 0x83, 0xc8, 0x01,                                     // or r8d, 0x01
            0xc6, 0x44, 0x24, 0x40, 0x01,                               // mov byte ptr [rsp+0x40], 0x01
            0x58,                                                       // pop rax
        };
        const void *returnAddress = base + patches[i].call + 5;
        std::memcpy(thunk + 3, &returnAddress, sizeof(returnAddress));
        const auto entry = code + i * 128;
        const auto trampoline = entry + 64;
        std::memcpy(entry, thunk, sizeof(thunk));
        AbsoluteJump(entry + sizeof(thunk), trampoline);
        std::memcpy(trampoline, patches[i].prologue, patches[i].length);
        AbsoluteJump(trampoline + patches[i].length, base + patches[i].emitter + patches[i].length);
    }
    DWORD oldCodeProtection = 0;
    if (!VirtualProtect(code, 4096, PAGE_EXECUTE_READ, &oldCodeProtection) || !FlushInstructionCache(GetCurrentProcess(), code, 4096)) {
        const DWORD error = GetLastError();
        VirtualFree(code, 0, MEM_RELEASE);
        return error;
    }
    for (auto &page : pages) {
        if (!VirtualProtect(page.address, system.dwPageSize, PAGE_EXECUTE_READWRITE, &page.protection)) {
            const DWORD error = GetLastError();
            restorePages();
            VirtualFree(code, 0, MEM_RELEASE);
            return error;
        }
        page.acquired = true;
    }
    for (SIZE_T i = 0; i < _countof(patches); ++i) {
        if (!valid(patches[i])) {
            restorePages();
            VirtualFree(code, 0, MEM_RELEASE);
            return ERROR_REVISION_MISMATCH;
        }
    }
    for (SIZE_T i = 0; i < _countof(patches); ++i) {
        BYTE replacement[sizeof(standard)] = {};
        std::memset(replacement, 0x90, sizeof(replacement));
        AbsoluteJump(replacement, code + i * 128);
        std::memcpy(base + patches[i].emitter, replacement, patches[i].length);
        std::memset(base + patches[i].early, 0x90, 6);
        std::memset(base + patches[i].late, 0x90, 6);
    }
    bool flushed = true;
    for (const auto &page : pages)
        if (!FlushInstructionCache(GetCurrentProcess(), page.address, system.dwPageSize))
            flushed = false;
    const bool restored = restorePages();
    if (!restored)
        restorePages();
    for (SIZE_T i = 0; i < _countof(patches); ++i)
        LogMessage("Copy %06lX: patch installed", patches[i].copy);
    return flushed && restored ? ERROR_SUCCESS : ERROR_WRITE_FAULT;
}

} // namespace FFXIVIntelDX11Fix
