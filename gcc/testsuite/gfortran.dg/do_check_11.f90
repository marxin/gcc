! { dg-do run }
! { dg-options "-fcheck=do" }
! { dg-shouldfail "DO check" }
!
! Run-time check for infinite loop with -ffast-do-loop
program test
  implicit none
  integer(1) :: i
  do i = HUGE(i)-10, HUGE(i)
    print *, i
  end do
end program test
! { dg-output "Fortran runtime error: Loop iterates infinitely" }
