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

/*
  libsplit is a library designed to split MPI_COMM_WORLD into multiple smaller
  communicators. Upon calling MPI_Init() the file pointed to by the WRAPRUN_FILE
  environment variable is opened and the contents read. This file content is parsed
  to determine the ranks particular color as well as the desired working directory
  and process specific environment variables.

  The color is used to create the MPI_COMM_SPLIT communicator. All MPI* functions
  containing a communicator are provided and anytime MPI_COMM_WORLD is passed
  in it will be switched out for MPI_COMM_SPLIT before calling PMPI*
*/

#define _GNU_SOURCE // RTLD_NEXT, must define this before ANY standard header
#include <dlfcn.h>  // dlsym()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "print_macros.h"
#include "mpi.h"

static MPI_Comm MPI_COMM_SPLIT = MPI_COMM_NULL;

// Reads in rank line of WRAPRUN_FILE
// space seperated values are parsed to set color, work_dir, and env_vars
static void GetRankParamsFromFile(const int rank, int *color, char *work_dir,
                                  char *env_vars) {
  // Get file name from environment variable
  const char *const file_name = getenv("WRAPRUN_FILE");
  if(!file_name)
    EXIT_PRINT("%s environment variable not set, exiting!\n", "WRAPRUN_FILE");

  // Search file and read in rank'th line of file
  FILE *const file = fopen(file_name, "r");
  if(!file)
    EXIT_PRINT("Can't open %s\n", file_name);

  char *line = NULL;

  int line_num;
  for(line_num=0; line_num<=rank; ++line_num) {
    size_t length = 0;
    const ssize_t char_count = getline(&line, &length, file);
    if(char_count == -1)
      EXIT_PRINT("Error reading rank %d info from %s\n", rank, file_name);
  }

  // Extract parameters
  const int num_params = sscanf(line, "%d %s %s", color, work_dir, env_vars);
  if(num_params == EOF)
    EXIT_PRINT("Error parsing file line\n");

  free(line);
  fclose(file);
}

static void SetSplitCommunicator(const int color) {
  const int err = PMPI_Comm_split(MPI_COMM_WORLD, color, 0, &MPI_COMM_SPLIT);
  if(err != MPI_SUCCESS)
    EXIT_PRINT("Failed to split communicator: %d!\n", err);
}

static void SetWorkingDirectory(const char *const work_dir) {
  const int err = chdir(work_dir);
  if(err)
    EXIT_PRINT("Failed to change working directory: %s!\n", strerror(errno));
}

// Set environment variables in env_vars string
// with format "key1=value2;key2=value2"
static void SetEnvironmentVaribles(char *env_vars) {
  char *token;

  // environment variables are optional
  if(strlen(env_vars) == 0)
    return;

  while ((token = strsep(&env_vars, ";")) != NULL) {
    char key[1024];
    char value[1024];
    const int num_components = sscanf(token, "%s=%s", key, value);
    if(num_components == 2) {
      const int err = setenv(key, value, 1);
      if(err)
        EXIT_PRINT("Error setting environment variable: %s\n", strerror(errno));
    }
    else
      EXIT_PRINT("Error parsing environment_variables\n");
  }
}

// Redirect stdout and stderr to file based upon color
static void SetStdOutErr(const int color) {
  const char *const job_id = getenv("PBS_JOBID");
  char file_name[1024];

  sprintf(file_name, "%s_w_%d.out", job_id, color);
  const FILE *const out_handle = freopen(file_name, "a", stdout);
  if(!out_handle)
    EXIT_PRINT("Error setting stdout!\n");

  sprintf(file_name, "%s_w_%d.err", job_id, color);
  const FILE *const err_handle = freopen(file_name, "a", stderr);
  if(!err_handle)
    EXIT_PRINT("Error setting stderr\n");
}

static void CloseStdOutErr() {
  fclose(stdout);
  fclose(stderr);
}

// Wait for all other wraprun processes to complete before exiting
// Calling MPI_Finalize and fprintf is undefined
// Cleanup operations have been problematic so are skipped
static void SegvHandler(int sig) {
  fprintf(stderr, "*********\n ERROR: Signal SEGV Received\n*********\n");

  if(getenv("W_SIG_DFL"))
    signal(SIGSEGV, SIG_DFL);

  // Try to clean up
  MPI_Finalize();
  _exit(EXIT_SUCCESS);
}

// Handle SIGABRT, to handle a call to abort() for instance
static void AbrtHandler(int sig) {
  fprintf(stderr, "*********\n ERROR: Signal SIGABRT Received\n*********\n");

  if(getenv("W_SIG_DFL")) 
    signal(SIGSEGV, SIG_DFL);

  // Try to clean up
  MPI_Finalize();
  exit(EXIT_SUCCESS);
}

static void SegvHandlerPause(int sig) {
  fprintf(stderr, "*********\n ERROR: Signal SEGV Received\n*********\n");
  pause();
}

static void AbrtHandlerPause(int sig) {
  fprintf(stderr, "*********\n ERROR: Signal Abrt Received\n*********\n");
  pause();
}

// For an exit code of 0, any process with non 0 exit will abort entire wraprun
// Works for exit() or return()
static void ExitHandler() {
  int finalized = 0;
  MPI_Finalized(&finalized);
  if(!finalized)
    MPI_Finalize();

  _exit(EXIT_SUCCESS);
}

static void SplitInit() {
  // Cray has issues when LD_PRELOAD is set
  // and exec*() is called...this is a workaround
  if (getenv("W_UNSET_PRELOAD"))
    unsetenv("LD_PRELOAD");

  int rank;
  PMPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int color;
  char *const work_dir = calloc(2048, sizeof(char));
  if(!work_dir)
    EXIT_PRINT("Error allocating work_dir memory!\n");
  char *const env_vars = calloc(4096, sizeof(char));
  if(!env_vars)
    EXIT_PRINT("Error allocating env_vars memory!\n");
  env_vars[0] = '\0'; // "zero" out env_vars

  if(getenv("W_RANK_FROM_ENV")) {
    int env_rank = atoi(getenv("W_ENV_RANK"));
    GetRankParamsFromFile(env_rank, &color, work_dir, env_vars);
  }
  else
    GetRankParamsFromFile(rank, &color, work_dir, env_vars);

  if (getenv("W_IGNORE_SEGV")) {
    sighandler_t err_sig;

    if(getenv("W_SIG_PAUSE"))
      err_sig = signal(SIGSEGV, SegvHandlerPause);
    else
      err_sig = signal(SIGSEGV, SegvHandler);

    if(err_sig == SIG_ERR)
      fprintf(stderr, "ERROR REGISTERING SIGSEGV HANDLER!\n");
  }

  if (getenv("W_IGNORE_ABRT")) {
    sighandler_t err_abrt;

    if(getenv("W_SIG_PAUSE"))
      err_abrt = signal(SIGABRT, AbrtHandlerPause);
    else
      err_abrt = signal(SIGABRT, AbrtHandlerPause);

    if(err_abrt == SIG_ERR)
      fprintf(stderr, "ERROR REGISTERING SIGARBT HANDLER!\n");
  }

  if (getenv("W_IGNORE_RETURN_CODE")) {
    int err_code = atexit(ExitHandler);
    if(err_code != 0)
      fprintf(stderr, "ERROR REGISTERING ATEXIT HANDLER!\n");
  }

  SetSplitCommunicator(color);

  SetWorkingDirectory(work_dir);

  if (getenv("W_REDIRECT_OUTERR"))
    SetStdOutErr(color);

  SetEnvironmentVaribles(env_vars);

  free(work_dir);
  free(env_vars);
}

int MPI_Init(int *argc, char ***argv) {
  // Allow MPI_Init to be called directly
  int return_value;
  if (getenv("W_UNWRAP_INIT")) {
    DEBUG_PRINT("Unwrapped!\n");
    int (*real_MPI_Init)(int*, char***) = dlsym(RTLD_NEXT, "MPI_Init");
    return_value = (*real_MPI_Init)(argc, argv);
  }
  else {
    DEBUG_PRINT("Wrapped!\n");
    return_value = PMPI_Init(argc, argv);
  }

  SplitInit();
  return return_value;
}

int MPI_Init_thread(int *argc, char ***argv, int required, int *provided) {
  // Allow MPI_Init_thread to be called directly
  int return_value;
  if (getenv("W_UNWRAP_INIT")) {
    DEBUG_PRINT("Unwrapped!\n");
    int (*real_MPI_Init_thread)(int*, char***, int, int*) = dlsym(RTLD_NEXT, "MPI_Init_thread");
    return_value = (*real_MPI_Init_thread)(argc, argv, required, provided);
  }
  else {
    DEBUG_PRINT("Wrapped!\n");
    return_value = PMPI_Init_thread(argc, argv, required, provided);
  }

  SplitInit();
  return return_value;
}

int MPI_Finalize() {
  if(MPI_COMM_SPLIT != MPI_COMM_NULL) {
    const int err = PMPI_Comm_free(&MPI_COMM_SPLIT);
    if(err != MPI_SUCCESS)
      EXIT_PRINT("Failed to free split communicator: %d !\n", err);
  }

  int return_value = 0;
  int finalized = 0;
  MPI_Finalized(&finalized);
  if(!finalized) {
    // Allow MPI_Finalize to be called directly
    if (getenv("W_UNWRAP_FINALIZE")) {
      DEBUG_PRINT("Unwrapped!\n");
      int (*real_MPI_Finalize)() = dlsym(RTLD_NEXT, "MPI_Finalize");
      return_value = (*real_MPI_Finalize)();
    }
    else {
      DEBUG_PRINT("Wrapped!\n");
      return_value = PMPI_Finalize();
    }
  }

  if (getenv("W_REDIRECT_OUTERR"))
    CloseStdOutErr();

  return return_value;
}

// If input_comm == MPI_COMM_WORLD return MPI_COMM_SPLIT else input_comm
// MPI standard guarantees opaque types comparable and assignable
static MPI_Comm GetCorrectComm(const MPI_Comm input_comm) {
  MPI_Comm correct_comm;
  if(input_comm == MPI_COMM_WORLD)
    correct_comm = MPI_COMM_SPLIT;
  else
    correct_comm = input_comm;

  return correct_comm;
}

///////////////////////////////////////////////////////////////////////////////
///// Simple MPI wrapper functions
//////////////////////////////////////////////////////////////////////////////
int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Send(buf, count, datatype, dest, tag, correct_comm);
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Recv(buf, count, datatype, source, tag, correct_comm, status);
}

int MPI_Bsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Bsend(buf, count, datatype, dest, tag, correct_comm);
}

int MPI_Ssend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ssend(buf, count, datatype, dest, tag, correct_comm);
}

int MPI_Rsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Rsend(buf, count, datatype, dest, tag, correct_comm);
}

int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Isend(buf, count, datatype, dest, tag, correct_comm, request);
}

int MPI_Ibsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ibsend(buf, count, datatype, dest, tag, correct_comm, request);
}

int MPI_Issend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Issend(buf, count, datatype, dest, tag, correct_comm, request);
}

int MPI_Irsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Irsend(buf, count, datatype, dest, tag, correct_comm, request);
}

int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source,
              int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Irecv(buf, count, datatype, source, tag, correct_comm, request);
}

int MPI_Iprobe(int source, int tag, MPI_Comm comm, int *flag, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Iprobe(source, tag, correct_comm, flag, status);
}

int MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Probe(source, tag, correct_comm, status);
}

int MPI_Send_init(const void *buf, int count, MPI_Datatype datatype, int dest,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Send_init(buf, count, datatype, dest, tag, correct_comm, request);
}

int MPI_Bsend_init(const void *buf, int count, MPI_Datatype datatype, int dest,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Bsend_init(buf, count, datatype, dest, tag, correct_comm, request);
}

int MPI_Ssend_init(const void *buf, int count, MPI_Datatype datatype, int dest,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ssend_init(buf, count, datatype, dest, tag, correct_comm, request);
}

int MPI_Rsend_init(const void *buf, int count, MPI_Datatype datatype, int dest,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Rsend_init(buf, count, datatype, dest, tag, correct_comm, request);
}

int MPI_Recv_init(void *buf, int count, MPI_Datatype datatype, int source,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Recv_init(buf, count, datatype, source, tag, correct_comm, request);
}

int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                int dest, int sendtag,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int source, int recvtag,
                MPI_Comm comm, MPI_Status *status) {

  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Sendrecv(sendbuf, sendcount, sendtype, dest, sendtag,
                       recvbuf, recvcount, recvtype, source, recvtag,
                       correct_comm, status);
}

int MPI_Sendrecv_replace(void *buf, int count, MPI_Datatype datatype,
                       int dest, int sendtag, int source, int recvtag,
                       MPI_Comm comm, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Sendrecv_replace(buf, count, datatype, dest, sendtag, source,
                               recvtag, correct_comm, status);
}

int MPI_Pack(const void *inbuf,
             int incount,
             MPI_Datatype datatype,
             void *outbuf,
             int outsize,
             int *position,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Pack(inbuf, incount, datatype,
                   outbuf, outsize, position, correct_comm);

}

int MPI_Unpack(const void *inbuf, int insize, int *position,
               void *outbuf, int outcount, MPI_Datatype datatype,
               MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Unpack(inbuf, insize, position, outbuf, outcount,
                     datatype, correct_comm);
}

int MPI_Pack_size(int incount, MPI_Datatype datatype, MPI_Comm comm, int *size) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Pack_size(incount, datatype, correct_comm, size);
}

int MPI_Barrier(MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Barrier(correct_comm);
}

int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Bcast(buffer, count, datatype, root, correct_comm);
}

int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
    void *recvbuf, int recvcount, MPI_Datatype recvtype, int root,
    MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Gather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                     root, correct_comm);
}

int MPI_Gatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int *recvcounts, const int *displs,
                MPI_Datatype recvtype, int root, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Gatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts,
                      displs, recvtype, root, correct_comm);
}

int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype, int root,
               MPI_Comm comm) {

  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Scatter(sendbuf, sendcount, sendtype, recvbuf, recvcount,
                      recvtype, root, correct_comm);

}

int MPI_Scatterv(const void *sendbuf, const int *sendcounts, const int *displs,
                 MPI_Datatype sendtype, void *recvbuf, int recvcount,
                 MPI_Datatype recvtype,
                 int root, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Scatterv(sendbuf, sendcounts, displs, sendtype, recvbuf, recvcount,
                      recvtype, root, correct_comm);
}

int MPI_Allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                        correct_comm);
}

int MPI_Allgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                   void *recvbuf, const int *recvcounts, const int *displs,
                   MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs,
                        recvtype, correct_comm);
}

int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount,
                       recvtype, correct_comm);
}

int MPI_Alltoallv(const void *sendbuf, const int *sendcounts,
                  const int *sdispls, MPI_Datatype sendtype, void *recvbuf,
                  const int *recvcounts, const int *rdispls, MPI_Datatype recvtype,
                  MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Alltoallv(sendbuf, sendcounts,
                        sdispls, sendtype, recvbuf,
                        recvcounts, rdispls, recvtype,
                        correct_comm);
}

int MPI_Alltoallw(const void *sendbuf, const int sendcounts[],
                  const int sdispls[], const MPI_Datatype sendtypes[],
                  void *recvbuf, const int recvcounts[], const int rdispls[],
                  const MPI_Datatype recvtypes[], MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Alltoallw(sendbuf, sendcounts,
                        sdispls, sendtypes,
                        recvbuf, recvcounts, rdispls,
                        recvtypes, correct_comm);
}

int MPI_Exscan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Exscan(sendbuf, recvbuf, count, datatype, op, correct_comm);
}

int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, int root, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Reduce(sendbuf, recvbuf, count, datatype,
                    op, root, correct_comm);
}

int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count,
                  MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Allreduce(sendbuf, recvbuf, count,
                        datatype, op, correct_comm);
}

int MPI_Reduce_scatter(const void *sendbuf, void *recvbuf, const int recvcounts[],
                      MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Reduce_scatter(sendbuf, recvbuf, recvcounts,
                             datatype, op, correct_comm);
}

int MPI_Scan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
             MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Scan(sendbuf, recvbuf, count, datatype,
                   op, correct_comm);
}

int MPI_Comm_group(MPI_Comm comm, MPI_Group *group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_group(correct_comm, group);
}

int MPI_Comm_size(MPI_Comm comm, int *size) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_size(correct_comm, size);
}

int MPI_Comm_rank(MPI_Comm comm, int *rank) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_rank(correct_comm, rank);
}

int MPI_Comm_compare(MPI_Comm comm1, MPI_Comm comm2, int *result) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm1 = GetCorrectComm(comm1);

  MPI_Comm correct_comm2 = GetCorrectComm(comm2);

  return PMPI_Comm_compare(correct_comm1, correct_comm2, result);
}

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_dup(correct_comm, newcomm);
}

int MPI_Comm_dup_with_info(MPI_Comm comm, MPI_Info info, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_dup_with_info(correct_comm, info, newcomm);
}

int MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_create(correct_comm, group, newcomm);
}

int MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_split(correct_comm, color, key, newcomm);
}

// Not needed, shouldn't ever free MPI_COMM_WORLD
int MPI_Comm_free(MPI_Comm *comm) {
  DEBUG_PRINT("Wrapped!\n");

  return PMPI_Comm_free(comm);
}

int MPI_Comm_test_inter(MPI_Comm comm, int *flag) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_test_inter(correct_comm, flag);
}

int MPI_Comm_remote_size(MPI_Comm comm, int *size) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_remote_size(correct_comm, size);
}

int MPI_Comm_remote_group(MPI_Comm comm, MPI_Group *group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_remote_group(correct_comm, group);
}

int MPI_Intercomm_create(MPI_Comm local_comm, int local_leader,
                       MPI_Comm peer_comm, int remote_leader, int tag,
                       MPI_Comm *newintercomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_local_comm = GetCorrectComm(local_comm);
  MPI_Comm correct_peer_comm = GetCorrectComm(peer_comm);

  return PMPI_Intercomm_create(correct_local_comm, local_leader,
                               correct_peer_comm, remote_leader, tag,
                               newintercomm);
}

int MPI_Intercomm_merge(MPI_Comm intercomm, int high, MPI_Comm *newintracomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_intercomm = GetCorrectComm(intercomm);

  return PMPI_Intercomm_merge(correct_intercomm, high, newintracomm);
}

int MPI_Attr_put(MPI_Comm comm, int keyval, void *attribute_val) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Attr_put(correct_comm, keyval, attribute_val);
}

int MPI_Attr_get(MPI_Comm comm, int keyval, void *attribute_val, int *flag) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Attr_get(correct_comm, keyval, attribute_val, flag);
}

int MPI_Attr_delete(MPI_Comm comm, int keyval) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Attr_delete(correct_comm, keyval);
}

int MPI_Topo_test(MPI_Comm comm, int *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Topo_test(correct_comm, status);
}

int MPI_Cart_create(MPI_Comm comm_old, int ndims, const int dims[],
                    const int periods[], int reorder, MPI_Comm *comm_cart) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm_old = GetCorrectComm(comm_old);

  return PMPI_Cart_create(correct_comm_old, ndims, dims, periods, reorder, comm_cart);
}
int MPI_Graph_create(MPI_Comm comm_old, int nnodes, const int indx[],
                     const int edges[], int reorder, MPI_Comm *comm_graph) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm_old = GetCorrectComm(comm_old);

  return PMPI_Graph_create(correct_comm_old, nnodes, indx, edges, reorder, comm_graph);
}

int MPI_Graphdims_get(MPI_Comm comm, int *nnodes, int *nedges) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Graphdims_get(correct_comm, nnodes, nedges);
}

int MPI_Graph_get(MPI_Comm comm, int maxindex, int maxedges, int indx[], int edges[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Graph_get(correct_comm, maxindex, maxedges, indx, edges);
}

int MPI_Cartdim_get(MPI_Comm comm, int *ndims) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Cartdim_get(correct_comm, ndims);
}

int MPI_Cart_get(MPI_Comm comm, int maxdims, int dims[], int periods[], int coords[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Cart_get(correct_comm, maxdims, dims, periods, coords);
}

int MPI_Cart_rank(MPI_Comm comm, const int coords[], int *rank) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Cart_rank(correct_comm, coords, rank);
}

int MPI_Cart_coords(MPI_Comm comm, int rank, int maxdims, int coords[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Cart_coords(correct_comm, rank, maxdims, coords);
}

int MPI_Graph_neighbors_count(MPI_Comm comm, int rank, int *nneighbors) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Graph_neighbors_count(correct_comm, rank, nneighbors);
}

int MPI_Graph_neighbors(MPI_Comm comm, int rank, int maxneighbors, int neighbors[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Graph_neighbors(correct_comm, rank, maxneighbors, neighbors);
}

int MPI_Cart_shift(MPI_Comm comm, int direction, int disp, int *rank_source, int *rank_dest) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Cart_shift(correct_comm, direction, disp, rank_source, rank_dest);
}
int MPI_Cart_sub(MPI_Comm comm, const int remain_dims[], MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Cart_sub(correct_comm, remain_dims, newcomm);
}

int MPI_Cart_map(MPI_Comm comm, int ndims, const int dims[], const int periods[], int *newrank) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Cart_map(correct_comm, ndims, dims, periods, newrank);
}

int MPI_Graph_map(MPI_Comm comm, int nnodes, const int indx[], const int edges[], int *newrank) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Graph_map(correct_comm, nnodes, indx, edges, newrank);
}

int MPI_Errhandler_set(MPI_Comm comm, MPI_Errhandler errhandler) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Errhandler_set(correct_comm, errhandler);
}

int MPI_Errhandler_get(MPI_Comm comm, MPI_Errhandler *errhandler) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Errhandler_get(correct_comm, errhandler);
}

int MPI_Abort(MPI_Comm comm, int errorcode) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Abort(correct_comm, errorcode);
}

/*
int MPI_DUP_FN(MPI_Comm comm, int key, void *extra,
        void *attrin, void *attrout, int *flag) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_DUP_FN(correct_comm, key, extra, attrin, attrout, flag);
}
*/

int MPI_Comm_connect(const char *port_name, MPI_Info info, int root, MPI_Comm comm,
                     MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_connect(port_name, info ,root, correct_comm, newcomm);
}

// Not needed, shouldn't ever free MPI_COMM_WORLD
int MPI_Comm_disconnect(MPI_Comm *comm) {
  DEBUG_PRINT("Wrapped!\n");
  return PMPI_Comm_disconnect(comm);
}

int MPI_Comm_spawn(const char *command, char *argv[], int maxprocs, MPI_Info info,
                  int root, MPI_Comm comm, MPI_Comm *intercomm,
                  int array_of_errcodes[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_spawn(command, argv, maxprocs, info,
                         root, correct_comm, intercomm, array_of_errcodes);
}
int MPI_Comm_spawn_multiple(int count, char *array_of_commands[],
                          char **array_of_argv[], const int array_of_maxprocs[],
                          const MPI_Info array_of_info[], int root, MPI_Comm comm,
                          MPI_Comm *intercomm, int array_of_errcodes[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_spawn_multiple(count, array_of_commands,
                            array_of_argv, array_of_maxprocs,
                            array_of_info, root, correct_comm,
                            intercomm, array_of_errcodes);
}

int MPI_Comm_set_info(MPI_Comm comm, MPI_Info info) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_set_info(correct_comm, info);
}

int MPI_Comm_get_info(MPI_Comm comm, MPI_Info *info) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_get_info(correct_comm, info);
}

int MPI_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info,
                  MPI_Comm comm, MPI_Win *win) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Win_create(base, size, disp_unit, info, correct_comm, win);
}

int MPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info,
                  MPI_Comm comm, void *baseptr, MPI_Win *win) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Win_allocate(size, disp_unit, info, correct_comm, baseptr, win);
}

int MPI_Win_allocate_shared(MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm,
                             void *baseptr, MPI_Win *win) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Win_allocate_shared(size, disp_unit, info, correct_comm, baseptr, win);
}

int MPI_Win_create_dynamic(MPI_Info info, MPI_Comm comm, MPI_Win *win) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Win_create_dynamic(info, correct_comm, win);
}

int MPI_Comm_call_errhandler(MPI_Comm comm, int errorcode) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_call_errhandler(correct_comm, errorcode);
}

int MPI_Comm_delete_attr(MPI_Comm comm, int comm_keyval) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_delete_attr(correct_comm, comm_keyval);
}

int MPI_Comm_get_attr(MPI_Comm comm, int comm_keyval, void *attribute_val, int *flag) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_get_attr(correct_comm, comm_keyval, attribute_val, flag);
}

int MPI_Comm_get_name(MPI_Comm comm, char *comm_name, int *resultlen) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_get_name(correct_comm, comm_name, resultlen);
}

int MPI_Comm_set_attr(MPI_Comm comm, int comm_keyval, void *attribute_val) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_set_attr(correct_comm, comm_keyval, attribute_val);
}

int MPI_Comm_set_name(MPI_Comm comm, const char *comm_name) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_set_name(correct_comm, comm_name);
}

int MPI_Comm_get_errhandler(MPI_Comm comm, MPI_Errhandler *errhandler) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_get_errhandler(correct_comm, errhandler);
}

int MPI_Comm_set_errhandler(MPI_Comm comm, MPI_Errhandler errhandler) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_set_errhandler(correct_comm, errhandler);
}

int MPI_Reduce_scatter_block(const void *sendbuf, void *recvbuf,
                             int recvcount,
                             MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Reduce_scatter_block(sendbuf, recvbuf, recvcount,
                                   datatype, op, correct_comm);
}

int MPI_Dist_graph_create_adjacent(MPI_Comm comm_old,
                                   int indegree, const int sources[],
                                   const int sourceweights[],
                                   int outdegree, const int destinations[],
                                   const int destweights[],
                                   MPI_Info info, int reorder, MPI_Comm *comm_dist_graph) {

  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm_old = GetCorrectComm(comm_old);

  return PMPI_Dist_graph_create_adjacent(correct_comm_old, indegree, sources,
                                         sourceweights, outdegree, destinations,
                                         destweights, info, reorder, comm_dist_graph);

}

int MPI_Dist_graph_create(MPI_Comm comm_old, int n, const int sources[],
                          const int degrees[], const int destinations[],
                          const int weights[],
                          MPI_Info info, int reorder, MPI_Comm *comm_dist_graph) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm_old = GetCorrectComm(comm_old);

  return PMPI_Dist_graph_create(correct_comm_old, n, sources, degrees, destinations,
                                weights, info, reorder, comm_dist_graph);
}

int MPI_Dist_graph_neighbors_count(MPI_Comm comm, int *indegree, int *outdegree, int *weighted) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Dist_graph_neighbors_count(correct_comm, indegree, outdegree, weighted);
}

int MPI_Dist_graph_neighbors(MPI_Comm comm,
                             int maxindegree, int sources[], int sourceweights[],
                             int maxoutdegree, int destinations[], int destweights[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Dist_graph_neighbors(correct_comm, maxindegree, sources, sourceweights,
                                   maxoutdegree, destinations, destweights);
}

int MPI_Improbe(int source, int tag, MPI_Comm comm, int *flag, MPI_Message *message, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Improbe(source, tag, correct_comm, flag, message, status);
}

int MPI_Mprobe(int source, int tag, MPI_Comm comm, MPI_Message *message, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Mprobe(source, tag, correct_comm, message, status);
}

int MPI_Comm_idup(MPI_Comm comm, MPI_Comm *newcomm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_idup(correct_comm, newcomm, request);
}

int MPI_Ibarrier(MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ibarrier(correct_comm, request);
}

int MPI_Ibcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ibcast(buffer, count, datatype, root, correct_comm, request);
}

int MPI_Igather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Igather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                      root, correct_comm, request);
}

int MPI_Igatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                 const int recvcounts[], const int displs[], MPI_Datatype recvtype, int root,
                 MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Igatherv(sendbuf, sendcount, sendtype, recvbuf,
                       recvcounts, displs, recvtype, root, correct_comm, request);
}

int MPI_Iscatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype, int root,
                 MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Iscatter(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                       root, correct_comm, request);
}

int MPI_Iscatterv(const void *sendbuf, const int sendcounts[], const int displs[],
                  MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  int root, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Iscatterv(sendbuf, sendcounts, displs, sendtype, recvbuf, recvcount, recvtype,
                       root, correct_comm, request);
}

int MPI_Iallgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                   void *recvbuf, int recvcount, MPI_Datatype recvtype,
                   MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Iallgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                         correct_comm, request);
}

int MPI_Iallgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                    const int recvcounts[], const int displs[], MPI_Datatype recvtype,
                    MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Iallgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype,
                          correct_comm, request);
}

int MPI_Ialltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ialltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                        correct_comm, request);
}

int MPI_Ialltoallv(const void *sendbuf, const int sendcounts[], const int sdispls[],
                   MPI_Datatype sendtype, void *recvbuf, const int recvcounts[],
                   const int rdispls[], MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ialltoallv(sendbuf, sendcounts, sdispls, sendtype, recvbuf, recvcounts,
                         rdispls, recvtype, correct_comm, request);
}

int MPI_Ialltoallw(const void *sendbuf, const int sendcounts[], const int sdispls[],
                   const MPI_Datatype sendtypes[], void *recvbuf, const int recvcounts[],
                   const int rdispls[], const MPI_Datatype recvtypes[], MPI_Comm comm,
                   MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ialltoallw(sendbuf, sendcounts, sdispls, sendtypes, recvbuf, recvcounts,
                         rdispls, recvtypes, correct_comm, request);
}

int MPI_Ireduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
                MPI_Op op, int root, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ireduce(sendbuf, recvbuf, count, datatype, op, root, correct_comm, request);
}

int MPI_Iallreduce(const void *sendbuf, void *recvbuf, int count,
                   MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
                   MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Iallreduce(sendbuf, recvbuf, count, datatype, op, correct_comm, request);
}

int MPI_Ireduce_scatter(const void *sendbuf, void *recvbuf, const int recvcounts[],
                        MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ireduce_scatter(sendbuf, recvbuf, recvcounts, datatype, op, correct_comm, request);
}

int MPI_Ireduce_scatter_block(const void *sendbuf, void *recvbuf,
                              int recvcount,
                              MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
                              MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ireduce_scatter_block(sendbuf, recvbuf, recvcount, datatype, op, correct_comm, request);
}

int MPI_Iscan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
              MPI_Op op, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Iscan(sendbuf, recvbuf, count, datatype, op, correct_comm, request);
}

int MPI_Iexscan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
                MPI_Op op, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Iexscan(sendbuf, recvbuf, count, datatype, op, correct_comm, request);
}

int MPI_Ineighbor_allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                            void *recvbuf, int recvcount, MPI_Datatype recvtype,
                            MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ineighbor_allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                                  correct_comm, request);
}

int MPI_Ineighbor_allgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                             void *recvbuf, const int recvcounts[], const int displs[],
                             MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ineighbor_allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs,
                                   recvtype, correct_comm, request);
}

int MPI_Ineighbor_alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                           void *recvbuf, int recvcount, MPI_Datatype recvtype,
                           MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ineighbor_alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                                 correct_comm, request);
}

int MPI_Ineighbor_alltoallv(const void *sendbuf, const int sendcounts[], const int sdispls[],
                            MPI_Datatype sendtype, void *recvbuf, const int recvcounts[],
                            const int rdispls[], MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ineighbor_alltoallv(sendbuf, sendcounts, sdispls, sendtype, recvbuf, recvcounts,
                           rdispls, recvtype, correct_comm, request);
}

int MPI_Ineighbor_alltoallw(const void *sendbuf, const int sendcounts[], const MPI_Aint sdispls[],
                            const MPI_Datatype sendtypes[], void *recvbuf, const int recvcounts[],
                            const MPI_Aint rdispls[], const MPI_Datatype recvtypes[],
                            MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Ineighbor_alltoallw(sendbuf, sendcounts, sdispls, sendtypes, recvbuf, recvcounts,
                                  rdispls, recvtypes, correct_comm, request);
}

int MPI_Neighbor_allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                           void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Neighbor_allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount,
                                 recvtype, correct_comm);
}

int MPI_Neighbor_allgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                            void *recvbuf, const int recvcounts[], const int displs[],
                            MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Neighbor_allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs,
                                  recvtype, correct_comm);
}

int MPI_Neighbor_alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                          void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Neighbor_alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, correct_comm);
}

int MPI_Neighbor_alltoallv(const void *sendbuf, const int sendcounts[], const int sdispls[],
                           MPI_Datatype sendtype, void *recvbuf, const int recvcounts[],
                           const int rdispls[], MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Neighbor_alltoallv(sendbuf, sendcounts, sdispls, sendtype, recvbuf, recvcounts,
                                 rdispls, recvtype, correct_comm);
}

int MPI_Neighbor_alltoallw(const void *sendbuf, const int sendcounts[], const MPI_Aint sdispls[],
                           const MPI_Datatype sendtypes[], void *recvbuf, const int recvcounts[],
                           const MPI_Aint rdispls[], const MPI_Datatype recvtypes[], MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Neighbor_alltoallw(sendbuf, sendcounts, sdispls, sendtypes, recvbuf, recvcounts, rdispls, recvtypes, correct_comm);
}

int MPI_Comm_split_type(MPI_Comm comm, int split_type, int key, MPI_Info info, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_split_type(correct_comm, split_type, key, info, newcomm);
}

int MPI_Comm_create_group(MPI_Comm comm, MPI_Group group, int tag, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_Comm_create_group(correct_comm, group, tag, newcomm);
}

int MPIX_Comm_group_failed(MPI_Comm comm, MPI_Group *failed_group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPIX_Comm_group_failed(correct_comm, failed_group);
}

int MPIX_Comm_remote_group_failed(MPI_Comm comm, MPI_Group *failed_group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPIX_Comm_remote_group_failed(correct_comm, failed_group);
}

int MPIX_Comm_reenable_anysource(MPI_Comm comm, MPI_Group *failed_group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPIX_Comm_reenable_anysource(correct_comm, failed_group);
}

int MPI_File_open(MPI_Comm comm, ROMIO_CONST char *filename, int amode,
                  MPI_Info info, MPI_File *fh) {

  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm correct_comm = GetCorrectComm(comm);

  return PMPI_File_open(correct_comm, filename, amode, info, fh);
}
