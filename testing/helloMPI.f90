program hello_world
  implicit none
  include 'mpif.h'
  integer ierr, rank, size
  character*128 cwd

  call MPI_INIT ( ierr )
  call MPI_COMM_RANK(MPI_COMM_WORLD, rank, ierr)
  call MPI_COMM_SIZE(MPI_COMM_WORLD, size, ierr)

  call getcwd(cwd)

  write(*,*) "rank ", rank, "of ", size, TRIM(cwd)

  call MPI_BARRIER(MPI_COMM_WORLD, ierr)
 
  call MPI_FINALIZE ( ierr )

end program hello_world
