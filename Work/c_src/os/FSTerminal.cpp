#include "FSTerminal.hpp"
#include <stdexcept>
#include <cstring>

// Platform detection: Cygwin provides POSIX sockets
#if defined(_WIN32) && !defined(__CYGWIN__)
#define USE_WINSOCK 1
#include <winsock2.h>
#else
#define USE_WINSOCK 0
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace os {

FSTerminal::FSTerminal(int socket_fd) : socket_fd(socket_fd) {
  // Send clear-screen character
  uint8_t clear = 014;
  ::send(socket_fd, reinterpret_cast<const char*>(&clear), 1, 0);
}

FSTerminal::~FSTerminal() {
  if(socket_fd >= 0) {
#if USE_WINSOCK
    closesocket(socket_fd);
#else
    ::close(socket_fd);
#endif
    socket_fd = -1;
  }
}

int32_t FSTerminal::available() {
#if USE_WINSOCK
  u_long count = 0;
  ioctlsocket(socket_fd, FIONREAD, &count);
  return static_cast<int32_t>(count);
#else
  int count = 0;
  ::ioctl(socket_fd, FIONREAD, &count);
  return count;
#endif
}

int32_t FSTerminal::read(std::vector<uint8_t>& bytes) {
  auto n = ::recv(socket_fd, reinterpret_cast<char*>(bytes.data()), bytes.size(), 0);
  if(n < 0)
    throw std::runtime_error("FSTerminal read() failed");
  if(n == 0)
    throw std::runtime_error("FSTerminal disconnect");
  return static_cast<int32_t>(n);
}

void FSTerminal::write(const std::vector<uint8_t>& bytes) {
  auto n = ::send(socket_fd, reinterpret_cast<const char*>(bytes.data()), bytes.size(), 0);
  if(n < 0)
    throw std::runtime_error("FSTerminal write() failed");
}

int32_t FSTerminal::read(std::vector<uint8_t>& bytes, bool line_mode) {
  if(!line_mode)
    return read(bytes);

  std::vector<uint8_t> char_buf(1);
  uint8_t erase[] = {031, ' ', 031};
  uint8_t nextline[] = {'\r', '\n'};
  int32_t position = 0;
  int32_t max_len = static_cast<int32_t>(bytes.size());

  while(true) {
    int32_t len = read(char_buf);
    if(len < 0)
      return (position == 0) ? -1 : position;

    if(char_buf[0] >= ' ' && char_buf[0] < 127) {
      if(position == max_len)
        char_buf[0] = 007; // bell
      else
        bytes[position++] = char_buf[0];
      ::send(socket_fd, reinterpret_cast<const char*>(char_buf.data()), 1, 0);
    }
    else if(char_buf[0] == '\b' || char_buf[0] == 127) {
      if(position == 0) {
        char_buf[0] = 007;
        ::send(socket_fd, reinterpret_cast<const char*>(char_buf.data()), 1, 0);
      } else {
        position--;
        ::send(socket_fd, reinterpret_cast<const char*>(erase), 3, 0);
      }
    }
    else if(char_buf[0] == '\r' || char_buf[0] == '\n') {
      ::send(socket_fd, reinterpret_cast<const char*>(nextline), 2, 0);
      return position;
    }
  }
}

} // namespace os
