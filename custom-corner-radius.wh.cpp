// ==WindhawkMod==
// @id              custom-corner-radius
// @name            Custom Window Corner Radius
// @description     Set any corner radius for non-maximized windows, like macOS
// @version         1.0.0
// @author          MarelowVale
// @github          https://github.com/MarelowVale
// @include         dwm.exe
// @architecture    x86-64
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Custom Window Corner Radius

Patch the corner-radius float constants (8.0 / 4.0) inside DWM's uDWM.dll,
so you can set any radius for normal windows and menus.

## Prerequisites
- Windows 11, Windhawk *installed* version (portable can't inject into dwm.exe)
- Windhawk Settings -> Advanced -> Process injection -> Custom process
  inclusion list: add `dwm.exe`, otherwise this mod silently fails.

## Notes
- Maximimized/fullscreen windows are unaffected (separate DWM code path)
- Settings apply instantly; disable/uninstall fully reverts defaults
- Don't combine with other DWM corner-patching tools
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- radius: 12
  $name: Window corner radius (px)
  $description: Radius for normal windows. Windows default is 8, macOS-like 10-14
- menuRadius: 4
  $name: Menu corner radius (px)
  $description: Radius for menus/flyouts. Windows default is 4
*/
// ==/WindhawkModSettings==

#include <windows.h>

struct { int radius; int menuRadius; } g_settings;

struct PatchedFloat { float* address; float original; };
PatchedFloat g_patches[64];
int g_patchCount = 0;

void LoadSettings() {
    g_settings.radius = Wh_GetIntSetting(L"radius");
    g_settings.menuRadius = Wh_GetIntSetting(L"menuRadius");
    if (g_settings.radius < 0)     g_settings.radius = 0;
    if (g_settings.radius > 32)    g_settings.radius = 32;
    if (g_settings.menuRadius < 0) g_settings.menuRadius = 0;
    if (g_settings.menuRadius > 32) g_settings.menuRadius = 32;
}

void RestorePatches() {
    for (int i = 0; i < g_patchCount; i++) {
        DWORD oldProtect;
        if (VirtualProtect(g_patches[i].address, sizeof(float),
                           PAGE_READWRITE, &oldProtect)) {
            *g_patches[i].address = g_patches[i].original;
            VirtualProtect(g_patches[i].address, sizeof(float),
                           oldProtect, &oldProtect);
        }
    }
    g_patchCount = 0;
}

// Scan uDWM.dll's .rdata for the 8.0/4.0 radius constants and rewrite them.
BOOL ApplyPatches() {
    HMODULE udwm = GetModuleHandle(L"udwm.dll");
    if (!udwm) { Wh_Log(L"udwm.dll not loaded (Windows 10?)"); return FALSE; }

    BYTE* base = (BYTE*)udwm;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return FALSE;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return FALSE;

    IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (memcmp(sec->Name, ".rdata", 6) != 0) continue;

        BYTE* start = base + sec->VirtualAddress;
        DWORD size = sec->Misc.VirtualSize;
        int patched = 0;

        for (DWORD off = 0; off + sizeof(float) <= size && g_patchCount < 64;
             off += sizeof(float)) {
            float* p = (float*)(start + off);
            float v = *p;
            if (v != 8.0f && v != 4.0f) continue;

            float newVal = (v == 8.0f) ? (float)g_settings.radius
                                       : (float)g_settings.menuRadius;
            if (newVal == v) continue;  // same as system default, skip

            DWORD oldProtect;
            if (!VirtualProtect(p, sizeof(float), PAGE_READWRITE, &oldProtect))
                continue;
            g_patches[g_patchCount].address = p;
            g_patches[g_patchCount].original = v;
            g_patchCount++;
            *p = newVal;
            VirtualProtect(p, sizeof(float), oldProtect, &oldProtect);
            Wh_Log(L"Patched %p: %.1f -> %.1f", p, v, newVal);
            patched++;
        }
        Wh_Log(L"Done, %d radius constants patched", patched);
        return TRUE;
    }
    Wh_Log(L".rdata section not found");
    return FALSE;
}

BOOL Wh_ModInit() {
    LoadSettings();
    return ApplyPatches();
}

void Wh_ModUninit() {
    RestorePatches();
}

void Wh_ModSettingsChanged() {
    RestorePatches();
    LoadSettings();
    ApplyPatches();
}
