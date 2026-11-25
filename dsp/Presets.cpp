/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
/* The factory patches, each what it changes from the INIT PATCH, by the parameters' ids:
 *   "T: ..."   all three tones     "T1: ...", "T2: ...", "T3: ..."  one tone     "C: ..."  the patch's own
 * (tones 2 and 3 are off in the INIT PATCH: "on=1" switches one on). Choices by their number (Params.h). */
#include "Presets.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace gl
{
const char *const BANK_NAME[8] = {"Leads", "Super Saws", "Pads", "Basses", "Plucks & Arps", "Trance Gate",
                                  "Keys & Bells", "FX & Retro"};

namespace
{
/* delay times synced (dl_time / pd_time): 1/16 20, 1/8T 30, 1/8 50, 1/8. 69, 1/4 79, 1/4. 99 */
struct Def {
    const char *name, *text;
};
const Def DEFS[NFACTORY] = {
    /* ----------------------------------------------------------------- A: LEADS */
    {"Gaia Trance Lead", "T: cutoff=96 reso=18 fenv_depth=16 fenv_d=72 fenv_s=70 fenv_r=60 aenv_r=52 |"
                         "T1: wave=6 variation=1 | T2: on=1 wave=0 variation=1 pitch=12 detune=6 level=72 |"
                         "C: delay=2 pd_time=69 pd_fb=55 pd_level=46 reverb=1 rv_level=42"},
    {"Sync Scream", "T1: wave=0 pitch=14 penv_depth=34 penv_d=78 cutoff=104 fenv_depth=8 aenv_r=40 lfo_pitch=0 |"
                    "T2: wave=0 on=0 | C: mod=1 mono=1 porta=1 porta_time=34 dist=1 dist_drive=46 dist_level=86 "
                    "delay=1 dl_time=69 dl_level=34"},
    {"Mono Glide Lead", "T: cutoff=84 reso=40 fenv_depth=24 fenv_d=62 fenv_s=48 aenv_r=34 |"
                        "T1: wave=0 variation=2 | T2: on=1 wave=1 pitch=-12 level=80 |"
                        "C: mono=1 porta=1 porta_time=42 delay=1 dl_time=69 dl_level=30 reverb=1 rv_level=32"},
    {"Euphoria Lead", "T: cutoff=106 fenv_depth=10 fenv_d=80 fenv_s=90 aenv_r=60 |"
                      "T1: wave=6 variation=2 | T2: on=1 wave=6 variation=1 pitch=12 level=74 |"
                      "T3: on=1 wave=4 pitch=24 level=50 | C: reverb=1 rv_level=56 rv_time=82 delay=2 pd_time=50 "
                      "pd_level=40 patch_level=92"},
    {"Square Lead", "T1: wave=1 variation=1 cutoff=92 fenv_depth=12 fenv_d=70 fenv_s=60 aenv_r=36 |"
                    "T2: on=1 wave=2 pw=58 pwm=34 lfo_rate=50 detune=8 level=84 cutoff=96 |"
                    "C: mono=1 porta=1 porta_time=30 delay=1 dl_time=69 dl_level=36 reverb=1 rv_level=30"},
    {"Ring Lead", "T1: wave=0 cutoff=100 fenv_depth=12 fenv_d=66 fenv_s=60 aenv_r=44 | T2: wave=4 pitch=7 |"
                  "C: mod=2 delay=2 pd_time=69 pd_level=40 reverb=1 rv_level=36"},
    {"Acid Lead", "T1: wave=0 cutoff=44 reso=104 fenv_depth=40 fenv_d=56 fenv_s=10 aenv_r=24 |"
                  "C: mono=1 legato=0 porta=1 porta_time=20 dist=1 dist_drive=40 dist_level=86 delay=1 dl_time=69 "
                  "dl_level=36 dl_fb=60"},
    {"Vocal Lead", "T1: wave=0 variation=1 | T2: on=1 wave=2 pw=40 detune=-7 level=82 |"
                   "T: fmode=3 cutoff=70 reso=92 lfo_filter=22 lfo_rate=42 aenv_r=44 |"
                   "C: patch_level=72 reverb=1 rv_type=1 rv_level=40 delay=1 dl_time=69 dl_level=26"},
    /* ----------------------------------------------------------------- B: SUPER SAWS */
    {"Supersaw Anthem", "T: cutoff=110 aenv_r=56 | T1: wave=6 variation=2 |"
                        "T2: on=1 wave=6 variation=1 pitch=12 level=80 | T3: on=1 wave=6 pitch=-12 level=70 |"
                        "C: reverb=1 rv_level=50 rv_time=82 delay=2 pd_time=79 pd_level=32 patch_level=86"},
    {"Trance Stab", "T: cutoff=70 fenv_depth=34 fenv_d=52 fenv_s=0 fenv_r=50 aenv_d=72 aenv_s=0 aenv_r=50 |"
                    "T1: wave=6 variation=2 | T2: on=1 wave=0 variation=1 pitch=12 level=76 |"
                    "C: reverb=1 rv_level=46 delay=2 pd_time=50 pd_level=40"},
    {"Hands Up Stab", "T: cutoff=80 fenv_depth=30 fenv_d=60 fenv_s=24 aenv_d=76 aenv_s=36 aenv_r=46 |"
                      "T1: wave=6 variation=1 | T2: on=1 wave=1 variation=1 pitch=12 level=70 |"
                      "C: reverb=1 rv_type=1 rv_level=40 delay=2 pd_time=69 pd_level=40"},
    {"Chord Pluck", "T: cutoff=60 reso=24 fenv_depth=38 fenv_d=52 fenv_s=0 aenv_d=62 aenv_s=0 aenv_r=52 |"
                    "T1: wave=6 variation=1 | T2: on=1 wave=0 variation=1 detune=10 level=80 |"
                    "C: delay=1 dl_time=69 dl_level=40 reverb=1 rv_level=40"},
    {"Detuned Wall", "T: wave=0 variation=1 cutoff=100 aenv_a=40 aenv_r=70 | T1: detune=-12 |"
                     "T2: on=1 pitch=12 detune=12 level=80 | T3: on=1 pitch=-12 detune=4 level=80 |"
                     "C: reverb=1 rv_level=48 patch_level=88"},
    {"Supersaw Lift", "T1: wave=6 variation=2 fmode=1 cutoff=40 lfo_sync=1 lfo_rate=20 lfo_shape=2 lfo_filter=48 "
                      "aenv_r=60 | T2: on=1 wave=6 pitch=12 level=70 fmode=1 cutoff=50 |"
                      "C: delay=2 pd_time=50 pd_level=42 reverb=1 rv_level=56"},
    {"Gated Supersaw", "T1: wave=6 variation=2 cutoff=104 aenv_r=40 | T2: on=1 wave=0 variation=1 pitch=12 level=70 |"
                       "C: gate_on=1 gate_pattern=0 gate_rate=3 reverb=1 rv_level=46 delay=2 pd_time=69 pd_level=30"},
    {"Saw Brass", "T: wave=0 variation=1 cutoff=60 fenv_depth=30 fenv_a=50 fenv_d=72 fenv_s=64 aenv_a=34 aenv_r=44 |"
                  "T2: on=1 detune=9 level=86 | C: reverb=1 rv_level=36 level_vel=36 fenv_vel=30"},
    /* ----------------------------------------------------------------- C: PADS */
    {"Gaia Strings", "T: cutoff=90 aenv_a=86 aenv_r=96 | T1: wave=6 variation=1 |"
                     "T2: on=1 wave=0 variation=1 pitch=12 detune=5 level=70 |"
                     "C: modfx=1 fl_fb=24 fl_depth=50 fl_rate=20 fl_level=56 reverb=1 rv_level=60 rv_time=90"},
    {"Dream Pad", "T: cutoff=76 fenv_depth=16 fenv_a=100 fenv_d=110 fenv_s=80 aenv_a=96 aenv_r=104 lfo_filter=10 lfo_rate=30 |"
                  "T1: wave=0 variation=1 | T2: on=1 wave=3 variation=2 pitch=12 level=76 |"
                  "T3: on=1 wave=4 pitch=-12 level=70 | C: reverb=1 rv_level=66 rv_time=100 delay=2 pd_time=79 pd_level=30"},
    {"PWM Pad", "T: wave=2 cutoff=86 aenv_a=80 aenv_r=96 | T1: pw=20 pwm=70 lfo_rate=45 |"
                "T2: on=1 pw=10 pwm=60 lfo_rate=52 detune=8 pitch=12 level=72 | C: patch_level=80 reverb=1 rv_level=56 rv_time=86"},
    {"Glass Pad", "T: aenv_a=90 aenv_r=100 | T1: wave=4 variation=2 | T2: on=1 wave=3 pitch=19 level=52 |"
                  "T3: on=1 wave=6 level=48 fmode=1 cutoff=70 | C: reverb=1 rv_type=1 rv_level=60 delay=2 pd_time=79 pd_level=34"},
    {"Sweep Pad", "T1: wave=6 variation=2 cutoff=50 reso=62 lfo_sync=1 lfo_rate=20 lfo_filter=46 aenv_a=86 aenv_r=100 |"
                  "T2: on=1 wave=0 variation=1 pitch=-12 cutoff=56 reso=40 level=70 lfo_sync=1 lfo_rate=20 lfo_filter=40 aenv_a=86 aenv_r=100 |"
                  "C: reverb=1 rv_level=60 rv_time=96"},
    {"Phaser Pad", "T: wave=0 variation=1 cutoff=88 aenv_a=76 aenv_r=94 | T2: on=1 detune=-9 pitch=-12 level=80 |"
                   "C: modfx=2 ph_depth=96 ph_rate=18 ph_reso=70 reverb=1 rv_level=54"},
    {"Choir", "T: fmode=3 cutoff=72 reso=74 aenv_a=76 aenv_r=96 mlfo_pitch=10 | T1: wave=0 variation=1 |"
              "T2: on=1 wave=1 pitch=12 level=66 | C: patch_level=72 reverb=1 rv_level=62 rv_time=96"},
    {"Breathing Pad", "T: aenv_a=70 aenv_r=90 cutoff=96 | T1: wave=6 variation=1 | T2: on=1 wave=3 pitch=12 level=66 |"
                      "C: gate_on=1 gate_pattern=26 gate_rate=1 gate_att=70 gate_rel=70 reverb=1 rv_level=58"},
    /* ----------------------------------------------------------------- D: BASSES */
    {"Trance Bass", "T: cutoff=56 reso=30 fenv_depth=34 fenv_d=50 fenv_s=10 aenv_d=60 aenv_s=84 aenv_r=24 |"
                    "T1: wave=0 variation=2 | T2: on=1 wave=1 pitch=-12 level=80 | C: octave=-1 level_vel=30"},
    {"Rolling Bass", "T1: wave=0 variation=1 cutoff=50 reso=24 fenv_depth=40 fenv_d=46 fenv_s=0 aenv_d=52 aenv_s=0 aenv_r=30 |"
                     "C: patch_level=127 arp_on=1 arp_var=48 octave=-1"},
    {"Psy Bass", "T1: wave=0 cutoff=44 reso=52 fenv_depth=42 fenv_d=40 fenv_s=0 aenv_d=46 aenv_s=0 aenv_r=20 |"
                 "C: patch_level=127 arp_on=1 arp_var=52 octave=-1"},
    {"Reese Bass", "T: wave=0 variation=1 cutoff=60 reso=20 aenv_r=30 | T2: on=1 detune=22 |"
                   "C: patch_level=80 mono=1 octave=-1 dist=1 dist_drive=30 dist_level=86"},
    {"Sub Sine Bass", "T1: wave=4 | T2: on=1 wave=3 pitch=12 level=46 | T: aenv_r=24 |"
                      "C: mono=1 porta=1 porta_time=26 octave=-1 lowboost=1 lb_amount=60"},
    {"Acid Bass", "T1: wave=0 cutoff=34 reso=112 fenv_depth=46 fenv_d=50 fenv_s=0 aenv_r=20 |"
                  "C: mono=1 legato=0 porta=1 porta_time=16 octave=-1 dist=1 dist_drive=36 dist_level=84"},
    {"Octave Bass", "T1: wave=2 pw=40 cutoff=60 fenv_depth=30 fenv_d=46 fenv_s=0 aenv_d=50 aenv_s=0 aenv_r=24 |"
                    "C: patch_level=127 arp_on=1 arp_var=51 octave=-1"},
    {"Fuzz Bass", "T1: wave=1 variation=1 cutoff=70 aenv_r=24 | C: mono=1 octave=-1 dist=2 fuzz_drive=50 fuzz_level=50"},
    /* ----------------------------------------------------------------- E: PLUCKS & ARPS */
    {"Trance Pluck", "T: cutoff=50 fenv_depth=46 fenv_d=50 fenv_s=0 fenv_r=50 aenv_d=62 aenv_s=0 aenv_r=50 |"
                     "T1: wave=6 variation=1 | T2: on=1 wave=0 variation=1 pitch=12 level=70 |"
                     "C: patch_level=127 delay=2 pd_time=69 pd_level=46 reverb=1 rv_level=46 level_vel=30"},
    {"Up Arp Pluck", "T: cutoff=56 fenv_depth=40 fenv_d=48 fenv_s=0 aenv_d=56 aenv_s=0 aenv_r=44 |"
                     "T1: wave=6 variation=1 | C: patch_level=127 arp_on=1 arp_var=1 delay=2 pd_time=69 pd_level=40 reverb=1 rv_level=40"},
    {"Bounce Arp", "T: cutoff=60 fenv_depth=36 fenv_d=50 fenv_s=0 aenv_d=56 aenv_s=0 aenv_r=40 |"
                   "T1: wave=0 variation=1 | T2: on=1 wave=1 pitch=12 level=60 |"
                   "C: patch_level=127 arp_on=1 arp_var=42 delay=1 dl_time=69 dl_level=36 reverb=1 rv_level=36"},
    {"Pedal Arp", "T: cutoff=64 fenv_depth=34 fenv_d=50 fenv_s=0 aenv_d=58 aenv_s=0 aenv_r=42 |"
                  "T1: wave=6 variation=0 | C: patch_level=127 arp_on=1 arp_var=40 delay=2 pd_time=50 pd_level=40 reverb=1 rv_level=40"},
    {"Random Bells", "T: aenv_d=76 aenv_s=0 aenv_r=70 | T1: wave=4 variation=2 | T2: on=1 wave=4 pitch=24 level=58 |"
                     "C: arp_on=1 arp_var=25 delay=2 pd_time=69 pd_level=44 reverb=1 rv_level=50"},
    {"Triplet Sparkle", "T1: wave=3 variation=2 pitch=12 cutoff=100 aenv_d=60 aenv_s=0 aenv_r=50 |"
                        "C: patch_level=127 arp_on=1 arp_var=5 delay=2 pd_time=30 pd_level=40 reverb=1 rv_level=46"},
    {"Offbeat Chords", "T: cutoff=72 fenv_depth=28 fenv_d=52 fenv_s=10 aenv_d=60 aenv_s=0 aenv_r=40 |"
                       "T1: wave=6 variation=1 | T2: on=1 wave=0 variation=1 pitch=12 level=64 |"
                       "C: arp_on=1 arp_var=34 reverb=1 rv_level=44 delay=1 dl_time=69 dl_level=30"},
    {"Octave Jumper", "T: cutoff=62 fenv_depth=36 fenv_d=46 fenv_s=0 aenv_d=50 aenv_s=0 aenv_r=34 | T1: wave=0 variation=1 |"
                      "C: patch_level=127 arp_on=1 arp_var=43 delay=2 pd_time=69 pd_level=36"},
    /* ----------------------------------------------------------------- F: TRANCE GATE */
    {"Gate Classic", "T: cutoff=100 aenv_a=10 aenv_r=60 | T1: wave=6 variation=2 | T2: on=1 wave=0 variation=1 pitch=12 level=70 |"
                     "C: gate_on=1 gate_pattern=0 gate_rate=3 reverb=1 rv_level=50 delay=2 pd_time=69 pd_level=30"},
    {"Gate Offbeat", "T: cutoff=96 aenv_a=10 aenv_r=60 | T1: wave=6 variation=1 | T2: on=1 wave=1 pitch=12 level=60 |"
                     "C: gate_on=1 gate_pattern=2 gate_rate=1 reverb=1 rv_level=50 delay=2 pd_time=50 pd_level=32"},
    {"Gate Gallop", "T: cutoff=86 reso=30 aenv_a=6 aenv_r=60 | T1: wave=6 variation=2 | T2: on=1 wave=0 pitch=-12 level=66 |"
                    "C: gate_on=1 gate_pattern=4 gate_rate=3 gate_filter=30 reverb=1 rv_level=46"},
    {"Gate Psy Roll", "T1: wave=0 variation=1 cutoff=60 reso=50 fenv_depth=20 fenv_d=80 fenv_s=40 aenv_r=40 |"
                      "C: gate_on=1 gate_pattern=9 gate_rate=3 gate_filter=40 octave=-1 dist=1 dist_drive=24 dist_level=90"},
    {"Gate Sidechain", "T: aenv_a=40 aenv_r=80 cutoff=96 | T1: wave=6 variation=1 | T2: on=1 wave=3 pitch=12 level=64 |"
                       "C: gate_on=1 gate_pattern=3 gate_rate=0 gate_att=110 gate_rel=4 reverb=1 rv_level=56"},
    {"Gate Triplets", "T: cutoff=96 aenv_r=56 | T1: wave=6 variation=2 | T2: on=1 wave=2 pw=50 pwm=30 level=60 |"
                      "C: gate_on=1 gate_pattern=6 gate_rate=4 reverb=1 rv_level=48 delay=2 pd_time=30 pd_level=30"},
    {"Gate Uplifter", "T: fmode=1 cutoff=40 aenv_r=60 | T1: wave=6 variation=2 | T2: on=1 wave=5 variation=1 level=50 |"
                      "C: gate_on=1 gate_pattern=7 gate_rate=5 gate_filter=40 reverb=1 rv_level=60 delay=2 pd_time=50 pd_level=40"},
    {"Gate Anthem", "T: cutoff=104 aenv_r=70 | T1: wave=6 variation=2 | T2: on=1 wave=0 variation=1 pitch=-12 level=56 aenv_a=70 |"
                    "C: gate_on=1 gate_pattern=24 gate_rate=3 gate_t2=0 reverb=1 rv_level=54 delay=2 pd_time=69 pd_level=34"},
    /* ----------------------------------------------------------------- G: KEYS & BELLS */
    {"Ring Bell", "T: aenv_d=86 aenv_s=0 aenv_r=80 | T1: wave=4 | T2: wave=4 pitch=19 |"
                  "C: mod=2 reverb=1 rv_level=54 delay=2 pd_time=79 pd_level=30"},
    {"Sync Sweep", "T1: wave=0 pitch=12 lfo_pitch=36 lfo_rate=30 cutoff=100 aenv_r=40 | T2: on=1 wave=0 level=46 |"
                   "C: mod=1 reverb=1 rv_level=40"},
    {"E-Piano", "T: aenv_d=92 aenv_s=0 aenv_r=60 | T1: wave=4 variation=2 | T2: on=1 wave=4 pitch=12 level=42 |"
                "C: level_vel=52 modfx=2 ph_depth=40 ph_rate=24 ph_reso=20 ph_level=60 reverb=1 rv_type=0 rv_level=36"},
    {"Organ", "T: aenv_s=127 aenv_r=8 | T1: wave=1 variation=2 | T2: on=1 wave=4 pitch=12 level=80 |"
              "T3: on=1 wave=4 pitch=19 level=56 | C: patch_level=84 modfx=1 fl_fb=10 fl_depth=40 fl_rate=54 fl_level=40 reverb=1 rv_type=0 rv_level=30 level_vel=0"},
    {"Clav", "T1: wave=2 pw=80 fmode=3 cutoff=86 reso=60 fenv_depth=20 fenv_d=56 fenv_s=0 aenv_d=62 aenv_s=0 aenv_r=24 |"
             "C: patch_level=127 level_vel=50 modfx=2 ph_level=50 reverb=1 rv_type=0 rv_level=26"},
    {"Hollow Keys", "T: aenv_d=80 aenv_s=40 aenv_r=50 cutoff=80 fenv_depth=18 fenv_d=70 fenv_s=30 | T1: wave=1 variation=2 |"
                    "T2: on=1 wave=3 pitch=12 level=60 | C: reverb=1 rv_type=1 rv_level=40 delay=2 pd_time=79 pd_level=26"},
    {"80s Tom", "T1: wave=4 penv_depth=63 penv_d=62 aenv_d=66 aenv_s=0 aenv_r=60 | T2: on=1 wave=5 fmode=2 cutoff=90 "
                "aenv_d=40 aenv_s=0 aenv_r=30 level=40 | C: reverb=1 rv_type=1 rv_level=44"},
    {"Bit Crash Keys", "T1: wave=3 variation=1 aenv_d=80 aenv_s=30 aenv_r=50 |"
                       "C: dist=3 bc_rate=70 bc_bits=74 bc_filter=30 bc_level=100 delay=1 dl_time=50 dl_level=30"},
    /* ----------------------------------------------------------------- H: FX & RETRO */
    {"Laser Zap", "T1: wave=0 penv_depth=-63 penv_d=56 aenv_d=60 aenv_s=0 aenv_r=60 cutoff=110 |"
                  "C: patch_level=127 delay=2 pd_time=50 pd_fb=70 pd_level=50 reverb=1 rv_level=40"},
    {"Noise Sweep", "T1: wave=5 variation=1 fmode=2 cutoff=46 reso=80 fenv_depth=48 fenv_a=104 fenv_d=110 fenv_s=0 "
                    "fenv_r=100 aenv_a=80 aenv_r=100 | C: reverb=1 rv_level=60 rv_time=100"},
    {"Wind", "T1: wave=5 variation=1 fmode=2 cutoff=64 reso=90 lfo_shape=5 lfo_rate=34 lfo_filter=40 aenv_a=90 aenv_r=100 |"
             "C: reverb=1 rv_level=56"},
    {"Uplifter FX", "T1: wave=5 fmode=1 cutoff=40 reso=40 lfo_shape=2 lfo_sync=1 lfo_rate=20 lfo_filter=56 aenv_a=40 aenv_r=90 |"
                    "C: delay=2 pd_time=50 pd_fb=60 pd_level=40 reverb=1 rv_level=60"},
    {"Fifth Lead", "T1: wave=0 variation=1 cutoff=94 fenv_depth=14 fenv_d=70 fenv_s=60 aenv_r=40 |"
                   "C: patch_level=127 mono=1 porta=1 porta_time=30 modfx=3 ps_pitch=7 ps_level=80 delay=1 dl_time=69 dl_level=34"},
    {"D-Beam Filter", "T: cutoff=62 reso=70 aenv_r=50 | T1: wave=6 variation=2 | T2: on=1 wave=0 pitch=12 level=60 |"
                      "C: db_mode=3 db_assign=12 gate_on=1 gate_pattern=0 gate_rate=3 reverb=1 rv_level=46"},
    {"Retro Sync Brass", "T1: wave=0 pitch=7 penv_depth=24 penv_d=70 cutoff=90 fenv_depth=20 fenv_d=70 fenv_s=50 aenv_a=20 aenv_r=40 |"
                         "T2: on=1 wave=0 level=70 | C: mod=1 reverb=1 rv_level=36"},
    {"SH Classic", "T1: wave=2 pw=50 pwm=40 lfo_rate=48 cutoff=70 reso=50 fenv_depth=30 fenv_d=66 fenv_s=30 aenv_r=30 |"
                   "C: mono=1 porta=1 porta_time=24 reverb=1 rv_type=0 rv_level=30"},
};

bool errors = false;

int find(const ParamDef *defs, int n, const std::string &id)
{
    for (int i = 0; i < n; i++)
        if (id == defs[i].id)
            return i;
    return -1;
}

void apply(Patch &p, const char *text)
{
    std::string s(text);
    size_t at = 0;
    while (at < s.size()) {
        size_t bar = s.find('|', at);
        std::string sec = s.substr(at, bar == std::string::npos ? std::string::npos : bar - at);
        at = bar == std::string::npos ? s.size() : bar + 1;
        size_t colon = sec.find(':');
        if (colon == std::string::npos)
            continue;
        std::string who = sec.substr(0, colon);
        who.erase(0, who.find_first_not_of(' '));
        who.erase(who.find_last_not_of(' ') + 1);
        int t0 = 0, t1 = -1;
        bool common = false;
        if (who == "T")
            t0 = 0, t1 = 2;
        else if (who == "T1" || who == "T2" || who == "T3")
            t0 = t1 = who[1] - '1';
        else if (who == "C")
            common = true;
        else
            errors = true;
        size_t q = colon + 1;
        while (q < sec.size()) {
            while (q < sec.size() && sec[q] == ' ')
                q++;
            size_t e = sec.find(' ', q);
            std::string kv = sec.substr(q, e == std::string::npos ? std::string::npos : e - q);
            q = e == std::string::npos ? sec.size() : e + 1;
            if (kv.empty())
                continue;
            size_t eq = kv.find('=');
            if (eq == std::string::npos) {
                errors = true;
                continue;
            }
            std::string k = kv.substr(0, eq);
            int v = std::atoi(kv.c_str() + eq + 1);
            if (common) {
                int i = find(COMMON, NCOMMON, k);
                if (i < 0 || v < COMMON[i].lo || v > COMMON[i].hi) {
                    std::fprintf(stderr, "preset %s: %s\n", p.name, kv.c_str());
                    errors = true;
                    continue;
                }
                p.com[i] = (int16_t)v;
            } else {
                int i = find(TONE, NTONE, k);
                if (i < 0 || v < TONE[i].lo || v > TONE[i].hi) {
                    std::fprintf(stderr, "preset %s: %s\n", p.name, kv.c_str());
                    errors = true;
                    continue;
                }
                for (int t = t0; t <= t1; t++)
                    p.tone[t][i] = (int16_t)v;
            }
        }
    }
    /* a gate pattern's own steps as the "User" ones too (so drawing starts from it) */
    if (p.com[C_GATE_PAT] < NGATEPAT)
        gatePatternSteps(p.com[C_GATE_PAT], p.gate);
}
} // namespace

bool factoryErrors()
{
    factory(0);
    return errors;
}

const Patch &factory(int k)
{
    static Patch all[NFACTORY];
    static bool made = [] {
        for (int i = 0; i < NFACTORY; i++) {
            patchInit(all[i]);
            std::snprintf(all[i].name, NAME_LEN, "%s", DEFS[i].name);
            apply(all[i], DEFS[i].text);
        }
        return true;
    }();
    (void)made;
    return all[k < 0 ? 0 : k >= NFACTORY ? NFACTORY - 1 : k];
}
} // namespace gl
