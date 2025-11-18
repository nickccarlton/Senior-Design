#include "daisysp.h"
#include "daisy_seed.h"
#include "hid/encoder.h"
#include "hid/switch.h"

using namespace daisy;
using namespace daisysp;

// ===== Hardware =====
static DaisySeed hw;

// ===== Controls =====
static Encoder enc1;          // Reverb LPF cutoff
static Encoder enc2;          // (Sidechain) Compressor Threshold (dB)
static Encoder enc3;          // (Sidechain) Compressor Release 

static Switch  sw_rev_fb;     // Switch 1: Reverb Feedback High/Low
static Switch  sw_comp_mode;  // Switch 2: Compressor sidechain / single-source
static Switch  sw_comp_on;    // Switch 3: Compressor on/off
static Switch  sw_rev_on;     // Switch 4: Reverb on/off

// ===== Effects =====
static ReverbSc  verb;
static Compressor comp;

// ===== Params =====
inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

// Reverb
float verb_fb_low   = 0.70f;     // 
float verb_fb_high  = 0.85f;     // 
float verb_fb       = verb_fb_high;

float verb_lpf_hz   = 12000.0f;  // Enc1
const float LPF_MIN = 3000.0f;
const float LPF_MAX = 18000.0f;
const float LPF_STP = 500.0f;    //  500 per tick Hz

// Compressor
float comp_thr_db    = -40.0f;   // Enc2
const float CTHR_MIN = -80.0f;
const float CTHR_MAX =   0.0f;
const float CTHR_STP =   1.0f;

float comp_rel_sec   = 0.100f;   // Enc3
const float CREL_MIN = 0.020f;
const float CREL_MAX = 0.500f;
const float CREL_STP = 0.010f;

// Switch states
volatile bool comp_on      = true;  // Switch 3
volatile bool sidechain_on = true;  // Switch 2
volatile bool reverb_on    = true;  // Switch 4
volatile bool rev_hi_fb    = true;  // Switch 1 (true=High, false=Low)

// ===== Audio Callback =====
static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    // Debounce
    enc1.Debounce();
    enc2.Debounce();
    enc3.Debounce();
    sw_rev_fb.Debounce();
    sw_comp_mode.Debounce();
    sw_comp_on.Debounce();
    sw_rev_on.Debounce();

    // Enc1: Reverb LPF cutoff
    if (int d1 = enc1.Increment())
    {
        verb_lpf_hz = clampf(verb_lpf_hz + d1 * LPF_STP, LPF_MIN, LPF_MAX);
        verb.SetLpFreq(verb_lpf_hz);
    }

    // Enc2: Compressor Threshold (dB)
    if (int d2 = enc2.Increment())
    {
        comp_thr_db = clampf(comp_thr_db + d2 * CTHR_STP, CTHR_MIN, CTHR_MAX);
        comp.SetThreshold(comp_thr_db);
    }

    // Enc3: Compressor Release (sec)
    if (int d3 = enc3.Increment())
    {
        comp_rel_sec = clampf(comp_rel_sec + d3 * CREL_STP, CREL_MIN, CREL_MAX);
        comp.SetRelease(comp_rel_sec);
    }

    // Switch states
    rev_hi_fb    = sw_rev_fb.Pressed();    // Switch 1: FB High/Low
    sidechain_on = sw_comp_mode.Pressed(); // Switch 2: sidechain / single-source
    comp_on      = sw_comp_on.Pressed();   // Switch 3: comp on/off
    reverb_on    = sw_rev_on.Pressed();    // Switch 4: reverb on/off

    // Reverb feedback (High/Low )
    float desired_fb = rev_hi_fb ? verb_fb_high : verb_fb_low;
    if (desired_fb != verb_fb)
    {
        verb_fb = desired_fb;
        verb.SetFeedback(verb_fb);
    }

    // ==== raw_in → Reverb → Compressor → out (mono) ====
    for (size_t i = 0; i < size; i += 2)
    {
        float inL = in[i];       // MAIN
        float inR = in[i + 1];   // SIDECHAIN detector (if used)

        // 1) Reverb 
        float wetL = inL, wetR = inL;
        if (reverb_on)
        {
            verb.Process(inL, inL, &wetL, &wetR);
        }
        // 
        float post_rev_mono = reverb_on ? 0.5f * (wetL + wetR) : inL;

        // 2) Compressor (single/sidechain)
        float y;
        if (comp_on)
        {
            y = sidechain_on ? comp.Process(post_rev_mono, inR)
                             : comp.Process(post_rev_mono);
        }
        else
        {
            y = post_rev_mono;
        }

        // 3) Mono out (L=R)
        out[i]     = y;
        out[i + 1] = y;
    }
}

// ===== Main =====
int main(void)
{
    hw.Configure();
    hw.Init();

    hw.SetAudioBlockSize(4);
    const float sr = hw.AudioSampleRate();

    // Effects init
    verb.Init(sr);
    verb.SetFeedback(verb_fb);
    verb.SetLpFreq(verb_lpf_hz);

    comp.Init(sr);
    comp.SetThreshold(comp_thr_db);
    comp.SetRatio(8.0f);
    comp.SetAttack(0.005f);
    comp.SetRelease(comp_rel_sec);

    // Controls init
    enc1.Init(hw.GetPin(1), hw.GetPin(2), Pin()); // Reverb LPF cutoff
    enc2.Init(hw.GetPin(3), hw.GetPin(4), Pin()); // Comp Threshold
    enc3.Init(hw.GetPin(5), hw.GetPin(6), Pin()); // Comp Release

    sw_rev_fb.Init(hw.GetPin(27));    // Switch 1: Reverb FB High/Low
    sw_comp_mode.Init(hw.GetPin(28)); // Switch 2: Sidechain / Single-source
    sw_comp_on.Init(hw.GetPin(29));   // Switch 3: Compressor On/Off
    sw_rev_on.Init(hw.GetPin(26));    // Switch 4: Reverb On/Off

    hw.StartAudio(AudioCallback);
    while (1) { System::Delay(1); }
}

////// IF sound is too noisy, adjust COMPRESSOR (ENC2 )encoder!!///