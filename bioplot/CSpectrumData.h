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

#ifndef _CSPECTRUMDATA_H_
#define _CSPECTRUMDATA_H_

#include <qwt-qt4/qwt_raster_data.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

using boost::shared_ptr;

/** class that contains power spectrum data from an EEg signal 
 */
class CSpectrumData : public QwtRasterData
{
public:
	typedef std::vector<float> spectrum_t;

	//! default constructor
	CSpectrumData();
	//! copy constructor
	CSpectrumData( 
		shared_ptr< std::vector<spectrum_t> > power_data,
		shared_ptr< std::vector<float>      > data_t,
		shared_ptr< float                   > binsize
	);

	//! destructor is empty:w
	~CSpectrumData() {};

	//! copy method is needed by QwtSpectrumPlot
	virtual QwtRasterData * copy() const;

	//! add a spectrum at time t
	void AddSpectrum( const float t, const float * spectrum, const size_t num, const float binsize );

	//! return the first time we have data for
	float GetMinTime() const 
	{ 
		if ( theDataTime==NULL || theDataTime->size()==0 )  return 0.0f;
		float min = theDataTime->front();
		return min;
	}

	//! return the last time we have data for
	float GetMaxTime() const 
	{ 
		if ( theDataTime==NULL || theDataTime->size()==0 )  return 0.0f;
		float max = theDataTime->back();
		return max;
	}

	//! return the frequency range we can handle (note: we're returning logarithms!)
	QwtDoubleInterval range() const { return QwtDoubleInterval(-6,0); };

	//! return the value at time t and frequency f
	double value( double t, double f ) const;


protected:
	// note: we have to use shared_ptrs here, because the spectogram plotting
	// function gets a copy of the object, which will be made once, while the
	// data will be updated all the time.

	//! shared pointers to the sprectrum data and x coordinates of each point
	shared_ptr< std::vector<spectrum_t> > theDataSpectrum;
	shared_ptr< std::vector<float>      > theDataTime;

	//! the number of Hertz per bin in the spectrums
	shared_ptr<float> theHertzPerBin;

	//! mutex to lock the data while it is being written
	mutable boost::mutex data_lock;
};


#endif
