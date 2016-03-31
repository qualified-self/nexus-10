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

/* This file define a CMemButton class, which is simply a normal QPushButton,
 * except that it remembers how often it was pressed */

#include "CMemButton.h"

#include <QPushButton>
#include <QMutexLocker>

#include <iostream>

#define __unused  __attribute__((unused))

CMemButton::CMemButton ( QWidget * a_parent )
	: QPushButton(a_parent)
	, data_lock()
	, num_pressed(0)
{
	Init();
}

CMemButton::CMemButton ( const QString & a_text, QWidget * a_parent )
	: QPushButton(a_text,a_parent)
	, num_pressed(0)
{
	Init();
}

CMemButton::CMemButton ( const QIcon & an_icon, const QString & a_text, QWidget * a_parent )
	: QPushButton(an_icon,a_text,a_parent)
	, num_pressed(0)
{
	Init();
}

void CMemButton::Init()
{
	connect( this, SIGNAL(clicked()), this, SLOT(ButtonPressed()) );
}

void CMemButton::ButtonPressed()
{
	QMutexLocker locker(&data_lock);

	if (num_pressed<MAX_PRESSES)
	{
		num_pressed++;
	}
	//std::cerr << "Button was pressed " << num_pressed << " times" << std::endl;
}

void CMemButton::Reset()
{
	QMutexLocker locker(&data_lock);
	num_pressed = 0;
}
