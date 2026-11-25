/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
/* GALA SH-1T's parameters, laid out as a Roland GAIA SH-01 patch (its owner's manual, "Patch Parameters", pp. 45-50):
 * three TONES of the same parameters (OSC, FILTER, AMP, LFO and the MODULATION LFO, each tone its own), and the
 * patch's COMMON ones (SYNC/RING, MONO, PORTAMENTO, the effects, the arpeggio, the D BEAM), every value an integer
 * in the GAIA's own range (CUTOFF 0..127, PITCH -24..+24, ENV DEPTH -63..+63, ...).
 *
 * What GALA adds to the GAIA: the TRANCE GATE (a step gate on the tones' amps, with its own pattern menu), CIRCUIT
 * MOD (an old machine's voices), LEGATO,
 * LOW BOOST's amount, the D BEAM's tones, and every SHIFT function of the GAIA on its own control. */
#pragma once
#include <cstdint>

namespace gl
{
/* ------------------------------------------------------------------ choices --- */
enum WaveType { W_SAW, W_SQR, W_PULSE, W_TRI, W_SIN, W_NOISE, W_SUPERSAW, NWAVE };
enum FMode { F_LPF, F_HPF, F_BPF, F_PKG, F_BYPASS, NFMODE };
enum LShape { LS_TRI, LS_SIN, LS_SAW, LS_SQR, LS_SH, LS_RND, NLSHAPE };
enum ModMode { MM_OFF, MM_SYNC, MM_RING };
enum DistType { DT_OFF, DT_DIST, DT_FUZZ, DT_BITCRASH };
enum ModFx { MF_OFF, MF_FLANGER, MF_PHASER, MF_PITCH };
enum DelayType { DL_OFF, DL_DELAY, DL_PANNING };
enum ReverbType { RT_ROOM, RT_PLATE, RT_HALL };
enum DBeamMode { DB_OFF, DB_PITCH, DB_VOLUME, DB_ASSIGN };
enum GateRate { GR_4, GR_8, GR_8T, GR_16, GR_16T, GR_32, NGATERATE };

constexpr int NLFOSYNC = 20;                       /* the LFOs' TEMPO SYNC values (manual p. 47) */

/* -------------------------------------------------------- a tone's parameters --- */
enum ToneParam {
    T_ON,
    /* OSC */
    T_WAVE, T_VAR, T_PITCH, T_DETUNE, T_PWM, T_PW, T_PENV_A, T_PENV_D, T_PENV_DEPTH,
    /* FILTER */
    T_FMODE, T_SLOPE, T_CUTOFF, T_RESO, T_KEYF, T_FENV_A, T_FENV_D, T_FENV_S, T_FENV_R, T_FENV_DEPTH,
    /* AMP */
    T_LEVEL, T_AENV_A, T_AENV_D, T_AENV_S, T_AENV_R, T_PAN,
    /* LFO */
    T_LFO_SHAPE, T_LFO_SYNC, T_LFO_RATE, T_LFO_FADE, T_LFO_PITCH, T_LFO_FILTER, T_LFO_AMP, T_LFO_PAN,
    /* MODULATION LFO (the mod lever's) */
    T_MLFO_SHAPE, T_MLFO_SYNC, T_MLFO_RATE, T_MLFO_PITCH, T_MLFO_FILTER, T_MLFO_AMP, T_MLFO_PAN,
    NTONE
};

/* ------------------------------------------------------ the patch's own ones --- */
enum CommonParam {
    C_MOD,                                   /* OFF, SYNC, RING: tone 2's OSC onto tone 1's */
    C_MONO, C_LEGATO, C_PORTA, C_PORTA_TIME, C_OCTAVE, C_BEND_UP, C_BEND_DOWN, C_TEMPO, C_LFO_KEYTRIG,
    C_FENV_VEL, C_LEVEL_VEL, C_PATCH_LEVEL,
    C_ARP_ON, C_ARP_VAR,
    /* EFFECTS */
    C_FX_ON,
    C_DIST, C_DIST_DRIVE, C_DIST_TYPE, C_DIST_PRES, C_DIST_LEVEL,
    C_FUZZ_DRIVE, C_FUZZ_TYPE, C_FUZZ_PRES, C_FUZZ_LEVEL,
    C_BC_RATE, C_BC_BITS, C_BC_FILTER, C_BC_LEVEL,
    C_MODFX, C_FL_FB, C_FL_DEPTH, C_FL_RATE, C_FL_LEVEL,
    C_PH_RESO, C_PH_DEPTH, C_PH_RATE, C_PH_LEVEL,
    C_PS_PITCH, C_PS_DETUNE, C_PS_PITCH2, C_PS_LEVEL,
    C_DELAY, C_DELAY_SYNC, C_DL_TIME, C_DL_FB, C_DL_DAMP, C_DL_LEVEL,
    C_PD_TIME, C_PD_FB, C_PD_DAMP, C_PD_LEVEL,
    C_REVERB, C_RV_TIME, C_RV_TYPE, C_RV_DAMP, C_RV_LEVEL,
    C_LOWBOOST, C_LB_AMOUNT,
    /* TRANCE GATE */
    C_GATE_ON, C_GATE_PAT, C_GATE_RATE, C_GATE_LEN, C_GATE_SWING, C_GATE_ATT, C_GATE_REL, C_GATE_DEPTH,
    C_GATE_FILTER, C_GATE_T1, C_GATE_T2, C_GATE_T3,
    /* D BEAM */
    C_DB_MODE, C_DB_ASSIGN, C_DB_POL, C_DB_TONES,
    /* CIRCUIT MOD: the voices as an old machine's (each its own tolerances, drift, tired highs, hiss and hum) */
    C_CMOD, C_CMOD_AGE, C_CMOD_DRIFT, C_CMOD_NOISE,
    NCOMMON
};

/* a patch parameter's place in one list: tone t's p at t * NTONE + p, the common ones after the three tones */
constexpr int NPATCH = 3 * NTONE + NCOMMON;
inline int toneIdx(int t, int p) { return t * NTONE + p; }
inline int comIdx(int p) { return 3 * NTONE + p; }

enum Kind : uint8_t {
    K_NUM,      /* a number as it is */
    K_SIGNED,   /* with its sign (+12, -3) */
    K_CHOICE,   /* a name from the list */
    K_ONOFF,
    K_LFORATE,  /* 0..127, a note value when synced */
    K_DLTIME,   /* the delay's time: ms, a note value when synced */
    K_ARP,      /* A1 .. H8 */
    K_DB        /* the delay's HIGH DAMP: -36..0 dB */
};

struct ParamDef {
    const char *id;           /* the host's id (stable: saved projects) */
    const char *name;         /* the host's and the display's name */
    int lo, hi, def;
    Kind kind;
    const char *const *choice;
};

extern const ParamDef TONE[NTONE];
extern const ParamDef COMMON[NCOMMON];
const ParamDef &patchDef(int idx);
extern const char *const WAVE_NAME[NWAVE];
extern const char *const FMODE_NAME[NFMODE];
extern const char *const LSHAPE_NAME[NLSHAPE];
extern const char *const LFOSYNC_NAME[NLFOSYNC];
extern const float LFOSYNC_BEATS[NLFOSYNC];
extern const char *const GATERATE_NAME[NGATERATE];
extern const float GATERATE_BEATS[NGATERATE];

/* the value as the display shows it (a patch parameter, its value; synced: the LFO's or the delay's TEMPO SYNC) */
void paramText(int idx, int v, char *out, int n, bool synced = false);

/* the D BEAM's ASSIGN targets (manual p. 48), each a patch parameter of the tones (tone: -1 a common one) */
struct DBeamTarget {
    const char *name;
    int param;                /* ToneParam, or CommonParam when common; -1: BENDER / MODULATION (special) */
    bool common;
};
constexpr int NDBTARGET = 33;
extern const DBeamTarget DBTARGET[NDBTARGET];
extern const char *const DBTARGET_NAME[NDBTARGET];

/* --------------------------------------------------------- the TRANCE GATE --- */
constexpr int GATE_STEPS = 32;
constexpr uint8_t GATE_TIE = 0x80;               /* a step's flag: it carries on from the one before (no gap) */
struct GatePattern {
    const char *name;
    const char *steps;        /* a character a step: 'X' full, 'x' 70 %, 'o' 45 %, '_' 25 %, '=' a tie, '.' rest */
    int8_t rate;              /* GateRate */
    int8_t swing, att, rel;   /* 0..100, 0..127, 0..127 */
};
constexpr int NGATEPAT = 40;
extern const GatePattern GATEPAT[NGATEPAT];
extern const char *const GATEPAT_NAME[NGATEPAT + 1];   /* (and "User": the steps drawn by hand) */
/* a pattern's steps into a patch's (levels 0..100, GATE_TIE), its length */
int gatePatternSteps(int k, uint8_t *steps);

/* ---------------------------------------------------------- the ARPEGGIO --- */
constexpr int NARP = 64;
struct ArpPattern {
    char name[24];
    uint8_t order;            /* ArpOrder */
    uint8_t octaves;          /* 1..4 */
    uint8_t rate;             /* GateRate (the same note values) */
    uint8_t len;              /* steps in the rhythm */
    char rhythm[17];          /* 'X' accent, 'x' a note, '.' rest, '-' the note before held */
    uint8_t gate;             /* % of a step */
};
enum ArpOrder { AO_UP, AO_DOWN, AO_UPDOWN, AO_RANDOM, AO_PLAYED, AO_CHORD, AO_PEDAL, AO_BOUNCE, AO_OCTJUMP, AO_CONVERGE };
extern const ArpPattern ARP[NARP];

/* ---------------------------------------------------------------- a patch --- */
constexpr int NAME_LEN = 20;
struct Patch {
    char name[NAME_LEN];
    int16_t tone[3][NTONE];
    int16_t com[NCOMMON];
    uint8_t gate[GATE_STEPS];                 /* the TRANCE GATE's steps (levels 0..100, GATE_TIE): "User" */
};
void patchInit(Patch &p);                     /* the GAIA's INIT: tone 1 a sawtooth, open, the others off */
int patchGet(const Patch &p, int idx);
void patchSet(Patch &p, int idx, int v);

} // namespace gl
