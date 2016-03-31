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

#ifndef _CSQUARE_H_
#define _CSQUARE_H_

#include <stdint.h>
#include <QColor>
#include <QWidget>

/** A simple widget that shows a filled square with (optional) text 
 */

class CSquare : public QWidget
{
	Q_OBJECT

public:
	CSquare( const unsigned int a_size = 64, 
		const QColor & fg = Qt::black, const QColor & bg = Qt::white );

	virtual QSize sizeHint() const;

public slots:
	void setText( const std::string & new_text );
	void setFGColour( const QColor & new_colour );
	void setFGColour( const uint8_t r, const uint8_t g, const uint8_t b );
	void setBGColour( const QColor & new_colour );
	void setBGColour( const uint8_t r, const uint8_t g, const uint8_t b );
	void switchColours();

signals:
	void update_signal();

protected:
	void paintEvent( QPaintEvent *an_event );

private:
	QSize   theSize;
	QString theText;
	QColor  theFGColour;
	QColor  theBGColour;
};


#endif
