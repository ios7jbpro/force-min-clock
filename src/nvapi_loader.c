#include "nvapi_loader.h"

#ifdef HAS_NVAPI

#include <strsafe.h>

static HMODULE g_nvapiDll = NULL;
static PFN_NvAPI_Initialize pNvAPI_Initialize = NULL;
static PFN_NvAPI_Unload pNvAPI_Unload = NULL;
static PFN_NvAPI_EnumPhysicalGPUs pNvAPI_EnumPhysicalGPUs = NULL;
static PFN_NvAPI_GPU_GetFullName pNvAPI_GPU_GetFullName = NULL;
static PFN_NvAPI_GPU_GetPstates20 pNvAPI_GPU_GetPstates20 = NULL;

NvAPI_Status NvAPI_Initialize(void) {
    return pNvAPI_Initialize ? pNvAPI_Initialize() : -1;
}

NvAPI_Status NvAPI_Unload(void) {
    return pNvAPI_Unload ? pNvAPI_Unload() : -1;
}

NvAPI_Status NvAPI_EnumPhysicalGPUs(NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS], NvU32 *pGpuCount) {
    return pNvAPI_EnumPhysicalGPUs ? pNvAPI_EnumPhysicalGPUs(nvGPUHandle, pGpuCount) : -1;
}

NvAPI_Status NvAPI_GPU_GetFullName(NvPhysicalGpuHandle hPhysicalGpu, NvAPI_ShortString szName) {
    return pNvAPI_GPU_GetFullName ? pNvAPI_GPU_GetFullName(hPhysicalGpu, szName) : -1;
}

NvAPI_Status NvAPI_GPU_GetPstates20(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_PERF_PSTATES20_INFO *pPstatesInfo) {
    return pNvAPI_GPU_GetPstates20 ? pNvAPI_GPU_GetPstates20(hPhysicalGpu, pPstatesInfo) : -1;
}

BOOL NvApiLoadLibrary(void) {
    g_nvapiDll = LoadLibraryW(L"nvapi64.dll");
    if (!g_nvapiDll)
        g_nvapiDll = LoadLibraryW(L"nvapi.dll");
    if (!g_nvapiDll)
        return FALSE;

    typedef void* (*PFN_nvapi_QueryInterface)(unsigned int id);
    PFN_nvapi_QueryInterface queryInterface = (PFN_nvapi_QueryInterface)GetProcAddress(g_nvapiDll, "nvapi_QueryInterface");
    if (!queryInterface)
        return FALSE;

    pNvAPI_Initialize = (PFN_NvAPI_Initialize)queryInterface(0x0150e828);
    pNvAPI_Unload = (PFN_NvAPI_Unload)queryInterface(0xd22bdd7e);
    pNvAPI_EnumPhysicalGPUs = (PFN_NvAPI_EnumPhysicalGPUs)queryInterface(0xe5ac921f);
    pNvAPI_GPU_GetFullName = (PFN_NvAPI_GPU_GetFullName)queryInterface(0xceee8e9f);
    pNvAPI_GPU_GetPstates20 = (PFN_NvAPI_GPU_GetPstates20)queryInterface(0x6ff81213);

    return (pNvAPI_Initialize && pNvAPI_Unload && pNvAPI_EnumPhysicalGPUs &&
            pNvAPI_GPU_GetFullName && pNvAPI_GPU_GetPstates20);
}

void NvApiFreeLibrary(void) {
    if (g_nvapiDll) {
        FreeLibrary(g_nvapiDll);
        g_nvapiDll = NULL;
    }
}

#endif
