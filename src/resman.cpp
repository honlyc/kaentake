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
    CWvsApp::InitializeResMan(this);
    LOG_DEBUG("InitializeResMan: original InitializeResMan done");

// add custom namespace to root
    IWzWritableNameSpacePtr pWritableRoot;
    if (FAILED(get_root()->QueryInterface(&pWritableRoot))) {
        ErrorMessage("Failed to cast root namespace");
        return;
    }
    LOG_DEBUG("InitializeResMan: got writable root namespace");

    IWzNameSpacePtr pNameSpace;
    PcCreateObject<IWzNameSpacePtr>(L"NameSpace", pNameSpace, nullptr);
    Ztl_variant_t vResult;
    pWritableRoot->AddObject(L"Custom", static_cast<IUnknown*>(pNameSpace), &vResult);
    g_pCustomNameSpace = vResult.GetUnknown();
    LOG_DEBUG("InitializeResMan: Custom namespace added to root (ptr=%p)", (void*)g_pCustomNameSpace.GetInterfacePtr());

    // load Custom.wz from file system
    IWzFileSystemPtr fs;
    PcCreateObject<IWzFileSystemPtr>(L"NameSpace#FileSystem", fs, nullptr);
    char sStartPath[MAX_PATH];
    GetModuleFileNameA(nullptr, sStartPath, MAX_PATH);
    Dir_BackSlashToSlash(sStartPath);
    Dir_upDir(sStartPath);
    strcat_s(sStartPath, MAX_PATH, "/Custom");
    LOG_DEBUG("InitializeResMan: FileSystem path = %s", sStartPath);
    fs->Init(Ztl_bstr_t(sStartPath));
    LOG_DEBUG("InitializeResMan: FileSystem initialized");

    // IWzPackagePtr pPackage;
    // PcCreateObject<IWzPackagePtr>(L"NameSpace#Package", pPackage, nullptr);
    // IWzSeekableArchivePtr pArchive = fs->item[L"Custom.wz"].GetUnknown();
    // pPackage->Init(L"83", L"Custom", pArchive);
    g_pCustomNameSpace->Mount(L"/", fs, 0);
    LOG_DEBUG("InitializeResMan: FileSystem mounted to Custom namespace");

    // iterate custom namespace
    std::vector<std::tuple<Ztl_bstr_t, IEnumVARIANTPtr>> stack;
    stack.emplace_back(L"", g_pCustomNameSpace->_NewEnum);
    LOG_DEBUG("InitializeResMan: iterating custom namespace");
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
    }
    std::sort(g_vecOverrides.begin(), g_vecOverrides.end()); // uses operator<
    LOG_DEBUG("InitializeResMan: found %d override entries", (int)g_vecOverrides.size());
    for (int i = 0; i < (int)g_vecOverrides.size() && i < 20; ++i) {
        LOG_DEBUG("InitializeResMan: override[%d] = %ls", i, g_vecOverrides[i].GetBSTR());
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