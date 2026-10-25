// Unit tests for the driver's pure rules (Source/Utilities/cablerule.h):
// the reader cursor and the sample format conversions. Builds with any
// C++ compiler on any computer:
//
//   c++ -std=c++17 -O2 -Wall -o cablerule_test tests/cablerule_test.cpp && ./cablerule_test
//
// Copyright (c) the BananaSplit authors. MIT licence (see splitter/LICENSE).

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <limits>
#include <vector>

typedef uint32_t ULONG;
typedef uint64_t ULONGLONG;
typedef unsigned char UCHAR;
typedef unsigned char BOOLEAN;
typedef float FLOAT;
#define TRUE 1
#define FALSE 0
#define __inline inline

#include "../driver/Source/Utilities/cablerule.h"

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                             \
        }                                                                           \
    } while (0)

// A ring like the driver's, built on the same cursor, for end-to-end checks.
struct Ring {
    CABLE_CURSOR c;
    std::vector<float> buf = std::vector<float>(CABLE_RING_FRAMES * 2, 0.0f);
    Ring() { CableCursorInit(&c); }
    void push(const float* s, ULONG frames) {
        ULONG skip = frames > CABLE_RING_FRAMES ? frames - CABLE_RING_FRAMES : 0;
        ULONGLONG pos = c.Write + skip;
        for (ULONG f = skip; f < frames; f++, pos++) {
            ULONG i = (ULONG)(pos & CABLE_RING_MASK) * 2;
            buf[i] = s[2 * f];
            buf[i + 1] = s[2 * f + 1];
        }
        c.Write += frames;
    }
    void pull(float* s, ULONG frames) {
        std::memset(s, 0, sizeof(float) * frames * 2);
        ULONGLONG from;
        ULONG n = CableCursorTake(&c, frames, &from);
        for (ULONG f = 0; f < n; f++) {
            ULONG i = (ULONG)((from + f) & CABLE_RING_MASK) * 2;
            s[2 * f] = buf[i];
            s[2 * f + 1] = buf[i + 1];
        }
    }
};

// Frame k carries the value k+1 on L and -(k+1) on R, so every frame can be
// traced through the ring.
static void ramp(std::vector<float>& v, ULONGLONG first, ULONG frames) {
    v.resize(frames * 2);
    for (ULONG f = 0; f < frames; f++) {
        v[2 * f] = (float)(first + f + 1);
        v[2 * f + 1] = -(float)(first + f + 1);
    }
}

static void test_waits_for_margin_then_plays_in_order() {
    Ring r;
    std::vector<float> w, out(480 * 2);
    ULONGLONG written = 0;
    // silence until a read can be served with the margin left behind it
    int silent = 0;
    for (;;) {
        ramp(w, written, 480); r.push(w.data(), 480); written += 480;
        r.pull(out.data(), 480);
        if (out[0] != 0.0f) break;
        for (float x : out) CHECK(x == 0.0f);
        silent++;
        CHECK(silent < 10);
        if (silent >= 10) return;
    }
    CHECK(written == CABLE_RING_MARGIN_FRAMES + 480);
    // it starts at the first frame ever written: nothing is skipped
    CHECK(out[0] == 1.0f && out[1] == -1.0f);
    CHECK(out[2 * 479] == 480.0f);
    // steady state: writer and reader in step, every frame once, in order,
    // and the margin is always what is left after a read
    float expect = 481.0f;
    for (int k = 0; k < 1000; k++) {
        ramp(w, written, 480); r.push(w.data(), 480); written += 480;
        r.pull(out.data(), 480);
        for (ULONG f = 0; f < 480; f++) {
            CHECK(out[2 * f] == expect);
            CHECK(out[2 * f + 1] == -expect);
            expect += 1.0f;
        }
        CHECK(r.c.Write - r.c.Read == CABLE_RING_MARGIN_FRAMES);
    }
}

static void test_jitter_within_margin_never_drops_or_repeats() {
    // Both halves move with the same clock (48 frames per ms, from the
    // performance counter) but their timers fire at different moments:
    // each 10 ms tick runs up to 4 ms late, independently for writer and
    // reader, in either order. Like UpdatePosition, each moves by whole
    // elapsed milliseconds. No frame may be lost, repeated or replaced by
    // silence once playing.
    Ring r;
    std::vector<float> w, out;
    ULONGLONG written = 0, read = 0;
    float expect = 1.0f;
    bool started = false;
    unsigned seed = 12345;
    auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 16) % 5; };
    for (int k = 1; k < 20000; k++) {
        ULONG tw = 10 * k + rnd();          // ms at which the writer runs
        ULONG tr = 10 * k + rnd();          // ms at which the reader runs
        auto doWrite = [&]() {
            ULONG n = (ULONG)(48ull * tw - written);
            ramp(w, written, n); r.push(w.data(), n); written += n;
        };
        auto doRead = [&]() {
            ULONG n = (ULONG)(48ull * tr - read);
            read += n;
            out.assign(n * 2, 0.0f);
            r.pull(out.data(), n);
            for (ULONG f = 0; f < n; f++) {
                if (!started) {
                    if (out[2 * f] == 0.0f) continue;
                    started = true;
                    CHECK(out[2 * f] == 1.0f);
                }
                if (out[2 * f] != expect) { std::printf("tick %d frame %u got %.0f want %.0f\n", k, (unsigned)f, out[2 * f], expect); CHECK(out[2 * f] == expect); return; }
                expect += 1.0f;
            }
        };
        if (tr <= tw) { doRead(); doWrite(); } else { doWrite(); doRead(); }
        if (failures) return;
    }
    CHECK(started);
    CHECK(expect > 1.0e6f);
}

static void test_writer_stops_reader_runs_dry_then_rewaits() {
    Ring r;
    std::vector<float> w, out(480 * 2);
    ULONGLONG written = 0;
    for (int k = 0; k < 10; k++) {
        ramp(w, written, 480); r.push(w.data(), 480); written += 480;
        r.pull(out.data(), 480);
    }
    CHECK(r.c.Primed);
    // writer stops: the margin (two packets) plays out, then silence
    r.pull(out.data(), 480);
    CHECK(out[0] != 0.0f && out[2 * 479] != 0.0f);
    r.pull(out.data(), 480);
    CHECK(out[0] != 0.0f && out[2 * 479] == (float)written);
    r.pull(out.data(), 480);
    for (float x : out) CHECK(x == 0.0f);
    CHECK(!r.c.Primed);
    // writer resumes: silence until the margin is back, then the new audio
    // from its first frame, nothing stale
    float first = (float)(written + 1);
    for (int k = 0; k < 2; k++) {
        ramp(w, written, 480); r.push(w.data(), 480); written += 480;
        r.pull(out.data(), 480);
        for (float x : out) CHECK(x == 0.0f);
    }
    ramp(w, written, 480); r.push(w.data(), 480); written += 480;
    r.pull(out.data(), 480);
    CHECK(out[0] == first);
}

static void test_late_reader_skips_ahead() {
    Ring r;
    std::vector<float> w, out(480 * 2);
    ULONGLONG written = 0;
    // the capture half isn't running: the writer goes on for 2 s
    for (int k = 0; k < 200; k++) {
        ramp(w, written, 480); r.push(w.data(), 480); written += 480;
    }
    // a reader that starts now plays recent audio, not 2 s old audio
    r.pull(out.data(), 480);
    CHECK(out[0] == (float)(written - CABLE_RING_MARGIN_FRAMES - 480 + 1));
    CHECK(r.c.Write - r.c.Read == CABLE_RING_MARGIN_FRAMES);
    // a reset reader waits for fresh audio
    CableCursorResetReader(&r.c);
    r.pull(out.data(), 480);
    for (float x : out) CHECK(x == 0.0f);
    // a reader held up mid-stream catches up the same way
    for (int k = 0; k < 20; k++) {
        ramp(w, written, 480); r.push(w.data(), 480); written += 480;
    }
    r.pull(out.data(), 480);
    CHECK(out[0] == (float)(written - CABLE_RING_MARGIN_FRAMES - 480 + 1));
}

static void test_huge_write_keeps_newest() {
    Ring r;
    std::vector<float> w;
    ULONG n = CABLE_RING_FRAMES + 1000;
    ramp(w, 0, n);
    r.push(w.data(), n);
    CHECK(r.c.Write == n);
    std::vector<float> out(480 * 2);
    r.pull(out.data(), 480);
    CHECK(out[0] == (float)(n - CABLE_RING_MARGIN_FRAMES - 480 + 1));
}

static void test_formats_round_trip() {
    const ULONG fmts[] = {CABLE_FMT_PCM16, CABLE_FMT_PCM24, CABLE_FMT_FLOAT32};
    const float tol[] = {2.0f / 32767.0f, 2.0f / 8388607.0f, 0.0f};
    CHECK(CableFormatFrameBytes(CABLE_FMT_PCM16) == 4);
    CHECK(CableFormatFrameBytes(CABLE_FMT_PCM24) == 6);
    CHECK(CableFormatFrameBytes(CABLE_FMT_FLOAT32) == 8);
    const ULONG frames = 1000;
    std::vector<float> src(frames * 2), back(frames * 2);
    for (ULONG i = 0; i < frames * 2; i++) src[i] = std::sin(i * 0.0123f) * 0.999f;
    for (int k = 0; k < 3; k++) {
        std::vector<UCHAR> bytes(frames * CableFormatFrameBytes(fmts[k]));
        CableEncode(src.data(), fmts[k], bytes.data(), frames);
        CableDecode(bytes.data(), fmts[k], back.data(), frames);
        for (ULONG i = 0; i < frames * 2; i++) CHECK(std::fabs(back[i] - src[i]) <= tol[k]);
    }
    // integer formats: exact full-scale codes, clipping, little-endian
    float edge[4] = {1.0f, -1.0f, 2.0f, -3.0f};
    UCHAR b16[8], b24[12];
    CableEncode(edge, CABLE_FMT_PCM16, b16, 2);
    CHECK(b16[0] == 0xFF && b16[1] == 0x7F);    // +32767
    CHECK(b16[2] == 0x01 && b16[3] == 0x80);    // -32767
    CHECK(b16[4] == 0xFF && b16[5] == 0x7F);    // clipped
    CHECK(b16[6] == 0x01 && b16[7] == 0x80);
    CableEncode(edge, CABLE_FMT_PCM24, b24, 2);
    CHECK(b24[0] == 0xFF && b24[1] == 0xFF && b24[2] == 0x7F);
    CHECK(b24[3] == 0x01 && b24[4] == 0x00 && b24[5] == 0x80);
    // decoding the most negative codes
    UCHAR m16[4] = {0x00, 0x80, 0x00, 0x00};
    UCHAR m24[6] = {0x00, 0x00, 0x80, 0x00, 0x00, 0x00};
    float d[2];
    CableDecode(m16, CABLE_FMT_PCM16, d, 1);
    CHECK(d[0] == -1.0f && d[1] == 0.0f);
    CableDecode(m24, CABLE_FMT_PCM24, d, 1);
    CHECK(d[0] == -1.0f && d[1] == 0.0f);
    // NaN and infinity from an app become silence in every format
    float bad[2] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
    UCHAR bf[8];
    CableEncode(bad, CABLE_FMT_FLOAT32, bf, 1);
    CableDecode(bf, CABLE_FMT_FLOAT32, d, 1);
    CHECK(d[0] == 0.0f && d[1] == 0.0f);
    std::memcpy(bf, bad, sizeof(bad));
    CableDecode(bf, CABLE_FMT_FLOAT32, d, 1);
    CHECK(d[0] == 0.0f && d[1] == 0.0f);
    CableEncode(bad, CABLE_FMT_PCM16, b16, 1);
    CHECK(b16[0] == 0 && b16[1] == 0 && b16[2] == 0 && b16[3] == 0);
}

int main() {
    test_waits_for_margin_then_plays_in_order();
    test_jitter_within_margin_never_drops_or_repeats();
    test_writer_stops_reader_runs_dry_then_rewaits();
    test_late_reader_skips_ahead();
    test_huge_write_keeps_newest();
    test_formats_round_trip();
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("cablerule: all checks passed\n");
    return 0;
}
