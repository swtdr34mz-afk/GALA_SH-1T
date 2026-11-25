/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
/* The GAIA's five effect blocks, in its order (manual pp. 38-39, 49-50), each its own parameters:
 *
 *   DIST       DIST (a guitar amp: drive, six types, presence, level), FUZZ (the same, a fuzz's), BIT CRASH (sample
 *              rate, bit down, filter, level). Mono, as the GAIA's.
 *   FLANGER    FLANGER (feedback, depth, rate, level), PHASER (resonance, depth, rate, level), PITCH SHIFTER (a voice
 *              of -12..+12 semitones and a detune, a second voice of -12..+12, level)
 *   DELAY      DELAY (stereo) or PANNING DELAY (left, right, left ...): time (or a note value: TEMPO SYNC), feedback,
 *              high damp, level
 *   REVERB     ROOM, PLATE or HALL: time, high damp, level
 *   LOW BOOST  a low shelf (GALA: its amount)
 *
 * Everything glides (no zipper, no click); a block switched or changed crossfades. */
#pragma once
#include "Dsp.h"
#include "Params.h"
#include <vector>

namespace gl
{
/* a parameter gliding toward its target (20 ms) */
struct Glide {
    float v = 0.0f, t = 0.0f;
    bool init = false;
    void set(float x)
    {
        t = x;
        if (!init) {
            v = x;
            init = true;
        }
    }
    inline float step(float k) { return v += (t - v) * k; }
};

/* a block's on/off (or its type): out, then the new one in */
struct Xfade {
    int want = 0, cur = 0;
    float x = 0.0f;
    bool init = false;
    void set(int w)
    {
        want = w;
        if (!init) {
            cur = w;
            x = w ? 1.0f : 0.0f;
            init = true;
        }
    }
    /* steps it a sample: true when the type changed (its state wants clearing) */
    inline bool step(float k)
    {
        if (want != cur) {
            x -= k;
            if (x <= 0.0f) {
                x = 0.0f;
                cur = want;
                return true;
            }
        } else if (cur != 0 && x < 1.0f) {
            x = minf(1.0f, x + k);
        } else if (cur == 0) {
            x = 0.0f;
        }
        return false;
    }
};

class FxChain
{
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        dlN = pow2((int)(sr * 1.4f) + 8);
        for (auto &b : dl)
            b.assign((size_t)dlN, 0.0f);
        mdN = pow2((int)(sr * 0.06f));
        for (auto &b : md)
            b.assign((size_t)mdN, 0.0f);
        for (int i = 0; i < 8; i++)
            rvLine[i].assign((size_t)((int)(RV_LEN[i] * sr / 44100.0f * 2.1f) + 4), 0.0f);
        for (int i = 0; i < 4; i++)
            rvAp[i].assign((size_t)((int)(RV_AP[i] * sr / 44100.0f) + 2), 0.0f);
        pdN = pow2((int)(sr * 0.06f));
        pre.assign((size_t)pdN, 0.0f);
        clear();
    }
    void clear()
    {
        for (auto &b : dl)
            std::fill(b.begin(), b.end(), 0.0f);
        for (auto &b : md)
            std::fill(b.begin(), b.end(), 0.0f);
        for (auto &b : rvLine)
            std::fill(b.begin(), b.end(), 0.0f);
        for (auto &b : rvAp)
            std::fill(b.begin(), b.end(), 0.0f);
        std::fill(pre.begin(), pre.end(), 0.0f);
        std::memset(rvW, 0, sizeof rvW);
        std::memset(rvApW, 0, sizeof rvApW);
        std::memset(rvLp, 0, sizeof rvLp);
        clearDist();
        clearMod();
        std::memset(dlLp, 0, sizeof dlLp);
        dlW = preW = 0;
        lbS[0] = lbS[1] = 0.0f;
    }

    /* the patch's effect parameters (a block's start), the tempo */
    void set(const int16_t *C, float bpm_, float dbDist, float dbMod, float dbDelay, float dbReverb)
    {
        bpm = bpm_ < 5.0f ? 120.0f : bpm_;
        on = C[C_FX_ON] != 0;
        dist.set(on ? C[C_DIST] : 0);
        mod.set(on ? C[C_MODFX] : 0);
        delay.set(on ? C[C_DELAY] : 0);
        rev.set(on && C[C_REVERB] ? 1 : 0);
        lb.set(on && C[C_LOWBOOST] ? 1 : 0);
        /* the active types' parameters */
        int dt = dist.want ? dist.want : dist.cur;
        int base = dt == DT_FUZZ ? C_FUZZ_DRIVE : dt == DT_BITCRASH ? C_BC_RATE : C_DIST_DRIVE;
        dP[0].set(clampf(C[base] + dbDist, 0.0f, 127.0f));
        dP[1].set(C[base + 1]);
        dP[2].set(C[base + 2]);
        dP[3].set(C[base + 3]);
        dType = dt;
        int mt = mod.want ? mod.want : mod.cur;
        int mb = mt == MF_PHASER ? C_PH_RESO : mt == MF_PITCH ? C_PS_PITCH : C_FL_FB;
        mP[0].set(C[mb]);
        mP[1].set(C[mb + 1]);
        mP[2].set(C[mb + 2]);
        mP[3].set(clampf(C[mb + 3] + dbMod, 0.0f, 127.0f));
        mType = mt;
        int et = delay.want ? delay.want : delay.cur;
        int eb = et == DL_PANNING ? C_PD_TIME : C_DL_TIME;
        float tsec;
        if (C[C_DELAY_SYNC]) {
            int k = C[eb] * 13 / 128;
            tsec = DLY_BEATS[k < 0 ? 0 : k > 12 ? 12 : k] * 60.0f / bpm;
            while (tsec > 1.3f)
                tsec *= 0.5f;
        } else {
            float v = (float)C[eb];
            tsec = 0.001f + 1.299f * v * v / (127.0f * 127.0f);
        }
        eTime.set(tsec);
        eP[0].set(C[eb + 1]);
        eP[1].set(C[eb + 2]);
        eP[2].set(clampf(C[eb + 3] + dbDelay, 0.0f, 127.0f));
        rTime.set(C[C_RV_TIME]);
        rDamp.set(C[C_RV_DAMP]);
        rLevel.set(clampf(C[C_RV_LEVEL] + dbReverb, 0.0f, 127.0f));
        rType = C[C_RV_TYPE];
        lbAmt.set(C[C_LB_AMOUNT]);
    }

    void process(float *L, float *R, int n)
    {
        const float isr = 1.0f / sr;
        const float kp = 1.0f - std::exp(-1.0f / (0.02f * sr));
        const float kx = 1.0f / (0.015f * sr);
        for (int i = 0; i < n; i++) {
            float l = L[i], r = R[i];
            /* DIST */
            if (dist.step(kx))
                clearDist();
            for (auto &p : dP)
                p.step(kp);
            if (dist.x > 0.0f) {
                float y = runDist(0.5f * (l + r), isr);
                l += (y - l) * dist.x;
                r += (y - r) * dist.x;
            }
            /* FLANGER / PHASER / PITCH SHIFTER */
            if (mod.step(kx))
                clearMod();
            for (auto &p : mP)
                p.step(kp);
            if (mod.x > 0.0f) {
                float yl, yr;
                runMod(l, r, yl, yr, isr);
                l += (yl - l) * mod.x;
                r += (yr - r) * mod.x;
            }
            /* DELAY / PANNING DELAY */
            if (delay.step(kx)) {
                for (auto &b : dl)
                    std::fill(b.begin(), b.end(), 0.0f);
                dlLp[0] = dlLp[1] = 0.0f;
            }
            float t = eTime.step(0.00025f * 44100.0f * isr);   /* (the time bends, as a tape's: no click) */
            for (auto &p : eP)
                p.step(kp);
            if (delay.x > 0.0f || delay.cur != 0) {
                float fb = minf(eP[0].v / 127.0f * 0.95f, 0.95f);
                float fc = 18000.0f * std::exp2(eP[1].v / 36.0f * 5.2f);   /* -36 dB: ~500 Hz */
                float kd = 1.0f - std::exp(-2.0f * PI * minf(fc, 0.45f * sr) * isr);
                float d = clampf(t * sr, 2.0f, (float)dlN - 4.0f);
                float a = rd(dl[0], dlN, dlW, d), b = rd(dl[1], dlN, dlW, d);
                dlLp[0] += (a - dlLp[0]) * kd;
                dlLp[1] += (b - dlLp[1]) * kd;
                if (delay.cur == DL_PANNING) {              /* left, then right, then left ... */
                    dl[0][(size_t)dlW] = 0.5f * (l + r) + softT(dlLp[1] * fb);
                    dl[1][(size_t)dlW] = softT(dlLp[0] * fb);
                } else {
                    dl[0][(size_t)dlW] = l + softT(dlLp[0] * fb);
                    dl[1][(size_t)dlW] = r + softT(dlLp[1] * fb);
                }
                dlW = (dlW + 1) & (dlN - 1);
                float g = eP[2].v / 127.0f * delay.x;
                l += a * g;
                r += b * g;
            }
            /* REVERB */
            rev.step(kx);
            rTime.step(kp);
            rDamp.step(kp);
            rLevel.step(kp);
            if (rev.x > 0.0f || tail > 1e-6f) {
                float yl, yr;
                runReverb(rev.x > 0.0f ? l : 0.0f, rev.x > 0.0f ? r : 0.0f, yl, yr, isr);
                float g = rLevel.v / 127.0f * 0.9f * (rev.cur ? rev.x : 0.0f);
                if (!rev.cur && rev.x <= 0.0f)
                    g = 0.0f;
                l += yl * g;
                r += yr * g;
                float a = std::fabs(yl) + std::fabs(yr);
                tail = a > tail ? a : tail * 0.9999f;
            }
            /* LOW BOOST: a low shelf at 90 Hz */
            lb.step(kx);
            lbAmt.step(kp);
            if (lb.x > 0.0f) {
                float k = 1.0f - std::exp(-2.0f * PI * 90.0f * isr);
                float gdb = 3.0f + lbAmt.v / 127.0f * 7.0f;
                float gl = (std::pow(10.0f, gdb / 20.0f) - 1.0f) * lb.x;
                lbS[0] += (l - lbS[0]) * k;
                lbS[1] += (r - lbS[1]) * k;
                l += lbS[0] * gl;
                r += lbS[1] * gl;
                float comp = 1.0f / (1.0f + 0.15f * gl);
                l *= comp;
                r *= comp;
            }
            L[i] = l;
            R[i] = r;
        }
        for (int c = 0; c < 2; c++) {
            dlLp[c] = flushDn(dlLp[c]);
            lbS[c] = flushDn(lbS[c]);
            dsHp[c] = flushDn(dsHp[c]);
            dsLp[c] = flushDn(dsLp[c]);
            for (int k = 0; k < 8; k++)
                ph8[c][k] = flushDn(ph8[c][k]);
        }
        for (float &v : rvLp)
            v = flushDn(v);
    }

private:
    static constexpr float RV_LEN[8] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
    static constexpr float RV_AP[4] = {142, 107, 379, 277};
    static constexpr float DLY_BEATS[13] = {0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f,
                                            0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 4.0f};
    static int pow2(int n)
    {
        int p = 1;
        while (p < n)
            p <<= 1;
        return p;
    }

    float sr = 44100.0f, bpm = 120.0f, tail = 0.0f;
    bool on = true;
    Xfade dist, mod, delay, rev, lb;
    Glide dP[4], mP[4], eP[3], eTime, rTime, rDamp, rLevel, lbAmt;
    int dType = DT_DIST, mType = MF_FLANGER, rType = RT_HALL;
    std::vector<float> dl[2], md[2], rvLine[8], rvAp[4], pre;
    int dlN = 1, dlW = 0, mdN = 1, mdW = 0, pdN = 1, preW = 0;
    int rvW[8] = {}, rvApW[4] = {};
    float rvLp[8] = {}, rvMph[8] = {}, dlLp[2] = {}, lbS[2] = {};
    /* DIST's */
    float dsHp[2] = {}, dsLp[2] = {}, dsPre = 0.0f, bcHold = 0.0f, bcPh = 0.0f, bcLp[2] = {};
    /* FLANGER / PHASER / PITCH SHIFTER's */
    float lfoPh = 0.0f, fbS[2] = {}, ph8[2][8] = {}, psPh[2] = {};
    uint32_t seed = 0x13579BDFu;

    void clearDist()
    {
        std::memset(dsHp, 0, sizeof dsHp);
        std::memset(dsLp, 0, sizeof dsLp);
        std::memset(bcLp, 0, sizeof bcLp);
        dsPre = bcHold = bcPh = 0.0f;
    }
    void clearMod()
    {
        for (auto &b : md)
            std::fill(b.begin(), b.end(), 0.0f);
        std::memset(fbS, 0, sizeof fbS);
        std::memset(ph8, 0, sizeof ph8);
        psPh[0] = 0.0f;
        psPh[1] = 0.0f;
        mdW = 0;
    }
    inline float rd(const std::vector<float> &b, int N, int w, float d) const
    {
        float p = (float)w - d;
        while (p < 0.0f)
            p += (float)N;
        int i0 = (int)p;
        float f = p - (float)i0;
        float a = b[(size_t)(i0 & (N - 1))], c = b[(size_t)((i0 + 1) & (N - 1))];
        return a + (c - a) * f;
    }

    /* ---------------------------------------------------------------- DIST --- */
    float runDist(float x, float isr)
    {
        if (dType == DT_BITCRASH) {
            /* SAMPLE RATE: held samples (down to ~400 Hz); BIT DOWN: 24 .. 2 bits; FILTER: a low-pass after */
            float rate = sr * std::exp2(-dP[0].v / 127.0f * 6.8f);
            bcPh += rate * isr;
            if (bcPh >= 1.0f) {
                bcPh -= std::floor(bcPh);
                float bits = 24.0f - dP[1].v / 127.0f * 22.0f, q = std::exp2(bits - 1.0f);
                bcHold = std::floor(x * q + 0.5f) / q;
            }
            float fc = 19000.0f * std::exp2(-dP[2].v / 127.0f * 5.5f);
            float k = 1.0f - std::exp(-2.0f * PI * minf(fc, 0.45f * sr) * isr);
            bcLp[0] += (bcHold - bcLp[0]) * k;
            bcLp[1] += (bcLp[0] - bcLp[1]) * k;
            return bcLp[1] * dP[3].v / 100.0f;
        }
        int type = clampi((int)(dP[1].v + 0.5f), 1, 6);
        float drive = dP[0].v / 127.0f;
        /* a guitar amp's input: the lows thinned before the clipper */
        float khp = 1.0f - std::exp(-2.0f * PI * (dType == DT_FUZZ ? 60.0f : 110.0f) * isr);
        dsHp[0] += (x - dsHp[0]) * khp;
        float in = x - dsHp[0] * 0.85f, y;
        if (dType == DT_DIST) {
            float g = 1.5f + drive * drive * 90.0f;
            switch (type) {
            case 1: y = softT(in * g); break;                                  /* overdrive */
            case 2: y = clampf(in * g, -1.0f, 1.0f); break;                     /* hard */
            case 3: y = softT(in * g + 0.25f) - softT(0.25f); break;            /* a tube's lean */
            case 4: y = softT(softT(in * g * 0.6f) * 3.0f); break;              /* crunch, two stages */
            case 5: y = softT(clampf(in * g * 2.0f, -1.5f, 1.5f) * 1.6f); break; /* metal */
            default: {                                                         /* a warm drive */
                float a = in * g * 0.7f;
                y = a / (1.0f + std::fabs(a));
                y *= 1.4f;
            }
            }
        } else {                                                               /* FUZZ */
            float g = 4.0f + drive * drive * 160.0f, a = in * g;
            switch (type) {
            case 1: y = clampf(a, -1.0f, 1.0f); break;
            case 2: y = clampf(std::fabs(a) * 1.2f - 0.4f, -1.0f, 1.0f); break; /* octave up (rectified) */
            case 3: y = a > 0.0f ? clampf(a, 0.0f, 1.0f) : softT(a * 0.3f); break;   /* gated, lopsided */
            case 4: y = clampf(softT(a) * 1.8f + 0.2f * softT(a * a * 0.05f), -1.0f, 1.0f); break;
            case 5: y = clampf(a, -0.6f, 1.0f) / 0.8f; break;
            default: y = softT(std::fabs(a) - std::fabs(a * 0.5f) + a * 0.5f) ; break;
            }
        }
        /* PRESENCE: the cabinet's top */
        float fc = 1800.0f * std::exp2(dP[2].v / 127.0f * 3.4f);
        float k = 1.0f - std::exp(-2.0f * PI * minf(fc, 0.45f * sr) * isr);
        dsLp[0] += (y - dsLp[0]) * k;
        dsLp[1] += (dsLp[0] - dsLp[1]) * k;
        return dsLp[1] * dP[3].v / 127.0f * 0.6f;
    }
    static inline int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

    /* ------------------------------------------------- FLANGER / PHASER / PITCH --- */
    void runMod(float x0, float x1, float &yl, float &yr, float isr)
    {
        switch (mType) {
        case MF_PHASER: {
            float rate = 0.03f + std::pow(mP[2].v / 127.0f, 2.0f) * 8.0f, depth = mP[1].v / 127.0f;
            float fb = mP[0].v / 127.0f * 0.85f;
            lfoPh += rate * isr;
            if (lfoPh >= 1.0f)
                lfoPh -= 1.0f;
            for (int c = 0; c < 2; c++) {
                float m = 0.5f + 0.5f * std::sin(2.0f * PI * (lfoPh + (c ? 0.18f : 0.0f)));
                float fc = 150.0f * std::exp2(depth * 5.0f * m + (1.0f - depth) * 2.2f);
                float t = tanW(clampf(PI * fc * isr, 0.0f, 1.2f)), a = (t - 1.0f) / (t + 1.0f);
                float x = (c ? x1 : x0) + fbS[c] * fb;
                for (int k = 0; k < 8; k++) {
                    float y = a * x + ph8[c][k];
                    ph8[c][k] = x - a * y;
                    x = y;
                }
                fbS[c] = softT(x);
                float lv = mP[3].v / 127.0f;
                float dry = c ? x1 : x0;
                (c ? yr : yl) = dry * (1.0f - 0.5f * lv) + x * 0.5f * lv;
            }
            break;
        }
        case MF_PITCH: {
            /* two voices of a crossfading two-tap shifter (40 ms window): PITCH + DETUNE, and PITCH 2 */
            static const float P2[11] = {-12, -7, -5, -2, -1, 0, 1, 2, 5, 7, 12};
            float semis[2] = {mP[0].v + mP[1].v / 100.0f, P2[clampi((int)(mP[2].v + 0.5f), 0, 10)]};
            float in = 0.5f * (x0 + x1);
            md[0][(size_t)mdW] = in;
            const float win = 0.04f * sr;
            float out[2] = {0.0f, 0.0f};
            for (int v = 0; v < 2; v++) {
                if (v == 1 && std::fabs(semis[1]) < 0.01f)
                    continue;                                /* (a second voice at 0: none) */
                float ratio = std::exp2(semis[v] / 12.0f);
                psPh[v] += (1.0f - ratio) / win;
                psPh[v] -= std::floor(psPh[v]);
                for (int tap = 0; tap < 2; tap++) {
                    float p = psPh[v] + 0.5f * (float)tap;
                    p -= std::floor(p);
                    float w = std::sin(PI * p);
                    out[v] += rd(md[0], mdN, mdW, 2.0f + p * win) * w * w;
                }
            }
            mdW = (mdW + 1) & (mdN - 1);
            float lv = mP[3].v / 127.0f;
            yl = x0 + (out[0] * 0.9f + out[1] * 0.6f) * lv;
            yr = x1 + (out[0] * 0.6f + out[1] * 0.9f) * lv;
            float comp = 1.0f / (1.0f + 0.5f * lv);
            yl *= comp;
            yr *= comp;
            break;
        }
        default: {                                       /* FLANGER */
            float rate = 0.02f + std::pow(mP[2].v / 127.0f, 2.0f) * 6.0f, depth = mP[1].v / 127.0f;
            float fb = mP[0].v / 127.0f * 0.92f;
            lfoPh += rate * isr;
            if (lfoPh >= 1.0f)
                lfoPh -= 1.0f;
            float ml = 0.5f + 0.5f * std::sin(2.0f * PI * lfoPh), mr = 0.5f + 0.5f * std::sin(2.0f * PI * (lfoPh + 0.25f));
            md[0][(size_t)mdW] = x0 + fbS[0];
            md[1][(size_t)mdW] = x1 + fbS[1];
            float base = 0.0004f * sr, sw = (0.0003f + 0.0062f * depth) * sr;
            float a = rd(md[0], mdN, mdW, base + sw * ml), b = rd(md[1], mdN, mdW, base + sw * mr);
            fbS[0] = softT(a * fb);
            fbS[1] = softT(b * fb);
            mdW = (mdW + 1) & (mdN - 1);
            float lv = mP[3].v / 127.0f;
            yl = x0 * (1.0f - 0.45f * lv) + a * 0.55f * lv;
            yr = x1 * (1.0f - 0.45f * lv) + b * 0.55f * lv;
        }
        }
    }

    /* ---------------------------------------------------------------- REVERB --- */
    void runReverb(float x0, float x1, float &yl, float &yr, float isr)
    {
        /* eight lines with a Householder mix, each damped and gently moving; ROOM short and close, PLATE dense and
         * bright (no pre-delay), HALL large with a pre-delay */
        static const float SIZE[3] = {0.45f, 0.75f, 1.6f}, TMAX[3] = {2.5f, 6.0f, 12.0f}, PRE[3] = {0.006f, 0.0f, 0.028f};
        int ty = clampi(rType, 0, 2);
        float t60 = 0.15f + std::pow(rTime.v / 127.0f, 1.5f) * TMAX[ty];
        float damp = 20000.0f * std::exp2(-(rDamp.v - 1.0f) / 99.0f * 4.6f) * (ty == RT_PLATE ? 1.3f : 1.0f);
        float kd = 1.0f - std::exp(-2.0f * PI * minf(damp, 0.45f * sr) * isr);
        float in = 0.3f * (x0 + x1);
        pre[(size_t)preW] = in;
        in = rd(pre, pdN, preW, maxf(1.0f, PRE[ty] * sr));
        preW = (preW + 1) & (pdN - 1);
        float apg = ty == RT_PLATE ? 0.7f : 0.6f;
        for (int i = 0; i < 4; i++) {
            auto &b = rvAp[i];
            int N = (int)b.size(), w = rvApW[i];
            float z = b[(size_t)w], y = -apg * in + z;
            b[(size_t)w] = in + apg * y;
            rvApW[i] = (w + 1) % N;
            in = y;
        }
        float o[8], sum = 0.0f;
        for (int i = 0; i < 8; i++) {
            auto &b = rvLine[i];
            int N = (int)b.size();
            rvMph[i] += (0.25f + 0.07f * (float)i) * isr;
            if (rvMph[i] >= 1.0f)
                rvMph[i] -= 1.0f;
            float len = RV_LEN[i] * sr / 44100.0f * SIZE[ty];
            float d = minf(len + 5.0f * std::sin(2.0f * PI * rvMph[i]), (float)N - 3.0f);
            float p = (float)rvW[i] - d;
            while (p < 0.0f)
                p += (float)N;
            int i0 = (int)p;
            float f = p - (float)i0, a = b[(size_t)(i0 % N)], c = b[(size_t)((i0 + 1) % N)];
            float y = a + (c - a) * f;
            float g = std::pow(10.0f, -3.0f * (d / sr) / t60);
            rvLp[i] += (y - rvLp[i]) * kd;
            o[i] = rvLp[i] * g;
            sum += o[i];
        }
        sum *= 0.25f;
        for (int i = 0; i < 8; i++) {
            auto &b = rvLine[i];
            int N = (int)b.size();
            float v = o[i] - sum + ((i & 1) ? in : in * 0.9f);
            b[(size_t)rvW[i]] = v > 3.0f ? 3.0f : v < -3.0f ? -3.0f : v;
            rvW[i] = (rvW[i] + 1) % N;
        }
        yl = 0.6f * (o[0] + o[2] + o[4] + o[6]);
        yr = 0.6f * (o[1] + o[3] + o[5] + o[7]);
    }
};
} // namespace gl
