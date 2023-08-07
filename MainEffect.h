#pragma once

#include "system/audio_effect.h"

#include "Biquad.h"
#include "Delay.h"
#include "Effect.h"
#include "FIR16.h"

/* 动态范围压缩总开关 */
#define DYNAMIC_RANGE_COMPRESSION_ENABLE 100
/* 动态范围压缩强度 */
#define DYNAMIC_RANGE_COMPRESSION_STRENGTH 101

/* 低音增益总开关 */
#define BASSBOOST_ENABLE 102
/* 低音增益强度 */
#define BASSBOOST_STRENGTH 103
/* 低音频点 */
#define BASSBOOST_FREQ_POINT 104

/* EQ 均衡器总开关 */
#define EQ_PARAM_TUNNER_ENABLE 105
/* EQ 均衡器单个频段 */
#define EQ_PARAM_TUNE_SINGLE_BAND 106
/* EQ 响度补偿开关 */
#define EQ_PARAM_LOUDNESS_CORRECTION_ENABLE 107
/* EQ 响度补偿参数 */
#define EQ_PARAM_LOUDNESS_CORRECTION_STRENGTH 108

/* 空间音频总开关 */
#define VIRTUALIZER_ENABLE 109
/* 空间音频类型 */
#define VIRTUALIZER_TYPE 111

/* 均衡器条数 */
#define EQ_BANDS 6

class MainEffect : public Effect {
    /* 私有部分 */
    private:
    /* 总开关 */
    int16_t compressionEnable, bassBoostEnable, equalizerEnable, loudnessEnable, virtualizerEnable;

    /* 动态范围压缩部分 */
    int32_t mUserLevel[2];
	float mCompressionRatio;
	int32_t mCompressionFade;
	int32_t mCurrentLevel[2];
	Biquad mWeigherBP[2];
	uint64_t estimateOneChannelLevel(audio_buffer_t *in, int32_t interleave, int32_t offset, Biquad& WeigherBP);
    void refreshDyanmicRangeCompression();

    /* 低音增益部分 */
    int16_t bassBoostStrength;
	double bassBoostCenterFrequency;
	Biquad biquadBoost;
	void refreshBassBoostStrength();

    /* 均衡器部分开始 */
    double mBand[6];
	Biquad mFilterL[5], mFilterR[5];
	/* Automatic equalizer */
	double mLoudnessAdjustment;
	double mLoudnessL;
	double mLoudnessR;
	int32_t mNextUpdate;
	int32_t mNextUpdateInterval;
	double mPowerSquaredL;
	double mPowerSquaredR;
	/* Smooth enable/disable */
	int32_t mEqualizerFade;
	void setBand(int32_t idx, float dB);
	double getAdjustedBand(int32_t idx, double loudness);
	void refreshBands();
	void updateLoudnessEstimate(double& loudness, double powerSquared);
    void initBandsValue();
    /* 均衡器部分结束 */

    /* 空间混响部分 */
	int16_t mVirtualizerStrength;
	bool mDeep, mWide;
	int64_t mLevel;
	Delay mReverbDelayL, mReverbDelayR;
	double mDelayDataL, mDelayDataR;
	Biquad mLocalization;
	void refreshVirtualizerStrength();

    /* 方法，选择最大值 */
    static inline int32_t max(int32_t a, int32_t b)
    {
	    return a > b ? a : b;
    }

    /* 均衡器构造 */
    typedef struct
    {
        int32_t status;
        uint32_t psize;
        uint32_t vsize;
        int32_t cmd;
        int16_t data;
    } reply1x4_1x2_t;

    typedef struct
    {
        int32_t status;
        uint32_t psize;
        uint32_t vsize;
        int32_t cmd;
        int16_t data1;
        int16_t data2;
    } reply1x4_2x2_t;

    typedef struct
    {
        int32_t status;
        uint32_t psize;
        uint32_t vsize;
        int32_t cmd;
        int32_t arg;
        int16_t data;
    } reply2x4_1x2_t;

    typedef struct
    {
        int32_t status;
        uint32_t psize;
        uint32_t vsize;
        int32_t cmd;
        int32_t arg;
        int32_t data;
    } reply2x4_1x4_t;

    typedef struct
    {
        int32_t status;
        uint32_t psize;
        uint32_t vsize;
        int32_t cmd;
        int32_t arg;
        int32_t data1;
        int32_t data2;
    } reply2x4_2x4_t;

    typedef struct
    {
        int32_t status;
        uint32_t psize;
        uint32_t vsize;
        int32_t cmd;
        int16_t data[8]; // numbands (6) + 2
    } reply1x4_props_t;

    /* 空间混响构造 */
    typedef struct
    {
        int32_t status;
        uint32_t psize;
        uint32_t vsize;
        int32_t cmd;
        int32_t data;
    } reply1x4_1x4_t;

    /* 开放部分 */
    public:
    MainEffect();
    /* 接收指令 */
    int32_t command(uint32_t cmdCode, uint32_t cmdSize, void* pCmdData, uint32_t* replySize, void* pReplyData);
    /* 执行指令 */
	int32_t process(audio_buffer_t *in, audio_buffer_t *out);
};


