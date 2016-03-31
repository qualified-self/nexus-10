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

#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include <sstream>
#include <errno.h>
#include <iostream>
#include <string.h>
#include "Client.h"
#include "Exception.h"

using namespace std;

Client::Client(const string & hostName, int port)
: hasMessage_(false) {

  addConnection(hostName, port);
}

void Client::addConnection(const std::string & hostName, int port) {
  ConnectionData connection = { hostName, port, -1, false };
  connections_.push_back(connection);
  connectAll();
}

bool Client::hasMessage() {
  if (!hasMessage_) {
    connectAll();

    bool retrying = false;
    vector<ConnectionData>::iterator i = connections_.begin();
    while (i != connections_.end()) {
      if (i->connected) {
        fd_set socketList;
        struct timeval zeroWait = {0, 0};

        FD_ZERO(&socketList);
        FD_SET(i->socket, &socketList);

        int result = select(i->socket + 1, &socketList, NULL, NULL, &zeroWait);
        if (result < 0) {
          throw Exception("select() failed");
        }
        if (result > 0) {
          readMessage(*i);
          if (hasMessage_) {
            retrying = false;
          } else {
            retrying = !retrying;
            if (retrying) {
              connectAll();
            }
          }
        }
        if (!retrying) {
          ++i;
        }
        retrying = false;
      } else {
        ++i;
      }
    }
  }
  return hasMessage_;
}

string Client::getMessage() {
  while (!hasMessage()) {
    waitForAnyReadableSocket();
  }
  assert(hasMessage_);
  hasMessage_ = false;
  return message_;
}

Client::~Client() {
  for (vector<ConnectionData>::iterator i = connections_.begin();
       i != connections_.end();
       ++i)
  {
    close(i->socket);
  }
}

void Client::connectAll() {
  for (vector<ConnectionData>::iterator i = connections_.begin();
       i != connections_.end();
       ++i)
  {
    if (!i->connected) {
      unsigned long address = lookupAddress(i->hostName);
      struct sockaddr_in sinRemote;
      sinRemote.sin_family = AF_INET;
      sinRemote.sin_port = htons((u_short) i->port);
      sinRemote.sin_addr.s_addr = address;
      i->socket = socket(AF_INET, SOCK_STREAM, 0);
      if (i->socket == -1) {
        throw Exception("Socket creation failed");
      }
      int result = -1;
      while (result<0) {
        result = ::connect(i->socket, (struct sockaddr *) &sinRemote, sizeof(sinRemote));
        if (result<0) {
          cout << "# Client: connection to " << i->hostName << ":" << i->port << " failed!" << endl;
          usleep(5*1000*1000);
        }
      }
      if (result < 0) {
        close(i->socket);
       } else {
        cout << "# Client: connected to " << i->hostName << ":" << i->port << endl;
        i->connected = true;
      }
    }
  }
}

void Client::sendMessage(char * message)
{
  for (vector<ConnectionData>::iterator i = connections_.begin();
       i != connections_.end();
       ++i)
  {
    if (i->connected) {
	write(i->socket, message, strlen(message));
    }
  }
}

unsigned long Client::lookupAddress(const string & hostName)
{
  u_long nRemoteAddr = inet_addr(hostName.c_str());
  if (nRemoteAddr == INADDR_NONE)
  {
    /* hostname isn't a dotted IP, so resolve it through DNS */
    struct hostent * hostentPtr = gethostbyname(hostName.c_str());
    if (NULL == hostentPtr)
    {
      nRemoteAddr = INADDR_NONE;
    }
    else
    {
      nRemoteAddr = *((u_long*)hostentPtr->h_addr_list[0]);
    }
  }
  return nRemoteAddr;
}

void Client::fail(ConnectionData & connection, const string & errorMsg) {
  cout << "# Error Client: " << errorMsg << " for " << connection.hostName << ":" << connection.port << endl;
  close(connection.socket);
  connection.connected = false;
  connectAll();
}

void Client::waitForAnyReadableSocket()
{
  fd_set socketList;
  FD_ZERO(&socketList);
  struct timeval oneSecond = {1, 0};
  int maxFd = 0;
  bool notAllConnected = false;
  for (vector<ConnectionData>::iterator i = connections_.begin();
       i != connections_.end();
       ++i)
  {
    if (i->connected) {
      FD_SET(i->socket, &socketList);
      maxFd = max(maxFd, i->socket);
    } else {
      notAllConnected = true;
    }
  }

  int result = select(maxFd + 1, &socketList, NULL, NULL, notAllConnected ? &oneSecond : NULL);
  if (result < 0) {
    throw Exception("select() failed");
  }
}

void Client::readMessage(ConnectionData & connection) {
  char header[7];
  int result = read(connection.socket, header, 6);
  header[6] = 0;
  if (result == 6) {
    if (header[0] != '0' || header[1] != 'x') {
      fail(connection, "Invalid header received");
    }
    istringstream headerStream(header + 2);
    int length;
    headerStream >> hex >> length;
    char *buffer = new char[length];
    int bytesRead = 0;
    while (bytesRead < length) {
      result = read(connection.socket, buffer + bytesRead, length - bytesRead);
      if (result == 0) {
        fail(connection, "Connection closed unexpectedly");
      } else if (result < 0) {
        fail(connection, "Socket read error");
      }
      bytesRead += result;
    }
    message_ = string(buffer, length);
    hasMessage_ = true;
    delete[] buffer;
  } else {
    if (result < 0) {
      fail(connection, "Socket read error");
    } else if (result == 0) {
      fail(connection, "Server closed connection");
    } else {
      fail(connection, "Received incomplete header");
    }
  }
}
