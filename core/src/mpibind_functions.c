#include "core/include/mpibind.h"

#include <math.h>       /* For ceil(...) */

/****                         MPIBIND FUNCTIONS                          ****/
void not_implemented(){}

void mpibind_gather_options(){
    char *mpibind_env;
    char *ptr, *ptr2;

    mpibind_env = getenv("MPIBIND");
    if(mpibind_env == NULL)       /* Check if environment varibale exists */
        return;

    /* Separate the string inti individual option strings */
    ptr2 = strtok (mpibind_env, ".");
    while (ptr2 != NULL){
        if(!strcmp(ptr2, "v")){
            verbose = 1;
        }
        else if(!strcmp(ptr2, "dry")){
            dryrun = 1;
        }
        else{
          fprintf(stderr, "CAFEBABE %s\n", ptr2);
          ptr = strstr(ptr2, "smt=");
          if(ptr != NULL){
              fprintf(stderr, "CAFEBABE\n");
              while(*ptr != '='){
                  ptr++;
              }
              ptr++;
              smt = atoi(ptr);
          }
        }
        ptr2 = strtok (NULL, ".");
    }
    #ifdef __DEBUG
    fprintf(stderr, "*** Getting options from the MPIBIND environment variable:\n");
    fprintf(stderr, "\tverbose: %d\n\tdryrun: %d\n\tsmt: %d\n\n", verbose, dryrun, smt);
    #endif /* __DEBUG */
}

hwloc_topology_t mpibind_get_topology(){
    int ret;
    hwloc_topology_t topology;
    const char *topo_path;

    /* Get topology path from environment variable*/
    topo_path = getenv("MPIBIND_TOPOLOGY_FILE");
    if(topo_path == NULL){      /* Check if environment varibale exists */
        fprintf(stderr, "##ERROR: Could not get topology file from environment variable 'MPIBIND_TOPOLOGY_FILE'\nExit\n");
        exit(1);
    }
    if(access(topo_path, F_OK) == -1){     /* Check if topology file exists */
        fprintf(stderr, "##ERROR: Could not open topology file '%s'\nExit\n", topo_path);
        exit(1);
    }

    /* Allocate and initialize topology object */
    hwloc_topology_init(&topology);
    /* Retrieve topology from xml file */
    ret = hwloc_topology_set_xml(topology, topo_path);
    if(ret == -1){
        fprintf(stderr, "##ERROR: Hwloc failed to load topology file '%s'\nExit\n", topo_path);
        exit(1);
    }
    /* Set flags to retrieve PCI devices */
    hwloc_topology_set_flags(topology, HWLOC_TOPOLOGY_FLAG_THISSYSTEM_ALLOWED_RESOURCES);
    hwloc_topology_set_all_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
    /* Build Topology */
    hwloc_topology_load(topology);
    
    return topology;
}

void mpibind_get_package_number(hwloc_topology_t topology){
    #ifdef __DEBUG
    fprintf(stderr, "*** Getting the number of packages\n\tPackage number: ");
    if (hwloc_get_type_depth(topology, HWLOC_OBJ_PACKAGE) == HWLOC_TYPE_DEPTH_UNKNOWN) printf("unknown\n");
    else printf("%u\n", hwloc_get_nbobjs_by_depth(topology, hwloc_get_type_depth(topology, HWLOC_OBJ_PACKAGE)));
    #endif /* __DEBUG */
}

void mpibind_package_list_init(hwloc_topology_t topology, hwloc_pkg_l **pkg_l, int local_nprocess){
    int i, ii, tmp_int;
    hwloc_pkg_l *tmp_pkg_l;
    static int process_loc_id_keeper = 0;

    #ifdef __DEBUG
    fprintf(stderr, "\n*** Getting the number of cores/pus/process per package\n");
    #endif /* __DEBUG */
    tmp_int = local_nprocess; /* Used to count remaining processes to distribute */
    tmp_pkg_l = *pkg_l;       /* Used to easily insert package in the list */
    for (i=0; i<hwloc_get_nbobjs_by_depth(topology, hwloc_get_type_depth(topology, HWLOC_OBJ_PACKAGE)); i++){
        /* Crete new list element */
        hwloc_pkg_l *pkg_obj; pkg_obj = NULL;
        pkg_obj = malloc(sizeof(hwloc_pkg_l));
        /* Fill object */
        pkg_obj->next       = NULL;
        pkg_obj->pkg        = hwloc_get_obj_by_depth(topology, hwloc_get_type_depth(topology, HWLOC_OBJ_PACKAGE), i);
        pkg_obj->nb_core    = hwloc_get_nbobjs_inside_cpuset_by_type(topology, pkg_obj->pkg->cpuset, HWLOC_OBJ_CORE);
        pkg_obj->nb_pu      = hwloc_get_nbobjs_inside_cpuset_by_type(topology, pkg_obj->pkg->cpuset, HWLOC_OBJ_PU);
        pkg_obj->nb_process = ceil(tmp_int / 
                                (hwloc_get_nbobjs_by_depth(topology, hwloc_get_type_depth(topology, HWLOC_OBJ_PACKAGE)) - i)); 
                                /* Remaining processes / remaining packages */
        tmp_int--;
        pkg_obj->nb_worker  = 0;
        pkg_obj->gpu_l      = NULL;
        pkg_obj->cpuset_l   = NULL;
        pkg_obj->process    = NULL;
        pkg_obj->process = malloc(pkg_obj->nb_process*sizeof(mpibind_process));
        for(ii=0; ii<pkg_obj->nb_process; ii++){
            pkg_obj->process[ii].local_id = process_loc_id_keeper;
            process_loc_id_keeper++;
            pkg_obj->process[ii].global_id = -1;
            pkg_obj->process[ii].nb_thread = -1;
        }
        /* Debug prints */
        #ifdef __DEBUG
        fprintf(stderr, "\tPackage%d: cores:%d, pus:%d, processes:%d\n", 
                pkg_obj->pkg->os_index, pkg_obj->nb_core, pkg_obj->nb_pu, pkg_obj->nb_process);
        #endif /* __DEBUG */
        /* Add to the list */
        if(*pkg_l == NULL) *pkg_l = pkg_obj;
        else tmp_pkg_l->next = pkg_obj;
        tmp_pkg_l = pkg_obj;
        if(tmp_int == 0) break;
    }
}

void mpibind_package_list_destroy(hwloc_pkg_l **pkg_l){
    hwloc_pkg_l *tmp_pkg_l;
    hwloc_gpu_l *tmp_gpu_l, *tmp_gpu_l_2;
    hwloc_cpuset_l *tmp_cpuset_l;

    /* Free pkg_list (with their cpuset and gpu list) */
    tmp_pkg_l = *pkg_l;              /* Used to traverse the package list */
    while(tmp_pkg_l != NULL){
        hwloc_pkg_l *tmp_pkg_l_2;
        /* Free cpuset list */
        hwloc_cpuset_l *tmp_cpuset_l_2;
        tmp_cpuset_l = tmp_pkg_l->cpuset_l;
        while(tmp_cpuset_l != NULL){
            tmp_cpuset_l_2 = tmp_cpuset_l;
            tmp_cpuset_l   = tmp_cpuset_l->next;
            hwloc_bitmap_free(tmp_cpuset_l_2->cpuset);
            free(tmp_cpuset_l_2);
        }
        /* Free gpu list */
        tmp_gpu_l = tmp_pkg_l->gpu_l;
        while(tmp_gpu_l != NULL){
            tmp_gpu_l_2 = tmp_gpu_l;
            tmp_gpu_l   = tmp_gpu_l->next;
            free(tmp_gpu_l_2);
        }
        /* Get next element and Free current package list element */
        tmp_pkg_l_2 = tmp_pkg_l;
        tmp_pkg_l = tmp_pkg_l->next;
        free(tmp_pkg_l_2);
    }
}

void mpibind_compute_thread_number_per_package(hwloc_topology_t topology, hwloc_pkg_l **pkg_l, int nthread){
    int i, ii, tmp_int;
    hwloc_pkg_l *tmp_pkg_l;

    #ifdef __DEBUG
    fprintf(stderr, "\n*** Getting the number of workers\n");
    #endif /* __DEBUG */
    tmp_pkg_l = *pkg_l;          /* Used to traverse the package list */
    while(tmp_pkg_l != NULL){
        if (nthread == -1){      /* If application is not threaded */
            tmp_pkg_l->nb_worker = tmp_pkg_l->nb_process;
            for(i=0; i<tmp_pkg_l->nb_process; i++){
                tmp_pkg_l->process[i].nb_thread = 1;
            }
        }
        else if(nthread > 0){    /* Else if nb thread id defined */
            tmp_pkg_l->nb_worker = nthread * tmp_pkg_l->nb_process;
            for(i=0; i<tmp_pkg_l->nb_process; i++){
                tmp_pkg_l->process[i].nb_thread = nthread;
            }
        }
        else if(nthread == 0){
            char *tmp_char;
            /* If nthread not defined (=0) look at OMP_NUM_THREADS */
            tmp_char = getenv("OMP_NUM_THREADS");
            if(tmp_char){
                nthread = atoi(tmp_char);
                /* Check if env variable is actually an integer */
                /* if(isdigit(nthread) != 0) */ /* TODO: Not working ?! */
                tmp_pkg_l->nb_worker = nthread * tmp_pkg_l->nb_process;
                for(i=0; i<tmp_pkg_l->nb_process; i++){
                    tmp_pkg_l->process[i].nb_thread = nthread;
                }
            }
            /* If nthread still =0 -> use all cores, 1 pu per core -> nworker = nb_core */
            if (nthread == 0){ 
                tmp_pkg_l->nb_worker = tmp_pkg_l->nb_core;
                tmp_int = tmp_pkg_l->nb_core;    /* Keep track of cores left to assign */
                for(i=0; i<tmp_pkg_l->nb_process; i++){
                    tmp_pkg_l->process[i].nb_thread = (int) (tmp_int / tmp_pkg_l->nb_process - i);
                    tmp_int = tmp_int - tmp_pkg_l->process[i].nb_thread;
                    if(tmp_int % (tmp_pkg_l->nb_process - i) > 0){
                        tmp_pkg_l->process[i].nb_thread++;
                        tmp_int--;
                    }
                }
            }
        }
        #ifdef __DEBUG
        fprintf(stderr, "\tPackage%d Workers: %d\n", tmp_pkg_l->pkg->os_index, tmp_pkg_l->nb_worker);
        for(ii=0; ii<tmp_pkg_l->nb_process; ii++){
            fprintf(stderr, "\t\tProcess%d Threads: %d\n", tmp_pkg_l->process[ii].local_id, tmp_pkg_l->process[ii].nb_thread);
        }
        #endif /* __DEBUG */
        /* next package */
        tmp_pkg_l = tmp_pkg_l->next;
    }
}

void mpibind_mappind_depth_per_package(hwloc_topology_t topology, hwloc_pkg_l **pkg_l){
    int i, tmp_int;
    hwloc_obj_t hwloc_obj;
    hwloc_pkg_l *tmp_pkg_l;

    #ifdef __DEBUG
    fprintf(stderr, "\n*** Highest topology object to map to:\n");
    #endif /* __DEBUG */
    tmp_pkg_l = *pkg_l;          /* Used to traverse the package list */
    while(tmp_pkg_l != NULL){
        if(tmp_pkg_l->nb_process == 0) break;
        if(tmp_pkg_l->nb_worker == 1){
            tmp_pkg_l->mapping_depth = hwloc_get_type_depth(topology, HWLOC_OBJ_PACKAGE);
        }
        else{
            tmp_int   = hwloc_get_type_depth(topology, HWLOC_OBJ_PACKAGE);  /* Used to keep track of depth */
            while(tmp_int < hwloc_topology_get_depth(topology)){
                int nb_children; nb_children = 0;
                hwloc_obj = NULL;
                /* Get all objects at depth */
                for(i=0; i<hwloc_get_nbobjs_by_depth(topology, tmp_int); i++){
                    hwloc_obj = hwloc_get_next_obj_by_depth(topology, tmp_int, hwloc_obj);
                    /* Check if current package is parent of this node */
                    if(hwloc_obj_is_in_subtree(topology, hwloc_obj, tmp_pkg_l->pkg)) nb_children++;
                }
                /* Enough nodes at depth for workers ? */
                if(nb_children >= tmp_pkg_l->nb_worker){
                    tmp_pkg_l->mapping_depth = tmp_int;
                    #ifdef __DEBUG
                    char *string; string = malloc(256*sizeof(char));
                    hwloc_obj = hwloc_get_next_obj_by_depth(topology, tmp_pkg_l->mapping_depth, NULL);
                    hwloc_obj_type_snprintf(string, 256, hwloc_obj, 0);
                    fprintf(stderr, "\tPackage%d: nb_process:%d nb_worker:%d depths: %d - %s\n", 
                            tmp_pkg_l->pkg->os_index, tmp_pkg_l->nb_process, tmp_pkg_l->nb_worker, 
                            tmp_pkg_l->mapping_depth, string);
                    free(string);
                    #endif /* __DEBUG */
                    break;
                }
                tmp_int++;
            }
        }
        /* next package */
        tmp_pkg_l = tmp_pkg_l->next;
    }
}

void mpibind_create_cpuset(hwloc_topology_t topology, hwloc_pkg_l **pkg_l){
    int i, ii, tst;
    hwloc_pkg_l *tmp_pkg_l;
    hwloc_obj_t hwloc_obj;
    hwloc_cpuset_l *tmp_cpuset_l;

    #ifdef __DEBUG
    fprintf(stderr, "\n*** Creating cpusets:\n");
    #endif /* __DEBUG */
    tmp_pkg_l = *pkg_l;         /* Used to traverse the package list */
    while(tmp_pkg_l != NULL){
        hwloc_obj = NULL;       /* Used to traverse the objects at mapping depth */
        tmp_cpuset_l = tmp_pkg_l->cpuset_l;
        /* Find the first object at mapping depths from the current package */
        tst = 0;
        while(tst == 0){
            /* Check if current package is parent of this node */
            hwloc_obj = hwloc_get_next_obj_by_depth(topology, tmp_pkg_l->mapping_depth, hwloc_obj);
            if(hwloc_obj_is_in_subtree(topology, hwloc_obj, tmp_pkg_l->pkg)) tst = 1;
        }
        /* Note that hwloc_obj is now the first object to add to the first cpuset */
        /* Create a new cpuset for each process in the package */
        for(i=0; i<tmp_pkg_l->nb_process; i++){
            /* Create new cpuset list element */
            hwloc_cpuset_l *cpuset_elem; cpuset_elem = NULL;
            cpuset_elem = malloc(sizeof(hwloc_cpuset_l));
            /* Fill element */
            cpuset_elem->next = NULL;
            cpuset_elem->cpuset = hwloc_bitmap_alloc();
            hwloc_bitmap_zero(cpuset_elem->cpuset);
            /* Fill the cpusets*/
            for(ii = 0; ii<tmp_pkg_l->process[i].nb_thread ; ii++){
                hwloc_bitmap_singlify(hwloc_obj->cpuset);
                /* Add the obj's cpuset to the process' cpuset */
                hwloc_bitmap_or(cpuset_elem->cpuset, cpuset_elem->cpuset, hwloc_obj->cpuset);
                /* Get the new object of next iteration or next process */
                hwloc_obj = hwloc_get_next_obj_by_depth(topology, tmp_pkg_l->mapping_depth, hwloc_obj);
            }
            /* Add element to the package cpuset list */
            if(tmp_pkg_l->cpuset_l == NULL){
                tmp_pkg_l->cpuset_l = cpuset_elem;
                tmp_cpuset_l = cpuset_elem;
            }else{
                tmp_cpuset_l->next = cpuset_elem;
                tmp_cpuset_l = tmp_cpuset_l->next;
            }
            #ifdef __DEBUG
            char *string;
            hwloc_bitmap_list_asprintf(&string, cpuset_elem->cpuset);
            fprintf(stderr, "\tPackage%d: Process%d: cpuset:%s\n", tmp_pkg_l->pkg->os_index, i, string);
            free(string);
            #endif /* __DEBUG */
        }
        tmp_cpuset_l = tmp_pkg_l->cpuset_l;
        while(tmp_cpuset_l != NULL){
            tmp_cpuset_l = tmp_cpuset_l->next;
        } 
        /* next package */
        tmp_pkg_l = tmp_pkg_l->next;
    }
}

void mpibind_gpu_list_init(hwloc_topology_t topology, hwloc_gpu_l **gpu_l){
    hwloc_gpu_l *tmp_gpu_l;
    hwloc_obj_t hwloc_obj;

    tmp_gpu_l = *gpu_l;   /* Used to keep track of the end of the gpu list */
    hwloc_obj = NULL;
    /* Go through every os device and find all gpus */
    while ((hwloc_obj = hwloc_get_next_osdev(topology, hwloc_obj)) != NULL){
        /* check if object is a GPU device */
        if(hwloc_obj->attr->osdev.type == HWLOC_OBJ_OSDEV_GPU){
            /* Create new gpu_l element and appen to the list */
            hwloc_gpu_l *new; new = NULL;
            new = malloc(sizeof(hwloc_gpu_l));
            new->next = NULL;
            new->gpu = hwloc_obj;
            /* Append to the list (easy because we keep track of the last element in the list */
            if(tmp_gpu_l == NULL){
                *gpu_l = new;
                tmp_gpu_l = new;
            }
            else{
                tmp_gpu_l->next = new;
                tmp_gpu_l = tmp_gpu_l->next;
            }
        }
        /* next object */
        hwloc_obj = hwloc_get_next_osdev(topology, hwloc_obj);
    }
    #ifdef __DEBUG
    fprintf(stderr, "\n*** GPU list creation:\n\t");
    tmp_gpu_l = *gpu_l;
    while(tmp_gpu_l != NULL){
        fprintf(stderr, "%s ", tmp_gpu_l->gpu->name);
        tmp_gpu_l = tmp_gpu_l->next;
    }
    fprintf(stderr, "\n");
    #endif /* __DEBUG */
}

void mpibind_gpu_list_destroy(hwloc_gpu_l **gpu_l){
    hwloc_gpu_l *tmp_gpu_l, *tmp_gpu_l_2;
    tmp_gpu_l = *gpu_l;
    while(tmp_gpu_l != NULL){
        tmp_gpu_l_2 = tmp_gpu_l;
        tmp_gpu_l   = tmp_gpu_l->next;
        free(tmp_gpu_l_2);
    }
}

void mpibind_assign_gpu(hwloc_topology_t topology, hwloc_pkg_l **pkg_l, hwloc_gpu_l **gpu_l){
    hwloc_pkg_l *tmp_pkg_l;
    hwloc_gpu_l *tmp_gpu_l, *tmp_gpu_l_2;
    hwloc_obj_t hwloc_obj;

    #ifdef __DEBUG
    fprintf(stderr, "\n*** GPU Assign:\n");
    #endif /* __DEBUG */
    hwloc_obj = NULL;

    /* Go through all packages and assign the right gpu (common ancestor is of type group) */
    tmp_pkg_l = *pkg_l;        /* Used to traverse the package list */
    while(tmp_pkg_l != NULL){
        /* Check each gpu */
        tmp_gpu_l = *gpu_l;
        tmp_gpu_l_2 = tmp_pkg_l->gpu_l;   /* Used to keep track of the end of the gpu list */
        while(tmp_gpu_l != NULL){
            /* Not working */
            //hwloc_obj = hwloc_get_common_ancestor_obj(topology, tmp_pkg_l->pkg, tmp_gpu_l->gpu);
            //if(hwloc_obj->type == HWLOC_OBJ_GROUP){
            /* Instead: find the first 'group obj' ancestor */
            /* Check if package is in it's subtree */
            hwloc_obj = tmp_gpu_l->gpu;
            while(hwloc_obj->type != HWLOC_OBJ_GROUP) hwloc_obj = hwloc_obj->parent;
            if(hwloc_obj_is_in_subtree(topology, tmp_pkg_l->pkg, hwloc_obj)){
                /* Create new gpu_l element for the package list */
                hwloc_gpu_l *new; new = NULL;
                new = malloc(sizeof(hwloc_gpu_l));
                new->next = NULL;
                new->gpu = tmp_gpu_l->gpu;
                /* Append to the list (easy because we keep track of the last element in the list */
                if(tmp_gpu_l_2 == NULL){
                    tmp_pkg_l->gpu_l = new;
                    tmp_gpu_l_2 = tmp_pkg_l->gpu_l;
                }
                else{
                    tmp_gpu_l_2->next = new;
                    tmp_gpu_l_2 = tmp_gpu_l_2->next;
                }
            }
            /* Next gpu */
            tmp_gpu_l = tmp_gpu_l->next;
        }
        /* If this package's gpu list is still empty at the end: assign all gpus */
        if(tmp_pkg_l->gpu_l == NULL) {
            tmp_pkg_l->gpu_l = *gpu_l;
        }
        #ifdef __DEBUG
        fprintf(stderr, "\tPackage%d full GPU list:", tmp_pkg_l->pkg->os_index);
        tmp_gpu_l_2 = tmp_pkg_l->gpu_l;   /* Used to keep track of the end of the gpu list */
        while(tmp_gpu_l_2 != NULL){
            fprintf(stderr, " %s", tmp_gpu_l_2->gpu->name);
            tmp_gpu_l_2 = tmp_gpu_l_2->next;
        }
        fprintf(stderr, "\n");
        #endif /* __DEBUG */
        /* next package */
        tmp_pkg_l = tmp_pkg_l->next;
    }
    #ifdef __DEBUG
    fprintf(stderr, "\n");
    #endif /* __DEBUG */
}


/****                         PRINT FUNCTIONS                            ****/
void mpibind_dryrun_print(hwloc_pkg_l *head){
    int ii;
    char *string;
    hwloc_cpuset_l *cpuset_l;
    hwloc_gpu_l *gpu_l;

    fprintf(stderr, "#### Mpibind binding: ####\n\n");
    while(head != NULL){
        /* Basic infos */
        fprintf(stderr, "## Package%d  nb_process:%d, nb_core:%d, nb_pu:%d, mapping_depth:%d\n",
                head->pkg->os_index, head->nb_process, head->nb_core, head->nb_pu, head->mapping_depth);
        /* Process specific infos */
        cpuset_l = head->cpuset_l;      /* Keep track of process's cpusets */
        gpu_l = head->gpu_l;            /* Keep track of process's gpu(s) */
        for(ii=0; ii<head->nb_process; ii++){
            /* Process infos */
            fprintf(stderr, "\tProcess%d Threads: %d\n", head->process[ii].local_id, head->process[ii].nb_thread);
            /* cpuset */
            hwloc_bitmap_list_asprintf(&string, cpuset_l->cpuset);
            fprintf(stderr, "\t\tcpuset_l: %s\n", string);
            cpuset_l = cpuset_l->next;
            free(string);
            /* GPU(s) */
            fprintf(stderr, "\t\tgpu_l:");
            while(gpu_l != NULL){
                fprintf(stderr, " %s", gpu_l->gpu->name);
                gpu_l = gpu_l->next;
            }
            fprintf(stderr, "\n");
        }
        /* Next package */
        head = head->next;
    }
}
