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

#include "CSpectrumData.h"

#include <iostream>
#include <cmath>

using std::cerr;
using std::endl;
using std::vector;



/** \file CSpectrumData.cpp
 * implements a class to hold spectral data as a function of time and frequency.
 */

/** default constructor
 */
CSpectrumData::CSpectrumData()
	: QwtRasterData()
{
	shared_ptr< vector<spectrum_t> > data_spectrum( new vector<spectrum_t> );
	shared_ptr< vector<float>      > data_time    ( new vector<float>      );
	shared_ptr< float              > binsize      ( new float(-1)          );

	theDataSpectrum = data_spectrum;
	theDataTime     = data_time;
	theHertzPerBin  = binsize;
}

/** copy constructor
 */
CSpectrumData::CSpectrumData( 
	shared_ptr< std::vector<spectrum_t> > power_data,
	shared_ptr< std::vector<float>      > data_x,
	shared_ptr< float                   > binsize
)
	: QwtRasterData()
	, theDataSpectrum( power_data )
	, theDataTime    ( data_x     )
	, theHertzPerBin ( binsize )
{
}

/** copy the current object
 */  
QwtRasterData * CSpectrumData::copy() const
{
	CSpectrumData * a_copy = new CSpectrumData( theDataSpectrum, theDataTime, theHertzPerBin );
	return a_copy;
}

/** Add a spectrum at time t
 */
void CSpectrumData::AddSpectrum(
	const float t,          //!< [in] time 
	const float * spectrum, //!< [in] pointer to array containing spectrum
	const size_t num,       //!< [in] number of element in the spectrum array
	const float binsize     //!< [in] Hertz per spectrogram bin
)
{
	if ( theDataTime==NULL) return;
	if ( theDataSpectrum==NULL) return;

	// add empty points if necessary (i.e., if we have lost some packets/sampel points)
	if ( theDataTime->size() >= 2)
	{
		boost::mutex::scoped_lock lock(data_lock);

		float delta_t = theDataTime->at(1) - theDataTime->at(0);
		size_t num_missing = 0;
		while ( theDataTime->back() + 1.5*delta_t < t )
		{
			// fill in missing sample with copy of last spectrum
			theDataTime->push_back(  theDataTime->back()+delta_t );
			theDataSpectrum->push_back( theDataSpectrum->back()   );
			num_missing++;
		}
		//if ( num_missing>0 ) cerr << "Inserted " << num_missing << " spectrum points" << endl;

	}

	// store the resolution of the vertical axis (i.e., number of Herz
	// represented by each FFT bin)
	if ( *theHertzPerBin < 0 )  
		*theHertzPerBin = binsize;
	else
	{
		if ( binsize != *theHertzPerBin )
		{
			cerr << "Spectral binsize changes!" << endl;
			throw;
		}
	}

	/* copy the spectrum into a vector, and normalize! */
	std::vector<float> spec;
	double total = 0;
	for ( size_t i = 0; i < num; i++ ) {
		total += spectrum[i];
	}
	for ( size_t i = 0; i < num; i++ ) {
		spec.push_back( spectrum[i]/total );
	}

	/* save the time and spectrum */
	{
		boost::mutex::scoped_lock lock(data_lock);

		theDataTime->push_back( t );
		theDataSpectrum->push_back( spec );
	}

	/* remove some elements if the data becomes too large */
	while ( theDataTime->size() > 250 ) 
	{
		const size_t del_num = 25;

		boost::mutex::scoped_lock lock(data_lock);

		theDataTime->erase( theDataTime->begin(), theDataTime->begin()+del_num );
		theDataSpectrum->erase( theDataSpectrum->begin(), theDataSpectrum->begin()+del_num );
	}
}

/** return the value at time t and frequency f
 */
double CSpectrumData::value( double t, double f ) const
{
	if ( t < 0 ) return 0;
	if ( f < 0 ) return 0;

	boost::mutex::scoped_lock lock( data_lock);

	if ( theDataTime == NULL || theDataTime->size()<2 ) return 0;
	if ( t < GetMinTime()  ||  t > GetMaxTime() ) return 0;

	if ( *theHertzPerBin < 0 ) 
	{
		cerr << "Spectum binsize was not initialized yet! This can't happen" << endl;
		throw;
	}

	/* calculate the correct index in the time dimension
	 * we assume that the time step is constant!
	 */
	double t_stepsize = theDataTime->at(1)-theDataTime->at(0);
	double t_start    = theDataTime->at(0);
	size_t i = (size_t) ( (t-t_start)/t_stepsize );

	//! frequency is simply in Hertz, so bin is found by rounding downwards
	size_t j = (size_t) ( f / *theHertzPerBin );

	if ( i >= theDataSpectrum->size() )      return 0;
	if ( j >= (*theDataSpectrum)[i].size() ) return 0;

	float val = theDataSpectrum->at(i).at(j);

	return log10f(val);
}

