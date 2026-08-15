#include "pch.h"
#include "hook.h"


// 修复技能描述中文换行乱码的问题 - fix garbled line-wrapping of Chinese (GBK double-byte)
// skill descriptions: the original code wraps each description line at a fixed limit of 55
// bytes, which can cut a GBK double-byte character in half and produce mojibake. Replace the
// constant with the nearest safe character boundary (55 or 56) computed from the actual text.
// reference: BeiDou-ijl15 - ezorsia/codecaves.h calcCharLen / skillToolTipNew
constexpr int SKILL_TOOLTIP_LINE_BYTES = 55;
constexpr int SKILL_TOOLTIP_SCAN_BYTES = 60;

static bool IsGbkLeadByte(unsigned char c) {
    return c >= 0x81 && c <= 0xFE;
}

static bool IsGbkTrailByte(unsigned char c) {
    return c >= 0x40 && c <= 0xFE && c != 0x7F;
}

int __stdcall CalcCharLen(const char* sText) {
    if (!sText || !*sText) {
        return SKILL_TOOLTIP_LINE_BYTES;
    }
    size_t uLength = strlen(sText);
    if (uLength <= static_cast<size_t>(SKILL_TOOLTIP_LINE_BYTES)) {
        return SKILL_TOOLTIP_LINE_BYTES;
    }
    size_t uScanLimit = std::min(uLength, static_cast<size_t>(SKILL_TOOLTIP_SCAN_BYTES));
    size_t i = 0;
    while (i < uScanLimit) {
        size_t uStep = 1;
        if (IsGbkLeadByte(static_cast<unsigned char>(sText[i])) && i + 1 < uScanLimit &&
            IsGbkTrailByte(static_cast<unsigned char>(sText[i + 1]))) {
            uStep = 2;
        }
        size_t uNextBoundary = i + uStep;
        if (uNextBoundary >= static_cast<size_t>(SKILL_TOOLTIP_LINE_BYTES)) {
            return static_cast<int>(uNextBoundary);
        }
        i = uNextBoundary;
    }
    return SKILL_TOOLTIP_LINE_BYTES;
}

static auto CUIToolTip__SkillToolTip_jmp = 0x008F383E;
static auto CUIToolTip__SkillToolTip_ret = 0x008F3844;
void __declspec(naked) CUIToolTip__SkillToolTip_hook() {
    __asm {
        pushfd                                  ; save flags
        push    ecx                             ; save caller-saved registers
        push    edx
        mov     eax, [ ebp + 0xC ]              ; arg2 - const char* sText
        push    eax
        call    CalcCharLen                     ; eax = GBK-safe line byte limit
        pop     edx
        pop     ecx
        mov     [ ebp - 0x1C ], eax             ; overwritten instruction
        popfd                                   ; restore flags
        lea     eax, [ ebp - 0x30 ]             ; overwritten instruction
        jmp     [ CUIToolTip__SkillToolTip_ret ]
    }
}


void AttachSkillToolTipMod() {
    PatchJmp(CUIToolTip__SkillToolTip_jmp, &CUIToolTip__SkillToolTip_hook); // 修复技能描述中文换行乱码的问题
    Patch1(CUIToolTip__SkillToolTip_jmp + 5, 0x90); // code cave overwrites 6 bytes, PatchJmp writes 5
}
