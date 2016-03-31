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

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <signal.h>
#include <unistd.h>
#include <sstream>
#include <iomanip>
#include <iostream>

#include "Server.h"
#include "Exception.h"


#ifdef _MSC_VER
  #include "win32_compat.h"
  #include <winsock.h>
  typedef int socklen_t;
#ifndef EWOULDBLOCK
  #define EWOULDBLOCK WSAEWOULDBLOCK
#endif
  #pragma comment(lib, "Ws2_32")
#else
  #include <sys/socket.h>
  #include <sys/select.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <fcntl.h>
  #include <errno.h>
  #define closesocket(a) close(a)
  #define INVALID_SOCKET (-1)
#endif


using namespace std;

#ifndef _MSC_VER
class SigPipeInit {
public:
  SigPipeInit() {
    struct sigaction ignoreHandler;
    ignoreHandler.sa_handler = SIG_IGN;
    sigemptyset(&ignoreHandler.sa_mask);
    sigaction(SIGPIPE, &ignoreHandler, NULL);
  }
  ~SigPipeInit() {
    struct sigaction defaultHandler;
    defaultHandler.sa_handler = SIG_DFL;
    sigemptyset(&defaultHandler.sa_mask);
    sigaction(SIGPIPE, &defaultHandler, NULL);
  }
};
static SigPipeInit __sigPipeInit;
#endif


Server::Server(int port) {
#ifdef _MSC_VER
  /* yeah, windows is rather, uhm, primitive */
  WSAData oh_my_god_what_a_piece_of_crap;
  int result = WSAStartup(0x202,&oh_my_god_what_a_piece_of_crap);
  if (result!=0) {
    throw Exception("Failed to initialize winsock");
  }
#endif

  serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (serverSocket_ == INVALID_SOCKET) {
    throw Exception("Failed to allocate socket");
  }

  const int one = 1;
  if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one)) != 0) {
    throw Exception("Failed to set SO_REUSEADDR socket option");
  }

  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddress.sin_port = htons((u_short) port);
  if (bind(serverSocket_, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
    ostringstream msg;
    msg << "# Error: Failed to bind server socket to port " << port << " (already in use?)";
    throw Exception(msg.str());
  }
#ifdef _MSC_VER
  u_long nonblockingmode = 1;
  if ( ioctlsocket(serverSocket_, FIONBIO, &nonblockingmode) != 0 )
  {
    throw Exception("ioctlsocket() for setting non-blcking mode failed");
  }
#else
  int oldFdFlags = fcntl(serverSocket_, F_GETFL);
  if (oldFdFlags == -1) {
    throw Exception("fcntl() failed");
  }
  if (fcntl(serverSocket_, F_SETFL, oldFdFlags | O_NONBLOCK) == -1) {
    throw Exception("fcntl() for settong O_NONBLOCK failed");
  }
#endif
  if (listen(serverSocket_, 30) < 0) {
    throw Exception("Socket listen() failed");
  }
  cout << "# Server: now listing on port " << port << endl;
}

void Server::sendMessage(const string & message) {
  socket_t clientSocket = INVALID_SOCKET;
  do {
    struct sockaddr_in clientAddr;
    socklen_t clientAddrSize = sizeof(clientAddr);
    clientSocket = accept(serverSocket_, (struct sockaddr *) &clientAddr, &clientAddrSize);
    if (clientSocket == INVALID_SOCKET) {
#ifdef _MSC_VER
      int the_error = WSAGetLastError();
      if (the_error != WSAEWOULDBLOCK) {
#else
      int the_error = errno;
      if (the_error != EWOULDBLOCK) {
#endif
        throw Exception("accept() failed");
      }
    } else {
      clientSockets_.push_back(clientSocket);
      cout << "# Server: new connection from ";
      for (int i = 0; i < 4; i++) {
        cout << (clientAddr.sin_addr.s_addr & 0xFF) << (i == 3 ? ":" : ".");
        clientAddr.sin_addr.s_addr >>= 8;
      }
      cout << clientAddr.sin_port << endl;
    }
  } while (clientSocket != INVALID_SOCKET);

  ostringstream protocolMessageStream;
  //protocolMessageStream << "0x" << hex << setw(4) << setfill('0') << message.size() << message;
  protocolMessageStream << message;
  string protocolMessage = protocolMessageStream.str();

  for (size_t i = 0; i != clientSockets_.size(); i++) {
#ifdef _MSC_VER
    // yeah, windows is rather braindead
    if (protocolMessage.size()>_UI32_MAX) {
      throw Exception("Protocolmessage size is too big!\n");
    }
#endif
    size_t result = send(clientSockets_[i], protocolMessage.c_str(), (unsigned int) protocolMessage.size(),0);
    if (result != protocolMessage.size()) {
      closesocket(clientSockets_[i]);
      clientSockets_.erase(clientSockets_.begin() + i);
      i--;
      cout << "# Server: connection dropped" << endl;
    }
  }
}

Server::~Server() {
  closesocket(serverSocket_);
  for (size_t i = 0; i < clientSockets_.size(); i++) {
    closesocket( clientSockets_[i] );
  }
}
