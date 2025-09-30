#include "daisysp.h"
#include "daisy_seed.h"
#include "hid/encoder.h"

using namespace daisy;
using namespace daisysp;

// === Hardware ===
static DaisySeed hw;
static Encoder enc;

// === Effects ===
static Compressor comp;
static Svf        lpf;
static Overdrive  drive;
static ReverbSc   verb;

// === Reverb feedback parameter ===
float feedback = 0.8f;
const float fb_min = 0.6f;
const float fb_max = 0.9f;

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    // --- Encoder  --- //
    enc.Debounce();
    int inc = enc.Increment();
    if(inc != 0)
    {
        feedback += inc * 0.02f;

        // Clamp between 0.6 and 0.9
        if(feedback < fb_min) feedback = fb_min;
        if(feedback > fb_max) feedback = fb_max;

        verb.SetFeedback(feedback);
    }

    for(size_t i = 0; i < size; i += 2)
    {
        // --- Mono Input --- //
        float inMono = in[i];

        // --- Compressor --- //
        float compOut = comp.Process(inMono);

        // --- LPF --- //
        lpf.Process(compOut);
        float lp = lpf.Low();

        // --- Overdrive --- //
        float od = drive.Process(lp);

        // --- Reverb --- //
        float wetL, wetR;
        verb.Process(od, od, &wetL, &wetR);

        // --- Output (Stereo) --- //
        out[i]     = wetL;
        out[i + 1] = wetR;
    }
}

int main(void)
{
    // --- Hardware Init --- //
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

    // --- Compressor setup --- //
    comp.Init(sample_rate);
    comp.SetThreshold(-14.0f);
    comp.SetRatio(2.0f);
    comp.SetAttack(0.005f);
    comp.SetRelease(0.2f);

    // --- LPF setup --- //
    lpf.Init(sample_rate);
    lpf.SetFreq(1000.0f);   /// I setup lowpass filter frequency 1kh for now.
    lpf.SetRes(0.0f);
    lpf.SetDrive(1.0f);

    // --- Overdrive setup --- //
    drive.Init();
    drive.SetDrive(0.4f);         // Above this causes noise 

    // --- Reverb setup --- //
    verb.Init(sample_rate);
    verb.SetFeedback(feedback);   // Will be adjusted with encoder
    verb.SetLpFreq(18000.0f);     // 

    // --- Encoder setup (pins: D1, D2, pushbutton unused) --- //
    enc.Init(hw.GetPin(1), hw.GetPin(2), Pin());    /// Pin() is encoder pushbutton

    // --- Start Audio --- //
    hw.StartAudio(AudioCallback);
    while(1) {}
}
