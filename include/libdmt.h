#pragma once

#ifdef _WIN32
  #define libdmt_EXPORT __declspec(dllexport)
#else
  #define libdmt_EXPORT
#endif

libdmt_EXPORT void libdmt();
