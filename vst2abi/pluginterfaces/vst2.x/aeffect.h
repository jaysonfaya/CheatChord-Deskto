/* CheatChord: a clean-room description of the VST 2.4 plug-in ABI (the binary interface hosts such as MPC 2 load),
 * written from the public ABI as it is used by open-source hosts. Not Steinberg's SDK. Only what the JUCE VST2 wrapper
 * uses is here. MIT, like the rest of CheatChord. */
#pragma once
#include <stdint.h>

#if defined(_WIN32)
#define VSTCALLBACK __cdecl
#pragma pack(push, 8)
#else
#define VSTCALLBACK
#endif

typedef int16_t VstInt16;
typedef int32_t VstInt32;
typedef int64_t VstInt64;
typedef intptr_t VstIntPtr;

#define kVstVersion 2400
#define kEffectMagic 0x56737450   /* 'VstP' */

struct AEffect;
typedef VstIntPtr (VSTCALLBACK *audioMasterCallback) (struct AEffect*, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
typedef VstIntPtr (VSTCALLBACK *AEffectDispatcherProc) (struct AEffect*, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);
typedef void (VSTCALLBACK *AEffectProcessProc) (struct AEffect*, float** inputs, float** outputs, VstInt32 sampleFrames);
typedef void (VSTCALLBACK *AEffectProcessDoubleProc) (struct AEffect*, double** inputs, double** outputs, VstInt32 sampleFrames);
typedef void (VSTCALLBACK *AEffectSetParameterProc) (struct AEffect*, VstInt32 index, float parameter);
typedef float (VSTCALLBACK *AEffectGetParameterProc) (struct AEffect*, VstInt32 index);

struct AEffect
{
    VstInt32 magic;
    AEffectDispatcherProc dispatcher;
    AEffectProcessProc process;            /* deprecated accumulating process */
    AEffectSetParameterProc setParameter;
    AEffectGetParameterProc getParameter;
    VstInt32 numPrograms, numParams, numInputs, numOutputs, flags;
    VstIntPtr resvd1, resvd2;
    VstInt32 initialDelay, realQualities, offQualities;
    float ioRatio;
    void* object;
    void* user;
    VstInt32 uniqueID, version;
    AEffectProcessProc processReplacing;
    AEffectProcessDoubleProc processDoubleReplacing;
    char future[56];
};

enum VstAEffectFlags
{
    effFlagsHasEditor = 1 << 0,
    effFlagsCanReplacing = 1 << 4,
    effFlagsProgramChunks = 1 << 5,
    effFlagsIsSynth = 1 << 8,
    effFlagsNoSoundInStop = 1 << 9,
    effFlagsCanDoubleReplacing = 1 << 12
};

enum AEffectOpcodes
{
    effOpen = 0, effClose, effSetProgram, effGetProgram, effSetProgramName, effGetProgramName,
    effGetParamLabel, effGetParamDisplay, effGetParamName, effGetVu,
    effSetSampleRate, effSetBlockSize, effMainsChanged,
    effEditGetRect, effEditOpen, effEditClose, effEditDraw, effEditMouse, effEditKey, effEditIdle, effEditTop, effEditSleep,
    effIdentify, effGetChunk, effSetChunk,
    effNumOpcodes
};

enum AudioMasterOpcodes
{
    audioMasterAutomate = 0, audioMasterVersion, audioMasterCurrentId, audioMasterIdle, audioMasterPinConnected
};

enum VstStringConstants
{
    kVstMaxProgNameLen = 24, kVstMaxParamStrLen = 8, kVstMaxVendorStrLen = 64, kVstMaxProductStrLen = 64, kVstMaxEffectNameLen = 32
};

#if defined(_WIN32)
#pragma pack(pop)
#endif
