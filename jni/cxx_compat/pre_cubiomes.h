/* Force-included on C++ compiles (see build-jni.bat).
 * cubiomes rng.h defines lerp(); C++20 <math.h> does using std::lerp.
 * Rename during the first include; later #include "rng.h" hits the include guard.
 */
#ifdef __cplusplus
#define lerp cubiomes_lerp
#include "cubiomes/rng.h"
#undef lerp
#endif
