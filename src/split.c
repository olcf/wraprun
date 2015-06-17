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
#include <string.h>
#include "debug.h"
#include "mpi.h"

static MPI_Comm MPI_COMM_SPLIT;

// Returns color integer fetched from WRAPRUN_{rank}
static int GetColor(const int rank)
{
  // construct environment variable WRAPRUN_${RANK}
  char color_var[64];
  sprintf(color_var, "WRAPRUN_%d", rank);

  // Read color from environment variable
  char *char_color = getenv(color_var);
  if(char_color == NULL) {
    printf("%s environment variable not set, exiting!\n", color_var);
    exit(1);
  }
  return strtol(char_color, &char_color, 10);
}

// Hijack to create MPI_COMM_SPLIT
int MPI_Init(int *argc, char ***argv) {
  DEBUG_PRINT("Wrapped!\n");

  int return_value = PMPI_Init(argc, argv);

  int rank;
  PMPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int color = GetColor(rank);

  int err = PMPI_Comm_split(MPI_COMM_WORLD, color, 0, &MPI_COMM_SPLIT);
  if(err != MPI_SUCCESS) {
    printf("Failed to split communicator: %d !\n", err)
    exit(1);
  }

  return return_value;
}

/*
  If input_comm == MPI_COMM_WORLD set output_com to MPI_COMM_SPLIT
  To avoid MPI_Comm_comp and MPI_Comm_dup it is
  assumed MPI_Comm is typedef of a basic type, e.g. int
*/
static MPI_Comm SwitchWorldComm(const MPI_Comm input_comm) {
  return (input_comm == MPI_COMM_WORLD) ? MPI_COMM_SPLIT : input_comm;
}

///////////////////////////////////////////////////////////////////////////////
///// Simple MPI wrapper functions
//////////////////////////////////////////////////////////////////////////////
int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Send(buf, count, datatype, dest, tag, split_comm);
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag,
             MPI_Comm comm, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Recv(buf, count, datatype, source, tag, split_comm, status);
}

int MPI_Bsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Bsend(buf, count, datatype, dest, tag, split_comm);
}

int MPI_Ssend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ssend(buf, count, datatype, dest, tag, split_comm);
}

int MPI_Rsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Rsend(buf, count, datatype, dest, tag, split_comm);
}

int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Isend(buf, count, datatype, dest, tag, split_comm, request);
}

int MPI_Ibsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ibsend(buf, count, datatype, dest, tag, split_comm, request);
}

int MPI_Issend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Issend(buf, count, datatype, dest, tag, split_comm, request);
}

int MPI_Irsend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag,
              MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Irsend(buf, count, datatype, dest, tag, split_comm, request);
}

int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source,
              int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Irecv(buf, count, datatype, source, tag, split_comm, request);
}

int MPI_Iprobe(int source, int tag, MPI_Comm comm, int *flag, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Iprobe(source, tag, split_comm, flag, status);
}

int MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Probe(source, tag, split_comm, status);
}

int MPI_Send_init(const void *buf, int count, MPI_Datatype datatype, int dest,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Send_init(buf, count, datatype, dest, tag, split_comm, request);
}

int MPI_Bsend_init(const void *buf, int count, MPI_Datatype datatype, int dest,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Bsend_init(buf, count, datatype, dest, tag, split_comm, request);
}

int MPI_Ssend_init(const void *buf, int count, MPI_Datatype datatype, int dest,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ssend_init(buf, count, datatype, dest, tag, split_comm, request);
}

int MPI_Rsend_init(const void *buf, int count, MPI_Datatype datatype, int dest,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Rsend_init(buf, count, datatype, dest, tag, split_comm, request);
}

int MPI_Recv_init(void *buf, int count, MPI_Datatype datatype, int source,
                 int tag, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Recv_init(buf, count, datatype, source, tag, split_comm, request);
}

int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                int dest, int sendtag,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int source, int recvtag,
                MPI_Comm comm, MPI_Status *status) {

  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Sendrecv(sendbuf, sendcount, sendtype, dest, sendtag,
                       recvbuf, recvcount, recvtype, source, recvtag,
                       split_comm, status);
}

int MPI_Sendrecv_replace(void *buf, int count, MPI_Datatype datatype,
                       int dest, int sendtag, int source, int recvtag,
                       MPI_Comm comm, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Sendrecv_replace(buf, count, datatype, dest, sendtag, source,
                               recvtag, split_comm, status);
}

int MPI_Pack(const void *inbuf,
             int incount,
             MPI_Datatype datatype,
             void *outbuf,
             int outsize,
             int *position,
             MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Pack(inbuf, incount, datatype,
                   outbuf, outsize, position, split_comm);

}

int MPI_Unpack(const void *inbuf, int insize, int *position,
               void *outbuf, int outcount, MPI_Datatype datatype,
               MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Unpack(inbuf, insize, position, outbuf, outcount,
                     datatype, split_comm);
}

int MPI_Pack_size(int incount, MPI_Datatype datatype, MPI_Comm comm, int *size) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Pack_size(incount, datatype, split_comm, size);
}

int MPI_Barrier(MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Barrier(split_comm);
}

int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Bcast(buffer, count, datatype, root, split_comm);
}

int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
    void *recvbuf, int recvcount, MPI_Datatype recvtype, int root,
    MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Gather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                     root, split_comm);
}

int MPI_Gatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, const int *recvcounts, const int *displs,
                MPI_Datatype recvtype, int root, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Gatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts,
                      displs, recvtype, root, split_comm);
}

int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype, int root,
               MPI_Comm comm) {

  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Scatter(sendbuf, sendcount, sendtype, recvbuf, recvcount,
                      recvtype, root, split_comm);

}

int MPI_Scatterv(const void *sendbuf, const int *sendcounts, const int *displs,
                 MPI_Datatype sendtype, void *recvbuf, int recvcount,
                 MPI_Datatype recvtype,
                 int root, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Scatterv(sendbuf, sendcounts, displs, sendtype, recvbuf, recvcount,
                      recvtype, root, split_comm);
}

int MPI_Allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                        split_comm);
}

int MPI_Allgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                   void *recvbuf, const int *recvcounts, const int *displs,
                   MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs,
                        recvtype, split_comm);
}

int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount,
                       recvtype, split_comm);
}

int MPI_Alltoallv(const void *sendbuf, const int *sendcounts,
                  const int *sdispls, MPI_Datatype sendtype, void *recvbuf,
                  const int *recvcounts, const int *rdispls, MPI_Datatype recvtype,
                  MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Alltoallv(sendbuf, sendcounts,
                        sdispls, sendtype, recvbuf,
                        recvcounts, rdispls, recvtype,
                        split_comm);
}

int MPI_Alltoallw(const void *sendbuf, const int sendcounts[],
                  const int sdispls[], const MPI_Datatype sendtypes[],
                  void *recvbuf, const int recvcounts[], const int rdispls[],
                  const MPI_Datatype recvtypes[], MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Alltoallw(sendbuf, sendcounts,
                        sdispls, sendtypes,
                        recvbuf, recvcounts, rdispls,
                        recvtypes, split_comm);
}

int MPI_Exscan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Exscan(sendbuf, recvbuf, count, datatype, op, split_comm);
}

int MPI_Reduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
               MPI_Op op, int root, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Reduce(sendbuf, recvbuf, count, datatype,
                    op, root, split_comm);
}

int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count,
                  MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Allreduce(sendbuf, recvbuf, count,
                        datatype, op, split_comm);
}

int MPI_Reduce_scatter(const void *sendbuf, void *recvbuf, const int recvcounts[],
                      MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Reduce_scatter(sendbuf, recvbuf, recvcounts,
                             datatype, op, split_comm);
}

int MPI_Scan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
             MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Scan(sendbuf, recvbuf, count, datatype,
                   op, split_comm);
}

int MPI_Comm_group(MPI_Comm comm, MPI_Group *group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_group(split_comm, group);
}

int MPI_Comm_size(MPI_Comm comm, int *size) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_size(split_comm, size);
}

int MPI_Comm_rank(MPI_Comm comm, int *rank) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_rank(split_comm, rank);
}

int MPI_Comm_compare(MPI_Comm comm1, MPI_Comm comm2, int *result) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm1 = SwitchWorldComm(comm1);

  MPI_Comm split_comm2 = SwitchWorldComm(comm2);

  return PMPI_Comm_compare(split_comm1, split_comm2, result);
}

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_dup(split_comm, newcomm);
}

int MPI_Comm_dup_with_info(MPI_Comm comm, MPI_Info info, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_dup_with_info(split_comm, info, newcomm);
}

int MPI_Comm_create(MPI_Comm comm, MPI_Group group, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_create(split_comm, group, newcomm);
}

int MPI_Comm_split(MPI_Comm comm, int color, int key, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_split(split_comm, color, key, newcomm);
}

// Ehhh...
int MPI_Comm_free(MPI_Comm *comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(*comm);

  return PMPI_Comm_free(&split_comm);
}

int MPI_Comm_test_inter(MPI_Comm comm, int *flag) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_test_inter(split_comm, flag);
}

int MPI_Comm_remote_size(MPI_Comm comm, int *size) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_remote_size(split_comm, size);
}

int MPI_Comm_remote_group(MPI_Comm comm, MPI_Group *group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_remote_group(split_comm, group);
}

int MPI_Intercomm_create(MPI_Comm local_comm, int local_leader,
                       MPI_Comm peer_comm, int remote_leader, int tag,
                       MPI_Comm *newintercomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm local_split_comm = SwitchWorldComm(local_comm);

  MPI_Comm peer_split_comm = SwitchWorldComm(peer_comm);

  return PMPI_Intercomm_create(local_split_comm, local_leader,
                               peer_split_comm, remote_leader, tag,
                               newintercomm);
}

int MPI_Intercomm_merge(MPI_Comm intercomm, int high, MPI_Comm *newintracomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(intercomm);

  return PMPI_Intercomm_merge(split_comm, high, newintracomm);
}

int MPI_Attr_put(MPI_Comm comm, int keyval, void *attribute_val) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Attr_put(split_comm, keyval, attribute_val);
}

int MPI_Attr_get(MPI_Comm comm, int keyval, void *attribute_val, int *flag) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Attr_get(split_comm, keyval, attribute_val, flag);
}

int MPI_Attr_delete(MPI_Comm comm, int keyval) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Attr_delete(split_comm, keyval);
}

int MPI_Topo_test(MPI_Comm comm, int *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Topo_test(split_comm, status);
}

int MPI_Cart_create(MPI_Comm comm_old, int ndims, const int dims[],
                    const int periods[], int reorder, MPI_Comm *comm_cart) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm_old);

  return PMPI_Cart_create(split_comm, ndims, dims, periods, reorder, comm_cart);
}
int MPI_Graph_create(MPI_Comm comm_old, int nnodes, const int indx[],
                     const int edges[], int reorder, MPI_Comm *comm_graph) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm_old);

  return PMPI_Graph_create(split_comm, nnodes, indx, edges, reorder, comm_graph);
}

int MPI_Graphdims_get(MPI_Comm comm, int *nnodes, int *nedges) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Graphdims_get(split_comm, nnodes, nedges);
}

int MPI_Graph_get(MPI_Comm comm, int maxindex, int maxedges, int indx[], int edges[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Graph_get(split_comm, maxindex, maxedges, indx, edges);
}

int MPI_Cartdim_get(MPI_Comm comm, int *ndims) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Cartdim_get(split_comm, ndims);
}

int MPI_Cart_get(MPI_Comm comm, int maxdims, int dims[], int periods[], int coords[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Cart_get(split_comm, maxdims, dims, periods, coords);
}

int MPI_Cart_rank(MPI_Comm comm, const int coords[], int *rank) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Cart_rank(split_comm, coords, rank);
}

int MPI_Cart_coords(MPI_Comm comm, int rank, int maxdims, int coords[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Cart_coords(split_comm, rank, maxdims, coords);
}

int MPI_Graph_neighbors_count(MPI_Comm comm, int rank, int *nneighbors) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Graph_neighbors_count(split_comm, rank, nneighbors);
}

int MPI_Graph_neighbors(MPI_Comm comm, int rank, int maxneighbors, int neighbors[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Graph_neighbors(split_comm, rank, maxneighbors, neighbors);
}

int MPI_Cart_shift(MPI_Comm comm, int direction, int disp, int *rank_source, int *rank_dest) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Cart_shift(split_comm, direction, disp, rank_source, rank_dest);
}
int MPI_Cart_sub(MPI_Comm comm, const int remain_dims[], MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Cart_sub(split_comm, remain_dims, newcomm);
}

int MPI_Cart_map(MPI_Comm comm, int ndims, const int dims[], const int periods[], int *newrank) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Cart_map(split_comm, ndims, dims, periods, newrank);
}

int MPI_Graph_map(MPI_Comm comm, int nnodes, const int indx[], const int edges[], int *newrank) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Graph_map(split_comm, nnodes, indx, edges, newrank);
}

int MPI_Errhandler_set(MPI_Comm comm, MPI_Errhandler errhandler) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Errhandler_set(split_comm, errhandler);
}

int MPI_Errhandler_get(MPI_Comm comm, MPI_Errhandler *errhandler) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Errhandler_get(split_comm, errhandler);
}

int MPI_Abort(MPI_Comm comm, int errorcode) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Abort(split_comm, errorcode);
}

int MPI_DUP_FN(MPI_Comm comm, int key, void *extra,
        void *attrin, void *attrout, int *flag) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_DUP_FN(split_comm, key, extra, attrin, attrout, flag);
}

int MPI_Comm_connect(const char *port_name, MPI_Info info, int root, MPI_Comm comm,
                     MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_connect(port_name, info ,root, split_comm, newcomm);
}

int MPI_Comm_disconnect(MPI_Comm *comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(*comm);

  return PMPI_Comm_disconnect(&split_comm);
}

int MPI_Comm_spawn(const char *command, char *argv[], int maxprocs, MPI_Info info,
                  int root, MPI_Comm comm, MPI_Comm *intercomm,
                  int array_of_errcodes[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_spawn(command, argv, maxprocs, info,
                         root, split_comm, intercomm, array_of_errcodes);
}
int MPI_Comm_spawn_multiple(int count, char *array_of_commands[],
                          char **array_of_argv[], const int array_of_maxprocs[],
                          const MPI_Info array_of_info[], int root, MPI_Comm comm,
                          MPI_Comm *intercomm, int array_of_errcodes[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_spawn_multiple(count, array_of_commands,
                            array_of_argv, array_of_maxprocs,
                            array_of_info, root, split_comm,
                            intercomm, array_of_errcodes);
}

int MPI_Comm_set_info(MPI_Comm comm, MPI_Info info) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_set_info(split_comm, info);
}

int MPI_Comm_get_info(MPI_Comm comm, MPI_Info *info) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_get_info(split_comm, info);
}

int MPI_Win_create(void *base, MPI_Aint size, int disp_unit, MPI_Info info,
                  MPI_Comm comm, MPI_Win *win) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Win_create(base, size, disp_unit, info, split_comm, win);
}

int MPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info,
                  MPI_Comm comm, void *baseptr, MPI_Win *win) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Win_allocate(size, disp_unit, info, split_comm, baseptr, win);
}

int MPI_Win_allocate_shared(MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm,
                             void *baseptr, MPI_Win *win) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Win_allocate_shared(size, disp_unit, info, split_comm, baseptr, win);
}

int MPI_Win_create_dynamic(MPI_Info info, MPI_Comm comm, MPI_Win *win) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Win_create_dynamic(info, split_comm, win);
}

int MPI_Comm_call_errhandler(MPI_Comm comm, int errorcode) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_call_errhandler(split_comm, errorcode);
}

int MPI_Comm_delete_attr(MPI_Comm comm, int comm_keyval) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_delete_attr(split_comm, comm_keyval);
}

int MPI_Comm_get_attr(MPI_Comm comm, int comm_keyval, void *attribute_val, int *flag) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_get_attr(split_comm, comm_keyval, attribute_val, flag);
}

int MPI_Comm_get_name(MPI_Comm comm, char *comm_name, int *resultlen) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_get_name(split_comm, comm_name, resultlen);
}

int MPI_Comm_set_attr(MPI_Comm comm, int comm_keyval, void *attribute_val) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_set_attr(split_comm, comm_keyval, attribute_val);
}

int MPI_Comm_set_name(MPI_Comm comm, const char *comm_name) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_set_name(split_comm, comm_name);
}

int MPI_Comm_get_errhandler(MPI_Comm comm, MPI_Errhandler *errhandler) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_get_errhandler(split_comm, errhandler);
}

int MPI_Comm_set_errhandler(MPI_Comm comm, MPI_Errhandler errhandler) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_set_errhandler(split_comm, errhandler);
}

int MPI_Reduce_scatter_block(const void *sendbuf, void *recvbuf,
                             int recvcount,
                             MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Reduce_scatter_block(sendbuf, recvbuf, recvcount,
                                   datatype, op, split_comm);
}

int MPI_Dist_graph_create_adjacent(MPI_Comm comm_old,
                                   int indegree, const int sources[],
                                   const int sourceweights[],
                                   int outdegree, const int destinations[],
                                   const int destweights[],
                                   MPI_Info info, int reorder, MPI_Comm *comm_dist_graph) {

  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm_old);

  return PMPI_Dist_graph_create_adjacent(split_comm, indegree, sources,
                                         sourceweights, outdegree, destinations,
                                         destweights, info, reorder, comm_dist_graph);

}

int MPI_Dist_graph_create(MPI_Comm comm_old, int n, const int sources[],
                          const int degrees[], const int destinations[],
                          const int weights[],
                          MPI_Info info, int reorder, MPI_Comm *comm_dist_graph) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm_old);

  return PMPI_Dist_graph_create(split_comm, n, sources, degrees, destinations,
                                weights, info, reorder, comm_dist_graph);
}

int MPI_Dist_graph_neighbors_count(MPI_Comm comm, int *indegree, int *outdegree, int *weighted) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Dist_graph_neighbors_count(split_comm, indegree, outdegree, weighted);
}

int MPI_Dist_graph_neighbors(MPI_Comm comm,
                             int maxindegree, int sources[], int sourceweights[],
                             int maxoutdegree, int destinations[], int destweights[]) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Dist_graph_neighbors(split_comm, maxindegree, sources, sourceweights,
                                   maxoutdegree, destinations, destweights);
}

int MPI_Improbe(int source, int tag, MPI_Comm comm, int *flag, MPI_Message *message, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Improbe(source, tag, split_comm, flag, message, status);
}

int MPI_Mprobe(int source, int tag, MPI_Comm comm, MPI_Message *message, MPI_Status *status) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Mprobe(source, tag, split_comm, message, status);
}

int MPI_Comm_idup(MPI_Comm comm, MPI_Comm *newcomm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_idup(split_comm, newcomm, request);
}

int MPI_Ibarrier(MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ibarrier(split_comm, request);
}

int MPI_Ibcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ibcast(buffer, count, datatype, root, split_comm, request);
}

int MPI_Igather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Igather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                      root, split_comm, request);
}

int MPI_Igatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                 const int recvcounts[], const int displs[], MPI_Datatype recvtype, int root,
                 MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Igatherv(sendbuf, sendcount, sendtype, recvbuf,
                       recvcounts, displs, recvtype, root, split_comm, request);
}

int MPI_Iscatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype, int root,
                 MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Iscatter(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                       root, split_comm, request);
}

int MPI_Iscatterv(const void *sendbuf, const int sendcounts[], const int displs[],
                  MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  int root, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Iscatterv(sendbuf, sendcounts, displs, sendtype, recvbuf, recvcount, recvtype,
                       root, split_comm, request);
}

int MPI_Iallgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                   void *recvbuf, int recvcount, MPI_Datatype recvtype,
                   MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Iallgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                         split_comm, request);
}

int MPI_Iallgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf,
                    const int recvcounts[], const int displs[], MPI_Datatype recvtype,
                    MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Iallgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype,
                          split_comm, request);
}

int MPI_Ialltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ialltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                        split_comm, request);
}

int MPI_Ialltoallv(const void *sendbuf, const int sendcounts[], const int sdispls[],
                   MPI_Datatype sendtype, void *recvbuf, const int recvcounts[],
                   const int rdispls[], MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ialltoallv(sendbuf, sendcounts, sdispls, sendtype, recvbuf, recvcounts,
                         rdispls, recvtype, split_comm, request);
}

int MPI_Ialltoallw(const void *sendbuf, const int sendcounts[], const int sdispls[],
                   const MPI_Datatype sendtypes[], void *recvbuf, const int recvcounts[],
                   const int rdispls[], const MPI_Datatype recvtypes[], MPI_Comm comm,
                   MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ialltoallw(sendbuf, sendcounts, sdispls, sendtypes, recvbuf, recvcounts,
                         rdispls, recvtypes, split_comm, request);
}

int MPI_Ireduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
                MPI_Op op, int root, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ireduce(sendbuf, recvbuf, count, datatype, op, root, split_comm, request);
}

int MPI_Iallreduce(const void *sendbuf, void *recvbuf, int count,
                   MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
                   MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Iallreduce(sendbuf, recvbuf, count, datatype, op, split_comm, request);
}

int MPI_Ireduce_scatter(const void *sendbuf, void *recvbuf, const int recvcounts[],
                        MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ireduce_scatter(sendbuf, recvbuf, recvcounts, datatype, op, split_comm, request);
}

int MPI_Ireduce_scatter_block(const void *sendbuf, void *recvbuf,
                              int recvcount,
                              MPI_Datatype datatype, MPI_Op op, MPI_Comm comm,
                              MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ireduce_scatter_block(sendbuf, recvbuf, recvcount, datatype, op, split_comm, request);
}

int MPI_Iscan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
              MPI_Op op, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Iscan(sendbuf, recvbuf, count, datatype, op, split_comm, request);
}

int MPI_Iexscan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
                MPI_Op op, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Iexscan(sendbuf, recvbuf, count, datatype, op, split_comm, request);
}

int MPI_Ineighbor_allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                            void *recvbuf, int recvcount, MPI_Datatype recvtype,
                            MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ineighbor_allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                                  split_comm, request);
}

int MPI_Ineighbor_allgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                             void *recvbuf, const int recvcounts[], const int displs[],
                             MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ineighbor_allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs,
                                   recvtype, split_comm, request);
}

int MPI_Ineighbor_alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                           void *recvbuf, int recvcount, MPI_Datatype recvtype,
                           MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ineighbor_alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype,
                                 split_comm, request);
}

int MPI_Ineighbor_alltoallv(const void *sendbuf, const int sendcounts[], const int sdispls[],
                            MPI_Datatype sendtype, void *recvbuf, const int recvcounts[],
                            const int rdispls[], MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ineighbor_alltoallv(sendbuf, sendcounts, sdispls, sendtype, recvbuf, recvcounts,
                           rdispls, recvtype, split_comm, request);
}

int MPI_Ineighbor_alltoallw(const void *sendbuf, const int sendcounts[], const MPI_Aint sdispls[],
                            const MPI_Datatype sendtypes[], void *recvbuf, const int recvcounts[],
                            const MPI_Aint rdispls[], const MPI_Datatype recvtypes[],
                            MPI_Comm comm, MPI_Request *request) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Ineighbor_alltoallw(sendbuf, sendcounts, sdispls, sendtypes, recvbuf, recvcounts,
                                  rdispls, recvtypes, split_comm, request);
}

int MPI_Neighbor_allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                           void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Neighbor_allgather(sendbuf, sendcount, sendtype, recvbuf, recvcount,
                                 recvtype, split_comm);
}

int MPI_Neighbor_allgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                            void *recvbuf, const int recvcounts[], const int displs[],
                            MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Neighbor_allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs,
                                  recvtype, split_comm);
}

int MPI_Neighbor_alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                          void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Neighbor_alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, split_comm);
}

int MPI_Neighbor_alltoallv(const void *sendbuf, const int sendcounts[], const int sdispls[],
                           MPI_Datatype sendtype, void *recvbuf, const int recvcounts[],
                           const int rdispls[], MPI_Datatype recvtype, MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Neighbor_alltoallv(sendbuf, sendcounts, sdispls, sendtype, recvbuf, recvcounts,
                                 rdispls, recvtype, split_comm);
}

int MPI_Neighbor_alltoallw(const void *sendbuf, const int sendcounts[], const MPI_Aint sdispls[],
                           const MPI_Datatype sendtypes[], void *recvbuf, const int recvcounts[],
                           const MPI_Aint rdispls[], const MPI_Datatype recvtypes[], MPI_Comm comm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Neighbor_alltoallw(sendbuf, sendcounts, sdispls, sendtypes, recvbuf, recvcounts, rdispls, recvtypes, split_comm);
}

int MPI_Comm_split_type(MPI_Comm comm, int split_type, int key, MPI_Info info, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_split_type(split_comm, split_type, key, info, newcomm);
}

int MPI_Comm_create_group(MPI_Comm comm, MPI_Group group, int tag, MPI_Comm *newcomm) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPI_Comm_create_group(split_comm, group, tag, newcomm);
}

int MPIX_Comm_group_failed(MPI_Comm comm, MPI_Group *failed_group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPIX_Comm_group_failed(split_comm, failed_group);
}

int MPIX_Comm_remote_group_failed(MPI_Comm comm, MPI_Group *failed_group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPIX_Comm_remote_group_failed(split_comm, failed_group);
}
int MPIX_Comm_reenable_anysource(MPI_Comm comm, MPI_Group *failed_group) {
  DEBUG_PRINT("Wrapped!\n");

  MPI_Comm split_comm = SwitchWorldComm(comm);

  return PMPIX_Comm_reenable_anysource(split_comm, failed_group);
}
