/*
 * StdInc.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../Global.h"

// pybind11 must see Python.h before any standard headers it wraps; keep this block first
// relative to other Python-related includes. Requires Python >= 3.12 and pybind11 >= 3.0
// (sub-interpreter support with per-interpreter GIL).
#include <pybind11/embed.h>
#include <pybind11/subinterpreter.h>

// This header should be treated as a pre compiled header file(PCH) in the compiler building settings.

// Here you can add specific libraries and macros which are specific to this project.
