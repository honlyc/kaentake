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
        DEBUG_MESSAGE("OnGetLocalObject_hook: fallback to Custom for %ls (hr=0x%08X)", sPath, (unsigned int)hr);
        HRESULT hr2 = g_pCustomNameSpace->raw__OnGetLocalObject(nIndex, sPath, pnPathUsed, pvRet);
        DEBUG_MESSAGE("OnGetLocalObject_hook: Custom result hr=0x%08X for %ls", (unsigned int)hr2, sPath);
        return hr2;
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
        DEBUG_MESSAGE("Serialize_hook: override for %ls", pArchive->absoluteUOL.GetBSTR());
        IWzPropertyPtr pProperty = get_rm()->GetObjectA(L"Custom/" + pArchive->absoluteUOL).GetUnknown();
        if (!pProperty) {
            DEBUG_MESSAGE("Serialize_hook: FAILED to get Custom property for %ls", pArchive->absoluteUOL.GetBSTR());
            return hr;
        }
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
    LOG_DEBUG("InitializeResMan: begin");
    // can change/move this variable to whatever you wish
    bool bUseFileSystem = true;
 
    // basic data mounting
    char sStartPath[MAX_PATH];
    GetModuleFileNameA(nullptr, sStartPath, MAX_PATH);
    Dir_BackSlashToSlash(sStartPath);
    Dir_upDir(sStartPath);
    if (bUseFileSystem) {
        // IMG format referred to kinoko_client/src/resman.cpp
        IWzResManPtr& rm = get_rm();
        PcCreateObject<IWzResManPtr>(L"ResMan", rm, nullptr);
        rm->SetResManParam(static_cast<RESMAN_PARAM>(RESMAN_PARAM::RC_AUTO_REPARSE | RESMAN_PARAM::RC_AUTO_SERIALIZE), -1, -1);
        IWzNameSpacePtr& root = get_root();
        PcCreateObject<IWzNameSpacePtr>(L"NameSpace", root, nullptr);
        PcSetRootNameSpace(root);
        IWzFileSystemPtr fs;
        PcCreateObject<IWzFileSystemPtr>(L"NameSpace#FileSystem", fs, nullptr);
        char sDataPath[MAX_PATH];
        sprintf_s(sDataPath, "%s/Data", sStartPath);
        fs->Init(Ztl_bstr_t(sDataPath));
        root->Mount(L"/", fs, 0);
    } else {
        // WZ format just call the original function
        CWvsApp::InitializeResMan(this);
        // planned to copy codes in kinoko_client/src/resman.cpp as above and just rewrite the whole function instead of doing this
        // not necessary but still calling teto god for assistance on this part T^T
        // get_sub() function address of v83 is 9F7A2F and the constant in there is 0xBF14A4
    }
    LOG_DEBUG("InitializeResMan: original InitializeResMan done");
    
    // add custom namespace to root
    IWzWritableNameSpacePtr pWritableRoot;
    if (FAILED(get_root()->QueryInterface(&pWritableRoot))) {
        ErrorMessage("Failed to cast root namespace");
        return;
    }
    IWzNameSpacePtr pNameSpace;
    PcCreateObject<IWzNameSpacePtr>(L"NameSpace", pNameSpace, nullptr);
    Ztl_variant_t vResult;
    pWritableRoot->AddObject(L"Custom", static_cast<IUnknown*>(pNameSpace), &vResult);
    g_pCustomNameSpace = vResult.GetUnknown();
 
    if (bUseFileSystem) {
        // IMG format mounting Data/Custom directory, with IWzFileSystem
        IWzFileSystemPtr fsCustom;
        PcCreateObject<IWzFileSystemPtr>(L"NameSpace#FileSystem", fsCustom, nullptr);
        char sCustomPath[MAX_PATH];
        sprintf_s(sCustomPath, "%s/Data/Custom", sStartPath);
        fsCustom->Init(Ztl_bstr_t(sCustomPath));
        g_pCustomNameSpace->Mount(L"/", fsCustom, 1);
    } else {
        // WZ format mounting Custom.wz, with IWzPackage and IWzSeekableArchive
        IWzFileSystemPtr fs;
        PcCreateObject<IWzFileSystemPtr>(L"NameSpace#FileSystem", fs, nullptr);
        fs->Init(sStartPath);
 
        IWzPackagePtr pPackage;
        PcCreateObject<IWzPackagePtr>(L"NameSpace#Package", pPackage, nullptr);
        IWzSeekableArchivePtr pArchive = fs->item[L"Custom.wz"].GetUnknown();
        if (pArchive) {
            pPackage->Init(L"83", L"Custom", pArchive);
            g_pCustomNameSpace->Mount(L"/", pPackage, 1);
        }
    }
 
    // iterate custom namespace
    std::vector<std::tuple<Ztl_bstr_t, IEnumVARIANTPtr>> stack;
    stack.emplace_back(L"", g_pCustomNameSpace->_NewEnum);
    while (!stack.empty()) {
        auto [sPath, pEnum] = stack.back();
        stack.pop_back();
 
        while (true) {
            Ztl_variant_t vNext;
            ULONG uCeltFetched;
            if (FAILED(pEnum->Next(1, &vNext, &uCeltFetched)) || uCeltFetched == 0) {
                break;
            }
            Ztl_bstr_t sUOL = (sPath.length() > 0 ? sPath + L"/" : L"") + V_BSTR(&vNext);
            Ztl_variant_t vObj = get_rm()->GetObjectA(L"Custom/" + sUOL);
            IUnknownPtr pUnk = vObj.GetUnknown();
            if (pUnk) {
                IWzNameSpacePtr pSub;
                if (SUCCEEDED(pUnk->QueryInterface(&pSub))) {
                    stack.emplace_back(sUOL, pSub->_NewEnum);
                    continue;
                }
                IWzPropertyPtr pProp;
                if (SUCCEEDED(pUnk->QueryInterface(&pProp))) {
                    stack.emplace_back(sUOL, pProp->_NewEnum);
                }
            }
            g_vecOverrides.push_back(sUOL);
        }
        std::sort(g_vecOverrides.begin(), g_vecOverrides.end());
    }

    // NameSpace.dll - try resolving from g_pCustomNameSpace
    IWzNameSpaceImpl::raw__OnGetLocalObject_orig = static_cast<IWzNameSpaceImpl::raw__OnGetLocalObject_t>(GetAddressByPattern("NAMESPACE.DLL", "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 81 EC 80"));
    LOG_DEBUG("InitializeResMan: OnGetLocalObject pattern -> %p", (void*)IWzNameSpaceImpl::raw__OnGetLocalObject_orig);
    ATTACH_HOOK(IWzNameSpaceImpl::raw__OnGetLocalObject_orig, IWzNameSpaceImpl::raw__OnGetLocalObject_hook);

    // PCOM.dll - patch CWzProperty objects during serialization
    CWzProperty::raw_Serialize_orig = static_cast<CWzProperty::raw_Serialize_t>(GetAddressByPattern("PCOM.DLL", "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 EC 68"));
    LOG_DEBUG("InitializeResMan: Serialize pattern -> %p", (void*)CWzProperty::raw_Serialize_orig);
    ATTACH_HOOK(CWzProperty::raw_Serialize_orig, CWzProperty::raw_Serialize_hook);
    LOG_DEBUG("InitializeResMan: done");
}

void CWvsApp::CleanUp_hook() {
    CWvsApp::CleanUp(this);
    g_pCustomNameSpace = nullptr;
}


void AttachResManMod() {
    ATTACH_HOOK(CWvsApp::InitializeResMan, CWvsApp::InitializeResMan_hook);
    ATTACH_HOOK(CWvsApp::CleanUp, CWvsApp::CleanUp_hook);
}