/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (GALA SH-1T) */
/* GALA SH-1T's factory patches: 64, in eight banks of eight as the GAIA's (A1 .. H8), each written in Presets.cpp as
 * what it changes from the INIT PATCH */
#pragma once
#include "Params.h"

namespace gl
{
constexpr int NFACTORY = 64;
extern const char *const BANK_NAME[8];
const Patch &factory(int k);
bool factoryErrors();                         /* (a factory patch names a parameter that is not there) */
} // namespace gl
