#pragma once

#if defined(LIGHTWARE_EXPORT)
#if defined(_WIN32)
#define LIGHTWARE_API __declspec(dllexport)
#elif defined(__ELF__)
#define LIGHTWARE_API __attribute__((visibility("default")))
#else
#define LIGHTWARE_API
#endif
#else
#if defined(_WIN32)
#define LIGHTWARE_API __declspec(dllimport)
#else
#define LIGHTWARE_API
#endif
#endif

LIGHTWARE_API void testFunc();