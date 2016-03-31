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

#include <qwt-qt4/qwt_text.h>
#include <qwt-qt4/qwt_plot_canvas.h>
#include <qwt-qt4/qwt_scale_widget.h>
#include <qwt-qt4/qwt_color_map.h>

#include <QHBoxLayout>

#include "CSpectrumPlot.h"
#include "CSpectrumData.h"

/** \file CSpectrumPlot.cpp
 *  Implements a plot os spectrum as a function of time
 */

using std::cerr;
using std::endl;

//! size in pixels reserved for the tick labels on the left axis
#define MARGIN 54
//! how often to replot the spectrum (in ms)
#define UPDATE_TIME 500
//! time axis range (seconds)
#define TIME_RANGE 30

//! front to use for axes etc
const static QFont axis_font( "Deja Sans", 8 );

/* Make Spectrum plot with 'title' and frequency on Y-axis from 'HzMin' to 'HzMax' */
CSpectrumPlot::CSpectrumPlot(const char * title, float HzMin, float HzMax)
	: QWidget        ()
	, thePlot        ( NULL )
	, theSpectrogram ( NULL )
	, theData        ( NULL )
{
	// set up the plot, the axes, etc
	SetupPlot(title, HzMin, HzMax);

	// initialize the spectrum data and the actual plot
	theData = new CSpectrumData();
	theSpectrogram = new QwtPlotSpectrogram();
	theSpectrogram->setData( *theData );
	theSpectrogram->attach( thePlot );

	// colours to use for the spectrum
	QwtLinearColorMap colorMap(Qt::darkBlue, Qt::white);
	colorMap.addColorStop(0.3, Qt::cyan);
	colorMap.addColorStop(0.45, Qt::green);
	colorMap.addColorStop(0.6, Qt::yellow);
	colorMap.addColorStop(0.8, Qt::red);
	theSpectrogram->setColorMap(colorMap);

	// set up the legend for the colours...
	QwtScaleWidget *rightAxis = thePlot->axisWidget(QwtPlot::yRight);
	rightAxis->setColorBarEnabled(true);
	rightAxis->setColorMap(theSpectrogram->data().range(), theSpectrogram->colorMap());

	// ... and put it at the right side
	thePlot->setAxisScale(QwtPlot::yRight,
			theSpectrogram->data().range().minValue(),
			theSpectrogram->data().range().maxValue() );
	thePlot->enableAxis(QwtPlot::yRight);


	// put the plot in a HBox
	QHBoxLayout *myLayout = new QHBoxLayout;
	myLayout->setSpacing(0);
	myLayout->setContentsMargins(0,0,0,0);
	myLayout->addWidget( thePlot );
	setLayout( myLayout );

	// update every second
	theTimer = new QTimer();
	connect(theTimer, SIGNAL(timeout()), this, SLOT(update()));
	theTimer->start( UPDATE_TIME );
}


CSpectrumPlot::~CSpectrumPlot()
{
	if ( thePlot        != NULL ) delete thePlot;
	if ( theSpectrogram != NULL ) delete theSpectrogram;
	if ( theData        != NULL ) delete theData;
	if ( theTimer       != NULL ) delete theTimer;
}

void CSpectrumPlot::SetupPlot(const char * title, float HzMin, float HzMax)
{
	QwtText label;

	// start with a clean plot
	if( thePlot != NULL ) delete thePlot;

	// create a new plot
	thePlot = new QwtPlot();

	// for faster painting, apparently
	thePlot->canvas()->setAttribute(Qt::WA_PaintOutsidePaintEvent, true);

	// plot can be rescaled
	thePlot->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );

	// set the left margin
	thePlot->axisScaleDraw( QwtPlot::yLeft )->setMinimumExtent( MARGIN );

	// font to use for axes etc
	thePlot->setAxisFont(  QwtPlot::yLeft,   axis_font );
	thePlot->setAxisFont(  QwtPlot::yRight,  axis_font );
	thePlot->setAxisFont(  QwtPlot::xBottom, axis_font );

	/*
	// x-axis label
	label = QString("t (s)");
	label.setFont( axis_font );
	thePlot->setAxisTitle( QwtPlot::xBottom, label );
	*/

	// y-axis label
	label = QString(title);
	label.setFont( axis_font );
	thePlot->setAxisTitle( QwtPlot::yLeft, label );

	// setup the scale of the axes
	SetAxisScale();
	thePlot->setAxisScale( QwtPlot::yLeft, HzMin, HzMax );

	// plot everything we made so far
	thePlot->replot();
}

/** Set the correct scale of the time axis 
 */
void CSpectrumPlot::SetAxisScale()
{
	const double move_time = 0.2*TIME_RANGE;

	// find the latest time we have data for, and round it upwards
	double max = 0.0;
	if ( theData != NULL ) max = theData->GetMaxTime();
	max = (int) (max/move_time + 1) * move_time;

	// minimum is simply a fixed interval back from max
	double min = max - TIME_RANGE;

	// make sure we don't show any negative time at the start of the program
	if ( min < 0 )
	{
		max -= min;
		min = 0;
	}

	thePlot->setAxisScale( QwtPlot::xBottom, min, max );
}



void CSpectrumPlot::update()
{
	if (thePlot==NULL) return;
	SetAxisScale();
	thePlot->replot();
}
