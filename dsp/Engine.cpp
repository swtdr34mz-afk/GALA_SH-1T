/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
#include "Engine.h"
#include <algorithm>
#include <vector>

namespace gl
{
/* ------------------------------------------------------------------ tables --- */
const float *blepTables()
{
    /* PROPHET 67's: a Kaiser-windowed sinc (cutoff 0.9 of Nyquist, beta 8) over 16 samples, integrated once (the
     * band-limited step) and its residual once more (the ramp's), 1024 points a sample. Made once, shared */
    static const std::vector<float> t = [] {
        const int H = BL_H, OS = BL_OS, N = BL_N;
        const double fc = 0.9, beta = 8.0, pi = 3.14159265358979323846;
        auto i0 = [](double x) {
            double s = 1, term = 1;
            for (int k = 1; k < 60; k++) {
                term *= (x / (2 * k)) * (x / (2 * k));
                s += term;
            }
            return s;
        };
        std::vector<double> h(N), B(N), R(N);
        for (int i = 0; i < N; i++) {
            double x = -H + (double)i / OS, r = x / H;
            double w = std::fabs(r) <= 1 ? i0(beta * std::sqrt(1 - r * r)) / i0(beta) : 0;
            double sc = x == 0 ? 1.0 : std::sin(pi * fc * x) / (pi * fc * x);
            h[i] = fc * sc * w;
        }
        double acc = 0;
        B[0] = 0;
        for (int i = 1; i < N; i++)
            B[i] = (acc += 0.5 * (h[i - 1] + h[i]) / OS);
        double total = B[2 * H * OS];
        for (int i = 0; i < N; i++)
            B[i] /= total;
        acc = 0;
        R[0] = 0;
        for (int i = 1; i < N; i++) {
            double x0 = -H + (double)(i - 1) / OS, x1 = -H + (double)i / OS;
            double e0 = B[i - 1] - (x0 >= 0 ? 1.0 : 0.0), e1 = B[i] - (x1 >= 0 ? 1.0 : 0.0);
            if (x0 < 0 && x1 >= 0)
                e0 = B[i - 1];
            R[i] = (acc += 0.5 * (e0 + e1) / OS);
        }
        double drift = R[2 * H * OS];
        std::vector<float> out(2 * N);
        for (int i = 0; i < N; i++) {
            out[i] = (float)B[i];
            out[N + i] = (float)(R[i] - drift * std::min(1.0, (double)i / (2 * H * OS)));
        }
        out[N - 1] = 1.0f;
        out[2 * N - 1] = 0.0f;
        return out;
    }();
    return t.data();
}

namespace
{
constexpr float VOICE_GAIN = 0.2f;
constexpr int KEY_ARP = 1000;
/* SUPER SAW: the six side saws' detune (a fraction of the frequency at full spread), JP-8000-like */
const float SS_DET[7] = {0.0f, -0.110f, 0.110f, -0.063f, 0.063f, -0.022f, 0.022f};
const float SS_SPREAD[3] = {0.055f, 0.095f, 0.16f};      /* (the outer saws: about +-10, +-18, +-30 cents) */
const float SS_SIDE[3] = {0.45f, 0.6f, 0.75f};

inline float envSec(float v) { return 0.0008f * std::exp2(v * (13.8f / 127.0f)); }       /* 0.8 ms .. 11 s */
inline float lfoHz(float v) { return 0.05f * std::exp2(v * (10.3f / 127.0f)); }          /* 0.05 .. 63 Hz */
inline float portaSec(float v) { return 0.003f * std::exp2(v * (10.0f / 127.0f)); }      /* 3 ms .. 3 s */
/* a depth -63..63 as a curve (fine near 0, wide at the ends): -1..1 */
inline float curve(float d) { float a = d / 63.0f; return a * std::fabs(a); }
inline float lfoShape(int sh, float ph, float held, float r0, float r1)
{
    switch (sh) {
    case LS_TRI: return 1.0f - 4.0f * std::fabs(ph - 0.5f);
    case LS_SIN: return std::sin(2.0f * PI * ph);
    case LS_SAW: return 2.0f * ph - 1.0f;
    case LS_SQR: return ph < 0.5f ? 1.0f : -1.0f;
    case LS_SH: return held;
    default: {                                              /* RANDOM: smooth, a new value each cycle */
        float c = 0.5f - 0.5f * std::cos(PI * ph);
        return r0 + (r1 - r0) * c;
    }
    }
}
} // namespace

Engine::Engine()
{
    patchInit(pat);
    tab = blepTables();
    for (auto &p : phr) {
        p.bars = 4;
        p.n = 0;
    }
    gatePatternSteps(0, patSteps);
}

void Engine::prepare(double sampleRate)
{
    sr = (float)sampleRate;
    fx.prepare(sr);
    for (auto &v : V) {
        v = Voice();
        for (auto &t : v.t)
            for (auto &o : t.o)
                o.clear();
    }
    /* CIRCUIT MOD: each voice its own parts, the same every time (a machine's voice 5 is always a little flat) */
    uint32_t tseed = 0x2F6B3A1Du;
    for (auto &v : V) {
        v.tolP = rndF(tseed);
        v.tolF = rndF(tseed);
        v.tolE = rndF(tseed);
        v.tolL = rndF(tseed);
        v.tolR = rndF(tseed);
    }
    nKeys = nLatched = nArpPlaying = 0;
    arpN = 0;
    gpos = CH;
    dcL = dcR = dcxL = dcxR = 0.0f;
    setPatch(pat);
}

void Engine::setPatch(const Patch &p)
{
    pat = p;
    patShown = -1;
    if (pat.com[C_GATE_PAT] < NGATEPAT) {
        patLen = gatePatternSteps(pat.com[C_GATE_PAT], patSteps);
        patShown = pat.com[C_GATE_PAT];
    }
    if (!pat.com[C_ARP_ON] && arpN > 0) {
        for (int i = 0; i < nArpPlaying; i++)
            stopNote(arpPlaying[i]);
        nArpPlaying = 0;
        arpN = 0;
    }
    arpRebuild();
}

void Engine::setParam(int idx, int v)
{
    if (idx < 0 || idx >= NPATCH)
        return;
    int was = patchGet(pat, idx);
    patchSet(pat, idx, v);
    v = patchGet(pat, idx);
    if (v == was)
        return;
    if (idx == comIdx(C_GATE_PAT) && v < NGATEPAT && v != patShown) {
        patLen = gatePatternSteps(v, patSteps);
        patShown = v;
    }
    if (idx == comIdx(C_ARP_ON) || idx == comIdx(C_ARP_VAR)) {
        if (!pat.com[C_ARP_ON]) {
            for (int i = 0; i < nArpPlaying; i++)
                stopNote(arpPlaying[i]);
            nArpPlaying = 0;
            arpN = 0;
            /* (the keys still held sound again, as they would without the arpeggio) */
            const int8_t *src = holdOn ? latched : keys;
            const float *sv = holdOn ? latchedVel : keyVel;
            int n = holdOn ? nLatched : nKeys;
            for (int i = 0; i < n; i++)
                playNote(src[i], src[i], sv[i]);
        } else {
            if (idx == comIdx(C_ARP_ON))
                for (int i = 0; i < 128; i++)
                    stopNote(i);
            arpRebuild();
        }
    }
    if (idx == comIdx(C_MONO))
        for (auto &vv : V)
            if (vv.note >= 0)
                releaseVoice(vv);
    if (phState == PH_RECORDING && !phRehearse && !fromPlayback && (phType == PT_ALL || phType == PT_CONTROLLER))
        phraseRecord(PE_PARAM, 0, idx, v);
}

void Engine::setGate(const uint8_t *steps) { std::memcpy(pat.gate, steps, GATE_STEPS); }
int Engine::gateLength() const { return pat.com[C_GATE_PAT] < NGATEPAT ? patLen : clampi_(pat.com[C_GATE_LEN]); }

void Engine::setVoices(int n)
{
    n = n < 1 ? 1 : n > MAXV ? MAXV : n;
    if (n == nVoices)
        return;
    for (int i = n; i < MAXV; i++)
        if (V[i].note >= 0) {
            V[i].note = -1;
            V[i].gate = false;
        }
    nVoices = n;
}

/* ------------------------------------------------------------------ D BEAM --- */
int Engine::dbOffset(bool common, int p, int t) const
{
    if (pat.com[C_DB_MODE] != DB_ASSIGN || dbS < 0.002f)
        return 0;
    const DBeamTarget &tg = DBTARGET[pat.com[C_DB_ASSIGN] < 0 ? 0 : pat.com[C_DB_ASSIGN] % NDBTARGET];
    if (tg.common != common || tg.param != p)
        return 0;
    int tones = pat.com[C_DB_TONES];
    if (!common && tones > 0 && tones - 1 != t)
        return 0;
    const ParamDef &d = common ? COMMON[p] : TONE[p];
    float off = dbS * (float)(d.hi - d.lo);
    return (int)std::lround(pat.com[C_DB_POL] ? -off : off);
}
int Engine::pv(int t, int p) const
{
    int v = pat.tone[t][p];
    int o = dbOffset(false, p, t);
    if (o) {
        v += o;
        v = v < TONE[p].lo ? TONE[p].lo : v > TONE[p].hi ? TONE[p].hi : v;
    }
    return v;
}
int Engine::cv(int p) const
{
    int v = pat.com[p];
    int o = dbOffset(true, p, -1);
    if (o) {
        v += o;
        v = v < COMMON[p].lo ? COMMON[p].lo : v > COMMON[p].hi ? COMMON[p].hi : v;
    }
    return v;
}
void Engine::dbeam(bool on, float v)
{
    dbOn = on;
    dbV = clampf(v, 0.0f, 1.0f);
}

/* ------------------------------------------------------------------ keys --- */
void Engine::noteOn(int note, float vel)
{
    note += transpose;
    if (note < 0 || note > 127)
        return;
    if (velFix)
        vel = 100.0f / 127.0f;
    if (phState == PH_RECORDING && !phRehearse && !fromPlayback && (phType == PT_ALL || phType == PT_NOTES))
        phraseRecord(PE_NOTE_ON, note, (int)std::lround(vel * 127.0f), 0);
    keyDown(note, vel);
}

void Engine::noteOff(int note)
{
    note += transpose;
    if (note < 0 || note > 127)
        return;
    if (phState == PH_RECORDING && !phRehearse && !fromPlayback && (phType == PT_ALL || phType == PT_NOTES))
        phraseRecord(PE_NOTE_OFF, note, 0, 0);
    keyUp(note);
}

void Engine::keyDown(int note, float vel)
{
    /* free running (the host stopped): the clock starts again with a key into silence, so the gate and the
     * arpeggio begin on their first step */
    if (!hostPlaying && nKeys == 0 && (!holdOn || latchFresh) && voicesSounding() == 0) {
        pos = 0.0;
        arpStepIdx = -1;
    }
    for (int i = 0; i < nKeys; i++)
        if (keys[i] == note) {
            std::memmove(keys + i, keys + i + 1, (size_t)(nKeys - i - 1));
            std::memmove(keyVel + i, keyVel + i + 1, sizeof(float) * (size_t)(nKeys - i - 1));
            nKeys--;
            break;
        }
    if (nKeys < NKEYS) {
        keys[nKeys] = (int8_t)note;
        keyVel[nKeys] = vel;
        nKeys++;
    }
    if (holdOn) {
        if (latchFresh) {                         /* a new chord: the one held lets go */
            if (!pat.com[C_ARP_ON])
                for (int i = 0; i < nLatched; i++)
                    stopNote(latched[i]);
            nLatched = 0;
            latchFresh = false;
        }
        bool there = false;
        for (int i = 0; i < nLatched; i++)
            there |= latched[i] == note;
        if (!there && nLatched < NKEYS) {
            latched[nLatched] = (int8_t)note;
            latchedVel[nLatched] = vel;
            nLatched++;
        }
    }
    if (pat.com[C_ARP_ON])
        arpRebuild();
    else
        playNote(note, note, vel);
}

void Engine::keyUp(int note)
{
    for (int i = 0; i < nKeys; i++)
        if (keys[i] == note) {
            std::memmove(keys + i, keys + i + 1, (size_t)(nKeys - i - 1));
            std::memmove(keyVel + i, keyVel + i + 1, sizeof(float) * (size_t)(nKeys - i - 1));
            nKeys--;
            break;
        }
    if (nKeys == 0)
        latchFresh = true;
    if (pat.com[C_ARP_ON]) {
        if (!holdOn)
            arpRebuild();
        return;
    }
    if (holdOn)
        return;
    stopNote(note);
}

void Engine::keyHold(bool on)
{
    if (on == holdOn)
        return;
    holdOn = on;
    if (on) {
        nLatched = 0;
        for (int i = 0; i < nKeys; i++) {
            latched[nLatched] = keys[i];
            latchedVel[nLatched] = keyVel[i];
            nLatched++;
        }
        latchFresh = nKeys == 0;
    } else {
        /* the notes no key holds any more let go */
        for (int i = 0; i < nLatched; i++) {
            bool held = false;
            for (int k = 0; k < nKeys; k++)
                held |= keys[k] == latched[i];
            if (!held && !pat.com[C_ARP_ON])
                stopNote(latched[i]);
        }
        nLatched = 0;
        if (pat.com[C_ARP_ON])
            arpRebuild();
    }
}

void Engine::sustain(bool on)
{
    susOn = on;
    if (!on)
        for (auto &v : V)
            if (v.sus) {
                v.sus = false;
                releaseVoice(v);
            }
}

void Engine::bend(float v)
{
    bendV = clampf(v, -1.0f, 1.0f);
    if (phState == PH_RECORDING && !phRehearse && !fromPlayback && (phType == PT_ALL || phType == PT_BENDER))
        phraseRecord(PE_BEND, 0, (int)std::lround(bendV * 8191.0f), 0);
}
void Engine::modWheel(float v)
{
    mwV = clampf(v, 0.0f, 1.0f);
    if (phState == PH_RECORDING && !phRehearse && !fromPlayback && (phType == PT_ALL || phType == PT_BENDER))
        phraseRecord(PE_MOD, 0, (int)std::lround(mwV * 127.0f), 0);
}
void Engine::expression(float v) { exV = clampf(v, 0.0f, 1.0f); }

void Engine::allOff()
{
    nKeys = nLatched = 0;
    latchFresh = true;
    for (int i = 0; i < nArpPlaying; i++)
        stopNote(arpPlaying[i]);
    nArpPlaying = 0;
    arpN = 0;
    for (auto &v : V) {
        v.sus = false;
        if (v.note >= 0)
            releaseVoice(v);
    }
}

void Engine::panic()
{
    allOff();
    phraseNotesOff();
    for (auto &v : V) {
        v.note = -1;
        v.gate = false;
        v.pendNote = -1;
        for (auto &t : v.t) {
            t.ae = Env();
            t.fe = Env();
            t.rampOk = false;
        }
    }
    fx.clear();
}

void Engine::host(bool playing, double ppq, double hbpm, bool sync)
{
    hostSync = sync;
    hostBpm = (float)hbpm;
    if (playing && sync) {
        if (!hostPlaying)
            arpStepIdx = -1;
        pos = ppq;
    }
    hostPlaying = playing && sync;
}

/* ------------------------------------------------------------------ voices --- */
int Engine::findVoice()
{
    /* a free one; else the oldest released; else the oldest */
    int best = -1;
    uint32_t bestAge = 0xffffffffu;
    for (int i = 0; i < nVoices; i++)
        if (V[i].note < 0 && V[i].pendNote < 0)
            return i;
    for (int i = 0; i < nVoices; i++)
        if (!V[i].gate && V[i].age < bestAge) {
            best = i;
            bestAge = V[i].age;
        }
    if (best >= 0)
        return best;
    for (int i = 0; i < nVoices; i++)
        if (V[i].age < bestAge) {
            best = i;
            bestAge = V[i].age;
        }
    return best < 0 ? 0 : best;
}

void Engine::playNote(int key, int note, float vel)
{
    if (pat.com[C_MONO]) {
        Voice &v = V[0];
        for (int i = 1; i < nVoices; i++)
            if (V[i].note >= 0)
                releaseVoice(V[i]);
        bool legato = v.gate && v.note >= 0 && pat.com[C_LEGATO];
        if (legato) {
            v.note = note;
            v.key = key;
            v.target = (float)note;
            if (!pat.com[C_PORTA])
                v.pitch = v.target;
        } else {
            startVoice(v, key, note, vel, true);
        }
        lastPitch = (float)note;
        return;
    }
    /* (the same key again: its voice) */
    for (int i = 0; i < nVoices; i++)
        if (V[i].key == key && V[i].note >= 0) {
            startVoice(V[i], key, note, vel, true);
            return;
        }
    Voice &v = V[findVoice()];
    if (v.note >= 0 && v.t[0].ae.v + v.t[1].ae.v + v.t[2].ae.v > 0.001f) {
        /* stolen: a short fade, then the new note */
        v.stolen = true;
        v.pendNote = note;
        v.pendKey = key;
        v.pendVel = vel;
        v.gate = false;
        return;
    }
    startVoice(v, key, note, vel, true);
}

void Engine::startVoice(Voice &v, int key, int note, float vel, bool retrig)
{
    bool fresh = v.note < 0;
    v.key = key;
    v.note = note;
    v.vel = vel;
    v.gate = true;
    v.sus = false;
    v.stolen = false;
    v.fade = 1.0f;
    v.pendNote = -1;
    v.age = ++ageN;
    v.target = (float)note;
    if (pat.com[C_PORTA])
        v.pitch = (pat.com[C_MONO] && !fresh) ? v.pitch : lastPitch;
    else
        v.pitch = v.target;
    lastPitch = (float)note;
    if (!retrig)
        return;
    for (int t = 0; t < 3; t++) {
        ToneV &tv = v.t[t];
        tv.ae.st = ES_ATT;
        tv.fe.st = ES_ATT;
        if (fresh) {
            tv.ae.v = 0.0f;
            tv.fe.v = 0.0f;
            tv.rampOk = false;
            for (auto &s : tv.s1)
                s = 0.0f;
            for (auto &s : tv.s2)
                s = 0.0f;
            /* the oscillators start where analog ones would: anywhere (SUPER SAW: each its own) */
            tv.seed = noiseSeed = noiseSeed * 1664525u + 1013904223u + (uint32_t)note;
            for (int k = 0; k < 7; k++) {
                tv.o[k].clear();
                tv.o[k].ph = (float)(rndU(tv.seed) & 0xffff) / 65536.0f;
            }
            tv.ph[0] = tv.ph[1] = tv.ph[2] = 0.0f;
            tv.fbY = 0.0f;
        }
        tv.pe = 0.0f;
        tv.peSt = 1;
        tv.fade = 0.0f;
        if (pat.com[C_LFO_KEYTRIG]) {
            tv.lfoPh = 0.0f;
            tv.lfoSh = rndF(lfoSeed);
            tv.lfoPrevR = tv.lfoSh;
            tv.lfoNextR = rndF(lfoSeed);
        }
    }
}

void Engine::stopNote(int key)
{
    if (pat.com[C_MONO]) {
        Voice &v = V[0];
        if (v.key != key)
            return;
        /* back to the last key still held (legato) */
        const int8_t *src = keys;
        int n = nKeys;
        if (key < KEY_ARP && n > 0) {
            int back = src[n - 1];
            float vel = keyVel[n - 1];
            if (pat.com[C_LEGATO] && v.gate) {
                v.note = back;
                v.key = back;
                v.target = (float)back;
                if (!pat.com[C_PORTA])
                    v.pitch = v.target;
            } else
                startVoice(v, back, back, vel, true);
            return;
        }
        if (susOn)
            v.sus = true;
        else
            releaseVoice(v);
        return;
    }
    for (int i = 0; i < nVoices; i++) {
        Voice &v = V[i];
        if (v.pendNote >= 0 && v.pendKey == key) {
            v.pendNote = -1;
            continue;
        }
        if (v.key == key && v.note >= 0 && v.gate) {
            if (susOn)
                v.sus = true;
            else
                releaseVoice(v);
        }
    }
}

void Engine::releaseVoice(Voice &v)
{
    v.gate = false;
    for (auto &t : v.t) {
        if (t.ae.st != ES_IDLE)
            t.ae.st = ES_REL;
        if (t.fe.st != ES_IDLE)
            t.fe.st = ES_REL;
    }
}

int Engine::voicesSounding() const
{
    int n = 0;
    for (int i = 0; i < nVoices; i++)
        n += V[i].note >= 0;
    return n;
}

/* ------------------------------------------------------------------ the ARPEGGIO --- */
void Engine::arpRebuild()
{
    const int8_t *src = holdOn ? latched : keys;
    const float *sv = holdOn ? latchedVel : keyVel;
    int n = holdOn ? nLatched : nKeys;
    if (!pat.com[C_ARP_ON] || n == 0) {
        if (arpN > 0) {
            for (int i = 0; i < nArpPlaying; i++)
                stopNote(arpPlaying[i]);
            nArpPlaying = 0;
        }
        arpN = 0;
        arpI = -1;
        return;
    }
    const ArpPattern &ap = ARP[pat.com[C_ARP_VAR] % NARP];
    int8_t s[NKEYS];
    float sv2[NKEYS];
    for (int i = 0; i < n; i++) {
        s[i] = src[i];
        sv2[i] = sv[i];
    }
    if (ap.order != AO_PLAYED)                     /* sorted, low to high */
        for (int i = 1; i < n; i++)
            for (int j = i; j > 0 && s[j] < s[j - 1]; j--) {
                std::swap(s[j], s[j - 1]);
                std::swap(sv2[j], sv2[j - 1]);
            }
    int oc = ap.octaves < 1 ? 1 : ap.octaves;
    int m = 0;
    auto put = [&](int note, float vel) {
        if (m < NKEYS * 4 && note <= 127) {
            arpNotes[m] = (int8_t)note;
            arpVel[m] = vel;
            m++;
        }
    };
    switch (ap.order) {
    case AO_DOWN:
        for (int o = oc - 1; o >= 0; o--)
            for (int i = n - 1; i >= 0; i--)
                put(s[i] + 12 * o, sv2[i]);
        break;
    case AO_UPDOWN: {
        for (int o = 0; o < oc; o++)
            for (int i = 0; i < n; i++)
                put(s[i] + 12 * o, sv2[i]);
        int up = m;
        for (int k = up - 2; k > 0; k--)
            put(arpNotes[k], arpVel[k]);
        break;
    }
    case AO_PEDAL:                                 /* the lowest note between each of the others */
        if (n == 1) {
            for (int o = 0; o < oc; o++) {
                put(s[0], sv2[0]);
                put(s[0] + 12 * (o + 1), sv2[0]);
            }
        } else
            for (int o = 0; o < oc; o++)
                for (int i = 1; i < n; i++) {
                    put(s[0], sv2[0]);
                    put(s[i] + 12 * o, sv2[i]);
                }
        break;
    case AO_BOUNCE:                                /* low, top, low, the next down ... */
        for (int o = oc - 1; o >= 0; o--)
            for (int i = n - 1; i >= (n > 1 ? 1 : 0); i--) {
                put(s[0], sv2[0]);
                put(s[i] + 12 * o + (n == 1 ? 12 : 0), sv2[i]);
            }
        break;
    case AO_OCTJUMP:
        for (int i = 0; i < n; i++) {
            put(s[i], sv2[i]);
            put(s[i] + 12 * oc, sv2[i]);
        }
        break;
    case AO_CONVERGE:
        for (int o = 0; o < oc; o++) {
            int a = 0, b = n - 1;
            while (a <= b) {
                put(s[a] + 12 * o, sv2[a]);
                if (b != a)
                    put(s[b] + 12 * o, sv2[b]);
                a++;
                b--;
            }
        }
        break;
    case AO_CHORD:
        for (int i = 0; i < n; i++)
            put(s[i], sv2[i]);
        break;
    default:                                       /* UP, RANDOM, AS PLAYED */
        for (int o = 0; o < oc; o++)
            for (int i = 0; i < n; i++)
                put(s[i] + 12 * o, sv2[i]);
    }
    if (arpN == 0)
        arpI = -1;
    arpN = m;
    if (arpI >= arpN)
        arpI = -1;
}

void Engine::arpStep()
{
    if (arpN == 0)
        return;
    const ArpPattern &ap = ARP[pat.com[C_ARP_VAR] % NARP];
    int len = ap.len < 1 ? 1 : ap.len;
    int k = (int)(arpStepIdx % len);
    arpShow = k;
    char c = ap.rhythm[k];
    double sb = GATERATE_BEATS[ap.rate];
    if (c == '-') {                                /* the note before carries on */
        if (arpOffAt >= 0.0)
            arpOffAt += sb;
        return;
    }
    for (int i = 0; i < nArpPlaying; i++)
        stopNote(arpPlaying[i]);
    nArpPlaying = 0;
    arpOffAt = -1.0;
    if (c == '.')
        return;
    float acc = c == 'X' ? 1.18f : 1.0f;
    if (ap.order == AO_CHORD) {
        for (int i = 0; i < arpN && nArpPlaying < NKEYS; i++) {
            playNote(KEY_ARP + arpNotes[i], arpNotes[i], minf(1.0f, arpVel[i] * acc));
            arpPlaying[nArpPlaying++] = KEY_ARP + arpNotes[i];
        }
    } else {
        if (ap.order == AO_RANDOM) {
            uint32_t r = rndU(lfoSeed);
            int j = (int)(r % (uint32_t)arpN);
            if (arpN > 1 && j == arpI)
                j = (j + 1) % arpN;
            arpI = j;
        } else
            arpI = (arpI + 1) % arpN;
        int note = arpNotes[arpI];
        playNote(KEY_ARP + note, note, minf(1.0f, arpVel[arpI] * acc));
        arpPlaying[nArpPlaying++] = KEY_ARP + note;
    }
    arpOffAt = pos + sb * clampf(ap.gate / 100.0f, 0.05f, 1.0f) - 1e-6;
}

/* ------------------------------------------------------------------ the TRANCE GATE --- */
float Engine::stepBeats() const { return GATERATE_BEATS[pat.com[C_GATE_RATE] % NGATERATE]; }

float Engine::gateAt(double x, int *stepOut) const
{
    const uint8_t *st = gateSteps();
    int len = gateLength();
    double sb = stepBeats();
    float swing = pat.com[C_GATE_SWING] / 100.0f * 0.6f;
    double s = x / sb;
    double pair = std::floor(s * 0.5);
    float p = (float)(s - 2.0 * pair), f;
    long idx;
    if (p < 1.0f + swing) {
        idx = (long)(2.0 * pair);
        f = p / (1.0f + swing);
    } else {
        idx = (long)(2.0 * pair) + 1;
        f = (p - 1.0f - swing) / (1.0f - swing);
    }
    int k = (int)(((idx % len) + len) % len);
    if (stepOut)
        *stepOut = k;
    float lvl = (float)(st[k] & 0x7f) / 100.0f;
    if (lvl <= 0.0f)
        return 0.0f;
    bool tie = (st[k] & GATE_TIE) != 0, nextTie = (st[(k + 1) % len] & GATE_TIE) != 0 && (st[(k + 1) % len] & 0x7f);
    float a = pat.com[C_GATE_ATT] / 127.0f * 0.7f;
    float env = (tie || a < 0.004f) ? 1.0f : minf(1.0f, f / a);
    if (!nextTie) {
        const float endOpen = 0.94f;
        float rr = maxf(0.03f, pat.com[C_GATE_REL] / 127.0f * 0.9f);
        float e0 = endOpen - rr;
        if (f > e0)
            env *= maxf(0.0f, (endOpen - f) / rr);
    }
    return lvl * env;
}

int Engine::clampi_(int v) const { return v < 1 ? 1 : v > GATE_STEPS ? GATE_STEPS : v; }

/* ------------------------------------------------------------------ the PHRASE RECORDER --- */
void Engine::phraseRecord(uint8_t type, int a, int b, int c)
{
    if (phPos < 0.0)
        return;
    Phrase &ph = phr[phSel];
    if (ph.n >= PHRASE_EVENTS)
        return;
    PhraseEvent e;
    e.beat = (float)phPos;
    e.type = type;
    e.a = (uint8_t)a;
    e.b = (int16_t)b;
    e.c = (int16_t)c;
    /* in time order (after those at the same beat) */
    int i = ph.n;
    while (i > 0 && ph.ev[i - 1].beat > e.beat)
        i--;
    std::memmove(ph.ev + i + 1, ph.ev + i, sizeof(PhraseEvent) * (size_t)(ph.n - i));
    ph.ev[i] = e;
    ph.n++;
}

void Engine::phraseDispatch(const PhraseEvent &e)
{
    fromPlayback = true;
    switch (e.type) {
    case PE_NOTE_ON:
        if (e.a < 128) {
            if (phNoteHeld[e.a])
                keyUp(e.a);
            phNoteHeld[e.a] = true;
            keyDown(e.a, (float)e.b / 127.0f);
        }
        break;
    case PE_NOTE_OFF:
        if (e.a < 128 && phNoteHeld[e.a]) {
            phNoteHeld[e.a] = false;
            keyUp(e.a);
        }
        break;
    case PE_BEND: bend((float)e.b / 8191.0f); break;
    case PE_MOD: modWheel((float)e.b / 127.0f); break;
    case PE_PARAM:
        if (e.b >= 0 && e.b < NPATCH) {
            setParam(e.b, e.c);
            int nw = (pqW + 1) & 255;
            if (nw != pqR) {
                playedQ[pqW][0] = e.b;
                playedQ[pqW][1] = e.c;
                pqW = nw;
            }
        }
        break;
    default: break;
    }
    fromPlayback = false;
}

bool Engine::pollPlayedParam(int &idx, int &v)
{
    if (pqR == pqW)
        return false;
    idx = playedQ[pqR][0];
    v = playedQ[pqR][1];
    pqR = (pqR + 1) & 255;
    return true;
}

void Engine::phraseNotesOff()
{
    fromPlayback = true;
    for (int n = 0; n < 128; n++)
        if (phNoteHeld[n]) {
            phNoteHeld[n] = false;
            keyUp(n);
        }
    fromPlayback = false;
}

void Engine::phraseRec()
{
    switch (phState) {
    case PH_IDLE:
    case PH_PLAYING:
        phraseNotesOff();
        phState = PH_STANDBY;
        phPos = 0.0;
        metroBeat = -1;
        break;
    case PH_STANDBY: phState = PH_IDLE; break;
    case PH_COUNTIN: phState = PH_STANDBY; break;
    case PH_RECORDING: phRehearse = !phRehearse; break;
    default: break;
    }
}

void Engine::phrasePlay()
{
    switch (phState) {
    case PH_STANDBY:
        phState = PH_COUNTIN;
        phPos = -4.0;                                /* a bar's count-in */
        metroBeat = -10;
        phRehearse = false;
        break;
    case PH_IDLE:
        if (phNext >= 0) {
            phSel = phNext;
            phNext = -1;
        }
        phState = PH_PLAYING;
        phPos = 0.0;
        /* (what sits at the very start: now) */
        for (int i = 0; i < phr[phSel].n && phr[phSel].ev[i].beat <= 0.0f; i++)
            phraseDispatch(phr[phSel].ev[i]);
        break;
    default:
        phState = PH_IDLE;
        phRehearse = false;
        phraseNotesOff();
        break;
    }
}

void Engine::phraseSelect(int k)
{
    k &= 7;
    if (phState == PH_PLAYING)
        phNext = k;
    else if (phState == PH_IDLE || phState == PH_STANDBY) {
        phSel = k;
        phNext = -1;
    }
}

void Engine::phraseLength(int bars)
{
    if (phState == PH_RECORDING || phState == PH_PLAYING || phState == PH_COUNTIN)
        return;
    Phrase &p = phr[phSel];
    p.bars = (uint8_t)(bars < 1 ? 1 : bars > 8 ? 8 : bars);
    p.n = 0;
}

void Engine::phraseClear()
{
    if (phState == PH_RECORDING || phState == PH_PLAYING)
        phraseNotesOff();
    phr[phSel].n = 0;
}

void Engine::phraseChunk(double from, double to)
{
    if (phState == PH_IDLE)
        return;
    double d = to - from;
    double prev = phPos;
    phPos += d;
    /* the metronome: a beat's click (the bar's first higher) */
    if (phState == PH_STANDBY || phState == PH_COUNTIN || phState == PH_RECORDING) {
        long b = (long)std::floor(phPos);
        if (b != metroBeat) {
            metroBeat = b;
            long inBar = ((b % 4) + 4) % 4;
            metroHz = inBar == 0 ? 1760.0f : 1175.0f;
            metroEnv = 1.0f;
            metroPh = 0.0f;
        }
    }
    if (phState == PH_STANDBY) {
        if (phPos >= 4.0)
            phPos -= 4.0;
        return;
    }
    if (phState == PH_COUNTIN) {
        if (phPos < 0.0)
            return;
        phState = PH_RECORDING;
        prev = 0.0;
        for (int i = 0; i < phr[phSel].n && phr[phSel].ev[i].beat <= 0.0f; i++)
            phraseDispatch(phr[phSel].ev[i]);
    }
    Phrase &ph = phr[phSel];
    double len = 4.0 * (ph.bars < 1 ? 1 : ph.bars);
    /* the window (prev, phPos], round the loop's end */
    auto window = [&](double ad, double bd) {
        /* (in the events' own float: an event just recorded at the window's start is not played again) */
        float a = (float)ad, b = (float)bd;
        if (phState == PH_RECORDING && phErasing && !phRehearse) {
            int w = 0;
            for (int i = 0; i < ph.n; i++) {
                const PhraseEvent &e = ph.ev[i];
                bool in = e.beat > a && e.beat <= b;
                bool mine = phType == PT_ALL || (phType == PT_NOTES && e.type <= PE_NOTE_OFF) ||
                            (phType == PT_BENDER && (e.type == PE_BEND || e.type == PE_MOD)) ||
                            (phType == PT_CONTROLLER && e.type == PE_PARAM);
                if (in && mine && e.type != PE_NOTE_OFF)
                    continue;
                ph.ev[w++] = e;
            }
            ph.n = (uint16_t)w;
        }
        for (int i = 0; i < ph.n; i++) {
            const PhraseEvent &e = ph.ev[i];
            if (e.beat > a && e.beat <= b)
                phraseDispatch(e);
        }
    };
    if (phPos < len) {
        window(prev, phPos);
        return;
    }
    window(prev, len);
    phPos -= len;
    if (phState == PH_PLAYING && phNext >= 0 && phNext != phSel) {
        phraseNotesOff();
        phSel = phNext;
        phNext = -1;
    }
    Phrase &ph2 = phr[phSel];
    for (int i = 0; i < ph2.n; i++)
        if (ph2.ev[i].beat <= (float)phPos)
            phraseDispatch(ph2.ev[i]);
}

/* ------------------------------------------------------------------ the clock --- */
void Engine::clockChunk(double from, double to)
{
    /* the ARPEGGIO: a step at each step's start */
    if (pat.com[C_ARP_ON] && arpN > 0) {
        const ArpPattern &ap = ARP[pat.com[C_ARP_VAR] % NARP];
        double sb = GATERATE_BEATS[ap.rate];
        if (arpOffAt >= 0.0 && to >= arpOffAt) {
            for (int i = 0; i < nArpPlaying; i++)
                stopNote(arpPlaying[i]);
            nArpPlaying = 0;
            arpOffAt = -1.0;
        }
        long s = (long)std::floor(to / sb + 1e-9);
        if (arpStepIdx < 0 || s != arpStepIdx) {
            if (arpStepIdx < 0)
                s = (long)std::floor(from / sb + 1e-9);
            arpStepIdx = s < 0 ? 0 : s;
            arpStep();
        }
    } else
        arpShow = -1;
    beatLed = (to - std::floor(to)) < 0.18;
}

/* ------------------------------------------------------------------ the LFOs (the tones' own, free) --- */
void Engine::lfoChunk(float dt)
{
    for (int t = 0; t < 3; t++) {
        const int16_t *P = pat.tone[t];
        auto adv = [&](float &ph, float &sh, float &r0, float &r1, int shape, bool sync, int rate) {
            float hz = sync ? 1.0f / (LFOSYNC_BEATS[rate * NLFOSYNC / 128] * 60.0f / bpm) : lfoHz((float)rate);
            ph += hz * dt;
            if (ph >= 1.0f) {
                ph -= std::floor(ph);
                sh = rndF(lfoSeed);
                r0 = r1;
                r1 = rndF(lfoSeed);
            }
            return lfoShape(shape, ph, sh, r0, r1);
        };
        tLfoV[t] = adv(tLfoPh[t], tLfoSh[t], tLfoR0[t], tLfoR1[t], P[T_LFO_SHAPE], P[T_LFO_SYNC] != 0, pv(t, T_LFO_RATE));
        mLfoV[t] = adv(mLfoPh[t], mLfoSh[t], mLfoR0[t], mLfoR1[t], P[T_MLFO_SHAPE], P[T_MLFO_SYNC] != 0, P[T_MLFO_RATE]);
        lfoShow[t] = tLfoV[t];
    }
}

float Engine::envStep(Env &e, float a, float d, float s, float r, float dt, bool gate)
{
    (void)gate;
    switch (e.st) {
    case ES_ATT:
        e.v += dt / envSec(a);
        if (e.v >= 1.0f) {
            e.v = 1.0f;
            e.st = ES_DEC;
        }
        break;
    case ES_DEC: {
        float k = 1.0f - std::exp(-dt * 4.0f / envSec(d));
        e.v += (s - e.v) * k;
        break;
    }
    case ES_REL: {
        float k = std::exp(-dt * 4.5f / envSec(r));
        e.v *= k;
        if (e.v < 0.0002f) {
            e.v = 0.0f;
            e.st = ES_IDLE;
        }
        break;
    }
    default: e.v = 0.0f;
    }
    return e.v;
}

/* ------------------------------------------------------------------ a tone, a chunk --- */
void Engine::toneChunk(Voice &v, int t, float *L, float *R, float *mod2, bool want2, float *wrap2)
{
    ToneV &tv = v.t[t];
    const int16_t *P = pat.tone[t];
    const float dt = (float)CH / sr;
    bool on = P[T_ON] != 0;
    int modMode = pat.com[C_MOD];

    /* CIRCUIT MOD: how old (0..1), this voice's parts */
    const float age = cmS * pat.com[C_CMOD_AGE] / 127.0f, drift = cmS * pat.com[C_CMOD_DRIFT] / 127.0f;
    const float eT = v.tolE * age * 7.0f;          /* (its envelopes' timing caps: a little slower or faster) */

    /* envelopes */
    float fe = envStep(tv.fe, pv(t, T_FENV_A) + eT, pv(t, T_FENV_D) + eT, pv(t, T_FENV_S) / 127.0f, pv(t, T_FENV_R) + eT, dt, v.gate);
    float ae = envStep(tv.ae, pv(t, T_AENV_A) + eT + age * 3.0f, pv(t, T_AENV_D) + eT, pv(t, T_AENV_S) / 127.0f, pv(t, T_AENV_R) + eT, dt, v.gate);
    if (tv.peSt == 1) {
        float a = envSec((float)pv(t, T_PENV_A));
        tv.pe += pv(t, T_PENV_A) == 0 ? 1.0f : dt / a;
        if (tv.pe >= 1.0f) {
            tv.pe = 1.0f;
            tv.peSt = 2;
        }
    } else if (tv.peSt == 2) {
        tv.pe *= std::exp(-dt * 4.0f / envSec((float)pv(t, T_PENV_D)));
        if (tv.pe < 0.0005f) {
            tv.pe = 0.0f;
            tv.peSt = 0;
        }
    }

    /* the LFO (KEY TRIGGER: its own; else the tone's), with its FADE TIME */
    float lfo;
    if (pat.com[C_LFO_KEYTRIG]) {
        float hz = P[T_LFO_SYNC] ? 1.0f / (LFOSYNC_BEATS[pv(t, T_LFO_RATE) * NLFOSYNC / 128] * 60.0f / bpm)
                                 : lfoHz((float)pv(t, T_LFO_RATE));
        tv.lfoPh += hz * dt;
        if (tv.lfoPh >= 1.0f) {
            tv.lfoPh -= std::floor(tv.lfoPh);
            tv.lfoSh = rndF(lfoSeed);
            tv.lfoPrevR = tv.lfoNextR;
            tv.lfoNextR = rndF(lfoSeed);
        }
        lfo = lfoShape(P[T_LFO_SHAPE], tv.lfoPh, tv.lfoSh, tv.lfoPrevR, tv.lfoNextR);
    } else
        lfo = tLfoV[t];
    int fadeV = pv(t, T_LFO_FADE);
    tv.fade = fadeV == 0 ? 1.0f : minf(1.0f, tv.fade + dt / (envSec((float)fadeV) * 0.9f));
    lfo *= tv.fade;
    float mlfo = mLfoV[t] * ctlMw;

    /* pitch */
    float semis = v.pitch + 12.0f * (float)pat.com[C_OCTAVE] + (float)pv(t, T_PITCH) + pv(t, T_DETUNE) / 100.0f;
    semis += curve((float)pv(t, T_PENV_DEPTH)) * 48.0f * tv.pe;
    semis += curve((float)pv(t, T_LFO_PITCH)) * 12.0f * lfo + curve((float)P[T_MLFO_PITCH]) * 12.0f * mlfo;
    semis += ctlBend >= 0.0f ? ctlBend * (float)pat.com[C_BEND_UP] : ctlBend * (float)pat.com[C_BEND_DOWN];
    if (pat.com[C_DB_MODE] == DB_PITCH)
        semis += dbS * 12.0f;
    semis += v.tolP * age * 0.14f + v.drP * drift * 0.3f + std::sin(cmWowPh) * drift * 0.04f;
    float inc = minf(hzOf(semis) / sr, 0.45f);

    /* the pulse's width: PW, PWM (the LFO) */
    float pw = 0.5f + 0.45f * pv(t, T_PW) / 127.0f + lfo * pv(t, T_PWM) / 127.0f * 0.45f;
    pw = clampf(pw, 0.03f, 0.97f);

    /* the filter */
    int mode = P[T_FMODE];
    bool slope24 = P[T_SLOPE] != 0;
    float velF = 1.0f - pat.com[C_FENV_VEL] / 63.0f * (1.0f - v.vel);
    float oct = pv(t, T_CUTOFF) / 127.0f * 10.0f + (v.pitch - 60.0f) / 12.0f * pv(t, T_KEYF) / 100.0f;
    oct += pv(t, T_FENV_DEPTH) / 63.0f * 8.0f * fe * velF;
    oct += curve((float)pv(t, T_LFO_FILTER)) * 5.0f * lfo + curve((float)P[T_MLFO_FILTER]) * 5.0f * mlfo;
    bool gated = pat.com[C_GATE_ON] && pat.com[C_GATE_T1 + t];
    if (gated)
        oct += pat.com[C_GATE_FILTER] / 63.0f * 4.0f * (gateNow - 1.0f);
    oct += v.tolF * age * 0.45f + v.drF * drift * 0.25f - age * 0.25f;
    float fc = clampf(20.0f * std::exp2(oct), 15.0f, 0.46f * sr);
    float g = std::tan(PI * fc / sr);
    float r = clampf(pv(t, T_RESO) / 127.0f * (1.0f + v.tolR * age * 0.25f), 0.0f, 1.0f), k;
    if (mode == F_PKG)
        k = 1.0f / (0.7f + r * 6.0f);
    else
        k = (slope24 ? 0.765f : 1.414f) * (1.0f - 0.985f * std::pow(r, 0.9f));

    /* the amp: LEVEL, its envelope, velocity, the LFO's tremolo, the gate */
    float lv = pv(t, T_LEVEL) / 127.0f * (1.0f + v.tolL * age * 0.18f);
    float velA = 1.0f - pat.com[C_LEVEL_VEL] / 63.0f * (1.0f - v.vel);
    float aa = curve((float)pv(t, T_LFO_AMP)), am = curve((float)P[T_MLFO_AMP]) * ctlMw;
    float trem = 1.0f - std::fabs(aa) * (1.0f - (aa >= 0 ? lfo : -lfo)) * 0.5f;
    trem *= 1.0f - std::fabs(am) * (1.0f - (am >= 0 ? mLfoV[t] : -mLfoV[t])) * 0.5f;
    float amp = on ? lv * lv * ae * velA * trem * v.fade * VOICE_GAIN : 0.0f;
    float gateA0 = 1.0f, gateA1 = 1.0f;
    if (gated) {
        float depth = cv(C_GATE_DEPTH) / 127.0f;
        gateA0 = 1.0f - depth * (1.0f - gatePrev);
        gateA1 = 1.0f - depth * (1.0f - gateNow);
    }
    /* pan */
    float pan = pv(t, T_PAN) / 63.0f + curve((float)P[T_LFO_PAN]) * lfo + curve((float)P[T_MLFO_PAN]) * mLfoV[t] * ctlMw;
    pan = clampf(pan, -1.0f, 1.0f);
    float ang = (pan + 1.0f) * PI * 0.25f;
    float pL = std::cos(ang) * 1.4142f, pR = std::sin(ang) * 1.4142f;

    if (!tv.rampOk) {
        tv.inc = inc;
        tv.g = g;
        tv.k = k;
        tv.amp = 0.0f;
        tv.panL = pL;
        tv.panR = pR;
        tv.pw = pw;
        tv.rampOk = true;
    }
    bool modulator = t == 1 && want2;
    bool silent = !on || (tv.ae.st == ES_IDLE && tv.amp <= 0.0f);
    if (silent && !modulator) {
        tv.amp = amp;
        tv.inc = inc;
        tv.g = g;
        tv.k = k;
        return;
    }
    const float ic = 1.0f / (float)CH;
    float dInc = (inc - tv.inc) * ic, dG = (g - tv.g) * ic, dK = (k - tv.k) * ic;
    float dAmp = (amp * gateA1 - tv.amp) * ic, dPL = (pL - tv.panL) * ic, dPR = (pR - tv.panR) * ic;
    float cInc = tv.inc, cG = tv.g, cK = tv.k, cAmp = tv.amp, cPL = tv.panL, cPR = tv.panR;
    (void)gateA0;

    int wave = P[T_WAVE], var = P[T_VAR] < 0 ? 0 : P[T_VAR] > 2 ? 2 : P[T_VAR];
    Wave wA, wB;
    switch (wave) {
    case W_SAW: wA.set(1, 0.5f); wB.set(1, 0.5f); break;
    case W_SQR: wA.set(4, 0.5f); wB.set(4, 0.5f); break;
    case W_PULSE: wA.set(4, pw); wB.set(4, var == 2 ? 0.5f : pw); break;
    case W_TRI: wA.set(3, 0.5f); wB.set(3, 0.5f); break;
    case W_SUPERSAW: wA.set(1, 0.5f); break;
    default: break;
    }
    /* the second oscillator's ratio (the variations) */
    float ratB = 1.0f, mixA = 1.0f, mixB = 0.0f;
    bool useB = false;
    switch (wave) {
    case W_SAW:
        if (var == 1) { ratB = std::exp2(12.0f / 1200.0f); mixA = 0.62f; mixB = 0.62f; useB = true; }
        if (var == 2) { ratB = 0.5f; mixA = 0.7f; mixB = 0.5f; useB = true; }
        break;
    case W_SQR:
        if (var == 1) { ratB = 0.5f; mixA = 0.62f; mixB = 0.55f; useB = true; }
        if (var == 2) { ratB = 2.0f; mixA = 0.75f; mixB = 0.38f; useB = true; }
        break;
    case W_PULSE:
        if (var == 1) { ratB = std::exp2(9.0f / 1200.0f); mixA = 0.62f; mixB = 0.62f; useB = true; }
        if (var == 2) { ratB = 0.5f; mixA = 0.7f; mixB = 0.5f; useB = true; }
        break;
    case W_TRI:
        if (var == 2) { ratB = 2.0f; mixA = 0.8f; mixB = 0.4f; useB = true; }
        break;
    default: break;
    }
    bool sync1 = t == 0 && modMode == MM_SYNC && mod2;
    bool ring1 = t == 0 && modMode == MM_RING && mod2;
    float buf[CH];
    for (int i = 0; i < CH; i++) {
        cInc += dInc;
        float reset = -1.0f;
        if (sync1 && wrap2[i] >= 0.0f)
            reset = wrap2[i];
        int wr = 0;
        float wd = 0.0f, y = 0.0f;
        switch (wave) {
        case W_SAW:
        case W_SQR:
        case W_PULSE:
        case W_TRI: {
            y = tv.o[0].tick(tab, wA, cInc, reset, wr, wd);
            if (useB) {
                int w2;
                float d2;
                y = mixA * y + mixB * tv.o[1].tick(tab, wB, cInc * ratB, reset, w2, d2);
            }
            if (wave == W_TRI && var == 1)              /* folded: brighter */
                y = std::sin(y * 2.2f) * 1.05f;
            if (wave == W_SQR || wave == W_PULSE)
                y *= 0.8f;
            break;
        }
        case W_SIN: {
            float ph = tv.ph[0];
            if (reset >= 0.0f)
                ph = cInc * reset;
            else {
                ph += cInc;
                if (ph >= 1.0f) {
                    ph -= 1.0f;
                    wr = 1;
                    wd = minf(ph / cInc, 0.999f);
                }
            }
            tv.ph[0] = ph;
            if (var == 0)
                y = std::sin(2.0f * PI * ph);
            else if (var == 1)
                y = 0.85f * std::sin(2.0f * PI * ph) + 0.3f * std::sin(4.0f * PI * ph);
            else {
                y = std::sin(2.0f * PI * ph + 1.3f * tv.fbY);
                tv.fbY = 0.5f * (y + tv.fbY);
            }
            break;
        }
        case W_NOISE: {
            float w = rndF(tv.seed);
            if (var == 0)
                y = w * 0.7f;
            else if (var == 1) {                          /* pink (Kellet's three poles) */
                tv.noiseB[0] = 0.99765f * tv.noiseB[0] + w * 0.0990460f;
                tv.noiseB[1] = 0.96300f * tv.noiseB[1] + w * 0.2965164f;
                tv.noiseB[2] = 0.57000f * tv.noiseB[2] + w * 1.0526913f;
                y = (tv.noiseB[0] + tv.noiseB[1] + tv.noiseB[2] + w * 0.1848f) * 0.25f;
            } else {                                      /* digital: held at the key's rate */
                tv.shPh += cInc * 4.0f;
                if (tv.shPh >= 1.0f) {
                    tv.shPh -= std::floor(tv.shPh);
                    tv.shV = w;
                }
                y = tv.shV * 0.7f;
            }
            break;
        }
        case W_SUPERSAW: {
            float side = SS_SIDE[var], spread = SS_SPREAD[var], s = 0.0f;
            for (int o = 0; o < 7; o++) {
                int w2;
                float d2;
                float x = tv.o[o].tick(tab, wA, cInc * (1.0f + SS_DET[o] * spread), reset, w2, d2);
                if (o == 0) {
                    wr = w2;
                    wd = d2;
                    s += x;
                } else
                    s += x * side;
            }
            y = s * 0.3f;
            break;
        }
        default: break;
        }
        if (t == 1 && want2) {
            mod2[i] = y;
            wrap2[i] = wr ? wd : -1.0f;
        }
        if (ring1)
            y *= mod2[i] * 1.5f;
        buf[i] = y;
    }
    if (silent) {
        tv.inc = inc;
        tv.amp = amp * gateA1;
        tv.g = g;
        tv.k = k;
        return;
    }
    /* the filter (two state-variable stages: -12 one, -24 both) and the amp */
    float s10 = tv.s1[0], s20 = tv.s2[0], s11 = tv.s1[1], s21 = tv.s2[1];
    float boost = 2.0f + 4.0f * r;
    float k1 = slope24 && mode != F_PKG ? 1.848f : 0.0f;
    float lvlSum = 0.0f;
    for (int i = 0; i < CH; i++) {
        cG += dG;
        cK += dK;
        cAmp += dAmp;
        cPL += dPL;
        cPR += dPR;
        float x = buf[i], y;
        if (mode == F_BYPASS)
            y = x;
        else {
            /* stage one: plain (-24: a Butterworth pole pair), or the only one */
            float kk = slope24 && mode != F_PKG ? k1 : cK;
            float a = 1.0f / (1.0f + cG * (cG + kk));
            float hp = (x - (cG + kk) * s10 - s20) * a;
            float bp = cG * hp + s10;
            s10 = cG * hp + bp;
            float lp = cG * bp + s20;
            s20 = cG * bp + lp;
            switch (mode) {
            case F_LPF: y = lp; break;
            case F_HPF: y = hp; break;
            case F_BPF: y = kk * bp; break;
            default: y = x + boost * kk * bp; break;
            }
            if (slope24) {
                float k2 = cK;
                float a2 = 1.0f / (1.0f + cG * (cG + k2));
                float hp2 = (y - (cG + k2) * s11 - s21) * a2;
                float bp2 = cG * hp2 + s11;
                s11 = cG * hp2 + bp2;
                float lp2 = cG * bp2 + s21;
                s21 = cG * bp2 + lp2;
                switch (mode) {
                case F_LPF: y = lp2; break;
                case F_HPF: y = hp2; break;
                case F_BPF: y = k2 * bp2 * (1.0f + 2.0f * r); break;
                default: y = y + 0.5f * boost * k2 * bp2; break;
                }
            }
            y = softT(y * (0.8f + age * 0.9f)) * 1.25f / (1.0f + age * 0.5f);   /* (the peaks kept in hand; old: hotter) */
        }
        y *= cAmp;
        lvlSum += std::fabs(y);
        L[i] += y * cPL;
        R[i] += y * cPR;
    }
    tv.s1[0] = flushDn(s10);
    tv.s2[0] = flushDn(s20);
    tv.s1[1] = flushDn(s11);
    tv.s2[1] = flushDn(s21);
    tv.inc = inc;
    tv.g = g;
    tv.k = k;
    tv.amp = amp * gateA1;
    tv.panL = pL;
    tv.panR = pR;
    tv.pw = pw;
    float m = lvlSum * ic * 4.0f;
    if (m > toneMeter[t])
        toneMeter[t] = m;
}

void Engine::voiceChunk(Voice &v, float *L, float *R)
{
    const float dt = (float)CH / sr;
    /* PORTAMENTO */
    if (pat.com[C_PORTA] && v.pitch != v.target) {
        float k = 1.0f - std::exp(-dt / portaSec((float)cv(C_PORTA_TIME)));
        v.pitch += (v.target - v.pitch) * k;
        if (std::fabs(v.target - v.pitch) < 0.001f)
            v.pitch = v.target;
    } else
        v.pitch = v.target;
    if (v.stolen) {
        v.fade -= dt / 0.004f;
        if (v.fade <= 0.0f) {
            v.fade = 0.0f;
            v.stolen = false;
            int n = v.pendNote, k = v.pendKey;
            float vel = v.pendVel;
            v.note = -1;
            for (auto &t : v.t) {
                t.ae = Env();
                t.fe = Env();
            }
            if (n >= 0)
                startVoice(v, k, n, vel, true);
            return;
        }
    }
    if (cmS > 0.0f) {                              /* CIRCUIT MOD: each voice wanders on its own, slowly */
        if ((rndU(cmSeed) & 255u) < 3u) {
            v.drPt = rndF(cmSeed);
            v.drFt = rndF(cmSeed);
        }
        float k = 1.0f - std::exp(-dt / 1.5f);
        v.drP += (v.drPt - v.drP) * k;
        v.drF += (v.drFt - v.drF) * k;
    }
    int mm = pat.com[C_MOD];
    bool want2 = mm != MM_OFF;
    float mod2[CH], wrap2[CH];
    toneChunk(v, 1, L, R, mod2, want2, wrap2);
    toneChunk(v, 0, L, R, want2 ? mod2 : nullptr, false, wrap2);
    toneChunk(v, 2, L, R, nullptr, false, nullptr);
    /* done when every tone that sounds is silent */
    bool alive = false;
    for (int t = 0; t < 3; t++)
        if (pat.tone[t][T_ON] && v.t[t].ae.st != ES_IDLE)
            alive = true;
    if (!alive && !v.gate) {
        v.note = -1;
        v.key = -1;
    }
}

/* ------------------------------------------------------------------ a chunk --- */
void Engine::chunk()
{
    const float dt = (float)CH / sr;
    bpm = (hostSync && hostBpm > 1.0f) ? hostBpm : (float)pat.com[C_TEMPO];
    double from = pos, to = pos + (double)dt * bpm / 60.0;
    pos = to;

    /* the controllers glide a little; the D BEAM */
    float kc = 1.0f - std::exp(-dt / 0.006f);
    float dbT = dbOn ? dbV : 0.0f;
    dbS += (dbT - dbS) * (1.0f - std::exp(-dt / 0.02f));
    float bT = bendV, mT = mwV;
    if (pat.com[C_DB_MODE] == DB_ASSIGN) {
        int tg = pat.com[C_DB_ASSIGN] % NDBTARGET;
        float s = pat.com[C_DB_POL] ? -dbS : dbS;
        if (DBTARGET[tg].param == -1)
            bT = clampf(bT + s, -1.0f, 1.0f);
        if (DBTARGET[tg].param == -2)
            mT = clampf(mT + dbS, 0.0f, 1.0f);
    }
    ctlBend += (bT - ctlBend) * kc;
    ctlMw += (mT - ctlMw) * kc;

    cmS += ((pat.com[C_CMOD] ? 1.0f : 0.0f) - cmS) * (1.0f - std::exp(-dt / 0.05f));
    if (cmS < 1e-4f)
        cmS = 0.0f;
    cmWowPh += 2.0f * PI * 0.55f * dt;
    if (cmWowPh > 2.0f * PI)
        cmWowPh -= 2.0f * PI;
    phraseChunk(from, to);
    clockChunk(from, to);
    lfoChunk(dt);
    /* the TRANCE GATE at this chunk's end */
    gatePrev = gateNow;
    if (pat.com[C_GATE_ON]) {
        int st = -1;
        gateNow = gateAt(to, &st);
        gateShow = st;
    } else {
        gateNow = 1.0f;
        gateShow = -1;
    }

    float L[CH] = {}, R[CH] = {};
    for (int t = 0; t < 3; t++)
        toneMeter[t] *= 0.97f;
    for (int i = 0; i < nVoices; i++) {
        Voice &v = V[i];
        if (v.note >= 0 || v.pendNote >= 0)
            voiceChunk(v, L, R);
    }
    /* the DC away, the effects, the patch's level, the volume (expression, the D BEAM's VOLUME) */
    for (int i = 0; i < CH; i++) {
        float xl = L[i], xr = R[i];
        dcL = xl - dcxL + 0.9995f * dcL;
        dcxL = xl;
        dcR = xr - dcxR + 0.9995f * dcR;
        dcxR = xr;
        L[i] = dcL;
        R[i] = dcR;
    }
    /* CIRCUIT MOD: the tired highs, and hiss and mains hum that come and go with the sound */
    if (cmS > 0.0f) {
        float age = cmS * pat.com[C_CMOD_AGE] / 127.0f, nz = cmS * pat.com[C_CMOD_NOISE] / 127.0f;
        float fc = 20000.0f * std::exp2(-age * 2.3f);
        float k = 1.0f - std::exp(-2.0f * PI * minf(fc, 0.45f * sr) / sr);
        for (int i = 0; i < CH; i++) {
            cmLp[0] += (L[i] - cmLp[0]) * k;
            cmLp[1] += (R[i] - cmLp[1]) * k;
            float a = std::fabs(L[i]) + std::fabs(R[i]);
            cmFollow = a > cmFollow ? cmFollow + (a - cmFollow) * 0.01f : cmFollow * 0.99995f;
            float bed = nz * minf(1.0f, cmFollow * 30.0f);
            cmHumPh += 50.0f / sr;
            if (cmHumPh >= 1.0f)
                cmHumPh -= 1.0f;
            float hum = std::sin(2.0f * PI * cmHumPh) + 0.4f * std::sin(6.0f * PI * cmHumPh);
            float hl = rndF(cmSeed), hr = rndF(cmSeed);
            L[i] += (cmLp[0] - L[i]) * cmS + bed * (0.0025f * hl + 0.0012f * hum);
            R[i] += (cmLp[1] - R[i]) * cmS + bed * (0.0025f * hr + 0.0012f * hum);
        }
        cmLp[0] = flushDn(cmLp[0]);
        cmLp[1] = flushDn(cmLp[1]);
    }
    auto off = [&](int p) { return (float)dbOffset(true, p, -1); };
    fx.set(pat.com, bpm, off(C_DIST_DRIVE), off(C_FL_LEVEL), off(C_DL_LEVEL), off(C_RV_LEVEL));
    fx.process(L, R, CH);
    float pl = pat.com[C_PATCH_LEVEL] / 100.0f;
    float lvl = pl * pl * exV;
    if (pat.com[C_DB_MODE] == DB_VOLUME && dbOn)
        lvl *= dbS;
    float l0 = levelS;
    levelS += (lvl - levelS) * (1.0f - std::exp(-dt / 0.01f));
    float dl = (levelS - l0) / (float)CH;
    /* the metronome (the phrase recorder's) */
    for (int i = 0; i < CH; i++) {
        l0 += dl;
        float m = 0.0f;
        if (metroEnv > 0.0005f) {
            metroPh += metroHz / sr;
            if (metroPh >= 1.0f)
                metroPh -= 1.0f;
            m = std::sin(2.0f * PI * metroPh) * metroEnv * metroLevel * 0.35f;
            metroEnv *= 0.9975f;
        }
        outL[i] = L[i] * l0 + m;
        outR[i] = R[i] * l0 + m;
    }
}

void Engine::render(float *L, float *R, int n)
{
    for (int i = 0; i < n; i++) {
        if (gpos >= CH) {
            chunk();
            gpos = 0;
        }
        L[i] = outL[gpos];
        R[i] = outR[gpos];
        gpos++;
    }
}
} // namespace gl
