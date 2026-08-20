#pragma once
#include "KeyboardLayout.h"
#include "FixIme.h"
#include "FixBuddy.h"

// Quick integration - just call these functions!

// Initialize keyboard layout (call once at startup)
inline void InitKeyboardLayout(int width, int height) {
    ApplyLongQuickSlot();
    AdjustKeyboardLayoutByResolution(width, height);
}

// Update keyboard layout when resolution changes
inline void UpdateKeyboardLayout(int width, int height) {
    AdjustKeyboardLayoutByResolution(width, height);
}

// Preset resolutions
inline void InitKeyboardLayout_800x600() {
    InitKeyboardLayout(800, 600);
}

inline void InitKeyboardLayout_1024x768() {
    InitKeyboardLayout(1024, 768);
}

inline void InitKeyboardLayout_1280x720() {
    InitKeyboardLayout(1280, 720);
}

inline void InitKeyboardLayout_1366x768() {
    InitKeyboardLayout(1366, 768);
}

inline void InitKeyboardLayout_1440x900() {
    InitKeyboardLayout(1440, 900);
}

inline void InitKeyboardLayout_1600x900() {
    InitKeyboardLayout(1600, 900);
}

inline void InitKeyboardLayout_1920x1080() {
    InitKeyboardLayout(1920, 1080);
}
