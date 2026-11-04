/* CheatChord: the VST 2.4 extensions of the ABI (see aeffect.h). */
#pragma once
#include "aeffect.h"

#if defined(_WIN32)
#pragma pack(push, 8)
#endif

enum Vst2StringConstants { kVstMaxNameLen = 64, kVstMaxLabelLen = 64, kVstMaxShortLabelLen = 8, kVstMaxCategLabelLen = 24, kVstMaxFileNameLen = 100 };

enum AudioMasterOpcodesX
{
    audioMasterWantMidi = 6, audioMasterGetTime, audioMasterProcessEvents, audioMasterSetTime, audioMasterTempoAt,
    audioMasterGetNumAutomatableParameters, audioMasterGetParameterQuantization, audioMasterIOChanged, audioMasterNeedIdle,
    audioMasterSizeWindow, audioMasterGetSampleRate, audioMasterGetBlockSize, audioMasterGetInputLatency,
    audioMasterGetOutputLatency, audioMasterGetPreviousPlug, audioMasterGetNextPlug, audioMasterWillReplaceOrAccumulate,
    audioMasterGetCurrentProcessLevel, audioMasterGetAutomationState, audioMasterOfflineStart, audioMasterOfflineRead,
    audioMasterOfflineWrite, audioMasterOfflineGetCurrentPass, audioMasterOfflineGetCurrentMetaPass,
    audioMasterSetOutputSampleRate, audioMasterGetOutputSpeakerArrangement, audioMasterGetVendorString,
    audioMasterGetProductString, audioMasterGetVendorVersion, audioMasterVendorSpecific, audioMasterSetIcon,
    audioMasterCanDo, audioMasterGetLanguage, audioMasterOpenWindow, audioMasterCloseWindow, audioMasterGetDirectory,
    audioMasterUpdateDisplay, audioMasterBeginEdit, audioMasterEndEdit, audioMasterOpenFileSelector,
    audioMasterCloseFileSelector, audioMasterEditFile, audioMasterGetChunkFile, audioMasterGetInputSpeakerArrangement
};

enum AEffectXOpcodes
{
    effProcessEvents = effSetChunk + 1, effCanBeAutomated, effString2Parameter, effGetNumProgramCategories,
    effGetProgramNameIndexed, effCopyProgram, effConnectInput, effConnectOutput, effGetInputProperties,
    effGetOutputProperties, effGetPlugCategory, effGetCurrentPosition, effGetDestinationBuffer, effOfflineNotify,
    effOfflinePrepare, effOfflineRun, effProcessVarIo, effSetSpeakerArrangement, effSetBlockSizeAndSampleRate,
    effSetBypass, effGetEffectName, effGetErrorText, effGetVendorString, effGetProductString, effGetVendorVersion,
    effVendorSpecific, effCanDo, effGetTailSize, effIdle, effGetIcon, effSetViewPosition, effGetParameterProperties,
    effKeysRequired, effGetVstVersion, effEditKeyDown, effEditKeyUp, effSetEditKnob, effGetMidiProgramName,
    effGetCurrentMidiProgram, effGetMidiProgramCategory, effHasMidiProgramsChanged, effGetMidiKeyName,
    effBeginSetProgram, effEndSetProgram, effGetSpeakerArrangement, effShellGetNextPlugin, effStartProcess,
    effStopProcess, effSetTotalSampleToProcess, effSetPanLaw, effBeginLoadBank, effBeginLoadProgram,
    effSetProcessPrecision, effGetNumMidiInputChannels, effGetNumMidiOutputChannels
};

enum VstEventTypes { kVstMidiType = 1, kVstAudioType, kVstVideoType, kVstParameterType, kVstTriggerType, kVstSysExType };

struct VstEvent { VstInt32 type, byteSize, deltaFrames, flags; char data[16]; };
struct VstEvents { VstInt32 numEvents; VstIntPtr reserved; struct VstEvent* events[2]; };
struct VstMidiEvent
{
    VstInt32 type, byteSize, deltaFrames, flags, noteLength, noteOffset;
    char midiData[4];
    char detune, noteOffVelocity, reserved1, reserved2;
};
enum VstMidiEventFlags { kVstMidiEventIsRealtime = 1 << 0 };
struct VstMidiSysexEvent
{
    VstInt32 type, byteSize, deltaFrames, flags, dumpBytes;
    VstIntPtr resvd1;
    char* sysexDump;
    VstIntPtr resvd2;
};

struct VstTimeInfo
{
    double samplePos, sampleRate, nanoSeconds, ppqPos, tempo, barStartPos, cycleStartPos, cycleEndPos;
    VstInt32 timeSigNumerator, timeSigDenominator, smpteOffset, smpteFrameRate, samplesToNextClock, flags;
};
enum VstTimeInfoFlags
{
    kVstTransportChanged = 1, kVstTransportPlaying = 1 << 1, kVstTransportCycleActive = 1 << 2, kVstTransportRecording = 1 << 3,
    kVstAutomationWriting = 1 << 6, kVstAutomationReading = 1 << 7, kVstNanosValid = 1 << 8, kVstPpqPosValid = 1 << 9,
    kVstTempoValid = 1 << 10, kVstBarsValid = 1 << 11, kVstCyclePosValid = 1 << 12, kVstTimeSigValid = 1 << 13,
    kVstSmpteValid = 1 << 14, kVstClockValid = 1 << 15
};
enum VstSmpteFrameRate
{
    kVstSmpte24fps = 0, kVstSmpte25fps = 1, kVstSmpte2997fps = 2, kVstSmpte30fps = 3, kVstSmpte2997dfps = 4,
    kVstSmpte30dfps = 5, kVstSmpteFilm16mm = 6, kVstSmpteFilm35mm = 7, kVstSmpte239fps = 10, kVstSmpte249fps = 11,
    kVstSmpte599fps = 12, kVstSmpte60fps = 13
};

struct ERect { VstInt16 top, left, bottom, right; };

enum VstPinPropertiesFlags { kVstPinIsActive = 1 << 0, kVstPinIsStereo = 1 << 1, kVstPinUseSpeaker = 1 << 2 };
struct VstPinProperties { char label[kVstMaxLabelLen]; VstInt32 flags, arrangementType; char shortLabel[kVstMaxShortLabelLen]; char future[48]; };

enum VstPlugCategory
{
    kPlugCategUnknown = 0, kPlugCategEffect, kPlugCategSynth, kPlugCategAnalysis, kPlugCategMastering, kPlugCategSpacializer,
    kPlugCategRoomFx, kPlugSurroundFx, kPlugCategRestoration, kPlugCategOfflineProcess, kPlugCategShell, kPlugCategGenerator,
    kPlugCategMaxCount
};

struct MidiKeyName { VstInt32 thisProgramIndex, thisKeyNumber; char keyName[kVstMaxNameLen]; VstInt32 reserved, flags; };

struct VstSpeakerProperties { float azimuth, elevation, radius, reserved; char name[kVstMaxNameLen]; VstInt32 type; char future[28]; };
struct VstSpeakerArrangement { VstInt32 type, numChannels; struct VstSpeakerProperties speakers[8]; };

enum VstSpeakerType
{
    kSpeakerUndefined = 0x7fffffff, kSpeakerM = 0, kSpeakerL, kSpeakerR, kSpeakerC, kSpeakerLfe, kSpeakerLs, kSpeakerRs,
    kSpeakerLc, kSpeakerRc, kSpeakerS, kSpeakerCs = kSpeakerS, kSpeakerSl, kSpeakerSr, kSpeakerTm, kSpeakerTfl, kSpeakerTfc,
    kSpeakerTfr, kSpeakerTrl, kSpeakerTrc, kSpeakerTrr, kSpeakerLfe2
};
enum VstSpeakerArrangementType
{
    kSpeakerArrUserDefined = -2, kSpeakerArrEmpty = -1, kSpeakerArrMono = 0, kSpeakerArrStereo, kSpeakerArrStereoSurround,
    kSpeakerArrStereoCenter, kSpeakerArrStereoSide, kSpeakerArrStereoCLfe, kSpeakerArr30Cine, kSpeakerArr30Music,
    kSpeakerArr31Cine, kSpeakerArr31Music, kSpeakerArr40Cine, kSpeakerArr40Music, kSpeakerArr41Cine, kSpeakerArr41Music,
    kSpeakerArr50, kSpeakerArr51, kSpeakerArr60Cine, kSpeakerArr60Music, kSpeakerArr61Cine, kSpeakerArr61Music,
    kSpeakerArr70Cine, kSpeakerArr70Music, kSpeakerArr71Cine, kSpeakerArr71Music, kSpeakerArr80Cine, kSpeakerArr80Music,
    kSpeakerArr81Cine, kSpeakerArr81Music, kSpeakerArr102, kNumSpeakerArr
};

enum VstProcessPrecision { kVstProcessPrecision32 = 0, kVstProcessPrecision64 };

#if defined(_WIN32)
#pragma pack(pop)
#endif
