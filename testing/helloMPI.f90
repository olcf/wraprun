program hello_world
  include 'mpif.h'
  integer ierr

  call MPI_INIT ( ierr )

  print *, "Hello world"

  call MPI_BARRIER(MPI_COMM_WORLD, ierr)
 
  call MPI_FINALIZE ( ierr )

end program hello_world
