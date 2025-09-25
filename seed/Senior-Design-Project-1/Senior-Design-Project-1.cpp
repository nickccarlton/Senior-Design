#include "daisysp.h"
#include "daisy_seed.h"

using namespace daisysp;
using namespace daisy;

static DaisySeed hw;
static Compressor comp;
static Svf        lpf;
static ReverbSc   verb;


static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)


{ float output_gain = 2.0f;  // 

    for(size_t i = 0; i < size; i += 2)
    {
        // --- Input ---
        float inMono = in[i];

        // --- Compressor ---
        float compOut = comp.Process(inMono);

        // --- Low Pass Filter ---
        lpf.Process(compOut);
        float lp = lpf.Low();

        // --- reverb ---
        float wetL, wetR;
        verb.Process(lp, lp, &wetL, &wetR);

        // --- Output (L/R) ---
        out[i]     = wetL* output_gain;;       
        out[i + 1] = wetR* output_gain;;
    }
}

int main(void)
{
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

    // ㅇㅇ --- Compressor setup ---ㅇㅇ//
    comp.Init(sample_rate);
    comp.SetThreshold(-14.0f);
    comp.SetRatio(2.0f);
    comp.SetAttack(0.005f);
    comp.SetRelease(0.2f);

    // ㅇㅇ--- Low pass filter setup ---ㅇㅇ//
    lpf.Init(sample_rate);
    lpf.SetFreq(1000.0f);
    lpf.SetRes(0.0f);
    lpf.SetDrive(1.0f);

    // ㅇㅇ--- reverb setup ---ㅇㅇ//
    verb.Init(sample_rate);
    verb.SetFeedback(0.75f);   // 
    verb.SetLpFreq(6000.0f);   // 

    hw.StartAudio(AudioCallback);
    while(1) {}
}
