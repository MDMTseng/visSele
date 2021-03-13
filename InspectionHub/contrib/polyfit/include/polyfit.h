#ifndef POLYFIT_H_
#define POLYFIT_H_

// #ifdef __cplusplus
// extern "C" {
// #endif

int polyfit(const float* const dependentValues,
            const float* const independentValues,
            const float* const independentValuesW,
            unsigned int        countOfElements,
            unsigned int        order,
            float*             coefficients,
            int dv_jump=sizeof(float),
            int iv_jump=sizeof(float),
            int ivw_jump=sizeof(float));

double polycalc(float x, float coeff[],float len);
// #ifdef __cplusplus
// }
// #endif

#endif
