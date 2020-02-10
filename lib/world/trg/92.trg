#9229
Feral Dog Pack~
0 n 100
~
if %self.master%
  halt
end
set num %random.3%
while %num% > 0
  eval num %num% - 1
  %load% mob 9229 ally
  set mob %self.room.people%
  if %mob.vnum% == 9229 && %mob% != %self%
    nop %mob.add_mob_flag(SENTINEL)%
  end
done
~
#9232
Box Jellyfish Combat~
0 k 100
~
switch %random.3%
  case 1
    set part leg
  break
  case 2
    set part arm
  break
  case 3
    set part neck
  break
done
%send% %actor% %self.name%'s tentacles wrap around your %part%... that stings!
%echoaround% %actor% %self.name%'s tentacles wrap around %actor.name%'s %part%!
%dot% #9232 %actor% 200 20 poison 100
~
$
