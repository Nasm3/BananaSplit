/*++

Splitter: the parts of a cable that are pure arithmetic, kept apart from
kernel code so they are unit-tested on any computer (tests/cablerule_test.cpp):

  - the reader's cursor: where a capture stream reads and how much of it is
    audio rather than silence;
  - conversion between the halves' sample formats and the ring's 32-bit
    float.

Copyright (c) the BananaSplit authors. MIT licence (see splitter/LICENSE).

Needs ULONG (32-bit), ULONGLONG (64-bit), UCHAR, BOOLEAN, FLOAT, TRUE/FALSE
and memcpy before it is included.
--*/

#ifndef _SPLITTER_CABLERULE_H_
#define _SPLITTER_CABLERULE_H_

// Six cables; SPLITTER_CABLE_COUNT in common.h is checked against this.
#define CABLE_RING_COUNT            6

#define CABLE_RING_CHANNELS         2

// Ring size: 2^15 frames = 0.68 s at 48 kHz (256 KiB per cable).
#define CABLE_RING_FRAMES           32768
#define CABLE_RING_MASK             (CABLE_RING_FRAMES - 1)

// How much the reader leaves in the ring after each read: 20 ms at 48 kHz.
// Both streams move in packets of about 10 ms whose timers fire a few ms
// apart, so this covers a packet of each plus that jitter. The latency of a
// cable is this margin plus one packet: 30 ms or so.
#define CABLE_RING_MARGIN_FRAMES    960

// A reader more than this far behind (beyond what it asks for) skips ahead
// to the margin.
#define CABLE_RING_LATE_FRAMES      (CABLE_RING_MARGIN_FRAMES * 4)

typedef char CABLE_RULE_CHECK_POW2[((CABLE_RING_FRAMES & CABLE_RING_MASK) == 0) ? 1 : -1];
typedef char CABLE_RULE_CHECK_LATE[(CABLE_RING_LATE_FRAMES < CABLE_RING_FRAMES / 2) ? 1 : -1];

// The most a reader may ask for in one call (the streams split larger
// requests): with it, Write - Read never exceeds the ring.
#define CABLE_RING_MAX_TAKE         (CABLE_RING_FRAMES / 4)

typedef char CABLE_RULE_CHECK_TAKE[(CABLE_RING_LATE_FRAMES + CABLE_RING_MAX_TAKE <= CABLE_RING_FRAMES) ? 1 : -1];

//=============================================================================
// Cursor
//
// Write and Read count frames since the driver loaded; Write >= Read always.
// The frames in [Read, Write) are in the ring as long as Write - Read <=
// CABLE_RING_FRAMES, which CableCursorTake keeps true by skipping ahead long
// before that (CABLE_RING_LATE_FRAMES).
//
typedef struct _CABLE_CURSOR
{
    ULONGLONG   Write;
    ULONGLONG   Read;
    BOOLEAN     Primed;     // the reader has its margin and is playing
} CABLE_CURSOR;

static __inline void CableCursorInit(CABLE_CURSOR* c)
{
    c->Write = 0;
    c->Read = 0;
    c->Primed = FALSE;
}

// A capture stream (re)starts: nothing written before now is played.
static __inline void CableCursorResetReader(CABLE_CURSOR* c)
{
    c->Read = c->Write;
    c->Primed = FALSE;
}

// The reader wants Frames frames. Returns how many of them are audio, read
// from position *From on; the rest are silence. Advances the read position.
static __inline ULONG CableCursorTake(CABLE_CURSOR* c, ULONG Frames, ULONGLONG* From)
{
    ULONGLONG avail = c->Write - c->Read;

    *From = c->Read;

    // Far behind (stopped, held up, or the writer jumped): skip ahead so
    // that this read leaves just the margin behind it.
    if (avail > (ULONGLONG)CABLE_RING_LATE_FRAMES + Frames)
    {
        avail = (ULONGLONG)CABLE_RING_MARGIN_FRAMES + Frames;
        c->Read = c->Write - avail;
        *From = c->Read;
    }

    // Waiting for the margin (at start, or after running dry): start once
    // this read can be served and still leave the margin behind it, and
    // start exactly there, so every later read finds the margin too.
    if (!c->Primed)
    {
        if (avail < (ULONGLONG)CABLE_RING_MARGIN_FRAMES + Frames)
        {
            return 0;
        }
        c->Read = c->Write - CABLE_RING_MARGIN_FRAMES - Frames;
        avail = (ULONGLONG)CABLE_RING_MARGIN_FRAMES + Frames;
        *From = c->Read;
        c->Primed = TRUE;
    }

    // Ran dry: play what there is, then wait for the margin again.
    if (avail < Frames)
    {
        c->Read += avail;
        c->Primed = FALSE;
        return (ULONG)avail;
    }

    c->Read += Frames;
    return Frames;
}

//=============================================================================
// Sample formats of a cable half (see cablewavtable.h). All little-endian,
// interleaved stereo.
//
#define CABLE_FMT_PCM16     0
#define CABLE_FMT_PCM24     1
#define CABLE_FMT_FLOAT32   2

static __inline ULONG CableFormatFrameBytes(ULONG Fmt)
{
    return Fmt == CABLE_FMT_PCM16 ? 2 * CABLE_RING_CHANNELS
         : Fmt == CABLE_FMT_PCM24 ? 3 * CABLE_RING_CHANNELS
         : 4 * CABLE_RING_CHANNELS;
}

// NaN and infinity become silence, so one broken app can't poison the
// mixer that records the cable.
static __inline FLOAT CableSanitize(FLOAT x)
{
    FLOAT d = x - x;    // 0 for finite x, NaN otherwise
    return (d == d) ? x : 0.0f;
}

static __inline FLOAT CableClamp(FLOAT x)
{
    x = CableSanitize(x);
    return x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
}

// Bytes in format Fmt -> float samples (Frames frames).
static __inline void CableDecode(const UCHAR* Src, ULONG Fmt, FLOAT* Dst, ULONG Frames)
{
    ULONG n = Frames * CABLE_RING_CHANNELS;
    for (ULONG i = 0; i < n; i++)
    {
        if (Fmt == CABLE_FMT_PCM16)
        {
            short v = (short)(Src[2 * i] | (Src[2 * i + 1] << 8));
            Dst[i] = (FLOAT)v / 32768.0f;
        }
        else if (Fmt == CABLE_FMT_PCM24)
        {
            long v = (long)(Src[3 * i] | (Src[3 * i + 1] << 8) | ((ULONG)Src[3 * i + 2] << 16));
            if (v & 0x800000)
            {
                v -= 0x1000000;
            }
            Dst[i] = (FLOAT)v / 8388608.0f;
        }
        else
        {
            FLOAT v;
            memcpy(&v, Src + 4 * i, sizeof(v));
            Dst[i] = CableSanitize(v);
        }
    }
}

// Float samples -> bytes in format Fmt (Frames frames). Integer formats are
// clipped at full scale; float passes through (only NaN/infinity become 0).
static __inline void CableEncode(const FLOAT* Src, ULONG Fmt, UCHAR* Dst, ULONG Frames)
{
    ULONG n = Frames * CABLE_RING_CHANNELS;
    for (ULONG i = 0; i < n; i++)
    {
        if (Fmt == CABLE_FMT_PCM16)
        {
            FLOAT x = CableClamp(Src[i]) * 32767.0f;
            long v = (long)(x >= 0.0f ? x + 0.5f : x - 0.5f);
            Dst[2 * i]     = (UCHAR)(v & 0xFF);
            Dst[2 * i + 1] = (UCHAR)((v >> 8) & 0xFF);
        }
        else if (Fmt == CABLE_FMT_PCM24)
        {
            FLOAT x = CableClamp(Src[i]) * 8388607.0f;
            long v = (long)(x >= 0.0f ? x + 0.5f : x - 0.5f);
            Dst[3 * i]     = (UCHAR)(v & 0xFF);
            Dst[3 * i + 1] = (UCHAR)((v >> 8) & 0xFF);
            Dst[3 * i + 2] = (UCHAR)((v >> 16) & 0xFF);
        }
        else
        {
            FLOAT v = CableSanitize(Src[i]);
            memcpy(Dst + 4 * i, &v, sizeof(v));
        }
    }
}

#endif // _SPLITTER_CABLERULE_H_
