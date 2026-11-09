#include "audio.h"

std::wstring GetDefaultDirectShowAudioDevice() {
    ICreateDevEnum* pDevEnum = NULL;
    IEnumMoniker* pEnum = NULL;
    std::wstring deviceName = L"";

    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
        CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pDevEnum);
    if (SUCCEEDED(hr)) {
        hr = pDevEnum->CreateClassEnumerator(CLSID_AudioInputDeviceCategory, &pEnum, 0);
        if (hr == S_OK) {
            IMoniker* pMoniker = NULL;
            if (pEnum->Next(1, &pMoniker, NULL) == S_OK) {
                IPropertyBag* pPropBag = NULL;
                hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);
                if (SUCCEEDED(hr)) {
                    VARIANT var;
                    VariantInit(&var);
                    hr = pPropBag->Read(L"FriendlyName", &var, 0);
                    if (SUCCEEDED(hr) && var.bstrVal) {
                        deviceName = var.bstrVal;
                        VariantClear(&var);
                    }
                    pPropBag->Release();
                }
                pMoniker->Release();
            }
            pEnum->Release();
        }
        pDevEnum->Release();
    }
    return deviceName;
}
