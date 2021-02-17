#pragma once

#include <cstdlib>
#include <iostream>
// #include <ctime>
#include "system/audio.h"
#include "hardware/audio_effect.h"

class Effect {
	private:
	effect_buffer_access_e mAccessMode;

	protected:
	bool mEnable;
	double mSamplingRate;
	int8_t formatFloatModeInt32Mode;
	uint8_t mPreviousRandom;

	/* High-passed triangular probability density function.
	 * Output varies from -0xff to 0xff.
	 * Support for 8.0 or lower android system */
	inline int32_t triangularDither8() {
		// srand((unsigned int)time(NULL));
		uint8_t newRandom = rand() % 256;
		int32_t rnd = int32_t(mPreviousRandom) - int32_t(newRandom);
		mPreviousRandom = newRandom;
		return rnd;
	}

	/* AudioFlinger only gives us 16-bit PCM for now. */
	inline double read(audio_buffer_t *in, int32_t idx) {
		return (double)in->s16[idx] * 256.0;
	}
	
	/* Android 9 use PCM_FLOAT input/output. so we make a method to read 
	 * input buffer correctly. */
	inline double readPcmFloat(audio_buffer_t *in, int32_t idx) {
		return double(in->f32[idx] * 1e6);
	}
	
	/* Android 9 use PCM_FLOAT input/output. so we make a method to write
	 * output buffer correctly. */
	inline void writePcmFloat(audio_buffer_t *out, int32_t idx, double sample) {
		out->f32[idx] = float(sample / 1e6);
	}

	/* AudioFlinger only expects 16-bit PCM for now. */
	inline void write(audio_buffer_t *out, int32_t idx, double sample) {
		sample = (sample + (double)triangularDither8()) / 256;
		if (sample > 32767) {
			sample = 32767;
		}
		if (sample < -32768) {
			sample = -32768;
		}
		out->s16[idx] = (int16_t)sample;
	}

	int32_t configure(void *pCmdData);

	public:
	Effect();
	virtual ~Effect();
	virtual int32_t process(audio_buffer_t *in, audio_buffer_t *out) = 0;
	virtual int32_t command(uint32_t cmdCode, uint32_t cmdSize, void* pCmdData, uint32_t* replySize, void* pReplyData) = 0;
};
