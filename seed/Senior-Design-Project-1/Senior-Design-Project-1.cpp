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
static Encoder enc1; // LPF cutoff
static Encoder enc2; // Reverb feedback
static Encoder enc3; // Overdrive drive

// Switches: 
static Switch sw_overdrive; // Switch 4: Overdrive on/off
static Switch sw_reverb;    // Switch 1: Reverb on/off
static Switch sw_comp_mode; // Switch 2: Compressor threshold Low/High
static Switch sw_comp_on;   // Switch 3: Compressor on/off

// Effects //
static Compressor comp;
static Svf        lpf;
static Overdrive  drive;
static ReverbSc   verb;
static daisysp::Limiter limiter;

// Parameters 
inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

// --- Compressor (Switch 2: Low/High preset, Switch 3: on/off)
const float   COMP_THR_LOW    = -120.0f; // Low preset
const float   COMP_THR_HIGH   = -80.0f; // High preset
float         comp_thresh_db  = COMP_THR_HIGH;
volatile bool comp_on         = true;

// --- LPF (SVF)  (Encoder 1)
float       lpf_freq_hz   = 1000.0f;
const float LPF_MIN_HZ    = 500.0f;
const float LPF_MAX_HZ    = 3000.0f;
const float LPF_STEP_HZ   = 100.0f; // 100 Hz per tick

// --- Overdrive (Encoder 3)
float       drive_amt     = 0.4f;
const float DRIVE_MIN     = 0.1f;
const float DRIVE_MAX     = 0.4f;
const float DRIVE_STEP    = 0.05f; // 0.05 per tick

// --- Reverb feedback (Encoder 2)
float       verb_fb       = 0.8f;
const float VERB_FB_MIN   = 0.60f;
const float VERB_FB_MAX   = 0.90f;
const float VERB_FB_STEP  = 0.05f; // 0.05 per tick

// --- Effects on/off 
volatile bool overdrive_on  = true; // Switch 4
volatile bool reverb_on     = true; // Switch 1

// Audio Callback
static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    // Read controls 
    enc1.Debounce();
    enc2.Debounce();
    enc3.Debounce();
    sw_overdrive.Debounce();
    sw_reverb.Debounce();
    sw_comp_mode.Debounce();
    sw_comp_on.Debounce();

    // Encoder increments
    int d1 = enc1.Increment();
    int d2 = enc2.Increment();
    int d3 = enc3.Increment();

    // === Encoder 1: LPF frequency ===
    if(d1)
    {
        lpf_freq_hz = clampf(lpf_freq_hz + d1 * LPF_STEP_HZ, LPF_MIN_HZ, LPF_MAX_HZ);
        lpf.SetFreq(lpf_freq_hz);
    }

    // === Encoder 2: Reverb feedback ===
    if(d2)
    {
        verb_fb = clampf(verb_fb + d2 * VERB_FB_STEP, VERB_FB_MIN, VERB_FB_MAX);
        verb.SetFeedback(verb_fb);
    }

    // === Encoder 3: Overdrive drive ===
    if(d3)
    {
        drive_amt = clampf(drive_amt + d3 * DRIVE_STEP, DRIVE_MIN, DRIVE_MAX);
        drive.SetDrive(drive_amt);
    }

    // === Switch states ===
    comp_on   = sw_comp_on.Pressed();    // Switch 3: Compressor on/off
    // Switch 2: Compressor threshold Low/High
    {
        bool  comp_low    = sw_comp_mode.Pressed();
        float desired_thr = comp_low ? COMP_THR_LOW : COMP_THR_HIGH;
        if(desired_thr != comp_thresh_db)
        {
            comp_thresh_db = desired_thr;
            comp.SetThreshold(comp_thresh_db);
        }
    }

    // 
    reverb_on    = sw_reverb.Pressed();     // Switch 1  Reverb on/off
    overdrive_on = sw_overdrive.Pressed();  // Switch 4  Overdrive on/off

    // Process audio
    for(size_t i = 0; i < size; i += 2)
    {
        float inL = in[i];
        float sig = inL; 

        // 1) Compressor
        if(comp_on)
            sig = comp.Process(sig);

        // 2) LPF 
        lpf.Process(sig);
        sig = lpf.Low();

        // 3) Overdrive 
        if(overdrive_on)
            sig = drive.Process(sig);

        // 4) Reverb 
        float outL = sig, outR = sig;
        if(reverb_on)
        {
            float wetL, wetR;
            verb.Process(sig, sig, &wetL, &wetR);
            outL = wetL;
            outR = wetR;
        }

        out[i]     = outL;
        out[i] = outR;
 // 5) Limiter 
        limiter.ProcessBlock(&outL, 1, 1.0f);
        limiter.ProcessBlock(&outR, 1, 1.0f);

        out[i]     = outL;
        out[i + 1] = outR;
    }
}


// Main

int main(void)
{
    hw.Configure();
    hw.Init();

    hw.SetAudioBlockSize(4);
    float sr = hw.AudioSampleRate();

    // Effects default
    comp.Init(sr);
    comp.SetThreshold(comp_thresh_db);
    comp.SetRatio(2.0f);
    comp.SetAttack(0.005f);
    comp.SetRelease(0.200f);

    lpf.Init(sr);
    lpf.SetFreq(lpf_freq_hz);
    lpf.SetRes(0.0f);
    lpf.SetDrive(1.0f);

    drive.Init();
    drive.SetDrive(drive_amt);

    verb.Init(sr);
    verb.SetFeedback(verb_fb);
    verb.SetLpFreq(18000.0f);
     limiter.Init();

    // Controls init
    // Encoders:
    enc1.Init(hw.GetPin(1),  hw.GetPin(2),  Pin()); // LPF cutoff
    enc2.Init(hw.GetPin(3),  hw.GetPin(4),  Pin()); // Reverb feedback
    enc3.Init(hw.GetPin(5),  hw.GetPin(6),  Pin()); // Overdrive drive

    // Switches:
    sw_overdrive.Init(hw.GetPin(26)); // Switch 4: Overdrive on/off
    sw_reverb.Init(hw.GetPin(27));    // Switch 1: Reverb on/off
    sw_comp_mode.Init(hw.GetPin(28)); // Switch 2: Comp Threshold Low/High
    sw_comp_on.Init(hw.GetPin(29));   // Switch 3: Compressor on/off

    hw.StartAudio(AudioCallback);

    while(1)
    {
        System::Delay(1);
    }
}
