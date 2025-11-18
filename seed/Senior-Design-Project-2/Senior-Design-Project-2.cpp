#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed hw;
Overdrive drive;

// 인터리빙 콜백 버전 (L,R,L,R,...)
void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    for(size_t i = 0; i < size; i += 2)
    {
        // 
        float input = in[i];  

        // 
        float sig = drive.Process(input);

        // 
        out[i]     = sig; // Left
        out[i + 1] = sig; // Right
    }
}

int main(void)
{
    hw.Configure();
    hw.Init();

    hw.SetAudioBlockSize(4);
    float sr = hw.AudioSampleRate();

    // 
    drive.Init();
    drive.SetDrive(0.7f);   // 

    hw.StartAudio(AudioCallback);

    while(1) {}
}
