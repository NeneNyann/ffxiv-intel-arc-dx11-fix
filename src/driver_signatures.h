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
    {SignatureRole::Entry, "4C 8B DC 55 41 56 49 8D AB 08 FB FF FF 48 81 EC 08 06 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 A0 04 00 00 45 33 C9 49 89 5B 20 41 8B C1 49 89 7B E0 4D 89 63 D8 41 8B D9 89 45 B0 41 BA 01 00 00 00 89 45 60 4D 8B E0 41 8B 40 74 4C 8B F1"},
    {SignatureRole::Early, "49 8B D7 89 7D B8 E8 ?? ?? ?? ?? 41 85 7D 00 75 ?? F6 83 D4 02 00 00 1F | 0F 84 ?? ?? ?? ?? 48 8D 73 28 48 85 F6 74 ?? 48 83 C6 D8 44 0F B6 A3 D4 02 00 00 48 8D 46 28 41 23 7D 00"},
    {SignatureRole::Late, "44 38 7D 88 75 ?? 44 38 7D B0 75 ?? 44 38 7D A0 75 ?? 44 38 7D 90 75 ?? 40 84 FF | 0F 84 ?? ?? ?? ?? 48 8D 46 28 48 85 C0 74 ?? 48 83 C0 D8 83 78 50 00 75 ?? 81 BE 50 A8 00 00 00 10 00 00"},
    {SignatureRole::Call, "44 88 44 24 30 48 8B D6 88 44 24 28 45 0F B6 C4 0F B6 45 80 88 44 24 20 | E8 ?? ?? ?? ?? 41 F6 C4 02 74 ?? 48 8B 4B 48 E8 ?? ?? ?? ?? 84 C0 74 ?? C6 83 B0 C4 00 00 00 EB ??"},
    {SignatureRole::EarlyTarget, "41 83 7D 00 00 4C 8B AC 24 E8 05 00 00 4C 8B BC 24 E0 05 00 00"},
    {SignatureRole::LateTarget, "8B 45 B8 44 0F B6 65 83 F7 D0 41 21 45 00 80 A3 D4 02 00 00 E0"},
};

constexpr SignatureWindow ArcBSignature[] = {
    {SignatureRole::Entry, "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 80 FB FF FF 48 81 EC A0 05 00 00 0F 29 B4 24 90 05 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 60 04 00 00 41 8B 40 74 45 33 C9 41 BA 01 00 00 00 4D 8B E0 41 8B DA 4C 8B FA 4C 8B"},
    {SignatureRole::Identity, "B9 00 08 00 00 81 8B A0 92 00 00 00 04 00 00 66 C7 83 71 8C 00 00 00 00 66 0B 48 2A 66 89 48 2A 49 8B 0E 33 D2 48 8B 41 48 83 89 9C A7 00 00 30 48 05 E8 50 01 00 66 83 49 2A 04 66 48"},
    {SignatureRole::Early, "49 8B D5 89 7D A8 E8 ?? ?? ?? ?? 41 85 3C 24 75 ?? F6 83 C4 02 00 00 1F | 0F 84 ?? ?? ?? ?? 48 8D 73 28 48 85 F6 74 ?? 48 83 C6 D8 41 23 3C 24 48 8D 46 28 44 0F B6 A3 C4 02 00 00"},
    {SignatureRole::Late, "44 38 6D 98 75 ?? 44 38 6D 94 75 ?? 44 38 6D 90 75 ?? 44 38 6D 8C 75 ?? 40 84 FF | 0F 84 ?? ?? ?? ?? 48 8D 46 28 48 85 C0 74 ?? 48 83 C0 D8 83 78 50 00 75 ?? 81 BE A0 A7 00 00 00 10 00 00"},
    {SignatureRole::Call, "44 88 44 24 30 48 8B D6 88 44 24 28 45 0F B6 C4 0F B6 45 81 88 44 24 20 | E8 ?? ?? ?? ?? 41 F6 C4 02 74 ?? C6 86 9E C8 00 00 00 48 8B 4B 48 E8 ?? ?? ?? ?? 84 C0 74 ?? C6 83 C8 C3 00 00 00"},
    {SignatureRole::EarlyTarget, "41 83 3C 24 00 75 ?? 80 BB C4 02 00 00 00 75 ?? B9 FF EF 00 00"},
    {SignatureRole::LateTarget, "8B 45 A8 4C 8D A3 C0 02 00 00 F7 D0 41 21 04 24 80 A3 C4 02 00 00 E0"},
};
