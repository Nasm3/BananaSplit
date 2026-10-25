/*++

Splitter: the ring buffer between the two halves of a cable.

Copyright (c) the BananaSplit authors. MIT licence (see splitter/LICENSE).

What an app plays into "Splitter N" is pushed into ring N by the render
stream; the capture stream of "Splitter N Output" pulls it out again. The
ring holds interleaved 32-bit float stereo frames, so each half can use its
own sample format.

Both halves are clocked by the same performance counter at exactly 48 kHz
(see CMiniportWaveRTStream::UpdatePosition), so they never drift apart: the
reader only has to keep a small margin behind the writer to ride out the
timer jitter of the two streams (cablerule.h has the rules).

  - The reader starts, and restarts after running dry, only once the margin
    (CABLE_RING_MARGIN_FRAMES) has been written. Until then it reads silence.
  - If the reader falls far behind (it was stopped, or held up), it skips
    ahead to the margin instead of playing old audio late.
  - Nothing playing into the cable: the reader runs dry and reads silence.

Calls may come at any IRQL <= DISPATCH_LEVEL (the streams call them under
their position spin lock). Nothing here allocates except CableRingsInit.
--*/

#ifndef _SPLITTER_CABLERING_H_
#define _SPLITTER_CABLERING_H_

// Needs the kernel headers (definitions.h) included first.
#include "cablerule.h"

// Allocate the rings (PASSIVE_LEVEL, DriverEntry) and free them
// (DriverUnload). Free is safe if Init failed or never ran.
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS CableRingsInit();

_IRQL_requires_(PASSIVE_LEVEL)
VOID CableRingsFree();

// Append Frames interleaved stereo frames to cable Cable's ring.
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID CableRingPush(_In_ ULONG Cable, _In_reads_(Frames * CABLE_RING_CHANNELS) const FLOAT* Samples, _In_ ULONG Frames);

// Take Frames frames out of cable Cable's ring; silence where there is none.
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID CableRingPull(_In_ ULONG Cable, _Out_writes_(Frames * CABLE_RING_CHANNELS) FLOAT* Samples, _In_ ULONG Frames);

// A capture stream starts: begin again behind the writer's current position.
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID CableRingResetReader(_In_ ULONG Cable);

#endif // _SPLITTER_CABLERING_H_
