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

#include "MainEffect.h"

#include <cmath>

/* 刷新低频增益强度 */
void MainEffect::refreshBassBoostStrength()
{
    /* Q = 0.5 .. 2.0 */
	biquadBoost.setLowPass(0, bassBoostCenterFrequency, mSamplingRate, 0.5 + bassBoostStrength / 666.0);
}
/* 刷新动态范围压缩强度 */
void MainEffect::refreshDyanmicRangeCompression()
{
    for (int32_t i = -1, len = 2; ++i < len;) {
		mCurrentLevel[i] = 0;
		mUserLevel[i] = 1 << 24;
	}
}
/* 刷新均衡器频段 */
void MainEffect::initBandsValue()
{
    for (int32_t i = -1, len = EQ_BANDS; ++i < len;) {
        mBand[i] = 0;
    }
}
/* 刷新空间音频强度 */
void MainEffect::refreshVirtualizerStrength()
{
    mDeep = mVirtualizerStrength != 0;
    mWide = mVirtualizerStrength >= 500;

    if (mVirtualizerStrength != 0)
    {
        double start = -15.0;
		double end = -5.0;
		double attenuation = start + (end - start) * (mVirtualizerStrength / 1000.0);
		double roomEcho = powf(10.0, attenuation / 20.0);
		mLevel = int64_t(roomEcho * (int64_t(1) << 32));
    } else {
        mLevel = 0;
    }
    
}
/* 实例音效对象 */
MainEffect::MainEffect()
    : compressionEnable(0), bassBoostEnable(0), equalizerEnable(0), loudnessEnable(0), virtualizerEnable(0), 
    mCompressionRatio(1.0), mCompressionFade(0),
    bassBoostStrength(0), bassBoostCenterFrequency(55.0),
    mLoudnessAdjustment(10000.0), mLoudnessL(50.0), mLoudnessR(50.0), mNextUpdate(0), mNextUpdateInterval(1000), mPowerSquaredL(0.0), mPowerSquaredR(0.0), mEqualizerFade(0),
    mVirtualizerStrength(0)
{
    /* 刷新动态范围压缩 */
    refreshDyanmicRangeCompression();
    /* 刷新低频增益强度 */
    refreshBassBoostStrength();
    /* 刷新均衡器频段 */
    initBandsValue();
    /* 刷新空间音效强度 */
    refreshVirtualizerStrength();
}

/* 动态范围压缩部分的小功能 */
/* Return fixed point 16.48 */
uint64_t MainEffect::estimateOneChannelLevel(audio_buffer_t *in, int32_t interleave, int32_t offset, Biquad& weigherBP)
{
	uint64_t power = 0;
	double tmp = 0.0;
	for (uint32_t i = -1, len = in->frameCount; ++i < len;) {
		
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

/* 均衡器部分的小功能 */
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
double MainEffect::getAdjustedBand(int32_t band, double loudness) {
	/* 1st derived by linear extrapolation from (62.5, 28) to (20, 41) */
	const double adj_beg[EQ_BANDS] = {  0.0,  0.0,  0.0,  0.0, -1.0, -1.5 };
	const double adj_end[EQ_BANDS] = { 42.3, 28.0, 10.0,  0.0, -3.0,  8.0 };
    double loudnessLevel = 0.0;
	/* Add loudness adjustment */
    if (loudnessEnable == 1)
    {
        loudnessLevel = loudness + mLoudnessAdjustment;
    }
    else
    {
        loudnessLevel = loudness + 100.0;
    }

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
	return f * (mEqualizerFade / 100.0);
}

void MainEffect::refreshBands()
{
	for (int32_t band = -1, len = 5; ++band < len;) {
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

void MainEffect::updateLoudnessEstimate(double& loudness, double powerSquared) {
	double signalPowerDb = 96.0 + log10(powerSquared / double(mNextUpdateInterval) / 281474976710656.0 + 1e-10) * 10.0;
	/* Immediate rise-time, and perceptibly linear 10 dB/s decay */
	if (loudness > signalPowerDb + 0.1) {
		loudness -= 0.1;
	} else {
		loudness = signalPowerDb;
	}
}

/* 获取指令 */
int32_t MainEffect::command(uint32_t cmdCode, uint32_t cmdSize, void* pCmdData, uint32_t* replySize, void* pReplyData)
{
    /* 设置音效配置 */
    if (cmdCode == EFFECT_CMD_SET_CONFIG) {
		int32_t *replyData = (int32_t *) pReplyData;
		int32_t ret = Effect::configure(pCmdData);
		if (ret != 0) {
			*replyData = ret;
			return 0;
        }

        /* 动态范围压缩部分开始 */
	    /* This filter gives a reasonable approximation of A- and C-weighting
	    * which is close to correct for 100 - 10 kHz. 10 dB gain must be added to result. */
	    mWeigherBP[0].setBandPass(0, 2200, mSamplingRate, 0.33);
	    mWeigherBP[1].setBandPass(0, 2200, mSamplingRate, 0.33);
        /* 动态范围压缩部分结束 */

        /* 均衡器部分开始 */
        /* 100 updates per second. */
		mNextUpdateInterval = int32_t(mSamplingRate / 100.0);
        /* 均衡器部分结束 */

        /* 空间压缩部分开始 */
		/* Haas effect delay -- slight difference between L & R
		 * to reduce artificialness of the ping-pong. */
		mReverbDelayL.setParameters(mSamplingRate, 0.029);
		mReverbDelayR.setParameters(mSamplingRate, 0.023);

		/* the -3 dB point is around 650 Hz, giving about 300 us to work with */
		mLocalization.setHighShelf(0, 800.0, mSamplingRate, -11.0, 0.72, 0);

		mDelayDataL = 0.0;
		mDelayDataR = 0.0;
        /* 空间压缩部分结束 */


        /* 啥也没有的话, 就收工了 */
	    *replyData = 0;
	    return 0;
	}

    /* 获取音效各阶段参数(暂时为空, 因为不需要) */
    // if (cmdCode == EFFECT_CMD_GET_PARAM)
    // {
        
    // }
    
    /* 设置音效各阶段参数 */
    if (cmdCode == EFFECT_CMD_SET_PARAM)
    {
        effect_param_t *cep = (effect_param_t *) pCmdData;
        int32_t *replyData = (int32_t *) pReplyData;
        /* 获取具体指令集 */
        if (cep->psize == 4 && cep->vsize == 2)
        {
            /* 获取指令 */
            int32_t cmd = ((int32_t *) cep)[3];
            /* 动态范围压缩开关 */
            if (cmd == DYNAMIC_RANGE_COMPRESSION_ENABLE) {
                int16_t enableSwitch = ((int16_t *) cep)[8];
                int16_t oldSwitch = compressionEnable;
                /* 对比新旧值, 要是发生变动就重新赋值 */
                if (enableSwitch != oldSwitch)
                {
                    compressionEnable = enableSwitch;
                    // refreshDyanmicRangeCompression();
                }
                *replyData = 0;
                return 0;
            }
            /* 动态范围压缩强度 */
            if (cmd == DYNAMIC_RANGE_COMPRESSION_STRENGTH) {
                /* 获取压缩强度 */
                int16_t value = ((int16_t *) cep)[8];
                /* 1.0 .. 11.0 */
                mCompressionRatio = 1.f + value / 100.f;
                *replyData = 0;
                return 0;
            }
            /* 低频增益开关 */
            if (cmd == BASSBOOST_ENABLE)
            {
                int16_t enableSwitch = ((int16_t *) cep)[8];
                int16_t oldSwitch = bassBoostEnable;
                /* 对比新旧值, 要是发生变动就重新赋值 */
                if (enableSwitch != oldSwitch)
                {
                    bassBoostEnable = enableSwitch;
                    // refreshBassBoostStrength();
                }
                *replyData = 0;
                return 0;
            }
            /* 低频增益频点 */
            if (cmd == BASSBOOST_FREQ_POINT)
            {
                int16_t freqPoint = ((int16_t *) cep)[8];
                if (bassBoostCenterFrequency != (double)freqPoint)
                {
                    bassBoostCenterFrequency = (double)freqPoint;
                    refreshBassBoostStrength();
                }
				*replyData = 0;
				return 0;
            }
            /* 低频增益强度 */
            if (cmd == BASSBOOST_STRENGTH)
            {
                int16_t strength = ((int16_t *) cep)[8];
                if (bassBoostStrength != strength)
                {
                    bassBoostStrength = strength;
                    refreshBassBoostStrength();
                }
				*replyData = 0;
				return 0;
            }
            /* 均衡器总开关 */
            if (cmd == EQ_PARAM_TUNNER_ENABLE)
            {
                int16_t enableSwitch = ((int16_t *) cep)[8];
                int16_t oldSwitch = equalizerEnable;
                /* 对比新旧值, 要是发生变动就重新赋值 */
                if (enableSwitch != oldSwitch)
                {
                    equalizerEnable = enableSwitch;
                    // initBandsValue();
                }
                *replyData = 0;
                return 0;
            }
            /* 均衡器响度补偿开关 */
            if (cmd == EQ_PARAM_LOUDNESS_CORRECTION_ENABLE)
            {
                int16_t enableSwitch = ((int16_t *) cep)[8];
                int16_t oldSwitch = loudnessEnable;
                /* 对比新旧值, 要是发生变动就重新赋值 */
                if (enableSwitch != oldSwitch)
                {
                    loudnessEnable = enableSwitch;
                }
                *replyData = 0;
                return 0;
            }
            /* 均衡器响度补偿强度 */
            if (cmd == EQ_PARAM_LOUDNESS_CORRECTION_STRENGTH)
            {
                int16_t loudnessStrength = ((int16_t *) cep)[8];
                mLoudnessAdjustment = loudnessStrength / 100.0;
                *replyData = 0;
                return 0;
            }
            /* 空间混响开关 */
            if (cmd == VIRTUALIZER_ENABLE)
            {
                int16_t enableSwitch = ((int16_t *) cep)[8];
                int16_t oldSwitch = virtualizerEnable;
                /* 对比新旧值, 要是发生变动就重新赋值 */
                if (enableSwitch != oldSwitch)
                {
                    virtualizerEnable = enableSwitch;
                }
                *replyData = 0;
                return 0;
            }
            /* 空间混响类型 */
            if (cmd == VIRTUALIZER_TYPE)
            {
                mVirtualizerStrength = ((int16_t *) cep)[8];
                refreshVirtualizerStrength();
                *replyData = 0;
                return 0;
            }
        }
        /* 均衡器配置 */
        if (cep->psize == 8 && cep->vsize == 2) {
            /* 获取指令 */
            int32_t cmd = ((int32_t *) cep)[3];
            /* 均衡器单个频率条设置 */
            if (cmd == EQ_PARAM_TUNE_SINGLE_BAND)
            {
                int32_t eqBandIndex = ((int32_t *) cep)[4];
                if (eqBandIndex >= 0 && eqBandIndex < EQ_BANDS)
                {
                    int16_t eqBandLevel = ((int16_t *) cep)[10];
                    mBand[eqBandIndex] = (double)eqBandLevel / 100.0;
                }
                *replyData = 0;
                return 0;
            }
        }
        /* 啥也没有的话, 就收工了 */
	    *replyData = 0;
	    return 0;
    }

    return Effect::command(cmdCode, cmdSize, pCmdData, replySize, pReplyData);
}

/* 处理根据上面指令获取到的参数 */
int32_t MainEffect::process(audio_buffer_t *in, audio_buffer_t *out)
{
    /* 总输入输出参数 */
    double mainInputL = 0.0, mainInputR = 0.0;
    /* 动态范围压缩参数 */
    uint64_t maximumPowerSquared = 0;
	// double compressionValueL = 0.0, compressionValueR = 0.0;
    int32_t compAdjL = 0, compAdjR = 0;
    /* 低音增益参数 */
    // double bassBoostDryL = 0.0, bassBoostDryR = 0.0;
    /* 空间混响参数 */
    double virtCacheL = 0.0, virtCacheR = 0.0;
    if (compressionEnable == 1) {
        for (uint32_t i = -1, len = 2; ++i < len;)
        {
            uint64_t candidatePowerSquared = estimateOneChannelLevel(in, 2, i, mWeigherBP[i]);
            if (candidatePowerSquared > maximumPowerSquared)
            {
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
        if (mCompressionFade != 100) {
            mCompressionFade += 1;
        }

        correctionDb *= mCompressionFade / 100.0;

        /* Reduce extreme boost by a smooth ramp.
         * New range -50 .. 0 dB */
        correctionDb -= pow(correctionDb / 100, 2.0) * (100.0 / 2.0);

        /* 40.24 */
        int64_t correctionFactor = (1 << 24) * pow(10.0, correctionDb / 20.0);

        /* 设置左右声道 */
		/* 8.24 */
		int32_t desiredLevelL = mUserLevel[0] * correctionFactor >> 24;
        int32_t desiredLevelR = mUserLevel[1] * correctionFactor >> 24;
        /* 8.24 */
        compAdjL = desiredLevelL - mCurrentLevel[0];
        compAdjR = desiredLevelR - mCurrentLevel[1];

        /* I want volume adjustments to occur in about 0.025 seconds.
		 * However, if the input buffer would happen to be longer than
		 * this, I'll just make sure that I am done with the adjustment
		 * by the end of it.
		 */
		int32_t adjLen = mSamplingRate / 48; // in practice, about 1100 frames
        /* This formulation results in piecewise linear approximation of
		 * exponential because the rate of adjustment decreases from granule
		 * to granule.
		 */
		compAdjL /= max(adjLen, in->frameCount);
        compAdjR /= max(adjLen, in->frameCount);
        /* Additionally, I want volume to increase only very slowly.
		 * This biases us against pumping effects and also tends to spare
		 * our ears when some very loud sound begins suddenly.
		 */
		if (compAdjL > 0) {
			compAdjL >>= 4;
		}
		if (compAdjR > 0) {
			compAdjR >>= 4;
		}
    }

    for (uint32_t i = -1, len = in->frameCount; ++i < len;)
    {
        /* calculate reverb wet into dataL, dataR */
        /* 获取正在播放的音频效果 */
        if (formatFloatModeInt32Mode == 0)
        {
            mainInputL = read(in, i << 1);
            mainInputR = read(in, (i << 1) + 1);
            // if (equalizerEnable == 1)
            // {
            //     /* Update signal loudness estimate in SPL */
            //     mPowerSquaredL += pow(mainInputL, 2);
            //     mPowerSquaredR += pow(mainInputR, 2);
            // }
        }
        else if (formatFloatModeInt32Mode == 1)
        {
            mainInputL = readPcmFloat(in, i << 1);
            mainInputR = readPcmFloat(in, (i << 1) + 1);
            // if (equalizerEnable == 1)
            // {
            //     /* Update signal loudness estimate in SPL */
            //     mPowerSquaredL += pow(mainInputL, 2) * 50.0;
            //     mPowerSquaredR += pow(mainInputR, 2) * 50.0;
            // }
        }
        else if (formatFloatModeInt32Mode == 2)
        {
            mainInputL = (double)in->s32[i << 1];
            mainInputR = (double)in->s32[(i << 1) + 1];
            // if (equalizerEnable == 1)
            // {
            //     /* Update signal loudness estimate in SPL */
            //     mPowerSquaredL += pow(mainInputL, 2);
            //     mPowerSquaredR += pow(mainInputR, 2);
            // }
        }

        /* 动态范围压缩 */
        if (compressionEnable == 1)
        {
            mainInputL = mainInputL * mCurrentLevel[0] / 16777216.0;
            mainInputR = mainInputR * mCurrentLevel[1] / 16777216.0;
        }
        
        /* 低音增益部分 */
        if (bassBoostEnable == 1)
        {
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
            double bassBoost = biquadBoost.process(mainInputL + mainInputR);
            mainInputL += bassBoost;
            mainInputR += bassBoost;
        }
        /* 空间混响部分 */
        if (virtualizerEnable == 1)
        {
            virtCacheL = mainInputL;
            virtCacheR = mainInputR;

            if (mDeep)
            {
                /* Note: a pinking filter here would be good. */
                virtCacheL += mDelayDataR;
                virtCacheR += mDelayDataL;
            }

            virtCacheL = mReverbDelayL.process(virtCacheL);
            virtCacheR = mReverbDelayR.process(virtCacheR);

            if (mWide)
            {
                virtCacheR = -virtCacheR;
            }

            virtCacheL = virtCacheL * mLevel / 4294967296.0;
            virtCacheR = virtCacheR * mLevel / 4294967296.0;

            mDelayDataL = virtCacheL;
            mDelayDataR = virtCacheR;

            /* Reverb wet done; mix with dry and do headphone virtualization */
            virtCacheL += mainInputL;
            virtCacheR += mainInputR;

			/* Center channel. */
            /* 人造中心点 */
			int32_t center  = (virtCacheL + virtCacheR) / 2;
			/* Direct radiation components. */
			int32_t side = (virtCacheL - virtCacheR) / 2;

			/* Adjust derived center channel coloration to emphasize forward
			 * direction impression. (XXX: disabled until configurable). */
			//center = mColorization.process(center);
			/* Sound reaching ear from the opposite speaker */
			side -= mLocalization.process(side);

            /* 最终处理 */
            mainInputL = center + side;
            mainInputR = center - side;
        }

        /* 均衡器部分 */
        if (equalizerEnable == 1)
        {
            if (formatFloatModeInt32Mode == 0)
            {
                /* Update signal loudness estimate in SPL */
                mPowerSquaredL += pow(mainInputL, 2);
                mPowerSquaredR += pow(mainInputR, 2);
            }
            else if (formatFloatModeInt32Mode == 1)
            {
                /* Update signal loudness estimate in SPL */
                mPowerSquaredL += pow(mainInputL, 2) * 50.0;
                mPowerSquaredR += pow(mainInputR, 2) * 50.0;
            }
            else if (formatFloatModeInt32Mode == 2)
            {
                /* Update signal loudness estimate in SPL */
                mPowerSquaredL += pow(mainInputL, 2);
                mPowerSquaredR += pow(mainInputR, 2);
            }
            /* Evaluate EQ filters */
            for (int32_t bandIndex = -1, len = 5; ++bandIndex < len;)
            {
                mainInputL = mFilterL[bandIndex].process(mainInputL);
                mainInputR = mFilterR[bandIndex].process(mainInputR);
            }

            /* Update EQ? */
            /* 是否更新 均衡器 */
            if (mNextUpdate == 0)
            {
                mNextUpdate = mNextUpdateInterval;

                updateLoudnessEstimate(mLoudnessL, mPowerSquaredL);
                updateLoudnessEstimate(mLoudnessR, mPowerSquaredR);

                /* clean up loudness cache */
                mPowerSquaredL = 0;
                mPowerSquaredR = 0;

                if (mEqualizerFade < 100)
                {
                    mEqualizerFade += 1;
                }
                // else if (mEqualizerFade > 0)
                // {
                //     mEqualizerFade -= 1;
                // }

                /* 更新频段条 */
                refreshBands();
            }
        }

        /* 将处理后的效果写入 */
        if (formatFloatModeInt32Mode == 0)
        {
            write(out, i << 1, mainInputL);
            write(out, (i << 1) + 1, mainInputR);
        }
        else if (formatFloatModeInt32Mode == 1)
        {
            writePcmFloat(out, i << 1, mainInputL);
            writePcmFloat(out, (i << 1) + 1, mainInputR);
        }
        else if (formatFloatModeInt32Mode == 2)
        {
            out->s32[i << 1] = (int32_t)(mainInputL);
            out->s32[(i << 1) + 1] = (int32_t)(mainInputR);
        }

        if (compressionEnable == 1)
        {
            mCurrentLevel[0] += compAdjL;
            mCurrentLevel[1] += compAdjR;
        }

        if (equalizerEnable == 1)
        {
            mNextUpdate--;
        }
        
    }
    
    /* 最后就是返回了 */
    // return mEnable || mCompressionFade != 0 || mEqualizerFade != 0 ? : -ENODATA;
    /* 最后就是返回了 */
    /* 设置三个部分的开关 */
    bool compEffectEnable = mCompressionFade != 0 && compressionEnable;
    bool eqEffectEnable = mEqualizerFade != 0 && equalizerEnable;
    // return mEnable || mCompressionFade != 0 || mEqualizerFade != 0 ? : -ENODATA;
    return mEnable && (compEffectEnable || bassBoostEnable || eqEffectEnable || virtualizerEnable) ? 0 : -ENODATA;
}
