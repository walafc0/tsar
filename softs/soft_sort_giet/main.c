//=================================================================================================
// File       : main.c
//
// Date       : 08/2011
//
// Author     : Cesar Fuguet Tortolero
//            :<cesar.fuguet-tortolero@lip6.fr>
//
// Description: Sort application using the GIET operating system.
//              This application uses the barrier routines to apply a sort algorithm
//              in several stages.
//=================================================================================================

#include "stdio.h"
#include "hard_config.h"

#define ARRAY_LENGTH (1 << 10)                    // 1024 ITEMS
#define IPP          (ARRAY_LENGTH / total_procs) // ITEMS PER PROCESSOR
#define VERBOSE      1

#if (VERBOSE == 1)
#define printf(...) _tty_printf(__VA_ARGS__)
#define puts(...)   _tty_puts(__VA_ARGS__)
#else
#define printf(...)
#define puts(...)
#endif

#define task0_printf(...) if(thread_id == 0) _tty_printf(__VA_ARGS__)

int array0[ARRAY_LENGTH];
int array1[ARRAY_LENGTH];

volatile int init_ok = 0;

void bubbleSort(int * array, unsigned int length, unsigned int init_pos);
void merge(int * array, int * result, int length, int init_pos_a, int init_pos_b, int init_pos_result);

void main()
{
    int total_procs = NB_PROCS_MAX * X_SIZE * Y_SIZE;
    int proc_id     = _procid();
    int lid         = proc_id % NB_PROCS_MAX;
    int cluster_xy  = proc_id / NB_PROCS_MAX;
    int x           = (cluster_xy >> Y_WIDTH);
    int y           = (cluster_xy           ) & ((1 << Y_WIDTH) - 1);

    int thread_id   = ((x * Y_SIZE) + y) * NB_PROCS_MAX + lid; 

    int * src_array;
    int * dst_array;
    int i;

    while((thread_id != 0) && (init_ok == 0));

    /**************************************************************************/
    /* Hello World */

    task0_printf("\n[ PROC_%d_%d_%d ] Starting SORT application\n",x,y,lid);

    task0_printf("[ PROC_%d_%d_%d ] MESH %d x %d x %d processors\n",
                 x,y,lid, X_SIZE, Y_SIZE, NB_PROCS_MAX);

    /**************************************************************************/
    /* Barriers Initialisation */

    if (thread_id == 0)
    {
        for (i = 0; i < __builtin_ctz(total_procs); i++)
        {
            printf("[ PROC_%d_%d_%d ] Initializing barrier %d with %d\n",
                x,y,lid, i, total_procs >> i);

            _barrier_init(i, total_procs >> i);
        }
        printf("\n");
        asm volatile ("sync");
        init_ok = 1;
    }

    /**************************************************************************/
    /* Array Initialisation */

    for (i = IPP * thread_id; i < IPP * (thread_id + 1); i++)
    {
        array0[i] = _rand();
    }

    asm volatile ("sync");
    _barrier_wait(0);

    /**************************************************************************/
    /* Parallel sorting of array pieces */

    printf("[ PROC_%d_%d_%d ] Stage 0: Starting...\n\r", x,y,lid);
    bubbleSort(array0, IPP, IPP * thread_id);
    printf("[ PROC_%d_%d_%d ] Stage 0: Finishing...\n\r", x,y,lid);

    for (i = 0; i < __builtin_ctz(total_procs); i++)
    {
        asm volatile ("sync");
        _barrier_wait(i);

        if((thread_id % (2 << i)) != 0) _exit();

        printf("[ PROC_%d_%d_%d ] Stage %d: Starting...\n\r", x,y,lid, i+1);

        if((i % 2) == 0)
        {
            src_array = &array0[0];
            dst_array = &array1[0];
        }
        else
        {
            src_array = &array1[0];
            dst_array = &array0[0];
        }

        merge(src_array, dst_array
                , IPP << i
                , IPP * thread_id
                , IPP * (thread_id + (1 << i))
                , IPP * thread_id
                );

        printf("[ PROC_%d_%d_%d ] Stage %d: Finishing...\n\r", x,y,lid, i+1);
    }

    int success;
    int failure_index;

    if(thread_id == 0)
    {
        success = 1;

        for(i=0; i<(ARRAY_LENGTH-1); i++)
        {
            if(dst_array[i] > dst_array[i+1])
            {

                success = 0;
                failure_index = i;
                break;
            }
        }

        if (success)
        {
            printf("[ PROC_%d_%d_%d ] Success!!\n\r", x,y,lid);
        }
        else
        {
            printf("Failure!! Incorrect element: %d\n\r", failure_index);


            for(i=0; i<ARRAY_LENGTH; i++)
            {
                printf("array[%d] = %d\n", i, dst_array[i]);
            }
        }
    }

    _exit();
}

void bubbleSort(int * array, unsigned int length, unsigned int init_pos)
{
    int i;
    int j;
    int aux;

    for(i = 0; i < length; i++)
    {
        for(j = init_pos; j < (init_pos + length - i - 1); j++)
        {
            if(array[j] > array[j + 1])
            {
                aux          = array[j + 1];
                array[j + 1] = array[j];
                array[j]     = aux;
            }
        }
    }
}

void merge(int * array, int * result, int length, int init_pos_a, int init_pos_b, int init_pos_result)
{
    int i;
    int j;
    int k;

    i = 0;
    j = 0;
    k = init_pos_result;

    while((i < length) || (j < length))
    {
        if((i < length) && (j < length))
        {
            if(array[init_pos_a + i] < array[init_pos_b + j])
            {
                result[k++] = array[init_pos_a + i];
                i++;
            }
            else
            {
                result[k++] = array[init_pos_b + j];
                j++;
            }
        }
        else if(i < length)
        {
            result[k++] = array[init_pos_a + i];
            i++;
        }
        else
        {
            result[k++] = array[init_pos_b + j];
            j++;
        }
    }
}

/* vim: tabstop=4 : shiftwidth=4 : expandtab
*/
