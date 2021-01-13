/*
    win32_mixer - a simple stereo audio mixer for games.
    NOTE: the mixer uses XAudio2 as backend, so this is Windows only.
    
    This a STB-style library, so do this:
    
        #define WIN32_MIXER_IMPLEMENTATION
    in *one* C++ source file before including this header to create the implementation.
    Something like:

        #include ...
        #include ...
        #define WIN32_MIXER_IMPLEMENTATION
        #include "win32_mixer.h"
     
    Additionally you can define, before including this header:
        #define WIN32_MIXER_STATIC
    to have all functions declared as static.
        #define SOUND_STREAM_BUFFER_SIZE n
    to change the default stream buffer size ('n' is an integer > 0).


    Example usage: simple program that loads a WAV file and plays it. (error checking, headers, etc. omitted for brevity.)
    
    #pragma pack(push, 4)
    struct wav_header {

        unsigned char chunkId[4];
        unsigned int chunkSize;
        unsigned char format[4];
        unsigned char subChunkId[4];
        unsigned int subChunkSize;
        unsigned short audioFormat;
        unsigned short numChannels;
        unsigned int sampleRate;
        unsigned int bytesPerSecond;
        unsigned short blockAlign;
        unsigned short bitsPerSample;
        unsigned char dataChunkId[4];
        unsigned int dataSize;
    };
    #pragma pack(pop)

    static unsigned char* LoadWAV(const char* Filename, unsigned int* Size) {
        wav_header Header;

        HANDLE File = CreateFileA(
            Filename,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (!File) return 0;
        DWORD BytesRead = 0;

        ReadFile(
            File,
            &Header,
            sizeof(Header),
            &BytesRead,
            NULL);
    
        if (memcmp(Header.chunkId, "RIFF", 4) != 0) {
            CloseHandle(File);
            return 0;
        }

        if (memcmp(Header.format, "WAVE", 4) != 0) {
            CloseHandle(File);
            return 0;
        }

        if (memcmp(Header.subChunkId, "fmt", 3) != 0) {
            CloseHandle(File);
            return 0;
        }

        if (memcmp(Header.dataChunkId, "data", 4) != 0) {
            CloseHandle(File);
            return 0;
        }

        const uint32 BytesPerSample = (DEFAULT_SOUND_BIT_DEPTH * DEFAULT_SOUND_CHANNELS) / 8;

        *Size = Header.dataSize;
        unsigned char* Buffer = (unsigned char*)malloc(*Size);

        BytesRead = 0;
        ReadFile(
            File,
            Buffer,
            *Size,
            &BytesRead,
            NULL);
    
        CloseHandle(File);
        return Buffer;
    }

    int main() {

        static sound_mixer Mixer;
        InitMixer(&Mixer);

        unsigned int AudioBufferSize = 0;
        unsigned char* AudioBuffer = LoadWAV("C:\\Users\\Filo\\Desktop\\gattaca2.wav", &AudioBufferSize);

        if (!AudioBuffer) {
           QuitMixer(&Mixer);
           return 0;
        }
         
        sound_stream Stream;
        CreateStream(&Stream, "test", AudioBuffer, AudioBufferSize);

        PlaySound(&Mixer, &Stream);
        while (IsSoundPlaying(&Mixer, &Stream));

        QuitMixer(&Mixer);
        free(AudioBuffer);
    
        return 0;
    }
 */


#ifndef WIN32_MIXER_H
#define WIN32_MIXER_H

#ifndef WIN32_MIXER_DEF
#ifdef WIN32_MIXER_STATIC
#define WIN32_MIXER_DEF static
#else
#define WIN32_MIXER_DEF extern
#endif
#endif

typedef unsigned char uint8;
typedef unsigned int uint32;
typedef short int16;
typedef int int32;
typedef float real32;
typedef unsigned long long int uint64;

enum sound_stream_flags {
    SOUND_PLAYING = 0,
    SOUND_LOOPING = 1,
    SOUND_STOPPED = 2,
    SOUND_PAUSED = 3,
    SOUND_STREAM_FLAGS_COUNT
};

enum sound_flags {
    SOUND_FADE_OUT = 0x10,
    SOUND_FADE_IN = 0x20,
    SOUND_FLAGS_COUNT = 2
};

struct volume_control {
    real32 GlobalVolume = 1;
    real32 LeftVolume = 1;
    real32 RightVolume = 1;
};

enum fade_mode {
    FADE_LINEAR = 1,
    FADE_EQPOW = 2,    // equal-power fade
    FADE_PULSEREL = 3, // pulse release fade
    FADE_REL = 4,      // release fade
    FADE_MODE_COUNT
};

struct volume_fader {
    fade_mode Mode = FADE_LINEAR;
    
    real32 StartTime = 0;
    real32 Length = 0;
    real32 EndTime = 0;

    volume_control PrevVolume;
    real32 StartVolume = 0;
    real32 EndVolume = 0;
};

struct sound_stream {
    const char* Name;
    uint8* SoundBuffer = 0;

    uint32 Index = 0xffffFFFF;
    uint32 SampleCount = 0;
    uint32 SamplesPlayed = 0;
    uint32 SamplesToPlay = 0;

    sound_stream_flags Flags = SOUND_STOPPED;
    uint32 SoundFlags = 0;

    volume_control CurrentVolume;
    volume_fader Fader;

    real32 FadeInTime = 0;
    real32 FadeOutTime = 0;
    bool FadeStarted = false;
};

#define MAX_MIXER_STREAM_COUNT 16

#define DEFAULT_SOUND_BIT_DEPTH 16
#define DEFAULT_SOUND_SAMPLES_PER_SEC 48000
#define DEFAULT_SOUND_SECONDS_PER_SAMPLE 2.083e-5
#define DEFAULT_SOUND_CHANNELS 2

#define MAX_SOUND_CHANNELS 2

// NOTE: speaker order goes from left to right.
#define SPEAKER_LEFT 0
#define SPEAKER_RIGHT 1

struct IXAudio2;
struct IXAudio2MasteringVoice;
struct IXAudio2SourceVoice;

struct sound_mixer {
    sound_stream* Streams[MAX_MIXER_STREAM_COUNT];
    uint8 StreamCount = 0;

    int16* SoundBuffer = 0;
    real32* MixBuffers[MAX_SOUND_CHANNELS] = { 0 };

    uint32 SamplesToMix = 0;
    uint32 SamplesMixed[MAX_MIXER_STREAM_COUNT] = { 0 };

    real32 MasterVolume = 1;

    IXAudio2* Handle = 0;
    IXAudio2MasteringVoice* MasterVoice = 0;
    IXAudio2SourceVoice* SourceVoice = 0;

    void* AudioThreadHandle = 0;
    void* EndOfBufferEvent = 0;
    void* StopMixingEvent = 0;
    void* AudioMutex = 0;
};


WIN32_MIXER_DEF bool InitMixer(sound_mixer* Mixer);
WIN32_MIXER_DEF void QuitMixer(sound_mixer* Mixer);

WIN32_MIXER_DEF void CreateStream(sound_stream* Stream, const char* Name, uint8* SoundData, uint32 DataSize);
WIN32_MIXER_DEF void StopAllStreams(sound_mixer* Mixer);
WIN32_MIXER_DEF void PauseAllStreams(sound_mixer* Mixer);
WIN32_MIXER_DEF void StartAllStreams(sound_mixer* Mixer);

WIN32_MIXER_DEF void StopMixer(sound_mixer* Mixer);
WIN32_MIXER_DEF void RestartMixer(sound_mixer* Mixer);

struct sound_parameters {
    volume_control CurrentVolume;
    real32 SoundDuration = 0; // 0 means play the entire sound.
    real32 FadeInTime = 0;
    real32 FadeOutTime = 0;
    uint32 Flags = 0;
    bool Loop = false;
};

WIN32_MIXER_DEF void PlaySound(sound_mixer* Mixer, const char* Name, real32 CurrentVolume = 1.0f, bool Loop = false);
WIN32_MIXER_DEF void PlaySound(sound_mixer* Mixer, const char* Name, const sound_parameters& SoundParams);

WIN32_MIXER_DEF void StopSound(sound_mixer* Mixer, const char* Name);
WIN32_MIXER_DEF void PauseSound(sound_mixer* Mixer, const char* Name);
WIN32_MIXER_DEF void RemoveSound(sound_mixer* Mixer, const char* Name);

WIN32_MIXER_DEF void PlaySound(sound_mixer* Mixer, sound_stream* Stream, real32 CurrentVolume = 1.0f, bool Loop = false);
WIN32_MIXER_DEF void PlaySound(sound_mixer* Mixer, sound_stream* Stream, const sound_parameters& SoundParams);

WIN32_MIXER_DEF void StopSound(sound_mixer* Mixer, sound_stream* Stream);
WIN32_MIXER_DEF void PauseSound(sound_mixer* Mixer, sound_stream* Stream);
WIN32_MIXER_DEF void RemoveSound(sound_mixer* Mixer, sound_stream* Stream);


WIN32_MIXER_DEF bool IsSoundPlaying(sound_mixer* Mixer, sound_stream* Stream);
WIN32_MIXER_DEF bool IsSoundPlaying(sound_mixer* Mixer, const char* Name);



#ifdef WIN32_MIXER_IMPLEMENTATION

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <audioclient.h>
#include <xaudio2.h>
#include <math.h>
#include <assert.h>

#pragma comment(lib, "ole32.lib")

#ifndef SOUND_STREAM_BUFFER_SIZE
#define SOUND_STREAM_BUFFER_SIZE 2400
#endif

#define SOUND_STREAM_BUFFER_COUNT 4

// Useful defines
#define RELEASE_AND_ZERO(p) if(p){p->Release();p = 0;}

#define Max(a, b) ((a) > (b)) ? (a) : (b)
#define Min(a, b) ((a) < (b)) ? (a) : (b)

#define EPSILON 1e-4f
#define ZeroStruct(s) memset(&(s), 0, sizeof(s))

static uint8 SoundOutputBuffers[SOUND_STREAM_BUFFER_COUNT][SOUND_STREAM_BUFFER_SIZE] = {};
static WAVEFORMATEXTENSIBLE WaveFormat;


static DWORD WINAPI AudioThreadProc(void* Data);

struct voice_stream_callback : IXAudio2VoiceCallback {
    HANDLE BufferEndEvent;

    voice_stream_callback(HANDLE Event) : IXAudio2VoiceCallback(), BufferEndEvent( Event ){}
    ~voice_stream_callback(){}

    void OnBufferEnd(void * pBufferContext) { SetEvent( BufferEndEvent ); }

    // Unused methods are stubs
    void OnVoiceProcessingPassEnd() {}
    void OnStreamEnd() {}
    void OnVoiceProcessingPassStart(UINT32 SamplesRequired) {}
    void OnBufferStart(void * pBufferContext) {}
    void OnLoopEnd(void * pBufferContext) {}
    void OnVoiceError(void * pBufferContext, HRESULT Error) {}
};

voice_stream_callback* MixCallBack = 0;

// Simple scoped mutex
struct scoped_lock {
    CRITICAL_SECTION* cs = 0;

    scoped_lock(void* Handle) : cs((CRITICAL_SECTION*)Handle) {
        if (cs) EnterCriticalSection(cs);
    }

    ~scoped_lock() {
        if (cs) LeaveCriticalSection(cs);
    }
};



WIN32_MIXER_DEF bool InitMixer(sound_mixer* Mixer) {

    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        return false;
    }

    if (FAILED(XAudio2Create(&Mixer->Handle, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        return false;
    }

/*
#ifdef DEBUG_MODE
    XAUDIO2_DEBUG_CONFIGURATION Config;
    Config.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS | XAUDIO2_LOG_DETAIL;
    Handle->SetDebugConfiguration(&Config);
#endif
*/

    if (FAILED(Mixer->Handle->CreateMasteringVoice(&Mixer->MasterVoice, DEFAULT_SOUND_CHANNELS, DEFAULT_SOUND_SAMPLES_PER_SEC))) {
        RELEASE_AND_ZERO(Mixer->Handle);
        return false;
    }

    // Mixer initialization
    Mixer->MixBuffers[0] = (real32*)malloc(SOUND_STREAM_BUFFER_SIZE);
    Mixer->MixBuffers[1] = (real32*)malloc(SOUND_STREAM_BUFFER_SIZE);

    Mixer->EndOfBufferEvent = CreateEventA(0, FALSE, FALSE, 0);
    Mixer->StopMixingEvent = CreateEventA(0, FALSE, FALSE, 0);
    Mixer->AudioMutex = (void*)malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection((CRITICAL_SECTION*)Mixer->AudioMutex);

    MixCallBack = new voice_stream_callback(Mixer->EndOfBufferEvent);

    // Initialize the default wave format.
    WaveFormat.Format.cbSize = sizeof(WaveFormat);
    WaveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;

    WaveFormat.Format.wBitsPerSample = DEFAULT_SOUND_BIT_DEPTH;
    WaveFormat.Format.nChannels = DEFAULT_SOUND_CHANNELS; // TODO: multichannel sound playback.
    WaveFormat.Format.nSamplesPerSec = DEFAULT_SOUND_SAMPLES_PER_SEC;

    // Redundant stuff.
    WaveFormat.Format.nBlockAlign = (WORD)(WaveFormat.Format.nChannels * WaveFormat.Format.wBitsPerSample / 8);
    WaveFormat.Format.nAvgBytesPerSec = WaveFormat.Format.nSamplesPerSec * WaveFormat.Format.nBlockAlign;

    WaveFormat.Samples.wValidBitsPerSample = DEFAULT_SOUND_BIT_DEPTH;
    WaveFormat.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    WaveFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    
    if (FAILED(Mixer->Handle->CreateSourceVoice(&Mixer->SourceVoice, &WaveFormat.Format, XAUDIO2_VOICE_NOPITCH | XAUDIO2_VOICE_NOSRC, 2.0f, MixCallBack))) {

        Mixer->MasterVoice->DestroyVoice();
        Mixer->MasterVoice = 0;
        RELEASE_AND_ZERO(Mixer->Handle);

        free(Mixer->MixBuffers[0]);
        free(Mixer->MixBuffers[1]);

        if (Mixer->EndOfBufferEvent) CloseHandle(Mixer->EndOfBufferEvent);
        if (Mixer->StopMixingEvent) CloseHandle(Mixer->StopMixingEvent);
        if (Mixer->AudioMutex) {
            DeleteCriticalSection((CRITICAL_SECTION*)Mixer->AudioMutex);
            free(Mixer->AudioMutex);
        }

        delete(MixCallBack);

        return false;
    }


    memset(SoundOutputBuffers[0], 0, SOUND_STREAM_BUFFER_SIZE);
    memset(SoundOutputBuffers[1], 0, SOUND_STREAM_BUFFER_SIZE);
    memset(SoundOutputBuffers[2], 0, SOUND_STREAM_BUFFER_SIZE);
    memset(SoundOutputBuffers[3], 0, SOUND_STREAM_BUFFER_SIZE);

    Mixer->AudioThreadHandle = CreateThread(0, 0, AudioThreadProc, Mixer, 0, 0);
    Mixer->SourceVoice->Start();

    return true;
}

static void MaybeAddStream(sound_mixer* Mixer, sound_stream* Stream) {
    if (Mixer->StreamCount >= MAX_MIXER_STREAM_COUNT) return;

    if (Stream->Index == 0xffffFFFF) {
        Stream->Index = Mixer->StreamCount;
        Mixer->Streams[Mixer->StreamCount++] = Stream;
    }
}

WIN32_MIXER_DEF void CreateStream(sound_stream* Stream, const char* Name, uint8* SoundData, uint32 DataSize) {
    assert(Stream);
    
    Stream->Name = Name;
    Stream->SoundBuffer = SoundData;

    uint32 SampleSizeInBytes = (DEFAULT_SOUND_BIT_DEPTH * DEFAULT_SOUND_CHANNELS) / 8;
    Stream->SampleCount = Stream->SamplesToPlay = DataSize / SampleSizeInBytes;
}


static void BeginFade(sound_stream* ss, real32 StartVolume, real32 TargetVolume, real32 StartTime, real32 Length) {
    assert(Length != 0);
    auto Fader = &ss->Fader;

    Fader->StartTime = StartTime;
    Fader->Length = Length;
    Fader->EndTime = StartTime + Length;
    Fader->PrevVolume = ss->CurrentVolume;

    Fader->StartVolume = StartVolume;
    Fader->EndVolume = TargetVolume;

    if (StartVolume < 0) Fader->StartVolume = ss->CurrentVolume.GlobalVolume;

    ss->FadeStarted = true;
}

static void EndFade(sound_stream* ss) {
    if (ss->FadeStarted) {
        ss->FadeStarted = false;
        ss->CurrentVolume = ss->Fader.PrevVolume;
        memset(&ss->Fader, 0, sizeof(volume_fader));
    }
}

// Taken from Sean Barret's mixer code.
static inline real32 ComputeFade(fade_mode Mode, real32 t) {

    real32 Result = 1;
    switch (Mode) {
        case FADE_LINEAR: Result = t; break;
        case FADE_EQPOW: 
        {
            Result = 1.57f * t + t * t * (-0.43f * t - 0.14f);
        } break;
        case FADE_PULSEREL:
        {
            real32 p = 1;
            real32 d = fabsf((1.0f - t) * 20 - 1);

            if (d < 1) p = 1 + (1 - (3 * d * d - 2 * d * d * d)) * 0.2f;

            real32 r = t * t * t;
            r = r * r; r *= 0.5f;

            d = (t < 0.95f) ? 1-(0.95f - t) * 16 : 1;
            Result = p * (r < d ? d : r);
        } break;
        case FADE_REL:
        {
            real32 r = t * t * t;
            r = r * r; r *= 0.5f;
            
            real32  d = 1-(1-t)*15;
            Result = r < d ? d : r;
        } break;
    }

    return Result;
}

static inline real32 Clamp01(real32 a) {
    if (a > 1) a = 1;
    if (a < 0) a = 0;
    return a;
}

// TODO: use FadeInTime and FadeOutTime
static void FadeVolume(sound_stream* ss, real32 CurrentTime, real32* NewVolume) {
    if ((ss->FadeStarted == false) || (ss->Fader.Length == 0)) return;

    auto Fader = &ss->Fader;
    if (CurrentTime > Fader->EndTime) {
        return EndFade(ss);
    }
    
    real32 FromVolume = Fader->StartVolume;
    real32 GlobalDelta = Fader->EndVolume - FromVolume;
    real32 d = Clamp01((CurrentTime - Fader->StartTime) / Fader->Length);
    
    *NewVolume = Max(FromVolume + GlobalDelta * ComputeFade(Fader->Mode, d), 0.0f); // Probably redundant Max() here.
}

static void MixStream(sound_stream* Stream, sound_mixer* Mixer) {

    const uint32 BytesPerSample = (DEFAULT_SOUND_BIT_DEPTH * DEFAULT_SOUND_CHANNELS) / 8;
    uint32 SampleCount = Stream->SamplesToPlay;

    if (Mixer->SamplesToMix > 0) {
        SampleCount = Min(Mixer->SamplesToMix, Stream->SamplesToPlay);
    }

    // Do the actual mixing
    // NOTE: this is 2->2 (stereo to stereo).
    real32* Dest1 = Mixer->MixBuffers[SPEAKER_LEFT];
    real32* Dest2 = Mixer->MixBuffers[SPEAKER_RIGHT];

    real32* StreamVolume = &Stream->CurrentVolume.GlobalVolume;
    real32* LeftVolume = &Stream->CurrentVolume.LeftVolume;
    real32* RightVolume = &Stream->CurrentVolume.RightVolume;

    real32 MasterVolume = Mixer->MasterVolume;

    // Do the fading.
    if ((Stream->SoundFlags & SOUND_FADE_IN) && !Stream->FadeStarted) {

        if (Stream->SamplesPlayed == 0) {
            real32 FadeTime = Stream->FadeInTime > 0.0f ? Stream->FadeInTime : 0.5f;
            BeginFade(Stream, 0, *StreamVolume, 0, FadeTime);
        }
    }

    if ((Stream->SoundFlags & SOUND_FADE_OUT) && !Stream->FadeStarted) {

        real32 FadeTime = Stream->FadeOutTime > 0.0f ? Stream->FadeOutTime : 0.75f; // TODO: hardcoded
        uint32 SamplesToFade = DEFAULT_SOUND_SAMPLES_PER_SEC / FadeTime;

        if (Stream->SamplesPlayed >= (Stream->SamplesToPlay - SamplesToFade)) {
            real32 StartTime = Stream->SamplesPlayed * DEFAULT_SOUND_SECONDS_PER_SAMPLE;
            FadeTime = (Stream->SamplesToPlay - Stream->SamplesPlayed) * DEFAULT_SOUND_SECONDS_PER_SAMPLE;
            BeginFade(Stream, -1, 0, StartTime, FadeTime);
        }
    }


    int16* Src = (int16*)(Stream->SoundBuffer + (uint64)Stream->SamplesPlayed * BytesPerSample);
    real32 CurrentTime = Stream->SamplesPlayed * DEFAULT_SOUND_SECONDS_PER_SAMPLE;
    for (uint32 i = 0; i < SampleCount; ++i) {
            real32 v1 = *Src++;
            real32 v2 = *Src++;

            real32 Volume = MasterVolume * *StreamVolume;

            *Dest1++ += Volume * *LeftVolume * v1;
            *Dest2++ += Volume * *RightVolume * v2;

            FadeVolume(Stream, CurrentTime, StreamVolume);
            CurrentTime += DEFAULT_SOUND_SECONDS_PER_SAMPLE;
     }

    Stream->SamplesPlayed += SampleCount;
    Mixer->SamplesMixed[Stream->Index] += SampleCount;
}

static inline bool ShouldMixStream(sound_mixer* Mixer, sound_stream* Stream) {
    if (Stream->Flags == SOUND_STOPPED || Stream->Flags == SOUND_PAUSED) return false;
    if (Mixer->SamplesMixed[Stream->Index] >= Stream->SamplesToPlay) return false;
    if (Stream->CurrentVolume.GlobalVolume <= EPSILON && !Stream->FadeStarted) return false;

    return true;
}

static void Mix(sound_mixer* Mixer, uint32 SamplesToMix = 0) {

    if (Mixer->MasterVolume <= EPSILON) return; // Don't do the mixing, the sound will be inaudible anyway!
    Mixer->SamplesToMix = SamplesToMix;

    memset(Mixer->MixBuffers[0], 0, SOUND_STREAM_BUFFER_SIZE);
    memset(Mixer->MixBuffers[1], 0, SOUND_STREAM_BUFFER_SIZE);

    uint32 StreamsMixed = 0;
    for (uint32 i = 0; i < Mixer->StreamCount; ++i) {
        auto Stream = Mixer->Streams[i];

        if (Stream->Flags == SOUND_LOOPING) {
            if (Stream->SamplesPlayed >= Stream->SamplesToPlay) {
                Stream->SamplesPlayed = 0;
                Mixer->SamplesMixed[Stream->Index] = 0;
            }
        }

        if (!ShouldMixStream(Mixer, Stream)) continue;

        MixStream(Stream, Mixer);
        ++StreamsMixed;

        if (Stream->Flags != SOUND_LOOPING && Stream->SamplesPlayed >= Stream->SamplesToPlay) {
            Mixer->SamplesMixed[Stream->Index] = 0;
            Stream->Flags = SOUND_STOPPED;
        }
    }

    if (!StreamsMixed) {
        memset(Mixer->SoundBuffer, Mixer->SamplesToMix * 4, 0); // Fill the sound buffer with 0's, don't bother doing the conversion.
        return;
    }

    // Convert the mixed buffer to 16 bit
    real32* SrcLeft  = Mixer->MixBuffers[0];
    real32* SrcRight = Mixer->MixBuffers[1];

    int16* Dest = Mixer->SoundBuffer;
    for (int32 SampleIndex = 0;
        SampleIndex < Mixer->SamplesToMix;
        ++SampleIndex)
    {
        // Write out all channels from left to right.
        // NOTE: this assumes a stereo output. :MultiChannelPlayback
        *Dest++ = (int16)(*SrcLeft++ + 0.5f); // L
        *Dest++ = (int16)(*SrcRight++ + 0.5f); // R
    }
}

static DWORD WINAPI AudioThreadProc(void* Data) {
    auto Mixer = static_cast<sound_mixer*>(Data);

    const uint32 BytesPerSample = (DEFAULT_SOUND_BIT_DEPTH * DEFAULT_SOUND_CHANNELS) / 8;
    const uint32 SamplesToMix = SOUND_STREAM_BUFFER_SIZE / BytesPerSample;
    int32 BufferIndex = 0;

    while (WaitForSingleObject(Mixer->StopMixingEvent, 0) != WAIT_OBJECT_0) {

        if (!Mixer->StreamCount) continue;

        XAUDIO2_VOICE_STATE State;
        Mixer->SourceVoice->GetState(&State);

        while (State.BuffersQueued < SOUND_STREAM_BUFFER_COUNT)
        {
            Mixer->SoundBuffer = (int16*)SoundOutputBuffers[BufferIndex];
            Mix(Mixer, SamplesToMix);

            XAUDIO2_BUFFER Info;
            ZeroStruct(Info);
            Info.AudioBytes = SOUND_STREAM_BUFFER_SIZE;
            Info.pAudioData = (uint8*)Mixer->SoundBuffer;

            auto Result = Mixer->SourceVoice->SubmitSourceBuffer(&Info);
            assert(Result == S_OK);

            BufferIndex = (BufferIndex + 1) & (SOUND_STREAM_BUFFER_COUNT - 1);
            Mixer->SourceVoice->GetState(&State);
        }
        WaitForSingleObject(Mixer->EndOfBufferEvent, INFINITE);
    }

    return 0;
}

static sound_stream* GetStreamByName(sound_mixer* Mixer, const char* Name) {
    sound_stream* Stream = 0;
    for (uint8 i = 0; i < Mixer->StreamCount; ++i) {
        sound_stream *Current = Mixer->Streams[i];
            
        if (strcmp(Name, Current->Name) == 0) {
            return Current;
        }
    }

    return Stream;
}

WIN32_MIXER_DEF void PlaySound(sound_mixer* Mixer, const char* Name, real32 CurrentVolume, bool Loop) {
    
    sound_stream* Stream = GetStreamByName(Mixer, Name);
    if (!Stream || Stream->Index == 0xffffFFFF) return;
    
    scoped_lock Lock(Mixer->AudioMutex);
    if (CurrentVolume >= 0) Stream->CurrentVolume.GlobalVolume = CurrentVolume;

    if (Stream->SamplesPlayed >= Stream->SampleCount) {
        // Wait for the sound to finish before restarting it.
        // This is to avoid glitches when playing the sound repeatedly.
        Stream->SamplesPlayed = 0;
    }

    Stream->Flags = Loop ? SOUND_LOOPING : SOUND_PLAYING;

    MaybeAddStream(Mixer, Stream);
}

void PlaySound(sound_mixer* Mixer, const char* Name, const sound_parameters& SoundParams) {
    sound_stream* Stream = GetStreamByName(Mixer, Name);
    if (!Stream || Stream->Index == 0xffffFFFF) return;

    scoped_lock Lock(Mixer->AudioMutex);
    Stream->CurrentVolume = SoundParams.CurrentVolume;
    Stream->FadeInTime = SoundParams.FadeInTime;
    Stream->FadeOutTime = SoundParams.FadeOutTime;

    if (SoundParams.SoundDuration > 0.0f) {
        Stream->SamplesToPlay = Min((uint32)(SoundParams.SoundDuration * DEFAULT_SOUND_SAMPLES_PER_SEC), Stream->SampleCount);
    }

    if (Stream->SamplesPlayed >= Stream->SampleCount) {
        // Wait for the sound to finish before restarting it.
        // This is to avoid glitches when playing the sound repeatedly.
        Stream->SamplesPlayed = 0;
    }

    Stream->Flags = SoundParams.Loop ? SOUND_LOOPING : SOUND_PLAYING;
    Stream->SoundFlags = SoundParams.Flags;

    MaybeAddStream(Mixer, Stream);
}

WIN32_MIXER_DEF void StopAllStreams(sound_mixer* Mixer) {
    if (!Mixer->StreamCount) return;

    scoped_lock Lock(Mixer->AudioMutex);
    for (uint32 i = 0; i < Mixer->StreamCount; ++i) {
        auto Stream = Mixer->Streams[i];
        Stream->Flags = SOUND_STOPPED;
        Stream->SamplesPlayed = 0;
        Mixer->SamplesMixed[Stream->Index] = 0;
    }
}

WIN32_MIXER_DEF void PauseAllStreams(sound_mixer* Mixer) {
    if (!Mixer->StreamCount) return;

    scoped_lock Lock(Mixer->AudioMutex);
    for (uint32 i = 0; i < Mixer->StreamCount; ++i) {
        auto Stream = Mixer->Streams[i];
        Stream->Flags = SOUND_PAUSED;
    }
}

WIN32_MIXER_DEF void StartAllStreams(sound_mixer* Mixer) {
    if (!Mixer->StreamCount) return;

    scoped_lock Lock(Mixer->AudioMutex);
    for (uint32 i = 0; i < Mixer->StreamCount; ++i) {
        auto Stream = Mixer->Streams[i];
        Stream->Flags = SOUND_PLAYING;
    }
}

WIN32_MIXER_DEF bool IsSoundPlaying(sound_mixer* Mixer, const char* Name) {
    sound_stream* Stream = GetStreamByName(Mixer, Name);
    if (!Stream || Stream->Index == 0xffffFFFF) return false;

    return Stream->SamplesToPlay > Stream->SamplesPlayed;
}

WIN32_MIXER_DEF bool IsSoundPlaying(sound_mixer* Mixer, sound_stream* Stream) {
    assert(Stream);
    if (Stream->Index == 0xffffFFFF) return false;
    return Stream->SamplesToPlay > Stream->SamplesPlayed;
}


WIN32_MIXER_DEF void StopSound(sound_mixer* Mixer, const char* Name) {
    if (!Mixer->StreamCount) return;

    sound_stream* Stream = GetStreamByName(Mixer, Name);
    if (!Stream || Stream->Index == 0xffffFFFF) return;

    scoped_lock Lock(Mixer->AudioMutex);
    Stream->Flags = SOUND_STOPPED;
    Stream->SamplesPlayed = 0; // Restart the stream.
    Mixer->SamplesMixed[Stream->Index] = 0;
}

WIN32_MIXER_DEF void PauseSound(sound_mixer* Mixer, const char* Name) {
    if (!Mixer->StreamCount) return;

    sound_stream* Stream = GetStreamByName(Mixer, Name);    
    if (!Stream || Stream->Index == 0xffffFFFF) return;

    scoped_lock Lock(Mixer->AudioMutex);
    Stream->Flags = SOUND_PAUSED;
}


WIN32_MIXER_DEF void RemoveSound(sound_mixer* Mixer, const char* Name) {
    if (!Mixer->StreamCount) return;

    sound_stream* Stream = GetStreamByName(Mixer, Name);
    if (!Stream || Stream->Index == 0xffffFFFF) return;

    scoped_lock Lock(Mixer->AudioMutex);
    if (Stream->Index == Mixer->StreamCount - 1) {
        Mixer->Streams[Stream->Index] = 0;
        Stream->Index = 0xffffFFFF;
        --Mixer->StreamCount;
    }
    else {
        for (uint32 i = Stream->Index; i < Mixer->StreamCount - 1; ++i) {
            auto s1 = Mixer->Streams[i];
            auto s2 = Mixer->Streams[i + 1];
            s1 = s2;
        }
        Stream->Index = 0xffffFFFF;
        Mixer->Streams[--Mixer->StreamCount] = 0;
    }
}

WIN32_MIXER_DEF void PlaySound(sound_mixer* Mixer, sound_stream* Stream, real32 CurrentVolume, bool Loop) {
    assert(Stream);

    scoped_lock Lock(Mixer->AudioMutex);
    if (CurrentVolume >= 0) Stream->CurrentVolume.GlobalVolume = CurrentVolume;

    if (Stream->SamplesPlayed >= Stream->SampleCount) {
        // Wait for the sound to finish before restarting it.
        // This is to avoid glitches when playing the sound repeatedly.
        Stream->SamplesPlayed = 0;
    }

    Stream->Flags = Loop ? SOUND_LOOPING : SOUND_PLAYING;

    MaybeAddStream(Mixer, Stream);
}

WIN32_MIXER_DEF void PlaySound(sound_mixer* Mixer, sound_stream* Stream, const sound_parameters& SoundParams) {
    assert(Stream);
    
    scoped_lock Lock(Mixer->AudioMutex);
    Stream->CurrentVolume = SoundParams.CurrentVolume;
    Stream->FadeInTime = SoundParams.FadeInTime;
    Stream->FadeOutTime = SoundParams.FadeOutTime;

    if (SoundParams.SoundDuration > 0.0f) {
        Stream->SamplesToPlay = Min((uint32)(SoundParams.SoundDuration * DEFAULT_SOUND_SAMPLES_PER_SEC), Stream->SampleCount);
    }

    if (Stream->SamplesPlayed >= Stream->SampleCount) {
        // Wait for the sound to finish before restarting it.
        // This is to avoid glitches when playing the sound repeatedly.
        Stream->SamplesPlayed = 0;
    }

    Stream->Flags = SoundParams.Loop ? SOUND_LOOPING : SOUND_PLAYING;
    Stream->SoundFlags = SoundParams.Flags;

    MaybeAddStream(Mixer, Stream);
}

WIN32_MIXER_DEF void StopSound(sound_mixer* Mixer, sound_stream* Stream) {
    assert(Stream);
    if (Stream->Index == 0xffffFFFF) return;

    scoped_lock Lock(Mixer->AudioMutex);
    Stream->Flags = SOUND_STOPPED;
    Stream->SamplesPlayed = 0; // Restart the stream.
    Mixer->SamplesMixed[Stream->Index] = 0;
}

WIN32_MIXER_DEF void PauseSound(sound_mixer* Mixer, sound_stream* Stream) {
    assert(Stream);
    if (Stream->Index == 0xffffFFFF) return;

    scoped_lock Lock(Mixer->AudioMutex);
    Stream->Flags = SOUND_PAUSED;
}

WIN32_MIXER_DEF void RemoveSound(sound_mixer* Mixer, sound_stream* Stream) {
    assert(Stream);
    if (Stream->Index == 0xffffFFFF) return;
    
    scoped_lock Lock(Mixer->AudioMutex);
    if (Stream->Index == Mixer->StreamCount - 1) {
        Mixer->Streams[Stream->Index] = 0;
        Stream->Index = 0xffffFFFF;
        --Mixer->StreamCount;
    }
    else {
        for (uint32 i = Stream->Index; i < Mixer->StreamCount - 1; ++i) {
            auto s1 = Mixer->Streams[i];
            auto s2 = Mixer->Streams[i + 1];
            s1 = s2;
        }
        Stream->Index = 0xffffFFFF;
        Mixer->Streams[--Mixer->StreamCount] = 0;
    }
}

WIN32_MIXER_DEF void QuitMixer(sound_mixer* Mixer) {

    SetEvent(Mixer->EndOfBufferEvent);
    SetEvent(Mixer->StopMixingEvent);

    WaitForSingleObject(Mixer->AudioThreadHandle, INFINITE);
    CloseHandle(Mixer->AudioThreadHandle);

    if (Mixer->SourceVoice) {
        Mixer->SourceVoice->Stop();
        Mixer->SourceVoice->FlushSourceBuffers();
    }

    if (Mixer->Handle) Mixer->Handle->StopEngine();

    if (Mixer->SourceVoice) Mixer->SourceVoice->DestroyVoice();
    if (Mixer->MasterVoice) Mixer->MasterVoice->DestroyVoice();

    free(Mixer->MixBuffers[0]);
    free(Mixer->MixBuffers[1]);

    CloseHandle(Mixer->StopMixingEvent);
    CloseHandle(Mixer->EndOfBufferEvent);

    DeleteCriticalSection((CRITICAL_SECTION*)Mixer->AudioMutex);
    free(Mixer->AudioMutex);

    RELEASE_AND_ZERO(Mixer->Handle);

    if (MixCallBack) delete(MixCallBack);
    
    CoUninitialize();
}

WIN32_MIXER_DEF void StopMixer(sound_mixer* Mixer) {
    SetEvent(Mixer->EndOfBufferEvent);
    SetEvent(Mixer->StopMixingEvent);

    WaitForSingleObject(Mixer->AudioThreadHandle, INFINITE);
    CloseHandle(Mixer->AudioThreadHandle);

    if (Mixer->SourceVoice) {
        Mixer->SourceVoice->Stop();
        Mixer->SourceVoice->FlushSourceBuffers();
    }

    if (Mixer->Handle) Mixer->Handle->StopEngine();

    // Reset all sound streams
    for (uint32 i = 0; i < Mixer->StreamCount; ++i) {
        auto Stream = Mixer->Streams[i];
        Stream->Flags = SOUND_STOPPED;
        Stream->SamplesPlayed = 0;
        Mixer->SamplesMixed[Stream->Index] = 0;
    }

}

WIN32_MIXER_DEF void RestartMixer(sound_mixer* Mixer) {
    Mixer->Handle->StartEngine();

    Mixer->AudioThreadHandle = CreateThread(0, 0, AudioThreadProc, Mixer, 0, 0);
    Mixer->SourceVoice->Start();
}


#endif // WIN32_MIXER_IMPLEMENTATION

#endif
