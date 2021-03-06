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
#define LOG_TAG "Effect-Virtualizer"

#include <log/log.h>
#endif

#include <cmath>

#include "EffectVirtualizer.h"

typedef struct {
	int32_t status;
	uint32_t psize;
	uint32_t vsize;
	int32_t cmd;
	int32_t data;
} reply1x4_1x4_t;

typedef struct {
	int32_t status;
	uint32_t psize;
	uint32_t vsize;
	int32_t cmd;
	int16_t data;
} reply1x4_1x2_t;

EffectVirtualizer::EffectVirtualizer()
	: mStrength(0)
{
	refreshStrength();
}

int32_t EffectVirtualizer::command(uint32_t cmdCode, uint32_t cmdSize, void* pCmdData, uint32_t* replySize, void* pReplyData)
{
	if (cmdCode == EFFECT_CMD_SET_CONFIG) {
		int32_t ret = Effect::configure(pCmdData);
		if (ret != 0) {
			int32_t *replyData = (int32_t *) pReplyData;
			*replyData = ret;
			return 0;
		}

		/* Haas effect delay -- slight difference between L & R
		 * to reduce artificialness of the ping-pong. */
		mReverbDelayL.setParameters(mSamplingRate, 0.029);
		mReverbDelayR.setParameters(mSamplingRate, 0.023);

		/* the -3 dB point is around 650 Hz, giving about 300 us to work with */
		mLocalization.setHighShelf(0, 800.0, mSamplingRate, -11.0, 0.72, 0);

		mDelayDataL = 0.0;
		mDelayDataR = 0.0;

		int32_t *replyData = (int32_t *) pReplyData;
		*replyData = 0;
		return 0;
	}

	if (cmdCode == EFFECT_CMD_GET_PARAM) {
		effect_param_t *cep = (effect_param_t *) pCmdData;
		if (cep->psize == 4) {
			int32_t cmd = ((int32_t *) cep)[3];
			if (cmd == VIRTUALIZER_PARAM_STRENGTH_SUPPORTED) {
				reply1x4_1x4_t *replyData = (reply1x4_1x4_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 4;
				replyData->data = 1;
				*replySize = sizeof(reply1x4_1x4_t);
				return 0;
			}
			if (cmd == VIRTUALIZER_PARAM_STRENGTH) {
				reply1x4_1x2_t *replyData = (reply1x4_1x2_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 2;
				replyData->data = mStrength;
				*replySize = sizeof(reply1x4_1x2_t);
				return 0;
			}
		}

#ifdef DEBUG
		ALOGE("Unknown GET_PARAM of %d bytes", cep->psize);
#endif

		effect_param_t *replyData = (effect_param_t *) pReplyData;
		replyData->status = -EINVAL;
		replyData->vsize = 0;
		*replySize = sizeof(effect_param_t);
		return 0;
	}

	if (cmdCode == EFFECT_CMD_SET_PARAM) {
		effect_param_t *cep = (effect_param_t *) pCmdData;
		if (cep->psize == 4 && cep->vsize == 2) {
			int32_t cmd = ((int32_t *) cep)[3];
			if (cmd == VIRTUALIZER_PARAM_STRENGTH) {
				mStrength = ((int16_t *) cep)[8];
				refreshStrength();
				int32_t *replyData = (int32_t *) pReplyData;
				*replyData = 0;
				return 0;
			}
		}

#ifdef DEBUG
		ALOGE("Unknown SET_PARAM of %d, %d bytes", cep->psize, cep->vsize);
#endif

		int32_t *replyData = (int32_t *) pReplyData;
		*replyData = -EINVAL;
		return 0;
	}

	return Effect::command(cmdCode, cmdSize, pCmdData, replySize, pReplyData);
}

void EffectVirtualizer::refreshStrength()
{
	mDeep = mStrength != 0;
	mWide = mStrength >= 500;

	if (mStrength != 0) {
		double start = -15.0;
		double end = -5.0;
		double attenuation = start + (end - start) * (mStrength / 1000.0);
		double roomEcho = powf(10.0, attenuation / 20.0);
		mLevel = int64_t(roomEcho * (int64_t(1) << 32));
	} else {
		mLevel = 0;
	}
}

int32_t EffectVirtualizer::process(audio_buffer_t* in, audio_buffer_t* out)
{

	double dryL = 0.0, dryR = 0.0, dataL = 0.0, dataR = 0.0;
	for (uint32_t i = 0; i < in->frameCount; i ++) {

		/* calculate reverb wet into dataL, dataR */
		if (formatFloatModeInt32Mode == 0) {
			dryL = read(in, i << 1);
			dryR = read(in, (i << 1) + 1);
		}
		else if (formatFloatModeInt32Mode == 1) {
			dryL = readPcmFloat(in, i << 1);
			dryR = readPcmFloat(in, (i << 1) + 1);
		}
		else if (formatFloatModeInt32Mode == 2) {
			dryL = (double)in->s32[i << 1];
			dryR = (double)in->s32[(i << 1) + 1];
		}

		dataL = dryL;
		dataR = dryR;

		if (mDeep) {
			/* Note: a pinking filter here would be good. */
			dataL += mDelayDataR;
			dataR += mDelayDataL;
		}

		dataL = mReverbDelayL.process(dataL);
		dataR = mReverbDelayR.process(dataR);

		if (mWide) {
			dataR = -dataR;
		}

		dataL = dataL * mLevel / 4294967296.0;
		dataR = dataR * mLevel / 4294967296.0;

		mDelayDataL = dataL;
		mDelayDataR = dataR;

		/* Reverb wet done; mix with dry and do headphone virtualization */
		dataL += dryL;
		dataR += dryR;

		if (formatFloatModeInt32Mode == 0) {
			/* Center channel. */
			int32_t center  = (dataL + dataR) / 2;
			/* Direct radiation components. */
			int32_t side = (dataL - dataR) / 2;

			/* Adjust derived center channel coloration to emphasize forward
			 * direction impression. (XXX: disabled until configurable). */
			//center = mColorization.process(center);
			/* Sound reaching ear from the opposite speaker */
			side -= mLocalization.process(side);

			write(out, i << 1, center + side);
			write(out, (i << 1) + 1, center - side);
		}
		else if (formatFloatModeInt32Mode == 1) {
			/* Center channel. */
			double center = (dataL + dataR) / 2;
			/* Direct radiation components. */
			double side = (dataL - dataR) / 2;
			
			/* Adjust derived center channel coloration to emphasize forward
			 * direction impression. (XXX: disabled until configurable). */
			//center = mColorization.process(center);
			/* Sound reaching ear from the opposite speaker */
			side -= mLocalization.process(side);

			writePcmFloat(out, i << 1, center + side);
			writePcmFloat(out, (i << 1) + 1, center - side);
		}
		else if (formatFloatModeInt32Mode == 2) {
			/* Center channel. */
			double center = (dataL + dataR) / 2;
			/* Direct radiation components. */
			double side = (dataL - dataR) / 2;
			
			/* Adjust derived center channel coloration to emphasize forward
			 * direction impression. (XXX: disabled until configurable). */
			//center = mColorization.process(center);
			/* Sound reaching ear from the opposite speaker */
			side -= mLocalization.process(side);
			
			out->s32[i << 1] = (int32_t)(center + side);
			out->s32[(i << 1) + 1] = (int32_t)(center - side);
		}
	}

	return mEnable ? 0 : -ENODATA;
}
