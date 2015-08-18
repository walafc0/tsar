
#include "hard_config.h"
#include "stdio.h"
#include "limits.h"
#include "../giet_tsar/block_device.h"

#define NL              128
#define NP              128
#define NB_IMAGES       5
 
// Only processor 0 in each cluster access TTY
#define PRINTF(...)      ({ if (lpid == 0) { _tty_printf(__VA_ARGS__); } })

#define DISPLAY_OK          1           // enable display on frame buffer when non zero 
#define CHECK_VERBOSE       !DISPLAY_OK // display a detailed check on TTY  when non zero
#define INSTRUMENTATION_OK  0           // display statistcs on TTY when non zero

// tricks to read some addresses from ldscript
extern struct plaf seg_ioc_base;
extern struct plaf seg_heap_base;

// global variables stored in seg_data (cluster 0)

// instrumentation counters (for each processor)
unsigned int LOAD_START[256][4];
unsigned int LOAD_END  [256][4];
unsigned int TRSP_START[256][4];
unsigned int TRSP_END  [256][4];
unsigned int DISP_START[256][4];
unsigned int DISP_END  [256][4];

// checksum variables (used by proc(0,0,0) only
unsigned check_line_before[NL];
unsigned check_line_after[NL];

/////////////
void main()
{
    unsigned int image = 0;

    unsigned int l;                                             // line index for loops
    unsigned int p;                                             // pixel index for loops
    unsigned int block_size  = _ioc_get_blocksize();            // get IOC block size
    unsigned int proc_id     = _procid();                       // processor id
    unsigned int nclusters   = X_SIZE*Y_SIZE;                   // number of clusters
    unsigned int lpid        = proc_id % NB_PROCS_MAX;          // local processor id
    unsigned int cluster_xy  = proc_id / NB_PROCS_MAX;          // cluster index (8 bits format)
    unsigned int x           = cluster_xy >> Y_WIDTH;           // x coordinate
    unsigned int y           = cluster_xy & ((1<<Y_WIDTH)-1);   // y coordinate
    unsigned int ntasks      = nclusters * NB_PROCS_MAX;        // number of tasks
    unsigned int npixels     = NP * NL;                         // number of pixel per image
    unsigned int nblocks     = npixels / block_size;            // number of blocks per image

    // task_id is a "continuous" index for the the task running on processor (x,y,lpid)
    unsigned int task_id = (((x * Y_SIZE) + y) * NB_PROCS_MAX) + lpid;

    // cluster_id is a "continuous" index for cluster(x,y)
    unsigned int cluster_id  = (x * Y_SIZE) + y;               

    PRINTF("\n *** Proc [%d,%d,0] enters main at cycle %d ***\n\n", 
           x, y, _proctime());

   //  parameters checking
   if ((NB_PROCS_MAX != 1) && (NB_PROCS_MAX != 2) && (NB_PROCS_MAX != 4))
   {
      PRINTF("NB_PROCS_MAX must be 1, 2 or 4\n");
      _exit();
   }
   if ((nclusters != 1) && (nclusters != 2) && (nclusters != 4) && (nclusters != 8) &&
         (nclusters != 16) && (nclusters != 32) && (nclusters != 64) && (nclusters != 128) &&
         (nclusters != 256))
   {
      PRINTF("NB_CLUSTERS must be a power of 1 between 1 and 256\n");
      _exit();
   }

   // pointers on the distributed buffers containing the images, 
   // allocated in the heap segment: each buffer contains 256 Kbytes
   unsigned char* buf_in  = (unsigned char*)&seg_heap_base;
   unsigned char* buf_out = buf_in + 0x00100000;

   PRINTF("NB_CLUSTERS     = %d\n", nclusters); 
   PRINTF("NB_LOCAL_PROCS  = %d\n", NB_PROCS_MAX); 
   PRINTF("NB_TASKS        = %d\n", ntasks);
   PRINTF("NB_PIXELS       = %d\n", npixels);
   PRINTF("BLOCK_SIZE      = %d\n", block_size);
   PRINTF("NB_BLOCKS       = %d\n\n", nblocks);

   PRINTF("*** Proc 0 in cluster [%d,%d] starts barrier init at cycle %d\n", 
          x, y, _proctime());

   //  barriers initialization
   _barrier_init(0, ntasks);
   _barrier_init(1, ntasks);
   _barrier_init(2, ntasks);
   _barrier_init(3, ntasks);

   PRINTF("*** Proc [%d,%d,0] completes barrier init at cycle %d\n",
          x, y, _proctime());

   // Main loop (on images)
   while (image < NB_IMAGES)
   {
      // pseudo parallel load from disk to buf_in buffer : nblocks/nclusters blocks
      // only task running on processor with (lpid == 0) does it

      LOAD_START[cluster_id][lpid] = _proctime();

      if (lpid == 0)
      {
         PRINTF("\n*** Proc [%d,%d,0] starts load for image %d at cycle %d\n",
                x, y, image, _proctime() );

         _ioc_read( ((image * nblocks) + ((nblocks * cluster_id) / nclusters)), 
                    buf_in,
                    (nblocks / nclusters),
                    cluster_xy );

         _ioc_completed();

         PRINTF("*** Proc [%d,%d,0] completes load for image %d at cycle %d\n",
                x, y, image, _proctime() );
      }

      LOAD_END[cluster_id][lpid] = _proctime();


      _tty_printf("*** Proc [%d,%d,%d] barrier wait (0)\n", x, y, lpid);
      _barrier_wait(0);

      // parallel transpose from buf_in to buf_out buffers
      // each processor makes the transposition for (NL/ntasks) lines
      // (p,l) are the pixel coordinates in the source image

      PRINTF("\n*** proc [%d,%d,0] starts transpose for image %d at cycle %d\n", 
             x, y, image, _proctime());

      TRSP_START[cluster_id][lpid] = _proctime();

      unsigned int nlt   = NL / ntasks;                // number of lines per processor
      unsigned int first = task_id * nlt;              // first line index
      unsigned int last  = first + nlt;                // last line index
      unsigned int nlines_clusters = NL / nclusters;   // number of lines per cluster
      unsigned int npix_clusters   = NP / nclusters;   // number of pixels per cluster

      unsigned int src_cluster;
      unsigned int src_index;
      unsigned int dst_cluster;
      unsigned int dst_index;

      unsigned int word;

      for (l = first; l < last; l++)
      {
         PRINTF("    - processing line %d\n", l);

         check_line_before[l] = 0;
         
         // in each iteration we read one word an write four bytes
         for (p = 0 ; p < NP ; p = p+4)
         {
            // read one word, with extended address from local buffer
            src_cluster = cluster_xy;
            src_index   = (l % nlines_clusters) * NP + p;
            word = _word_extended_read( src_cluster, 
                                        (unsigned int)&buf_in[src_index] );

            unsigned char byte0 = (unsigned char)( word      & 0x000000FF);
            unsigned char byte1 = (unsigned char)((word>>8)  & 0x000000FF);
            unsigned char byte2 = (unsigned char)((word>>16) & 0x000000FF);
            unsigned char byte3 = (unsigned char)((word>>24) & 0x000000FF);

            // compute checksum
            check_line_before[l] = check_line_before[l] + byte0 + byte1 + byte2 + byte3;

            // write four bytes with extended address to four remote buffers
            dst_cluster = (((p / npix_clusters) / Y_SIZE) << Y_WIDTH) + 
                           ((p / npix_clusters) % Y_SIZE);
            dst_index   = (p % npix_clusters) * NL + l;
            _byte_extended_write( dst_cluster, 
                                  (unsigned int)&buf_out[dst_index], 
                                  byte0 );

            dst_cluster = ((((p+1) / npix_clusters) / Y_SIZE) << Y_WIDTH) + 
                           (((p+1) / npix_clusters) % Y_SIZE);
            dst_index   = ((p+1) % npix_clusters) * NL + l;
            _byte_extended_write( dst_cluster, 
                                  (unsigned int)&buf_out[dst_index], 
                                  byte1 );

            dst_cluster = ((((p+2) / npix_clusters) / Y_SIZE) << Y_WIDTH) + 
                           (((p+2) / npix_clusters) % Y_SIZE);
            dst_index   = ((p+2) % npix_clusters) * NL + l;
            _byte_extended_write( dst_cluster, 
                                  (unsigned int)&buf_out[dst_index], 
                                  byte2 );

            dst_cluster = ((((p+3) / npix_clusters) / Y_SIZE) << Y_WIDTH) + 
                           (((p+3) / npix_clusters) % Y_SIZE);
            dst_index   = ((p+3) % npix_clusters) * NL + l;
            _byte_extended_write( dst_cluster, 
                                  (unsigned int)&buf_out[dst_index], 
                                  byte3 );
         }
      }

      PRINTF("*** proc [%d,%d,0] complete transpose for image %d at cycle %d\n", 
             x, y, image, _proctime() );

      TRSP_END[cluster_id][lpid] = _proctime();

      _barrier_wait(1);

      // optional parallel display from local buf_out to frame buffer

      if ( DISPLAY_OK )
      {
          PRINTF("\n*** proc [%d,%d,0] starts display for image %d at cycle %d\n", 
                 x, y, image, _proctime() );

          DISP_START[cluster_id][lpid] = _proctime();

          unsigned int npxt = npixels / ntasks;   // number of pixels per task
          unsigned int buffer = (unsigned int)buf_out + npxt*lpid;

          _fb_sync_write( npxt * task_id, buffer, npxt, cluster_xy );

          PRINTF("*** Proc [%d,%d,0] completes display for image %d at cycle %d\n",
                 x, y, image, _proctime() );

          DISP_END[cluster_id][lpid] = _proctime();

          _barrier_wait(2);
      }

      // checksum (done by processor 0 in each cluster)

      if ( lpid == 0 )
      {
         PRINTF("\n*** Proc [%d,%d,0] starts checks for image %d at cycle %d\n\n",
                x, y, image, _proctime() );

         unsigned int success = 1;
         unsigned int start   = cluster_id * (NL / nclusters);
         unsigned int stop    = start + (NL / nclusters);

         for ( l = start ; l < stop ; l++ )
         {
            check_line_after[l] = 0;

            for ( p = 0 ; p < NP ; p++ )
            {
               // read one byte in remote buffer
               src_cluster = (((p / npix_clusters) / Y_SIZE) << Y_WIDTH) +
                             ((p / npix_clusters) % Y_SIZE);
               src_index   = (p % npix_clusters) * NL + l;

               unsigned char byte = _byte_extended_read( src_cluster,
                                                         (unsigned int)&buf_out[src_index] );

               check_line_after[l] = check_line_after[l] + byte;
            }

            if( CHECK_VERBOSE )
            {
                PRINTF(" - l = %d / before = %d / after = %d \n",
                       l, check_line_before[l], check_line_after[l] );
            }

            if ( check_line_before[l] != check_line_after[l] ) success = 0;
         }

         if ( success ) PRINTF("\n*** proc [%d,%d,0] : CHECKSUM OK \n\n", x, y);
         else           PRINTF("\n*** proc [%d,%d,0] : CHECKSUM KO \n\n", x, y);
      }

      // instrumentation ( done by processor [0,0,0]

      if ( (proc_id == 0) && INSTRUMENTATION_OK )
      {
         int cc, pp;
         unsigned int min_load_start = INT_MAX;
         unsigned int max_load_start = 0;
         unsigned int min_load_ended = INT_MAX;
         unsigned int max_load_ended = 0;
         unsigned int min_trsp_start = INT_MAX;
         unsigned int max_trsp_start = 0;
         unsigned int min_trsp_ended = INT_MAX;
         unsigned int max_trsp_ended = 0;
         unsigned int min_disp_start = INT_MAX;
         unsigned int max_disp_start = 0;
         unsigned int min_disp_ended = INT_MAX;
         unsigned int max_disp_ended = 0;

         for (cc = 0; cc < nclusters; cc++)
         {
            for (pp = 0; pp < NB_PROCS_MAX; pp++)
            {
               if (LOAD_START[cc][pp] < min_load_start)  min_load_start = LOAD_START[cc][pp];
               if (LOAD_START[cc][pp] > max_load_start)  max_load_start = LOAD_START[cc][pp];
               if (LOAD_END[cc][pp]   < min_load_ended)  min_load_ended = LOAD_END[cc][pp]; 
               if (LOAD_END[cc][pp]   > max_load_ended)  max_load_ended = LOAD_END[cc][pp];
               if (TRSP_START[cc][pp] < min_trsp_start)  min_trsp_start = TRSP_START[cc][pp];
               if (TRSP_START[cc][pp] > max_trsp_start)  max_trsp_start = TRSP_START[cc][pp];
               if (TRSP_END[cc][pp]   < min_trsp_ended)  min_trsp_ended = TRSP_END[cc][pp];
               if (TRSP_END[cc][pp]   > max_trsp_ended)  max_trsp_ended = TRSP_END[cc][pp];
               if (DISP_START[cc][pp] < min_disp_start)  min_disp_start = DISP_START[cc][pp];
               if (DISP_START[cc][pp] > max_disp_start)  max_disp_start = DISP_START[cc][pp];
               if (DISP_END[cc][pp]   < min_disp_ended)  min_disp_ended = DISP_END[cc][pp];
               if (DISP_END[cc][pp]   > max_disp_ended)  max_disp_ended = DISP_END[cc][pp];
            }
         }

         PRINTF(" - LOAD_START : min = %d / max = %d / med = %d / delta = %d\n",
               min_load_start, max_load_start, (min_load_start+max_load_start)/2, 
               max_load_start-min_load_start); 

         PRINTF(" - LOAD_END   : min = %d / max = %d / med = %d / delta = %d\n",
               min_load_ended, max_load_ended, (min_load_ended+max_load_ended)/2, 
               max_load_ended-min_load_ended); 

         PRINTF(" - TRSP_START : min = %d / max = %d / med = %d / delta = %d\n",
               min_trsp_start, max_trsp_start, (min_trsp_start+max_trsp_start)/2, 
               max_trsp_start-min_trsp_start); 

         PRINTF(" - TRSP_END   : min = %d / max = %d / med = %d / delta = %d\n",
               min_trsp_ended, max_trsp_ended, (min_trsp_ended+max_trsp_ended)/2, 
               max_trsp_ended-min_trsp_ended); 

         PRINTF(" - DISP_START : min = %d / max = %d / med = %d / delta = %d\n",
               min_disp_start, max_disp_start, (min_disp_start+max_disp_start)/2, 
               max_disp_start-min_disp_start); 

         PRINTF(" - DISP_END   : min = %d / max = %d / med = %d / delta = %d\n",
               min_disp_ended, max_disp_ended, (min_disp_ended+max_disp_ended)/2, 
               max_disp_ended-min_disp_ended); 
      }

      image++;

      _barrier_wait( 3 );
   } // end while image      


   _exit();

} // end main()

// Local Variables:
// tab-width: 3
// c-basic-offset: 3
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3



