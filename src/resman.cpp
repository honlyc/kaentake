#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "wvs/wvsapp.h"
#include "wvs/util.h"
#include "ztl/ztl.h"
#include <algorithm>
#include <vector>
#include <tuple>


static IWzNameSpacePtr g_pCustomNameSpace;
static std::vector<Ztl_bstr_t> g_vecOverrides;

class IWzNameSpaceImpl {
public:
    typedef HRESULT(__stdcall* raw__OnGetLocalObject_t)(IWzNameSpaceImpl*, int, BSTR, int*, VARIANT*);
    inline static raw__OnGetLocalObject_t raw__OnGetLocalObject_orig;

    HRESULT __stdcall raw__OnGetLocalObject_hook(int nIndex, BSTR sPath, int* pnPathUsed, VARIANT* pvRet) {
        HRESULT hr = raw__OnGetLocalObject_orig(this, nIndex, sPath, pnPathUsed, pvRet);
        if (SUCCEEDED(hr)) {
            return hr;
        }
        if (!std::binary_search(g_vecOverrides.begin(), g_vecOverrides.end(), Ztl_bstr_t(sPath))) {
            return hr;
        }
        return g_pCustomNameSpace->raw__OnGetLocalObject(nIndex, sPath, pnPathUsed, pvRet);
    }
};

class CWzProperty : public IWzProperty {
public:
    typedef HRESULT(__stdcall* raw_Serialize_t)(CWzProperty*, IWzArchive*);
    inline static raw_Serialize_t raw_Serialize_orig;

    HRESULT __stdcall raw_Serialize_hook(IWzArchive* pArchive) {
        HRESULT hr = raw_Serialize_orig(this, pArchive);
        if (FAILED(hr)) {
            return hr;
        }
        if (!std::binary_search(g_vecOverrides.begin(), g_vecOverrides.end(), pArchive->absoluteUOL)) {
            return hr;
        }
        IWzPropertyPtr pProperty = get_rm()->GetObjectA(L"Custom/" + pArchive->absoluteUOL).GetUnknown();
        IEnumVARIANTPtr pEnum = pProperty->_NewEnum;
        while (true) {
            Ztl_variant_t vNext;
            ULONG uCeltFetched;
            if (FAILED(pEnum->Next(1, &vNext, &uCeltFetched)) || uCeltFetched == 0) {
                break;
            }
            Ztl_bstr_t sNext = V_BSTR(&vNext);
            IUnknownPtr pUnk = pProperty->item[sNext].GetUnknown();
            IWzPropertyPtr pSub;
            if (!pUnk || FAILED(pUnk->QueryInterface(&pSub))) {
                this->Add(sNext, pProperty->item[sNext], false);
            }
        }
        return S_OK;
    }
};


void CWvsApp::InitializeResMan_hook() {
    LOG_INFO("CWvsApp::InitializeResMan_hook - Start");
    LOG_INFO("CWvsApp::InitializeResMan_hook - Calling original InitializeResMan");
    try {
        IWzResManPtr& rm = get_rm();
        PcCreateObject<IWzResManPtr>(L"ResMan", rm, nullptr);
        rm->SetResManParam(static_cast<RESMAN_PARAM>(RESMAN_PARAM::RC_AUTO_REPARSE | RESMAN_PARAM::RC_AUTO_SERIALIZE), -1, -1);

        IWzNameSpacePtr& root = get_root();
        PcCreateObject<IWzNameSpacePtr>(L"NameSpace", root, nullptr);
        PcSetRootNameSpace(root);

        IWzFileSystemPtr fs;
        PcCreateObject<IWzFileSystemPtr>(L"NameSpace#FileSystem", fs, nullptr);
        char sStartPath[MAX_PATH];
        GetModuleFileNameA(nullptr, sStartPath, MAX_PATH);
        Dir_BackSlashToSlash(sStartPath);
        Dir_upDir(sStartPath);
        strcat_s(sStartPath, MAX_PATH, "/Data");
        fs->Init(Ztl_bstr_t(sStartPath));
        root->Mount(L"/", fs, 0);
    } catch (const _com_error& e) {
        HRESULT hr = e.Error();
        ZException exception(hr);
        if (hr == 0x80070005) {
            hr = 0x22000005; // EC_INVALID_GAME_DATA_VERSION
        } else if (hr == 0x80070057) {
            hr = 0x22000003; // EC_NOT_ENOUGH_MEMORY
        } else {
            hr = 0x22000004; // EC_NO_DATA_PACAKGE
        }
        // CTerminateException::CTerminateException(&exception, hr);
        reinterpret_cast<void(__thiscall*)(void*, HRESULT)>(0x00401D50)(&exception, hr);
        throw exception;
    }
    LOG_INFO("CWvsApp::InitializeResMan_hook - Skipping Custom.wz loading for img client compatibility");
    LOG_INFO("CWvsApp::InitializeResMan_hook - Complete");
}

void CWvsApp::CleanUp_hook() {
    CWvsApp::CleanUp(this);
    g_pCustomNameSpace = nullptr;
}


void AttachResManMod() {
    ATTACH_HOOK(CWvsApp::InitializeResMan, CWvsApp::InitializeResMan_hook);
    ATTACH_HOOK(CWvsApp::CleanUp, CWvsApp::CleanUp_hook);
}