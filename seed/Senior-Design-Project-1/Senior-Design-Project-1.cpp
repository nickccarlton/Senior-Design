#include "daisysp.h"
#include "daisy_seed.h"

using namespace daisysp;
using namespace daisy;

static DaisySeed hw;
static Compressor comp;
static Svf        lpf;

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        // --- Input ---
        float inMono = in[i];

        // --- Compressor ---
        float compOut = comp.Process(inMono);

        // --- LPF ---
        lpf.Process(compOut);
        float lp = lpf.Low();

        // --- output  (L/R) ---
        out[i]     = lp;
        out[i + 1] = lp;
    }
}

int main(void)
{
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    float sample_rate = hw.AudioSampleRate();

    // --- Compressor setup ---
    comp.Init(sample_rate);
    comp.SetThreshold(-12.0f); //  Threshold  (dB)
    comp.SetRatio(4.0f);       // Compression ratio  (4:1)
    comp.SetAttack(0.01f);     // attack time??? I don't know what it is.
    comp.SetRelease(0.1f);     // Release time??? I don't know what it is. 

    // --- Low pass filter setup ---
    lpf.Init(sample_rate);
    lpf.SetFreq(1000.0f); // Cutoff frequency  (Hz)
    lpf.SetRes(0.0f);   // 
    lpf.SetDrive(1.0f); // 

    hw.StartAudio(AudioCallback);
    while(1) {}
}
