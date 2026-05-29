#ifndef JUDGE_CALC_H
#define JUDGE_CALC_H

#include "FeatureManager_sig360_circle_line.h"

// Postfix CALC expression evaluator for judge features.
// Extracted verbatim from FeatureManager_sig360_circle_line.cpp (behavior-preserving).

// Returns the referenced judge id encoded as "[<id>]", or -1 if `post_exp`
// is not an id reference.
int parse_CALC_Id(const char *post_exp);

// Evaluates a judge's CALC postfix expression against already-computed judge
// reports. Returns 0 on success (result in *ret_result), negative on error
// (result set to NAN).
int judge_CALC(FeatureReport_sig360_circle_line_single &reports,
               FeatureReport_judgeDef &judge, float *ret_result);

#endif // JUDGE_CALC_H
