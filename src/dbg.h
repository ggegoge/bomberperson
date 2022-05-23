// Tiny header for a single debugging function both for both client and sever.

#ifndef _DBG_H_
#define _DBG_H_

#include <iostream>

#ifdef NDEBUG
constexpr bool debug = false;
#else
constexpr bool debug = true;
#endif  // NDEBUG

// Print a debug line to the stderr if in debug mode.
template <typename... Args>
void dbg(Args&&... args)
{
  if constexpr (debug) {
    (std::cerr << ... << args);
    std::cerr << "\n";
  }
}

#endif  // _DBG_H_
