#pragma once

#include <sys/thread.h>

void reaper_enqueue_thread(thread_t *t);

void reaper_init();
