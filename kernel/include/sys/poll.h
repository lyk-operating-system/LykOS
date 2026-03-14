#pragma once

#define POLLIN      0x0001  // ready for reading non-high-priority data
#define POLLRDNORM  0x0040  // ready for reading normal data
#define POLLRDBAND  0x0080  // ready for reading priority data
#define POLLPRI     0x0002  // ready for reading high-priority data

#define POLLOUT     0x0004  // ready for writing normal data
#define POLLWRNORM  POLLOUT // same as POLLOUT
#define POLLWRBAND  0x0200  // ready for writing priority data

#define POLLERR     0x0008  // error
#define POLLHUP     0x0010  // fd was hung up
#define POLLNVAL    0x0020  // invalid fd
