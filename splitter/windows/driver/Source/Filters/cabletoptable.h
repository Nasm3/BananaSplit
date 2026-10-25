/*++

Splitter: topology filters of the six cables.

Derived from speakertoptable.h and micarray1toptable.h of Microsoft's
SimpleAudioSample (Copyright (c) Microsoft Corporation, MS-PL; see
LICENSE-MS-PL.txt).

Each cable has two topology filters:

  render  (apps play into it):   [0 from wave] ---> [1 line out "Splitter N"]
  capture (BananaSplit reads it): [0 line in "Splitter N Output"] ---> [1 to wave]

There are no volume or mute nodes: Windows then gives every endpoint a
software volume of its own, which is all a cable needs.

The connector pin of every filter carries a name GUID of its own (see
endpoints.h), which is what gives the endpoint its numbered name, so the
pin arrays and filter descriptors are made once per cable by the macros
below.
--*/

#ifndef _SPLITTER_CABLETOPTABLE_H_
#define _SPLITTER_CABLETOPTABLE_H_

#include "cabletopo.h"

//=============================================================================
static
KSDATARANGE CableTopoPinDataRangesBridge[] =
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
PKSDATARANGE CableTopoPinDataRangePointersBridge[] =
{
  &CableTopoPinDataRangesBridge[0]
};

//=============================================================================
// Jacks: only the connector pin has one. Index = pin id.
//
static
KSJACK_DESCRIPTION CableJackDesc =
{
    KSAUDIO_SPEAKER_STEREO,
    JACKDESC_RGB(0xF5, 0xD5, 0x47),            // banana yellow
    eConnTypeUnknown,
    eGeoLocFront,
    eGenLocPrimaryBox,
    ePortConnIntegratedDevice,
    TRUE                                        // always "plugged in"
};

static
PKSJACK_DESCRIPTION CableRenderJackDescriptions[] =
{
    NULL,                                       // KSPIN_TOPO_WAVEOUT_SOURCE
    &CableJackDesc                              // KSPIN_TOPO_LINEOUT_DEST
};

static
PKSJACK_DESCRIPTION CableCaptureJackDescriptions[] =
{
    &CableJackDesc,                             // KSPIN_TOPO_MIC_ELEMENTS (line in)
    NULL                                        // KSPIN_TOPO_BRIDGE
};

//=============================================================================
static
PCPROPERTY_ITEM PropertiesCableRenderTopoFilter[] =
{
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_CableRenderTopoFilter
    },
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION2,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_CableRenderTopoFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationCableRenderTopoFilter, PropertiesCableRenderTopoFilter);

static
PCPROPERTY_ITEM PropertiesCableCaptureTopoFilter[] =
{
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_CableCaptureTopoFilter
    },
    {
        &KSPROPSETID_Jack,
        KSPROPERTY_JACK_DESCRIPTION2,
        KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
        PropertyHandler_CableCaptureTopoFilter
    }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationCableCaptureTopoFilter, PropertiesCableCaptureTopoFilter);

//=============================================================================
static
PCCONNECTION_DESCRIPTOR CableRenderTopoConnections[] =
{
    //  FromNode,       FromPin,                    ToNode,         ToPin
    {   PCFILTER_NODE,  KSPIN_TOPO_WAVEOUT_SOURCE,  PCFILTER_NODE,  KSPIN_TOPO_LINEOUT_DEST }
};

static
PCCONNECTION_DESCRIPTOR CableCaptureTopoConnections[] =
{
    //  FromNode,       FromPin,                    ToNode,         ToPin
    {   PCFILTER_NODE,  KSPIN_TOPO_MIC_ELEMENTS,    PCFILTER_NODE,  KSPIN_TOPO_BRIDGE }
};

//=============================================================================
// One render topology filter per cable: N = 1..6.
//
#define SPLITTER_RENDER_TOPO(N)                                                 \
static                                                                          \
PCPIN_DESCRIPTOR CableRender##N##TopoPins[] =                                   \
{                                                                               \
  /* KSPIN_TOPO_WAVEOUT_SOURCE */                                               \
  {                                                                             \
    0, 0, 0, NULL,                                                              \
    {                                                                           \
      0, NULL, 0, NULL,                                                         \
      SIZEOF_ARRAY(CableTopoPinDataRangePointersBridge),                        \
      CableTopoPinDataRangePointersBridge,                                      \
      KSPIN_DATAFLOW_IN,                                                        \
      KSPIN_COMMUNICATION_NONE,                                                 \
      &KSCATEGORY_AUDIO,                                                        \
      NULL,                                                                     \
      0                                                                         \
    }                                                                           \
  },                                                                            \
  /* KSPIN_TOPO_LINEOUT_DEST: the connector, named "Splitter N" */              \
  {                                                                             \
    0, 0, 0, NULL,                                                              \
    {                                                                           \
      0, NULL, 0, NULL,                                                         \
      SIZEOF_ARRAY(CableTopoPinDataRangePointersBridge),                        \
      CableTopoPinDataRangePointersBridge,                                      \
      KSPIN_DATAFLOW_OUT,                                                       \
      KSPIN_COMMUNICATION_NONE,                                                 \
      &KSNODETYPE_SPEAKER,                                                      \
      &NAME_SPLITTER_RENDER##N,                                                 \
      0                                                                         \
    }                                                                           \
  }                                                                             \
};                                                                              \
                                                                                \
static                                                                          \
PCFILTER_DESCRIPTOR CableRender##N##TopoFilterDescriptor =                      \
{                                                                               \
  0,                                                                            \
  &AutomationCableRenderTopoFilter,                                             \
  sizeof(PCPIN_DESCRIPTOR),                                                     \
  SIZEOF_ARRAY(CableRender##N##TopoPins),                                       \
  CableRender##N##TopoPins,                                                     \
  sizeof(PCNODE_DESCRIPTOR),                                                    \
  0,                                                                            \
  NULL,                                                                         \
  SIZEOF_ARRAY(CableRenderTopoConnections),                                     \
  CableRenderTopoConnections,                                                   \
  0,                                                                            \
  NULL                                                                          \
};

//=============================================================================
// One capture topology filter per cable: N = 1..6.
//
#define SPLITTER_CAPTURE_TOPO(N)                                                \
static                                                                          \
PCPIN_DESCRIPTOR CableCapture##N##TopoPins[] =                                  \
{                                                                               \
  /* KSPIN_TOPO_MIC_ELEMENTS: the connector, named "Splitter N Output" */       \
  {                                                                             \
    0, 0, 0, NULL,                                                              \
    {                                                                           \
      0, NULL, 0, NULL,                                                         \
      SIZEOF_ARRAY(CableTopoPinDataRangePointersBridge),                        \
      CableTopoPinDataRangePointersBridge,                                      \
      KSPIN_DATAFLOW_IN,                                                        \
      KSPIN_COMMUNICATION_NONE,                                                 \
      &KSNODETYPE_LINE_CONNECTOR,                                               \
      &NAME_SPLITTER_CAPTURE##N,                                                \
      0                                                                         \
    }                                                                           \
  },                                                                            \
  /* KSPIN_TOPO_BRIDGE */                                                       \
  {                                                                             \
    0, 0, 0, NULL,                                                              \
    {                                                                           \
      0, NULL, 0, NULL,                                                         \
      SIZEOF_ARRAY(CableTopoPinDataRangePointersBridge),                        \
      CableTopoPinDataRangePointersBridge,                                      \
      KSPIN_DATAFLOW_OUT,                                                       \
      KSPIN_COMMUNICATION_NONE,                                                 \
      &KSCATEGORY_AUDIO,                                                        \
      NULL,                                                                     \
      0                                                                         \
    }                                                                           \
  }                                                                             \
};                                                                              \
                                                                                \
static                                                                          \
PCFILTER_DESCRIPTOR CableCapture##N##TopoFilterDescriptor =                     \
{                                                                               \
  0,                                                                            \
  &AutomationCableCaptureTopoFilter,                                            \
  sizeof(PCPIN_DESCRIPTOR),                                                     \
  SIZEOF_ARRAY(CableCapture##N##TopoPins),                                      \
  CableCapture##N##TopoPins,                                                    \
  sizeof(PCNODE_DESCRIPTOR),                                                    \
  0,                                                                            \
  NULL,                                                                         \
  SIZEOF_ARRAY(CableCaptureTopoConnections),                                    \
  CableCaptureTopoConnections,                                                  \
  0,                                                                            \
  NULL                                                                          \
};

SPLITTER_RENDER_TOPO(1)
SPLITTER_RENDER_TOPO(2)
SPLITTER_RENDER_TOPO(3)
SPLITTER_RENDER_TOPO(4)
SPLITTER_RENDER_TOPO(5)
SPLITTER_RENDER_TOPO(6)

SPLITTER_CAPTURE_TOPO(1)
SPLITTER_CAPTURE_TOPO(2)
SPLITTER_CAPTURE_TOPO(3)
SPLITTER_CAPTURE_TOPO(4)
SPLITTER_CAPTURE_TOPO(5)
SPLITTER_CAPTURE_TOPO(6)

#endif // _SPLITTER_CABLETOPTABLE_H_
