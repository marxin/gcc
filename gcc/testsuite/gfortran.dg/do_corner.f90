! { dg-do run }
! { dg-options "-fno-fast-do-loop" }
! Program to check corner cases for DO statements.


program do_1
  implicit none
  integer i, j

  ! limit=HUGE(i), step 1
  j = 0
  do i = HUGE(i) - 10, HUGE(i), 1
    j = j + 1
  end do
  if (j .ne. 11) call abort

  ! limit=-HUGE(i)-1, step -1
  j = 0
  do i = -HUGE(i) + 10 - 1, -HUGE(i) - 1, -1
    j = j + 1
  end do
  if (j .ne. 11) call abort

end program
