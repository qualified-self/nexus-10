#include "QwtLogScale.h"

#include <qwt-qt4/qwt_math.h>
#include <qwt-qt4/qwt_scale_map.h>

#include <cmath>
#include <cstdio>
#include <iostream>

// set to 1 to enable debugging output
#define __QLS_DEBUG 0

#if __QLS_DEBUG>0
  #define debug(format, args...) fprintf (stderr, format, ##args)
#else
  #define debug(a,args...)
#endif

#define _UNUSED __attribute__((unused))

QwtLogScaleEngine::QwtLogScaleEngine()
	: QwtLog10ScaleEngine()
{
}

void QwtLogScaleEngine::autoScale(int maxNumSteps _UNUSED, 
		double &x1, double &x2, double &stepSize _UNUSED) const
{
	debug("autoScale1: maxNumSteps=%i, x1=%.2f, x2=%.2f, stepSize=%.2f\n",\
	      maxNumSteps,x1,x2,stepSize);
	
	// calculate x1new: largest {1,2,5}10^N smaller than x1
	double x1new;
	int x1pow10  = ::pow(10, (int) floor(::log10(x1)) ); // closest power of 10 smaller than x1
	double x1red = x1/x1pow10; // guaranteed to be 1<=x1red<10
	if      (x1red>=5) x1new = 5*x1pow10;
	else if (x1red>=2) x1new = 2*x1pow10;
	else               x1new = 1*x1pow10;
	debug("          : x1pow10=%i, x1red=%.2f, x1new=%.2f\n", x1pow10, x1red, x1new);

	// calculate x2new: smallest {1,2,5}10^N larger than x2
	double x2new;
	int x2pow10  = ::pow(10, (int) floor(::log10(x2)) ); // closest power of 10 smaller than x1
	double x2red = x2/x2pow10; // guaranteed to be 1<=x2red<10
	if      (x2red<=2) x2new =  2*x2pow10;
	else if (x2red<=5) x2new =  5*x2pow10;
	else               x2new = 10*x2pow10;
	debug("          : x2pow10=%i, x2red=%.2f, x2new=%.2f\n", x2pow10, x2red, x2new);
	
	// adjust values by 1% to make sure the ticks at the edges are visible
	x1new *= 0.99;
	x2new *= 1.01;

	debug("autoScale3: adjust [%.2f,%.2f] to [%.2f,%.2f]\n", x1,x2, x1new,x2new);
	debug("\n");

	// return updated values
	x1 = x1new;
	x2 = x2new;
}

QwtScaleDiv QwtLogScaleEngine::divideScale(double x1, double x2,
		int maxMajSteps, int maxMinSteps,
		double stepSize) const 
{
	debug("divideScale    :   x1=%.2f, x2=%.2f, maxMajSteps=%i, maxMinSteps=%i, "
	      "stepSize=%.2f\n",x1,x2,maxMajSteps,maxMinSteps,stepSize);

	// call the divideScale method of the parent class
	QwtScaleDiv sd1 = QwtLog10ScaleEngine::divideScale(x1,x2,maxMajSteps,maxMinSteps,stepSize);

	// because QwtScaleDiv doesn't allow us to directly edit the ticks, we need
	// to copy them out of the object, adjust them, and create a new
	// QwtScaleDiv object
	QwtValueList ticksMinor = sd1.ticks(QwtScaleDiv::MinorTick);
	QwtValueList ticksMajor = sd1.ticks(QwtScaleDiv::MajorTick);

	// adjust ticks, such that lowest and largest value always are a major tick
	if (ticksMinor.size()>=2 && ticksMajor.size()<3)
	{
		ticksMajor.prepend( ticksMinor.takeFirst() );
		ticksMajor.append ( ticksMinor.takeLast() );
	}

	// create the new QwtScaleDiv
	QwtValueList ticks[QwtScaleDiv::NTickTypes];
	ticks[QwtScaleDiv::MinorTick] = ticksMinor;
	ticks[QwtScaleDiv::MajorTick] = ticksMajor;
	QwtScaleDiv sd2( sd1.lowerBound(), sd1.upperBound(), ticks);

	// debugging output
	debug("divideScale res: ival=[%.2f,%.2f]\n", sd2.lowerBound(), sd2.upperBound() );
	debug("      maj ticks: ");
	for (ssize_t i=0; i<sd2.ticks(QwtScaleDiv::MajorTick).size(); i++) 
		debug("%.2f, ", sd2.ticks(QwtScaleDiv::MajorTick).operator[](i));
	debug("\n      min ticks: ");
	for (ssize_t i=0; i<sd2.ticks(QwtScaleDiv::MinorTick).size(); i++) 
		debug("%.2f, ", sd2.ticks(QwtScaleDiv::MinorTick).operator[](i));
	debug("\n\n");

	return sd2;
}


