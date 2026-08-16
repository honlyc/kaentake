#pragma once
#include <string>

class ClientConfig {
public:
    static int MsgAmount;
    static bool CustomLoginFrame;
    static bool WindowedMode;
    static bool RemoveLogos;
    static int setDamageCap;
    static int setMAtkCap;
    static int setAtkCap;
    static int setAccCap;
    static int setAvdCap;
    static double setAtkOutCap;
    static bool useTubi;
    static bool bigLoginFrame;
    static bool SwitchChinese;
    static int speedMovementCap;
    static bool noPassword;
    static bool debug;
    static bool climbSpeedAuto;
    static float climbSpeed;
    static unsigned char imeType;
    static std::string ServerIP_Address;
    static int serverIP_Port;
    static bool talkRepeat;
    static int talkTime;

    static void Init();
    static void ApplyPatches();
};