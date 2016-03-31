#ifndef RunningAverage_h
#define RunningAverage_h

#define RUNNINGAVERAGE_LIB_VERSION "0.2.02"

class RunningAverage 
{
	public:
	RunningAverage(void);
	RunningAverage(int);
	~RunningAverage();
	void clear();
	void addValue(int);
	float getAverage();
	float getMin();
	float getMax();
	void fillValue(int, int);

protected:
	int _size;
	int _cnt;
	int _idx;
	float _sum;
	float _min;
	float _max;
	int * _ar;
};

#endif
// END OF FILE
