#include "daisysp.h"
#include "daisy_seed.h"
#include "hid/encoder.h"
#include "hid/analog_switch.h" // 

using namespace daisy;
using namespace daisysp;

// ===== Hardware =====
static DaisySeed hw;

// ===== Controls =====
static Encoder      enc1;      // Reverb LPF cutoff
static Encoder      enc2;      // (Sidechain) Compressor Threshold (dB)
static Encoder      enc3;      // (Sidechain) Compressor Release 
static AnalogSwitch4 analog_sw; // 4bit ladder 

// ===== Effects =====
static ReverbSc  verb;
static Compressor comp;

// ===== Params / Helpers =====
inline float clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

// Reverb
float verb_fb_low   = 0.70f;     // Low feedback
float verb_fb_high  = 0.85f;     // High feedback
float verb_fb       = verb_fb_high;

float verb_lpf_hz   = 12000.0f;  // Enc1: LPF cutoff
const float LPF_MIN = 3000.0f;
const float LPF_MAX = 18000.0f;
const float LPF_STP = 500.0f;    // 500 Hz / tick

// Compressor
float comp_thr_db    = -40.0f;   // Enc2: Threshold
const float CTHR_MIN = -80.0f;
const float CTHR_MAX =   0.0f;
const float CTHR_STP =   1.0f;   // 1 dB / tick

float comp_rel_sec   = 0.100f;   // Enc3: Release
const float CREL_MIN = 0.020f;
const float CREL_MAX = 0.500f;
const float CREL_STP = 0.010f;   // 10 ms / tick

// Switch
volatile bool comp_on      = true;  // bit 2
volatile bool sidechain_on = true;  // bit 1
volatile bool reverb_on    = true;  // bit 3
volatile bool rev_hi_fb    = true;  // bit 0 (true = High FB, false = Low FB)

// ===== Audio Callback =====
static void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                          AudioHandle::InterleavingOutputBuffer out,
                          size_t                                size)
{
    // --- Controls 디바운스 ---
    enc1.Debounce();
    enc2.Debounce();
    enc3.Debounce();
    analog_sw.Debounce();

    // === Analog switch bits ===
    // bit 0 (SW4): Reverb Feedback High / Low
    // bit 1 (SW5): Compressor Sidechain ON / OFF
    // bit 2 (SW6): Compressor ON / OFF
    // bit 3 (SW7): Reverb ON / OFF
    rev_hi_fb    = analog_sw.Pressed(0); // 1: High FB, 0: Low FB
    sidechain_on = analog_sw.Pressed(1); // 1: Sidechain, 0: Single-source
    comp_on      = analog_sw.Pressed(2); // 1: Comp ON
    reverb_on    = analog_sw.Pressed(3); // 1: Reverb ON

    // === Encoder 1: Reverb LPF cutoff ===
    if(int d1 = enc1.Increment())
    {
        verb_lpf_hz = clampf(verb_lpf_hz + d1 * LPF_STP, LPF_MIN, LPF_MAX);
        verb.SetLpFreq(verb_lpf_hz);
    }

    // === Encoder 2: Compressor Threshold (dB) ===
    if(int d2 = enc2.Increment())
    {
        comp_thr_db = clampf(comp_thr_db + d2 * CTHR_STP, CTHR_MIN, CTHR_MAX);
        comp.SetThreshold(comp_thr_db);
    }

    // === Encoder 3: Compressor Release (sec) ===
    if(int d3 = enc3.Increment())
    {
        comp_rel_sec = clampf(comp_rel_sec + d3 * CREL_STP, CREL_MIN, CREL_MAX);
        comp.SetRelease(comp_rel_sec);
    }

    // === Reverb feedback (High / Low ) ===
    float desired_fb = rev_hi_fb ? verb_fb_high : verb_fb_low;
    if(desired_fb != verb_fb)
    {
        verb_fb = desired_fb;
        verb.SetFeedback(verb_fb);
    }

    // ==== raw_in (L) → Reverb → Compressor (sidechain or not) → out (mono) ====
    for(size_t i = 0; i < size; i += 2)
    {
        float inL = in[i];       // MAIN  (AUDIO_IN_1)
        float inR = in[i + 1];   // SIDECHAIN detector (AUDIO_IN_2)

        // 1) Reverb
        float wetL = inL, wetR = inL;
        if(reverb_on)
        {
            verb.Process(inL, inL, &wetL, &wetR);
        }
        // 
        float post_rev_mono = reverb_on ? 0.5f * (wetL + wetR) : inL;

        // 2) Compressor (single/sidechain )
        float y;
        if(comp_on)
        {
            if(sidechain_on)
            {
                // 
                y = comp.Process(post_rev_mono, inR);
            }
            else
            {
                // single-source
                y = comp.Process(post_rev_mono);
            }
        }
        else
        {
            // Comp bypass
            y = post_rev_mono;
        }

        // 3) Mono out (L=R)
        out[i]     = y;
        out[i + 1] = y;
    }
}

// ===== Main =====
int main(void)
{
    hw.Configure();
    hw.Init();

    hw.SetAudioBlockSize(4);
    const float sr = hw.AudioSampleRate();

    // ===== Effects init =====
    verb.Init(sr);
    verb.SetFeedback(verb_fb);
    verb.SetLpFreq(verb_lpf_hz);

    comp.Init(sr);
    comp.SetThreshold(comp_thr_db);
    comp.SetRatio(8.0f);
    comp.SetAttack(0.005f);
    comp.SetRelease(comp_rel_sec);

    // ===== Encoders init =====
    enc1.Init(hw.GetPin(2), hw.GetPin(1), Pin()); // LPF cutoff
    enc2.Init(hw.GetPin(4), hw.GetPin(3), Pin()); // Comp Threshold
    enc3.Init(hw.GetPin(5), hw.GetPin(6), Pin()); // Comp Release

    // ===== Analog switch init =====
    // ladder_out_pin: 
    dsy_gpio_pin ladder_out_pin = hw.GetPin(15); // 
    analog_sw.Init(&hw, ladder_out_pin);
    // 

    hw.StartAudio(AudioCallback);

    while(1)
    {
        System::Delay(1);
    }
}
