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

#include <iostream>
#include <cstdio>

#include <vector>

#include <assert.h>

#include <QString>
#include <QHBoxLayout>
#include <QThread>
#include <QMetaObject>
#include <QApplication>

#include <qwt-qt4/qwt_text.h>
#include <qwt-qt4/qwt_plot_canvas.h>
#include <qwt-qt4/qwt_plot_grid.h>
#include <qwt-qt4/qwt_scale_widget.h>
#include <qwt-qt4/qwt_scale_engine.h>

#include "CBioPlot.h"
//#include "QwtLogScale.h"

using std::cerr;
using std::endl;
using std::vector;

/** \file CBioPlot.cpp
 */

//! left margin of the graph (space reserved for tick labels)
#define MARGIN 64
//! maximum number of samples to store
#define MAX_SIZE 12000

//! colours to use for different curves
static const char * const curve_colors[BIOPLOT_MAX_CURVES] = {
	"#f00", "#0a0", "#00f", "#0af", "#fa0", "#555", "#a0f", "#5f0"
};


const static QFont axis_font( "Deja Sans", 8 ); // 8 PC 12 Nokia

/////////////////////////////////////////////////////////////////////


CBioPlot::CBioPlot( const size_t num_curves, const double ival_len, 
	const char * title, const bool log_scale, const bool grid,
	const int pen_width, const double update_freq )
	: QWidget        ()
	, theTimer       ( NULL     )
	, thePlot        ( NULL     )
	, ScaleXAxisProgressive ( true     )
	, theTimeRange   ( ival_len )
	, theSkipFraction( 0.2      )
{
	// create the plot, axes, etc
	SetupPlot(title);

	// check that everything went well
	if ( thePlot==NULL )
	{
		cerr << "Plot wasn't created correctly!" << endl;
		throw;
	}

	if ( log_scale )
	{
		thePlot->setAxisScaleEngine( QwtPlot::yLeft, new QwtLog10ScaleEngine );
		//thePlot->setAxisScaleEngine( QwtPlot::yLeft, new QwtLogScaleEngine );
	}

	if ( grid )
	{
		// grid
		QwtPlotGrid *the_grid = new QwtPlotGrid;
		the_grid->enableX(true);
		the_grid->enableY(true);
		the_grid->enableXMin(false);
		the_grid->enableYMin(false);
		the_grid->setMajPen(QPen(Qt::gray, 0, Qt::DotLine));
		the_grid->setMinPen(QPen(Qt::gray, 0, Qt::DotLine));
		the_grid->attach(thePlot);
	}

	// add a curve to the plot
	for ( size_t i = 0; i < num_curves; i++ ) AddCurve( pen_width );

	//thePlot->setFrameStyle( QFrame::StyledPanel );

	// put the plot in a HBox
	QHBoxLayout *myLayout = new QHBoxLayout;
	myLayout->setSpacing(0);
	myLayout->setContentsMargins(0,0,0,0);
	myLayout->addWidget( thePlot );
	setLayout( myLayout );

	// set a time to update the graph
	theTimer = new QTimer();
	connect(theTimer, SIGNAL(timeout()), this, SLOT(Update()));
	SetUpdateFreq( update_freq );
}

CBioPlot::~CBioPlot()
{
	boost::mutex::scoped_lock lock(data_lock);

	for ( size_t i = 0; i < theCurves.size(); i++)
	{
		if (theCurves[i]!=NULL) delete theCurves[i];
	}
	if (thePlot !=NULL) delete thePlot;
	if (theTimer!=NULL) delete theTimer;
}

void CBioPlot::SetupPlot(const char * title)
{
	QwtText label;

	// start with a clean plot
	if (thePlot == NULL) delete thePlot;

	// create a new plot
	thePlot = new QwtPlot();

	// we do our own painting
	thePlot->setAutoReplot( false );

	// for faster painting, apparently
	thePlot->canvas()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);

	// plot can be rescaled
	thePlot->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

	// plot background
	thePlot->setCanvasBackground(QColor(Qt::white));

	// font to use for axes etc
	thePlot->setAxisFont(  QwtPlot::yLeft,   axis_font );
	thePlot->setAxisFont(  QwtPlot::yRight,  axis_font );
	thePlot->setAxisFont(  QwtPlot::xBottom, axis_font );
	thePlot->setAxisFont(  QwtPlot::xTop,    axis_font );

	// x-axis label
	label = QString("t (s)");
	label.setFont( axis_font );
	thePlot->setAxisTitle( QwtPlot::xBottom, label );

	// y-axis label
	label = QString(title);
	label.setFont( axis_font );
	thePlot->setAxisTitle( QwtPlot::yLeft, label );

	// set the left margin
	thePlot->axisScaleDraw( QwtPlot::yLeft )->setMinimumExtent( MARGIN );

	// max 5 ticks on the vertical axis
	thePlot->setAxisMaxMajor( QwtPlot::yLeft,  5);
	thePlot->setAxisMaxMajor( QwtPlot::yRight, 5);

	// setup the scale of the time axis
	SetAxisScale();

	//// set autoscaling for the y-axis, but start at [-1,1]
	thePlot->setAxisScale    ( QwtPlot::yLeft, -1, 1 );
	thePlot->setAxisAutoScale( QwtPlot::yLeft );

	// plot everything we made so far
	thePlot->replot();
}

void CBioPlot::AddCurve( int pen_width )
{
	boost::mutex::scoped_lock lock(data_lock);

	// sanity check
	if ( thePlot == NULL )
	{
		cerr << "Woops, no plot!" << endl;
		throw;
	}

	// don't add more crves if we are at the max
	if ( NumCurves() >= BIOPLOT_MAX_CURVES ) return;

	size_t curve_num = NumCurves();

	// create a new curve for this data set
	QwtPlotCurve * curve = new QwtPlotCurve();
	theCurves.push_back( curve );

	// add the curve to the plot
	curve->attach(thePlot);

	// set the color of the curve
	QPen * pen = new QPen;
	thePens.push_back( pen );

	pen->setColor( QColor( curve_colors[curve_num] ) );
	pen->setWidth( pen_width );

	/* set pen to curve */
	curve->setPen( *pen );

	// add a data set for this curve and bind it to the curve
	theDataX.push_back( data_t() );
	theDataY.push_back( data_t() );
	curve->setData( theDataX[curve_num], theDataY[curve_num] );

	// change style or quality of the curve
	//curve->setStyle( QwtPlotCurve::Dots );
	//curve->setRenderHint(QwtPlotItem::RenderAntialiased);

}

/** set color of specific curve 
 */
void CBioPlot::SetCurveColor( const size_t curve, const uint8_t R, const uint8_t G, const uint8_t B)
{
	if (curve > NumCurves()) 
	{
		std::cerr << "Curve " << curve << " doesn't exist" << std::endl;
		return;
	}

	QColor color(R,G,B);
	thePens[curve]->setColor( color );
	theCurves[curve]->setPen( *thePens[curve] );
}

/** set width of curve */
void CBioPlot::SetCurveWidth( const size_t curve, const float a_width )
{
	if (curve > NumCurves()) 
	{
		std::cerr << "Curve " << curve << " doesn't exist" << std::endl;
		return;
	}
	thePens[curve]->setWidthF( a_width );
	theCurves[curve]->setPen( *thePens[curve] );
}

/* set width of all curves */
void CBioPlot::SetAllCurveWidth( const float a_width )
{
	for (size_t curve=0; curve<NumCurves(); curve++)
	{
		SetCurveWidth(curve,a_width);
	}
}

/** Set the correct scale of the time axis
 */
void CBioPlot::SetAxisScale()
{
	// do we use automatic X-axis scaling?
	if (!ScaleXAxisProgressive) return;

	const double move_time = theSkipFraction*theTimeRange;

	// nothing defined yet
	if ( theDataX.size() == 0 )  return;

	// find the maximum over all curves
	float max = 0;
	{
		boost::mutex::scoped_lock lock(data_lock);

		for ( size_t c = 0; c < theDataX.size(); c++ )
		{
			if ( theDataX[c].size() > 0 )
			{
				if (theDataX[c].back() > max) max = theDataX[c].back();
			}
		}
	}
	max = (int) (max/move_time + 1) * move_time;

	// minimum is simply a fixed interval back from max
	float min = max - theTimeRange;

	// make sure we don't show any negative time at the start of the program
	if ( min < 0 )
	{
		max -= min;
		min = 0;
	}

	//cerr << "Axisscale to " << min << "," << max << endl;
	thePlot->setAxisScale( QwtPlot::xBottom, min, max );

	// if we've stored too many samples, get rid of some old ones
	min -= 0.1*theSkipFraction; // small margin at the left of graph
	for ( size_t c = 0; c < NumCurves(); c++ )
	{
		boost::mutex::scoped_lock lock(data_lock);

		if ( theDataX[c].size()>0 && theDataX[c].at(0) < min  )
		{
			// find out how many points to remove
			int i = 0;
			while ( i < theDataX[c].size() && theDataX[c].at(i) < min ) { i++; }

			theDataX[c].erase( theDataX[c].begin(), theDataX[c].begin()+i );
			theDataY[c].erase( theDataY[c].begin(), theDataY[c].begin()+i );
		}
	}
}

void CBioPlot::Clear()
{
	for ( size_t c = 0; c < NumCurves(); c++)
	{
		Clear(c);
	}
}

void CBioPlot::Clear( const size_t num_curve )
{
	// sanity check
	if ( num_curve >= NumCurves() ) return;

	boost::mutex::scoped_lock lock(data_lock);

	theDataX[num_curve].clear();
	theDataY[num_curve].clear();
}

void CBioPlot::AddSample( const float time, const float sample, const size_t curve_num )
{
	// sanity check
	if ( curve_num >= NumCurves() ) return;

	//assert( time>=0 );
	if (time < -1e-7) {
		fprintf(stderr,"t %.3f < 0.0 sample %f curve_num %u!!!\n",time,sample,curve_num);
		return;
	}

	boost::mutex::scoped_lock lock(data_lock);

        //fprintf(stderr,"# AddSample t %f sample %f curve %d\n",time,sample,curve_num);
        
	// add empty points if necessary (i.e., if packets were lost)
	if ( theDataX[curve_num].size() > 2 )
	{
		double delta_t = theDataX[curve_num].at(1) - theDataX[curve_num].at(0);

		size_t num_missing = 0;
		if (theDataX[curve_num].back() + 1.5*delta_t < time ) {
		  //fprintf(stderr,"# Missing %.3f [s]",time - theDataX[curve_num].back()) ;
		}
		while ( theDataX[curve_num].back() + 1.5*delta_t < time )
		{
			assert( theDataX[curve_num].size() < 1e6 );
			//if ( num_missing>500 ) {
			//	cerr << "# Warning: more than 500 missing samples; bailing out" <<endl;
			//	//abort();  // too strong reaction
			//}

			theDataX[curve_num].push_back( theDataX[curve_num].back()+delta_t );
			theDataY[curve_num].push_back( theDataY[curve_num].back()         );
			num_missing++;
		}
		//if (num_missing>0) cerr << "# Inserted " << num_missing << " points" << endl;
	}

	if ( theDataX[curve_num].size()>0 && time < theDataX[curve_num].back() )
	{
		cerr << "# WARNING: data points for curve "<< curve_num<<" are going back in time (last time=" 
			<< theDataX[curve_num].back() << ", new time=" << time << ")!!!" << endl;
		theDataX[curve_num].clear();
		theDataY[curve_num].clear();
	}

	// save the time and the sample
	theDataX[curve_num].push_back(time);
	theDataY[curve_num].push_back(sample);

}

void CBioPlot::SetSamples( const std::vector<double> & Xvalues, const std::vector<double> & Yvalues,
		const size_t num_curve )
{
	// sanity check
	if ( num_curve >= NumCurves() ) return;

	// if you are using this function, you most problably don't want to use Progressive Scaling
	enableProgressiveXAxisScaling(false);

	// clear out old data
	Clear(num_curve);

	// lock data and put in the new data
	{
		boost::mutex::scoped_lock lock(data_lock);

		theDataX[num_curve] = QVector<double>::fromStdVector( Xvalues );
		theDataY[num_curve] = QVector<double>::fromStdVector( Yvalues );
	}

	Update();
}

void CBioPlot::SetSamples(
		const double Xvalues[], const double Yvalues[], const size_t num,
		const size_t num_curve )
{
	// sanity check
	if ( num_curve >= NumCurves() ) return;

	// if you are using this function, you most problably don't want to use Progressive Scaling
	enableProgressiveXAxisScaling(false);

	// clear out old data
	Clear(num_curve);

	// lock data and put in the new data
	{
		boost::mutex::scoped_lock lock(data_lock);

		assert( theDataX[num_curve].size()==0 );
		assert( theDataY[num_curve].size()==0 );

		theDataX[num_curve].reserve( num );
		for (size_t i = 0; i < num; i++) theDataX[num_curve].append( Xvalues[i] );

		theDataY[num_curve].reserve( num );
		for (size_t i = 0; i < num; i++) theDataY[num_curve].append( Yvalues[i] );
	}

	Update();
}

void CBioPlot::Update()
{
	// make sure that Update() is only called in the GUI thread
	if ( QThread::currentThread() != qApp->thread() )
	{
		// schedule an update event to be run in the GUI thread
		QMetaObject::invokeMethod( this, "Update", Qt::QueuedConnection );
		return;
	}

	// set the new data
	for ( size_t i = 0; i < NumCurves(); i++ )
	{
		theCurves[i]->setData( theDataX[i], theDataY[i] );
	}

	// rescale
	SetAxisScale();

	// replot
	boost::mutex::scoped_lock lock(data_lock);
	thePlot->replot();
}

void CBioPlot::enableProgressiveXAxisScaling( const bool enable )
{
	ScaleXAxisProgressive = enable;
	if (!enable)
	{
		thePlot->setAxisAutoScale( QwtPlot::xBottom );
	}
}

void CBioPlot::enableY2axis( const bool enable )
{
	thePlot->enableAxis( QwtPlot::yRight, enable );
}

void CBioPlot::curveUseY2axis( const size_t curve, const bool enable )
{
	if ( curve >= NumCurves() ) throw;
	if ( enable )
		theCurves[curve]->setYAxis( QwtPlot::yRight );
	else
		theCurves[curve]->setYAxis( QwtPlot::yLeft );
}

void CBioPlot::setYAxisScale( const double min, const double max, const double step )
{
	thePlot->setAxisScale( QwtPlot::yLeft, min, max, step );
}


void CBioPlot::setYAxisFloat( const bool val )
{
	thePlot->axisScaleEngine(QwtPlot::yLeft )->setAttribute(QwtScaleEngine::Floating,val);
	thePlot->axisScaleEngine(QwtPlot::yRight)->setAttribute(QwtScaleEngine::Floating,val);
}

void CBioPlot::SetUpdateFreq( const double freq )
{
	if ( theTimer == NULL ) return;

	theTimer->stop();

	if ( freq<=0 )
	{
		theTimer->start( (int) (1000.0*theTimeRange/(-freq)) );
	}
	else
	{
		theTimer->start( (int) (1000.0/freq) );
	}

}

