#pragma once

#include <stdint.h>

class Biquad {
	protected:
	double mX1, mX2;
	double mY1, mY2;
	double mB0, mB1, mB2, mA1, mA2;
	double mB0dif, mB1dif, mB2dif, mA1dif, mA2dif;
	int32_t mInterpolationSteps;

	void setCoefficients(int32_t steps, double a0, double a1, double a2, double b0, double b1, double b2);

	public:
	Biquad();
	virtual ~Biquad();
	void setHighShelf(int32_t steps, double cf, double sf, double gaindB, double slope, double overallGain);
	void setBandPass(int32_t steps, double cf, double sf, double resonance);
	void setHighPass(int32_t steps, double cf, double sf, double resonance);
	void setLowPass(int32_t steps, double cf, double sf, double resonance);
	double process(double in);
	void reset();
};
