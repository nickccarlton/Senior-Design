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

    // Overdrive 세팅
    drive.Init();           // 샘플레이트 필요 없음
    drive.SetDrive(0.7f);   // 원하는 드라이브 강도 설정
    
    hw.StartAudio(AudioCallback);
    while(1) {}
}
