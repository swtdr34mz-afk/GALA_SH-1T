/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
/* The engine's tests, without JUCE: every factory patch at three rates (sound, no NaN, no overload), every wave and
 * variation through every filter mode and slope, SYNC and RING, every effect, the TRANCE GATE (its steps open and
 * close where they should), the ARPEGGIO's 64 variations, the PHRASE RECORDER (a phrase recorded and played back),
 * the D BEAM, 64 voices, any block size; and how fast. */
#include "Engine.h"
#include "Presets.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using namespace gl;

static int fails = 0;
#define CHECK(c, ...)                                                                                                  \
    do {                                                                                                               \
        if (!(c)) {                                                                                                    \
            std::printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                           \
            std::printf(__VA_ARGS__);                                                                                  \
            std::printf("\n");                                                                                         \
            fails++;                                                                                                   \
        }                                                                                                              \
    } while (0)

struct Stats {
    float peak = 0.0f, rms = 0.0f;
    bool finite = true;
};

static Stats run(Engine &e, int samples, int block = 256)
{
    std::vector<float> L((size_t)block), R((size_t)block);
    Stats s;
    double acc = 0.0;
    long n = 0;
    for (int done = 0; done < samples; done += block) {
        int b = std::min(block, samples - done);
        e.render(L.data(), R.data(), b);
        for (int i = 0; i < b; i++) {
            if (!std::isfinite(L[(size_t)i]) || !std::isfinite(R[(size_t)i]))
                s.finite = false;
            s.peak = std::max(s.peak, std::max(std::fabs(L[(size_t)i]), std::fabs(R[(size_t)i])));
            acc += (double)L[(size_t)i] * L[(size_t)i] + (double)R[(size_t)i] * R[(size_t)i];
            n += 2;
        }
    }
    s.rms = (float)std::sqrt(acc / (double)std::max(1L, n));
    return s;
}

int main()
{
    auto E = std::make_unique<Engine>();
    Engine &e = *E;
    CHECK(!factoryErrors(), "a factory patch names a parameter that is not there");

    /* every factory patch: a chord, held, let go */
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        e.prepare(rate);
        for (int k = 0; k < NFACTORY; k++) {
            e.panic();
            e.setPatch(factory(k));
            e.host(false, 0.0, 120.0, false);
            run(e, 2000);
            e.noteOn(60, 0.8f);
            e.noteOn(64, 0.8f);
            e.noteOn(67, 0.8f);
            Stats a = run(e, (int)(rate * 1.2));
            e.noteOff(60);
            e.noteOff(64);
            e.noteOff(67);
            Stats b = run(e, (int)(rate * 0.5));
            CHECK(a.finite && b.finite, "%s at %.0f: not finite", factory(k).name, rate);
            CHECK(a.rms > 0.004f, "%s at %.0f: silent (rms %.5f)", factory(k).name, rate, a.rms);
            CHECK(a.peak < 2.5f, "%s at %.0f: too loud (peak %.2f)", factory(k).name, rate, a.peak);
            if (rate == 48000.0)
                std::printf("  %c%d %-18s peak %.2f rms %.3f\n", 'A' + k / 8, k % 8 + 1, factory(k).name, a.peak, a.rms);
        }
    }

    /* every wave, variation, filter mode and slope */
    e.prepare(48000.0);
    for (int w = 0; w < NWAVE; w++)
        for (int var = 0; var < 3; var++)
            for (int m = 0; m < NFMODE; m++)
                for (int sl = 0; sl < 2; sl++) {
                    Patch p;
                    patchInit(p);
                    p.tone[0][T_WAVE] = (int16_t)w;
                    p.tone[0][T_VAR] = (int16_t)var;
                    p.tone[0][T_FMODE] = (int16_t)m;
                    p.tone[0][T_SLOPE] = (int16_t)sl;
                    p.tone[0][T_CUTOFF] = 80;
                    p.tone[0][T_RESO] = 127;
                    p.tone[0][T_PW] = 64;
                    p.tone[0][T_PWM] = 64;
                    p.tone[0][T_LFO_AMP] = 40;
                    e.panic();
                    e.setPatch(p);
                    e.noteOn(48, 1.0f);
                    e.noteOn(84, 1.0f);
                    Stats s = run(e, 24000);
                    e.allOff();
                    CHECK(s.finite && s.peak < 3.0f && s.rms > 1e-4f, "wave %d var %d mode %d slope %d: peak %.2f rms %.5f",
                          w, var, m, sl, s.peak, s.rms);
                }

    /* SYNC and RING */
    for (int mm = 1; mm <= 2; mm++) {
        Patch p;
        patchInit(p);
        p.com[C_MOD] = (int16_t)mm;
        p.tone[0][T_PITCH] = 7;
        for (int w : {W_SAW, W_SIN, W_SUPERSAW}) {
            p.tone[1][T_WAVE] = (int16_t)w;
            e.panic();
            e.setPatch(p);
            e.noteOn(60, 1.0f);
            Stats s = run(e, 24000);
            e.allOff();
            CHECK(s.finite && s.rms > 0.003f && s.peak < 2.5f, "mod %d wave %d: peak %.2f rms %.4f", mm, w, s.peak, s.rms);
        }
    }

    /* every effect, its parameters at their ends */
    for (int d = 0; d < 4; d++)
        for (int mf = 0; mf < 4; mf++)
            for (int dl = 0; dl < 3; dl++)
                for (int ext : {0, 127}) {
                    Patch p;
                    patchInit(p);
                    p.com[C_DIST] = (int16_t)d;
                    p.com[C_MODFX] = (int16_t)mf;
                    p.com[C_DELAY] = (int16_t)dl;
                    p.com[C_REVERB] = 1;
                    p.com[C_LOWBOOST] = 1;
                    for (int c = C_DIST_DRIVE; c <= C_LB_AMOUNT; c++)
                        if (COMMON[c].kind == K_NUM || COMMON[c].kind == K_DLTIME)
                            p.com[c] = (int16_t)(ext ? COMMON[c].hi : COMMON[c].lo);
                    p.com[C_RV_TYPE] = (int16_t)(ext ? 2 : 0);
                    e.panic();
                    e.setPatch(p);
                    e.noteOn(57, 1.0f);
                    Stats s = run(e, 30000);
                    e.allOff();
                    Stats t = run(e, 30000);
                    CHECK(s.finite && t.finite && s.peak < (ext ? 9.0f : 3.0f), "fx %d %d %d (%d): peak %.2f", d, mf, dl, ext, s.peak);
                }

    /* the TRANCE GATE: CLASSIC 16THS at 120: open at a step's start, shut at its end */
    {
        Patch p;
        patchInit(p);
        p.tone[0][T_WAVE] = W_SAW;
        p.com[C_GATE_ON] = 1;
        p.com[C_GATE_PAT] = 2;                    /* OFFBEAT 8THS: .X.X.X.X */
        p.com[C_GATE_RATE] = GR_8;
        p.com[C_GATE_ATT] = 0;
        p.com[C_GATE_REL] = 0;
        p.com[C_TEMPO] = 120;
        e.prepare(48000.0);
        e.setPatch(p);
        e.host(false, 0.0, 120.0, false);
        e.noteOn(60, 1.0f);
        /* an eighth at 120: 12000 samples; the first (a rest) quiet, the second loud */
        Stats rest = run(e, 11000, 100);
        run(e, 1500, 100);
        Stats hit = run(e, 9000, 100);
        CHECK(rest.rms < hit.rms * 0.1f, "gate: rest %.4f against hit %.4f", rest.rms, hit.rms);
        e.allOff();
        for (int k = 0; k < NGATEPAT; k++) {
            uint8_t st[GATE_STEPS];
            int n = gatePatternSteps(k, st);
            CHECK(n >= 1 && n <= GATE_STEPS, "gate pattern %d: %d steps", k, n);
        }
    }

    /* the ARPEGGIO: every variation plays notes */
    for (int k = 0; k < NARP; k++) {
        Patch p;
        patchInit(p);
        p.tone[0][T_AENV_R] = 10;
        p.com[C_ARP_ON] = 1;
        p.com[C_ARP_VAR] = (int16_t)k;
        e.panic();
        e.setPatch(p);
        e.host(false, 0.0, 140.0, false);
        e.noteOn(60, 0.9f);
        e.noteOn(63, 0.9f);
        e.noteOn(67, 0.9f);
        Stats s = run(e, 48000);
        e.allOff();
        CHECK(s.finite && s.rms > 0.003f, "arp %d (%s): rms %.4f", k, ARP[k].name, s.rms);
        CHECK(ARP[k].len >= 1 && ARP[k].len <= 16 && (int)std::strlen(ARP[k].rhythm) >= ARP[k].len, "arp %d's rhythm", k);
    }

    /* the PHRASE RECORDER: two bars recorded after the count-in, played back */
    {
        Patch p;
        patchInit(p);
        p.com[C_TEMPO] = 120;
        e.prepare(48000.0);
        e.setPatch(p);
        e.host(false, 0.0, 120.0, false);
        e.phraseRec();
        CHECK(e.phraseState() == PH_STANDBY, "phrase: standby");
        e.phraseLength(2);
        e.phrasePlay();
        CHECK(e.phraseState() == PH_COUNTIN, "phrase: count-in");
        run(e, 96000 + 480);                      /* a bar at 120: 2 s */
        CHECK(e.phraseState() == PH_RECORDING, "phrase: recording (%d)", e.phraseState());
        e.noteOn(60, 0.8f);
        run(e, 12000);
        e.noteOff(60);
        e.setParam(toneIdx(0, T_CUTOFF), 40);
        run(e, 12000);
        e.noteOn(67, 0.8f);
        run(e, 12000);
        e.noteOff(67);
        CHECK(e.phrase(0).n == 5, "phrase: %d events", e.phrase(0).n);
        e.phrasePlay();                            /* stop */
        CHECK(e.phraseState() == PH_IDLE, "phrase: stopped");
        e.setParam(toneIdx(0, T_CUTOFF), 127);
        e.phrasePlay();                            /* play it back */
        Stats s = run(e, 24000);
        CHECK(s.rms > 0.003f, "phrase: playback silent");
        int idx = -1, v = -1, got = 0;
        while (e.pollPlayedParam(idx, v))
            got++;
        CHECK(got == 1 && idx == toneIdx(0, T_CUTOFF) && v == 40, "phrase: the knob played back (%d: %d = %d)", got, idx, v);
        run(e, 200000);                            /* round the loop a few times */
        e.phrasePlay();
        Stats after = run(e, 48000);
        CHECK(after.rms < 0.01f, "phrase: notes left on after stop (%.4f)", after.rms);
    }

    /* CIRCUIT MOD: the voices drift apart, nothing breaks */
    {
        Patch p = factory(8);
        p.com[C_CMOD] = 1;
        p.com[C_CMOD_AGE] = 127;
        p.com[C_CMOD_DRIFT] = 127;
        p.com[C_CMOD_NOISE] = 127;
        e.prepare(48000.0);
        e.setPatch(p);
        for (int n : {48, 52, 55, 60, 64, 67})
            e.noteOn(n, 0.8f);
        Stats s = run(e, 96000);
        e.allOff();
        CHECK(s.finite && s.rms > 0.01f && s.peak < 2.5f, "circuit mod: peak %.2f rms %.4f", s.peak, s.rms);
    }

    /* the D BEAM: VOLUME, ASSIGN */
    {
        Patch p;
        patchInit(p);
        p.tone[0][T_CUTOFF] = 30;
        p.com[C_DB_MODE] = DB_ASSIGN;
        p.com[C_DB_ASSIGN] = 12;                  /* cutoff */
        e.panic();
        e.setPatch(p);
        e.noteOn(48, 1.0f);
        Stats dark = run(e, 12000);
        e.dbeam(true, 1.0f);
        run(e, 4000);
        Stats bright = run(e, 12000);
        e.dbeam(false, 0.0f);
        e.allOff();
        CHECK(bright.rms > dark.rms * 1.5f, "d beam: cutoff %.4f -> %.4f", dark.rms, bright.rms);
    }

    /* 64 voices, odd block sizes, and the time it takes */
    {
        e.prepare(48000.0);
        e.setVoices(64);
        e.setPatch(factory(8));                    /* SUPERSAW ANTHEM: three super saws */
        for (int n = 0; n < 64; n++)
            e.noteOn(36 + n, 0.7f);
        auto t0 = std::chrono::steady_clock::now();
        Stats s = run(e, 48000, 37);
        auto t1 = std::chrono::steady_clock::now();
        double sec = std::chrono::duration<double>(t1 - t0).count();
        CHECK(s.finite, "64 voices: not finite");
        std::printf("64 voices of three super saws: %.2f x real time (one core)\n", sec);
        e.allOff();
        e.setVoices(16);
        e.setPatch(factory(0));
        for (int n = 0; n < 8; n++)
            e.noteOn(48 + n * 3, 0.7f);
        t0 = std::chrono::steady_clock::now();
        run(e, 48000, 512);
        t1 = std::chrono::steady_clock::now();
        std::printf("8 notes of GAIA TRANCE LEAD: %.3f x real time\n", std::chrono::duration<double>(t1 - t0).count());
    }

    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
