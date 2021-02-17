#pragma once

#include <stdint.h>

class Delay {
	double* mState;
	int32_t mIndex;
	int32_t mLength;

	public:
	Delay();
	~Delay();
	void setParameters(float rate, float time);
	double process(double x0);
};
