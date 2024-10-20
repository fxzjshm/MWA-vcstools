/********************************************************
 *                                                      *
 * Licensed under the Academic Free License version 3.0 *
 *                                                      *
 ********************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <hip/hip_runtime.h>

extern "C" {
#include "beam_common.h"
#include "form_beam.h"
#include "mycomplex.h"
}


#define hipCheckErrors(msg) \
    do { \
        hipError_t __err = hipGetLastError(); \
        if (__err != hipSuccess) { \
            fprintf(stderr, "Fatal error: %s (%s at %s:%d)\n", \
                msg, hipGetErrorString(__err), \
                __FILE__, __LINE__); \
            fprintf(stderr, "*** FAILED - ABORTING\n"); \
            exit(1); \
        } \
    } while (0)

inline void gpuAssert(hipError_t code, const char *file, int line, bool abort=true)
{
    /* Wrapper function for GPU/CUDA error handling. Every CUDA call goes through
      this function. It will return a message giving your the error string,
      file name and line of the error. Aborts on error. */

    if (code != 0)
    {
        fprintf(stderr, "GPUAssert:: %s - %s (%d)\n", hipGetErrorString(code), file, line);
        if (abort)
        {
            exit(code);
        }
    }
}

// define a macro for accessing gpuAssert
#define gpuErrchk(ans) {gpuAssert((ans), __FILE__, __LINE__, true);}


// define constants to be used in the kernel
#define NSTATION  128
#define NPOL      2
#define NSTOKES   4
// maximum number of pointings (currently)
#define NPOINTING 4

__global__ void invj_the_data( uint8_t       *data,
                               ComplexDouble *J,
                               ComplexDouble *W,
                               ComplexDouble *JDx,
                               ComplexDouble *JDy,
                               float         *Ia,
                               int            incoh )
/* Layout for input arrays:
*   data [nsamples] [nchan] [NPFB] [NREC] [NINC] -- see docs
*   J    [NSTATION] [nchan] [NPOL] [NPOL]        -- jones matrix
*   incoh --true if outputing an incoherent beam
* Layout for output arrays:
*   JDx  [nsamples] [nchan] [NPFB] [NREC] [NINC]
*   JDy  [nsamples] [nchan] [NPFB] [NREC] [NINC]
*/
{
    // Translate GPU block/thread numbers into meaning->l names
    int c    = blockIdx.x;  /* The (c)hannel number */
    int nc   = gridDim.x;   /* The (n)umber of (c)hannels (=128) */
    int s    = blockIdx.y;  /* The (s)ample number */

    int ant  = threadIdx.x; /* The (ant)enna number */

    ComplexDouble Dx, Dy;
    // Convert input data to complex float
    Dx  = UCMPLX4_TO_CMPLX_FLT(data[D_IDX(s,c,ant,0,nc)]);
    Dy  = UCMPLX4_TO_CMPLX_FLT(data[D_IDX(s,c,ant,1,nc)]);

    // If tile is flagged in the calibration, flag it in the incoherent beam
    if (incoh)
    {
        if (CReald(W[W_IDX(0,ant,c,0,nc)]) == 0.0 &&
            CImagd(W[W_IDX(0,ant,c,0,nc)]) == 0.0 &&
            CReald(W[W_IDX(0,ant,c,1,nc)]) == 0.0 &&
            CImagd(W[W_IDX(0,ant,c,1,nc)]) == 0.0)
            Ia[JD_IDX(s,c,ant,nc)] = 0.0;
        else
            Ia[JD_IDX(s,c,ant,nc)] = DETECT(Dx) + DETECT(Dy);
    }

    // Calculate the first step (J*D) of the coherent beam (B = J*W*D)
    // Nick: by my math the order should be:
    // JDx = Jxx*Dx + Jxy*Dy
    // JDy = Jyx*Dx + Jyy*Dy
    // This is what I have implemented (as it produces higher signal-to-noise
    // ratio detections). The original code (the single-pixel beamformer)
    // switched the yx and xy terms but we get similar SNs
    JDx[JD_IDX(s,c,ant,nc)] = CAddd( CMuld( J[J_IDX(ant,c,0,0,nc)], Dx ),
                                     CMuld( J[J_IDX(ant,c,0,1,nc)], Dy ) );
    JDy[JD_IDX(s,c,ant,nc)] = CAddd( CMuld( J[J_IDX(ant,c,1,0,nc)], Dx ),
                                     CMuld( J[J_IDX(ant,c,1,1,nc)], Dy ) );


}

__global__ void beamform_kernel( ComplexDouble *JDx,
                                 ComplexDouble *JDy,
                                 ComplexDouble *W,
                                 float *Iin,
                                 double invw,
                                 int p,
                                 int coh_pol,
                                 int incoh,
                                 int soffset,
                                 int nchunk,
                                 ComplexDouble *Bd,
                                 float *C,
                                 float *I )
/* Layout for input arrays:
*   JDx  [nsamples] [nchan] [NPFB] [NREC] [NINC] -- calibrated voltages
*   JDy  [nsamples] [nchan] [NPFB] [NREC] [NINC]
*   W    [NSTATION] [nchan] [NPOL]               -- weights array
*   Iin  [nsamples] [nchan] [nant]               -- detected incoh
*   invw                                         -- inverse atrix
* Layout of input options
*   p                                            -- pointing number
*   coh_pol                                      -- coherent polorisation number
*   incoh                                        -- true if outputing an incoherent beam
*   soffset                                      -- sample offset (10000/nchunk)
*   nchunk                                       -- number of chunks each second is split into
* Layout for output arrays:
*   Bd   [nsamples] [nchan]   [NPOL]             -- detected beam
*   C    [nsamples] [NSTOKES] [nchan]            -- coherent full stokes
*   I    [nsamples] [nchan]                      -- incoherent
*/
{
    // Translate GPU block/thread numbers into meaning->l names
    int c    = blockIdx.x;  /* The (c)hannel number */
    int nc   = gridDim.x;   /* The (n)umber of (c)hannels (=128) */
    int s    = blockIdx.y;  /* The (s)ample number */
    int ns   = gridDim.y*nchunk;   /* The (n)umber of (s)amples (=10000)*/

    int ant  = threadIdx.x; /* The (ant)enna number */
    int nant = blockDim.x;  /* The (n)_umber of (ant)ennas */

    /*// GPU profiling
    clock_t start, stop;
    double setup_t, detect_t, sum_t, stokes_t;
    if ((p == 0) && (ant == 0) && (c == 0) && (s == 0)) start = clock();*/

    // Calculate the beam and the noise floor
    __shared__ double Ia[NSTATION];
    __shared__ ComplexDouble Bx[NSTATION], By[NSTATION];

    __shared__ ComplexDouble Nxx[NSTATION], Nxy[NSTATION],
                            Nyy[NSTATION];//Nyx[NSTATION]


    /* Fix from Maceij regarding NaNs in output when running on Athena, 13 April 2018.
    Apparently the different compilers and architectures are treating what were
    unintialised variables very differently */

    Bx[ant]  = CMaked( 0.0, 0.0 );
    By[ant]  = CMaked( 0.0, 0.0 );

    Nxx[ant] = CMaked( 0.0, 0.0 );
    Nxy[ant] = CMaked( 0.0, 0.0 );
    //Nyx[ant] = CMaked( 0.0, 0.0 );
    Nyy[ant] = CMaked( 0.0, 0.0 );

    if ((p == 0) && (incoh)) Ia[ant] = Iin[JD_IDX(s,c,ant,nc)];

    /*if ((p == 0) && (ant == 0) && (c == 0) && (s == 0))
    {
        stop = clock();
        setup_t = (double)(stop - start) / CLOCKS_PER_SEC * NPOINTING * NANT;
        start = clock();
    }*/

    // Calculate beamform products for each antenna, and then add them together
    // Calculate the coherent beam (B = J*W*D)
    Bx[ant] = CMuld( W[W_IDX(p,ant,c,0,nc)], JDx[JD_IDX(s,c,ant,nc)] );
    By[ant] = CMuld( W[W_IDX(p,ant,c,1,nc)], JDy[JD_IDX(s,c,ant,nc)] );

    Nxx[ant] = CMuld( Bx[ant], CConjd(Bx[ant]) );
    Nxy[ant] = CMuld( Bx[ant], CConjd(By[ant]) );
    //Nyx[ant] = CMuld( By[ant], CConjd(Bx[ant]) );
    Nyy[ant] = CMuld( By[ant], CConjd(By[ant]) );

    /*if ((p == 0) && (ant == 0) && (c == 0) && (s == 0))
    {
        stop = clock();
        detect_t = (double)(stop - start) / CLOCKS_PER_SEC * NPOINTING * NANT;
        start = clock();
    }*/

    // Detect the coherent beam
    // A summation over an array is faster on a GPU if you add half on array
    // to its other half as than can be done in parallel. Then this is repeated
    // with half of the previous array until the array is down to 1.
    __syncthreads();
    for ( int h_ant = nant / 2; h_ant > 0; h_ant = h_ant / 2 )
    {
        if (ant < h_ant)
        {
            if ( (p == 0) && (incoh)) Ia[ant] += Ia[ant+h_ant];
            Bx[ant]  = CAddd( Bx[ant],  Bx[ant  + h_ant] );
            By[ant]  = CAddd( By[ant],  By[ant  + h_ant] );
            Nxx[ant] = CAddd( Nxx[ant], Nxx[ant + h_ant] );
            Nxy[ant] = CAddd( Nxy[ant], Nxy[ant + h_ant] );
            //Nyx[ant]=CAddd( Nyx[ant], Nyx[ant + h_ant] );
            Nyy[ant] = CAddd( Nyy[ant], Nyy[ant + h_ant] );
        }
        // below makes no difference so removed
        // else return;
        __syncthreads();
    }

    /*if ((p == 0) && (ant == 0) && (c == 0) && (s == 0))
    {
        stop = clock();
        sum_t = (double)(stop - start) / CLOCKS_PER_SEC * NPOINTING * NANT;
        start = clock();

    }*/

    // Form the stokes parameters for the coherent beam
    // Only doing it for ant 0 so that it only prints once
    if ( ant == 0 )
    {
        float bnXX = DETECT(Bx[0]) - CReald(Nxx[0]);
        float bnYY = DETECT(By[0]) - CReald(Nyy[0]);
        ComplexDouble bnXY = CSubd( CMuld( Bx[0], CConjd( By[0] ) ),
                                    Nxy[0] );

        // The incoherent beam
        if ( (p == 0) && (incoh)) I[I_IDX(s+soffset,c,nc)] = Ia[0];

        // Stokes I, Q, U, V:
        C[C_IDX(p,s+soffset,0,c,ns,coh_pol,nc)] = invw*(bnXX + bnYY);
        if ( coh_pol == 4 )
        {
            C[C_IDX(p,s+soffset,1,c,ns,coh_pol,nc)] = invw*(bnXX - bnYY);
            C[C_IDX(p,s+soffset,2,c,ns,coh_pol,nc)] =  2.0*invw*CReald( bnXY );
            C[C_IDX(p,s+soffset,3,c,ns,coh_pol,nc)] = -2.0*invw*CImagd( bnXY );
        }

        // The beamformed products
        Bd[B_IDX(p,s+soffset,c,0,ns,nc)] = Bx[0];
        Bd[B_IDX(p,s+soffset,c,1,ns,nc)] = By[0];
    }
    /*if ((p == 0) && (ant == 0) && (c == 0) && (s == 0))
    {
        stop = clock();
        stokes_t = (double)(stop - start) / CLOCKS_PER_SEC * NPOINTING * NANT;
        printf("Time:  setup: % f detect: %f    sum: %f     stokes: %f\n", setup_t, detect_t, sum_t, stokes_t);
    }*/

}

__global__ void flatten_bandpass_I_kernel( float *I,
                                           int nstep )
{
    // For just doing stokes I
    // One block
    // 128 threads each thread will do one channel
    // (we have already summed over all ant)

    // For doing the C array (I,Q,U,V)
    // ... figure it out later.

    // Translate GPU block/thread numbers into meaningful names
    int chan = threadIdx.x; /* The (c)hannel number */
    int nchan = blockDim.x; /* The total number of channels */
    float band;

    int new_var = 32; /* magic number */
    int i;

    float *data_ptr;

    // initialise the band 'array'
    band = 0.0;

    // accumulate abs(data) over all time samples and save into band
    data_ptr = I + I_IDX(0, chan, nchan);
    for (i=0;i<nstep;i++) { // time steps
        band += fabsf(*data_ptr);
        data_ptr = I + I_IDX(i,chan,nchan);
    }

    // now normalise the incoherent beam
    data_ptr = I + I_IDX(0, chan, nchan);
    for (i=0;i<nstep;i++) { // time steps
        *data_ptr = (*data_ptr)/( (band/nstep)/new_var );
        data_ptr = I + I_IDX(i,chan,nchan);
    }

}


__global__ void flatten_bandpass_C_kernel( float *C, int nstep )
{
    // For just doing stokes I
    // One block
    // 128 threads each thread will do one channel
    // (we have already summed over all ant)

    // For doing the C array (I,Q,U,V)
    // ... figure it out later.

    // Translate GPU block/thread numbers into meaningful names
    int chan    = threadIdx.x; /* The (c)hannel number */
    int nchan   = blockDim.x;  /* The (n)umber of (c)hannels */
    int stokes  = threadIdx.y; /* The (stokes) number */
    int nstokes = blockDim.y;  /* The (n)umber of (stokes) */

    int p      = blockIdx.x;  /* The (p)ointing number */

    float band;

    int new_var = 32; /* magic number */
    int i;

    float *data_ptr;

    // initialise the band 'array'
    band = 0.0;

    // accumulate abs(data) over all time samples and save into band
    //data_ptr = C + C_IDX(0,stokes,chan,nchan);
    for (i=0;i<nstep;i++) { // time steps
        data_ptr = C + C_IDX(p,i,stokes,chan,nstep,nstokes,nchan);
        band += fabsf(*data_ptr);
    }

    // now normalise the coherent beam
    //data_ptr = C + C_IDX(0,stokes,chan,nchan);
    for (i=0;i<nstep;i++) { // time steps
        data_ptr = C + C_IDX(p,i,stokes,chan,nstep,nstokes,nchan);
        *data_ptr = (*data_ptr)/( (band/nstep)/new_var );
    }

}



void cu_form_beam( uint8_t *data, struct make_beam_opts *opts,
                   ComplexDouble ****complex_weights_array,
                   ComplexDouble ****invJi, int file_no,
                   int npointing, int nstation, int nchan,
                   int npol, int outpol_coh, double invw,
                   struct gpu_formbeam_arrays *g,
                   ComplexDouble ****detected_beam, float *coh, float *incoh,
                   hipStream_t *streams, int incoh_check, int nchunk )
/* The CPU version of the beamforming operations, using OpenMP for
* parallelisation.
*
* Inputs:
*   data    = array of 4bit+4bit complex numbers. For data order, refer to the
*             documentation.
*   opts    = passed option parameters, containing meta information about the
*             obs and the data
*   W       = complex weights array. [npointing][nstation][nchan][npol]
*   J       = inverse Jones matrix.  [nstation][nchan][npol][npol]
*   file_no = number of file we are processing, starting at 0.
*   nstation     = 128
*   nchan        = 128
*   npol         = 2 (X,Y)
*   outpol_coh   = 4 (I,Q,U,V)
*   invw         = the reciprocal of the sum of the antenna weights
*   g            = struct containing pointers to various arrays on
*                  both host and device
*
* Outputs:
*   detected_beam = result of beamforming operation, summed over antennas
*                   [2*nsamples][nchan][npol]
*   coh           = result in Stokes parameters (minus noise floor)
*                   [nsamples][nstokes][nchan]
*   incoh         = result (just Stokes I)
*                   [nsamples][nchan]
*
* Assumes "coh" and "incoh" contain only zeros.
*/
{
    // Setup input values (= populate W and J)
    int p, ant, ch, pol, pol2;
    int Wi, Ji;
    for (p   = 0; p   < npointing; p++  )
    for (ant = 0; ant < nstation ; ant++)
    for (ch  = 0; ch  < nchan    ; ch++ )
    for (pol = 0; pol < npol     ; pol++)
    {
        Wi = p   * (npol*nchan*nstation) +
             ant * (npol*nchan) +
             ch  * (npol) +
             pol;
        g->W[Wi] = complex_weights_array[p][ant][ch][pol];

        if ( p == 0 )
        for (pol2 = 0; pol2 < npol; pol2++)
        {
            Ji = ant * (npol*npol*nchan) +
                 ch  * (npol*npol) +
                 pol * (npol) +
                 pol2;
            g->J[Ji] = invJi[ant][ch][pol][pol2];
        }
    }
    // Copy the data to the device
    gpuErrchk(hipMemcpy( g->d_W,    g->W, g->W_size,    hipMemcpyHostToDevice ));
    gpuErrchk(hipMemcpy( g->d_J,    g->J, g->J_size,    hipMemcpyHostToDevice ));
    gpuErrchk( hipDeviceSynchronize() );

    // Divide the gpu calculation into multiple time chunks so there is enough room on the GPU
    for (int ichunk = 0; ichunk < nchunk; ichunk++)
    {
        //int dataoffset = ichunk * g->data_size / sizeof(uint8_t);
        gpuErrchk(hipMemcpy( g->d_data,
                                   data + ichunk * g->data_size / sizeof(uint8_t),
                                   g->data_size, hipMemcpyHostToDevice ));

        // Call the kernels
        // samples_chan(index=blockIdx.x  size=gridDim.x,
        //              index=blockIdx.y  size=gridDim.y)
        // stat_point  (index=threadIdx.x size=blockDim.x,
        //              index=threadIdx.y size=blockDim.y)
        //dim3 samples_chan(opts->sample_rate, nchan);
        dim3 chan_samples( nchan, opts->sample_rate / nchunk );
        dim3 stat( NSTATION );

        gpuErrchk( hipDeviceSynchronize() );
        // convert the data and multiply it by J
        invj_the_data<<<chan_samples, stat>>>( g->d_data, g->d_J, g->d_W, g->d_JDx, g->d_JDy,
                                               g->d_Ia, incoh_check );

        // Send off a parrellel cuda stream for each pointing
        for ( int p = 0; p < npointing; p++ )
        {
            beamform_kernel<<<chan_samples, stat, 0, streams[p]>>>( g->d_JDx, g->d_JDy,
                            g->d_W, g->d_Ia, invw,
                            p, outpol_coh , incoh_check, ichunk*opts->sample_rate/nchunk, nchunk,
                            g->d_Bd, g->d_coh, g->d_incoh );

            gpuErrchk( hipPeekAtLastError() );
        }
    }
    gpuErrchk( hipDeviceSynchronize() );


    // Flatten the bandpass
    if ( incoh_check )
    {
        flatten_bandpass_I_kernel<<<1, nchan, 0, streams[0]>>>( g->d_incoh,
                                                                opts->sample_rate );
        gpuErrchk( hipPeekAtLastError() );
    }
    // Now do the same for the coherent beam
    dim3 chan_stokes(nchan, outpol_coh);
    flatten_bandpass_C_kernel<<<npointing, chan_stokes, 0, streams[0]>>>( g->d_coh,
                                                                opts->sample_rate );
    gpuErrchk( hipPeekAtLastError() );

    gpuErrchk( hipDeviceSynchronize() );

    // Copy the results back into host memory
    gpuErrchk(hipMemcpy( g->Bd, g->d_Bd,    g->Bd_size,    hipMemcpyDeviceToHost ));
    gpuErrchk(hipMemcpy( incoh, g->d_incoh, g->incoh_size, hipMemcpyDeviceToHost ));
    gpuErrchk(hipMemcpy( coh,   g->d_coh,   g->coh_size,   hipMemcpyDeviceToHost ));

    // Copy the data back from Bd back into the detected_beam array
    // Make sure we put it back into the correct half of the array, depending
    // on whether this is an even or odd second.
    int offset, i;
    offset = file_no % 2 * opts->sample_rate;

    for ( int p   = 0; p   < npointing        ; p++  )
    for ( int s   = 0; s   < opts->sample_rate; s++  )
    for ( int ch  = 0; ch  < nchan            ; ch++ )
    for ( int pol = 0; pol < npol             ; pol++)
    {
        i = p  * (npol*nchan*opts->sample_rate) +
            s  * (npol*nchan)                   +
            ch * (npol)                         +
            pol;

        detected_beam[p][s+offset][ch][pol] = g->Bd[i];
    }
}

void malloc_formbeam( struct gpu_formbeam_arrays *g, unsigned int sample_rate,
                      int nstation, int nchan, int npol, int *nchunk, float gpu_mem_gb, int outpol_coh,
                      int outpol_incoh, int npointing, double time )
{
    size_t data_base_size;
    size_t JD_base_size;

    // Calculate array sizes for host and device
    g->coh_size    = npointing * sample_rate * outpol_coh * nchan * sizeof(float);
    g->incoh_size  = sample_rate * outpol_incoh * nchan * sizeof(float);
    data_base_size = sample_rate * nstation * nchan * npol * sizeof(uint8_t);
    //g->data_size  = sample_rate * nstation * nchan * npol / nchunk * sizeof(uint8_t);
    g->Bd_size     = npointing * sample_rate * nchan * npol * sizeof(ComplexDouble);
    g->W_size      = npointing * nstation * nchan * npol * sizeof(ComplexDouble);
    g->J_size      = nstation * nchan * npol * npol * sizeof(ComplexDouble);
    JD_base_size   = sample_rate * nstation * nchan * sizeof(ComplexDouble);
    //g->JD_size    = sample_rate * nstation * nchan / nchunk * sizeof(ComplexDouble);
    
    size_t gpu_mem;
    if ( gpu_mem_gb == -1.0f ) {
        // Find total GPU memory
        struct hipDeviceProp_t gpu_properties;
        hipGetDeviceProperties( &gpu_properties, 0 );
        gpu_mem = gpu_properties.totalGlobalMem;
        gpu_mem_gb = (float)gpu_mem / (float)(1024*1024*1024);
    }
    else {
        gpu_mem = (size_t)(gpu_mem_gb * (float)(1024*1024*1024));
    }


    // Work out how many chunks to split a second into so there is enough memory on the gpu
    *nchunk = 0;
    size_t gpu_mem_used = pow(10, 15); // 1 PB
    while ( gpu_mem_used > gpu_mem ) 
    {
        *nchunk += 1;
        // Make sure the nchunk is divisable by the samples
        while ( sample_rate%*nchunk != 0 )
        {
            *nchunk += 1;
        }
        gpu_mem_used = (g->W_size + g->J_size + g->Bd_size + data_base_size / *nchunk +
                        g->coh_size + g->incoh_size + 3*JD_base_size / *nchunk);
    }
    float gpu_mem_used_gb = (float)gpu_mem_used / (float)(1024*1024*1024);
    fprintf( stderr, "[%f]  Splitting each second into %d chunks\n", time, *nchunk);
    fprintf( stderr, "[%f]  %6.3f GB out of the total %6.3f GPU memory allocated\n",
                     time, gpu_mem_used_gb, gpu_mem_gb );

    g->data_size = data_base_size / *nchunk;
    g->JD_size   = JD_base_size / *nchunk;


    // Allocate host memory
    //g->W  = (ComplexDouble *)malloc( g->W_size );
    //g->J  = (ComplexDouble *)malloc( g->J_size );
    //g->Bd = (ComplexDouble *)malloc( g->Bd_size );
    hipHostMalloc( &g->W, g->W_size );
    hipCheckErrors("hipHostMalloc W fail");
    hipHostMalloc( &g->J, g->J_size );
    hipCheckErrors("hipHostMalloc J fail");
    hipHostMalloc( &g->Bd, g->Bd_size );
    hipCheckErrors("hipHostMalloc Bd fail");

    fprintf( stderr, "[%f]  coh_size   %9.3f MB GPU mem\n",
                      time, (float)g->coh_size  / (float)(1024*1024) );
    fprintf( stderr, "[%f]  incoh_size %9.3f MB GPU mem\n",
                      time, (float)g->incoh_size/ (float)(1024*1024) );
    fprintf( stderr, "[%f]  data_size  %9.3f MB GPU mem\n",
                      time, (float)g->data_size / (float)(1024*1024) );
    fprintf( stderr, "[%f]  Bd_size    %9.3f MB GPU mem\n",
                      time, (float)g->Bd_size   / (float)(1024*1024) );
    fprintf( stderr, "[%f]  W_size     %9.3f MB GPU mem\n",
                      time, (float)g->W_size    / (float)(1024*1024) );
    fprintf( stderr, "[%f]  J_size     %9.3f MB GPU mem\n",
                      time, (float)g->J_size    / (float)(1024*1024) );
    fprintf( stderr, "[%f]  JD_size    %9.3f MB GPU mem\n",
                      time, (float)g->JD_size*3 / (float)(1024*1024) );


    // Allocate device memory
    gpuErrchk(hipMalloc( (void **)&g->d_W,     g->W_size ));
    gpuErrchk(hipMalloc( (void **)&g->d_J,     g->J_size ));
    gpuErrchk(hipMalloc( (void **)&g->d_JDx,   g->JD_size ));
    gpuErrchk(hipMalloc( (void **)&g->d_JDy,   g->JD_size ));
    gpuErrchk(hipMalloc( (void **)&g->d_Ia,    g->JD_size ));
    gpuErrchk(hipMalloc( (void **)&g->d_Bd,    g->Bd_size ));
    gpuErrchk(hipMalloc( (void **)&g->d_data,  g->data_size ));
    gpuErrchk(hipMalloc( (void **)&g->d_coh,   g->coh_size ));
    gpuErrchk(hipMalloc( (void **)&g->d_incoh, g->incoh_size ));

}

void free_formbeam( struct gpu_formbeam_arrays *g )
{
    // Free memory on host and device
    hipHostFree( g->W );
    hipHostFree( g->J );
    hipHostFree( g->Bd );
    hipFree( g->d_W );
    hipFree( g->d_J );
    hipFree( g->d_Bd );
    hipFree( g->d_data );
    hipFree( g->d_coh );
    hipFree( g->d_incoh );
}

float *create_pinned_data_buffer_psrfits( size_t size )
{
    float *ptr;
    hipHostMalloc( &ptr, size * sizeof(float) );
    //hipError_t status = hipHostRegister((void**)&ptr, size * sizeof(float),
    //                                      hipHostRegisterPortable );
    hipCheckErrors("hipHostMalloc data_buffer_psrfits fail");
    return ptr;
}

float *create_pinned_data_buffer_vdif( size_t size )
{
    float *ptr;
    hipHostMalloc( &ptr, size * sizeof(float) );
    //hipError_t status = hipHostRegister((void**)&ptr, size * sizeof(float),
    //                                      hipHostRegisterPortable );
    hipCheckErrors("hipHostMalloc data_buffer_vdif fail");
    return ptr;
}

void populate_weights_johnes( struct gpu_formbeam_arrays *g,
                              ComplexDouble ****complex_weights_array,
                              ComplexDouble *****invJi,
                              int npointing, int nstation, int nchan, int npol )
{
    // Setup input values (= populate W and J)
    int p, ant, ch, pol, pol2;
    int Wi, Ji;
    for (p   = 0; p   < npointing; p++  )
    for (ant = 0; ant < nstation ; ant++)
    for (ch  = 0; ch  < nchan    ; ch++ )
    for (pol = 0; pol < npol     ; pol++)
    {
        Wi = p   * (npol*nchan*nstation) +
             ant * (npol*nchan) +
             ch  * (npol) +
             pol;
        g->W[Wi] = complex_weights_array[p][ant][ch][pol];

        for (pol2 = 0; pol2 < npol; pol2++)
        {
            Ji = Wi*npol + pol2;
            g->J[Ji] = invJi[p][ant][ch][pol][pol2];
        }
    }
    // Copy the data to the device
    gpuErrchk(hipMemcpy( g->d_W, g->W, g->W_size, hipMemcpyHostToDevice ));
    gpuErrchk(hipMemcpy( g->d_J, g->J, g->J_size, hipMemcpyHostToDevice ));
}

