#ifndef __MPIBIND_H__
#define __MPIBIND_H__

#define __DEBUG

/****************************************************************************/
/*                               INCLUDES                                   */
/****************************************************************************/
#include "hwloc.h"

/****************************************************************************/
/*                              STRUCTURES                                  */
/****************************************************************************/
/**** hwloc object list ****/
struct mpibind_process_s{
    int local_id;
    int global_id;
    int nb_thread;
};
typedef struct mpibind_process_s mpibind_process;

/**** hwloc object list ****/
struct hwloc_obj_l_s{
    struct hwloc_obj_l_s *next;
    hwloc_obj_t obj;
};
typedef struct hwloc_obj_l_s hwloc_obj_l;

/**** hwloc cpuset list ****/
struct hwloc_cpuset_l_s{
    struct hwloc_cpuset_l_s *next;
    hwloc_bitmap_t cpuset;
};
typedef struct hwloc_cpuset_l_s hwloc_cpuset_l;

/**** hwloc gpu list ****/
struct hwloc_gpu_l_s{
    struct hwloc_gpu_l_s *next;
    hwloc_obj_t gpu;
};
typedef struct hwloc_gpu_l_s hwloc_gpu_l;

/**** hwloc package lists ****/
struct hwloc_pkg_l_s{
    struct hwloc_pkg_l_s *next;
    hwloc_obj_t pkg;                /* Pointer to the hwloc package object */
    int nb_core;                    /* Number of cores in this package */
    int nb_pu;                      /* Number of pus in this package */
    int nb_process;                 /* Number of processes to be mapped on this package */
    int nb_worker;                  /* Number of worker threads to be maped on this package */
    int mapping_depth;              /* Depths used to mapped process (mpibind) */
    mpibind_process *process;       /* Array of process to map on this package */
    hwloc_gpu_l *gpu_l;             /* List of GPUs associated to this package */
    hwloc_cpuset_l *cpuset_l;       /* List of cpusets, one per process in mapped on the package */
};
typedef struct hwloc_pkg_l_s hwloc_pkg_l;

/****************************************************************************/
/*                               FUNCTIONS                                  */
/****************************************************************************/

/****                          MAIN FUNCTION                             ****/
/** mpibind
 * local_nprocess: number of process on the target node
 * nthread: number of threads per process.
 *          Can be 0 if not defined
 *          = -1 if application is not threaded
 */
void mpibind(int local_nprocess, int nthread);


/****                         PRINT FUNCTIONS                            ****/
/** mpibind_dryrun_print
 * Print results in the dryrun format
 */
void mpibind_dryrun_print(hwloc_pkg_l*);


/****                         MPIBIND FUNCTIONS                          ****/
/** not_implemented
 * Place holder
 */
void not_implemented();

/** mpibind_get_topology
 * Create and retrieve hwloc topology
 */
hwloc_topology_t mpibind_get_topology();

/** mpibind_get_package_number
 * Get the number of packages
 * Only used to print something if __DEBUG is defined
 */
void mpibind_get_package_number(hwloc_topology_t);

/** mpibind_package_list_init
 * Create a list of package from the topology
 * Determine the number of cores and pu per package
 * Also determine the number of process per package and initialise an array of process per package
 */
void mpibind_package_list_init(hwloc_topology_t, hwloc_pkg_l**, int);

/** mpibind_package_list_destroy
 * Free package list
 */
void mpibind_package_list_destroy(hwloc_pkg_l**);

/** mpibind_compute_thread_number_per_package
 * First try to get thread number from function arguments
 * If needed try to get them from OpenMP environment variable
 * If needed compute the number from topology
 * Arguments:
 *   -> int = -1 : application is not threaded -> one thread per process
 *   -> int > 0  : application is threaded, number of thread is given by user
 *   -> int = 0  : application is threaded, number of threads needs to be computed
 */
void mpibind_compute_thread_number_per_package(hwloc_topology_t, hwloc_pkg_l**, int);

/** mpibind_mappind_depth_per_package
 * For each package find the highest topology level such that nvertices >= nb_worker
 */
void mpibind_mappind_depth_per_package(hwloc_topology_t, hwloc_pkg_l**);

/** mpibind_create_cpuset
 * Create a cpuset for each process, for each package in the package list
 */
void mpibind_create_cpuset(hwloc_topology_t, hwloc_pkg_l**);

/** mpibind_gpu_list_init
 * Create a list of all gpu present on this topology
 */
void mpibind_gpu_list_init(hwloc_topology_t, hwloc_gpu_l**);

/** mpibind_gpu_list_destroy
 * Free gpu list
 */
void mpibind_gpu_list_destroy(hwloc_gpu_l**);

/** mpibind_assign_gpu
 * Assign gpus to packages
 */
void mpibind_assign_gpu(hwloc_topology_t, hwloc_pkg_l**, hwloc_gpu_l**);


#endif /* __MPIBIND_H__ */