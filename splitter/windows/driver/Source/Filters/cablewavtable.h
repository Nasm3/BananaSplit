/*++

Splitter: wave filters of the cables (the same two for all six cables).

Derived from speakerwavtable.h and micarraywavtable.h of Microsoft's
SimpleAudioSample (Copyright (c) Microsoft Corporation, MS-PL; see
LICENSE-MS-PL.txt).

Every cable runs at 48 kHz stereo, both halves, always: a cable has no clock
of its own to convert between rates, and BananaSplit mixes at 48 kHz. Each
half can be set to 16-bit, 24-bit or 32-bit float in Windows' sound settings
(Properties > Advanced); the halves of one cable may differ, because the ring
between them holds 32-bit float.
--*/

#ifndef _SPLITTER_CABLEWAVTABLE_H_
#define _SPLITTER_CABLEWAVTABLE_H_

#define CABLE_CHANNELS                      2
#define CABLE_SAMPLE_RATE                   48000

// Max # of pin instances: one stream per half (the audio engine in shared
// mode, or one app in exclusive mode).
#define CABLE_MAX_STREAMS                   1

//=============================================================================
// Device formats. Index 1 (24-bit) is the default for both halves.
//
#define CABLE_PCM_FORMAT(bits)                                                  \
    {                                                                           \
        {                                                                       \
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),                          \
            0,                                                                  \
            0,                                                                  \
            0,                                                                  \
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),                              \
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),                             \
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)                   \
        },                                                                      \
        {                                                                       \
            {                                                                   \
                WAVE_FORMAT_EXTENSIBLE,                                         \
                CABLE_CHANNELS,                                                 \
                CABLE_SAMPLE_RATE,                                              \
                CABLE_SAMPLE_RATE * CABLE_CHANNELS * ((bits) / 8),              \
                CABLE_CHANNELS * ((bits) / 8),                                  \
                (bits),                                                         \
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)             \
            },                                                                  \
            (bits),                                                             \
            KSAUDIO_SPEAKER_STEREO,                                             \
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)                              \
        }                                                                       \
    }

static
KSDATAFORMAT_WAVEFORMATEXTENSIBLE CablePinSupportedDeviceFormats[] =
{
    CABLE_PCM_FORMAT(16),   // 0
    CABLE_PCM_FORMAT(24),   // 1
    { // 2: 32-bit float
        {
            sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        {
            {
                WAVE_FORMAT_EXTENSIBLE,
                CABLE_CHANNELS,
                CABLE_SAMPLE_RATE,
                CABLE_SAMPLE_RATE * CABLE_CHANNELS * 4,
                CABLE_CHANNELS * 4,
                32,
                sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)
            },
            32,
            KSAUDIO_SPEAKER_STEREO,
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
        }
    }
};

//
// Supported modes (only on streaming pins).
//
static
MODE_AND_DEFAULT_FORMAT CablePinSupportedDeviceModes[] =
{
    {
        STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT,
        &CablePinSupportedDeviceFormats[1].DataFormat   // 48 kHz 24-bit
    }
};

//
// The entries here must follow the same order as the filter's pin
// descriptor array.
//
static
PIN_DEVICE_FORMATS_AND_MODES CableRenderPinDeviceFormatsAndModes[] =
{
    {
        SystemRenderPin,
        CablePinSupportedDeviceFormats,
        SIZEOF_ARRAY(CablePinSupportedDeviceFormats),
        CablePinSupportedDeviceModes,
        SIZEOF_ARRAY(CablePinSupportedDeviceModes)
    },
    {
        BridgePin,
        NULL,
        0,
        NULL,
        0
    }
};

static
PIN_DEVICE_FORMATS_AND_MODES CableCapturePinDeviceFormatsAndModes[] =
{
    {
        BridgePin,
        NULL,
        0,
        NULL,
        0
    },
    {
        SystemCapturePin,
        CablePinSupportedDeviceFormats,
        SIZEOF_ARRAY(CablePinSupportedDeviceFormats),
        CablePinSupportedDeviceModes,
        SIZEOF_ARRAY(CablePinSupportedDeviceModes)
    }
};

//=============================================================================
// Data ranges: PCM 16..24 bit and float 32 bit, both 48 kHz stereo.
//
static
KSDATARANGE_AUDIO CablePinDataRangesStream[] =
{
    { // 0
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        CABLE_CHANNELS,
        16,
        24,
        CABLE_SAMPLE_RATE,
        CABLE_SAMPLE_RATE
    },
    { // 1
        {
            sizeof(KSDATARANGE_AUDIO),
            KSDATARANGE_ATTRIBUTES,         // An attributes list follows this data range
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT),
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)
        },
        CABLE_CHANNELS,
        32,
        32,
        CABLE_SAMPLE_RATE,
        CABLE_SAMPLE_RATE
    }
};

static
PKSDATARANGE CablePinDataRangePointersStream[] =
{
    PKSDATARANGE(&CablePinDataRangesStream[0]),
    PKSDATARANGE(&PinDataRangeAttributeList),
    PKSDATARANGE(&CablePinDataRangesStream[1]),
    PKSDATARANGE(&PinDataRangeAttributeList),
};

//=============================================================================
static
KSDATARANGE CablePinDataRangesBridge[] =
{
    {
        sizeof(KSDATARANGE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    }
};

static
PKSDATARANGE CablePinDataRangePointersBridge[] =
{
    &CablePinDataRangesBridge[0]
};

//=============================================================================
static
PCPROPERTY_ITEM PropertiesCableWaveFilter[] =
{
    {
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_PROPOSEDATAFORMAT,
        KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_WaveFilter
    },
    {
        &KSPROPSETID_Pin,
        KSPROPERTY_PIN_PROPOSEDATAFORMAT2,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_WaveFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationCableWaveFilter, PropertiesCableWaveFilter);

//=============================================================================
// Render half: apps play into pin 0, pin 1 leads to the topology filter.
//
//                   ----------------------------
//  Host Pin     0-->|                          |--> 1 KSPIN_WAVE_RENDER3_SOURCE
//                   ----------------------------
static
PCPIN_DESCRIPTOR CableRenderWavePins[] =
{
    // KSPIN_WAVE_RENDER3_SINK_SYSTEM
    {
        CABLE_MAX_STREAMS,
        CABLE_MAX_STREAMS,
        0,
        NULL,        // AutomationTable
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersStream),
            CablePinDataRangePointersStream,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // KSPIN_WAVE_RENDER3_SOURCE
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersBridge),
            CablePinDataRangePointersBridge,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
};

static
PCCONNECTION_DESCRIPTOR CableRenderWaveConnections[] =
{
    { PCFILTER_NODE,    KSPIN_WAVE_RENDER3_SINK_SYSTEM,     PCFILTER_NODE,   KSPIN_WAVE_RENDER3_SOURCE }
};

static
PCFILTER_DESCRIPTOR CableRenderWaveFilterDescriptor =
{
    0,                                              // Version
    &AutomationCableWaveFilter,                     // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                       // PinSize
    SIZEOF_ARRAY(CableRenderWavePins),              // PinCount
    CableRenderWavePins,                            // Pins
    sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
    0,                                              // NodeCount
    NULL,                                           // Nodes
    SIZEOF_ARRAY(CableRenderWaveConnections),       // ConnectionCount
    CableRenderWaveConnections,                     // Connections
    0,                                              // CategoryCount
    NULL                                            // Categories  - use defaults (audio, render, capture)
};

//=============================================================================
// Capture half: pin 0 comes from the topology filter, BananaSplit reads pin 1.
//
//                   ----------------------------
//  Bridge Pin   0-->|           ADC            |--> 1 KSPIN_WAVEIN_HOST
//                   ----------------------------
static
PCPIN_DESCRIPTOR CableCaptureWavePins[] =
{
    // KSPIN_WAVE_BRIDGE
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersBridge),
            CablePinDataRangePointersBridge,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // KSPIN_WAVEIN_HOST
    {
        CABLE_MAX_STREAMS,
        CABLE_MAX_STREAMS,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(CablePinDataRangePointersStream),
            CablePinDataRangePointersStream,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            &KSAUDFNAME_RECORDING_CONTROL,
            0
        }
    }
};

static
PCNODE_DESCRIPTOR CableCaptureWaveNodes[] =
{
    // KSNODE_WAVE_ADC
    {
        0,                      // Flags
        NULL,                   // AutomationTable
        &KSNODETYPE_ADC,        // Type
        NULL                    // Name
    }
};

static
PCCONNECTION_DESCRIPTOR CableCaptureWaveConnections[] =
{
    { PCFILTER_NODE,        KSPIN_WAVE_BRIDGE,      KSNODE_WAVE_ADC,     1 },
    { KSNODE_WAVE_ADC,      0,                      PCFILTER_NODE,       KSPIN_WAVEIN_HOST },
};

static
PCFILTER_DESCRIPTOR CableCaptureWaveFilterDescriptor =
{
    0,                                              // Version
    &AutomationCableWaveFilter,                     // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),                       // PinSize
    SIZEOF_ARRAY(CableCaptureWavePins),             // PinCount
    CableCaptureWavePins,                           // Pins
    sizeof(PCNODE_DESCRIPTOR),                      // NodeSize
    SIZEOF_ARRAY(CableCaptureWaveNodes),            // NodeCount
    CableCaptureWaveNodes,                          // Nodes
    SIZEOF_ARRAY(CableCaptureWaveConnections),      // ConnectionCount
    CableCaptureWaveConnections,                    // Connections
    0,                                              // CategoryCount
    NULL                                            // Categories  - use defaults (audio, render, capture)
};

#endif // _SPLITTER_CABLEWAVTABLE_H_
