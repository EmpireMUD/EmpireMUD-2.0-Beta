#803
Tarnished mirror look~
1 c 3
look~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
%send% %actor% You look into @%self%, but it's too tarnished to see a good reflection.
%echoaround% %actor% ~%actor% looks into @%self%.
~
#806
Looking Glass reflection~
1 c 3
look~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
%send% %actor% You look into @%self%...
%echoaround% %actor% ~%actor% looks into @%self%.
if %actor.plr_flagged(VAMPIRE)%
  %send% %actor% You don't seem to have a reflection.
else
  %force% %actor% look self
end
set ch %actor.room.people%
set any 0
while %ch%
  if (%ch% != %actor% && (%ch.plr_flagged(VAMPIRE)% || %ch.mob_flagged(VAMPIRE)%))
    if !%any%
      %send% %actor% &&0
      set any 1
    end
    %send% %actor% Over your shoulder, you notice that ~%ch% has no reflection.
  end
  set ch %ch.next_in_room%
done
~
$
