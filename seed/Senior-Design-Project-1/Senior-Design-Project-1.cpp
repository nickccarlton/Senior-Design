#include "daisysp.h"
#include "daisy_seed.h"
#include "hid/encoder.h"
#include "hid/switch.h"

using namespace daisy;
using namespace daisysp;

// =========================
// Hardware
// =========================
static DaisySeed hw;

// Encoders: 
static Encoder enc1; // Compressor threshold
static Encoder enc2; // LPF cutoff
static Encoder enc3; // Overdrive drive
static Encoder enc4; // Reverb feedback

// Switches: on/off 
static Switch sw_overdrive; // Overdrive on/off
static Switch sw_reverb;    // Reverb on/off


// Effects //
static Compressor comp;
static Svf        lpf;
static Overdrive  drive;
static ReverbSc   verb;


// Parameters 

inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

// --- Compressor
float       comp_thresh_db = -14.0f;
const float   COMP_THR_MIN    = -60.0f;
const float   COMP_THR_MAX    =   0.0f;
const float   COMP_THR_STEP   =   5.0f; // 5 dB per tick

// --- LPF (SVF)
float       lpf_freq_hz   = 1000.0f;
const float   LPF_MIN_HZ    = 500.0f;
const float   LPF_MAX_HZ    = 3000.0f;
const float   LPF_STEP_HZ   = 100.0f;

// --- Overdrive
float       drive_amt     = 0.4f;
const float   DRIVE_MIN     = 0.0f;
const float   DRIVE_MAX     = 0.5f;
const float   DRIVE_STEP    = 0.05f;

// --- Reverb feedback
float       verb_fb       = 0.8f;
const float   VERB_FB_MIN   = 0.60f;
const float   VERB_FB_MAX   = 0.90f;
const float   VERB_FB_STEP  = 0.05f;

// --- Effects on/off 
volatile bool overdrive_on  = true;
volatile bool reverb_on     = true;


// Audio Callback

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    // Read controls 
    enc1.Debounce();
    enc2.Debounce();
    enc3.Debounce();
    enc4.Debounce();
    sw_overdrive.Debounce();
    sw_reverb.Debounce();

    // Encoder increments
    int d1 = enc1.Increment();
    int d2 = enc2.Increment();
    int d3 = enc3.Increment();
    int d4 = enc4.Increment();

    // clamping values//
    if(d1)
    {
        comp_thresh_db = clampf(comp_thresh_db + d1 * COMP_THR_STEP, COMP_THR_MIN, COMP_THR_MAX);
        comp.SetThreshold(comp_thresh_db);
    }
    if(d2)
    {
        lpf_freq_hz = clampf(lpf_freq_hz + d2 * LPF_STEP_HZ, LPF_MIN_HZ, LPF_MAX_HZ);
        lpf.SetFreq(lpf_freq_hz);
    }
    if(d3)
    {
        drive_amt = clampf(drive_amt + d3 * DRIVE_STEP, DRIVE_MIN, DRIVE_MAX);
        drive.SetDrive(drive_amt);
    }
    if(d4)
    {
        verb_fb = clampf(verb_fb + d4 * VERB_FB_STEP, VERB_FB_MIN, VERB_FB_MAX);
        verb.SetFeedback(verb_fb);
    }

    // Switches on/off states
    overdrive_on = sw_overdrive.Pressed();
    reverb_on    = sw_reverb.Pressed();

    // Process audio
    for(size_t i = 0; i < size; i += 2)
    {
        float inL = in[i];
        float sig = inL; 

        // 1) Compressor
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
        out[i + 1] = outR;
    }
}

// =========================
// Main
// =========================
int main(void)
{
    hw.Configure();
    hw.Init();

    
    hw.SetAudioBlockSize(4);
    float sr = hw.AudioSampleRate();

    
    // Effects defalt
   
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

    
    // Controls init
    
   
    enc1.Init(hw.GetPin(1),  hw.GetPin(2),  Pin()); // Comp threshold
    enc2.Init(hw.GetPin(3),  hw.GetPin(4),  Pin()); // LPF cutoff
    enc3.Init(hw.GetPin(5),  hw.GetPin(6),  Pin()); // Overdrive drive
    enc4.Init(hw.GetPin(7),  hw.GetPin(8),  Pin()); // Reverb feedback

    // Switches:
   
    sw_overdrive.Init(hw.GetPin(26)); // Overdrive on/off
    sw_reverb.Init(hw.GetPin(27));    // Reverb on/off

   
    
    
    hw.StartAudio(AudioCallback);

    
    while(1)
    {
        
        System::Delay(1);
    }
}
