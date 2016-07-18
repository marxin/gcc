! PR fortran/71799
! { dg-do compile }

subroutine test2(array, s, block)
integer(1) :: i, block(9), array(2)
integer (8) :: s

do i = 10, HUGE(i) - 10, 222
  s = s + 1
end do

end subroutine test2
