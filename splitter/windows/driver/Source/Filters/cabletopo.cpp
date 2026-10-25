/*++

Splitter: property handlers of the cables' topology filters (jack
descriptions; a cable's connector always reports "plugged in").

Derived from speakertopo.cpp of Microsoft's SimpleAudioSample
(Copyright (c) Microsoft Corporation, MS-PL; see LICENSE-MS-PL.txt).
--*/

#pragma warning (disable : 4127)

#include "definitions.h"
#include "endpoints.h"
#include "mintopo.h"
#include "cabletopo.h"
#include "cabletoptable.h"

#pragma code_seg("PAGE")
static
NTSTATUS
JackProperty
(
    _In_ PPCPROPERTY_REQUEST                        PropertyRequest,
    _In_ ULONG                                      cJacks,
    _In_reads_(cJacks) PKSJACK_DESCRIPTION *        Jacks
)
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    // MajorTarget is the miniport object (filled in by portcls).
    NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PCMiniportTopology  pMiniport = (PCMiniportTopology)PropertyRequest->MajorTarget;

    if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Jack))
    {
        if (PropertyRequest->PropertyItem->Id == KSPROPERTY_JACK_DESCRIPTION)
        {
            ntStatus = pMiniport->PropertyHandlerJackDescription(PropertyRequest, cJacks, Jacks);
        }
        else if (PropertyRequest->PropertyItem->Id == KSPROPERTY_JACK_DESCRIPTION2)
        {
            ntStatus = pMiniport->PropertyHandlerJackDescription2(PropertyRequest, cJacks, Jacks, 0);
        }
    }

    return ntStatus;
}

#pragma code_seg("PAGE")
NTSTATUS
PropertyHandler_CableRenderTopoFilter
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
    PAGED_CODE();
    DPF_ENTER(("[PropertyHandler_CableRenderTopoFilter]"));
    return JackProperty(PropertyRequest, ARRAYSIZE(CableRenderJackDescriptions), CableRenderJackDescriptions);
}

#pragma code_seg("PAGE")
NTSTATUS
PropertyHandler_CableCaptureTopoFilter
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
{
    PAGED_CODE();
    DPF_ENTER(("[PropertyHandler_CableCaptureTopoFilter]"));
    return JackProperty(PropertyRequest, ARRAYSIZE(CableCaptureJackDescriptions), CableCaptureJackDescriptions);
}

#pragma code_seg()
