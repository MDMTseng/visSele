
#ifndef CIRCLE_FITTING_H
#define CIRCLE_FITTING_H

#include "circle.h"
#include "data.h"

void RandomNormalPair( reals& x, reals& y );

void SimulateArc(Data& data, reals a, reals b, reals R, reals theta1, reals theta2, reals sigma);

void SimulateRandom(Data& data, reals Window);

reals Sigma (Data& data, Circle& circle);

reals SigmaReduced (Data& data, Circle& circle);

reals SigmaReducedNearLinearCase (Data& data, Circle& circle);

reals SigmaReducedForCenteredScaled (Data& data, Circle& circle);

reals OptimalRadius (Data& data, Circle& circle);

Circle CircleFitByKasa (Data& data);
Circle CircleFitByPratt (Data& data);
Circle CircleFitByTaubin (Data& data);
Circle CircleFitByHyper (Data& data);
#endif
