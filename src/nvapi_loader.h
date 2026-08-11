#pragma once

#ifdef HAS_NVAPI

#include <windows.h>

typedef int NvAPI_Status;
#define NVAPI_OK 0

typedef void* NvPhysicalGpuHandle;
typedef void* NvDRSSessionHandle;
typedef void* NvDRSProfileHandle;

typedef char NvAPI_ShortString[64];
typedef unsigned int NvU32;
typedef int NvS32;

#define NVAPI_MAX_PHYSICAL_GPUS 64
#define NVAPI_MAX_GPU_PUBLIC_CLOCKS 32
#define NVAPI_MAX_GPU_PSTATE20_PSTATES 16
#define NVAPI_MAX_GPU_PSTATE20_CLOCKS 8
#define NVAPI_MAX_GPU_PSTATE20_BASE_VOLTAGES 4

typedef enum {
    NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS = 0,
    NVAPI_GPU_PUBLIC_CLOCK_MEMORY = 4,
    NVAPI_GPU_PUBLIC_CLOCK_PROCESSOR = 7,
    NVAPI_GPU_PUBLIC_CLOCK_VIDEO = 8,
    NVAPI_GPU_PUBLIC_CLOCK_UNDEFINED = NVAPI_MAX_GPU_PUBLIC_CLOCKS,
} NV_GPU_PUBLIC_CLOCK_ID;

typedef enum {
    NVAPI_GPU_PERF_PSTATE_P0 = 0,
    NVAPI_GPU_PERF_PSTATE_UNDEFINED = NVAPI_MAX_GPU_PUBLIC_CLOCKS,
    NVAPI_GPU_PERF_PSTATE_ALL,
} NV_GPU_PERF_PSTATE_ID;

typedef enum {
    NVAPI_GPU_PERF_VOLTAGE_INFO_DOMAIN_CORE = 0,
    NVAPI_GPU_PERF_VOLTAGE_INFO_DOMAIN_UNDEFINED = 16,
} NV_GPU_PERF_VOLTAGE_INFO_DOMAIN_ID;

typedef enum {
    NVAPI_GPU_PERF_PSTATE20_CLOCK_TYPE_SINGLE = 0,
    NVAPI_GPU_PERF_PSTATE20_CLOCK_TYPE_RANGE,
} NV_GPU_PERF_PSTATE20_CLOCK_TYPE_ID;

typedef struct {
    NvS32 value;
    struct {
        NvS32 min;
        NvS32 max;
    } valueRange;
} NV_GPU_PERF_PSTATES20_PARAM_DELTA;

typedef struct {
    NV_GPU_PUBLIC_CLOCK_ID domainId;
    NV_GPU_PERF_PSTATE20_CLOCK_TYPE_ID typeId;
    NvU32 bIsEditable:1;
    NvU32 reserved:31;
    NV_GPU_PERF_PSTATES20_PARAM_DELTA freqDelta_kHz;
    union {
        struct {
            NvU32 freq_kHz;
        } single;
        struct {
            NvU32 minFreq_kHz;
            NvU32 maxFreq_kHz;
            NV_GPU_PERF_VOLTAGE_INFO_DOMAIN_ID domainId;
            NvU32 minVoltage_uV;
            NvU32 maxVoltage_uV;
        } range;
    } data;
} NV_GPU_PSTATE20_CLOCK_ENTRY_V1;

typedef struct {
    NV_GPU_PERF_VOLTAGE_INFO_DOMAIN_ID domainId;
    NvU32 bIsEditable:1;
    NvU32 reserved:31;
    NvU32 volt_uV;
    NV_GPU_PERF_PSTATES20_PARAM_DELTA voltDelta_uV;
} NV_GPU_PSTATE20_BASE_VOLTAGE_ENTRY_V1;

typedef struct {
    NvU32 version;
    NvU32 bIsEditable:1;
    NvU32 reserved:31;
    NvU32 numPstates;
    NvU32 numClocks;
    NvU32 numBaseVoltages;
    struct {
        NV_GPU_PERF_PSTATE_ID pstateId;
        NvU32 bIsEditable:1;
        NvU32 reserved:31;
        NV_GPU_PSTATE20_CLOCK_ENTRY_V1 clocks[NVAPI_MAX_GPU_PSTATE20_CLOCKS];
        NV_GPU_PSTATE20_BASE_VOLTAGE_ENTRY_V1 baseVoltages[NVAPI_MAX_GPU_PSTATE20_BASE_VOLTAGES];
    } pstates[NVAPI_MAX_GPU_PSTATE20_PSTATES];
    struct {
        NvU32 numVoltages;
        NV_GPU_PSTATE20_BASE_VOLTAGE_ENTRY_V1 voltages[NVAPI_MAX_GPU_PSTATE20_BASE_VOLTAGES];
    } ov;
} NV_GPU_PERF_PSTATES20_INFO;

#define MAKE_NVAPI_VERSION(typeVer, ver) (((sizeof(typeVer)) << 16) | (ver))
#define NV_GPU_PERF_PSTATES20_INFO_VER MAKE_NVAPI_VERSION(NV_GPU_PERF_PSTATES20_INFO, 2)

typedef NvAPI_Status (*PFN_NvAPI_Initialize)(void);
typedef NvAPI_Status (*PFN_NvAPI_Unload)(void);
typedef NvAPI_Status (*PFN_NvAPI_EnumPhysicalGPUs)(NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS], NvU32 *pGpuCount);
typedef NvAPI_Status (*PFN_NvAPI_GPU_GetFullName)(NvPhysicalGpuHandle hPhysicalGpu, NvAPI_ShortString szName);
typedef NvAPI_Status (*PFN_NvAPI_GPU_GetPstates20)(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_PERF_PSTATES20_INFO *pPstatesInfo);

NvAPI_Status NvAPI_Initialize(void);
NvAPI_Status NvAPI_Unload(void);
NvAPI_Status NvAPI_EnumPhysicalGPUs(NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS], NvU32 *pGpuCount);
NvAPI_Status NvAPI_GPU_GetFullName(NvPhysicalGpuHandle hPhysicalGpu, NvAPI_ShortString szName);
NvAPI_Status NvAPI_GPU_GetPstates20(NvPhysicalGpuHandle hPhysicalGpu, NV_GPU_PERF_PSTATES20_INFO *pPstatesInfo);

#endif
