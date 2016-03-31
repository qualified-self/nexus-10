
#include "RunningAverage.h"
#include <stdlib.h>
#include <limits>

RunningAverage::RunningAverage(int n)
{
	// initialize min/max
	_max = std::numeric_limits<float>::max();
	_min = std::numeric_limits<float>::min();

	_size = n;
	_ar = (int*) malloc(_size * sizeof(int));
	clear();
}

RunningAverage::~RunningAverage()
{
	free(_ar);
}

// resets all counters
void RunningAverage::clear() 
{ 
	_cnt = 0;
	_idx = 0;
	_sum = 0.0;
	for (int i = 0; i< _size; i++) _ar[i] = 0.0;  // needed to keep addValue simple
}

// adds a new value to the data-set
void RunningAverage::addValue(int f)
{
	_sum -= _ar[_idx];
	_ar[_idx] = f;
	_sum += _ar[_idx];
	_idx++;
	if (_idx == _size) _idx = 0;  // faster than %
	if (_cnt < _size) _cnt++;

	// move min/max
	if(f < _min) _min = f;
	if(f > _max) _max = f;
}

// returns the average of the data-set added sofar
float RunningAverage::getAverage()
{
	if (_cnt == 0) return 0; // NaN ?  math.h
	return _sum / _cnt;
}

float RunningAverage::getMin() {
	return _min;
}

float RunningAverage::getMax() {
	return _max;
}

// fill the average with a value
// the param number determines how often value is added (weight)
// number should preferably be between 1 and size
void RunningAverage::fillValue(int value, int number)
{
	clear();
	for (int i = 0; i < number; i++) 
	{
		addValue(value);
	}
}
// END OF FILE
