
#include "limits.h"
#include "stdio.h"

#include "../giet_tsar/block_device.h"

////////////////////////////////////
// Image parameters

#define NB_CLUSTER_MAX 256
#define PIXEL_SIZE     2
#define NL             1024
#define NP             1024

#define NB_PIXELS      ((NP) * (NL))
#define FRAME_SIZE     ((NB_PIXELS) * (PIXEL_SIZE))

#define PRINTF(...)      ({ if (proc_id == 0) { tty_printf(__VA_ARGS__); } })

#define TA(c,l,p)  (A[c][((NP) * (l)) + (p)])
#define TB(c,p,l)  (B[c][((NL) * (p)) + (l)])
#define TC(c,l,p)  (C[c][((NP) * (l)) + (p)])
#define TD(c,l,p)  (D[c][((NP) * (l)) + (p)])
#define TZ(c,l,p)  (Z[c][((NP) * (l)) + (p)])

#define max(x,y) ((x) > (y) ? (x) : (y))
#define min(x,y) ((x) < (y) ? (x) : (y))

///////////////////////////////////////////
// tricks to read parameters from ldscript
///////////////////////////////////////////

struct plaf;

extern struct plouf seg_ioc_base;
extern struct plaf seg_heap_base;
extern struct plaf NB_PROCS;
extern struct plaf NB_CLUSTERS;


// Required when initializing an array all at once
static void *memcpy(void *_dst, const void *_src, unsigned int size){
   unsigned int *dst = _dst;
   const unsigned int *src = _src;
   if (! ((unsigned int)dst & 3) && ! ((unsigned int)src & 3)){
      while (size > 3){
         *dst++ = *src++;
         size -= 4;
      }
   }

   unsigned char *cdst = (unsigned char*)dst;
   unsigned char *csrc = (unsigned char*)src;

   while (size--){
      *cdst++ = *csrc++;
   }
   return _dst;
}








void main(){

   //////////////////////////////////
   // convolution kernel parameters
   // The content of this section is
   // Philips proprietary information.
   ///////////////////////////////////

   int   vnorm  = 115;
   int   vf[35] = { 1, 1, 2, 2, 2,
                    2, 3, 3, 3, 4,
                    4, 4, 4, 5, 5,
                    5, 5, 5, 5, 5,
                    5, 5, 4, 4, 4,
                    4, 3, 3, 3, 2,
                    2, 2, 2, 1, 1 };

   int hrange = 100;
   int hnorm  = 201;

   unsigned int date = 0;

   int c; // cluster index for loops
   int l; // line index for loops
   int p; // pixel index for loops
   int x; // filter index for loops

   const unsigned int proc_id       = procid();                      // processor id
   const unsigned int nlocal_procs  = (int) &NB_PROCS;               // number of processors per cluster
   const unsigned int nclusters     = (int) &NB_CLUSTERS;            // number of clusters
   const unsigned int local_id      = proc_id % nlocal_procs;        // local task id
   const unsigned int cluster_id    = proc_id / nlocal_procs;        // cluster task id
   const unsigned int base          = (unsigned int) &seg_heap_base; // base address for shared buffers
   const unsigned int increment     = 0x80000000 / nclusters * 2;    // cluster increment
   const unsigned int nglobal_procs = nclusters * nlocal_procs;      // number of tasks
   const unsigned int npixels       = NB_PIXELS;                     // Number of pixel per frame
   const unsigned int frame_size    = FRAME_SIZE;                    // Size of 1 frame (in bytes)
   const unsigned int * ioc_address = (unsigned int *) &seg_ioc_base;
   const unsigned int block_size    = ioc_address[BLOCK_DEVICE_BLOCK_SIZE];
   const unsigned int nblocks       = frame_size / block_size;       // number of blocks per frame

   const unsigned int lines_per_task     = NL / nglobal_procs; // number of lines per task
   const unsigned int lines_per_cluster  = NL / nclusters;     // number of lines per cluster
   const unsigned int pixels_per_task    = NP / nglobal_procs; // number of columns per task
   const unsigned int pixels_per_cluster = NP / nclusters;     // number of columns per cluster

   int first, last;

   PRINTF("\n*** Processor %d entering main at cycle %d ***\n\n", proc_id, proctime());


   /////////////////////////
   // parameters checking //
   /////////////////////////
   

   if ((nlocal_procs != 1) && (nlocal_procs != 2) && (nlocal_procs != 4)){
      PRINTF("NB_PROCS must be 1, 2 or 4\n");
      exit();
   }

   ////////////////////////////////////////////////////////////////////////
   // Warning: NB_CLUSTERS must be at least 4 because of the heap size;  //
   //          if there are less clusters, the heap mixes with the stack //
   //          (the total heap size must be at least 0x01000000)         //
   ////////////////////////////////////////////////////////////////////////
   if ((nclusters !=  4) && (nclusters !=  8) && (nclusters != 16) && 
         (nclusters != 32) && (nclusters != 64) && (nclusters !=128) && (nclusters != 256)){
      PRINTF("NB_CLUSTERS must be a power of 2 between 4 and 256\n");
      exit();
   }

   if (proc_id >= nglobal_procs){
      PRINTF("processor id %d larger than NB_CLUSTERS*NB_PROCS\n", proc_id);
      exit();
   }

   if (NL % nclusters != 0){
      PRINTF("NB_CLUSTERS must be a divider of NL");
      exit();
   }

   if (NP % nclusters != 0){
      PRINTF("NB_CLUSTERS must be a divider of NP");
      exit();
   }


   // Arrays of pointers on the shared, distributed buffers  
   // containing the images (sized for the worst case : 256 clusters)
   unsigned short * A[NB_CLUSTER_MAX];
   int *            B[NB_CLUSTER_MAX];
   int *            C[NB_CLUSTER_MAX];
   int *            D[NB_CLUSTER_MAX];
   unsigned char *  Z[NB_CLUSTER_MAX];

   // Arrays of pointers on the instrumentation arrays
   // These arrays are indexed by the cluster index (sized for the worst case : 256 clusters)
   // each pointer points on the base adress of an array of 4 (NPROCS max) unsigned int
   unsigned int * LOAD_START[NB_CLUSTER_MAX];
   unsigned int * LOAD_END[NB_CLUSTER_MAX];
   unsigned int * VERT_START[NB_CLUSTER_MAX];
   unsigned int * VERT_END[NB_CLUSTER_MAX];
   unsigned int * HORI_START[NB_CLUSTER_MAX];
   unsigned int * HORI_END[NB_CLUSTER_MAX];
   unsigned int * DISP_START[NB_CLUSTER_MAX];
   unsigned int * DISP_END[NB_CLUSTER_MAX];

   // The shared, distributed buffers addresses are computed
   // from the seg_heap_base value defined in the ldscript file
   // and from the cluster increment = 4Gbytes/nclusters.
   // These arrays of pointers are identical and
   // replicated in the stack of each task 
   for (c = 0; c < nclusters; c++){
      unsigned int offset = base + increment * c;
      A[c] = (unsigned short *) (offset);
      B[c] = (int *)            (offset + frame_size * 1 / nclusters); // We increment by 2 * frame_size
      C[c] = (int *)            (offset + frame_size * 3 / nclusters); // because sizeof(int) = 2*sizeof(short)
      D[c] = (int *)            (offset + frame_size * 5 / nclusters); // so an array of frame_size elements of type
      Z[c] = (unsigned char *)  (offset + frame_size * 7 / nclusters); // int can contain the equivalent of 2 frames

      offset = base + increment * c + frame_size * 8 / nclusters;
      LOAD_START[c] = (unsigned int *) (offset + nlocal_procs * sizeof(unsigned int) * 0);
      LOAD_END[c]   = (unsigned int *) (offset + nlocal_procs * sizeof(unsigned int) * 1);
      VERT_START[c] = (unsigned int *) (offset + nlocal_procs * sizeof(unsigned int) * 2);
      VERT_END[c]   = (unsigned int *) (offset + nlocal_procs * sizeof(unsigned int) * 3);
      HORI_START[c] = (unsigned int *) (offset + nlocal_procs * sizeof(unsigned int) * 4);
      HORI_END[c]   = (unsigned int *) (offset + nlocal_procs * sizeof(unsigned int) * 5);
      DISP_START[c] = (unsigned int *) (offset + nlocal_procs * sizeof(unsigned int) * 6);
      DISP_END[c]   = (unsigned int *) (offset + nlocal_procs * sizeof(unsigned int) * 7);
   }

   PRINTF("NB_CLUSTERS     = %d\n", nclusters); 
   PRINTF("NB_LOCAL_PROCS  = %d\n", nlocal_procs); 
   PRINTF("NB_GLOBAL_PROCS = %d\n", nglobal_procs);
   PRINTF("NB_PIXELS       = %d\n", npixels);
   PRINTF("PIXEL_SIZE      = %d\n", PIXEL_SIZE);
   PRINTF("FRAME_SIZE      = %d\n", frame_size);
   PRINTF("BLOCK_SIZE      = %d\n", block_size);
   PRINTF("NB_BLOCKS       = %d\n\n", nblocks);


   PRINTF("*** Starting barrier init at cycle %d ***\n", proctime());

   //  barriers initialization
   barrier_init(0, nglobal_procs);
   barrier_init(1, nglobal_procs);
   barrier_init(2, nglobal_procs);
   barrier_init(3, nglobal_procs);

   PRINTF("*** Completing barrier init at cycle %d ***\n", proctime());


   ////////////////////////////////////////////////////////
   // pseudo parallel load from disk to A[c] buffers
   // only task running on processor with (local_id==0) does it
   // nblocks/nclusters are loaded in each cluster
   ////////////////////////////////////////////////////////

   if (local_id == 0){
      int p;
      date  = proctime();
      PRINTF("\n*** Starting load at cycle %d\n", date);
      for (p = 0; p < nlocal_procs; p++){
         LOAD_START[cluster_id][p] = date;
      }

      if (ioc_read(nblocks*cluster_id/nclusters, A[cluster_id], nblocks/nclusters)){
         PRINTF("echec ioc_read\n");
         exit(1);
      }
      if (ioc_completed()){
         PRINTF("echec ioc_completed\n");
         exit(1);
      }

      date  = proctime();
      PRINTF("*** Completing load at cycle %d\n", date);
      for (p = 0; p < nlocal_procs; p++){
         LOAD_END[cluster_id][p] = date;
      }
   }

   barrier_wait(0);


   ////////////////////////////////////////////////////////
   // parallel horizontal filter : 
   // B <= transpose(FH(A))
   // D <= A - FH(A)
   // Each task computes (NL/nglobal_procs) lines 
   // The image must be extended :
   // if (z<0)    TA(cluster_id,l,z) == TA(cluster_id,l,0)
   // if (z>NP-1) TA(cluster_id,l,z) == TA(cluster_id,l,NP-1)
   ////////////////////////////////////////////////////////

   date  = proctime();
   PRINTF("\n*** Starting horizontal filter at cycle %d\n", date);
   HORI_START[cluster_id][local_id] = date;

   // l = absolute line index / p = absolute pixel index  
   // first & last define which lines are handled by a given task(cluster_id,local_id)

   first = (cluster_id * nlocal_procs + local_id) * lines_per_task;
   last  = first + lines_per_task;

   for (l = first; l < last; l++){
      // src_c and src_l are the cluster index and the line index for A & D
      int src_c = l / lines_per_cluster;
      int src_l = l % lines_per_cluster;

      // We use the specific values of the horizontal ep-filter for optimisation:
      // sum(p) = sum(p-1) + TA[p+hrange] - TA[p-hrange-1]
      // To minimize the number of tests, the loop on pixels is split in three domains 

      int sum_p = (hrange + 2) * TA(src_c, src_l, 0);
      for (x = 1; x < hrange; x++){
         sum_p = sum_p + TA(src_c, src_l, x);
      }

      // first domain : from 0 to hrange
      for (p = 0; p < hrange + 1; p++){
         // dst_c and dst_p are the cluster index and the pixel index for B
         int dst_c = p / pixels_per_cluster;
         int dst_p = p % pixels_per_cluster;
         sum_p = sum_p + (int) TA(src_c, src_l, p + hrange) - (int) TA(src_c, src_l, 0);
         TB(dst_c, dst_p, l) = sum_p / hnorm;
         TD(src_c, src_l, p) = (int) TA(src_c, src_l, p) - sum_p / hnorm;
      }
      // second domain : from (hrange+1) to (NP-hrange-1)
      for (p = hrange + 1; p < NP - hrange; p++){
         // dst_c and dst_p are the cluster index and the pixel index for B
         int dst_c = p / pixels_per_cluster;
         int dst_p = p % pixels_per_cluster;
         sum_p = sum_p + (int) TA(src_c, src_l, p + hrange) - (int) TA(src_c, src_l, p - hrange - 1);
         TB(dst_c, dst_p, l) = sum_p / hnorm;
         TD(src_c, src_l, p) = (int) TA(src_c, src_l, p) - sum_p / hnorm;
      }
      // third domain : from (NP-hrange) to (NP-1)
      for (p = NP - hrange; p < NP; p++){
         // dst_c and dst_p are the cluster index and the pixel index for B
         int dst_c = p / pixels_per_cluster;
         int dst_p = p % pixels_per_cluster;
         sum_p = sum_p + (int) TA(src_c, src_l, NP - 1) - (int) TA(src_c, src_l, p - hrange - 1);
         TB(dst_c, dst_p, l) = sum_p / hnorm;
         TD(src_c, src_l, p) = (int) TA(src_c, src_l, p) - sum_p / hnorm;
      }

      PRINTF(" - line %d computed at cycle %d\n", l, proctime());
   }

   date  = proctime();
   PRINTF("*** Completing horizontal filter at cycle %d\n", date);
   HORI_END[cluster_id][local_id] = date;

   barrier_wait(1);


   //////////////////////////////////////////////////////////
   // parallel vertical filter : 
   // C <= transpose(FV(B))
   // Each task computes (NP/nglobal_procs) columns
   // The image must be extended :
   // if (l<0)    TB(cluster_id,p,x) == TB(cluster_id,p,0)
   // if (l>NL-1)   TB(cluster_id,p,x) == TB(cluster_id,p,NL-1)
   //////////////////////////////////////////////////////////

   date  = proctime();
   PRINTF("\n*** starting vertical filter at cycle %d\n", date);
   VERT_START[cluster_id][local_id] = date;

   // l = absolute line index / p = absolute pixel index
   // first & last define which pixels are handled by a given task(cluster_id,local_id)

   first = (cluster_id * nlocal_procs + local_id) * pixels_per_task;
   last  = first + pixels_per_task;

   for (p = first; p < last; p++){
      // src_c and src_p are the cluster index and the pixel index for B
      int src_c = p / pixels_per_cluster;
      int src_p = p % pixels_per_cluster;

      int sum_l;

      // We use the specific values of the vertical ep-filter
      // To minimize the number of tests, the NL lines are split in three domains 

      // first domain : explicit computation for the first 18 values
      for (l = 0; l < 18; l++){
         // dst_c and dst_l are the cluster index and the line index for C
         int dst_c = l / lines_per_cluster;
         int dst_l = l % lines_per_cluster;

         for (x = 0, sum_l = 0; x < 35; x++){
            sum_l = sum_l + vf[x] * TB(src_c, src_p, max(l - 17 + x,0) );
         }
         TC(dst_c, dst_l, p) = sum_l / vnorm;
      }
      // second domain
      for (l = 18; l < NL - 17; l++){
         // dst_c and dst_l are the cluster index and the line index for C
         int dst_c = l / lines_per_cluster;
         int dst_l = l % lines_per_cluster;

         sum_l = sum_l + TB(src_c, src_p, l + 4)
            + TB(src_c, src_p, l + 8)
            + TB(src_c, src_p, l + 11)
            + TB(src_c, src_p, l + 15)
            + TB(src_c, src_p, l + 17)
            - TB(src_c, src_p, l - 5)
            - TB(src_c, src_p, l - 9)
            - TB(src_c, src_p, l - 12)
            - TB(src_c, src_p, l - 16)
            - TB(src_c, src_p, l - 18);
         TC(dst_c, dst_l, p) = sum_l / vnorm;
      }
      // third domain
      for (l = NL - 17; l < NL; l++){
         // dst_c and dst_l are the cluster index and the line index for C
         int dst_c = l / lines_per_cluster;
         int dst_l = l % lines_per_cluster;

         sum_l = sum_l + TB(src_c, src_p, min(l + 4, NL - 1))
            + TB(src_c, src_p, min(l + 8, NL - 1))
            + TB(src_c, src_p, min(l + 11, NL - 1))
            + TB(src_c, src_p, min(l + 15, NL - 1))
            + TB(src_c, src_p, min(l + 17, NL - 1))
            - TB(src_c, src_p, l - 5)
            - TB(src_c, src_p, l - 9)
            - TB(src_c, src_p, l - 12)
            - TB(src_c, src_p, l - 16)
            - TB(src_c, src_p, l - 18);
         TC(dst_c, dst_l, p) = sum_l / vnorm;
      }
      PRINTF(" - column %d computed at cycle %d\n", p, proctime());
   }

   date  = proctime();
   PRINTF("*** Completing vertical filter at cycle %d\n", date);
   VERT_END[cluster_id][local_id] = date;

   barrier_wait(2);


   ////////////////////////////////////////////////////////////////
   // final computation and parallel display 
   // Z <= D + C
   // Each processor use its private DMA channel to display 
   // the resulting image, line  per line (one byte per pixel).
   // Eah processor computes & displays (NL/nglobal_procs) lines. 
   ////////////////////////////////////////////////////////////////

   date  = proctime();
   PRINTF("\n*** Starting display at cycle %d\n", date);
   DISP_START[cluster_id][local_id] = date;

   first = local_id * lines_per_task;
   last  = first + lines_per_task;

   for (l = first; l < last; l++){
      for (p = 0; p < NP; p++){
         TZ(cluster_id,l,p) = (unsigned char) (((TD(cluster_id,l,p) + TC(cluster_id,l,p)) >> 8) & 0xFF);
      }
      fb_write(NP * (cluster_id * lines_per_cluster + l), &TZ(cluster_id,l,0), NP);
   }

   date  = proctime();
   PRINTF("*** Completing display at cycle %d\n", date);
   DISP_END[cluster_id][local_id] = date;

   barrier_wait(3);

   
   /////////////////////////////////////////////////////////
   // Instrumentation (done by processor 0 in cluster 0)   
   /////////////////////////////////////////////////////////

   if (proc_id == 0){
      date  = proctime();
      PRINTF("\n*** Starting Instrumentation at cycle %d\n\n", date);

      int cc, pp;
      unsigned int min_load_start = INT_MAX;
      unsigned int max_load_start = 0;
      unsigned int min_load_ended = INT_MAX;
      unsigned int max_load_ended = 0;

      unsigned int min_hori_start = INT_MAX;
      unsigned int max_hori_start = 0;
      unsigned int min_hori_ended = INT_MAX;
      unsigned int max_hori_ended = 0;

      unsigned int min_vert_start = INT_MAX;
      unsigned int max_vert_start = 0;
      unsigned int min_vert_ended = INT_MAX;
      unsigned int max_vert_ended = 0;

      unsigned int min_disp_start = INT_MAX;
      unsigned int max_disp_start = 0;
      unsigned int min_disp_ended = INT_MAX;
      unsigned int max_disp_ended = 0;

      for (cc = 0; cc < nclusters; cc++){
         for (pp = 0; pp < nlocal_procs; pp++ ){
            if (LOAD_START[cc][pp] < min_load_start){
               min_load_start = LOAD_START[cc][pp];
            }
            if (LOAD_START[cc][pp] > max_load_start){
               max_load_start = LOAD_START[cc][pp];
            }
            if (LOAD_END[cc][pp] < min_load_ended){
               min_load_ended = LOAD_END[cc][pp];
            }
            if (LOAD_END[cc][pp] > max_load_ended){
               max_load_ended = LOAD_END[cc][pp];
            }

            if (HORI_START[cc][pp] < min_hori_start){
               min_hori_start = HORI_START[cc][pp];
            }
            if (HORI_START[cc][pp] > max_hori_start){
               max_hori_start = HORI_START[cc][pp];
            }
            if (HORI_END[cc][pp] < min_hori_ended){
               min_hori_ended = HORI_END[cc][pp];
            }
            if (HORI_END[cc][pp] > max_hori_ended){
               max_hori_ended = HORI_END[cc][pp];
            }

            if (VERT_START[cc][pp] < min_vert_start){
               min_vert_start = VERT_START[cc][pp];
            }
            if (VERT_START[cc][pp] > max_vert_start){
               max_vert_start = VERT_START[cc][pp];
            }
            if (VERT_END[cc][pp] < min_vert_ended){
               min_vert_ended = VERT_END[cc][pp];
            }
            if (VERT_END[cc][pp] > max_vert_ended){
               max_vert_ended = VERT_END[cc][pp];
            }

            if (DISP_START[cc][pp] < min_disp_start){
               min_disp_start = DISP_START[cc][pp];
            }
            if (DISP_START[cc][pp] > max_disp_start){
               max_disp_start = DISP_START[cc][pp];
            }
            if (DISP_END[cc][pp] < min_disp_ended){
               min_disp_ended = DISP_END[cc][pp];
            }
            if (DISP_END[cc][pp] > max_disp_ended){
               max_disp_ended = DISP_END[cc][pp];
            }
         }
      }
      PRINTF(" - LOAD_START : min = %d / max = %d / med = %d / delta = %d\n",
            min_load_start, max_load_start, (min_load_start+max_load_start) / 2, max_load_start-min_load_start);
      PRINTF(" - LOAD_END   : min = %d / max = %d / med = %d / delta = %d\n",
            min_load_ended, max_load_ended, (min_load_ended+max_load_ended) / 2, max_load_ended-min_load_ended);

      PRINTF(" - HORI_START : min = %d / max = %d / med = %d / delta = %d\n",
            min_hori_start, max_hori_start, (min_hori_start+max_hori_start) / 2, max_hori_start-min_hori_start);
      PRINTF(" - HORI_END   : min = %d / max = %d / med = %d / delta = %d\n",
            min_hori_ended, max_hori_ended, (min_hori_ended+max_hori_ended) / 2, max_hori_ended-min_hori_ended);

      PRINTF(" - VERT_START : min = %d / max = %d / med = %d / delta = %d\n",
            min_vert_start, max_vert_start, (min_vert_start+max_vert_start) / 2, max_vert_start-min_vert_start);
      PRINTF(" - VERT_END   : min = %d / max = %d / med = %d / delta = %d\n",
            min_vert_ended, max_vert_ended, (min_vert_ended+max_vert_ended) / 2, max_vert_ended-min_vert_ended);

      PRINTF(" - DISP_START : min = %d / max = %d / med = %d / delta = %d\n",
            min_disp_start, max_disp_start, (min_disp_start+max_disp_start) / 2, max_disp_start-min_disp_start);
      PRINTF(" - DISP_END   : min = %d / max = %d / med = %d / delta = %d\n",
            min_disp_ended, max_disp_ended, (min_disp_ended+max_disp_ended) / 2, max_disp_ended-min_disp_ended);

      PRINTF(" - BARRIER LOAD/HORI = %d\n", min_hori_start - max_load_ended);
      PRINTF(" - BARRIER HORI/VERT = %d\n", min_vert_start - max_hori_ended);
      PRINTF(" - BARRIER VERT/DISP = %d\n", min_disp_start - max_vert_ended);

      PRINTF(" - LOAD              = %d\n", max_load_ended);
      PRINTF(" - FILTER            = %d\n", max_vert_ended - max_load_ended);
      PRINTF(" - DISPLAY           = %d\n", max_disp_ended - max_vert_ended);

      PRINTF("\nBEGIN LOAD_START\n");
      for (cc = 0; cc < nclusters; cc++){
         for (pp = 0; pp < nlocal_procs; pp++){
            PRINTF("cluster= %d proc= %d date= %d\n", cc, pp, LOAD_START[cc][pp]);  
         }
      }
      PRINTF("END\n");

      PRINTF("\nBEGIN LOAD_END\n");
      for (cc = 0; cc < nclusters; cc++){
         for (pp = 0; pp < nlocal_procs; pp++){
            PRINTF("cluster= %d proc= %d date= %d\n", cc, pp, LOAD_END[cc][pp]);  
         }
      }
      PRINTF("END\n");

      PRINTF("\nBEGIN HORI_START\n");
      for (cc = 0; cc < nclusters; cc++){
         for (pp = 0; pp < nlocal_procs; pp++){
            PRINTF("cluster= %d proc= %d date= %d\n", cc, pp, HORI_START[cc][pp]);  
         }
      }
      PRINTF("END\n");

      PRINTF("\nBEGIN HORI_END\n");
      for (cc = 0; cc < nclusters; cc++){
         for (pp = 0; pp < nlocal_procs; pp++){
            PRINTF("cluster= %d proc= %d date= %d\n", cc, pp, HORI_END[cc][pp]);  
         }
      }
      PRINTF("END\n");

      PRINTF("\nBEGIN VERT_START\n");
      for (cc = 0; cc < nclusters; cc++){
         for (pp = 0; pp < nlocal_procs; pp++){
            PRINTF("cluster= %d proc= %d date= %d\n", cc, pp, VERT_START[cc][pp]);  
         }
      }
      PRINTF("END\n");

      PRINTF("\nBEGIN VERT_END\n");
      for (cc = 0; cc < nclusters; cc++){
         for (pp = 0; pp < nlocal_procs; pp++ ){
            PRINTF("cluster= %d proc= %d date= %d\n", cc, pp, VERT_END[cc][pp]);  
         }
      }
      PRINTF("END\n");

      PRINTF("\nBEGIN DISP_START\n");
      for (cc = 0; cc < nclusters; cc++){
         for (pp = 0; pp < nlocal_procs; pp++){
            PRINTF("cluster= %d proc= %d date= %d\n", cc, pp, DISP_START[cc][pp]);  
         }
      }
      PRINTF("END\n");

      PRINTF("\nBEGIN DISP_END\n");
      for (cc = 0; cc < nclusters; cc++){
         for (pp = 0; pp < nlocal_procs; pp++){
            PRINTF("cluster= %d proc= %d date= %d\n", cc, pp, DISP_END[cc][pp]);  
         }
      }
      PRINTF("END\n");
   }

   while(1);

} // end main()

// Local Variables:
// tab-width: 3
// c-basic-offset: 3
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:

// vim: filetype=cpp:expandtab:shiftwidth=3:tabstop=3:softtabstop=3


