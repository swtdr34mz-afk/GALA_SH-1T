/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
#include "Params.h"
#include <cstdio>
#include <cstring>

namespace gl
{
const char *const WAVE_NAME[NWAVE] = {"Saw", "Square", "Pulse/PWM", "Triangle", "Sine", "Noise", "Super Saw"};
const char *const FMODE_NAME[NFMODE] = {"LPF", "HPF", "BPF", "PKG", "Bypass"};
const char *const LSHAPE_NAME[NLSHAPE] = {"Triangle", "Sine", "Saw", "Square", "S&H", "Random"};
const char *const LFOSYNC_NAME[NLFOSYNC] = {"16",   "12",  "8",   "4",   "2",   "1",    "3/4", "2/3",  "1/2",  "3/8",
                                            "1/3",  "1/4", "3/16", "1/6", "1/8", "3/32", "1/12", "1/16", "1/24", "1/32"};
/* (the values are note lengths: 1 a whole note, four beats) */
const float LFOSYNC_BEATS[NLFOSYNC] = {64, 48, 32, 16, 8, 4, 3, 8.0f / 3.0f, 2, 1.5f, 4.0f / 3.0f, 1, 0.75f,
                                       4.0f / 6.0f, 0.5f, 0.375f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f};
const char *const GATERATE_NAME[NGATERATE] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
const float GATERATE_BEATS[NGATERATE] = {1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f};

static const char *const ONOFF[] = {"Off", "On"};
static const char *const VAR[] = {"1", "2", "3"};
static const char *const SLOPE[] = {"-12 dB", "-24 dB"};
static const char *const MODMODE[] = {"Off", "Sync", "Ring"};
static const char *const DIST[] = {"Off", "Dist", "Fuzz", "Bit Crash"};
static const char *const MODFX[] = {"Off", "Flanger", "Phaser", "Pitch Shifter"};
static const char *const DELAY[] = {"Off", "Delay", "Panning Delay"};
static const char *const RVTYPE[] = {"Room", "Plate", "Hall"};
static const char *const DBMODE[] = {"Off", "Pitch", "Volume", "Assign"};
static const char *const POL[] = {"+", "-"};
static const char *const DBTONES[] = {"All", "Tone 1", "Tone 2", "Tone 3"};
static const char *const PS2[] = {"-12", "-7", "-5", "-2", "-1", "0", "+1", "+2", "+5", "+7", "+12"};
static const char *const DLSYNC[] = {"1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T",
                                     "1/8.", "1/4",  "1/2T",  "1/4.", "1/2",  "1"};

const DBeamTarget DBTARGET[NDBTARGET] = {
    {"LFO Rate", T_LFO_RATE, false},      {"LFO Fade Time", T_LFO_FADE, false}, {"LFO Pitch Depth", T_LFO_PITCH, false},
    {"LFO Filter Depth", T_LFO_FILTER, false}, {"LFO Amp Depth", T_LFO_AMP, false}, {"Pitch", T_PITCH, false},
    {"Detune", T_DETUNE, false},          {"PWM", T_PWM, false},              {"PW", T_PW, false},
    {"Pitch Env A", T_PENV_A, false},     {"Pitch Env D", T_PENV_D, false},   {"Pitch Env Depth", T_PENV_DEPTH, false},
    {"Cutoff", T_CUTOFF, false},          {"Resonance", T_RESO, false},       {"Key Follow", T_KEYF, false},
    {"Filter Env A", T_FENV_A, false},    {"Filter Env D", T_FENV_D, false},  {"Filter Env S", T_FENV_S, false},
    {"Filter Env R", T_FENV_R, false},    {"Filter Env Depth", T_FENV_DEPTH, false}, {"Level", T_LEVEL, false},
    {"Amp Env A", T_AENV_A, false},       {"Amp Env D", T_AENV_D, false},     {"Amp Env S", T_AENV_S, false},
    {"Amp Env R", T_AENV_R, false},       {"Dist Drive", C_DIST_DRIVE, true}, {"Mod FX Level", C_FL_LEVEL, true},
    {"Delay Level", C_DL_LEVEL, true},    {"Reverb Level", C_RV_LEVEL, true}, {"Porta Time", C_PORTA_TIME, true},
    {"Gate Depth", C_GATE_DEPTH, true},   {"Bender", -1, true},               {"Modulation", -2, true},
};
const char *const DBTARGET_NAME[NDBTARGET] = {
    DBTARGET[0].name,  DBTARGET[1].name,  DBTARGET[2].name,  DBTARGET[3].name,  DBTARGET[4].name,  DBTARGET[5].name,
    DBTARGET[6].name,  DBTARGET[7].name,  DBTARGET[8].name,  DBTARGET[9].name,  DBTARGET[10].name, DBTARGET[11].name,
    DBTARGET[12].name, DBTARGET[13].name, DBTARGET[14].name, DBTARGET[15].name, DBTARGET[16].name, DBTARGET[17].name,
    DBTARGET[18].name, DBTARGET[19].name, DBTARGET[20].name, DBTARGET[21].name, DBTARGET[22].name, DBTARGET[23].name,
    DBTARGET[24].name, DBTARGET[25].name, DBTARGET[26].name, DBTARGET[27].name, DBTARGET[28].name, DBTARGET[29].name,
    DBTARGET[30].name, DBTARGET[31].name, DBTARGET[32].name};

#define N(id, name, lo, hi, def) {id, name, lo, hi, def, K_NUM, nullptr}
#define S(id, name, lo, hi, def) {id, name, lo, hi, def, K_SIGNED, nullptr}
#define C(id, name, n, def, list) {id, name, 0, (n) - 1, def, K_CHOICE, list}
#define B(id, name, def) {id, name, 0, 1, def, K_ONOFF, ONOFF}

const ParamDef TONE[NTONE] = {
    B("on", "Tone On", 1),
    C("wave", "OSC Wave", NWAVE, W_SAW, WAVE_NAME),
    C("variation", "OSC Variation", 3, 0, VAR),
    S("pitch", "OSC Pitch", -24, 24, 0),
    S("detune", "OSC Detune", -50, 50, 0),
    N("pwm", "OSC PWM", 0, 127, 0),
    N("pw", "OSC PW", 0, 127, 0),
    N("penv_a", "Pitch Env A", 0, 127, 0),
    N("penv_d", "Pitch Env D", 0, 127, 0),
    S("penv_depth", "Pitch Env Depth", -63, 63, 0),
    C("fmode", "Filter Mode", NFMODE, F_LPF, FMODE_NAME),
    C("slope", "Filter Slope", 2, 1, SLOPE),
    N("cutoff", "Cutoff", 0, 127, 127),
    N("reso", "Resonance", 0, 127, 0),
    S("keyfollow", "Key Follow", -100, 100, 0),
    N("fenv_a", "Filter Env A", 0, 127, 0),
    N("fenv_d", "Filter Env D", 0, 127, 50),
    N("fenv_s", "Filter Env S", 0, 127, 0),
    N("fenv_r", "Filter Env R", 0, 127, 40),
    S("fenv_depth", "Filter Env Depth", -63, 63, 0),
    N("level", "Amp Level", 0, 127, 100),
    N("aenv_a", "Amp Env A", 0, 127, 0),
    N("aenv_d", "Amp Env D", 0, 127, 0),
    N("aenv_s", "Amp Env S", 0, 127, 127),
    N("aenv_r", "Amp Env R", 0, 127, 15),
    S("pan", "Pan", -63, 63, 0),
    C("lfo_shape", "LFO Shape", NLSHAPE, LS_TRI, LSHAPE_NAME),
    B("lfo_sync", "LFO Tempo Sync", 0),
    {"lfo_rate", "LFO Rate", 0, 127, 64, K_LFORATE, nullptr},
    N("lfo_fade", "LFO Fade Time", 0, 127, 0),
    S("lfo_pitch", "LFO Pitch Depth", -63, 63, 0),
    S("lfo_filter", "LFO Filter Depth", -63, 63, 0),
    S("lfo_amp", "LFO Amp Depth", -63, 63, 0),
    S("lfo_pan", "LFO Pan Depth", -63, 63, 0),
    C("mlfo_shape", "Mod LFO Shape", NLSHAPE, LS_TRI, LSHAPE_NAME),
    B("mlfo_sync", "Mod LFO Tempo Sync", 0),
    {"mlfo_rate", "Mod LFO Rate", 0, 127, 82, K_LFORATE, nullptr},
    S("mlfo_pitch", "Mod LFO Pitch Depth", -63, 63, 12),
    S("mlfo_filter", "Mod LFO Filter Depth", -63, 63, 0),
    S("mlfo_amp", "Mod LFO Amp Depth", -63, 63, 0),
    S("mlfo_pan", "Mod LFO Pan Depth", -63, 63, 0),
};

const ParamDef COMMON[NCOMMON] = {
    C("mod", "Sync/Ring", 3, MM_OFF, MODMODE),
    B("mono", "Mono", 0),
    B("legato", "Legato", 1),
    B("porta", "Portamento", 0),
    N("porta_time", "Portamento Time", 0, 127, 40),
    S("octave", "Patch Octave", -3, 3, 0),
    N("bend_up", "Bend Range Up", 0, 24, 2),
    N("bend_down", "Bend Range Down", 0, 24, 2),
    N("tempo", "Patch Tempo", 5, 300, 138),
    B("lfo_keytrig", "LFO Key Trigger", 0),
    N("fenv_vel", "Filter Env Velocity", 0, 63, 0),
    N("level_vel", "Level Velocity", 0, 63, 20),
    N("patch_level", "Patch Level", 0, 127, 100),
    B("arp_on", "Arpeggio", 0),
    {"arp_var", "Arpeggio Variation", 0, NARP - 1, 0, K_ARP, nullptr},
    B("fx_on", "Effects On", 1),
    C("dist", "Dist Block", 4, DT_OFF, DIST),
    N("dist_drive", "Dist Drive", 0, 127, 64),
    N("dist_type", "Dist Type", 1, 6, 1),
    N("dist_pres", "Dist Presence", 0, 127, 64),
    N("dist_level", "Dist Level", 0, 127, 90),
    N("fuzz_drive", "Fuzz Drive", 0, 127, 64),
    N("fuzz_type", "Fuzz Type", 1, 6, 1),
    N("fuzz_pres", "Fuzz Presence", 0, 127, 64),
    N("fuzz_level", "Fuzz Level", 0, 127, 90),
    N("bc_rate", "Bit Crash Rate", 0, 127, 60),
    N("bc_bits", "Bit Crash Bit Down", 0, 127, 60),
    N("bc_filter", "Bit Crash Filter", 0, 127, 40),
    N("bc_level", "Bit Crash Level", 0, 127, 100),
    C("modfx", "Flanger Block", 4, MF_OFF, MODFX),
    N("fl_fb", "Flanger Feedback", 0, 127, 70),
    N("fl_depth", "Flanger Depth", 0, 127, 70),
    N("fl_rate", "Flanger Rate", 0, 127, 25),
    N("fl_level", "Flanger Level", 0, 127, 90),
    N("ph_reso", "Phaser Resonance", 0, 127, 70),
    N("ph_depth", "Phaser Depth", 0, 127, 90),
    N("ph_rate", "Phaser Rate", 0, 127, 25),
    N("ph_level", "Phaser Level", 0, 127, 100),
    S("ps_pitch", "Pitch Shifter Pitch", -12, 12, 12),
    N("ps_detune", "Pitch Shifter Detune", 0, 50, 0),
    C("ps_pitch2", "Pitch Shifter Pitch 2", 11, 5, PS2),
    N("ps_level", "Pitch Shifter Level", 0, 127, 64),
    C("delay", "Delay Block", 3, DL_OFF, DELAY),
    B("delay_sync", "Delay Tempo Sync", 1),
    {"dl_time", "Delay Time", 0, 127, 80, K_DLTIME, nullptr},
    N("dl_fb", "Delay Feedback", 0, 127, 50),
    {"dl_damp", "Delay High Damp", -36, 0, -12, K_DB, nullptr},
    N("dl_level", "Delay Level", 0, 127, 50),
    {"pd_time", "Panning Delay Time", 0, 127, 80, K_DLTIME, nullptr},
    N("pd_fb", "Panning Delay Feedback", 0, 127, 50),
    {"pd_damp", "Panning Delay High Damp", -36, 0, -12, K_DB, nullptr},
    N("pd_level", "Panning Delay Level", 0, 127, 50),
    B("reverb", "Reverb", 0),
    N("rv_time", "Reverb Time", 0, 127, 70),
    C("rv_type", "Reverb Type", 3, RT_HALL, RVTYPE),
    N("rv_damp", "Reverb High Damp", 1, 100, 50),
    N("rv_level", "Reverb Level", 0, 127, 50),
    B("lowboost", "Low Boost", 0),
    N("lb_amount", "Low Boost Amount", 0, 127, 70),
    B("gate_on", "Trance Gate", 0),
    C("gate_pattern", "Gate Pattern", NGATEPAT + 1, 0, GATEPAT_NAME),
    C("gate_rate", "Gate Rate", NGATERATE, GR_16, GATERATE_NAME),
    N("gate_len", "Gate Length", 1, GATE_STEPS, 16),
    N("gate_swing", "Gate Swing", 0, 100, 0),
    N("gate_att", "Gate Attack", 0, 127, 4),
    N("gate_rel", "Gate Release", 0, 127, 24),
    N("gate_depth", "Gate Depth", 0, 127, 127),
    S("gate_filter", "Gate Filter", -63, 63, 0),
    B("gate_t1", "Gate Tone 1", 1),
    B("gate_t2", "Gate Tone 2", 1),
    B("gate_t3", "Gate Tone 3", 1),
    C("db_mode", "D Beam", 4, DB_OFF, DBMODE),
    C("db_assign", "D Beam Assign", NDBTARGET, 12, DBTARGET_NAME),
    C("db_polarity", "D Beam Polarity", 2, 0, POL),
    C("db_tones", "D Beam Tones", 4, 0, DBTONES),
    B("cmod", "Circuit Mod", 0),
    N("cmod_age", "Circuit Mod Age", 0, 127, 70),
    N("cmod_drift", "Circuit Mod Drift", 0, 127, 50),
    N("cmod_noise", "Circuit Mod Noise", 0, 127, 30),
};

const ParamDef &patchDef(int idx) { return idx < 3 * NTONE ? TONE[idx % NTONE] : COMMON[idx - 3 * NTONE]; }

/* ------------------------------------------------------------ the TRANCE GATE --- */
const GatePattern GATEPAT[NGATEPAT] = {
    {"Classic 16ths", "XxXxXxXxXxXxXxXx", GR_16, 0, 2, 22},
    {"Straight 16ths", "xxxxxxxxxxxxxxxx", GR_16, 0, 2, 30},
    {"Offbeat 8ths", ".X.X.X.X", GR_8, 0, 2, 30},
    {"Sidechain Pump", "XXXX", GR_4, 0, 110, 4},
    {"Gallop", "X.xxX.xxX.xxX.xx", GR_16, 0, 2, 24},
    {"Reverse Gallop", "xxX.xxX.xxX.xxX.", GR_16, 0, 2, 24},
    {"Triplet Roll", "XxxXxxXxxXxx", GR_16T, 0, 2, 24},
    {"Uplifter 32", "x.x.x.x.xxxxxxxxXxXxXxXxXXXXXXXX", GR_32, 0, 1, 20},
    {"Stutter", "XXXX....XX..X...", GR_16, 0, 1, 18},
    {"Psy Roll", ".XXX.XXX.XXX.XXX", GR_16, 0, 2, 34},
    {"Euro Chop", "X.X.XX.X.XX.X.X.", GR_16, 0, 2, 26},
    {"Halftime", "X.......X...X...", GR_16, 0, 3, 60},
    {"Morse", "X.X.XXX.X...XXX.", GR_16, 0, 2, 24},
    {"Broken Beat", "X..X..X...X..X..", GR_16, 0, 2, 40},
    {"Syncopated", ".X..X.X..X..X.X.", GR_16, 0, 2, 30},
    {"3-3-2", "X..X..X.X..X..X.", GR_16, 0, 2, 50},
    {"Dotted 8ths", "X==X==X==X==X==X", GR_16, 0, 2, 40},
    {"Vocal Chop", "XX.X..XX.X..X.XX", GR_16, 0, 2, 20},
    {"Build-Up", "X...X...X.X.X.X.XXXXXXXXXXXXXXXX", GR_16, 0, 2, 22},
    {"Ghost Notes", "Xo_oXo_oXo_oXo_o", GR_16, 0, 2, 30},
    {"Rolling Shutter", "XxoxXxoxXxoxXxox", GR_16, 0, 2, 26},
    {"Stab Rhythm", "X..X..X.......X.", GR_16, 0, 1, 18},
    {"Pulse 8ths", "XXXXXXXX", GR_8, 0, 1, 32},
    {"Ratchet", "X...X...X...XXXXX...X...X...XXXX", GR_32, 0, 1, 24},
    {"Anthem", "X==.X==.X=.XX=.X", GR_16, 0, 2, 30},
    {"Bounce", "X.xX.xX.xX.xX.xx", GR_16, 0, 2, 26},
    {"Pad Breather", "X=X=X=X=", GR_8, 0, 70, 70},
    {"Glitch", "XX.X.XXX..X.XX.XX.XX.X..XXX.X.XX", GR_32, 0, 1, 16},
    {"Trancewave", "X=x.X=x.X=x.X=xx", GR_16, 0, 2, 28},
    {"Hypnotic", "X.xxX.x.X.xxX.x.", GR_16, 0, 2, 26},
    {"Swing 16ths", "XxXxXxXxXxXxXxXx", GR_16, 60, 2, 24},
    {"Swing Offbeat", ".X.X.X.XxX.X.X.X", GR_16, 55, 2, 28},
    {"Triplet Gate", "X.XX.XX.XX.X", GR_16T, 0, 2, 24},
    {"Long Short", "X=.XX=.XX=.XX=.X", GR_16, 0, 2, 28},
    {"Acid Pattern", "XxX.xX.XxX.xX.Xx", GR_16, 0, 2, 22},
    {"Quarter Chop", "XXXX", GR_4, 0, 2, 40},
    {"Eighth Chop", "XXXXXXXX", GR_8, 0, 2, 50},
    {"Double Time", "XxXxXxXxXxXxXxXxXxXxXxXxXxXxXxXx", GR_32, 0, 1, 26},
    {"Fade In Beats", "_oxX_oxX_oxX_oxX", GR_16, 0, 2, 26},
    {"Open (No Gate)", "X===============", GR_16, 0, 0, 0},
};
const char *const GATEPAT_NAME[NGATEPAT + 1] = {
    GATEPAT[0].name,  GATEPAT[1].name,  GATEPAT[2].name,  GATEPAT[3].name,  GATEPAT[4].name,  GATEPAT[5].name,
    GATEPAT[6].name,  GATEPAT[7].name,  GATEPAT[8].name,  GATEPAT[9].name,  GATEPAT[10].name, GATEPAT[11].name,
    GATEPAT[12].name, GATEPAT[13].name, GATEPAT[14].name, GATEPAT[15].name, GATEPAT[16].name, GATEPAT[17].name,
    GATEPAT[18].name, GATEPAT[19].name, GATEPAT[20].name, GATEPAT[21].name, GATEPAT[22].name, GATEPAT[23].name,
    GATEPAT[24].name, GATEPAT[25].name, GATEPAT[26].name, GATEPAT[27].name, GATEPAT[28].name, GATEPAT[29].name,
    GATEPAT[30].name, GATEPAT[31].name, GATEPAT[32].name, GATEPAT[33].name, GATEPAT[34].name, GATEPAT[35].name,
    GATEPAT[36].name, GATEPAT[37].name, GATEPAT[38].name, GATEPAT[39].name, "User"};

int gatePatternSteps(int k, uint8_t *steps)
{
    std::memset(steps, 0, GATE_STEPS);
    if (k < 0 || k >= NGATEPAT)
        return 16;
    const char *s = GATEPAT[k].steps;
    int n = 0;
    for (; s[n] && n < GATE_STEPS; n++) {
        switch (s[n]) {
        case 'X': steps[n] = 100; break;
        case 'x': steps[n] = 70; break;
        case 'o': steps[n] = 45; break;
        case '_': steps[n] = 25; break;
        case '=': steps[n] = (uint8_t)((n > 0 ? steps[n - 1] & 0x7f : 100) | GATE_TIE); break;
        default: steps[n] = 0;
        }
    }
    return n < 1 ? 1 : n;
}

/* ---------------------------------------------------------------- ARPEGGIO --- */
/* eight banks of eight, as the GAIA's (A1 .. H8): up, down, up and down, random, chords, trance, bass, as played */
#define ALL16 "xxxxxxxxxxxxxxxx"
const ArpPattern ARP[NARP] = {
    {"Up 1/16", AO_UP, 1, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Up 2 Oct", AO_UP, 2, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Up 3 Oct", AO_UP, 3, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Up 1/8", AO_UP, 1, GR_8, 8, "Xxxxxxxx", 70},
    {"Up 1/32", AO_UP, 2, GR_32, 16, ALL16, 55},
    {"Up Triplets", AO_UP, 2, GR_16T, 12, "Xxxxxxxxxxxx", 60},
    {"Up Staccato", AO_UP, 2, GR_16, 16, "Xxxxxxxxxxxxxxxx", 22},
    {"Up 4 Oct", AO_UP, 4, GR_16, 16, ALL16, 55},
    {"Down 1/16", AO_DOWN, 1, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Down 2 Oct", AO_DOWN, 2, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Down 3 Oct", AO_DOWN, 3, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Down 1/8", AO_DOWN, 1, GR_8, 8, "Xxxxxxxx", 70},
    {"Down 1/32", AO_DOWN, 2, GR_32, 16, ALL16, 55},
    {"Down Triplets", AO_DOWN, 2, GR_16T, 12, "Xxxxxxxxxxxx", 60},
    {"Down Staccato", AO_DOWN, 2, GR_16, 16, "Xxxxxxxxxxxxxxxx", 22},
    {"Down 4 Oct", AO_DOWN, 4, GR_16, 16, ALL16, 55},
    {"Up+Down 1/16", AO_UPDOWN, 1, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Up+Down 2 Oct", AO_UPDOWN, 2, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Up+Down 3 Oct", AO_UPDOWN, 3, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Up+Down 1/8", AO_UPDOWN, 2, GR_8, 8, "Xxxxxxxx", 70},
    {"Up+Down 1/32", AO_UPDOWN, 2, GR_32, 16, ALL16, 55},
    {"Up+Down Trip", AO_UPDOWN, 2, GR_16T, 12, "Xxxxxxxxxxxx", 60},
    {"Converge", AO_CONVERGE, 1, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Converge 2 Oct", AO_CONVERGE, 2, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"Random 1/16", AO_RANDOM, 1, GR_16, 16, ALL16, 60},
    {"Random 2 Oct", AO_RANDOM, 2, GR_16, 16, ALL16, 60},
    {"Random 1/8", AO_RANDOM, 2, GR_8, 8, "xxxxxxxx", 70},
    {"Random 1/32", AO_RANDOM, 2, GR_32, 16, ALL16, 50},
    {"Random Gaps", AO_RANDOM, 2, GR_16, 16, "x.xxx.x.xx.xxx.x", 55},
    {"Random Trip", AO_RANDOM, 2, GR_16T, 12, "xxxxxxxxxxxx", 60},
    {"Random Stacc", AO_RANDOM, 3, GR_16, 16, ALL16, 20},
    {"Random 3 Oct", AO_RANDOM, 3, GR_16, 16, ALL16, 60},
    {"Chord 1/4", AO_CHORD, 1, GR_16, 16, "X...x...x...x...", 50},
    {"Chord 1/8", AO_CHORD, 1, GR_8, 8, "Xxxxxxxx", 50},
    {"Chord Offbeat", AO_CHORD, 1, GR_16, 16, "..X...x...x...x.", 50},
    {"Chord Stabs", AO_CHORD, 1, GR_16, 16, "X..x..x...x..x..", 35},
    {"Chord 1/16", AO_CHORD, 1, GR_16, 16, "XxxxXxxxXxxxXxxx", 40},
    {"Chord Gallop", AO_CHORD, 1, GR_16, 16, "X.xxx.xxx.xxx.xx", 40},
    {"Chord 3-3-2", AO_CHORD, 1, GR_16, 16, "X..x..x.x..x..x.", 45},
    {"Chord Push", AO_CHORD, 1, GR_16, 16, "x.x..x.x..x.x..X", 45},
    {"Trance Pedal", AO_PEDAL, 1, GR_16, 16, "Xxxxxxxxxxxxxxxx", 55},
    {"Pedal 2 Oct", AO_PEDAL, 2, GR_16, 16, "Xxxxxxxxxxxxxxxx", 55},
    {"Trance Bounce", AO_BOUNCE, 2, GR_16, 16, "Xxxxxxxxxxxxxxxx", 55},
    {"Octave Jump", AO_OCTJUMP, 1, GR_16, 16, "Xxxxxxxxxxxxxxxx", 50},
    {"Trance Gated", AO_UP, 2, GR_16, 16, "XX.xx.xx.xx.xxxx", 45},
    {"Offbeat Up", AO_UP, 2, GR_16, 16, ".x.x.x.x.x.x.x.x", 55},
    {"Anthem Arp", AO_UPDOWN, 2, GR_16, 16, "X-xxX-xxX-xxX-xx", 60},
    {"Pedal 1/32", AO_PEDAL, 1, GR_32, 16, ALL16, 50},
    {"Rolling Bass", AO_UP, 1, GR_16, 16, ".xxx.xxx.xxx.xxx", 55},
    {"Offbeat Bass", AO_UP, 1, GR_16, 16, "..x...x...x...x.", 50},
    {"Gallop Bass", AO_UP, 1, GR_16, 16, "x.xxx.xxx.xxx.xx", 50},
    {"Octave Bass", AO_OCTJUMP, 1, GR_16, 16, "Xxxxxxxxxxxxxxxx", 45},
    {"Psy Bass", AO_UP, 1, GR_16, 16, ".xxx.xxx.xxx.xxx", 30},
    {"Oct Offbeat", AO_OCTJUMP, 1, GR_16, 16, ".x.x.x.x.x.x.x.x", 45},
    {"Bounce Bass", AO_BOUNCE, 1, GR_16, 16, "x.xx.xx.x.xx.xx.", 50},
    {"Synco Bass", AO_PLAYED, 1, GR_16, 16, "x..x..x..x..x.x.", 50},
    {"As Played 1/16", AO_PLAYED, 1, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"As Played 1/8", AO_PLAYED, 1, GR_8, 8, "Xxxxxxxx", 70},
    {"As Played 2 Oct", AO_PLAYED, 2, GR_16, 16, "Xxxxxxxxxxxxxxxx", 60},
    {"As Played Trip", AO_PLAYED, 1, GR_16T, 12, "Xxxxxxxxxxxx", 60},
    {"As Played Gaps", AO_PLAYED, 1, GR_16, 16, "xx.x.xx.xx.x.xx.", 55},
    {"As Played Legato", AO_PLAYED, 1, GR_16, 16, "X-x-x-x-X-x-x-x-", 100},
    {"As Played 1/32", AO_PLAYED, 2, GR_32, 16, ALL16, 55},
    {"As Played Synco", AO_PLAYED, 1, GR_16, 16, "x.x..x.x.x..x.x.", 55},
};

/* ---------------------------------------------------------------- texts --- */
void paramText(int idx, int v, char *out, int n, bool synced)
{
    const ParamDef &d = patchDef(idx);
    switch (d.kind) {
    case K_SIGNED: std::snprintf(out, (size_t)n, v > 0 ? "+%d" : "%d", v); break;
    case K_CHOICE:
    case K_ONOFF: std::snprintf(out, (size_t)n, "%s", d.choice[v < d.lo ? d.lo : v > d.hi ? d.hi : v]); break;
    case K_LFORATE:
        if (synced)
            std::snprintf(out, (size_t)n, "%s", LFOSYNC_NAME[v * NLFOSYNC / 128]);
        else
            std::snprintf(out, (size_t)n, "%d", v);
        break;
    case K_DLTIME:
        if (synced)
            std::snprintf(out, (size_t)n, "%s", DLSYNC[v * 13 / 128]);
        else {
            /* (the engine's: 1 ms .. 1.3 s) */
            float ms = 1.0f + 1299.0f * (float)(v * v) / (127.0f * 127.0f);
            std::snprintf(out, (size_t)n, ms < 100.0f ? "%.1f ms" : "%.0f ms", ms);
        }
        break;
    case K_ARP: std::snprintf(out, (size_t)n, "%c%d %s", 'A' + v / 8, v % 8 + 1, ARP[v < 0 ? 0 : v >= NARP ? NARP - 1 : v].name); break;
    case K_DB: std::snprintf(out, (size_t)n, "%d dB", v); break;
    default: std::snprintf(out, (size_t)n, "%d", v);
    }
}

/* ---------------------------------------------------------------- a patch --- */
void patchInit(Patch &p)
{
    std::memset(&p, 0, sizeof p);
    std::snprintf(p.name, NAME_LEN, "INIT PATCH");
    for (int t = 0; t < 3; t++)
        for (int i = 0; i < NTONE; i++)
            p.tone[t][i] = (int16_t)TONE[i].def;
    p.tone[1][T_ON] = 0;
    p.tone[2][T_ON] = 0;
    for (int i = 0; i < NCOMMON; i++)
        p.com[i] = (int16_t)COMMON[i].def;
    gatePatternSteps(0, p.gate);
}
int patchGet(const Patch &p, int idx) { return idx < 3 * NTONE ? p.tone[idx / NTONE][idx % NTONE] : p.com[idx - 3 * NTONE]; }
void patchSet(Patch &p, int idx, int v)
{
    const ParamDef &d = patchDef(idx);
    v = v < d.lo ? d.lo : v > d.hi ? d.hi : v;
    if (idx < 3 * NTONE)
        p.tone[idx / NTONE][idx % NTONE] = (int16_t)v;
    else
        p.com[idx - 3 * NTONE] = (int16_t)v;
}
} // namespace gl
