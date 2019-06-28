/********************************************************
 *                                                      *
 * Licensed under the Academic Free License version 3.0 *
 *                                                      *
 ********************************************************/

// TODO: Remove superfluous #includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "slalib.h"
#include "slamac.h"
#include "ascii_header.h"
#include "mwa_header.h"
#include <glob.h>
#include <fcntl.h>
#include <assert.h>
#include "beam_common.h"
#include "beam_psrfits.h"
#include "beam_vdif.h"
#include "make_beam.h"
#include "vdifio.h"
#include "filter.h"
#include "psrfits.h"
#include "mycomplex.h"
#include "form_beam.h"
#include <omp.h>

#ifdef HAVE_CUDA

#include <cuda_runtime.h>
#include "ipfb.h"
#define NOW  ((double)clock()/(double)CLOCKS_PER_SEC)

#else

#define NOW  (omp_get_wtime())

#endif

int main(int argc, char **argv)
{
    #ifndef HAVE_CUDA
    // Initialise FFTW with OpenMP
    fftw_init_threads();
    fftw_plan_with_nthreads( omp_get_max_threads() );
    #endif

    // A place to hold the beamformer settings
    struct make_beam_opts opts;

    /* Set default beamformer settings */

    // Variables for required options
    opts.obsid       = NULL; // The observation ID
    opts.begin       = 0;    // GPS time -- when to start beamforming
    opts.end         = 0;    // GPS time -- when to stop beamforming
    opts.time_utc    = NULL; // utc time string "yyyy-mm-ddThh:mm:ss"
    opts.pointings   = NULL; // list of pointings "dd:mm:ss_hh:mm:ss,dd:mm:ss_hh:mm:ss"
    opts.datadir     = NULL; // The path to where the recombined data live
    opts.metafits    = NULL; // filename of the metafits file
    opts.rec_channel = NULL; // 0 - 255 receiver 1.28MHz channel
    opts.frequency   = 0;    // = rec_channel expressed in Hz

    // Variables for MWA/VCS configuration
    opts.nstation      = 128;    // The number of antennas
    opts.nchan         = 128;    // The number of fine channels (per coarse channel)
    opts.chan_width    = 10000;  // The bandwidth of an individual fine chanel (Hz)
    opts.sample_rate   = 10000;  // The VCS sample rate (Hz)
    opts.custom_flags  = NULL;   // Use custom list for flagging antennas

    // Output options
    opts.out_incoh     = 0;  // Default = PSRFITS (incoherent) output turned OFF
    opts.out_coh       = 0;  // Default = PSRFITS (coherent)   output turned OFF
    opts.out_vdif      = 0;  // Default = VDIF                 output turned OFF
    opts.out_uvdif     = 0;  // Default = upsampled VDIF       output turned OFF

    // Variables for calibration settings
    opts.cal.filename          = NULL;
    opts.cal.bandpass_filename = NULL;
    opts.cal.chan_width        = 40000;
    opts.cal.nchan             = 0;
    opts.cal.cal_type          = NO_CALIBRATION;
    opts.cal.offr_chan_num     = 0;

    // Parse command line arguments
    make_beam_parse_cmdline( argc, argv, &opts );

    // Create "shorthand" variables for options that are used frequently
    int nstation           = opts.nstation;
    int nchan              = opts.nchan;
    const int npol         = 2;      // (X,Y)
    const int outpol_coh   = 4;      // (I,Q,U,V)
    const int outpol_incoh = 1;      // ("I")

    float vgain = 1.0; // This is re-calculated every second for the VDIF output
    float ugain = 1.0; // This is re-calculated every second for the VDIF output

    // Start counting time from here (i.e. after parsing the command line)
    double begintime = NOW;
    #ifdef HAVE_CUDA
    fprintf( stderr, "[%f]  Starting %s with GPU acceleration\n", NOW-begintime, argv[0] );
    #else
    fprintf( stderr, "[%f]  Starting %s with %d possible OpenMP threads\n", NOW-begintime, argv[0], omp_get_max_threads());
    #endif

    // Calculate the number of files
    int nfiles = opts.end - opts.begin + 1;
    if (nfiles <= 0) {
        fprintf(stderr, "Cannot beamform on %d files (between %lu and %lu)\n", nfiles, opts.begin, opts.end);
        exit(EXIT_FAILURE);
    }

    // Parse input pointings
    int max_npointing = 120; // Could be more
    char RAs[max_npointing][64];
    char DECs[max_npointing][64];
    int npointing = sscanf( opts.pointings, 
            "%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,],%[^_]_%[^,]," , 
                            RAs[0],  DECs[0],  RAs[1],  DECs[1],  RAs[2],  DECs[2],
                            RAs[3],  DECs[3],  RAs[4],  DECs[4],  RAs[5],  DECs[5],
                            RAs[6],  DECs[6],  RAs[7],  DECs[7],  RAs[8],  DECs[8],
                            RAs[9],  DECs[9],  RAs[10], DECs[10], RAs[11], DECs[11],
                            RAs[12], DECs[12], RAs[13], DECs[13], RAs[14], DECs[14],
                            RAs[15], DECs[15], RAs[16], DECs[16], RAs[17], DECs[17],
                            RAs[18], DECs[18], RAs[19], DECs[19], RAs[20], DECs[20],
                            RAs[21], DECs[21], RAs[22], DECs[22], RAs[23], DECs[23],
                            RAs[24], DECs[24], RAs[25], DECs[25], RAs[26], DECs[26],
                            RAs[27], DECs[27], RAs[28], DECs[28], RAs[29], DECs[29],
                            RAs[30], DECs[30], RAs[31], DECs[31], RAs[32], DECs[32],
                            RAs[33], DECs[33], RAs[34], DECs[34], RAs[35], DECs[35],
                            RAs[36], DECs[36], RAs[37], DECs[37], RAs[38], DECs[38],
                            RAs[39], DECs[39], RAs[40], DECs[40], RAs[41], DECs[41],
                            RAs[42], DECs[42], RAs[43], DECs[43], RAs[44], DECs[44],
                            RAs[45], DECs[45], RAs[46], DECs[46], RAs[47], DECs[47],
                            RAs[48], DECs[48], RAs[49], DECs[49], RAs[50], DECs[50],
                            RAs[51], DECs[51], RAs[52], DECs[52], RAs[53], DECs[53],
                            RAs[54], DECs[54], RAs[55], DECs[55], RAs[56], DECs[56],
                            RAs[57], DECs[57], RAs[58], DECs[58], RAs[59], DECs[59],
                            RAs[60], DECs[60], RAs[61], DECs[61], RAs[62], DECs[62],
                            RAs[63], DECs[63], RAs[64], DECs[64], RAs[65], DECs[65],
                            RAs[66], DECs[66], RAs[67], DECs[67], RAs[68], DECs[68],
                            RAs[69], DECs[69], RAs[70], DECs[70], RAs[71], DECs[71],
                            RAs[72], DECs[72], RAs[73], DECs[73], RAs[74], DECs[74],
                            RAs[75], DECs[75], RAs[76], DECs[76], RAs[77], DECs[77],
                            RAs[78], DECs[78], RAs[79], DECs[79], RAs[80], DECs[80],
                            RAs[81], DECs[81], RAs[82], DECs[82], RAs[83], DECs[83],
                            RAs[84], DECs[84], RAs[85], DECs[85], RAs[86], DECs[86],
                            RAs[87], DECs[87], RAs[88], DECs[88], RAs[89], DECs[89],
                            RAs[90], DECs[90], RAs[91], DECs[91], RAs[92], DECs[92],
                            RAs[93], DECs[93], RAs[94], DECs[94], RAs[95], DECs[95],
                            RAs[96], DECs[96], RAs[97], DECs[97], RAs[98], DECs[98],
                            RAs[99], DECs[99], RAs[100], DECs[100], RAs[101], DECs[101],
                            RAs[102], DECs[102], RAs[103], DECs[103], RAs[104], DECs[104],
                            RAs[105], DECs[105], RAs[106], DECs[106], RAs[107], DECs[107],
                            RAs[108], DECs[108], RAs[109], DECs[109], RAs[110], DECs[110],
                            RAs[111], DECs[111], RAs[112], DECs[112], RAs[113], DECs[113],
                            RAs[114], DECs[114], RAs[115], DECs[115], RAs[116], DECs[116],
                            RAs[117], DECs[117], RAs[118], DECs[118], RAs[119], DECs[119] );

    if (npointing%2 == 1)
    {
        fprintf(stderr, "Number of RAs do not equal the number of Decs given. Exiting\n");
        fprintf(stderr, "npointings : %d\n", npointing);
        fprintf(stderr, "RAs[0] : %s\n", RAs[0]);
        fprintf(stderr, "DECs[0] : %s\n", DECs[0]);
        exit(0);
    }
    else
        npointing /= 2; // converting from number of RAs and DECs to number of pointings

    char pointing_array[npointing][2][64];
    int p;
    for ( p = 0; p < npointing; p++) 
    {
       strcpy( pointing_array[p][0], RAs[p] );
       strcpy( pointing_array[p][1], DECs[p] );
       fprintf(stderr, "[%f]  Pointing Num: %i  RA: %s  Dec: %s\n", NOW-begintime,
                             p, pointing_array[p][0], pointing_array[p][1]);
    }

    // Allocate memory
    char **filenames = create_filenames( &opts );
    ComplexDouble ****complex_weights_array = create_complex_weights( npointing, nstation, nchan, npol ); // [npointing][nstation][nchan][npol]
    ComplexDouble *****invJi = create_invJi( npointing, nstation, nchan, npol ); // [npointing][nstation][nchan][npol][npol]
    ComplexDouble ****detected_beam = create_detected_beam( npointing, 3*opts.sample_rate, nchan, npol ); // [npointing][3*opts.sample_rate][nchan][npol]

    // Read in info from metafits file
    fprintf( stderr, "[%f]  Reading in metafits file information from %s\n", NOW-begintime, opts.metafits);
    struct metafits_info mi;
    get_metafits_info( opts.metafits, &mi, opts.chan_width );

    // If using bandpass calibration solutions, calculate number of expected bandpass channels
    if (opts.cal.cal_type == RTS_BANDPASS)
        opts.cal.nchan = (nchan * opts.chan_width) / opts.cal.chan_width;

    // If a custom flag file has been provided, use that instead of the metafits flags
    int i;
    if (opts.custom_flags != NULL)
    {
        // Reset the weights to 1
        for (i = 0; i < nstation*npol; i++)
            mi.weights_array[i] = 1.0;

        // Open custom flag file for reading
        FILE *flagfile = fopen( opts.custom_flags, "r" );
        if (flagfile == NULL)
        {
            fprintf( stderr, "error: couldn't open flag file \"%s\" for "
                             "reading\n", opts.custom_flags );
            exit(EXIT_FAILURE);
        }

        // Read in flags from file
        int nitems;
        int flag, ant;
        while (!feof(flagfile))
        {
            // Read in next item
            nitems = fscanf( flagfile, "%d", &ant );
            if (nitems != 1 && !feof(flagfile))
            {
                fprintf( stderr, "error: couldn't parse flag file \"%s\"\n",
                        opts.custom_flags );
                exit(EXIT_FAILURE);
            }

            // Flag both polarisations of the antenna in question
            flag = ant*2;
            mi.weights_array[flag]   = 0.0;
            mi.weights_array[flag+1] = 0.0;
        }

        // Close file
        fclose( flagfile );
    }

    // Issue warnings if any antennas are being used which are flagged in the metafits file
    for (i = 0; i < nstation*npol; i++)
    {
        if (mi.weights_array[i] != 0.0 &&
            mi.flag_array[i]    != 0.0)
        {
            fprintf( stderr, "warning: antenna %3d, pol %d is included even "
                             "though it is flagged in the metafits file\n",
                             i / npol,
                             i % npol );
        }
    }
    
    double wgt_sum = 0;
    for (i = 0; i < nstation*npol; i++)
        wgt_sum += mi.weights_array[i];
    double invw = 1.0/wgt_sum;

    // Run get_delays to populate the delay_vals struct
    fprintf( stderr, "[%f]  Setting up output header information\n", NOW-begintime);
    struct delays delay_vals[npointing];
    get_delays(
            pointing_array,     // an array of pointings [pointing][ra/dec][characters]
            npointing,          // number of pointings
            opts.frequency,     // middle of the first frequency channel in Hz
            &opts.cal,          // struct holding info about calibration
            opts.sample_rate,   // = 10000 samples per sec
            opts.time_utc,      // utc time string
            0.0,                // seconds offset from time_utc at which to calculate delays
            delay_vals,        // Populate psrfits header info
            &mi,                // Struct containing info from metafits file
            NULL,               // complex weights array (ignore this time)
            NULL                // invJi array           (ignore this time)
    );

    // Create structures for holding header information
    struct psrfits  *pf;
    struct psrfits  *pf_incoh;
    pf = (struct psrfits *)malloc(npointing * sizeof(struct psrfits));
    pf_incoh = (struct psrfits *)malloc(1 * sizeof(struct psrfits));
    vdif_header     vhdr;
    vdif_header     uvhdr;
    struct vdifinfo *vf;
    struct vdifinfo *uvf;
    vf = (struct vdifinfo *)malloc(npointing * sizeof(struct vdifinfo));
    uvf = (struct vdifinfo *)malloc(npointing * sizeof(struct vdifinfo));


    // Create structures for the PFB filter coefficients
    int ntaps    = 12;
    int fil_size = ntaps * nchan; // = 12 * 128 = 1536
    double coeffs[] = FINE_PFB_FILTER_COEFFS; // Hardcoded 1536 numbers
    ComplexDouble fil[fil_size];
    double approx_filter_scale = 1.0/120000.0;
    for (i = 0; i < fil_size; i++)
    {
        fil[i] = CMaked( coeffs[i] * approx_filter_scale, 0.0 );
    }
    //TODO I think i only need one fil

    // Memory for fil_ramps is allocated here:
    ComplexDouble **fil_ramps  = NULL;
    ComplexDouble **fil_ramps1 = apply_mult_phase_ramps( fil, fil_size, nchan );
    ComplexDouble **fil_ramps2 = apply_mult_phase_ramps( fil, fil_size, nchan );

    // Populate the relevant header structs
    populate_psrfits_header( pf, opts.metafits, opts.obsid, opts.time_utc,
            opts.sample_rate, opts.frequency, nchan, opts.chan_width,
            outpol_coh, opts.rec_channel, delay_vals, mi, npointing );
    populate_psrfits_header( pf_incoh, opts.metafits, opts.obsid,
            opts.time_utc, opts.sample_rate, opts.frequency, nchan,
            opts.chan_width, outpol_incoh, opts.rec_channel, delay_vals, mi, 1);

    populate_vdif_header( vf, &vhdr, opts.metafits, opts.obsid,
            opts.time_utc, opts.sample_rate, opts.frequency, nchan,
            opts.chan_width, opts.rec_channel, delay_vals, npointing );
    populate_vdif_header( uvf, &uvhdr, opts.metafits, opts.obsid,
            opts.time_utc, opts.sample_rate, opts.frequency, nchan,
            opts.chan_width, opts.rec_channel, delay_vals, npointing );

    /*for ( p=0; p<npointing; p++ )
        sprintf( uvf[p].basefilename, "%s_%s_ch%03d_u",
                 uvf[p].exp_name, uvf[p].scan_name, atoi(opts.rec_channel) );*/

    // To run asynchronously we require two memory allocations for each data 
    // set so multiple parts of the memory can be worked on at once.
    // We control this by changing the pointer to alternate between
    // the two memory allocations
    
    // Create array for holding the raw data
    int bytes_per_file = opts.sample_rate * nstation * npol * nchan;
    uint8_t *data;
    #ifdef HAVE_CUDA
    uint8_t *data1;
    uint8_t *data2;
    cudaMallocHost( &data1, bytes_per_file * sizeof(uint8_t) );
    cudaMallocHost( &data2, bytes_per_file * sizeof(uint8_t) );
    #else
    uint8_t *data1 = (uint8_t *)malloc( bytes_per_file * sizeof(uint8_t) );
    uint8_t *data2 = (uint8_t *)malloc( bytes_per_file * sizeof(uint8_t) );
    #endif
    assert(data1);
    assert(data2);

    // Create output buffer arrays
    float *data_buffer_coh    = NULL;
    float *data_buffer_coh1   = NULL;
    float *data_buffer_coh2   = NULL; 
    float *data_buffer_incoh  = NULL;
    float *data_buffer_incoh1 = NULL;
    float *data_buffer_incoh2 = NULL;
    float *data_buffer_vdif   = NULL;
    float *data_buffer_vdif1  = NULL;
    float *data_buffer_vdif2  = NULL;
    float *data_buffer_uvdif  = NULL;
    float *data_buffer_uvdif1 = NULL;
    float *data_buffer_uvdif2 = NULL;

    #ifdef HAVE_CUDA
    data_buffer_coh1   = create_pinned_data_buffer_psrfits( npointing * nchan *
                                                            outpol_coh * pf[0].hdr.nsblk );
    data_buffer_coh2   = create_pinned_data_buffer_psrfits( npointing * nchan * 
                                                            outpol_coh * pf[0].hdr.nsblk );
    data_buffer_incoh1 = create_pinned_data_buffer_psrfits( nchan * outpol_incoh *
                                                            pf_incoh[0].hdr.nsblk );
    data_buffer_incoh2 = create_pinned_data_buffer_psrfits( nchan * outpol_incoh *
                                                            pf_incoh[0].hdr.nsblk );
    data_buffer_vdif1  = create_pinned_data_buffer_vdif( vf->sizeof_buffer *
                                                         npointing );
    data_buffer_vdif2  = create_pinned_data_buffer_vdif( vf->sizeof_buffer *
                                                         npointing );
    data_buffer_uvdif1 = create_pinned_data_buffer_vdif( uvf->sizeof_buffer *
                                                         npointing );
    data_buffer_uvdif2 = create_pinned_data_buffer_vdif( uvf->sizeof_buffer *
                                                         npointing );
    
    #else
    data_buffer_coh1   = create_data_buffer_psrfits( npointing * nchan *
                                                     outpol_coh * pf[0].hdr.nsblk );
    data_buffer_coh2   = create_data_buffer_psrfits( npointing * nchan * 
                                                     outpol_coh * pf[0].hdr.nsblk );
    data_buffer_incoh1 = create_data_buffer_psrfits( nchan * outpol_incoh * pf_incoh[0].hdr.nsblk );
    data_buffer_incoh2 = create_data_buffer_psrfits( nchan * outpol_incoh * pf_incoh[0].hdr.nsblk );
    data_buffer_vdif1  = create_data_buffer_vdif( vf->sizeof_buffer * npointing );
    data_buffer_vdif2  = create_data_buffer_vdif( vf->sizeof_buffer * npointing );
    data_buffer_uvdif1 = create_data_buffer_vdif( uvf->sizeof_buffer * npointing );
    data_buffer_uvdif2 = create_data_buffer_vdif( uvf->sizeof_buffer * npointing );
    #endif

    /* Allocate host and device memory for the use of the cu_form_beam function */
    // Declaring pointers to the structs so the memory can be alternated
    struct gpu_formbeam_arrays gf;
    struct gpu_ipfb_arrays gi;
    #ifdef HAVE_CUDA
    malloc_formbeam( &gf, opts.sample_rate, nstation, nchan, npol,
            outpol_coh, outpol_incoh, npointing, NOW-begintime );

    if (opts.out_uvdif)
    {
        malloc_ipfb( &gi, ntaps, opts.sample_rate, nchan, npol, fil_size, npointing );
        // Below may need a npointing update but I don't think it's used
        cu_load_filter( fil_ramps1, &gi, nchan );
    }

    // Set up parrel streams
    cudaStream_t streams[npointing];

    for ( p = 0; p < npointing; p++ )
        cudaStreamCreate(&(streams[p])) ;
    
    // TODO work out why the below won't work
    //populate_weights_johnes( &gf, complex_weights_array, invJi,
    //                         npointing, nstation, nchan, npol );
        
    #endif


    fprintf( stderr, "[%f]  **BEGINNING BEAMFORMING**\n", NOW-begintime);
    
    // Set up sections checks that allow the asynchronous sections know when 
    // other sections have completed
    int file_no;
    int *read_check;
    int *calc_check;
    int **write_check;
    read_check = (int*)malloc(nfiles*sizeof(int));
    calc_check = (int*)malloc(nfiles*sizeof(int));
    write_check = (int**)malloc(nfiles*sizeof(int *));
    for ( file_no = 0; file_no < nfiles; file_no++ )
    {
        read_check[file_no]  = 0;//False
        calc_check[file_no]  = 0;//False
        write_check[file_no] = (int*)malloc(npointing*sizeof(int));
        for (p=0;p<npointing;p++) write_check[file_no][p] = 0;//False
    } 
    
    // Set up timing for each section
    long read_total_time, calc_total_time, write_total_time;

    int nthread;
    #pragma omp parallel 
    {
        #pragma omp master
        {
            nthread = omp_get_num_threads();
            fprintf( stderr, "[%f]  Number of CPU threads: %d\n", NOW-begintime, nthread);
        }
    }
    int thread_no;
    int exit_check = 0;
    // Sets up a parallel for loop for each of the available thread and 
    // assigns a section to each thread
    #pragma omp parallel for shared(read_check, calc_check, write_check, pf) private( thread_no, file_no, p, exit_check, data, data_buffer_coh, data_buffer_incoh, data_buffer_vdif, data_buffer_uvdif, fil_ramps)
    for (thread_no = 0; thread_no < nthread; ++thread_no)
    {
        // Read section
        if (thread_no == 0)
        {
            for (file_no = 0; file_no < nfiles; file_no++)
            {
                //Work out which memory allocation it's requires
                if (file_no%2 == 0) data = data1;
                else data = data2;
                
                //Waits until it can read 
                exit_check = 0; 
                while (1) 
                { 
                    #pragma omp critical (read_queue) 
                    { 
                        if (file_no == 0) 
                            exit_check = 1;//First read 
                        else if ( (read_check[file_no - 1] == 1) && (file_no == 1))  
                            exit_check = 1;//Second read
                        else if ( (read_check[file_no - 1] == 1) && (calc_check[file_no - 2] == 1) )
                            exit_check = 1;//Rest of the reads
                        else
                            exit_check = 0;
                    } 
                    if (exit_check) break; 
                }
                clock_t start = clock();
                #pragma omp critical (read_queue)
                {
                    // Read in data from next file
                    fprintf( stderr, "[%f] [%d/%d] Reading in data from %s \n", NOW-begintime,
                            file_no+1, nfiles, filenames[file_no]);
                    read_data( filenames[file_no], data, bytes_per_file  );
                    
                    // Records that this read section is complete
                    read_check[file_no] = 1;
                }
                read_total_time += clock() - start;
            }
        }

        // Calc section
        if (thread_no == 1)
        {
            int write_array_check = 1;
            //fprintf( stderr, "Calc  section start on thread: %d\n", thread_no);
            for (file_no = 0; file_no < nfiles; file_no++)
            {
                //Work out which memory allocation it's requires
                if (file_no%2 == 0)
                {
                   data = data1;
                   data_buffer_coh   = data_buffer_coh1;
                   data_buffer_incoh = data_buffer_incoh1;
                   data_buffer_vdif  = data_buffer_vdif1;
                   data_buffer_uvdif = data_buffer_uvdif2;
                   fil_ramps = fil_ramps1;
                }
                else
                {
                   data = data2;
                   data_buffer_coh   = data_buffer_coh2;
                   data_buffer_incoh = data_buffer_incoh2;
                   data_buffer_vdif  = data_buffer_vdif2;
                   data_buffer_uvdif = data_buffer_uvdif2;
                   fil_ramps = fil_ramps2;
                }

                // Waits until it can start the calc
                exit_check = 0;
                while (1)
                {
                    #pragma omp critical (calc_queue)
                    {
                        // First two checks
                        // fprintf( stderr, "file_no: %d  read_check: %d\n", file_no, read_check[file_no]);
                        if ( (file_no < 2) && (read_check[file_no] == 1) ) exit_check = 1;
                        // Rest of the checks. Checking if output memory is ready to be changed
                        else if (read_check[file_no] == 1) 
                        {    
                            write_array_check = 1;
                            // Loop through each pointing's write_check
                            for (int pc=0; pc<npointing; pc++)
                            {   
                                if (write_check[file_no - 2][pc] == 0) 
                                {
                                    // Not complete so changing check to False
                                    write_array_check = 0;
                                }
                            }
                            if (write_array_check == 1) exit_check = 1;
                        }
                    }
                    if (exit_check == 1) break; 
                }
                clock_t start = clock();
                // Get the next second's worth of phases / jones matrices, if needed
                fprintf( stderr, "[%f] [%d/%d] Calculating delays\n", NOW-begintime,
                                        file_no+1, nfiles );
                get_delays(
                        pointing_array,     // an array of pointings [pointing][ra/dec][characters]
                        npointing,          // number of pointings
                        opts.frequency,         // middle of the first frequency channel in Hz
                        &opts.cal,              // struct holding info about calibration
                        opts.sample_rate,       // = 10000 samples per sec
                        opts.time_utc,          // utc time string
                        (double)file_no,        // seconds offset from time_utc at which to calculate delays
                        NULL,                   // Don't update delay_vals
                        &mi,                    // Struct containing info from metafits file
                        complex_weights_array,  // complex weights array (answer will be output here)
                        invJi );                // invJi array           (answer will be output here)


                /*for (i = 0; i < npointing * nchan * outpol_coh * opts.sample_rate; i++)
                    data_buffer_coh[i] = 0.0;

                for (i = 0; i < npointing * nchan * outpol_incoh * opts.sample_rate; i++)
                    data_buffer_incoh[i] = 0.0;*/
                fprintf( stderr, "[%f] [%d/%d] Calculating beam\n", NOW-begintime,
                                        file_no+1, nfiles);
                
                #ifdef HAVE_CUDA
                cu_form_beam( data, &opts, complex_weights_array, invJi, file_no,
                              npointing, nstation, nchan, npol, outpol_coh, invw, &gf,
                              detected_beam, data_buffer_coh, data_buffer_incoh,
                              streams );
                #else
                form_beam( data, &opts, complex_weights_array, invJi, file_no,
                           nstation, nchan, npol, outpol_coh, outpol_incoh, invw,
                           detected_beam, data_buffer_coh, data_buffer_incoh );
                #endif

                // Invert the PFB, if requested
                if (opts.out_vdif)
                {
                    fprintf( stderr, "[%f] [%d/%d]  Inverting the PFB (IFFT)\n",
                                      NOW-begintime, file_no+1, nfiles);
                    #ifndef HAVE_CUDA
                    invert_pfb_ifft( detected_beam, file_no, opts.sample_rate, nchan,
                            npol, data_buffer_vdif );
                    #endif
                }

                if (opts.out_uvdif)
                {
                    fprintf( stderr, "[%f] [%d/%d]   Inverting the PFB (full)\n", 
                                     NOW-begintime, file_no+1, nfiles);
                    #ifdef HAVE_CUDA
                    cu_invert_pfb_ord( detected_beam, file_no, npointing, 
                            opts.sample_rate, nchan, npol, &gi, data_buffer_uvdif );
                    #else
                    invert_pfb_ord( detected_beam, file_no, opts.sample_rate, nchan,
                            npol, fil_ramps, fil_size, data_buffer_uvdif );
                    #endif
                }

                // Records that this calc section is complete
                calc_check[file_no] = 1;
                calc_total_time += clock() - start;
            }
        }    
        // Write section
        if (thread_no == 2) //(thread_no > 1 && thread_no < npointing + 2)
        {
            p = thread_no - 2;
            //fprintf( stderr, "Write section p:%d started on thread: %d\n", p, thread_no);
            for (file_no = 0; file_no < nfiles; file_no++)
            {
                //Work out which memory allocation it's requires
                if (file_no%2 == 0)
                {
                   data_buffer_coh   = data_buffer_coh1;
                   data_buffer_incoh = data_buffer_incoh1;
                   data_buffer_vdif  = data_buffer_vdif1;
                   data_buffer_uvdif = data_buffer_uvdif2;
                }
                else
                {
                   data_buffer_coh   = data_buffer_coh2;
                   data_buffer_incoh = data_buffer_incoh2;
                   data_buffer_vdif  = data_buffer_vdif2;
                   data_buffer_uvdif = data_buffer_uvdif2;
                }
                
                // Waits until it's time to write
                exit_check = 0;
                while (1)
                {
                    #pragma omp critical (write_queue)
                    if (calc_check[file_no] == 1) exit_check = 1;
                    if (exit_check == 1) break;
                }
                
                clock_t start = clock();

                for ( p = 0; p < npointing; p++)
                {
                    //printf_psrfits(&pf[p]);
                    fprintf( stderr, "[%f] [%d/%d] [%d/%d] Writing data to file(s)\n",
                            NOW-begintime, file_no+1, nfiles, p+1, npointing );

                    if (opts.out_coh)
                        psrfits_write_second( &pf[p], data_buffer_coh, nchan, outpol_coh, p );
                    if (opts.out_incoh && p == 0)
                        psrfits_write_second( &pf_incoh[p], data_buffer_incoh, nchan, outpol_incoh, p );
                    if (opts.out_vdif)
                        vdif_write_second( &vf[p], &vhdr, data_buffer_vdif, &vgain, p );
                    if (opts.out_uvdif)
                        vdif_write_second( &uvf[p], &uvhdr, data_buffer_uvdif, &ugain, p );

                    // Records that this write section is complete
                    write_check[file_no][p] = 1;
                }
                write_total_time += clock() - start;
            }
        }
    }

    fprintf( stderr, "[%f]  **FINISHED BEAMFORMING**\n", NOW-begintime);
    int read_ms = read_total_time * 1000 / CLOCKS_PER_SEC;
    fprintf( stderr, "[%f]  Total read  processing time: %3d.%3d s\n", 
                NOW-begintime, read_ms/1000, read_ms%1000);
    int calc_ms = calc_total_time * 1000 / CLOCKS_PER_SEC;
    fprintf( stderr, "[%f]  Total calc  processing time: %3d.%3d s\n", 
                NOW-begintime, calc_ms/1000, calc_ms%1000);
    int write_ms = write_total_time * 1000 / CLOCKS_PER_SEC;
    fprintf( stderr, "[%f]  Total write processing time: %3d.%3d s\n", 
                NOW-begintime, write_ms/1000, write_ms%1000);
    fprintf( stderr, "[%f]  Starting clean-up\n", NOW-begintime);

    // Free up memory
    destroy_filenames( filenames, &opts );
    destroy_complex_weights( complex_weights_array, npointing, nstation, nchan );
    destroy_invJi( invJi, npointing, nstation, nchan, npol );
    destroy_detected_beam( detected_beam, npointing, 3*opts.sample_rate, nchan );
    
    for ( int ch = 0; ch < nchan; ch++)
    {
        free( fil_ramps1[ch] );
        free( fil_ramps2[ch] );
    }
    free( fil_ramps1 );
    free( fil_ramps2 );
    destroy_metafits_info( &mi );
    free( data_buffer_coh    );
    free( data_buffer_incoh  );
    free( data_buffer_vdif   );
    #ifdef HAVE_CUDA
    cudaFreeHost( data_buffer_coh1   );
    cudaFreeHost( data_buffer_coh2   );
    cudaFreeHost( data_buffer_incoh1 );
    cudaFreeHost( data_buffer_incoh2 );
    cudaFreeHost( data_buffer_vdif1  );
    cudaFreeHost( data_buffer_vdif2  );
    cudaFreeHost( data_buffer_uvdif1 );
    cudaFreeHost( data_buffer_uvdif2 );
    cudaFreeHost( data1 );
    cudaFreeHost( data2 );
    #else
    free( data_buffer_coh1   );
    free( data_buffer_coh2   );
    free( data_buffer_incoh1 );
    free( data_buffer_incoh2 );
    free( data_buffer_vdif1 );
    free( data_buffer_vdif2 );
    free( data_buffer_uvdif1 );
    free( data_buffer_uvdif2 );
    free( data1 );
    free( data2 );
    #endif
    
    free( opts.obsid        );
    free( opts.time_utc     );
    free( opts.pointings    );
    free( opts.datadir      );
    free( opts.metafits     );
    free( opts.rec_channel  );
    free( opts.cal.filename );
    if (opts.out_incoh)
    {
        free( pf_incoh[0].sub.data        );
        free( pf_incoh[0].sub.dat_freqs   );
        free( pf_incoh[0].sub.dat_weights );
        free( pf_incoh[0].sub.dat_offsets );
        free( pf_incoh[0].sub.dat_scales  );
    }
    for (p = 0; p < npointing; p++)
    {
        if (opts.out_coh)
        {
            free( pf[p].sub.data        );
            free( pf[p].sub.dat_freqs   );
            free( pf[p].sub.dat_weights );
            free( pf[p].sub.dat_offsets );
            free( pf[p].sub.dat_scales  );
        }
        if (opts.out_vdif)
        {
            free( vf[p].b_scales  );
            free( vf[p].b_offsets );
        }
        if (opts.out_uvdif)
        {
            free( uvf[p].b_scales  );
            free( uvf[p].b_offsets );
        }
    }
    #ifdef HAVE_CUDA
    free_formbeam( &gf );
    if (opts.out_uvdif)
    {
        free_ipfb( &gi );
    }
    #endif

    #ifndef HAVE_CUDA
    // Clean up FFTW OpenMP
    fftw_cleanup_threads();
    #endif

    return 0;
}


void usage() {
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: make_beam [OPTIONS]\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "REQUIRED OPTIONS\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-o, --obsid=GPSTIME       ");
    fprintf(stderr, "Observation ID (GPS seconds).\n");
    fprintf(stderr, "\t-b, --begin=GPSTIME       ");
    fprintf(stderr, "Begin time of observation, in GPS seconds\n");
    fprintf(stderr, "\t-e, --end=GPSTIME         ");
    fprintf(stderr, "End time of observation, in GPS seconds\n");
    fprintf(stderr, "\t-z, --utc-time=UTCTIME    ");
    fprintf(stderr, "The UTC time that corresponds to the GPS time given by the -b\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "option. UTCTIME must have the format: yyyy-mm-ddThh:mm:ss\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-P, --pointings=hh:mm:ss.s_dd:mm:ss.s,hh:mm:ss.s_dd:mm:ss.s... ");
    fprintf(stderr, "Right ascension and declinations of multiple pointings\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-d, --data-location=PATH  ");
    fprintf(stderr, "PATH is the directory containing the recombined data\n");
    fprintf(stderr, "\t-m, --metafits-file=FILE  ");
    fprintf(stderr, "FILE is the metafits file pertaining to the OBSID given by the\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr,  "-o option\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-f, --coarse-chan=N       ");
    fprintf(stderr, "Absolute coarse channel number (0-255)\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "OUTPUT OPTIONS\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-i, --incoh                ");
    fprintf(stderr, "Turn on incoherent PSRFITS beam output.                          ");
    fprintf(stderr, "[default: OFF]\n");
    fprintf(stderr, "\t-p, --psrfits              ");
    fprintf(stderr, "Turn on coherent PSRFITS output (will be turned on if none of\n");
    fprintf(stderr, "\t                           ");
    fprintf(stderr, "-i, -p, -u, -v are chosen).                                      ");
    fprintf(stderr, "[default: OFF]\n");
    fprintf(stderr, "\t-u, --uvdif                ");
    fprintf(stderr, "Turn on VDIF output with upsampling                              ");
    fprintf(stderr, "[default: OFF]\n");
    fprintf(stderr, "\t-v, --vdif                 ");
    fprintf(stderr, "Turn on VDIF output without upsampling                           ");
    fprintf(stderr, "[default: OFF]\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "MWA/VCS CONFIGURATION OPTIONS\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-a, --antennas=N          ");
    fprintf(stderr, "The number of antennas in the array. For MWA Phase 2, N=128.     ");
    fprintf(stderr, "[default: 128]\n");
    fprintf(stderr, "\t-n, --num-fine-chans=N    ");
    fprintf(stderr, "The number of fine channels per coarse channel.                  ");
    fprintf(stderr, "[default: 128]\n");
    fprintf(stderr, "\t-w, --fine-chan-width=N   ");
    fprintf(stderr, "The bandwidth of an individual fine channel (Hz).                ");
    fprintf(stderr, "[default: 10000]\n");
    fprintf(stderr, "\t-r, --sample-rate=N       ");
    fprintf(stderr, "The VCS sample rate, in Hz. (The sample rate given in the meta-  ");
    fprintf(stderr, "[default: 10000]\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "metafits file matches the correlator settings at the time of\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "the observation, which is not necessarily the same as that of\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "the VCS. Hence the necessity of this option.)\n");
    fprintf(stderr, "\t-F, --custom-flags=file   ");
    fprintf(stderr, "Flag the antennas listed in file instead of those flagged in the ");
    fprintf(stderr, "[default: none]\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "metafits file given by the -m option.\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "CALIBRATION OPTIONS (RTS)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-J, --dijones-file=PATH   ");
    fprintf(stderr, "The direction-independent Jones matrix file that is output from\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "the RTS. Using this option instructs the beamformer to use the\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "RTS-generated calibration solution. Either -J or -O must be\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "supplied. If both are supplied the one that comes last will\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "override the former.\n");
    fprintf(stderr, "\t-B, --bandpass-file=PATH  ");
    fprintf(stderr, "The bandpass file that is output from the RTS. If this option\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "is given, the RTS calibration solution will be applied to each\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "fine channel. If -J is supplied but -B is not, then the coarse\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "channel solution will be applied to ALL fine channels\n");
    fprintf(stderr, "\t-W, --rts-chan-width      ");
    fprintf(stderr, "RTS calibration channel bandwidth (Hz)                           ");
    fprintf(stderr, "[default: 40000]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "CALIBRATION OPTIONS (OFFRINGA)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-O, --offringa-file=PATH  ");
    fprintf(stderr, "The calibration solution file that is output from the tools\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "made by Andre Offringa. Using this option instructs the beam-\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "former to use the Offringa-style calibration solution. Either\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "-J or -O must be supplied. If both are supplied the one that\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "comes last will override the former.\n");
    fprintf(stderr, "\t-C, --offringa-chan=N     ");
    fprintf(stderr, "The zero-offset position of the coarse channel solution in the   ");
    fprintf(stderr, "[default: 0]\n");
    fprintf(stderr, "\t                          ");
    fprintf(stderr, "calibration file given by the -O option.\n");

    fprintf(stderr, "\n");
    fprintf(stderr, "OTHER OPTIONS\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\t-h, --help                ");
    fprintf(stderr, "Print this help and exit\n");
    fprintf(stderr, "\t-V, --version             ");
    fprintf(stderr, "Print version number and exit\n");
    fprintf(stderr, "\n");
}



void make_beam_parse_cmdline(
        int argc, char **argv, struct make_beam_opts *opts )
{
    if (argc > 1) {

        int c;
        while (1) {

            static struct option long_options[] = {
                {"obsid",           required_argument, 0, 'o'},
                {"begin",           required_argument, 0, 'b'},
                {"end",             required_argument, 0, 'e'},
                {"incoh",           no_argument,       0, 'i'},
                {"psrfits",         no_argument,       0, 'p'},
                {"vdif",            no_argument,       0, 'v'},
                {"utc-time",        required_argument, 0, 'z'},
                {"pointings",       required_argument, 0, 'P'},
                {"data-location",   required_argument, 0, 'd'},
                {"metafits-file",   required_argument, 0, 'm'},
                {"coarse-chan",     required_argument, 0, 'f'},
                {"antennas",        required_argument, 0, 'a'},
                {"num-fine-chans",  required_argument, 0, 'n'},
                {"fine-chan-width", required_argument, 0, 'w'},
                {"sample-rate",     required_argument, 0, 'r'},
                {"custom-flags",    required_argument, 0, 'F'},
                {"dijones-file",    required_argument, 0, 'J'},
                {"bandpass-file",   required_argument, 0, 'B'},
                {"rts-chan-width",  required_argument, 0, 'W'},
                {"offringa-file",   required_argument, 0, 'O'},
                {"offringa-chan",   required_argument, 0, 'C'},
                {"help",            required_argument, 0, 'h'},
                {"version",         required_argument, 0, 'V'}
            };

            int option_index = 0;
            c = getopt_long( argc, argv,
                             "a:b:B:C:d:e:f:F:hiJ:m:n:o:O:pP:r:uvVw:W:z:",
                             long_options, &option_index);
            if (c == -1)
                break;

            switch(c) {

                case 'a':
                    opts->nstation = atoi(optarg);
                    break;
                case 'b':
                    opts->begin = atol(optarg);
                    break;
                case 'B':
                    opts->cal.bandpass_filename = strdup(optarg);
                    opts->cal.cal_type = RTS_BANDPASS;
                    break;
                case 'C':
                    opts->cal.offr_chan_num = atoi(optarg);
                    break;
                case 'd':
                    opts->datadir = strdup(optarg);
                    break;
                case 'e':
                    opts->end = atol(optarg);
                    break;
                case 'f':
                    opts->rec_channel = strdup(optarg);
                    // The base frequency of the coarse channel in Hz
                    opts->frequency = atoi(optarg) * 1.28e6 - 640e3;
                    break;
                case 'F':
                    opts->custom_flags = strdup(optarg);
                    break;
                case 'h':
                    usage();
                    exit(0);
                    break;
                case 'i':
                    opts->out_incoh = 1;
                    break;
                case 'J':
                    opts->cal.filename = strdup(optarg);
                    if (opts->cal.cal_type != RTS_BANDPASS)
                        opts->cal.cal_type = RTS;
                    break;
                case 'm':
                    opts->metafits = strdup(optarg);
                    break;
                case 'n':
                    opts->nchan = atoi(optarg);
                    break;
                case 'o':
                    opts->obsid = strdup(optarg);
                    break;
                case 'O':
                    opts->cal.filename = strdup(optarg);
                    opts->cal.cal_type = OFFRINGA;
                    break;
                case 'p':
                    opts->out_coh = 1;
                    break;
                case 'P':
                    opts->pointings = strdup(optarg);
                    break;
                case 'r':
                    opts->sample_rate = atoi(optarg);
                    break;
                case 'u':
                    opts->out_uvdif = 1;
                    break;
                case 'v':
                    opts->out_vdif = 1;
                    break;
                case 'V':
                    fprintf( stderr, "MWA Beamformer v%s\n", VERSION_BEAMFORMER);
                    exit(0);
                    break;
                case 'w':
                    opts->chan_width = atoi(optarg);
                    break;
                case 'W':
                    opts->cal.chan_width = atoi(optarg);
                    break;
                case 'z':
                    opts->time_utc = strdup(optarg);
                    break;
                default:
                    fprintf(stderr, "error: make_beam_parse_cmdline: "
                                    "unrecognised option '%s'\n", optarg);
                    usage();
                    exit(EXIT_FAILURE);
            }
        }
    }
    else {
        usage();
        exit(EXIT_FAILURE);
    }

#ifdef HAVE_CUDA
    // At the moment, -v is not implemented if CUDA is available
    if (opts->out_vdif)
    {
        fprintf( stderr, "error: -v is not available in the CUDA version. "
                         "To use -v, please recompile without CUDA.\n" );
        exit(EXIT_FAILURE);
    }
#endif

    // Check that all the required options were supplied
    assert( opts->obsid        != NULL );
    assert( opts->begin        != 0    );
    assert( opts->end          != 0    );
    assert( opts->time_utc     != NULL );
    assert( opts->pointings    != NULL );
    assert( opts->datadir      != NULL );
    assert( opts->metafits     != NULL );
    assert( opts->rec_channel  != NULL );
    assert( opts->cal.cal_type != NO_CALIBRATION );

    // If neither -i, -p, nor -v were chosen, set -p by default
    if ( !opts->out_incoh && !opts->out_coh &&
         !opts->out_vdif  && !opts->out_uvdif )
    {
        opts->out_coh = 1;
    }
}



char **create_filenames( struct make_beam_opts *opts )
{
    // Calculate the number of files
    int nfiles = opts->end - opts->begin + 1;
    if (nfiles <= 0) {
        fprintf( stderr, "Cannot beamform on %d files (between %lu and %lu)\n",
                 nfiles, opts->begin, opts->end);
        exit(EXIT_FAILURE);
    }
    // Allocate memory for the file name list
    char **filenames = NULL;
    filenames = (char **)malloc( nfiles*sizeof(char *) );

    // Allocate memory and write filenames
    int second;
    unsigned long int timestamp;
    for (second = 0; second < nfiles; second++) {
        timestamp = second + opts->begin;
        filenames[second] = (char *)malloc( MAX_COMMAND_LENGTH*sizeof(char) );
        sprintf( filenames[second], "%s/%s_%ld_ch%s.dat",
                 opts->datadir, opts->obsid, timestamp, opts->rec_channel );
    }

    return filenames;
}

void destroy_filenames( char **filenames, struct make_beam_opts *opts )
{
    int nfiles = opts->end - opts->begin + 1;
    int second;
    for (second = 0; second < nfiles; second++)
        free( filenames[second] );
    free( filenames );
}


ComplexDouble ****create_complex_weights( int npointing, int nstation, int nchan, int npol )
// Allocate memory for complex weights matrices
{
    int p, ant, ch; // Loop variables
    ComplexDouble ****array;
    
    array = (ComplexDouble ****)malloc( npointing * sizeof(ComplexDouble ***) );
    
    for (p = 0; p < npointing; p++)
    {
        array[p] = (ComplexDouble ***)malloc( nstation * sizeof(ComplexDouble **) );

        for (ant = 0; ant < nstation; ant++)
        {
            array[p][ant] = (ComplexDouble **)malloc( nchan * sizeof(ComplexDouble *) );

            for (ch = 0; ch < nchan; ch++)
                array[p][ant][ch] = (ComplexDouble *)malloc( npol * sizeof(ComplexDouble) );
        }
    }
    return array;
}


void destroy_complex_weights( ComplexDouble ****array, int npointing, int nstation, int nchan )
{
    int p, ant, ch;
    for (p = 0; p < npointing; p++)
    {
        for (ant = 0; ant < nstation; ant++)
        {
            for (ch = 0; ch < nchan; ch++)
                free( array[p][ant][ch] );

            free( array[p][ant] );
        }
        free( array[p] );
    }
    free( array );
}


ComplexDouble *****create_invJi( int npointing, int nstation, int nchan, int npol )
// Allocate memory for (inverse) Jones matrices
{
    int p, ant, pol, ch; // Loop variables
    ComplexDouble *****invJi;
    
    invJi = (ComplexDouble *****)malloc( npointing * sizeof(ComplexDouble ****) );
    for (p = 0; p < npointing; p++)
    {
        invJi[p] = (ComplexDouble ****)malloc( nstation * sizeof(ComplexDouble ***) );

        for (ant = 0; ant < nstation; ant++)
        {
            invJi[p][ant] =(ComplexDouble ***)malloc( nchan * sizeof(ComplexDouble **) );

            for (ch = 0; ch < nchan; ch++)
            {
                invJi[p][ant][ch] = (ComplexDouble **)malloc( npol * sizeof(ComplexDouble *) );

                for (pol = 0; pol < npol; pol++)
                    invJi[p][ant][ch][pol] = (ComplexDouble *)malloc( npol * sizeof(ComplexDouble) );
            }
        }
    }
    return invJi;
}


void destroy_invJi( ComplexDouble *****array, int npointing, int nstation, int nchan, int npol )
{
    int p, ant, ch, pol;
    for (p = 0; p < npointing; p++)
    {
        for (ant = 0; ant < nstation; ant++)
        {
            for (ch = 0; ch < nchan; ch++)
            {
                for (pol = 0; pol < npol; pol++)
                    free( array[p][ant][ch][pol] );

                free( array[p][ant][ch] );
            }

            free( array[p][ant] );
        }
        free( array[p] );
    }
    free( array );
}


ComplexDouble ****create_detected_beam( int npointing, int nsamples, int nchan, int npol )
// Allocate memory for complex weights matrices
{
    int p, s, ch; // Loop variables
    ComplexDouble ****array;
    
    array = (ComplexDouble ****)malloc( npointing * sizeof(ComplexDouble ***) );
    for (p = 0; p < npointing; p++) 
    {
        array[p] = (ComplexDouble ***)malloc( nsamples * sizeof(ComplexDouble **) );

        for (s = 0; s < nsamples; s++)
        {
            array[p][s] = (ComplexDouble **)malloc( nchan * sizeof(ComplexDouble *) );

            for (ch = 0; ch < nchan; ch++)
                array[p][s][ch] = (ComplexDouble *)malloc( npol * sizeof(ComplexDouble) );
        }
    }
    return array;
}

void destroy_detected_beam( ComplexDouble ****array, int npointing, int nsamples, int nchan )
{
    int p, s, ch;
    for (p = 0; p < npointing; p++)    
    {
        for (s = 0; s < nsamples; s++)
        {
            for (ch = 0; ch < nchan; ch++)
                free( array[p][s][ch] );

            free( array[p][s] );
        }

        free( array[p] );
    }

    free( array );
}

float *create_data_buffer_psrfits( size_t size )
{
    float *ptr = (float *)malloc( size * sizeof(float) );
    return ptr;
}


float *create_data_buffer_vdif( size_t size )
{
    float *ptr  = (float *)malloc( size * sizeof(float) );
    return ptr;
}

