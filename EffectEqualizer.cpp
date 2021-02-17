/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef DEBUG
#define LOG_TAG "Effect-Equalizer"

#include <log/log.h>
#endif

#include "EffectEqualizer.h"

#include <cmath>

#define NUM_BANDS 6

typedef struct {
	int32_t status;
	uint32_t psize;
	uint32_t vsize;
	int32_t cmd;
	int16_t data;
} reply1x4_1x2_t;

typedef struct {
	int32_t status;
	uint32_t psize;
	uint32_t vsize;
	int32_t cmd;
	int16_t data1;
	int16_t data2;
} reply1x4_2x2_t;

typedef struct {
	int32_t status;
	uint32_t psize;
	uint32_t vsize;
	int32_t cmd;
	int32_t arg;
	int16_t data;
} reply2x4_1x2_t;

typedef struct {
	int32_t status;
	uint32_t psize;
	uint32_t vsize;
	int32_t cmd;
	int32_t arg;
	int32_t data;
} reply2x4_1x4_t;

typedef struct {
	int32_t status;
	uint32_t psize;
	uint32_t vsize;
	int32_t cmd;
	int32_t arg;
	int32_t data1;
	int32_t data2;
} reply2x4_2x4_t;

typedef struct {
	int32_t status;
	uint32_t psize;
	uint32_t vsize;
	int32_t cmd;
	int16_t data[8]; // numbands (6) + 2
} reply1x4_props_t;

EffectEqualizer::EffectEqualizer()
	: mLoudnessAdjustment(10000.0), mLoudnessL(50.0), mLoudnessR(50.0),
		mNextUpdate(0), mNextUpdateInterval(1000), mPowerSquaredL(0.0), mPowerSquaredR(0.0), mFade(0)
{
	for (int32_t i = 0; i < 6; i ++) {
		mBand[i] = 0;
	}
}

int32_t EffectEqualizer::command(uint32_t cmdCode, uint32_t cmdSize, void* pCmdData, uint32_t* replySize, void* pReplyData)
{
	if (cmdCode == EFFECT_CMD_SET_CONFIG) {
		int32_t ret = Effect::configure(pCmdData);
		if (ret != 0) {
#ifdef DEBUG
			ALOGE("EFFECT_CMD_SET_CONFIG failed");
#endif
			int32_t *replyData = (int32_t *) pReplyData;
			*replyData = ret;
			return 0;
		}

		/* 100 updates per second. */
		mNextUpdateInterval = int32_t(mSamplingRate / 100.0);

		int32_t *replyData = (int32_t *) pReplyData;
		*replyData = 0;
		return 0;
	}

	if (cmdCode == EFFECT_CMD_GET_PARAM) {
		effect_param_t *cep = (effect_param_t *) pCmdData;
		if (cep->psize == 4) {
			int32_t cmd = ((int32_t *) cep)[3];
			if (cmd == EQ_PARAM_NUM_BANDS) {
				reply1x4_1x2_t *replyData = (reply1x4_1x2_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 2;
				replyData->data = NUM_BANDS;
				*replySize = sizeof(reply1x4_1x2_t);
				return 0;
			}
			if (cmd == EQ_PARAM_LEVEL_RANGE) {
				reply1x4_2x2_t *replyData = (reply1x4_2x2_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 4;
				replyData->data1 = -1000;
				replyData->data2 = 1000;
				*replySize = sizeof(reply1x4_2x2_t);
				return 0;
			}
			if (cmd == EQ_PARAM_GET_NUM_OF_PRESETS) {
				reply1x4_1x2_t *replyData = (reply1x4_1x2_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 2;
				replyData->data = 0;
				*replySize = sizeof(reply1x4_1x2_t);
				return 0;
			}
			if (cmd == EQ_PARAM_PROPERTIES) {
				reply1x4_props_t *replyData = (reply1x4_props_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 2*8;
				replyData->data[0] = (int16_t)-1; // PRESET_CUSTOM
				replyData->data[1] = (int16_t)6;  // number of bands
				for (int i = 0; i < NUM_BANDS; i++) {
					replyData->data[2 + i] = (int16_t)(mBand[i] * 100 + 0.5f); // band levels
				}
				*replySize = sizeof(reply1x4_props_t);
				return 0;
			}
		} else if (cep->psize == 8) {
			int32_t cmd = ((int32_t *) cep)[3];
			int32_t arg = ((int32_t *) cep)[4];
			if (cmd == EQ_PARAM_BAND_LEVEL && arg >= 0 && arg < NUM_BANDS) {
				reply2x4_1x2_t *replyData = (reply2x4_1x2_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 2;
				replyData->data = int16_t(mBand[arg] * 100 + 0.5f);
				*replySize = sizeof(reply2x4_1x2_t);
				return 0;
			}
			if (cmd == EQ_PARAM_CENTER_FREQ && arg >= 0 && arg < NUM_BANDS) {
				double centerFrequency = 15.625 * pow(4, arg);
				reply2x4_1x4_t *replyData = (reply2x4_1x4_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 4;
				replyData->data = int32_t(centerFrequency * 1000);
				*replySize = sizeof(reply2x4_1x4_t);
				return 0;
			}
			if (cmd == EQ_PARAM_GET_BAND) {
				double band = log(double(arg / 1000) / 15.625) / log(4);
				reply2x4_1x2_t *replyData = (reply2x4_1x2_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 2;
				replyData->data = int16_t(band);
				*replySize = sizeof(reply2x4_1x2_t);
				return 0;
			}
			if (cmd == EQ_PARAM_BAND_FREQ_RANGE && arg >= 0 && arg < NUM_BANDS) {
				double centerFrequency = 15.625 * pow(4, arg);
				double topFrequency = ((1.5f * 15.625) * pow(4, arg));
				double bottomFrequency = (centerFrequency - (topFrequency - centerFrequency));
				reply2x4_2x4_t *replyData = (reply2x4_2x4_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 8;
				replyData->data1 = int32_t(bottomFrequency * 1000);
				replyData->data2 = int32_t(topFrequency * 1000);
				*replySize = sizeof(reply2x4_2x4_t);
				return 0;
			}
		}

		/* Didn't support this command. We'll just set error status. */
#ifdef DEBUG
		ALOGE("Unknown GET_PARAM of size %d", cep->psize);
#endif
		effect_param_t *replyData = (effect_param_t *) pReplyData;
		replyData->status = -EINVAL;
		replyData->vsize = 0;
		*replySize = sizeof(effect_param_t);
		return 0;
	}

	if (cmdCode == EFFECT_CMD_SET_PARAM) {
		effect_param_t *cep = (effect_param_t *) pCmdData;
		int32_t *replyData = (int32_t *) pReplyData;

		if (cep->psize == 4 && cep->vsize == 2) {
			int32_t cmd = ((int32_t *) cep)[3];
			if (cmd == CUSTOM_EQ_PARAM_LOUDNESS_CORRECTION) {
				int16_t value = ((int16_t *) cep)[8];
				mLoudnessAdjustment = value / 100.0;
#ifdef DEBUG
				ALOGI("Setting loudness correction reference to %f dB", mLoudnessAdjustment);
#endif
				*replyData = 0;
				return 0;
			}
		}

		if (cep->psize == 8 && cep->vsize == 2) {
			int32_t cmd = ((int32_t *) cep)[3];
			int32_t arg = ((int32_t *) cep)[4];

			if (cmd == EQ_PARAM_BAND_LEVEL && arg >= 0 && arg < NUM_BANDS) {
				*replyData = 0;
				int16_t value = ((int16_t *) cep)[10];
#ifdef DEBUG
				ALOGI("Setting band %d to %d", arg, value);
#endif
				mBand[arg] = (double)value / 100.0;
				return 0;
			}
		}

		if (cep->psize == 4 && cep->vsize >= 4 && cep->vsize <= 16) {
			int32_t cmd = ((int32_t *) cep)[3];
			if (cmd == EQ_PARAM_PROPERTIES) {
				*replyData = 0;
				if ((((int16_t *) cep)[8]) >= 0) {
					*replyData = -EINVAL;
#ifdef DEBUG
					ALOGE("Asking for non-existing preset ID");
#endif
					return 0;
				}
				if ((((int16_t *) cep)[9]) != NUM_BANDS) {
					*replyData = -EINVAL;
#ifdef DEBUG
					ALOGE("Asking to manipulate invalid number of bands");
#endif
					return 0;
				}
				for (int i = 0; i < NUM_BANDS; i++) {
					mBand[i] = ((int16_t *) cep)[10 + i] / 100.0;
				}

				return 0;
			}
		}

#ifdef DEBUG
		ALOGE("Unknown SET_PARAM size %d, %d bytes", cep->psize, cep->vsize);
#endif
		*replyData = -EINVAL;
		return 0;
	}

	return Effect::command(cmdCode, cmdSize, pCmdData, replySize, pReplyData);
}

/* Source material: ISO 226:2003 curves.
 *
 * On differencing 100 dB curves against 80 dB, 60 dB, 40 dB and 20 dB, a pattern
 * can be established where each loss of 20 dB of power in signal suggests gradually
 * decreasing ear sensitivity, until the bottom is reached at 20 dB SPL where no more
 * boosting is required. Measurements end at 100 dB, which is assumed to be the reference
 * sound pressure level.
 *
 * The boost can be calculated as linear scaling of the following adjustment:
 *     20 Hz  0.0 .. 41.0 dB
 *   62.5 Hz  0.0 .. 28.0 dB
 *    250 Hz  0.0 .. 10.0 dB
 *   1000 Hz  0.0 ..  0,0 dB
 *   4000 Hz -1.0 .. -3.0 dB
 *  16000 Hz -1.5 ..  8.0 dB
 *
 * The boost will be applied maximally for signals of 20 dB and less,
 * and linearly decreased for signals 20 dB ... 100 dB, and no adjustment is
 * made for 100 dB or higher. User must configure a reference level that maps the
 * digital sound level against the SPL achieved in the ear.
 */
double EffectEqualizer::getAdjustedBand(int32_t band, double loudness) {
	/* 1st derived by linear extrapolation from (62.5, 28) to (20, 41) */
	const double adj_beg[NUM_BANDS] = {  0.0,  0.0,  0.0,  0.0, -1.0, -1.5 };
	const double adj_end[NUM_BANDS] = { 42.3, 28.0, 10.0,  0.0, -3.0,  8.0 };

	/* Add loudness adjustment */
	double loudnessLevel = loudness + mLoudnessAdjustment;
	if (loudnessLevel > 100.0) {
		loudnessLevel = 100.0;
	}
	if (loudnessLevel < 20.0) {
		loudnessLevel = 20.0;
	}
	/* Maximum loudness = no adj (reference behavior at 100 dB) */
	loudnessLevel = (loudnessLevel - 20.0) / (100.0 - 20.0);

	/* Read user setting */
	double f = mBand[band];
	/* Add compensation values */
	f += adj_beg[band] + (adj_end[band] - adj_beg[band]) * (1.0 - loudnessLevel);
	/* Account for effect smooth fade in/out */
	return f * (mFade / 100.0);
}

void EffectEqualizer::refreshBands()
{
	for (int32_t band = 0; band < (NUM_BANDS - 1); band ++) {
		/* 15.625, 62.5, 250, 1000, 4000, 16000 */
		double centerFrequency = 15.625 * pow(4, band);

		double dBL = getAdjustedBand(band + 1, mLoudnessL) - getAdjustedBand(band, mLoudnessL);
		double overallGainL = band == 0 ? getAdjustedBand(0, mLoudnessL) : 0.0;
		mFilterL[band].setHighShelf(mNextUpdateInterval, centerFrequency * 2.0, mSamplingRate, dBL, 1.0, overallGainL);

		double dBR = getAdjustedBand(band + 1, mLoudnessR) - getAdjustedBand(band, mLoudnessR);
		double overallGainR = band == 0 ? getAdjustedBand(0, mLoudnessR) : 0.0;
		mFilterR[band].setHighShelf(mNextUpdateInterval, centerFrequency * 2.0, mSamplingRate, dBR, 1.0, overallGainR);
	}
}

void EffectEqualizer::updateLoudnessEstimate(double& loudness, double powerSquared) {
	double signalPowerDb = 96.0 + log10(powerSquared / double(mNextUpdateInterval) / 281474976710656.0 + 1e-10) * 10.0;
	/* Immediate rise-time, and perceptibly linear 10 dB/s decay */
	if (loudness > signalPowerDb + 0.1) {
		loudness -= 0.1;
	} else {
		loudness = signalPowerDb;
	}
}

int32_t EffectEqualizer::process(audio_buffer_t *in, audio_buffer_t *out)
{
	double tmpL = 0.0, tmpR = 0.0;
	for (uint32_t i = 0; i < in->frameCount; i ++) {

		/* Read sound input.  */
		if (formatFloatModeInt32Mode == 0) {
			tmpL = read(in, i << 1);
			tmpR = read(in, (i << 1) + 1);

			/* Update signal loudness estimate in SPL */
			mPowerSquaredL += pow(tmpL, 2);
			mPowerSquaredR += pow(tmpR, 2);
		}
		else if (formatFloatModeInt32Mode == 1) {
			tmpL = readPcmFloat(in, i << 1);
			tmpR = readPcmFloat(in, (i << 1) + 1);

			/* Update signal loudness estimate in SPL */
			mPowerSquaredL += pow(tmpL, 2) * 50.0;
			mPowerSquaredR += pow(tmpR, 2) * 50.0;
		}
		else if (formatFloatModeInt32Mode == 2) {
			tmpL = (double)in->s32[i * 2];
			tmpR = (double)in->s32[i * 2 + 1];

			/* Update signal loudness estimate in SPL */
			mPowerSquaredL += pow(tmpL, 2);
			mPowerSquaredR += pow(tmpR, 2);
		}


		/* Evaluate EQ filters */
		for (int32_t j = 0; j < (NUM_BANDS - 1); j ++) {
			tmpL = mFilterL[j].process(tmpL);
			tmpR = mFilterR[j].process(tmpR);
		}

		/* Update EQ? */
		if (mNextUpdate == 0) {
			mNextUpdate = mNextUpdateInterval;

#ifdef DEBUG
			ALOGI("powerSqL: %lld, powerSqR: %lld", mPowerSquaredL, mPowerSquaredR);
#endif
			updateLoudnessEstimate(mLoudnessL, mPowerSquaredL);
			updateLoudnessEstimate(mLoudnessR, mPowerSquaredR);
#ifdef DEBUG
			ALOGI("loudnessL: %f, loudnessR: %f", mLoudnessL, mLoudnessR);
#endif

			/* clean up loudness cache */
			mPowerSquaredL = 0;
			mPowerSquaredR = 0;


			if (mEnable && mFade < 100) {
				mFade += 1;
			}
			else if (!mEnable && mFade > 0) {
				mFade -= 1;
			}

			refreshBands();
		}

		if (formatFloatModeInt32Mode == 0) {
			write(out, i << 1, tmpL);
			write(out, (i << 1) + 1, tmpR);
		}
		else if (formatFloatModeInt32Mode == 1) {
			writePcmFloat(out, i << 1, tmpL);
			writePcmFloat(out, (i << 1) + 1, tmpR);
		}
		else if (formatFloatModeInt32Mode == 2) {
			out->s32[i << 1] = (int32_t)tmpL;
			out->s32[(i << 1) + 1] = (int32_t)tmpR;
		}

		mNextUpdate --;
	}

	return mEnable || mFade != 0 ? 0 : -ENODATA;
}
