#include "core/include/mpibind.h"

/****                           GLOBAL VARS                              ****/
int verbose = -1;
int dryrun  = -1;
int smt     = -1;

/****                          MAIN FUNCTION                             ****/
void mpibind(int local_nprocess, int nthread){
    hwloc_topology_t topology;

    hwloc_pkg_l *pkg_l;
    hwloc_gpu_l *gpu_l;

    pkg_l = NULL; gpu_l = NULL;

    /**** Get function options from the MPIBIND environment variable ****/
    mpibind_gather_options();

    /**** Create and retrive topology ****/
    topology = mpibind_get_topology();

    /**** Get the number of packages ****/
    mpibind_get_package_number(topology);

    /**** Determine the number of cores and pu per package ****/
    /**** Also determine the number of process per package ****/
    mpibind_package_list_init(topology, &pkg_l, local_nprocess);

    /**** Get number of threads with OMP_NUM_THREADS or function arguments ****/
    mpibind_compute_thread_number_per_package(topology, &pkg_l, nthread);

    /**** For each package find the highest topology level such that nvertices >= nb_worker ****/
    mpibind_mappind_depth_per_package(topology, &pkg_l);

    /**** Create process cpusets ****/
    mpibind_create_cpuset(topology, &pkg_l);

    /*** GPU management ***/
    mpibind_gpu_list_init(topology, &gpu_l);
    mpibind_assign_gpu(topology, &pkg_l, &gpu_l);

    /**** Print results as mpibind dryrun ****/
    if(verbose || dryrun) mpibind_dryrun_print(pkg_l);
    
    /**** Create a message Flux to transmit cpusets ****/
    if(!dryrun) not_implemented();

//    /**** Free stuff  ****/
//    /* Free package list */
//    mpibind_package_list_destroy(&pkg_l);
//    /* Free the full gpu list */
//    mpibind_gpu_list_destroy(&gpu_l);
//    /* Destroy hwloc topology object. */
//    hwloc_topology_destroy(topology);
}

