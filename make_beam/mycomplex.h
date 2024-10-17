/********************************************************
 *                                                      *
 * Licensed under the Academic Free License version 3.0 *
 *                                                      *
 ********************************************************/

#ifndef MYCOMPLEX_H
#define MYCOMPLEX_H

/* Define how to handle complex numbers */
#ifdef HAVE_CUDA

#include <cuComplex.h>

#define  ComplexDouble  cuDoubleComplex
#define  ComplexFloat   cuFloatComplex

#define  CMakef(X,Y)    (make_cuFloatComplex(X,Y))
#define  CMaked(X,Y)    (make_cuDoubleComplex(X,Y))

#define  CAddf(X,Y)     (cuCaddf(X,Y))
#define  CSubf(X,Y)     (cuCsubf(X,Y))
#define  CMulf(X,Y)     (cuCmulf(X,Y))
#define  CDivf(X,Y)     (cuCdivf(X,Y))

#define  CRealf(X)      (cuCrealf(X))
#define  CImagf(X)      (cuCimagf(X))

#define  CAddd(X,Y)     (cuCadd(X,Y))
#define  CSubd(X,Y)     (cuCsub(X,Y))
#define  CMuld(X,Y)     (cuCmul(X,Y))
#define  CDivd(X,Y)     (cuCdiv(X,Y))

#define  CReald(X)      (cuCreal(X))
#define  CImagd(X)      (cuCimag(X))

#define  CConjf(X)      (cuConjf(X))
#define  CConjd(X)      (cuConj(X))

#define  CAbsf(X)       (cuCabsf(X))
#define  CAbsd(X)       (cuCabs(X))

#define  CExpf(X)       (CMakef(expf(CRealf(X))*cos(CImagf(X)), \
                                expf(CRealf(X))*sin(CImagf(X))))
#define  CExpd(X)       (CMaked(expf(CReald(X))*cos(CImagd(X)), \
                                expf(CReald(X))*sin(CImagd(X))))

#define  CSclf(X,F)     (CMakef(F*CRealf(X),F*CImagf(X)))
#define  CScld(X,F)     (CMaked(F*CReald(X),F*CImagd(X)))

#define  CRcpf(X)       (CSclf(CConjf(X),1.0/(CRealf(X)*CRealf(X) + CImagf(X)*CImagf(X))))
#define  CRcpd(X)       (CScld(CConjd(X),1.0/(CReald(X)*CReald(X) + CImagd(X)*CImagd(X))))

#define  CNegf(X)       (CSclf((X),-1.0))
#define  CNegd(X)       (CScld((X),-1.0))

#define  CD2F(X)        (CMakef((float)CReald(X),(float)CImagd(X)))
#define  CF2D(X)        (CMaked((double)CRealf(X),(double)CImagf(X)))

#elif defined(HAVE_HIP) && false

#include <hip/hip_complex.h>

#define  ComplexDouble  hipDoubleComplex
#define  ComplexFloat   hipFloatComplex

#define  CMakef(X,Y)    (make_hipFloatComplex(X,Y))
#define  CMaked(X,Y)    (make_hipDoubleComplex(X,Y))

#define  CAddf(X,Y)     (hipCaddf(X,Y))
#define  CSubf(X,Y)     (hipCsubf(X,Y))
#define  CMulf(X,Y)     (hipCmulf(X,Y))
#define  CDivf(X,Y)     (hipCdivf(X,Y))

#define  CRealf(X)      (hipCrealf(X))
#define  CImagf(X)      (hipCimagf(X))

#define  CAddd(X,Y)     (hipCadd(X,Y))
#define  CSubd(X,Y)     (hipCsub(X,Y))
#define  CMuld(X,Y)     (hipCmul(X,Y))
#define  CDivd(X,Y)     (hipCdiv(X,Y))

#define  CReald(X)      (hipCreal(X))
#define  CImagd(X)      (hipCimag(X))

#define  CConjf(X)      (hipConjf(X))
#define  CConjd(X)      (hipConj(X))

#define  CAbsf(X)       (hipCabsf(X))
#define  CAbsd(X)       (hipCabs(X))

#define  CExpf(X)       (CMakef(expf(CRealf(X))*cos(CImagf(X)), \
                                expf(CRealf(X))*sin(CImagf(X))))
#define  CExpd(X)       (CMaked(expf(CReald(X))*cos(CImagd(X)), \
                                expf(CReald(X))*sin(CImagd(X))))

#define  CSclf(X,F)     (CMakef(F*CRealf(X),F*CImagf(X)))
#define  CScld(X,F)     (CMaked(F*CReald(X),F*CImagd(X)))

#define  CRcpf(X)       (CSclf(CConjf(X),1.0/(CRealf(X)*CRealf(X) + CImagf(X)*CImagf(X))))
#define  CRcpd(X)       (CScld(CConjd(X),1.0/(CReald(X)*CReald(X) + CImagd(X)*CImagd(X))))

#define  CNegf(X)       (CSclf((X),-1.0))
#define  CNegd(X)       (CScld((X),-1.0))

#define  CD2F(X)        (CMakef((float)CReald(X),(float)CImagd(X)))
#define  CF2D(X)        (CMaked((double)CRealf(X),(double)CImagf(X)))

#else


#ifndef __cplusplus
#include <complex.h>
#define ComplexDouble  _Complex double
#define ComplexFloat   _Complex float

#define  CMakef(X,Y)    ((X+(Y*I)))
#define  CMaked(X,Y)    ((X+(Y*I)))

#define  CRealf(X)      (crealf(X))
#define  CImagf(X)      (cimagf(X))

#define  CReald(X)      (creal(X))
#define  CImagd(X)      (cimag(X))

#else

#include <complex>
#define ComplexDouble std::complex<double>
#define ComplexFloat  std::complex<float>

#define  CMakef(X,Y)    ComplexFloat(X,Y)
#define  CMaked(X,Y)    ComplexDouble(X,Y)

#define  CRealf(X)      ((X).real())
#define  CImagf(X)      ((X).imag())

#define  CReald(X)      ((X).real())
#define  CImagd(X)      ((X).imag())

#endif

#define  CAddf(X,Y)     ((X)+(Y))
#define  CSubf(X,Y)     ((X)-(Y))
#define  CMulf(X,Y)     ((X)*(Y))
#define  CDivf(X,Y)     ((X)/(Y))

#define  CAddd(X,Y)     ((X)+(Y))
#define  CSubd(X,Y)     ((X)-(Y))
#define  CMuld(X,Y)     ((X)*(Y))
#define  CDivd(X,Y)     ((X)/(Y))

#define  CConjf(X)      (conjf(X))
#define  CConjd(X)      (conj(X))

#define  CAbsf(X)       (cabsf(X))
#define  CAbsd(X)       (cabs(X))

#define  CExpf(X)       (cexpf(X))
#define  CExpd(X)       (cexp(X))

#define  CSclf(X,F)     ((F)*(X))
#define  CScld(X,F)     ((F)*(X))

#define  CRcpf(X)       (1.0/(X))
#define  CRcpd(X)       (1.0/(X))

#define  CNegf(X)       (-(X))
#define  CNegd(X)       (-(X))

#define  CD2F(X)        ((ComplexFloat)(X))
#define  CF2D(X)        ((ComplexDouble)(X))

#endif



#endif
