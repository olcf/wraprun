/*
The MIT License (MIT)

Copyright (c) 2015 Adam Simpson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include "print_macros.h"

// Format of arguments: #num_proc entries #num_proc_per_node1 #numprocs_per_node2 :: a.out arg1 arg2 :: b.out arg1 arg2 ... ::
// e.g: run four instances of ./a.out arg1 and three instances of ./b.out arg1 arg2: 2 4 3 :: ./a.out arg1 :: ./b.out arg1 arg2 ::

static int compare(const void *a, const void*b){
  return ( *(int*)a - *(int*)b );
}

const int MAX_INSTANCES=16; // Maximum number of app instances per node

int main(int argc, char **argv, char** envp) {
  // Get number of entries to parse
  const int instance_entries = atoi(argv[1]);

  // Get process count per application per node
  int instance_counts[MAX_INSTANCES];
  int instance_count = 0; // Total instances per node
  for(int i=0; i<instance_entries; i++) {
    int arg_num = 2 + i;
    const int count_i = atoi(argv[arg_num]);
    instance_counts[i] = count_i;
    instance_count += count_i;
  }

  // Wait for all pids to be visible
  int num_pids = 0;
  pid_t pids[MAX_INSTANCES];
  while(num_pids < instance_count) {
    const int max_chars = MAX_INSTANCES*10;
    char line[max_chars];
    FILE *cmd = popen("pidof intra.out", "r");
    fgets(line, max_chars, cmd);

    const int base = 10;
    char *start = line;
    char *end;
    num_pids = 0;
    while(1) {
      pid_t pid = strtoul(start, &end, base);
      if( end == start)
        break;

      pids[num_pids] = pid;
      start = end;
      num_pids++;
    }
    pclose(cmd);
    sleep(1);
  }

  pid_t my_pid = getpid();

  // Sort PIDs
  qsort(pids, num_pids, sizeof(pid_t), compare);

  // Find location in pids array of this rank
  int my_pid_index = -1;
  for(int i=0; i<num_pids; i++) {
    if(pids[i] == my_pid) {
      my_pid_index = i;
      break;
    }
  }

  // Determine which application this rank is responsible for launching
  int instances=0;
  int my_app_num = 0;
  for(int i=0; i<instance_entries; i++) {
    instances += instance_counts[i];
    if(my_pid_index < instances)
      break;

    my_app_num++;
  }

  DEBUG_PRINT("app num: %d\n", my_app_num);

  // Get start and end index in argv of this ranks app and arguments
  int start = 1 + instance_entries + 1 + 1;
  int app_num = -1; // -1 so first app discovered is app 0
  int app_argv_start = -1; // index in argv that this ranks app begins
  int app_argv_end = -1;   // 1 past the index in argv that this ranks app arguments end 
  for(int i=start; i<argc; i++) {
    // If previous token arg was "::" the current token is the application
    if(strcmp(argv[i-1], "::") == 0) {
      app_num++;
      printf("app index: %d\n", i);
    }

    if(app_num == my_app_num) {
      app_argv_start = i;
      // Search for end of the found apps arguments
      int j;
      for(j=i+1; j<argc; j++) {
        if(strcmp(argv[j],"::") == 0){
          break;
        }
      }
      app_argv_end = j;
      break;
    }
  }

  int child_status;
  pid_t child_pid = fork();

  if(child_pid == 0) {
    // Preload libsplit
    setenv("LD_PRELOAD", getenv("WRAPRUN_PRELOAD"), 1);

    // Set app id environment variable
    char *app_num_arg = malloc(MAX_INSTANCES+1);
    sprintf(app_num_arg, "%d", my_app_num);
    setenv("W_RANK_FROM_ENV", app_num_arg , 1);

    // Set new end for this instances arguments
    argv[app_argv_end] = NULL;

    // execv with argv pointing to the begining of this ranks app
    int err = execv(argv[app_argv_start], argv + app_argv_start);
    if(err < 0) {
      printf("Failed execv with err=%d!\n", errno);
      exit(1);
    }
  }
  else {
      waitpid(child_pid, &child_status, 0);
  }

  return child_status;
}
