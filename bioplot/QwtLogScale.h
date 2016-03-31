#include <qwt-qt4/qwt_scale_engine.h>

class QWT_EXPORT QwtLogScaleEngine: public QwtLog10ScaleEngine
{
	public:
		QwtLogScaleEngine();

		virtual void autoScale(int maxSteps, 
				double &x1, double &x2, double &stepSize) const;

		virtual QwtScaleDiv divideScale(double x1, double x2,
				int numMajorSteps, int numMinorSteps,
				double stepSize = 0.0) const;

};


