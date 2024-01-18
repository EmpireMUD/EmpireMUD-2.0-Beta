#3202
Egg: Throw~
1 c 2
throw~
return 0
if %actor.obj_target(%arg.car%)% != %self%
  halt
end
wait 1
if !%self.carried_by% && %self.room% != %actor.room%
  %echo% @%self% smashes open.
  %purge% %self%
end
~
$
