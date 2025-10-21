#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisySeed  hw;
Overdrive  drive;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        float input = in[0][i];  
        float sig   = drive.Process(input);
        out[0][i] = sig;
        out[1][i] = sig;
    }
}

int main(void)
{
    hw.Configure();
    hw.Init();
    hw.SetAudioBlockSize(4);

    
    drive.Init();           
    drive.SetDrive(0.7f);   
    
    hw.StartAudio(AudioCallback);
    while(1) {}
}
