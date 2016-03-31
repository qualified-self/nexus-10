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

#include "CBar.h"

#include <cmath>

#include <QWidget>
#include <QColor>
#include <QPainter>

#define __unused __attribute__((unused))

CBar::CBar( const unsigned int a_size, const QColor & fg, const QColor & bg )
	: QWidget()
	, theSize( QSize(a_size,4*a_size) )
	, theText("")
	, theFGColour( fg )
	, theBGColour( bg )
	, theFilledFraction( 1.0 )
{
	setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::MinimumExpanding ) );

	QFont new_font( "Helvetica", 24, QFont::Bold );
	setFont( new_font );

	// nice signal to communicate across threads
	connect( this, SIGNAL(update_signal()), this, SLOT(update()) );

	emit update_signal();
}

QSize CBar::sizeHint() const
{
	return theSize;
}


void CBar::setText( const std::string & new_text )
{
	QString new_text_2 = QString::fromUtf8( new_text.c_str() );;
	if ( theText == new_text_2 ) return;
	theText = new_text_2;

	emit update_signal();
}

void CBar::setFGColour( const uint8_t r, const uint8_t g, const uint8_t b )
{
	setFGColour( QColor::fromRgb(r,g,b) );
}

void CBar::setFGColour( const QColor & new_colour )
{
	if ( theFGColour == new_colour ) return;

	theFGColour = new_colour;
	emit update_signal();
}

void CBar::setBGColour( const uint8_t r, const uint8_t g, const uint8_t b )
{
	setBGColour( QColor::fromRgb(r,g,b) );
}

void CBar::setBGColour( const QColor & new_colour )
{
	if ( theBGColour == new_colour ) return;

	theBGColour = new_colour;
	emit update_signal();
}

void CBar::switchColours()
{
	QColor tmp = theFGColour;
	theFGColour = theBGColour;
	theBGColour = tmp;

	emit update_signal();
}

void CBar::setFilledFraction( const float fraction )
{
	theFilledFraction = fraction;
	emit update_signal();

}

void CBar::paintEvent( QPaintEvent *an_event __unused )
{
	QPainter painter(this);

	// draw background
	int the_height = (int) round( theFilledFraction * height() );
	if (the_height<0)        the_height = 0;
	if (the_height>height()) the_height = height();

	painter.setPen( theBGColour );
	painter.fillRect( 0,height()-the_height, width(),  height(), theBGColour );
	painter.drawRect( 0,0,                   width()-1,height()-1 );

	if ( theText.length() > 0 )
	{
		// calculate position of the text
		QFontMetrics metrics( font() );
		int pos_x = (width() - metrics.width(theText)) / 2;
		int pos_y = (height() + metrics.ascent() - metrics.descent()) / 2;

		// draw text
		painter.setPen( theFGColour );
		painter.drawText( pos_x, pos_y, theText );
	}
}

