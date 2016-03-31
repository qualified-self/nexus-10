/** @Copyright

This software and associated documentation files (the "Software") are 
copyright ©  2010 Koninklijke Philips Electronics N.V. All Rights Reserved.

A copyright license is hereby granted for redistribution and use of the 
Software in source and binary forms, with or without modification, provided 
that the following conditions are met:
 1. Redistributions of source code must retain the above copyright notice, 
    this copyright license and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, 
    this copyright license and the following disclaimer in the documentation 
    and/or other materials provided with the distribution.
 3. Neither the name of Koninklijke Philips Electronics N.V. nor the names 
    of its subsidiaries may be used to endorse or promote products derived 
    from the Software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS 
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, 
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef CBIOPLOT_H_
#define CBIOPLOT_H_

#include <vector>

#include <QWidget>
#include <QTimer>

#include <qwt-qt4/qwt_plot.h>
#include <qwt-qt4/qwt_plot_curve.h>

#include <boost/thread/mutex.hpp>

//! maximum number of curves
#define BIOPLOT_MAX_CURVES 8

/** \file CBioPlot.cpp
 * implements class to plot a simple graph
 */

class CBioPlot : public QWidget
{
	Q_OBJECT
	
public:
	/** construct a plot
	 * @param[in] num_curves       The number of curves to plot in the figure
	 * @param[in] inval_length     The length of the time interval to display
	 * @param[in] title            Title to show at the left axis
	 * @param[in] log_scale        Do we want a log scale on the y-axis
	 * @param[in] grid             Do we want a grid on the y-axis
	 * @param[in] update_freq      Update frequency of the graph (see SetUpdateFreq())
	 */
	CBioPlot( const size_t num_curves = 1, const double ival_len = 5.0, 
	          const char * title = "title", const bool log_scale = false, 
	          const bool grid = false, const int pen_width = 1,
            const double update_freq = -50.0 );
	~CBioPlot();

	/** record a sample
	 * @param[in] time      The time coordinate of the sample
	 * @param[in] sample    The value of the sample to record
	 * @param[in] num_curve The number of the curve the sample should be added to
	 */
	void AddSample( const float time, const float sample, const size_t num_curve = 0 );

	/** replace the currently recorded samples by the ones specified
	 * @param[in] Xvalues     The X coordinates of the samples
	 * @param[in] YValues     The Y coordinates of the samples
	 * @param[in] num_curve   The number of the curve the samples
	 */
	void SetSamples( 
			const std::vector<double> & Xvalues, const std::vector<double> & Yvalues, 
			const size_t num_curve = 0 );

	/** replace the currently recorded samples by the ones specified
	 * @param[in] Xvalues     The X coordinates of the samples
	 * @param[in] YValues     The Y coordinates of the samples
	 * @param[in] num         number of values in Xvalues and Yvalues
	 * @param[in] num_curve   The number of the curve the samples
	 */
	void SetSamples( 
			const double Xvalues[], const double Yvalues[], const size_t num,
			const size_t num_curve = 0 );

	/** remove all data and clears the graph 
	 *  does not reset the axis settings etc
	 */
	void Clear();

	/** remove all data for a specific curve
	 *  does not reset the axis settings etc
	 */
	void Clear( const size_t num_curve );

	/** Enable or disable progressive autoscaling on the X-axes
	 * if enabled, the scale on the x-axis will shift to show only the latest samples
	 * if disabled, the entire range of data will be shown
	 */
	void enableProgressiveXAxisScaling( const bool enable = true );

	/** Enable or disable the 2nd (right) y-axis
	 */
	void enableY2axis( const bool enable = true );
	/** let the specified curve use the Y2 axis instead of Y1
	 */
	void curveUseY2axis( const size_t curve, const bool enable = true );

	/** Set and fix the limits on the y-axis
	 */
	void setYAxisScale( const double min, const double max, const double step = 0 );

	/** Let the Y-axes float (i.e., logscale are not foxed to start with a power of 10
	 */
	void setYAxisFloat( const bool val = true );

	/** Set the update frequency of the graph (in Hz)
	 *    positive values mean a fixed updating frequency in Hz
	 *    negative values mean automatic updating depending on the range shown
	 *    i.e. if ival_len=5s and update_freq=-10, update every 0.5s
	 */
	void SetUpdateFreq( const double freq = -50.0 );

	/* set color of curve */
	void SetCurveColor( const size_t curve, const uint8_t R, const uint8_t G, const uint8_t B);

	/* set width of curve */
	void SetCurveWidth( const size_t curve, const float a_width );

	/* set width of all curves */
	void SetAllCurveWidth( const float a_width );


public slots:
	/** update the plot
	 */
	void Update();

private:
	/** Build the plot elements and setup the plot
	 */
	void SetupPlot(const char * title);
	/** Calculate the correct range of the time axis
	 */
	void SetAxisScale();
	/** Add an extra curve to the graph
	 */
	void AddCurve( int pen_width );
	/** Return the nuber of curves present 
	 */
	size_t NumCurves() const { return theDataX.size(); };

	QTimer * theTimer; //!< Timer to trigger updating of the graph

	typedef QwtArray<double> data_t; //!< Type for the data 

	std::vector<data_t> theDataX; //!< For each curve, the x (==time) data
	std::vector<data_t> theDataY; //!< For each curve, the y (==sample) data

	QwtPlot      *thePlot;                //!< the actual plot to draw 
	std::vector<QwtPlotCurve*> theCurves; //!< the curves to plot in the graph
	std::vector<QPen*>         thePens;   //!< the pens for each curve

	bool   ScaleXAxisProgressive;  //!< set to tue to automatically sclae the X-axis
	double theTimeRange;    //!< the lenght of the interval to show on the time axis
	double theSkipFraction; //!< the fraction of the time scape that should skip

	//! mutex to lock the data while it is being written
	mutable boost::mutex data_lock;
};



#endif // Menu_h_
