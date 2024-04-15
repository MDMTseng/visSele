#pragma once

#include <MSteppers_setup.h>
struct xVec
{
  int32_t vec[MSTP_VEC_SIZE];
};


struct xVec_f
{
  float vec[MSTP_VEC_SIZE];
};


template <int VEC_DIM>
struct xnVec_f
{
  float vec[VEC_DIM];
};
template <int VEC_DIM>
struct xnVec
{
  int32_t vec[VEC_DIM];
};