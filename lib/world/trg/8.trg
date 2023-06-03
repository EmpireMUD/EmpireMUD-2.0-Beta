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
#804
Lantern: Refill with candle using put~
1 c 2
put~
set max_light 48
* basic args (pass back to normal 'put')
set candle_arg %arg.car%
set lantern_arg %arg.cdr.car%
if !%candle_arg% || !%lantern_arg%
  return 0
  halt
elseif %actor.obj_target(%lantern_arg%)% != %self%
  return 0
  halt  
end
* will send errors from here, so set return
return 1
* validate candle
set candle %actor.obj_target_inv(%candle_arg%)%
if !%candle%
  %send% %actor% You don't seem to have %candle_arg.ana% %candle_arg%.
  halt
elseif !%candle.is_component(851)%
  %send% %actor% You can't put that in @%self%.
  halt
end
* determine amount
if %candle.type% == LIGHT && %candle.val0% > -1
  set amount %candle.val0%
else
  set amount 8
end
* final validation
if %amount% < 1
  %send% %actor% There's not enough of @%candle% left to bother.
  halt
elseif %self.val0% >= %max_light%
  %send% %actor% You can't put any more candles in there right now.
  halt
end
* and do it
%send% %actor% You put @%candle% into @%self%.
eval new_amount %self.val0% + %amount%
if %new_amount% > %max_light%
  set new_amount %max_light%
end
nop %self.val0(%new_amount%)%
%purge% %candle%
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
