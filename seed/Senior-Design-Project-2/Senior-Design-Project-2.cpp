/*
This FX chain is our "SparkleClean" chain which processes raw_in using digital (d_) and analog (a_) FX to output processed_out in the following signal path:
raw_in -> a_preamp -> d_chorus -> d_cabinet model -> a_distortion -> processed_out.
*/
#include "daisy_seed.h"
#include "daisysp.h"
#include "hid/encoder.h"
#include "hid/switch.h"

using namespace daisy;
using namespace daisysp;

static DaisySeed hw;

// === Chorus ===
static Chorus chorus;

// === Encoders ===
// enc1: Depth, enc2: SetLfoFrequency, enc3: SetDelayMs
static Encoder enc1;
static Encoder enc2;
static Encoder enc3;

// === Switch ===
// switch  Chorus Feedback  low/high
static Switch sw_fb;

// === Parameters ===
inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
inline float ms_to_s(float ms) { return ms * 0.001f; }

// Depth (0.0 ~ 2.0)
float depthL = 1.0f, depthR = 1.0f;
const float DEPTH_MIN = 0.0f, DEPTH_MAX = 2.0f, DEPTH_STEP = 0.1f;

// LFO Frequency (Hz)
float lfoL = 0.25f, lfoR = 0.30f;
const float LFO_MIN = 0.05f, LFO_MAX = 5.0f, LFO_STEP = 0.1f;

// SetDelayMs
float delayMsL = 8.0f, delayMsR = 9.0f;
const float DMS_MIN = 2.0f, DMS_MAX = 25.0f, DMS_STEP = 0.5f;

// Feedback presets
const float FB_LOW  = 0.06f;
const float FB_HIGH = 0.22f;

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size)
{
    // === Controls ===
    enc1.Debounce();
    enc2.Debounce();
    enc3.Debounce();
    sw_fb.Debounce();

    // Encoder 1: Depth
    int inc1 = enc1.Increment();
    if(inc1)
    {
        depthL = clampf(depthL + inc1 * DEPTH_STEP, DEPTH_MIN, DEPTH_MAX);
        depthR = clampf(depthR + inc1 * DEPTH_STEP, DEPTH_MIN, DEPTH_MAX);
        chorus.SetLfoDepth(depthL, depthR);
    }

    // Encoder 2: LFO Freq
    int inc2 = enc2.Increment();
    if(inc2)
    {
        lfoL = clampf(lfoL + inc2 * LFO_STEP, LFO_MIN, LFO_MAX);
        lfoR = clampf(lfoR + inc2 * LFO_STEP, LFO_MIN, LFO_MAX);
        chorus.SetLfoFreq(lfoL, lfoR);
    }

    // Encoder 3: Delay 
    int inc3 = enc3.Increment();
    if(inc3)
    {
        delayMsL = clampf(delayMsL + inc3 * DMS_STEP, DMS_MIN, DMS_MAX);
        delayMsR = clampf(delayMsR + inc3 * DMS_STEP, DMS_MIN, DMS_MAX);
        chorus.SetDelay(ms_to_s(delayMsL), ms_to_s(delayMsR));
    }

    // Switch: Feedback preset (OFF=low, ON=high)
    chorus.SetFeedback(sw_fb.Pressed() ? FB_HIGH : FB_LOW);

    // === Audio processing ===
    for(size_t i = 0; i < size; i += 2)
    {
        float dryL = in[i]; // output 
        chorus.Process(dryL);
        out[i]     = chorus.GetLeft();
        out[i + 1] = chorus.GetRight();
    }
}

int main(void)
{
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

    // Chorus init 
    chorus.Init(sample_rate);
    chorus.SetLfoFreq(lfoL, lfoR);
    chorus.SetLfoDepth(depthL, depthR);
    chorus.SetDelay(ms_to_s(delayMsL), ms_to_s(delayMsR));
    chorus.SetFeedback(FB_LOW);

    
    // enc pin
    enc1.Init(hw.GetPin(1), hw.GetPin(2), Pin()); // Depth
    enc2.Init(hw.GetPin(3), hw.GetPin(4), Pin()); // LFO Freq
    enc3.Init(hw.GetPin(5), hw.GetPin(6), Pin()); // Delay

    //switch pin
    sw_fb.Init(hw.GetPin(26));

    hw.StartAudio(AudioCallback);
    while(1) {}
}