/* The body that has to cross a unit boundary to reach its caller.
 *
 * Its second parameter is named k, the same as the caller's, on purpose:
 * that clash is what makes the two declarations of this function tell
 * apart.  The caller's declaration of scale_by and this one declare
 * parameters of the same names and types and none of the same identity,
 * so binding an argument to the wrong one is invisible until a name has
 * to be renamed for clashing -- and then the body reads the caller's
 * variable rather than what it was handed.
 */
#include "linked.h"

float scale_by(float x, float k)
{
	return x * k;
}
