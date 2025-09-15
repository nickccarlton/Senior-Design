/*
This FX chain is our "SparkleClean" chain which processes raw_in using digital (d_) and analog (a_) FX to output processed_out in the following signal path:
raw_in -> a_preamp -> d_chorus -> d_cabinet model -> a_distortion -> processed_out.
*/


#include "daisy_seed.h"
#include "daisysp.h"
#include "hid/encoder.h"

using namespace daisy;
using namespace daisysp;

static DaisySeed hw;

// === Chorus ===
static Chorus chorus;

// === encoder  ===
static Encoder enc;
float depthL = 1.0f; //depth ratio (0.0 ~ 1.0)
float depthR = 1.0f;  //depth ratio (0.0 ~ 1.0)

static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size)
{
    // === Encoder ===
    enc.Debounce();
    int inc = enc.Increment();

    if(inc != 0)
    {
        depthL += inc * 0.05f;
        depthR += inc * 0.05f;

        // Depth limit
        if(depthL < 0.0f) depthL = 0.0f;
        if(depthR < 0.0f) depthR = 0.0f;
        if(depthL > 2.0f) depthL = 2.0f;
        if(depthR > 2.0f) depthR = 2.0f;

        chorus.SetLfoDepth(depthL, depthR);
    }

    // === Audio processing ===
    for(size_t i = 0; i < size; i += 2)
    {
        float dry = in[i];  // left channel

        chorus.Process(dry);

        out[i]     = chorus.GetLeft();   // LEFT output
        out[i + 1] = chorus.GetRight();  // RIGHT output
    }
}

int main(void)
{
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

    // Chorus 
    chorus.Init(sample_rate);
    chorus.SetLfoFreq(0.25f, 0.3f);   // LFO 
    chorus.SetLfoDepth(depthL, depthR); //  Depth calculation with encoder.
    chorus.SetDelay(0.7f, 0.8f);      //  Delay 

    // === D1, D2 , Pin(this is encoder pushbutton ) ) === encoder pushbutton:
	//********not decided yet.*********
    enc.Init(hw.GetPin(1), hw.GetPin(2), Pin());

    // 
    hw.StartAudio(AudioCallback);

    while(1) {} // do nothing
}
