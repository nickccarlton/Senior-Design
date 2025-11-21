#include "daisysp.h"
#include "daisy_seed.h"
#include "hid/encoder.h"
#include "hid/analog_switch.h"
#include "hid/limiter.h"

using namespace daisy;
using namespace daisysp;

// =========================
// Hardware
// =========================
static DaisySeed hw;

// Encoders with push buttons
static Encoder enc1; // Push: Switch to Chain 1
static Encoder enc2;
static Encoder enc3; // Push: Switch to Chain 3

// Analog switch (4 bits from 1 ADC pin)
static AnalogSwitch4 analog_sw;

// =========================
// Shared Reverb (Only ONE)
// =========================
static ReverbSc verb;          // <--
static daisysp::Limiter limiter1;

// =========================
// Chain 1 Effects
// =========================
static Compressor comp1;
static Svf        lpf1;
static Overdrive  drive1;

// Chain3 Effects ()
static Compressor comp3;

// =========================
// Chain Selection
// =========================
volatile int active_chain = 1; // 1 = Chain1, 3 = Chain3

inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

// =========================
// Chain 1 Parameters
// =========================
const float   COMP_THR_LOW_1  = -120.0f;
const float   COMP_THR_HIGH_1 = -80.0f;
float         comp_thresh_db_1 = COMP_THR_HIGH_1;
volatile bool comp_on_1        = true;

float       lpf_freq_hz_1   = 1000.0f;
const float LPF_MIN_HZ_1    = 500.0f;
const float LPF_MAX_HZ_1    = 3000.0f;
const float LPF_STEP_HZ_1   = 100.0f;

float       drive_amt_1     = 0.4f;
const float DRIVE_MIN_1     = 0.1f;
const float DRIVE_MAX_1     = 0.4f;
const float DRIVE_STEP_1    = 0.05f;

float       verb_fb_1       = 0.8f;
const float VERB_FB_MIN_1   = 0.60f;
const float VERB_FB_MAX_1   = 0.90f;
const float VERB_FB_STEP_1  = 0.05f;

volatile bool overdrive_on_1 = true;
volatile bool reverb_on_1    = true;

// =========================
// Chain 3 Parameters
// =========================
float verb_fb_low_3   = 0.70f;
float verb_fb_high_3  = 0.85f;
float verb_fb_3       = verb_fb_high_3;

float verb_lpf_hz_3   = 12000.0f;
const float LPF_MIN_3 = 3000.0f;
const float LPF_MAX_3 = 18000.0f;
const float LPF_STP_3 = 500.0f;

float comp_thr_db_3    = -40.0f;
const float CTHR_MIN_3 = -80.0f;
const float CTHR_MAX_3 =   0.0f;
const float CTHR_STP_3 =   1.0f;

float comp_rel_sec_3   = 0.100f;
const float CREL_MIN_3 = 0.020f;
const float CREL_MAX_3 = 0.500f;
const float CREL_STP_3 = 0.010f;

volatile bool comp_on_3      = true;
volatile bool sidechain_on_3 = true;
volatile bool reverb_on_3    = true;
volatile bool rev_hi_fb_3    = true;

// =========================
// Process Chain 1
// =========================
void ProcessChain1(AudioHandle::InterleavingInputBuffer in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t size)
{
    // Chain1 전용 Reverb 설정
    verb.SetFeedback(verb_fb_1);
    verb.SetLpFreq(18000.0f);

    for(size_t i = 0; i < size; i += 2)
    {
        float inL = in[i];
        float sig = inL;

        if(comp_on_1)
            sig = comp1.Process(sig);

        lpf1.Process(sig);
        sig = lpf1.Low();

        if(overdrive_on_1)
            sig = drive1.Process(sig);

        float outL = sig, outR = sig;

        if(reverb_on_1)
        {
            float wetL, wetR;
            verb.Process(sig, sig, &wetL, &wetR);
            outL = wetL;
            outR = wetR;
        }

        limiter1.ProcessBlock(&outL, 1, 1.0f);
        limiter1.ProcessBlock(&outR, 1, 1.0f);

        out[i]     = outL;
        out[i + 1] = outR;
    }
}

// =========================
// Process Chain 3
// =========================
void ProcessChain3(AudioHandle::InterleavingInputBuffer in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t size)
{
    verb.SetFeedback(verb_fb_3);
    verb.SetLpFreq(verb_lpf_hz_3);

    for(size_t i = 0; i < size; i += 2)
    {
        float inL = in[i];
        float inR = in[i + 1];

        float wetL = inL, wetR = inL;

        if(reverb_on_3)
        {
            verb.Process(inL, inL, &wetL, &wetR);
        }

        float mono = reverb_on_3 ? 0.5f * (wetL + wetR) : inL;

        float y;

        if(comp_on_3)
        {
            if(sidechain_on_3)
                y = comp3.Process(mono, inR);
            else
                y = comp3.Process(mono);
        }
        else
            y = mono;

        out[i]     = y;
        out[i + 1] = y;
    }
}

// =========================
// AudioCallback
// =========================
static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    enc1.Debounce();
    enc2.Debounce();
    enc3.Debounce();
    analog_sw.Debounce();

    if(enc1.RisingEdge()) active_chain = 1;
    if(enc3.RisingEdge()) active_chain = 3;

    // Controls
    if(active_chain == 1)
    {
        int d1 = enc1.Increment();
        int d2 = enc2.Increment();
        int d3 = enc3.Increment();

        if(d1)
        {
            lpf_freq_hz_1 = clampf(lpf_freq_hz_1 + d1 * LPF_STEP_HZ_1,
                                   LPF_MIN_HZ_1, LPF_MAX_HZ_1);
            lpf1.SetFreq(lpf_freq_hz_1);
        }

        if(d2)
        {
            verb_fb_1 = clampf(verb_fb_1 + d2 * VERB_FB_STEP_1,
                               VERB_FB_MIN_1, VERB_FB_MAX_1);
        }

        if(d3)
        {
            drive_amt_1 = clampf(drive_amt_1 + d3 * DRIVE_STEP_1,
                                 DRIVE_MIN_1, DRIVE_MAX_1);
            drive1.SetDrive(drive_amt_1);
        }

        reverb_on_1    = analog_sw.Pressed(0);
        bool comp_low  = analog_sw.Pressed(1);
        comp_on_1      = analog_sw.Pressed(2);
        overdrive_on_1 = analog_sw.Pressed(3);

        float desired_thr = comp_low ? COMP_THR_LOW_1 : COMP_THR_HIGH_1;
        if(desired_thr != comp_thresh_db_1)
        {
            comp_thresh_db_1 = desired_thr;
            comp1.SetThreshold(comp_thresh_db_1);
        }
    }
    else // Chain3
    {
        int d1 = enc1.Increment();
        int d2 = enc2.Increment();
        int d3 = enc3.Increment();

        if(d1)
        {
            verb_lpf_hz_3 = clampf(verb_lpf_hz_3 + d1 * LPF_STP_3,
                                   LPF_MIN_3, LPF_MAX_3);
        }

        if(d2)
        {
            comp_thr_db_3 = clampf(comp_thr_db_3 + d2 * CTHR_STP_3,
                                   CTHR_MIN_3, CTHR_MAX_3);
            comp3.SetThreshold(comp_thr_db_3);
        }

        if(d3)
        {
            comp_rel_sec_3 = clampf(comp_rel_sec_3 + d3 * CREL_STP_3,
                                    CREL_MIN_3, CREL_MAX_3);
            comp3.SetRelease(comp_rel_sec_3);
        }

        rev_hi_fb_3    = analog_sw.Pressed(0);
        sidechain_on_3 = analog_sw.Pressed(1);
        comp_on_3      = analog_sw.Pressed(2);
        reverb_on_3    = analog_sw.Pressed(3);

        float desired_fb = rev_hi_fb_3 ? verb_fb_high_3 : verb_fb_low_3;
        if(desired_fb != verb_fb_3)
        {
            verb_fb_3 = desired_fb;
        }
    }

    if(active_chain == 1)
        ProcessChain1(in, out, size);
    else
        ProcessChain3(in, out, size);
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

    // Chain1 DSP init
    comp1.Init(sr);
    comp1.SetThreshold(comp_thresh_db_1);
    comp1.SetRatio(2.0f);
    comp1.SetAttack(0.005f);
    comp1.SetRelease(0.200f);

    lpf1.Init(sr);
    lpf1.SetFreq(lpf_freq_hz_1);

    drive1.Init();
    drive1.SetDrive(drive_amt_1);

    // Shared Reverb Init
    verb.Init(sr);
    verb.SetFeedback(verb_fb_1);
    verb.SetLpFreq(18000.0f);

    limiter1.Init();

    // Chain3 DSP init
    comp3.Init(sr);
    comp3.SetThreshold(comp_thr_db_3);
    comp3.SetRatio(8.0f);
    comp3.SetAttack(0.005f);
    comp3.SetRelease(comp_rel_sec_3);

    // Encoders + Push Buttons
    enc1.Init(hw.GetPin(2), hw.GetPin(1), hw.GetPin(7));
    enc2.Init(hw.GetPin(4), hw.GetPin(3), hw.GetPin(8));
    enc3.Init(hw.GetPin(6), hw.GetPin(5), hw.GetPin(9));

    // Analog ladder switch
    analog_sw.Init(&hw, hw.GetPin(15));

    hw.StartAudio(AudioCallback);

    while(1)
        System::Delay(1);
}
