#include "daisysp.h"
#include "daisy_seed.h"

using namespace daisysp;
using namespace daisy;

#define MAX_DELAY static_cast<size_t>(48000 * 0.75f)
#define LEFT (i)
#define RIGHT (i + 1)

static DaisySeed hw;
static DelayLine<float, MAX_DELAY> delay;
static ReverbSc verb;

float delay_buf[MAX_DELAY];

static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    float inMono, delayed, wetL, wetR;

    for (size_t i = 0; i < size; i += 2)
    {
        // === MONO INPUT ===
        inMono = in[i]; // Only use Audio In 1

        // === DELAY ===
        delayed = delay.Read();
        delay.Write(inMono + delayed * 0.5f); // 50% feedback

        // === REVERB ===
        verb.Process(delayed, delayed, &wetL, &wetR); // same mono in → stereo out

        // === OUTPUT ===
        out[i]     = wetL; // Left
        out[i + 1] = wetR; // Right
    }
}

int main(void)
{
    float sample_rate;

    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);
    sample_rate = hw.AudioSampleRate();

    delay.Init();
    delay.SetDelay(sample_rate * 0.4f); // ~400ms delay

    verb.Init(sample_rate);
    verb.SetFeedback(0.85f);      // long tail
    verb.SetLpFreq(16000.0f);     // high-frequency rolloff

    hw.StartAudio(AudioCallback);
    while (1) {}
}
