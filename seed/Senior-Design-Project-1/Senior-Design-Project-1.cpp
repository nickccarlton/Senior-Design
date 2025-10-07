#include "daisysp.h"
#include "daisy_seed.h"
#include "hid/encoder.h"
#include "hid/switch.h"   

using namespace daisy;
using namespace daisysp;

// === Hardware ===
static DaisySeed hw;
static Encoder   enc;
static Switch    slideSw;  

// === Effects ===
static Compressor comp;
static Svf        lpf;
static Overdrive  drive;
static ReverbSc   verb;

// === Reverb feedback parameter ===
float feedback = 0.8f;
const float fb_min = 0.6f;
const float fb_max = 0.9f;

// === Effect state ===
bool effectOn = true;

// === Audio Callback ===
static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    enc.Debounce();
    int inc = enc.Increment();
    if(inc != 0)
    {
        feedback += inc * 0.02f;
        feedback = fclamp(feedback, fb_min, fb_max);
        verb.SetFeedback(feedback);
    }

    // 
    slideSw.Debounce();
    effectOn = slideSw.Pressed(); // 

    for(size_t i = 0; i < size; i += 2)
    {
        float inMono = in[i];
        float outL, outR;

        if(effectOn)
        {
            float compOut = comp.Process(inMono);
            lpf.Process(compOut);
            float lp = lpf.Low();
            float od = drive.Process(lp);

            float wetL, wetR;
            verb.Process(od, od, &wetL, &wetR);

            outL = wetL;
            outR = wetR;
        }
        else
        {
            // BYPASS 
            outL = in[i];
            outR = in[i + 1];
        }

        out[i]     = outL;
        out[i + 1] = outR;
    }
}

// === Main ===
int main(void)
{
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

    // === Effects Init ===
    comp.Init(sample_rate);
    comp.SetThreshold(-14.0f);
    comp.SetRatio(2.0f);
    comp.SetAttack(0.005f);
    comp.SetRelease(0.2f);

    lpf.Init(sample_rate);
    lpf.SetFreq(1000.0f);
    lpf.SetRes(0.0f);
    lpf.SetDrive(1.0f);

    drive.Init();
    drive.SetDrive(0.4f);

    verb.Init(sample_rate);
    verb.SetFeedback(feedback);
    verb.SetLpFreq(18000.0f);

    enc.Init(hw.GetPin(1), hw.GetPin(2), Pin());

    // ===  (D26, pullup resistor, GND ) ===
    slideSw.Init(hw.GetPin(26));

    // === Audio Start ===
    hw.StartAudio(AudioCallback);

    while(1)
    {
        slideSw.Debounce();
        System::Delay(1);
    }
}
