/*++

Splitter: the ring buffer between the two halves of a cable. See
cablering.h for how the reader keeps its distance.

Copyright (c) the BananaSplit authors. MIT licence (see splitter/LICENSE).
--*/

#include "definitions.h"
#include "cablering.h"

#define CABLERING_POOLTAG   'rSpS'

typedef struct _CABLE_RING
{
    KSPIN_LOCK      Lock;
    FLOAT*          Buffer;         // CABLE_RING_FRAMES * CABLE_RING_CHANNELS
    CABLE_CURSOR    Cursor;         // write/read positions (cablerule.h)
} CABLE_RING;

static CABLE_RING   g_CableRings[CABLE_RING_COUNT];
static BOOLEAN      g_CableRingsReady = FALSE;

#pragma code_seg("PAGE")
_Use_decl_annotations_
NTSTATUS CableRingsInit()
{
    PAGED_CODE();

    RtlZeroMemory(g_CableRings, sizeof(g_CableRings));
    for (ULONG i = 0; i < CABLE_RING_COUNT; i++)
    {
        KeInitializeSpinLock(&g_CableRings[i].Lock);
        // ExAllocatePool2 zeroes the memory: a new ring holds silence.
        g_CableRings[i].Buffer = (FLOAT*)ExAllocatePool2(
            POOL_FLAG_NON_PAGED,
            sizeof(FLOAT) * CABLE_RING_FRAMES * CABLE_RING_CHANNELS,
            CABLERING_POOLTAG);
        if (g_CableRings[i].Buffer == NULL)
        {
            CableRingsFree();
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        CableCursorInit(&g_CableRings[i].Cursor);
    }
    g_CableRingsReady = TRUE;
    return STATUS_SUCCESS;
}

#pragma code_seg("PAGE")
_Use_decl_annotations_
VOID CableRingsFree()
{
    PAGED_CODE();

    g_CableRingsReady = FALSE;
    for (ULONG i = 0; i < CABLE_RING_COUNT; i++)
    {
        if (g_CableRings[i].Buffer != NULL)
        {
            ExFreePoolWithTag(g_CableRings[i].Buffer, CABLERING_POOLTAG);
            g_CableRings[i].Buffer = NULL;
        }
    }
}

#pragma code_seg()
_Use_decl_annotations_
VOID CableRingPush(ULONG Cable, const FLOAT* Samples, ULONG Frames)
{
    if (!g_CableRingsReady || Cable >= CABLE_RING_COUNT || Frames == 0)
    {
        return;
    }

    CABLE_RING* ring = &g_CableRings[Cable];
    KIRQL       oldIrql;

    KeAcquireSpinLock(&ring->Lock, &oldIrql);

    // Only the newest CABLE_RING_FRAMES frames can be kept.
    ULONG skip = Frames > CABLE_RING_FRAMES ? Frames - CABLE_RING_FRAMES : 0;
    ULONGLONG pos = ring->Cursor.Write + skip;
    for (ULONG f = skip; f < Frames; f++, pos++)
    {
        ULONG i = (ULONG)(pos & CABLE_RING_MASK) * CABLE_RING_CHANNELS;
        ring->Buffer[i]     = Samples[f * CABLE_RING_CHANNELS];
        ring->Buffer[i + 1] = Samples[f * CABLE_RING_CHANNELS + 1];
    }
    ring->Cursor.Write += Frames;

    KeReleaseSpinLock(&ring->Lock, oldIrql);
}

#pragma code_seg()
_Use_decl_annotations_
VOID CableRingPull(ULONG Cable, FLOAT* Samples, ULONG Frames)
{
    RtlZeroMemory(Samples, sizeof(FLOAT) * Frames * CABLE_RING_CHANNELS);

    if (!g_CableRingsReady || Cable >= CABLE_RING_COUNT || Frames == 0)
    {
        return;
    }

    CABLE_RING* ring = &g_CableRings[Cable];
    KIRQL       oldIrql;

    KeAcquireSpinLock(&ring->Lock, &oldIrql);

    // Decide where to read from and how many frames are real audio (the
    // rest stays silent), then copy them.
    ULONGLONG from;
    ULONG     count = CableCursorTake(&ring->Cursor, Frames, &from);
    for (ULONG f = 0; f < count; f++)
    {
        ULONG i = (ULONG)((from + f) & CABLE_RING_MASK) * CABLE_RING_CHANNELS;
        Samples[f * CABLE_RING_CHANNELS]     = ring->Buffer[i];
        Samples[f * CABLE_RING_CHANNELS + 1] = ring->Buffer[i + 1];
    }

    KeReleaseSpinLock(&ring->Lock, oldIrql);
}

#pragma code_seg()
_Use_decl_annotations_
VOID CableRingResetReader(ULONG Cable)
{
    if (!g_CableRingsReady || Cable >= CABLE_RING_COUNT)
    {
        return;
    }

    CABLE_RING* ring = &g_CableRings[Cable];
    KIRQL       oldIrql;

    KeAcquireSpinLock(&ring->Lock, &oldIrql);
    CableCursorResetReader(&ring->Cursor);
    KeReleaseSpinLock(&ring->Lock, oldIrql);
}
