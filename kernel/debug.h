#ifndef _DEBUG_H
#define _DEBUG_H

// Debugging helpers. These are macros rather than functions so that a disabled
// DEBUG() costs nothing at all, and so that CHECK() can print the failing
// expression as source text.
//
// Note that this header deliberately includes nothing. CHECK() expands to a
// call to the `panic()` helper you wrote in Project 1, so `panic` must be
// declared wherever you *use* CHECK -- not here.

// Compile with `make DEBUG_LVL=1` (or 2) to enable debug output.
// The Makefile turns that into -DDEBUG_LEVEL=<n>.
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 0  // if not provided via -DDEBUG_LEVEL at compile time
#endif

// Runs C if any positive debug level is enabled.
// The do...while(0) wrapper is what lets a call to the macro end with a `;`
// and still behave like a single statement inside an `if` without braces.
#define DEBUG(C)           \
  do {                     \
    if (DEBUG_LEVEL > 0) { \
      C;                   \
    }                      \
  } while (0)

// Runs C only if DEBUG_LEVEL is at least LVL.
#define DEBUG_L(LVL, C)         \
  do {                          \
    if ((LVL) <= DEBUG_LEVEL) { \
      C;                        \
    }                           \
  } while (0)

// Assert that EXPR holds, and panic naming the expression if it does not.
// Cheap insurance in code where a wrong value produces symptoms far away from
// the cause -- which is most of this project.
#define CHECK(EXPR)                     \
  do {                                  \
    if (!(EXPR)) {                      \
      panic("CHECK failed: %s", #EXPR); \
    }                                   \
  } while (0)

#endif  // _DEBUG_H
