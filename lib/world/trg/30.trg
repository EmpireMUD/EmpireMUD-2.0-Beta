#3053
Single calabash drying timer (deprecated)~
1 f 0 1
L c 3056
~
* this is replaced by simple decays-to interactions
if %self.carried_by%
  %send% %self.carried_by% @%self% dries out. You throw it away, but the seeds are still good.
  %load% obj 3056 %self.carried_by% inv
else
  %echo% @%self% dries out, leaving behind only some seeds.
  %load% obj 3056
end
return 0
%purge% %self%
~
#3054
Calabash drying timer (deprecated)~
1 f 0 2
L c 3055
L c 3056
~
* this is replaced by simple decays-to interactions
if %self.carried_by%
  %send% %self.carried_by% @%self% dries out.
  %load% obj 3055 %self.carried_by% inv
  if %random.3% == 3
    %load% obj 3056 %self.carried_by% inv
  end
else
  %echo% @%self% dries out.
  %load% obj 3055
  if %random.3% == 3
    %load% obj 3056
  end
end
return 0
%purge% %self%
~
#3090
Plant wildflowers based on climate~
1 c 2 3
L g 3088
L g 3091
L g 3093
plant~
* temporarily becomes plantable, then changes back
return 0
*
if %actor.obj_target(%arg.argument1%)% != %self%
  halt
end
*
if %self.room.climate(temperate)%
  set vnum 3088
elseif %self.room.climate(arid)%
  set vnum 3091
elseif %self.room.climate(tropical)%
  set vnum 3093
else
  * nothing to plant here
  if %self.is_flagged(PLANTABLE)%
    nop %self.flag(PLANTABLE)%
  end
  halt
end
*
nop %self.val1(%vnum%)%
if !%self.is_flagged(PLANTABLE)%
  nop %self.flag(PLANTABLE)%
end
*
* set back if still here
wait 1
nop %self.val1(0)%
nop %self.flag(PLANTABLE)%
~
$
