/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
/* GALA SH-1T's engine: a Roland GAIA SH-01 (its owner's manual) with a modern TRANCE GATE.
 *
 *   voices    8 .. 64 (the GAIA's 64), each playing the three TONES that are on
 *   a tone    OSC: SAW, SQUARE, PULSE/PWM, TRIANGLE, SINE, NOISE, SUPER SAW, three VARIATIONS each, PITCH, DETUNE,
 *             PW, PWM, a pitch envelope (A, D, DEPTH); FILTER: LPF, HPF, BPF, PKG or BYPASS, -12 or -24 dB, CUTOFF,
 *             RESONANCE, KEY FOLLOW, an ADSR with DEPTH; AMP: LEVEL, an ADSR, PAN; an LFO (six shapes, TEMPO SYNC,
 *             FADE TIME, pitch, filter, amp and pan depths) and the MODULATION LFO (the mod lever's)
 *   SYNC/RING tone 2's oscillator onto tone 1's (hard sync, or the two multiplied)
 *   play      MONO (with LEGATO), PORTAMENTO, the patch's OCTAVE, BEND ranges, KEY HOLD, the ARPEGGIO's 64
 *             variations, the D BEAM (pitch, volume or one of 33 targets)
 *   effects   DIST / FUZZ / BIT CRASH, FLANGER / PHASER / PITCH SHIFTER, DELAY / PANNING DELAY, REVERB, LOW BOOST
 *   gate      the TRANCE GATE: up to 32 steps (levels, ties), 1/4 .. 1/32, swing, attack, release, depth, a filter
 *             amount, on the tones chosen
 *   phrases   the PHRASE RECORDER: eight phrases of up to eight bars, notes, the lever and the knobs, recorded over
 *             a count-in and looped (overdub, rehearse, erase), played back
 *
 * The engine renders in chunks of 16 samples (the modulation's rate); notes land on the next chunk. All calls from
 * the audio thread. */
#pragma once
#include "Dsp.h"
#include "Fx.h"
#include "Params.h"

namespace gl
{
constexpr int MAXV = 64, CH = 16, NKEYS = 32;

/* ---------------------------------------------------------- the PHRASE RECORDER --- */
enum PhraseEv : uint8_t { PE_NOTE_ON, PE_NOTE_OFF, PE_BEND, PE_MOD, PE_PARAM };
struct PhraseEvent {
    float beat;               /* where in the phrase (beats from its start) */
    uint8_t type, a;          /* PE_*; the note (ON / OFF), the velocity's 0..127 in b */
    int16_t b, c;             /* PARAM: the patch parameter (b) and its value (c); BEND: -8192..8191; MOD 0..127 */
};
constexpr int PHRASE_EVENTS = 3000, NPHRASE = 8;
struct Phrase {
    uint8_t bars;             /* 1..8 (four beats each) */
    uint16_t n;
    PhraseEvent ev[PHRASE_EVENTS];
};
enum PhraseState { PH_IDLE, PH_STANDBY, PH_COUNTIN, PH_RECORDING, PH_PLAYING };
enum PhraseType { PT_ALL, PT_BENDER, PT_CONTROLLER, PT_NOTES };

class Engine
{
public:
    Engine();
    void prepare(double sampleRate);
    /* the sound: a whole patch, any time (a block's start) */
    void setPatch(const Patch &p);
    const Patch &patch() const { return pat; }
    /* one of the patch's parameters (the host's, the panel's): the phrase recorder takes it while recording */
    void setParam(int idx, int v);
    void setGate(const uint8_t *steps);          /* the TRANCE GATE's "User" steps (levels 0..100, | GATE_TIE) */
    int gateLength() const;                      /* the steps in use (the pattern's own, or GATE LEN for "User") */
    const uint8_t *gateSteps() const { return pat.com[C_GATE_PAT] < NGATEPAT ? patSteps : pat.gate; }
    void setVoices(int n);                       /* 8 .. 64 */
    void setVelocityFix(bool on) { velFix = on; }
    void setTranspose(int t) { transpose = t; }

    void noteOn(int note, float vel);            /* vel 0..1 */
    void noteOff(int note);
    void sustain(bool on);
    void bend(float v);                          /* -1..1 */
    void modWheel(float v);                      /* 0..1 */
    void expression(float v);
    void keyHold(bool on);
    bool holding() const { return holdOn; }
    void allOff();
    void panic();
    /* the host's transport: playing, its position (quarters), its tempo; sync: follow its tempo (else the patch's) */
    void host(bool playing, double ppq, double bpm, bool sync);
    /* the D BEAM: a hand over it (on), how high (0..1) */
    void dbeam(bool on, float v);

    void render(float *L, float *R, int n);

    /* the PHRASE RECORDER */
    void phraseRec();                            /* REC: standby; while recording: rehearse (and back) */
    void phrasePlay();                           /* PLAY/STOP */
    void phraseSelect(int k);                    /* 0..7 (while playing: at the phrase's end) */
    void phraseLength(int bars);                 /* in standby: the length, the phrase cleared */
    void phraseClear();
    void phraseType(int t) { phType = t; }       /* what is recorded (and erased): PT_* */
    void phraseErase(bool on) { phErasing = on; }
    void setMetronome(float level) { metroLevel = level; }
    int phraseState() const { return phState; }
    bool phraseRehearsing() const { return phRehearse; }
    int phraseSelected() const { return phSel; }
    float phrasePos() const { return (float)phPos; }      /* beats */
    Phrase &phrase(int k) { return phr[k & 7]; }
    const Phrase &phrase(int k) const { return phr[k & 7]; }
    /* the knobs the playback turned: (index, value) pairs taken by the plugin (false: none waiting) */
    bool pollPlayedParam(int &idx, int &v);

    /* for the editor */
    int voicesSounding() const;
    int gateStep() const { return gateShow; }    /* the step playing (-1 none) */
    float gateLevel() const { return gateNow; }
    int arpStepShown() const { return arpShow; }
    bool beat() const { return beatLed; }
    float bpmNow() const { return bpm; }
    float lfoValue(int t) const { return lfoShow[t]; }
    float toneLevel(int t) const { return toneMeter[t]; }

private:
    struct Env {
        float v = 0.0f;
        uint8_t st = 0;                          /* ES_* */
    };
    enum { ES_IDLE, ES_ATT, ES_DEC, ES_REL };

    struct ToneV {
        Osc o[7];                                /* the oscillators (SUPER SAW: seven; the variations: two) */
        float ph[3] = {};                        /* the sine's, the triangle's fold, the second sine's */
        float fbY = 0.0f;                        /* SINE 3: its feedback */
        float noiseB[3] = {}, shV = 0.0f, shPh = 0.0f;
        uint32_t seed = 1;
        float s1[2] = {}, s2[2] = {};            /* the filter's two state-variable stages */
        Env fe, ae;
        float pe = 0.0f;                         /* the pitch envelope, 0..1 */
        uint8_t peSt = 0;
        float lfoPh = 0.0f, lfoSh = 0.0f, lfoPrevR = 0.0f, lfoNextR = 0.0f, fade = 0.0f;
        /* the chunk's ramps (where the last chunk ended) */
        float inc = 0.0f, g = 0.0f, k = 2.0f, amp = 0.0f, panL = 0.7f, panR = 0.7f, pw = 0.5f;
        bool rampOk = false;
    };
    struct Voice {
        int note = -1, key = -1;                 /* key: what plays it (a key, the arp's) */
        bool gate = false, sus = false;
        float vel = 0.0f;
        float pitch = 60.0f, target = 60.0f;     /* glides (PORTAMENTO) */
        uint32_t age = 0;
        float fade = 1.0f;                       /* a stolen voice's quick fade */
        bool stolen = false;
        int pendNote = -1, pendKey = -1;
        float pendVel = 0.0f;
        ToneV t[3];
        /* CIRCUIT MOD: this voice's parts (tolerances, -1..1, fixed) and its drift now */
        float tolP = 0, tolF = 0, tolE = 0, tolL = 0, tolR = 0, drP = 0, drF = 0, drPt = 0, drFt = 0;
    };

    Patch pat;
    Voice V[MAXV];
    FxChain fx;
    int nVoices = 32;
    float sr = 44100.0f;
    const float *tab = nullptr;
    uint32_t ageN = 0, noiseSeed = 0x2545F491u;
    float bendV = 0, mwV = 0, exV = 1.0f, ctlBend = 0, ctlMw = 0;
    bool susOn = false, holdOn = false, velFix = false;
    int transpose = 0;
    bool hostPlaying = false, hostSync = true;
    double hostPpq = 0.0;
    float hostBpm = 120.0f, bpm = 120.0f;
    double pos = 0.0;                            /* the clock: quarters */
    bool posFresh = true;                        /* (free running: restarts with the first key) */
    float outL[CH] = {}, outR[CH] = {};
    int gpos = CH;
    float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0;
    float levelS = 1.0f;
    /* CIRCUIT MOD's: the tired output's low-pass, the follower that lets the hiss and hum come with the sound, wow */
    float cmLp[2] = {}, cmFollow = 0.0f, cmHumPh = 0.0f, cmWowPh = 0.0f, cmS = 0.0f;
    uint32_t cmSeed = 0x6A09E667u;

    /* keys: held (in order), for MONO, the ARPEGGIO */
    int8_t keys[NKEYS];
    float keyVel[NKEYS];
    int nKeys = 0;
    int8_t latched[NKEYS];                       /* KEY HOLD's */
    float latchedVel[NKEYS];
    int nLatched = 0;
    bool latchFresh = true;
    float lastPitch = 60.0f;                     /* PORTAMENTO's start (poly) */

    /* the tones' free LFOs (KEY TRIGGER off) and the MODULATION LFOs */
    float tLfoPh[3] = {}, tLfoSh[3] = {}, tLfoR0[3] = {}, tLfoR1[3] = {};
    float mLfoPh[3] = {}, mLfoSh[3] = {}, mLfoR0[3] = {}, mLfoR1[3] = {};
    float tLfoV[3] = {}, mLfoV[3] = {}, lfoShow[3] = {}, toneMeter[3] = {};
    uint32_t lfoSeed = 0x9E3779B9u;

    /* the ARPEGGIO */
    int8_t arpNotes[NKEYS * 4];
    float arpVel[NKEYS * 4];
    int arpN = 0, arpI = -1, arpDir = 1, arpShow = -1;
    long arpStepIdx = -1;
    int arpPlaying[NKEYS];
    int nArpPlaying = 0;
    double arpOffAt = -1.0;

    /* the TRANCE GATE */
    float gateNow = 1.0f, gatePrev = 1.0f, gateSm = 1.0f;
    uint8_t patSteps[GATE_STEPS] = {};
    int patLen = 16, patShown = -1;
    int gateShow = -1;
    bool beatLed = false;

    /* the D BEAM */
    bool dbOn = false;
    float dbV = 0.0f, dbS = 0.0f;

    /* the PHRASE RECORDER */
    Phrase phr[NPHRASE];
    int phState = PH_IDLE, phSel = 0, phNext = -1, phType = PT_ALL;
    bool phRehearse = false, phErasing = false, phPlayingBack = false;
    double phPos = 0.0;
    float metroLevel = 0.5f, metroEnv = 0.0f, metroPh = 0.0f, metroHz = 1000.0f;
    long metroBeat = -1;
    bool phNoteHeld[128] = {};                    /* notes the playback holds */
    int playedQ[256][2];
    int pqW = 0, pqR = 0;
    bool fromPlayback = false;

    /* parts */
    int pv(int t, int p) const;                   /* a tone's parameter with the D BEAM on it */
    int cv(int p) const;
    int dbOffset(bool common, int p, int t) const;
    void keyDown(int note, float vel);
    void keyUp(int note);
    void playNote(int key, int note, float vel);
    void stopNote(int key);
    void startVoice(Voice &v, int key, int note, float vel, bool retrig);
    int findVoice();
    void releaseVoice(Voice &v);
    void chunk();
    void clockChunk(double from, double to);
    void arpStep();
    void arpRebuild();
    void gateChunk(double at);
    float gateAt(double x, int *stepOut) const;
    void phraseChunk(double from, double to);
    void phraseRecord(uint8_t type, int a, int b, int c);
    void phraseDispatch(const PhraseEvent &e);
    void phraseNotesOff();
    void lfoChunk(float dt);
    void voiceChunk(Voice &v, float *L, float *R);
    void toneChunk(Voice &v, int t, float *L, float *R, float *mod2, bool want2, float *wrap2);
    float envStep(Env &e, float a, float d, float s, float r, float dt, bool gate);
    float stepBeats() const;
    int clampi_(int v) const;
};
} // namespace gl
