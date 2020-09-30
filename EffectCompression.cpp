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

#define LOG_TAG "Effect-DRC"

#include <cutils/log.h>
#include "EffectCompression.h"

#include <math.h>



static int32_t max(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

EffectCompression::EffectCompression()
    : mCompressionRatio(2.0), mFade(0)
{
    for (int32_t i = 0; i < 2; i ++) {
        mCurrentLevel[i] = 0;
        mUserLevel[i] = 1 << 24;
    }
}

int32_t EffectCompression::command(uint32_t cmdCode, uint32_t cmdSize, void* pCmdData, uint32_t* replySize, void* pReplyData)
{
    if (cmdCode == EFFECT_CMD_SET_CONFIG) {
        int32_t *replyData = (int32_t *) pReplyData;
        int32_t ret = Effect::configure(pCmdData);
        if (ret != 0) {
            *replyData = ret;
            return 0;
        }

        /* This filter gives a reasonable approximation of A- and C-weighting
         * which is close to correct for 100 - 10 kHz. 10 dB gain must be added to result. */
        mWeigherBP[0].setBandPass(0, 2200, mSamplingRate, 0.33);
        mWeigherBP[1].setBandPass(0, 2200, mSamplingRate, 0.33);

        *replyData = 0;
        return 0;
    }

    if (cmdCode == EFFECT_CMD_SET_PARAM) {
        effect_param_t *cep = (effect_param_t *) pCmdData;
        if (cep->psize == 4 && cep->vsize == 2) {
            int32_t *replyData = (int32_t *) pReplyData;
            int16_t value = ((int16_t *)cep)[8];
			int32_t cmd = ((int32_t *)cep)[3];
			if (cmd == 0) {
				/* 1.0 .. 11.0 */
				mCompressionRatio = 1.f + value / 100.f;
				ALOGI("Compression factor set to: %f", mCompressionRatio);
				*replyData = 0;
				return 0;
			}
        }

        ALOGE("Unknown SET_PARAM of %d, %d bytes", cep->psize, cep->vsize);
        return -1;
    }

    if (cmdCode == EFFECT_CMD_SET_VOLUME && cmdSize == 8) {
        ALOGI("Setting volumes");

        if (pReplyData != NULL) {
            int32_t *userVols = (int32_t *) pCmdData;
            for (uint32_t i = 0; i < cmdSize / 4; i ++) {
                ALOGI("user volume on channel %d: %d", i, userVols[i]);
                mUserLevel[i] = userVols[i];
            }

            int32_t *myVols = (int32_t *) pReplyData;
            for (uint32_t i = 0; i < *replySize / 4; i ++) {
                ALOGI("Returning unity for our pre-requested volume on channel %d", i);
                myVols[i] = 1 << 24; /* Unity gain */
            }
        } else {
            /* We don't control volume. */
            for (int32_t i = 0; i < 2; i ++) {
                mUserLevel[i] = 1 << 24;
            }
        }

        return 0;
    }

    /* Init to current volume level on enabling effect to prevent
     * initial fade in / other shite */
    if (cmdCode == EFFECT_CMD_ENABLE) {
        ALOGI("Copying user levels as initial loudness.");
        /* Unfortunately Android calls SET_VOLUME after ENABLE for us.
         * so we can't really use those volumes. It's safest just to fade in
         * each time. */
        for (int32_t i = 0; i < 2; i ++) {
             mCurrentLevel[i] = 0;
        }
    }

    return Effect::command(cmdCode, cmdSize, pCmdData, replySize, pReplyData);
}

/* Return fixed point 16.48 */
uint64_t EffectCompression::estimateOneChannelLevel(audio_buffer_t *in, int32_t interleave, int32_t offset, Biquad& weigherBP)
{
    uint64_t power = 0;
    for (uint32_t i = 0; i < in->frameCount; i ++) {
		double tmp = 0.0;
		if (formatFloatModeInt32Mode == 0) {
			tmp = read(in, offset);
		}
		else if (formatFloatModeInt32Mode == 1) {
			tmp = readPcmFloat(in, offset);
		}
		else if (formatFloatModeInt32Mode == 2) {
			tmp = (double)in->s32[offset];
		}
        tmp = weigherBP.process(tmp);

        /* 2^24 * 2^24 = 48 */
        power += int64_t(tmp) * int64_t(tmp);
        offset += interleave;
    }

    return (power / in->frameCount);
}

int32_t EffectCompression::process(audio_buffer_t *in, audio_buffer_t *out)
{
    /* Analyze both channels separately, pick the maximum power measured. */
    uint64_t maximumPowerSquared = 0;
    for (uint32_t i = 0; i < 2; i ++) {
        uint64_t candidatePowerSquared = estimateOneChannelLevel(in, 2, i, mWeigherBP[i]);
        if (candidatePowerSquared > maximumPowerSquared) {
            maximumPowerSquared = candidatePowerSquared;
        }
    }

    /* -100 .. 0 dB. */
    double signalPowerDb = log10(maximumPowerSquared / double(int64_t(1) << 48) + 1e-10) * 10.0;

    /* Target 83 dB SPL */
    signalPowerDb += 96.0 - 83.0 + 10.0;

    /* now we have an estimate of the signal power, with 0 level around 83 dB.
     * we now select the level to boost to. */
    double desiredLevelDb = signalPowerDb / mCompressionRatio;

    /* turn back to multiplier */
    double correctionDb = desiredLevelDb - signalPowerDb;

    if (mEnable && mFade != 100) {
        mFade += 1;
    }
    if (!mEnable && mFade != 0) {
        mFade -= 1;
    }

    correctionDb *= mFade / 100.0;

    /* Reduce extreme boost by a smooth ramp.
     * New range -50 .. 0 dB */
    correctionDb -= pow(correctionDb / 100, 2.0) * (100.0 / 2.0);

    /* 40.24 */
    int64_t correctionFactor = (1 << 24) * pow(10.0, correctionDb / 20.0);

    /* Now we have correction factor and user-desired sound level. */
    for (uint32_t i = 0; i < 2; i ++) {
        /* 8.24 */
        int32_t desiredLevel = mUserLevel[i] * correctionFactor >> 24;

        /* 8.24 */
        int32_t volAdj = desiredLevel - mCurrentLevel[i];

        /* I want volume adjustments to occur in about 0.025 seconds.
         * However, if the input buffer would happen to be longer than
         * this, I'll just make sure that I am done with the adjustment
         * by the end of it. */
        int32_t adjLen = mSamplingRate / 48; // in practice, about 1100 frames
        /* This formulation results in piecewise linear approximation of
         * exponential because the rate of adjustment decreases from granule
         * to granule. */
        volAdj /= max(adjLen, in->frameCount);

        /* Additionally, I want volume to increase only very slowly.
         * This biases us against pumping effects and also tends to spare
         * our ears when some very loud sound begins suddenly. */
        if (volAdj > 0) {
            volAdj >>= 4;
        }

		double value = 0.0;
        for (uint32_t j = 0; j < in->frameCount; j ++) {
			
			 if (formatFloatModeInt32Mode == 0) {
				 value = read(in, j * 2 + i);
			 }
			 else if (formatFloatModeInt32Mode == 1) {
				 value = readPcmFloat(in, j * 2 + i);
			 }
			 else if (formatFloatModeInt32Mode == 2) {
				 value = (double)in->s32[j * 2 + i];
			 }

             value = value * mCurrentLevel[i] / 16777216.0;
             
			 
			 if (formatFloatModeInt32Mode == 0) {
				 write(out, j * 2 + i, value);
			 }
			 else if (formatFloatModeInt32Mode == 1) {
				 writePcmFloat(out, j * 2 + i, value);
			 }
			 else if (formatFloatModeInt32Mode == 2) {
				 out->s32[j * 2 + i] = (int32_t)value;
			 }
			 
             mCurrentLevel[i] += volAdj;
        }
    }

    return mEnable || mFade != 0 ? 0 : -ENODATA;
}
