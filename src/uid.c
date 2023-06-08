#include "uid.h"

static uid next = 1; /* skip id = 0 */

uid uid_new(void) { return next++; }
