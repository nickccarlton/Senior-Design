#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// Hardware and effect objects
static DaisySeed hw;
static Compressor comp;

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        // 채널 매핑: CH1 = MAIN, CH2 = SIDECHAIN INPUT
        float main_in = in[i];       // AUDIO_IN_1
        float side_in = in[i + 1];   // AUDIO_IN_2 (TRIGGER INPUT)

        // 
        float compressed_out = comp.Process(main_in, side_in);

        // 
        out[i]     = compressed_out; // Left
        out[i + 1] = compressed_out; // Right
    }
}

int main(void)
{
    hw.Configure();
    hw.Init();
    float sample_rate = hw.AudioSampleRate();

    comp.Init(sample_rate);
    comp.SetThreshold(-40.0f);  // //
    comp.SetRatio(6.0f); ///////
    comp.SetAttack(0.005f);
    comp.SetRelease(0.100f);
    comp.AutoMakeup(false);
    comp.SetMakeup(0.0f);

    hw.StartAudio(AudioCallback);
    while(true) {}
}
