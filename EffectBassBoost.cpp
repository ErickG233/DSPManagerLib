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
#define LOG_TAG "Effect-BassBoost"

#include <log/log.h>
#endif

#include "EffectBassBoost.h"

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

EffectBassBoost::EffectBassBoost()
	: mStrength(0), mCenterFrequency(55.0)
{
	refreshStrength();
}

int32_t EffectBassBoost::command(uint32_t cmdCode, uint32_t cmdSize, void* pCmdData, uint32_t* replySize, void* pReplyData)
{
	if (cmdCode == EFFECT_CMD_SET_CONFIG) {
		int32_t ret = Effect::configure(pCmdData);
		if (ret != 0) {
			int32_t *replyData = (int32_t *) pReplyData;
			*replyData = ret;
			return 0;
		}

		int32_t *replyData = (int32_t *) pReplyData;
		*replyData = 0;
		return 0;
	}

	if (cmdCode == EFFECT_CMD_GET_PARAM) {
		effect_param_t *cep = (effect_param_t *) pCmdData;
		if (cep->psize == 4) {
			int32_t cmd = ((int32_t *) cep)[3];
			if (cmd == BASSBOOST_PARAM_STRENGTH_SUPPORTED) {
				reply1x4_1x4_t *replyData = (reply1x4_1x4_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 4;
				replyData->data = 1;
				*replySize = sizeof(reply1x4_1x4_t);
				return 0;
			}
			if (cmd == BASSBOOST_PARAM_STRENGTH) {
				reply1x4_1x2_t *replyData = (reply1x4_1x2_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 2;
				replyData->data = mStrength;
				*replySize = sizeof(reply1x4_1x2_t);
				return 0;
			}
			if (cmd == 133) {
				reply1x4_1x2_t *replyData = (reply1x4_1x2_t *) pReplyData;
				replyData->status = 0;
				replyData->vsize = 2;
				replyData->data = (int16_t) mCenterFrequency;
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
			if (cmd == BASSBOOST_PARAM_STRENGTH) {
				mStrength = ((int16_t *) cep)[8];
#ifdef DEBUG
				ALOGI("New strength: %d", mStrength);
#endif
				refreshStrength();
				int32_t *replyData = (int32_t *) pReplyData;
				*replyData = 0;
				return 0;
			}
			if (cmd == 133) {
				mCenterFrequency = ((int16_t* )cep)[8];
#ifdef DEBUG
				ALOGI("New center freq: %d", mCenterFrequency);
#endif
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

void EffectBassBoost::refreshStrength()
{
	/* Q = 0.5 .. 2.0 */
	mBoost.setLowPass(0, mCenterFrequency, mSamplingRate, 0.5 + mStrength / 666.0);
}

int32_t EffectBassBoost::process(audio_buffer_t* in, audio_buffer_t* out)
{
	double dryL = 0.0, dryR = 0.0;
	for (uint32_t i = 0; i < in->frameCount; i ++) {

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

		/* Original LVM effect was far more involved than this one.
		* This effect is mostly a placeholder until I port that, or
		* something else. LVM process diagram was as follows:
		*
		* in -> [ HPF ] -+-> [ mono mix ] -> [ BPF ] -> [ compressor ] -> out
		*                `-->------------------------------>--'
		*
		* High-pass filter was optional, and seemed to be
		* tuned at 55 Hz and upwards. BPF is probably always tuned
		* at the same frequency, as this would make sense.
		*
		* Additionally, a compressor element was used to limit the
		* mixing of the boost (only!) to avoid clipping.
		*/
		 
		double boost = mBoost.process(dryL + dryR);

        
		if (formatFloatModeInt32Mode == 0) {
			write(out, i << 1, dryL + boost);
			write(out, (i << 1) + 1, dryR + boost);
		}
		else if (formatFloatModeInt32Mode == 1) {
			writePcmFloat(out, i << 1, dryL + boost);
			writePcmFloat(out, (i << 1) + 1, dryR + boost);
		}
		else if (formatFloatModeInt32Mode == 2) {
			out->s32[i << 1] = (int32_t)(dryL + boost);
			out->s32[(i << 1) + 1] = (int32_t)(dryL + boost);
		}

	}

    return mEnable ? 0 : -ENODATA;
}
