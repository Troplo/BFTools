#pragma once
#if defined(_WIN32) && !defined(_UCRT)
#ifndef __LARGE_MBSTATE_T
#define __LARGE_MBSTATE_T 1
#endif
#ifdef __cplusplus
extern "C" {
inline void quick_exit(int) {}
inline int at_quick_exit(void (*)(void)) { return 0; }
}
#endif
#endif
