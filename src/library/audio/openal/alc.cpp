/*
    Copyright 2015-2020 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "alc.h"
#include "efx.h"
#include "../AudioContext.h"
#include "../../logging.h"

namespace libtas {

static ALCdevice dummyDevice = 0;
static ALCcontext dummyContext = -1;
static ALCcontext currentContext = -1;

static ALCenum alcError = ALC_NO_ERROR;
#define ALCSETERROR(error) if(alcError==ALC_NO_ERROR) alcError = error

/* Override */ ALCenum alcGetError(ALCdevice *device)
{
    DEBUGLOGCALL(LCF_SOUND);
    ALCenum err = alcError;
    alcError = ALC_NO_ERROR;
    return err;
}

/* Override */ ALCdevice* alcOpenDevice(const ALCchar* devicename)
{
    DEBUGLOGCALL(LCF_SOUND);
    game_info.audio |= GameInfo::OPENAL;
    game_info.tosend = true;
    return &dummyDevice;
}

/* Override */ ALCboolean alcCloseDevice(ALCdevice* deviceHandle)
{
    DEBUGLOGCALL(LCF_SOUND);
    return ALC_TRUE;
}

/* Override */ ALCcontext* alcCreateContext(ALCdevice *device, const ALCint* attrlist)
{
    DEBUGLOGCALL(LCF_SOUND);

    if (shared_config.audio_disabled)
        return nullptr;

    if (dummyContext != -1) {
        debuglog(LCF_SOUND | LCF_TODO, "We don't support multiple openAL contexts yet");
        return NULL;
    }
    if (attrlist) {
        for (int attr = 0; attrlist[attr] != 0; attr+=2) {
            debuglog(LCF_SOUND, "Attribute ", attrlist[attr], " is ", attrlist[attr+1]);
        }
    }

    dummyContext = 0;
    return &dummyContext;
}

/* Override */ ALCboolean alcMakeContextCurrent(ALCcontext *context)
{
    DEBUGLOGCALL(LCF_SOUND);

    if (context == NULL) {
        currentContext = -1;
        return ALC_TRUE;
    }

    if (*context != 0) {
        ALCSETERROR(ALC_INVALID_CONTEXT);
        return ALC_FALSE;
    }
    currentContext = *context;
    return ALC_TRUE;
}

/* Override */ void alcProcessContext(ALCcontext *context)
{
    DEBUGLOGCALL(LCF_SOUND);
    if (context == NULL)
        ALCSETERROR(ALC_INVALID_CONTEXT);
    if (*context != dummyContext)
        ALCSETERROR(ALC_INVALID_CONTEXT);
}

/* Override */ void alcSuspendContext(ALCcontext *context)
{
    DEBUGLOGCALL(LCF_SOUND);
    if (context == NULL)
        ALCSETERROR(ALC_INVALID_CONTEXT);
    if (*context != dummyContext)
        ALCSETERROR(ALC_INVALID_CONTEXT);
}

/* Override */ void alcDestroyContext(ALCcontext *context)
{
    DEBUGLOGCALL(LCF_SOUND);
    if (context == NULL)
        ALCSETERROR(ALC_INVALID_CONTEXT);
    if (*context == dummyContext) {
        if (*context == currentContext) {
            ALCSETERROR(ALC_INVALID_VALUE);
        }
        else {
            dummyContext = -1;
        }
    }
    else
        ALCSETERROR(ALC_INVALID_CONTEXT);
}

/* Override */ ALCcontext* alcGetCurrentContext(void)
{
    DEBUGLOGCALL(LCF_SOUND);
    if (currentContext == -1)
        return NULL;
    else
        return &dummyContext;

}

/* Override */ ALCdevice* alcGetContextsDevice(ALCcontext *context)
{
    DEBUGLOGCALL(LCF_SOUND);
    return &dummyDevice;
}

/* Override */ ALCboolean alcIsExtensionPresent(ALCdevice *device, const ALCchar *extname)
{
    DEBUGLOGCALL(LCF_SOUND);
    if (extname == NULL) {
        ALCSETERROR(ALC_INVALID_VALUE);
        return ALC_FALSE;
    }

    debuglog(LCF_SOUND, "Extension asked is ", extname);

    if (strcmp(extname, "ALC_ENUMERATION_EXT") == 0) {
        return ALC_TRUE;
    }

    if (strcmp(extname, "ALC_ENUMERATE_ALL_EXT") == 0) {
        return ALC_TRUE;
    }

    if (strcmp(extname, "ALC_EXT_CAPTURE") == 0) {
        return ALC_TRUE;
    }

    if (strcmp(extname, "ALC_SOFT_HRTF") == 0) {
        return ALC_FALSE;
    }

    debuglog(LCF_SOUND | LCF_ERROR, "Extension ", extname, " not supported, but we will still return yes because some games crash if we return no");
    return ALC_TRUE;
}

/* Override */ void* alcGetProcAddress(ALCdevice *device, const ALCchar *funcname)
{
    DEBUGLOGCALL(LCF_SOUND);
    if (funcname == NULL) {
        ALCSETERROR(ALC_INVALID_VALUE);
        return NULL;
    }

    if (strcmp(funcname, "alcSetThreadContext") == 0) {
        return reinterpret_cast<void*>(myalcSetThreadContext);
    }
    if (strcmp(funcname, "alcGetThreadContext") == 0) {
        return reinterpret_cast<void*>(myalcGetThreadContext);
    }
    if (strcmp(funcname, "alcLoopbackOpenDeviceSOFT") == 0) {
        return reinterpret_cast<void*>(myalcLoopbackOpenDeviceSOFT);
    }
    if (strcmp(funcname, "alcIsRenderFormatSupportedSOFT") == 0) {
        return reinterpret_cast<void*>(myalcIsRenderFormatSupportedSOFT);
    }
    if (strcmp(funcname, "alcRenderSamplesSOFT") == 0) {
        return reinterpret_cast<void*>(myalcRenderSamplesSOFT);
    }
    if (strcmp(funcname, "alcGetStringiSOFT") == 0) {
        return reinterpret_cast<void*>(myalcGetStringiSOFT);
    }
    if (strcmp(funcname, "alcResetDeviceSOFT") == 0) {
        return reinterpret_cast<void*>(myalcResetDeviceSOFT);
    }

    debuglog(LCF_SOUND | LCF_ERROR, "Requesting function ", funcname);

    return NULL;
}

/* Override */ ALCenum alcGetEnumValue(ALCdevice *device, const ALCchar *enumname)
{
    DEBUGLOGCALL(LCF_SOUND);
    if (enumname == NULL) {
        ALCSETERROR(ALC_INVALID_VALUE);
        return 0;
    }

    debuglog(LCF_SOUND | LCF_ERROR, "Requesting enum ", enumname);
    return 0;
}

static const ALCchar* alcNoErrorStr = "No error";
static const ALCchar* alcInvalidDeviceStr = "Invalid device";
static const ALCchar* alcInvalidContextStr = "Invalid context";
static const ALCchar* alcInvalidEnumStr = "Invalid enum";
static const ALCchar* alcInvalidValueStr = "Invalid value";
static const ALCchar* alcOutOfMemoryStr = "Out of memory";
static const ALCchar* alcExtensionsStr = "ALC_ENUMERATION_EXT ALC_ENUMERATE_ALL_EXT ALC_EXT_CAPTURE"; // extensions strings separated by space
static const ALCchar* alcDeviceListStr = "libTAS device\0"; // must be double null-terminated
static const ALCchar* alcDeviceStr = "libTAS device";
static const ALCchar* alcCaptureListStr = "\0"; // must be double null-terminated
static const ALCchar* alcDefault = "";

/* Override */ const ALCchar* alcGetString(ALCdevice *device, ALCenum param)
{
    //debuglog(LCF_SOUND, __func__, " call with param ", std::hex, param, std::dec);
    debuglog(LCF_SOUND, __func__, " call with param ", param);

    switch (param) {
        case ALC_DEFAULT_DEVICE_SPECIFIER:
            debuglog(LCF_SOUND, "Request default device");
            return alcDeviceStr;

        case ALC_DEVICE_SPECIFIER:
            if (device == NULL) {
                debuglog(LCF_SOUND, "Request list of available devices");
                return alcDeviceListStr;
            }
            else {
                debuglog(LCF_SOUND, "Request current device");
                return alcDeviceStr;
            }

        case ALC_DEFAULT_ALL_DEVICES_SPECIFIER:
            debuglog(LCF_SOUND, "Request default all device");
            return alcDeviceStr;

        case ALC_ALL_DEVICES_SPECIFIER:
            if (device == NULL) {
                debuglog(LCF_SOUND, "Request list of available all devices");
                return alcDeviceListStr;
            }
            else {
                debuglog(LCF_SOUND, "Request current device");
                return alcDeviceStr;
            }

        case ALC_EXTENSIONS:
            debuglog(LCF_SOUND, "Request list of supported extensions");
            if (device == NULL) {
                ALCSETERROR(ALC_INVALID_DEVICE);
                return NULL;
            }
            return alcExtensionsStr;

        case ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER:
            debuglog(LCF_SOUND, "Request default capture device");
            return NULL;
        case ALC_CAPTURE_DEVICE_SPECIFIER:
            if (device == NULL) {
                debuglog(LCF_SOUND, "Request list of available capture devices");
                return alcCaptureListStr;
            }
            else {
                debuglog(LCF_SOUND, "Request current capture device");
                return NULL;
            }

        /* Request error strings */
        case ALC_NO_ERROR:
            return alcNoErrorStr;
        case ALC_INVALID_DEVICE:
            return alcInvalidDeviceStr;
        case ALC_INVALID_CONTEXT:
            return alcInvalidContextStr;
        case ALC_INVALID_ENUM:
            return alcInvalidEnumStr;
        case ALC_INVALID_VALUE:
            return alcInvalidValueStr;
        case ALC_OUT_OF_MEMORY:
            return alcOutOfMemoryStr;
        default:
            return alcDefault;
    }
}

void alcGetIntegerv(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *values)
{
    DEBUGLOGCALL(LCF_SOUND);

    if (values == NULL)
        return;
    if (size == 0)
        return;

    switch (param) {
        case ALC_FREQUENCY:
            debuglog(LCF_SOUND | LCF_TODO, "Request frequency");
            values[0] = 44100;
            return;
        case ALC_REFRESH:
            debuglog(LCF_SOUND | LCF_TODO, "Request refresh");
            values[0] = 60;
            return;
        case ALC_SYNC:
            debuglog(LCF_SOUND | LCF_TODO, "Request sync");
            values[0] = 0;
            return;
        case ALC_MONO_SOURCES:
            debuglog(LCF_SOUND | LCF_TODO, "Request mono sources");
            values[0] = 255;
            return;
        case ALC_STEREO_SOURCES:
            debuglog(LCF_SOUND | LCF_TODO, "Request stereo sources");
            values[0] = 255;
            return;
        case ALC_ATTRIBUTES_SIZE:
            debuglog(LCF_SOUND | LCF_TODO, "Request attributes size");
            //if (device == NULL)
            values[0] = 13;
            return;
        case ALC_ALL_ATTRIBUTES:
            debuglog(LCF_SOUND | LCF_TODO, "Request all attributes");
            values[0] = ALC_FREQUENCY;
            values[1] = 44100;
            values[2] = ALC_REFRESH;
            values[3] = 60;
            values[4] = ALC_SYNC;
            values[5] = 0;
            values[6] = ALC_MONO_SOURCES;
            values[7] = 255;
            values[8] = ALC_STEREO_SOURCES;
            values[9] = 255;
            values[10] = ALC_MAX_AUXILIARY_SENDS;
            values[11] = 2;
            values[12] = 0;
            return;
        case ALC_MAJOR_VERSION:
            debuglog(LCF_SOUND, "Request major version");
            values[0] = 1;
            return;
        case ALC_MINOR_VERSION:
            debuglog(LCF_SOUND, "Request minor version");
            values[0] = 1;
            return;
        case ALC_CAPTURE_SAMPLES:
            debuglog(LCF_SOUND | LCF_TODO, "Request capture samples");
            values[0] = 0;
            return;
        case ALC_MAX_AUXILIARY_SENDS:
            debuglog(LCF_SOUND | LCF_TODO, "Request max auxiliary sends");
            values[0] = 2;
            return;
        case ALC_NUM_HRTF_SPECIFIERS_SOFT:
            debuglog(LCF_SOUND | LCF_TODO, "Request number of HRTFs");
            values[0] = 0;
            return;
        case ALC_HRTF_SOFT:
            debuglog(LCF_SOUND | LCF_TODO, "Request HRTF state");
            values[0] = ALC_FALSE;
            return;
        case ALC_HRTF_STATUS_SOFT:
            debuglog(LCF_SOUND | LCF_TODO, "Request HRTF status");
            values[0] = ALC_HRTF_DISABLED_SOFT;
            return;
        default:
            debuglog(LCF_SOUND | LCF_TODO, "Unknown param ", param);
            values[0] = 2;
            return;
    }
}

ALCboolean myalcSetThreadContext(ALCcontext *context)
{
    DEBUGLOGCALL(LCF_SOUND | LCF_TODO);
    return ALC_TRUE;
}

ALCcontext* myalcGetThreadContext(void)
{
    DEBUGLOGCALL(LCF_SOUND | LCF_TODO);
    if (currentContext == -1)
        return nullptr;
    else
        return &dummyContext;
}

ALCdevice* myalcLoopbackOpenDeviceSOFT(const ALCchar *deviceName)
{
    DEBUGLOGCALL(LCF_SOUND | LCF_TODO);
    game_info.audio |= GameInfo::OPENAL;
    game_info.tosend = true;
    return &dummyDevice;
}

ALCboolean myalcIsRenderFormatSupportedSOFT(ALCdevice *device, ALCsizei freq, ALCenum channels, ALCenum type)
{
    debuglog(LCF_SOUND | LCF_TODO, __func__, " call with freq ", freq, ", channels ", channels, " and type ", type);
    return ALC_TRUE;
}

void myalcRenderSamplesSOFT(ALCdevice *device, ALCvoid *buffer, ALCsizei samples)
{
    DEBUGLOGCALL(LCF_SOUND | LCF_TODO);
    audiocontext.mixAllSources(samples*audiocontext.outAlignSize);
    memcpy(buffer, audiocontext.outSamples.data(), audiocontext.outBytes);
}

const ALCchar* myalcGetStringiSOFT(ALCdevice *device, ALCenum paramName, ALCsizei index)
{
    DEBUGLOGCALL(LCF_SOUND | LCF_TODO);
    return "";
}

ALCboolean myalcResetDeviceSOFT(ALCdevice *device, const ALCint *attribs)
{
    DEBUGLOGCALL(LCF_SOUND | LCF_TODO);
    return ALC_TRUE;
}

}
