/*
This FX chain is our "Grungy" chain which processes raw_in using digital (d_) and analog (a_) FX in the following signal path:
raw_in -> a_preamp -> d_compressor -> d_LPF -> d_overdrive -> d_reverb -> a_distortion -> processed_out.
*/

#include "daisysp.h"
#include "daisy_seed.h"
#include "hid/encoder.h"

// Interleaved audio definitions
#define LEFT (i)
#define RIGHT (i + 1)

// Set max delay time to 0.75 of samplerate.
#define MAX_DELAY static_cast<size_t>(48000 * 0.75f)
using namespace daisysp;
using namespace daisy;
static DaisySeed hw;

// Helper Modules
static AdEnv env;
static Oscillator osc;
static Metro tick;

// Declare a DelayLine of MAX_DELAY number of floats.
static DelayLine<float, MAX_DELAY> del;

// === encoder ===
static Encoder enc; 		//
float delay_ratio = 0.75f; 	// ratio (0.0 ~ 1.0)



static void AudioCallback(AudioHandle::InterleavingInputBuffer in,
 						  AudioHandle::InterleavingOutputBuffer out,
						  size_t size){
	float osc_out, env_out, feedback, del_out, sig_out;
 
	 // === encoder ===//
 	enc.Debounce();
 	int inc = enc.Increment();

 	if(inc != 0){
 		delay_ratio += inc * 0.01f; // 0.01 per one tick
 		if(delay_ratio < 0.0f) delay_ratio = 0.0f;
 		if(delay_ratio > 1.0f) delay_ratio = 1.0f;
 		del.SetDelay(hw.AudioSampleRate() * delay_ratio);
 	}
 
 	for(size_t i = 0; i < size; i += 2){
		// When the Metro ticks:
 		// trigger the envelope to start, and change freq of oscillator.
 		if(tick.Process()){
 			float freq = rand() % 200;
 			osc.SetFreq(freq + 100.0f);
 			env.Trigger();
 		}
 		
		// Use envelope to control the amplitude of the oscillator.
 		env_out = env.Process();
 		osc.SetAmp(env_out);
 		osc_out = in[LEFT]; /////input
 
		// Read from delay line
 		del_out = del.Read();
 
		// Calculate output and feedback
 		sig_out = del_out + osc_out;
 		feedback = (del_out * 0.75f) + osc_out;
 
		// Write to the delay
 		del.Write(feedback);
 
		// Output
 		out[LEFT] = sig_out;
 		out[RIGHT] = sig_out;
 	}
}

int main(void){
	// initialize seed hardware and daisysp modules
 	float sample_rate;
 	hw.Configure();
 	hw.Init();
 	hw.SetAudioBlockSize(4);
 	sample_rate = hw.AudioSampleRate();
 	env.Init(sample_rate);
 	osc.Init(sample_rate);
 	del.Init();
 
	// Set up Metro to pulse every second
 	tick.Init(1.0f, sample_rate);
 
	// set adenv parameters
 	env.SetTime(ADENV_SEG_ATTACK, 0.001);
 	env.SetTime(ADENV_SEG_DECAY, 0.50);
 	env.SetMin(0.0);
 	env.SetMax(0.25);
 	env.SetCurve(0); // linear
 
	// Set parameters for oscillator
 	osc.SetWaveform(osc.WAVE_TRI);
 	osc.SetFreq(220);
 	osc.SetAmp(0.25);
 
	// Calculates delay rate
 	del.SetDelay(sample_rate * delay_ratio);
 
	// === D1, D2 , Pin(this is encoder pushbutton ) ) === encoder pushbutton:
	//********not decided yet.*********
 	enc.Init(hw.GetPin(1), hw.GetPin(2), Pin());
 
	// start callback
 	hw.StartAudio(AudioCallback);
 
	while(1){
		//do nothing
	}
}