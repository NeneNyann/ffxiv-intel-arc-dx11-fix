#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

namespace FFXIVIntelDX11Fix {

DWORD ApplyDriverPatch();
void LogMessage(const char *format, ...);
void StartRuntime(HMODULE self);

} // namespace FFXIVIntelDX11Fix
