/*++

Splitter: property handlers of the cables' topology filters.

Derived from speakertopo.h of Microsoft's SimpleAudioSample
(Copyright (c) Microsoft Corporation, MS-PL; see LICENSE-MS-PL.txt).
--*/

#ifndef _SPLITTER_CABLETOPO_H_
#define _SPLITTER_CABLETOPO_H_

NTSTATUS PropertyHandler_CableRenderTopoFilter(_In_ PPCPROPERTY_REQUEST PropertyRequest);

NTSTATUS PropertyHandler_CableCaptureTopoFilter(_In_ PPCPROPERTY_REQUEST PropertyRequest);

#endif // _SPLITTER_CABLETOPO_H_
