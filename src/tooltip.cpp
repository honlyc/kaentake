#include "pch.h"
#include "hook.h"
#include "wvs/secure.h"
#include "wvs/tooltip.h"
#include "wvs/iteminfo.h"
#include "ztl/ztl.h"


class GW_ItemSlotEquip {
public:
    MEMBER_AT(TSecType<int>, 0xC, nItemID)
    MEMBER_AT(ZtlSecurePacked<unsigned char>, 0x28, nRUC)
    MEMBER_AT(ZtlSecure<short>, 0x34, niSTR)
    MEMBER_AT(ZtlSecure<short>, 0x3C, niDEX)
    MEMBER_AT(ZtlSecure<short>, 0x44, niINT)
    MEMBER_AT(ZtlSecure<short>, 0x4C, niLUK)
    MEMBER_AT(ZtlSecure<short>, 0x54, niMaxHP)
    MEMBER_AT(ZtlSecure<short>, 0x5C, niMaxMP)
    MEMBER_AT(ZtlSecure<short>, 0x64, niPAD)
    MEMBER_AT(ZtlSecure<short>, 0x6C, niMAD)
    MEMBER_AT(ZtlSecure<short>, 0x74, niPDD)
    MEMBER_AT(ZtlSecure<short>, 0x7C, niMDD)
    MEMBER_AT(ZtlSecure<short>, 0x84, niACC)
    MEMBER_AT(ZtlSecure<short>, 0x8C, niEVA)
    MEMBER_AT(ZtlSecure<short>, 0x94, niCraft)
    MEMBER_AT(ZtlSecure<short>, 0x9C, niSpeed)
    MEMBER_AT(ZtlSecure<short>, 0xA4, niJump)
    MEMBER_AT(ZtlSecure<short>, 0xAC, nAttribute)
};


// ==================== 潜能显示（模拟数据） ====================
// TODO: 数据通路（封包死字段 / 旁路同步封包）确定后，用真实数据源替换 GetMockPotential

struct PotentialOptionInfo {
    int nOptionID;
    const char* sDesc;
};

const PotentialOptionInfo g_aPotentialOptionTable[] = {
    { 30001, "力量 : +6%" },
    { 30002, "敏捷 : +6%" },
    { 30003, "智力 : +6%" },
    { 30004, "运气 : +6%" },
    { 30005, "攻击力 : +12" },
    { 30006, "魔法攻击力 : +12" },
    { 30007, "MaxHP : +240" },
    { 30008, "MaxMP : +240" },
    { 30009, "物理防御力 : +6%" },
    { 30010, "魔法防御力 : +6%" },
};
const int g_nPotentialOptionCount = sizeof(g_aPotentialOptionTable) / sizeof(g_aPotentialOptionTable[0]);

const char* GetPotentialGradeName(int nGrade) {
    switch (nGrade) {
    case 1: return "稀有";
    case 2: return "史诗";
    case 3: return "传说";
    }
    return "";
}

const char* GetPotentialOptionDesc(int nOptionID) {
    for (int i = 0; i < g_nPotentialOptionCount; i++) {
        if (g_aPotentialOptionTable[i].nOptionID == nOptionID) {
            return g_aPotentialOptionTable[i].sDesc;
        }
    }
    return "";
}

// 模拟潜能数据：按 itemID 确定性生成，保证同一件装备每次悬停显示一致
// nGrade: 0 = 无潜能，1 = 稀有，2 = 史诗，3 = 传说
void GetMockPotential(int nItemID, int& nGrade, int anOption[3]) {
    nGrade = 0;
    anOption[0] = anOption[1] = anOption[2] = 0;
    if (nItemID % 5 == 0) {
        return; // 一部分装备无潜能，用于验证无潜能时不渲染
    }
    unsigned int uSeed = static_cast<unsigned int>(nItemID) * 2654435761u;
    nGrade = static_cast<int>(uSeed % 3) + 1;
    int nBase = static_cast<int>((uSeed >> 8) % g_nPotentialOptionCount);
    for (int i = 0; i < 3; i++) {
        anOption[i] = g_aPotentialOptionTable[(nBase + i * 3) % g_nPotentialOptionCount].nOptionID;
    }
}

// ==================== 潜能分隔线（Draw 阶段像素级绘制） ====================
// CUIToolTip::DrawToolTip_Equip 地址来自 SetEquipV3，已确认与 kaentake 同一客户端 build
static auto CUIToolTip__DrawToolTip_Equip =
    reinterpret_cast<void(__thiscall*)(CUIToolTip*, int, GW_ItemSlotEquip*)>(0x008ED0D2);

// 分隔线 Y 坐标校准常量（像素）：如线位置与潜能块不贴合，微调这两个值
constexpr int kToolTipRowHeight = 13;      // AddInfoEx 单行高度
constexpr int kPotentialLineGap = 2;       // 分隔线与潜能块之间的间距

// 由 SetToolTip_Equip_Basic_hook 记录，DrawToolTip_Equip_hook 读取
static int g_nPotentialPendingItemID = 0;
static int g_nPotentialLineY = 0;

void CUIToolTip::SetToolTip_Equip_Basic_hook(GW_ItemSlotEquip* pe) {
    int nItemID = pe->nItemID;
    auto pEquipItem = CItemInfo::GetInstance()->GetEquipItem(nItemID);
    if (!pEquipItem) {
        return;
    }
    // get_weapon_category_name
    ZXString<char> sWeaponCategory;
    reinterpret_cast<ZXString<char>*(__cdecl*)(ZXString<char>*, int)>(0x005C99FC)(&sWeaponCategory, nItemID);
    if (!sWeaponCategory.IsEmpty()) {
        AddInfoEx(14, 15, "武器分类 :", sWeaponCategory, 1, 1001);
    }
    // get_item_category_name
    ZXString<char> sItemCategory;
    reinterpret_cast<ZXString<char>*(__cdecl*)(ZXString<char>*, int)>(0x005C9E61)(&sItemCategory, nItemID);
    if (!sItemCategory.IsEmpty()) {
        AddInfoEx(14, 15, "装备分类 :", sItemCategory, 1, 1001);
    }
    // get_weapon_attack_speed
    ZXString<char> sAttackSpeed;
    reinterpret_cast<ZXString<char>*(__cdecl*)(ZXString<char>*, int)>(0x005C9AFA)(&sAttackSpeed, nItemID);
    if (!sAttackSpeed.IsEmpty()) {
        AddInfoEx(14, 15, "攻击速度 :", sAttackSpeed, 1, 1001);
    }

    PrintValueEx(PT_INC, pe->niSTR, pEquipItem->niSTR, "力量 :", 0);
    PrintValueEx(PT_INC, pe->niDEX, pEquipItem->niDEX, "敏捷 :", 0);
    PrintValueEx(PT_INC, pe->niINT, pEquipItem->niINT, "智力 :", 0);
    PrintValueEx(PT_INC, pe->niLUK, pEquipItem->niLUK, "运气 :", 0);
    PrintValueEx(PT_INC, pe->niMaxHP, pEquipItem->niMaxHP, "HP :", 0);
    PrintValueEx(PT_INC, pe->niMaxMP, pEquipItem->niMaxMP, "MP :", 0);

    PrintValueEx(PT_VALUE, pe->niPAD, pEquipItem->niPAD, "攻击力 :", 0);
    PrintValueEx(PT_VALUE, pe->niMAD, pEquipItem->niMAD, "魔法攻击力 :", 0);
    PrintValueEx(PT_VALUE, pe->niPDD, pEquipItem->niPDD, "物理防御力 :", 0);
    PrintValueEx(PT_VALUE, pe->niMDD, pEquipItem->niMDD, "魔法防御力 :", 0);

    PrintValueEx(PT_INC, pe->niACC, pEquipItem->niACC, "命中率 :", 0);
    PrintValueEx(PT_INC, pe->niEVA, pEquipItem->niEVA, "回避率 :", 0);
    PrintValueEx(PT_INC, pe->niCraft, pEquipItem->niCraft, "手技 :", 0);
    PrintValueEx(PT_INC, pe->niSpeed, pEquipItem->niSpeed, "移动速度 :", 0);
    PrintValueEx(PT_INC, pe->niJump, pEquipItem->niJump, "跳跃力 :", 0);

    PrintValue(PT_PERCENT, pEquipItem->nKnockback, "击退概率 :", 0);
    if (pe->nAttribute & 2) {
        AddInfoEx(14, 15, "增加防滑", "", 1, 1001);
    }
    if (pe->nAttribute & 4) {
        AddInfoEx(14, 15, "增加抗寒", "", 1, 1001);
    }
    if (pEquipItem->nRUC) {
        PrintValue(PT_VALUE, pe->nRUC, "装备可升级次数 :", 1);
    }

    // 潜能显示：潜能等级 + 3 条潜能属性；分隔线由 DrawToolTip_Equip_hook 像素级绘制
    int nPotentialGrade;
    int anPotentialOption[3];
    GetMockPotential(nItemID, nPotentialGrade, anPotentialOption);
    g_nPotentialPendingItemID = 0;
    if (nPotentialGrade > 0) {
        int nRowCount = 1;
        AddInfoEx(14, 15, "潜能等级 :", GetPotentialGradeName(nPotentialGrade), 1, 1001);
        for (int i = 0; i < 3; i++) {
            if (anPotentialOption[i]) {
                AddInfoEx(14, 15, GetPotentialOptionDesc(anPotentialOption[i]), "", 1, 1001);
                nRowCount++;
            }
        }
        // 此刻 m_nHeight 正好是潜能块底部（本函数返回后追加的金锤子/描述行都在其下方），
        // 在此捕获分隔线 Y，与尾部还会追加多少行无关
        g_nPotentialLineY = m_nHeight - kToolTipRowHeight * nRowCount - kPotentialLineGap;
        g_nPotentialPendingItemID = nItemID;
    }
}

// ==================== 装备有效期限日期格式修复 ====================

DWORD fixDateFormatRtnAddr = 0x008EBF65;
__declspec(naked) void fixDateFormat() {
    __asm {
        movzx   ecx, word ptr[ebp - 16h]
        push    ecx
        movzx   ecx, word ptr[ebp - 1Ah]
        push    ecx
        movzx   ecx, word ptr[ebp - 1Ch]
        jmp fixDateFormatRtnAddr
    }
}

DWORD fixDateFormat2RtnAddr = 0x008EBFAF;
__declspec(naked) void fixDateFormat2() {
    __asm {
        movzx   ecx, word ptr[ebp - 16h]
        push    ecx
        movzx   ecx, word ptr[ebp - 1Ah]
        push    ecx
        movzx   ecx, word ptr[ebp - 1Ch]
        jmp fixDateFormat2RtnAddr
    }
}

DWORD fixDateFormat3RtnAddr = 0x008EC328;
__declspec(naked) void fixDateFormat3() {
    __asm {
        movzx   ecx, word ptr[ebp - 1Eh]
        push    ecx
        movzx   ecx, word ptr[ebp - 22h]
        push    ecx
        movzx   ecx, word ptr[ebp - 24h]
        jmp fixDateFormat3RtnAddr
    }
}

DWORD fixDateFormat4RtnAddr = 0x008EBF13;
__declspec(naked) void fixDateFormat4() {
    __asm {
        movzx   ecx, word ptr[ebp - 16h]
        push    ecx
        movzx   ecx, word ptr[ebp - 1Ah]
        push    ecx
        movzx   ecx, word ptr[ebp - 1Ch]
        jmp fixDateFormat4RtnAddr
    }
}

// ==================== 装备类型中文修复 ====================

DWORD getItemType2Addr = 0x005CFAC2;
__declspec(naked) void getItemType1() {
    __asm {
        jmp getItemType2Addr
    }
}

DWORD getItemType2ErrRtnAddr = 0x005CFAA8;
DWORD getItemType2RtnAddr = 0x005CFADD;
__declspec(naked) void getItemType2() {
    __asm {
        dec eax
        jz label_eqp
        dec eax
        jz label_use
        dec eax
        jz label_ins
        dec eax
        jz label_etc
        dec eax
        jz label_cash
        jmp getItemType2ErrRtnAddr
    label_cash:
        push 0x159C
        jmp getItemType2RtnAddr
    label_etc:
        push 0x6DD
        jmp getItemType2RtnAddr
    label_ins:
        push 0x0B
        jmp getItemType2RtnAddr
    label_use:
        push 0x6E3
        jmp getItemType2RtnAddr
    label_eqp:
        push 0x6D9
        jmp getItemType2RtnAddr
    }
}
//中文换行
int charLen = 55;
void calcCharLen(const char* word) {
    // std::cout << "文字 " << word << std::endl;
    const std::string str = std::string(word);
    auto firstByte = static_cast<unsigned char>(str[0]);
    // auto secondByte = static_cast<unsigned char>(str[55]);

    if (str.length() < 55) {
        charLen = 55;
        return;
    }
    for (int i = 0; i < 60; i++) {
        firstByte = static_cast<unsigned char>(str[i]);
        if (firstByte >= 0x81 && firstByte <= 0xFE) {
            i++; // 是中文字符跳过双字节
            continue;
        }
        if (i >= 55) {
            charLen = i;
            break;
        }
    }
}

constexpr DWORD skillToolTipNewRtn = 0x008F3844;
__declspec(naked) void skillToolTipNew() {
    __asm {
                mov eax, [ebp + 0Ch]
                push eax
                call calcCharLen
                pop eax
                mov eax, charLen
                mov[ebp - 1Ch], eax
                lea eax, [ebp - 30h]
                jmp skillToolTipNewRtn
    }
}
// 在原生绘制完成后，于潜能块上方画一条像素级分隔线（白线，左右各留 6px）
void __fastcall CUIToolTip__DrawToolTip_Equip_hook(CUIToolTip* pThis, void* _EDX, int a2, GW_ItemSlotEquip* pe) {
    CUIToolTip__DrawToolTip_Equip(pThis, a2, pe);
    if (!pe || !pThis->m_pLayer) {
        return;
    }
    int nItemID = pe->nItemID;
    if (nItemID != g_nPotentialPendingItemID) {
        return;
    }
    IWzCanvasPtr pCanvas = pThis->m_pLayer->canvas[0];
    if (!pCanvas) {
        return;
    }
    // 使用 Set 阶段捕获的 Y：潜能块之后引擎还会追加金锤子/描述等行，不能从 m_nHeight 底部反推
    pCanvas->DrawRectangle(6, g_nPotentialLineY, pThis->m_nWidth - 12, 1, 0xFFFFFFFF);
}

void AttachToolTipMod() {
    ATTACH_HOOK(CUIToolTip::SetToolTip_Equip_Basic, CUIToolTip::SetToolTip_Equip_Basic_hook);
    ATTACH_HOOK(CUIToolTip__DrawToolTip_Equip, CUIToolTip__DrawToolTip_Equip_hook);
    // 装备有效期限日期格式修复
    CodeCave(0x008EBF57, fixDateFormat, 14);  // StringPool 5273
    CodeCave(0x008EBFA1, fixDateFormat2, 14); // StringPool 655
    CodeCave(0x008EC31A, fixDateFormat3, 14); // StringPool 679
    CodeCave(0x008EBF05, fixDateFormat4, 14); // StringPool 3138

    // 装备类型中文修复
    CodeCave(0x005CFA99, getItemType1, 15);
    CodeCave(getItemType2Addr, getItemType2, 27);

   //中文换行乱码
    CodeCave(0x008F383E, skillToolTipNew, 6);
}

