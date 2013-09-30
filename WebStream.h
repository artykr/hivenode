// Stream wrapper for Webduino

#ifndef WebStream_h
#define WebStream_h
#include "WebServer.h"
#include "aJSON.h"

// DEFINITION

class WebStream : public Stream {
public:
  WebStream(WebServer *server_);
  
  size_t write(uint8_t ch);
  int read();
  int available();
  void flush();
  int peek();

private:
  WebServer *server_obj;

};

// IMPLEMENTATION

WebStream::WebStream(WebServer *server_)
    : server_obj(server_)
    {}

size_t WebStream::write(uint8_t ch) {
  return server_obj->write(ch);
}

int WebStream::read() {
  return server_obj->read();
}

int WebStream::available() {
  return server_obj->available();
}

void WebStream::flush() {

}

int WebStream::peek() {
  int ch = server_obj->read();
  server_obj->push(ch);
  return ch;
}

#endif