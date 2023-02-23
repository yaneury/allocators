#pragma once

#ifdef DMT_DEBUG

#include <plog/Log.h>

#define DINFO(x) PLOGD << x;
#define DERROR(x) PLOGE << x;
#else
#define DINFO(x)
#define DERROR(x)
#endif