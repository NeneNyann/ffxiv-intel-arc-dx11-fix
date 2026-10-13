#pragma once

enum class SignatureRole {
    Entry,
    Identity,
    Early,
    Late,
    Call,
    EarlyTarget,
    LateTarget
};

struct SignatureWindow {
    SignatureRole role;
    const char *pattern;
};

constexpr SignatureWindow ArcASignature[] = {
    {SignatureRole::Entry, "4C 8B DC 55 57 49 8D AB 88 FB FF FF 48 81 EC 88 05 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 30 04 00 00 41 8B 40 74 45 33 C9 41 BA 01 00 00 00 49 89 5B 20 41 8B D9 49 89 73 E8 48 8B F2 4D 89 63 E0 48 8B F9 4D 89 73 D0"},
    {SignatureRole::Early, "49 8B D7 44 89 75 B8 E8 ?? ?? ?? ?? 45 85 34 24 75 ?? F6 83 D4 02 00 00 1F | 0F 84 ?? ?? ?? ?? 4C 8D 73 28 4D 85 F6 74 ?? 49 83 C6 D8 8B 75 B8 49 8D 46 28 41 23 34 24 44 0F B6 A3 D4 02 00 00 41 80 E4 1F"},
    {SignatureRole::Late, "75 ?? 44 38 7D B0 75 ?? 44 38 7D A0 75 ?? 40 84 F6 | 0F 84 ?? ?? ?? ?? 49 8D 46 28 48 85 C0 74 ?? 48 83 C0 D8 83 78 50 00 75 ?? 41 81 BE 50 A8 00 00 00 10 00 00 74 ?? 48 8D 48 28"},
    {SignatureRole::Call, "44 88 4C 24 38 44 0F B6 CA 44 88 44 24 30 49 8B D6 88 44 24 28 45 0F B6 C4 0F B6 45 80 88 44 24 20 | E8 ?? ?? ?? ?? 41 F6 C4 02 74 ?? 48 8B 4B 48 E8 ?? ?? ?? ?? 84 C0 74 ?? C6 83 B0 C4 00 00 00"},
    {SignatureRole::EarlyTarget, "41 83 3C 24 00 4C 8B A4 24 78 05 00 00 4C 8B BC 24 60 05 00 00 4C 8B B4 24 68 05 00 00 75 ??"},
    {SignatureRole::LateTarget, "8B 45 B8 4C 8D A3 D0 02 00 00 0F B6 75 83 F7 D0 41 21 04 24 80 A3 D4 02 00 00 E0 41 83 3C 24 00"},
};

constexpr SignatureWindow ArcBSignature[] = {
    {SignatureRole::Entry, "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 D0 FB FF FF 48 81 EC 50 05 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 20 04 00 00 41 8B 40 74 45 33 ED 41 B9 01 00 00 00 4D 8B F0 48 8B F2"},
    {SignatureRole::Early, "49 8B D7 89 75 C0 E8 ?? ?? ?? ?? 41 85 75 00 75 ?? F6 83 C4 02 00 00 1F | 0F 84 ?? ?? ?? ?? 4C 8D 73 28 4D 85 F6 74 ?? 49 83 C6 D8 44 0F B6 A3 C4 02 00 00 49 8D 46 28 41 23 75 00 41 80 E4 1F 48 85 C0"},
    {SignatureRole::Late, "75 ?? 44 38 7D E8 75 ?? 44 38 7D E4 75 ?? 40 84 F6 | 0F 84 ?? ?? ?? ?? 49 8D 46 28 48 85 C0 74 ?? 48 83 C0 D8 83 78 50 00 75 ?? 41 81 BE A0 A7 00 00 00 10 00 00 74 ?? 48 8D 48 28"},
    {SignatureRole::Call, "44 88 44 24 40 49 8B D6 44 88 5C 24 38 45 0F B6 C4 44 88 54 24 30 88 44 24 28 0F B6 45 80 88 44 24 20 | E8 ?? ?? ?? ?? 41 F6 C4 02 74 ?? 41 C6 86 BE C8 00 00 00 48 8B 4B 48 E8 ?? ?? ?? ?? 84 C0 74 ??"},
    {SignatureRole::EarlyTarget, "41 83 7D 00 00 75 ?? 80 BB C4 02 00 00 00 75 ?? B9 FF EF 00 00"},
    {SignatureRole::LateTarget, "8B 45 C0 F7 D0 41 21 45 00 80 A3 C4 02 00 00 E0 41 83 7D 00 00 75 ?? 80 BB C4 02 00 00 00"},
};
