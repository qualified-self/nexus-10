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

#ifndef CSPECTRUMPLOT_H_
#define CSPECTRUMPLOT_H_

#include <QWidget>
#include <QTimer>

#include <qwt-qt4/qwt_plot.h>
#include <qwt-qt4/qwt_plot_spectrogram.h>

#include "CSpectrumData.h"

class CSpectrumPlot : public QWidget
{
	Q_OBJECT
	
public:
	CSpectrumPlot(const char *title = "a title", const float HzMin=6, const float HzMax=16);
	~CSpectrumPlot();

	void AddSpectrum( const float time, const float * spectrum, const size_t num, const float binsize )
	{
		if ( theData == NULL ) return;
		theData->AddSpectrum(time, spectrum, num, binsize );
	}

public slots:
	/** update the plot */
	void update();

private:
	/** set up the plot */
	void SetupPlot(const char * title, const float HzMin, const float HzMax);

	/** set the axis scale such that the latest interval is shown */
	void SetAxisScale();

	QwtPlot            *thePlot;        /**< contains the actual plotting area */
	QwtPlotSpectrogram *theSpectrogram; /**< contains the spectrogram plot */
	CSpectrumData      *theData;        /**< contains the data to be plotted */

	QTimer * theTimer; /**< timer to update the graph a few times per second */
};


#endif // Menu_h_
