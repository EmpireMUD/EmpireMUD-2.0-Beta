#803
Tarnished mirror look~
1 c 3
look~
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
  return 0
  halt
end
%send% %actor% You look into %self.shortdesc%, but it's too tarnished to see a good reflection.
%echoaround% %actor% %actor.name% looks into %self.shortdesc%.
~
#806
Looking Glass reflection~
1 c 3
look~
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
  return 0
  halt
end
%send% %actor% You look into %self.shortdesc%...
%echoaround% %actor% %actor.name% looks into %self.shortdesc%.
if %actor.plr_flagged(VAMPIRE)%
  %send% %actor% You don't seem to have a reflection.
else
  %force% %actor% look self
end
eval room_var %actor.room%
eval ch %room_var.people%
eval any 0
while %ch%
  if (%ch% != %actor% && (%ch.plr_flagged(VAMPIRE)% || %ch.mob_flagged(VAMPIRE)%))
    if !%any%
      %send% %actor% &0
      eval any 1
    end
    %send% %actor% Over your shoulder, you notice that %ch.name% has no reflection.
  end
  eval ch %ch.next_in_room%
done
~
$
