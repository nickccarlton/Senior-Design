#include "daisysp.h"
#include "daisy_seed.h"
#include "hid/encoder.h"
#include "hid/switch.h"
#include "hid/limiter.h"

using namespace daisy;
using namespace daisysp;

// =========================
// Hardware
// =========================
static DaisySeed hw;

// Encoders: 
static Encoder enc1; // Gate [threshold] AND [release] (higher threshold ~ longer release)
static Encoder enc2; // (Sidechain) Compressor [threshold]
static Encoder enc3; // (Sidechain) Compressor [release]
// Switches: 
static Switch sw_;          // Switch 4: Gate [on/off]
static Switch sw_reverb;    // Switch 1: Reverb [feedback, high/low] 
static Switch sw_comp_mode; // Switch 2: Compressor [sidechain/single-source] 
static Switch sw_comp_on;   // Switch 3: Compressor on/off

// Effects //
static Compressor comp;
static ReverbSc   verb;
static daisysp::Limiter limiter;
