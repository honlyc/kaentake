#include "pch.h"
#include "ClientConfig.h"
#include "hook.h"
#include "constants.h"
#include "debug.h"

int ClientConfig::MsgAmount = 26;
bool ClientConfig::CustomLoginFrame = true;
bool ClientConfig::WindowedMode = true;
bool ClientConfig::RemoveLogos = true;
int ClientConfig::setDamageCap = 19999999;
int ClientConfig::setMAtkCap = 1999999;
int ClientConfig::setAtkCap = 1999999;
int ClientConfig::setAccCap = 999;
int ClientConfig::setAvdCap = 999;
double ClientConfig::setAtkOutCap = 199999999.0;
bool ClientConfig::useTubi = false;
bool ClientConfig::bigLoginFrame = true;
bool ClientConfig::SwitchChinese = true;
int ClientConfig::speedMovementCap = 140;
bool ClientConfig::noPassword = false;
bool ClientConfig::debug = false;
bool ClientConfig::climbSpeedAuto = false;
float ClientConfig::climbSpeed = 3.0f;
unsigned char ClientConfig::imeType = 1;
std::string ClientConfig::ServerIP_Address = "127.0.0.1";
int ClientConfig::serverIP_Port = 8484;
bool ClientConfig::talkRepeat = false;
int ClientConfig::talkTime = 2000;

// 琛ヤ竵鍒嗙粍寮�鍏筹紙浠? config.ini [client] 璇诲彇锛?1=鍚敤 0=绂佺敤锛?
static bool g_EnableManifestAdmin = true;
static bool g_EnableServerIP = true;
static bool g_EnableCaps = true;
static bool g_EnableSkillTweaks = true;
static bool g_EnableWindowLogos = true;
static bool g_EnableClimbSpeed = true;
static bool g_EnableAssassinateTweaks = true;

static bool GetBool(const char* key, bool defVal) {
    char sBuffer[32];
    char sDef[8];
    StringCbPrintfA(sDef, sizeof(sDef), "%d", defVal ? 1 : 0);
    GetPrivateProfileStringA("client", key, sDef, sBuffer, sizeof(sBuffer), ".\\" CONSTANTS_CONFIG_NAME);
    return atoi(sBuffer) != 0;
}

static int GetInt(const char* key, int defVal) {
    char sBuffer[32];
    char sDef[16];
    StringCbPrintfA(sDef, sizeof(sDef), "%d", defVal);
    GetPrivateProfileStringA("client", key, sDef, sBuffer, sizeof(sBuffer), ".\\" CONSTANTS_CONFIG_NAME);
    return atoi(sBuffer);
}

void ClientConfig::Init() {
    // 璇诲彇鍔熻兘寮�鍏?
    g_EnableManifestAdmin = GetBool("patch_manifest", true);
    g_EnableServerIP      = GetBool("patch_server_ip", true);
    g_EnableCaps          = GetBool("patch_caps", true);
    g_EnableSkillTweaks   = GetBool("patch_skill_tweaks", true);
    g_EnableWindowLogos   = GetBool("patch_window_logos", true);
    g_EnableClimbSpeed    = GetBool("patch_climb_speed", true);
    g_EnableAssassinateTweaks = GetBool("patch_assassinate_tweaks", true);

    // 璇诲彇鍔熻兘鍙傛暟
    WindowedMode     = GetBool("windowed_mode", true);
    RemoveLogos      = GetBool("remove_logos", true);
    useTubi          = GetBool("use_tubi", false);
    setDamageCap     = GetInt("damage_cap", 19999999);
    setMAtkCap       = GetInt("matk_cap", 1999999);
    setAtkCap        = GetInt("atk_cap", 1999999);
    setAccCap        = GetInt("acc_cap", 999);
    setAvdCap        = GetInt("avd_cap", 999);
    speedMovementCap = GetInt("speed_movement_cap", 140);
    serverIP_Port    = GetInt("server_port", 8484);

    char sIP[64];
    GetPrivateProfileStringA("client", "server_ip", "127.0.0.1", sIP, sizeof(sIP), ".\\" CONSTANTS_CONFIG_NAME);
    ServerIP_Address = sIP;

    char sClimb[32];
    GetPrivateProfileStringA("client", "climb_speed", "3.0", sClimb, sizeof(sClimb), ".\\" CONSTANTS_CONFIG_NAME);
    climbSpeed = static_cast<float>(atof(sClimb));

    ApplyPatches();
}

void ClientConfig::ApplyPatches() {
    if (g_EnableManifestAdmin) {
    LOG_DEBUG("ApplyPatches: stage 1 (manifest/admin)");
    FillBytes(0x00C08459, 0x20, 0x00C0846E - 0x00C08459);
    Patch1(0x00C08459, 0x22);
    PatchStr(0x00C08459 + 1, "asInvoker");
    Patch1(0x00C08463, 0x22);
    Patch1(0x0049C2CD + 1, 0x01);
    Patch1(0x0049CFE8 + 1, 0x01);
    Patch1(0x0049D398 + 1, 0x01);
    }

    if (g_EnableServerIP) {
    LOG_DEBUG("ApplyPatches: stage 2 (server IP)");
    FillBytes(0x00AFE084, 0x00, 0x006FE0B2 - 0x006FE084);
    const char* serverIP = ServerIP_Address.c_str();
    PatchStr(0x00AFE084, serverIP);
    PatchStr(0x00AFE084 + 16, serverIP);
    PatchStr(0x00AFE084 + 32, serverIP);
    Patch4(0x007519C1 + 1, serverIP_Port);
    }

    if (useTubi) {
        LOG_DEBUG("ApplyPatches: tubi");
        FillBytes(0x00485C32, 0x90, 2);
    }

    if (g_EnableCaps) {
    LOG_DEBUG("ApplyPatches: stage 3 (caps)");
    Patch4(0x0077E055 + 1, 2147483646);
    Patch4(0x0077E12F + 1, 2147483646);
    Patch4(0x008C3304 + 1, setDamageCap);
    Patch4(0x0077E215 + 1, setMAtkCap);
    Patch4(0x00780620 + 1, setMAtkCap);
    Patch4(0x007806D0 + 1, setAccCap);
    Patch4(0x00780702 + 1, setAvdCap);
    Patch4(0x0078FF5F + 1, 2147483646);
    Patch4(0x0079166C + 1, 2147483646);
    Patch4(0x00791CD5 + 1, 2147483646);
    Patch4(0x0078E061 + 1, 2147483646);
    Patch4(0x0078E67D + 1, 2147483646);
    Patch4(0x007918FC + 1, 2147483646);

    unsigned char doubleBytes[8];
    *(double*)doubleBytes = setAtkOutCap;
    PatchMemory((void*)0x00AFE8A0, doubleBytes, 8);

    Patch4(0x00780743 + 3, speedMovementCap);
    Patch4(0x008C4286 + 1, speedMovementCap);
    Patch4(0x0094D91E + 1, speedMovementCap);

    unsigned char corkscrewBlow[] = { 0xE9, 0x19, 0x00, 0x00 };
    PatchMemory((void*)0x00968278, corkscrewBlow, sizeof(corkscrewBlow));
    unsigned char backspinDelay[] = { 0xFE, 0x00 };
    PatchMemory((void*)((uintptr_t)0x00953054 + 1), backspinDelay, sizeof(backspinDelay));
    unsigned char backspinCooltime[] = { 0xB3, 0x04 };
    PatchMemory((void*)((uintptr_t)0x009530FB + 2), backspinCooltime, sizeof(backspinCooltime));
    unsigned char doubleupper[] = { 0x00, 0x01 };
    PatchMemory((void*)((uintptr_t)0x00953688 + 1), doubleupper, sizeof(doubleupper));
    }

    if (g_EnableSkillTweaks) {
    LOG_DEBUG("ApplyPatches: stage 4 (skill tweaks)");
    PatchNop(0x009A4482, 0x009A4484);

    PatchNop(0x008E4252, 0x008E4254);
    PatchNop(0x008AD21F, 0x008AD224);	//移除一转技能点不足检测

    Patch1(0x0068DE1F + 1, 0x86);
    Patch1(0x0068DFBD + 1, 0x86);
    Patch1(0x0068E0E7 + 1, 0x86);
    Patch1(0x0068E534 + 1, 0x86);
    Patch1(0x0068E65D + 1, 0x86);
    Patch1(0x0068E709 + 1, 0x86);
    }

    if (g_EnableWindowLogos) {
    if (WindowedMode) {
        LOG_DEBUG("ApplyPatches: windowed mode");
        unsigned char forced_window[] = { 0xb8, 0x00, 0x00, 0x00, 0x00 };
        PatchMemory((void*)0x009F7A9B, forced_window, sizeof(forced_window));
    }

    if (RemoveLogos) {
        LOG_DEBUG("ApplyPatches: remove logos");
        FillBytes(0x0062EE54, 0x90, 21);
    }
    }

    if (g_EnableClimbSpeed) {
    LOG_DEBUG("ApplyPatches: stage 5 (climb speed)");
    Patch4(0x009CC6F9 + 2, 0x00C1CF80);
    unsigned char climbSpeedBytes[8];
    *(double*)climbSpeedBytes = climbSpeed * 3.0f;
    PatchMemory((void*)0x00C1CF80, climbSpeedBytes, 8);
    }
    if (g_EnableAssassinateTweaks) {
    LOG_DEBUG("ApplyPatches: stage 6 (assassinate tweaks)");

    // 移除暗杀技能的隐身术(Dark Sight)前置要求
    Patch1(0x0096953E, 0xEB);

    // 禁用暗杀最后一击的冲刺位移
    Patch1(0x00952D9B, 0xEB);

    // 禁用隐身术状态下暗杀的额外伤害加成 (CalcDamage::PDamage)
    unsigned char jumpPatch[] = { 0xE9, 0xEE, 0x00, 0x00, 0x00, 0x90 };
    PatchMemory((void*)0x0078EDCE, jumpPatch, sizeof(jumpPatch));
    Patch1(0x00790107, 0xEB);
    Patch1(0x00790296, 0xEB);

    // 允许在空中使用暗杀技能
    FillBytes(0x00950B42, 0x90, 6);

    // 移除暗杀时的"隐身术"视觉特效 (12 = 无效技能ID，不显示效果)
    Patch4(0x00951A32 + 1, 12);
    }

    LOG_DEBUG("ApplyPatches: done");
}