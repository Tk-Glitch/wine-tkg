/*
 * Copyright (c) 2018 Ethan Lee for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"

#include <stdarg.h>
#include <FACT.h>

#define NONAMELESSUNION
#define COBJMACROS

#include "initguid.h"
#include "xact3.h"
#include "rpcproxy.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(xact3);

static HINSTANCE instance;

typedef struct _XACT3CueImpl {
    IXACT3Cue IXACT3Cue_iface;
    FACTCue *fact_cue;
} XACT3CueImpl;

static inline XACT3CueImpl *impl_from_IXACT3Cue(IXACT3Cue *iface)
{
    return CONTAINING_RECORD(iface, XACT3CueImpl, IXACT3Cue_iface);
}

static HRESULT WINAPI IXACT3CueImpl_Play(IXACT3Cue *iface)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)\n", iface);

    return FACTCue_Play(This->fact_cue);
}

static HRESULT WINAPI IXACT3CueImpl_Stop(IXACT3Cue *iface, DWORD dwFlags)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u)\n", iface, dwFlags);

    return FACTCue_Stop(This->fact_cue, dwFlags);
}

static HRESULT WINAPI IXACT3CueImpl_GetState(IXACT3Cue *iface, DWORD *pdwState)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%p)\n", iface, pdwState);

    return FACTCue_GetState(This->fact_cue, pdwState);
}

static HRESULT WINAPI IXACT3CueImpl_Destroy(IXACT3Cue *iface)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);
    HRESULT hr;

    TRACE("(%p)\n", iface);

    hr = FACTCue_Destroy(This->fact_cue);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT WINAPI IXACT3CueImpl_SetMatrixCoefficients(IXACT3Cue *iface,
        UINT32 uSrcChannelCount, UINT32 uDstChannelCount,
        float *pMatrixCoefficients)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u, %u, %p)\n", iface, uSrcChannelCount, uDstChannelCount,
            pMatrixCoefficients);

    return FACTCue_SetMatrixCoefficients(This->fact_cue, uSrcChannelCount,
        uDstChannelCount, pMatrixCoefficients);
}

static XACTVARIABLEINDEX WINAPI IXACT3CueImpl_GetVariableIndex(IXACT3Cue *iface,
        PCSTR szFriendlyName)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%s)\n", iface, szFriendlyName);

    return FACTCue_GetVariableIndex(This->fact_cue, szFriendlyName);
}

static HRESULT WINAPI IXACT3CueImpl_SetVariable(IXACT3Cue *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE nValue)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u, %f)\n", iface, nIndex, nValue);

    return FACTCue_SetVariable(This->fact_cue, nIndex, nValue);
}

static HRESULT WINAPI IXACT3CueImpl_GetVariable(IXACT3Cue *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE *nValue)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u, %p)\n", iface, nIndex, nValue);

    return FACTCue_GetVariable(This->fact_cue, nIndex, nValue);
}

static HRESULT WINAPI IXACT3CueImpl_Pause(IXACT3Cue *iface, BOOL fPause)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);

    TRACE("(%p)->(%u)\n", iface, fPause);

    return FACTCue_Pause(This->fact_cue, fPause);
}

static HRESULT WINAPI IXACT3CueImpl_GetProperties(IXACT3Cue *iface,
        XACT_CUE_INSTANCE_PROPERTIES **ppProperties)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);
    FACTCueInstanceProperties *fProps;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", iface, ppProperties);

    hr = FACTCue_GetProperties(This->fact_cue, &fProps);
    if(FAILED(hr))
        return hr;

    *ppProperties = (XACT_CUE_INSTANCE_PROPERTIES*) fProps;
    return hr;
}

static HRESULT WINAPI IXACT3CueImpl_SetOutputVoices(IXACT3Cue *iface,
        const XAUDIO2_VOICE_SENDS *pSendList)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);
    FIXME("(%p)->(%p): stub!\n", This, pSendList);
    return S_OK;
}

static HRESULT WINAPI IXACT3CueImpl_SetOutputVoiceMatrix(IXACT3Cue *iface,
        IXAudio2Voice *pDestinationVoice, UINT32 SourceChannels,
        UINT32 DestinationChannels, const float *pLevelMatrix)
{
    XACT3CueImpl *This = impl_from_IXACT3Cue(iface);
    FIXME("(%p)->(%p %u %u %p): stub!\n", This, pDestinationVoice, SourceChannels,
            DestinationChannels, pLevelMatrix);
    return S_OK;
}

static const IXACT3CueVtbl XACT3Cue_Vtbl =
{
    IXACT3CueImpl_Play,
    IXACT3CueImpl_Stop,
    IXACT3CueImpl_GetState,
    IXACT3CueImpl_Destroy,
    IXACT3CueImpl_SetMatrixCoefficients,
    IXACT3CueImpl_GetVariableIndex,
    IXACT3CueImpl_SetVariable,
    IXACT3CueImpl_GetVariable,
    IXACT3CueImpl_Pause,
    IXACT3CueImpl_GetProperties,
    IXACT3CueImpl_SetOutputVoices,
    IXACT3CueImpl_SetOutputVoiceMatrix
};

typedef struct _XACT3SoundBankImpl {
    IXACT3SoundBank IXACT3SoundBank_iface;

    FACTSoundBank *fact_soundbank;
} XACT3SoundBankImpl;

static inline XACT3SoundBankImpl *impl_from_IXACT3SoundBank(IXACT3SoundBank *iface)
{
    return CONTAINING_RECORD(iface, XACT3SoundBankImpl, IXACT3SoundBank_iface);
}

static XACTINDEX WINAPI IXACT3SoundBankImpl_GetCueIndex(IXACT3SoundBank *iface,
        PCSTR szFriendlyName)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%s)\n", This, szFriendlyName);

    return FACTSoundBank_GetCueIndex(This->fact_soundbank, szFriendlyName);
}

static HRESULT WINAPI IXACT3SoundBankImpl_GetNumCues(IXACT3SoundBank *iface,
        XACTINDEX *pnNumCues)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%p)\n", This, pnNumCues);

    return FACTSoundBank_GetNumCues(This->fact_soundbank, pnNumCues);
}

static HRESULT WINAPI IXACT3SoundBankImpl_GetCueProperties(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, XACT_CUE_PROPERTIES *pProperties)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%u, %p)\n", This, nCueIndex, pProperties);

    return FACTSoundBank_GetCueProperties(This->fact_soundbank, nCueIndex,
            (FACTCueProperties*) pProperties);
}

static HRESULT WINAPI IXACT3SoundBankImpl_Prepare(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, DWORD dwFlags, XACTTIME timeOffset,
        IXACT3Cue** ppCue)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);
    XACT3CueImpl *cue;
    FACTCue *fcue;
    UINT ret;

    TRACE("(%p)->(%u, 0x%x, %u, %p)\n", This, nCueIndex, dwFlags, timeOffset,
            ppCue);

    ret = FACTSoundBank_Prepare(This->fact_soundbank, nCueIndex, dwFlags,
            timeOffset, &fcue);
    if(ret != 0)
    {
        ERR("Failed to CreateCue: %d\n", ret);
        return E_FAIL;
    }

    cue = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*cue));
    if (!cue)
    {
        FACTCue_Destroy(fcue);
        ERR("Failed to allocate XACT3CueImpl!");
        return E_OUTOFMEMORY;
    }

    cue->IXACT3Cue_iface.lpVtbl = &XACT3Cue_Vtbl;
    cue->fact_cue = fcue;
    *ppCue = (IXACT3Cue*)&cue->IXACT3Cue_iface;

    TRACE("Created Cue: %p\n", cue);

    return S_OK;
}

static HRESULT WINAPI IXACT3SoundBankImpl_Play(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, DWORD dwFlags, XACTTIME timeOffset,
        IXACT3Cue** ppCue)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);
    XACT3CueImpl *cue;
    FACTCue *fcue;
    HRESULT hr;

    TRACE("(%p)->(%u, 0x%x, %u, %p)\n", This, nCueIndex, dwFlags, timeOffset,
            ppCue);

    /* If the application doesn't want a handle, don't generate one at all.
     * Let the engine handle that memory instead.
     * -flibit
     */
    if (ppCue == NULL){
        hr = FACTSoundBank_Play(This->fact_soundbank, nCueIndex, dwFlags,
                timeOffset, NULL);
    }else{
        hr = FACTSoundBank_Play(This->fact_soundbank, nCueIndex, dwFlags,
                timeOffset, &fcue);
        if(FAILED(hr))
            return hr;

        cue = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*cue));
        if (!cue)
        {
            FACTCue_Destroy(fcue);
            ERR("Failed to allocate XACT3CueImpl!");
            return E_OUTOFMEMORY;
        }

        cue->IXACT3Cue_iface.lpVtbl = &XACT3Cue_Vtbl;
        cue->fact_cue = fcue;
        *ppCue = (IXACT3Cue*)&cue->IXACT3Cue_iface;
    }

    return hr;
}

static HRESULT WINAPI IXACT3SoundBankImpl_Stop(IXACT3SoundBank *iface,
        XACTINDEX nCueIndex, DWORD dwFlags)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%u)\n", This, dwFlags);

    return FACTSoundBank_Stop(This->fact_soundbank, nCueIndex, dwFlags);
}

static HRESULT WINAPI IXACT3SoundBankImpl_Destroy(IXACT3SoundBank *iface)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);
    HRESULT hr;

    TRACE("(%p)\n", This);

    hr = FACTSoundBank_Destroy(This->fact_soundbank);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT WINAPI IXACT3SoundBankImpl_GetState(IXACT3SoundBank *iface,
        DWORD *pdwState)
{
    XACT3SoundBankImpl *This = impl_from_IXACT3SoundBank(iface);

    TRACE("(%p)->(%p)\n", This, pdwState);

    return FACTSoundBank_GetState(This->fact_soundbank, pdwState);
}

static const IXACT3SoundBankVtbl XACT3SoundBank_Vtbl =
{
    IXACT3SoundBankImpl_GetCueIndex,
    IXACT3SoundBankImpl_GetNumCues,
    IXACT3SoundBankImpl_GetCueProperties,
    IXACT3SoundBankImpl_Prepare,
    IXACT3SoundBankImpl_Play,
    IXACT3SoundBankImpl_Stop,
    IXACT3SoundBankImpl_Destroy,
    IXACT3SoundBankImpl_GetState
};

typedef struct _XACT3WaveImpl {
    IXACT3Wave IXACT3Wave_iface;

    FACTWave *fact_wave;
} XACT3WaveImpl;

static inline XACT3WaveImpl *impl_from_IXACT3Wave(IXACT3Wave *iface)
{
    return CONTAINING_RECORD(iface, XACT3WaveImpl, IXACT3Wave_iface);
}

static HRESULT WINAPI IXACT3WaveImpl_Destroy(IXACT3Wave *iface)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);
    HRESULT hr;

    TRACE("(%p)\n", This);

    hr = FACTWave_Destroy(This->fact_wave);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT WINAPI IXACT3WaveImpl_Play(IXACT3Wave *iface)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)\n", This);

    return FACTWave_Play(This->fact_wave);
}

static HRESULT WINAPI IXACT3WaveImpl_Stop(IXACT3Wave *iface, DWORD dwFlags)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(0x%x)\n", This, dwFlags);

    return FACTWave_Stop(This->fact_wave, dwFlags);
}

static HRESULT WINAPI IXACT3WaveImpl_Pause(IXACT3Wave *iface, BOOL fPause)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%u)\n", This, fPause);

    return FACTWave_Pause(This->fact_wave, fPause);
}

static HRESULT WINAPI IXACT3WaveImpl_GetState(IXACT3Wave *iface, DWORD *pdwState)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%p)\n", This, pdwState);

    return FACTWave_GetState(This->fact_wave, pdwState);
}

static HRESULT WINAPI IXACT3WaveImpl_SetPitch(IXACT3Wave *iface, XACTPITCH pitch)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%d)\n", This, pitch);

    return FACTWave_SetPitch(This->fact_wave, pitch);
}

static HRESULT WINAPI IXACT3WaveImpl_SetVolume(IXACT3Wave *iface, XACTVOLUME volume)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%f)\n", This, volume);

    return FACTWave_SetVolume(This->fact_wave, volume);
}

static HRESULT WINAPI IXACT3WaveImpl_SetMatrixCoefficients(IXACT3Wave *iface,
        UINT32 uSrcChannelCount, UINT32 uDstChannelCount,
        float *pMatrixCoefficients)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%u, %u, %p)\n", This, uSrcChannelCount, uDstChannelCount,
            pMatrixCoefficients);

    return FACTWave_SetMatrixCoefficients(This->fact_wave, uSrcChannelCount,
            uDstChannelCount, pMatrixCoefficients);
}

static HRESULT WINAPI IXACT3WaveImpl_GetProperties(IXACT3Wave *iface,
    XACT_WAVE_INSTANCE_PROPERTIES *pProperties)
{
    XACT3WaveImpl *This = impl_from_IXACT3Wave(iface);

    TRACE("(%p)->(%p)\n", This, pProperties);

    return FACTWave_GetProperties(This->fact_wave,
            (FACTWaveInstanceProperties*) pProperties);
}

static const IXACT3WaveVtbl XACT3Wave_Vtbl =
{
    IXACT3WaveImpl_Destroy,
    IXACT3WaveImpl_Play,
    IXACT3WaveImpl_Stop,
    IXACT3WaveImpl_Pause,
    IXACT3WaveImpl_GetState,
    IXACT3WaveImpl_SetPitch,
    IXACT3WaveImpl_SetVolume,
    IXACT3WaveImpl_SetMatrixCoefficients,
    IXACT3WaveImpl_GetProperties
};

typedef struct _XACT3WaveBankImpl {
    IXACT3WaveBank IXACT3WaveBank_iface;

    FACTWaveBank *fact_wavebank;
} XACT3WaveBankImpl;

static inline XACT3WaveBankImpl *impl_from_IXACT3WaveBank(IXACT3WaveBank *iface)
{
    return CONTAINING_RECORD(iface, XACT3WaveBankImpl, IXACT3WaveBank_iface);
}

static HRESULT WINAPI IXACT3WaveBankImpl_Destroy(IXACT3WaveBank *iface)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);
    HRESULT hr;

    TRACE("(%p)\n", This);

    hr = FACTWaveBank_Destroy(This->fact_wavebank);
    HeapFree(GetProcessHeap(), 0, This);
    return hr;
}

static HRESULT WINAPI IXACT3WaveBankImpl_GetNumWaves(IXACT3WaveBank *iface,
        XACTINDEX *pnNumWaves)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%p)\n", This, pnNumWaves);

    return FACTWaveBank_GetNumWaves(This->fact_wavebank, pnNumWaves);
}

static XACTINDEX WINAPI IXACT3WaveBankImpl_GetWaveIndex(IXACT3WaveBank *iface,
        PCSTR szFriendlyName)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%s)\n", This, szFriendlyName);

    return FACTWaveBank_GetWaveIndex(This->fact_wavebank, szFriendlyName);
}

static HRESULT WINAPI IXACT3WaveBankImpl_GetWaveProperties(IXACT3WaveBank *iface,
        XACTINDEX nWaveIndex, XACT_WAVE_PROPERTIES *pWaveProperties)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%u, %p)\n", This, nWaveIndex, pWaveProperties);

    return FACTWaveBank_GetWaveProperties(This->fact_wavebank, nWaveIndex,
            (FACTWaveProperties*) pWaveProperties);
}

static HRESULT WINAPI IXACT3WaveBankImpl_Prepare(IXACT3WaveBank *iface,
        XACTINDEX nWaveIndex, DWORD dwFlags, DWORD dwPlayOffset,
        XACTLOOPCOUNT nLoopCount, IXACT3Wave** ppWave)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);
    XACT3WaveImpl *wave;
    FACTWave *fwave;
    UINT ret;

    TRACE("(%p)->(0x%x, %u, 0x%x, %u, %p)\n", This, nWaveIndex, dwFlags,
            dwPlayOffset, nLoopCount, ppWave);

    ret = FACTWaveBank_Prepare(This->fact_wavebank, nWaveIndex, dwFlags,
            dwPlayOffset, nLoopCount, &fwave);
    if(ret != 0)
    {
        ERR("Failed to CreateWave: %d\n", ret);
        return E_FAIL;
    }

    wave = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wave));
    if (!wave)
    {
        FACTWave_Destroy(fwave);
        ERR("Failed to allocate XACT3WaveImpl!");
        return E_OUTOFMEMORY;
    }

    wave->IXACT3Wave_iface.lpVtbl = &XACT3Wave_Vtbl;
    wave->fact_wave = fwave;
    *ppWave = &wave->IXACT3Wave_iface;

    TRACE("Created Wave: %p\n", wave);

    return S_OK;
}

static HRESULT WINAPI IXACT3WaveBankImpl_Play(IXACT3WaveBank *iface,
        XACTINDEX nWaveIndex, DWORD dwFlags, DWORD dwPlayOffset,
        XACTLOOPCOUNT nLoopCount, IXACT3Wave** ppWave)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);
    XACT3WaveImpl *wave;
    FACTWave *fwave;
    HRESULT hr;

    TRACE("(%p)->(0x%x, %u, 0x%x, %u, %p)\n", This, nWaveIndex, dwFlags, dwPlayOffset,
            nLoopCount, ppWave);

    /* If the application doesn't want a handle, don't generate one at all.
     * Let the engine handle that memory instead.
     * -flibit
     */
    if (ppWave == NULL){
        hr = FACTWaveBank_Play(This->fact_wavebank, nWaveIndex, dwFlags,
                dwPlayOffset, nLoopCount, NULL);
    }else{
        hr = FACTWaveBank_Play(This->fact_wavebank, nWaveIndex, dwFlags,
                dwPlayOffset, nLoopCount, &fwave);
        if(FAILED(hr))
            return hr;

        wave = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wave));
        if (!wave)
        {
            FACTWave_Destroy(fwave);
            ERR("Failed to allocate XACT3WaveImpl!");
            return E_OUTOFMEMORY;
        }

        wave->IXACT3Wave_iface.lpVtbl = &XACT3Wave_Vtbl;
        wave->fact_wave = fwave;
        *ppWave = &wave->IXACT3Wave_iface;
    }

    return hr;
}

static HRESULT WINAPI IXACT3WaveBankImpl_Stop(IXACT3WaveBank *iface,
        XACTINDEX nWaveIndex, DWORD dwFlags)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%u, %u)\n", This, nWaveIndex, dwFlags);

    return FACTWaveBank_Stop(This->fact_wavebank, nWaveIndex, dwFlags);
}

static HRESULT WINAPI IXACT3WaveBankImpl_GetState(IXACT3WaveBank *iface,
        DWORD *pdwState)
{
    XACT3WaveBankImpl *This = impl_from_IXACT3WaveBank(iface);

    TRACE("(%p)->(%p)\n", This, pdwState);

    return FACTWaveBank_GetState(This->fact_wavebank, pdwState);
}

static const IXACT3WaveBankVtbl XACT3WaveBank_Vtbl =
{
    IXACT3WaveBankImpl_Destroy,
    IXACT3WaveBankImpl_GetNumWaves,
    IXACT3WaveBankImpl_GetWaveIndex,
    IXACT3WaveBankImpl_GetWaveProperties,
    IXACT3WaveBankImpl_Prepare,
    IXACT3WaveBankImpl_Play,
    IXACT3WaveBankImpl_Stop,
    IXACT3WaveBankImpl_GetState
};

typedef struct _XACT3EngineImpl {
    IXACT3Engine IXACT3Engine_iface;

    FACTAudioEngine *fact_engine;

    XACT_READFILE_CALLBACK pReadFile;
    XACT_GETOVERLAPPEDRESULT_CALLBACK pGetOverlappedResult;
    XACT_NOTIFICATION_CALLBACK notification_callback;
} XACT3EngineImpl;

typedef struct wrap_readfile_struct {
    XACT3EngineImpl *engine;
    HANDLE file;
} wrap_readfile_struct;

static int32_t FACTCALL wrap_readfile(
    void* hFile,
    void* lpBuffer,
    uint32_t nNumberOfBytesRead,
    uint32_t *lpNumberOfBytesRead,
    FACTOverlapped *lpOverlapped)
{
    wrap_readfile_struct *wrap = (wrap_readfile_struct*) hFile;
    return wrap->engine->pReadFile(wrap->file, lpBuffer, nNumberOfBytesRead,
            lpNumberOfBytesRead, (LPOVERLAPPED)lpOverlapped);
}

static int32_t FACTCALL wrap_getoverlappedresult(
    void* hFile,
    FACTOverlapped *lpOverlapped,
    uint32_t *lpNumberOfBytesTransferred,
    int32_t bWait)
{
    wrap_readfile_struct *wrap = (wrap_readfile_struct*) hFile;
    return wrap->engine->pGetOverlappedResult(wrap->file, (LPOVERLAPPED)lpOverlapped,
            lpNumberOfBytesTransferred, bWait);
}

static inline XACT3EngineImpl *impl_from_IXACT3Engine(IXACT3Engine *iface)
{
    return CONTAINING_RECORD(iface, XACT3EngineImpl, IXACT3Engine_iface);
}

static HRESULT WINAPI IXACT3EngineImpl_QueryInterface(IXACT3Engine *iface,
        REFIID riid, void **ppvObject)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%s, %p)\n", This, debugstr_guid(riid), ppvObject);

    if(IsEqualGUID(riid, &IID_IUnknown) ||
            IsEqualGUID(riid, &IID_IXACT3Engine)){
        *ppvObject = &This->IXACT3Engine_iface;
    }
    else
        *ppvObject = NULL;

    if (*ppvObject){
        IUnknown_AddRef((IUnknown*)*ppvObject);
        return S_OK;
    }

    FIXME("(%p)->(%s,%p), not found\n", This, debugstr_guid(riid), ppvObject);

    return E_NOINTERFACE;
}

static ULONG WINAPI IXACT3EngineImpl_AddRef(IXACT3Engine *iface)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    ULONG ref = FACTAudioEngine_AddRef(This->fact_engine);
    TRACE("(%p)->(): Refcount now %u\n", This, ref);
    return ref;
}

static ULONG WINAPI IXACT3EngineImpl_Release(IXACT3Engine *iface)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    ULONG ref = FACTAudioEngine_Release(This->fact_engine);

    TRACE("(%p)->(): Refcount now %u\n", This, ref);

    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);
    return ref;
}

static HRESULT WINAPI IXACT3EngineImpl_GetRendererCount(IXACT3Engine *iface,
        XACTINDEX *pnRendererCount)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%p)\n", This, pnRendererCount);

    return FACTAudioEngine_GetRendererCount(This->fact_engine, pnRendererCount);
}

static HRESULT WINAPI IXACT3EngineImpl_GetRendererDetails(IXACT3Engine *iface,
        XACTINDEX nRendererIndex, XACT_RENDERER_DETAILS *pRendererDetails)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%d, %p)\n", This, nRendererIndex, pRendererDetails);

    return FACTAudioEngine_GetRendererDetails(This->fact_engine,
            nRendererIndex, (FACTRendererDetails*) pRendererDetails);
}

static HRESULT WINAPI IXACT3EngineImpl_GetFinalMixFormat(IXACT3Engine *iface,
        WAVEFORMATEXTENSIBLE *pFinalMixFormat)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%p)\n", This, pFinalMixFormat);

    return FACTAudioEngine_GetFinalMixFormat(This->fact_engine,
            (FAudioWaveFormatExtensible*) pFinalMixFormat);
}

static void FACTCALL fact_notification_cb(const FACTNotification *notification)
{
    XACT3EngineImpl *engine = (XACT3EngineImpl *)notification->pvContext;

    /* Older versions of FAudio don't pass through the context */
    if (!engine)
    {
        WARN("Notification context is NULL\n");
        return;
    }

    if (notification->type == XACTNOTIFICATIONTYPE_SOUNDBANKDESTROYED)
    {
        FIXME("Callback XACTNOTIFICATIONTYPE_SOUNDBANKDESTROYED\n");
    }
    else
        FIXME("Unsupported callback type %d\n", notification->type);
}

static HRESULT WINAPI IXACT3EngineImpl_Initialize(IXACT3Engine *iface,
        const XACT_RUNTIME_PARAMETERS *pParams)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FACTRuntimeParameters params;
    UINT ret;

    TRACE("(%p)->(%p)\n", This, pParams);

    memcpy(&params, pParams, sizeof(FACTRuntimeParameters));

    /* FIXME: pXAudio2 and pMasteringVoice are pointers to
     * IXAudio2/IXAudio2MasteringVoice objects. FACT wants pointers to
     * FAudio/FAudioMasteringVoice objects. In Wine's XAudio2 implementation, we
     * actually have them available, but only if you access their internal data.
     * For now we can just force these pointers to NULL, as XACT simply
     * generates its own engine and endpoint in that situation. These parameters
     * are mostly an optimization for games with multiple XACT3Engines that want
     * a single engine running everything.
     * -flibit
     */
    if (pParams->pXAudio2 != NULL){
        FIXME("pXAudio2 parameter not supported! Falling back to NULL\n");
        params.pXAudio2 = NULL;

        if (pParams->pMasteringVoice != NULL){
            FIXME("pXAudio2 parameter not supported! Falling back to NULL\n");
            params.pMasteringVoice = NULL;
        }
    }

    /* Force Windows I/O, do NOT use the FACT default! */
    This->pReadFile = (XACT_READFILE_CALLBACK)
            pParams->fileIOCallbacks.readFileCallback;
    This->pGetOverlappedResult = (XACT_GETOVERLAPPEDRESULT_CALLBACK)
            pParams->fileIOCallbacks.getOverlappedResultCallback;
    if (This->pReadFile == NULL)
        This->pReadFile = (XACT_READFILE_CALLBACK) ReadFile;
    if (This->pGetOverlappedResult == NULL)
        This->pGetOverlappedResult = (XACT_GETOVERLAPPEDRESULT_CALLBACK)
                GetOverlappedResult;
    params.fileIOCallbacks.readFileCallback = wrap_readfile;
    params.fileIOCallbacks.getOverlappedResultCallback = wrap_getoverlappedresult;
    params.fnNotificationCallback = fact_notification_cb;

    This->notification_callback = (XACT_NOTIFICATION_CALLBACK)pParams->fnNotificationCallback;

    ret = FACTAudioEngine_Initialize(This->fact_engine, &params);
    if (ret != 0)
        WARN("FACTAudioEngine_Initialize returned %d\n", ret);

    return !ret ? S_OK : E_FAIL;
}

static HRESULT WINAPI IXACT3EngineImpl_ShutDown(IXACT3Engine *iface)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)\n", This);

    return FACTAudioEngine_ShutDown(This->fact_engine);
}

static HRESULT WINAPI IXACT3EngineImpl_DoWork(IXACT3Engine *iface)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)\n", This);

    return FACTAudioEngine_DoWork(This->fact_engine);
}

static HRESULT WINAPI IXACT3EngineImpl_CreateSoundBank(IXACT3Engine *iface,
        const void* pvBuffer, DWORD dwSize, DWORD dwFlags,
        DWORD dwAllocAttributes, IXACT3SoundBank **ppSoundBank)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    XACT3SoundBankImpl *sb;
    FACTSoundBank *fsb;
    UINT ret;

    TRACE("(%p)->(%p, %u, 0x%x, 0x%x, %p): stub!\n", This, pvBuffer, dwSize, dwFlags,
            dwAllocAttributes, ppSoundBank);

    ret = FACTAudioEngine_CreateSoundBank(This->fact_engine, pvBuffer, dwSize,
            dwFlags, dwAllocAttributes, &fsb);
    if(ret != 0)
    {
        ERR("Failed to CreateSoundBank: %d\n", ret);
        return E_FAIL;
    }

    sb = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*sb));
    if (!sb)
    {
        FACTSoundBank_Destroy(fsb);
        ERR("Failed to allocate XACT3SoundBankImpl!");
        return E_OUTOFMEMORY;
    }

    sb->IXACT3SoundBank_iface.lpVtbl = &XACT3SoundBank_Vtbl;
    sb->fact_soundbank = fsb;
    *ppSoundBank = &sb->IXACT3SoundBank_iface;

    TRACE("Created SoundBank: %p\n", sb);

    return S_OK;
}

static HRESULT WINAPI IXACT3EngineImpl_CreateInMemoryWaveBank(IXACT3Engine *iface,
        const void* pvBuffer, DWORD dwSize, DWORD dwFlags,
        DWORD dwAllocAttributes, IXACT3WaveBank **ppWaveBank)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    XACT3WaveBankImpl *wb;
    FACTWaveBank *fwb;
    UINT ret;

    TRACE("(%p)->(%p, %u, 0x%x, 0x%x, %p)\n", This, pvBuffer, dwSize, dwFlags,
            dwAllocAttributes, ppWaveBank);

    ret = FACTAudioEngine_CreateInMemoryWaveBank(This->fact_engine, pvBuffer,
            dwSize, dwFlags, dwAllocAttributes, &fwb);
    if(ret != 0)
    {
        ERR("Failed to CreateWaveBank: %d\n", ret);
        return E_FAIL;
    }

    wb = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wb));
    if (!wb)
    {
        FACTWaveBank_Destroy(fwb);
        ERR("Failed to allocate XACT3WaveBankImpl!");
        return E_OUTOFMEMORY;
    }

    wb->IXACT3WaveBank_iface.lpVtbl = &XACT3WaveBank_Vtbl;
    wb->fact_wavebank = fwb;
    *ppWaveBank = &wb->IXACT3WaveBank_iface;

    TRACE("Created in-memory WaveBank: %p\n", wb);

    return S_OK;
}

static HRESULT WINAPI IXACT3EngineImpl_CreateStreamingWaveBank(IXACT3Engine *iface,
        const XACT_WAVEBANK_STREAMING_PARAMETERS *pParms,
        IXACT3WaveBank **ppWaveBank)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FACTStreamingParameters fakeParms;
    wrap_readfile_struct *fake;
    XACT3WaveBankImpl *wb;
    FACTWaveBank *fwb;
    UINT ret;

    TRACE("(%p)->(%p, %p)\n", This, pParms, ppWaveBank);

    /* We have to wrap the file to fix up the callbacks! */
    fake = (wrap_readfile_struct*) CoTaskMemAlloc(
            sizeof(wrap_readfile_struct));
    fake->engine = This;
    fake->file = pParms->file;
    fakeParms.file = fake;
    fakeParms.flags = pParms->flags;
    fakeParms.offset = pParms->offset;
    fakeParms.packetSize = pParms->packetSize;

    ret = FACTAudioEngine_CreateStreamingWaveBank(This->fact_engine, &fakeParms,
            &fwb);
    if(ret != 0)
    {
        ERR("Failed to CreateWaveBank: %d\n", ret);
        return E_FAIL;
    }

    wb = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wb));
    if (!wb)
    {
        FACTWaveBank_Destroy(fwb);
        ERR("Failed to allocate XACT3WaveBankImpl!");
        return E_OUTOFMEMORY;
    }

    wb->IXACT3WaveBank_iface.lpVtbl = &XACT3WaveBank_Vtbl;
    wb->fact_wavebank = fwb;
    *ppWaveBank = &wb->IXACT3WaveBank_iface;

    TRACE("Created streaming WaveBank: %p\n", wb);

    return S_OK;
}

static HRESULT WINAPI IXACT3EngineImpl_PrepareInMemoryWave(IXACT3Engine *iface,
        DWORD dwFlags, WAVEBANKENTRY entry, DWORD *pdwSeekTable,
        BYTE *pbWaveData, DWORD dwPlayOffset, XACTLOOPCOUNT nLoopCount,
        IXACT3Wave **ppWave)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    XACT3WaveImpl *wave;
    FACTWave *fwave = NULL;
    FACTWaveBankEntry fact_wavebank;
    UINT ret;

    TRACE("(%p)->(0x%08x, %p, %p, %p, %d, %d, %p)\n", This, dwFlags, &entry, pdwSeekTable,
          pbWaveData, dwPlayOffset, nLoopCount, ppWave);

    fact_wavebank.dwFlagsAndDuration = entry.u.dwFlagsAndDuration;
    fact_wavebank.Format.dwValue = entry.Format.dwValue;
    fact_wavebank.PlayRegion.dwOffset = entry.PlayRegion.dwOffset;
    fact_wavebank.PlayRegion.dwLength = entry.PlayRegion.dwLength;
    fact_wavebank.LoopRegion.dwStartSample = entry.LoopRegion.dwStartSample;
    fact_wavebank.LoopRegion.dwTotalSamples = entry.LoopRegion.dwTotalSamples;

    ret = FACTAudioEngine_PrepareInMemoryWave(This->fact_engine, dwFlags, fact_wavebank,
            pdwSeekTable, pbWaveData, dwPlayOffset, nLoopCount, &fwave);
    if(ret != 0 || !fwave)
    {
        ERR("Failed to CreateWave: %d (%p)\n", ret, fwave);
        return E_FAIL;
    }

    wave = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wave));
    if (!wave)
    {
        FACTWave_Destroy(fwave);
        ERR("Failed to allocate XACT3WaveImpl!");
        return E_OUTOFMEMORY;
    }

    wave->IXACT3Wave_iface.lpVtbl = &XACT3Wave_Vtbl;
    wave->fact_wave = fwave;
    *ppWave = &wave->IXACT3Wave_iface;

    TRACE("Created Wave: %p\n", wave);

    return S_OK;
}

static HRESULT WINAPI IXACT3EngineImpl_PrepareStreamingWave(IXACT3Engine *iface,
        DWORD dwFlags, WAVEBANKENTRY entry,
        XACT_STREAMING_PARAMETERS streamingParams, DWORD dwAlignment,
        DWORD *pdwSeekTable, DWORD dwPlayOffset, XACTLOOPCOUNT nLoopCount,
        IXACT3Wave **ppWave)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    XACT3WaveImpl *wave;
    FACTWave *fwave = NULL;
    FACTStreamingParameters fakeParms;
    wrap_readfile_struct *fake;
    FACTWaveBankEntry fact_wavebank;
    UINT ret;

    TRACE("(%p)->(0x%08x, %p, %p, %d, %p, %d, %d, %p)\n", This, dwFlags, &entry, &streamingParams,
            dwAlignment, pdwSeekTable, dwPlayOffset, nLoopCount, ppWave);

    fake = (wrap_readfile_struct*) CoTaskMemAlloc(
            sizeof(wrap_readfile_struct));
    fake->engine = This;
    fake->file = streamingParams.file;
    fakeParms.file = fake;
    fakeParms.flags = streamingParams.flags;
    fakeParms.offset = streamingParams.offset;
    fakeParms.packetSize = streamingParams.packetSize;

    fact_wavebank.dwFlagsAndDuration = entry.u.dwFlagsAndDuration;
    fact_wavebank.Format.dwValue = entry.Format.dwValue;
    fact_wavebank.PlayRegion.dwOffset = entry.PlayRegion.dwOffset;
    fact_wavebank.PlayRegion.dwLength = entry.PlayRegion.dwLength;
    fact_wavebank.LoopRegion.dwStartSample = entry.LoopRegion.dwStartSample;
    fact_wavebank.LoopRegion.dwTotalSamples = entry.LoopRegion.dwTotalSamples;

    /* FAudio Prototype is incorrect and shouldn't take a buffer as an parameter,
     *   passing through NULL to ensure it's not used.
     */
    ret = FACTAudioEngine_PrepareStreamingWave(This->fact_engine, dwFlags, fact_wavebank, fakeParms,
            dwAlignment, pdwSeekTable, NULL, dwPlayOffset, nLoopCount, &fwave);

    if(ret != 0 || !fwave)
    {
        ERR("Failed to CreateWave: %d (%p)\n", ret, fwave);
        return E_FAIL;
    }

    wave = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wave));
    if (!wave)
    {
        FACTWave_Destroy(fwave);
        ERR("Failed to allocate XACT3WaveImpl!");
        return E_OUTOFMEMORY;
    }

    wave->IXACT3Wave_iface.lpVtbl = &XACT3Wave_Vtbl;
    wave->fact_wave = fwave;
    *ppWave = &wave->IXACT3Wave_iface;

    TRACE("Created Wave: %p\n", wave);

    return S_OK;
}

static HRESULT WINAPI IXACT3EngineImpl_PrepareWave(IXACT3Engine *iface,
        DWORD dwFlags, PCSTR szWavePath, WORD wStreamingPacketSize,
        DWORD dwAlignment, DWORD dwPlayOffset, XACTLOOPCOUNT nLoopCount,
        IXACT3Wave **ppWave)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    XACT3WaveImpl *wave;
    FACTWave *fwave = NULL;
    UINT ret;

    TRACE("(%p)->(0x%08x, %s, %d, %d, %d, %d, %p)\n", This, dwFlags, debugstr_a(szWavePath),
          wStreamingPacketSize, dwAlignment, dwPlayOffset, nLoopCount, ppWave);

    ret = FACTAudioEngine_PrepareWave(This->fact_engine, dwFlags, szWavePath, wStreamingPacketSize,
            dwAlignment, dwPlayOffset, nLoopCount, &fwave);
    if(ret != 0 || !fwave)
    {
        ERR("Failed to CreateWave: %d (%p)\n", ret, fwave);
        return E_FAIL;
    }

    wave = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wave));
    if (!wave)
    {
        FACTWave_Destroy(fwave);
        ERR("Failed to allocate XACT3WaveImpl!");
        return E_OUTOFMEMORY;
    }

    wave->IXACT3Wave_iface.lpVtbl = &XACT3Wave_Vtbl;
    wave->fact_wave = fwave;
    *ppWave = &wave->IXACT3Wave_iface;

    TRACE("Created Wave: %p\n", wave);

    return S_OK;
}

enum {
    NOTIFY_SoundBank = 0x01,
    NOTIFY_WaveBank  = 0x02,
    NOTIFY_Cue       = 0x04,
    NOTIFY_Wave      = 0x08,
    NOTIFY_cueIndex  = 0x10,
    NOTIFY_waveIndex = 0x20
};

static inline void unwrap_notificationdesc(FACTNotificationDescription *fd,
        const XACT_NOTIFICATION_DESCRIPTION *xd)
{
    DWORD flags = 0;

    TRACE("Type %d\n", xd->type);

    memset(fd, 0, sizeof(*fd));

    /* Supports SoundBank, Cue index, Cue instance */
    if (xd->type == XACTNOTIFICATIONTYPE_CUEPREPARED || xd->type == XACTNOTIFICATIONTYPE_CUEPLAY ||
        xd->type == XACTNOTIFICATIONTYPE_CUESTOP || xd->type == XACTNOTIFICATIONTYPE_CUEDESTROYED ||
        xd->type == XACTNOTIFICATIONTYPE_MARKER || xd->type == XACTNOTIFICATIONTYPE_LOCALVARIABLECHANGED)
    {
        flags = NOTIFY_SoundBank | NOTIFY_cueIndex | NOTIFY_Cue;
    }
    /* Supports WaveBank */
    else if (xd->type == XACTNOTIFICATIONTYPE_WAVEBANKDESTROYED || xd->type == XACTNOTIFICATIONTYPE_WAVEBANKPREPARED ||
             xd->type == XACTNOTIFICATIONTYPE_WAVEBANKSTREAMING_INVALIDCONTENT)
    {
        flags = NOTIFY_WaveBank;
    }
    /* Supports NOTIFY_SoundBank */
    else if (xd->type == XACTNOTIFICATIONTYPE_SOUNDBANKDESTROYED)
    {
        flags = NOTIFY_SoundBank;
    }
    /* Supports WaveBank, Wave index, Wave instance */
    else if (xd->type == XACTNOTIFICATIONTYPE_WAVEPREPARED || xd->type == XACTNOTIFICATIONTYPE_WAVEDESTROYED)
    {
        flags = NOTIFY_WaveBank | NOTIFY_waveIndex | NOTIFY_Wave;
    }
    /* Supports SoundBank, SoundBank, Cue index, Cue instance, WaveBank, Wave instance */
    else if (xd->type == XACTNOTIFICATIONTYPE_WAVEPLAY || xd->type == XACTNOTIFICATIONTYPE_WAVESTOP ||
             xd->type == XACTNOTIFICATIONTYPE_WAVELOOPED)
    {
        flags = NOTIFY_SoundBank | NOTIFY_cueIndex | NOTIFY_Cue | NOTIFY_WaveBank | NOTIFY_Wave;
    }

    /* We have to unwrap the FACT object first! */
    fd->type = xd->type;
    fd->flags = xd->flags;
    fd->pvContext = xd->pvContext;
    if (flags & NOTIFY_cueIndex)
        fd->cueIndex = xd->cueIndex;
    if (flags & NOTIFY_waveIndex)
        fd->waveIndex = xd->waveIndex;

    if (flags & NOTIFY_Cue && xd->pCue != NULL)
    {
        XACT3CueImpl *cue = impl_from_IXACT3Cue(xd->pCue);
        if (cue)
            fd->pCue = cue->fact_cue;
    }

    if (flags & NOTIFY_SoundBank && xd->pSoundBank != NULL)
    {
        XACT3SoundBankImpl *sound = impl_from_IXACT3SoundBank(xd->pSoundBank);
        if (sound)
            fd->pSoundBank = sound->fact_soundbank;
    }

    if (flags & NOTIFY_WaveBank && xd->pWaveBank != NULL)
    {
        XACT3WaveBankImpl *bank = impl_from_IXACT3WaveBank(xd->pWaveBank);
        if (bank)
            fd->pWaveBank = bank->fact_wavebank;
    }

    if (flags & NOTIFY_Wave && xd->pWave != NULL)
    {
        XACT3WaveImpl *wave = impl_from_IXACT3Wave(xd->pWave);
        if (wave)
            fd->pWave = wave->fact_wave;
    }
}

static HRESULT WINAPI IXACT3EngineImpl_RegisterNotification(IXACT3Engine *iface,
        const XACT_NOTIFICATION_DESCRIPTION *pNotificationDesc)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FACTNotificationDescription fdesc;

    TRACE("(%p)->(%p)\n", This, pNotificationDesc);

    unwrap_notificationdesc(&fdesc, pNotificationDesc);
    fdesc.pvContext = This;
    return FACTAudioEngine_RegisterNotification(This->fact_engine, &fdesc);
}

static HRESULT WINAPI IXACT3EngineImpl_UnRegisterNotification(IXACT3Engine *iface,
        const XACT_NOTIFICATION_DESCRIPTION *pNotificationDesc)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);
    FACTNotificationDescription fdesc;

    TRACE("(%p)->(%p)\n", This, pNotificationDesc);

    unwrap_notificationdesc(&fdesc, pNotificationDesc);
    fdesc.pvContext = This;
    return FACTAudioEngine_UnRegisterNotification(This->fact_engine, &fdesc);
}

static XACTCATEGORY WINAPI IXACT3EngineImpl_GetCategory(IXACT3Engine *iface,
        PCSTR szFriendlyName)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%s)\n", This, szFriendlyName);

    return FACTAudioEngine_GetCategory(This->fact_engine, szFriendlyName);
}

static HRESULT WINAPI IXACT3EngineImpl_Stop(IXACT3Engine *iface,
        XACTCATEGORY nCategory, DWORD dwFlags)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, 0x%x)\n", This, nCategory, dwFlags);

    return FACTAudioEngine_Stop(This->fact_engine, nCategory, dwFlags);
}

static HRESULT WINAPI IXACT3EngineImpl_SetVolume(IXACT3Engine *iface,
        XACTCATEGORY nCategory, XACTVOLUME nVolume)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, %f)\n", This, nCategory, nVolume);

    return FACTAudioEngine_SetVolume(This->fact_engine, nCategory, nVolume);
}

static HRESULT WINAPI IXACT3EngineImpl_Pause(IXACT3Engine *iface,
        XACTCATEGORY nCategory, BOOL fPause)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, %u)\n", This, nCategory, fPause);

    return FACTAudioEngine_Pause(This->fact_engine, nCategory, fPause);
}

static XACTVARIABLEINDEX WINAPI IXACT3EngineImpl_GetGlobalVariableIndex(
        IXACT3Engine *iface, PCSTR szFriendlyName)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%s)\n", This, szFriendlyName);

    return FACTAudioEngine_GetGlobalVariableIndex(This->fact_engine,
            szFriendlyName);
}

static HRESULT WINAPI IXACT3EngineImpl_SetGlobalVariable(IXACT3Engine *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE nValue)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, %f)\n", This, nIndex, nValue);

    return FACTAudioEngine_SetGlobalVariable(This->fact_engine, nIndex, nValue);
}

static HRESULT WINAPI IXACT3EngineImpl_GetGlobalVariable(IXACT3Engine *iface,
        XACTVARIABLEINDEX nIndex, XACTVARIABLEVALUE *nValue)
{
    XACT3EngineImpl *This = impl_from_IXACT3Engine(iface);

    TRACE("(%p)->(%u, %p)\n", This, nIndex, nValue);

    return FACTAudioEngine_GetGlobalVariable(This->fact_engine, nIndex, nValue);
}

static const IXACT3EngineVtbl XACT3Engine_Vtbl =
{
    IXACT3EngineImpl_QueryInterface,
    IXACT3EngineImpl_AddRef,
    IXACT3EngineImpl_Release,
    IXACT3EngineImpl_GetRendererCount,
    IXACT3EngineImpl_GetRendererDetails,
    IXACT3EngineImpl_GetFinalMixFormat,
    IXACT3EngineImpl_Initialize,
    IXACT3EngineImpl_ShutDown,
    IXACT3EngineImpl_DoWork,
    IXACT3EngineImpl_CreateSoundBank,
    IXACT3EngineImpl_CreateInMemoryWaveBank,
    IXACT3EngineImpl_CreateStreamingWaveBank,
    IXACT3EngineImpl_PrepareWave,
    IXACT3EngineImpl_PrepareInMemoryWave,
    IXACT3EngineImpl_PrepareStreamingWave,
    IXACT3EngineImpl_RegisterNotification,
    IXACT3EngineImpl_UnRegisterNotification,
    IXACT3EngineImpl_GetCategory,
    IXACT3EngineImpl_Stop,
    IXACT3EngineImpl_SetVolume,
    IXACT3EngineImpl_Pause,
    IXACT3EngineImpl_GetGlobalVariableIndex,
    IXACT3EngineImpl_SetGlobalVariable,
    IXACT3EngineImpl_GetGlobalVariable
};

void* XACT_Internal_Malloc(size_t size)
{
    return CoTaskMemAlloc(size);
}

void XACT_Internal_Free(void* ptr)
{
    return CoTaskMemFree(ptr);
}

void* XACT_Internal_Realloc(void* ptr, size_t size)
{
    return CoTaskMemRealloc(ptr, size);
}

static HRESULT WINAPI XACT3CF_QueryInterface(IClassFactory *iface, REFIID riid, void **ppobj)
{
    if(IsEqualGUID(riid, &IID_IUnknown) ||
       IsEqualGUID(riid, &IID_IClassFactory))
    {
        *ppobj = iface;
        return S_OK;
    }

    *ppobj = NULL;
    WARN("(%p)->(%s, %p): interface not found\n", iface, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

static ULONG WINAPI XACT3CF_AddRef(IClassFactory *iface)
{
    return 2;
}

static ULONG WINAPI XACT3CF_Release(IClassFactory *iface)
{
    return 1;
}

static HRESULT WINAPI XACT3CF_CreateInstance(IClassFactory *iface, IUnknown *pOuter,
                                               REFIID riid, void **ppobj)
{
    HRESULT hr;
    XACT3EngineImpl *object;

    TRACE("(%p)->(%p,%s,%p)\n", iface, pOuter, debugstr_guid(riid), ppobj);

    *ppobj = NULL;

    if(pOuter)
        return CLASS_E_NOAGGREGATION;

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if(!object)
        return E_OUTOFMEMORY;

    object->IXACT3Engine_iface.lpVtbl = &XACT3Engine_Vtbl;

    FACTCreateEngineWithCustomAllocatorEXT(
        0,
        &object->fact_engine,
        XACT_Internal_Malloc,
        XACT_Internal_Free,
        XACT_Internal_Realloc
    );

    hr = IXACT3Engine_QueryInterface(&object->IXACT3Engine_iface, riid, ppobj);
    if(FAILED(hr)){
        HeapFree(GetProcessHeap(), 0, object);
        return hr;
    }

    return hr;
}

static HRESULT WINAPI XACT3CF_LockServer(IClassFactory *iface, BOOL dolock)
{
    TRACE("(%p)->(%d): stub!\n", iface, dolock);
    return S_OK;
}

static const IClassFactoryVtbl XACT3CF_Vtbl =
{
    XACT3CF_QueryInterface,
    XACT3CF_AddRef,
    XACT3CF_Release,
    XACT3CF_CreateInstance,
    XACT3CF_LockServer
};

static IClassFactory XACTFactory = { &XACT3CF_Vtbl };

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD reason, void *pReserved)
{
    TRACE("(%p, %d, %p)\n", hinstDLL, reason, pReserved);

    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        instance = hinstDLL;
        DisableThreadLibraryCalls( hinstDLL );

#ifdef HAVE_FAUDIOLINKEDVERSION
        TRACE("Using FAudio version %d\n", FAudioLinkedVersion() );
#endif

        break;
    }
    return TRUE;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    if (IsEqualGUID(rclsid, &CLSID_XACTEngine))
    {
        TRACE("(%s, %s, %p)\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
        return IClassFactory_QueryInterface(&XACTFactory, riid, ppv);
    }

    FIXME("Unknown class %s\n", debugstr_guid(rclsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources(instance);
}

HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources(instance);
}
