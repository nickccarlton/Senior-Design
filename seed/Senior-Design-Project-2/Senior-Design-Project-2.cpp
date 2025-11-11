// master_toggle_test.cpp
#include "daisy_seed.h"
#include "daisysp.h"
#include "hid/encoder.h"

using namespace daisy;
using namespace daisysp;

static DaisySeed hw;

// Controls
static Encoder enc1; // A,B,Click

// Effects
static ReverbSc verb;
static Chorus   chorus;

// State
static bool use_chorus = false; // false=Reverb, true=Chorus

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    // --- Read click (once per block) ---
    enc1.Debounce();
    if(enc1.RisingEdge())
        use_chorus = !use_chorus;

    // --- Audio ---
    for(size_t i = 0; i < size; i += 2)
    {
        float inL = in[i];
        float inR = in[i + 1];
        float outL, outR;

        if(use_chorus)
        {
            // Chorus: mono in -> stereo out
            float dry = 0.5f * (inL + inR);
            chorus.Process(dry);
            outL = chorus.GetLeft();
            outR = chorus.GetRight();
        }
        else
        {
            // Reverb: stereo in -> stereo out (with simple wet/dry)
            float wetL, wetR;
            verb.Process(inL, inR, &wetL, &wetR);
            const float mix = 0.35f; // adjust to taste
            outL = (1.0f - mix) * inL + mix * wetL;
            outR = (1.0f - mix) * inR + mix * wetR;
        }

        out[i]     = outL;
        out[i + 1] = outR;
    }
}

int main(void)
{
    // --- Hardware ---
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    const float sr = hw.AudioSampleRate();

    // --- Effects init ---
    verb.Init(sr);
    verb.SetFeedback(0.75f);
    verb.SetLpFreq(18000.0f);

    chorus.Init(sr);
    chorus.SetLfoFreq(0.33f, 0.20f);
    chorus.SetLfoDepth(1.0f, 1.0f);
    chorus.SetDelay(0.75f, 0.90f);

    // --- Encoder init (A, B, Click) ---
    // !!! Replace pins to match your wiring !!!
    enc1.Init(hw.GetPin(1), hw.GetPin(2), hw.GetPin(7));

    // --- Start audio ---
    hw.StartAudio(AudioCallback);
    while(1) { System::Delay(1); }
}
