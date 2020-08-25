#2149
Guard whistle~
1 c 2
use~
* 'use' summons several guards based on charisma; vnum is val 0 on the obj
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if %actor.cooldown(2029)%
  %send% %actor% Your %cooldown.2029% is on cooldown.
  halt
end
if %actor.position% != Standing && %actor.position% != Fighting
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.empire%
  %send% %actor% You must be a member of an empire to use this.
  halt
end
* check room
set room %actor.room%
if %room.empire% != %actor.empire%
  %send% %actor% You can only use this in one of your own cities.
  halt
end
if !%room.in_city(true)%
  %send% %actor% You can only use this in an established city.
  halt
end
* ok... messaging
%send% %actor% You blow %self.shortdesc%...
%echoaround% %actor% %actor.name% blows %self.shortdesc%...
* and summon
eval to_summon %actor.charisma% / 3
if %to_summon% < 1
  set to_summon 1
end
set count 0
while %count% < %to_summon%
  %load% mob %self.val0%
  set mob %room.people%
  %own% %mob% %actor.empire%
  %echo% %mob.name% comes running!
  eval count %count% + 1
done
nop %actor.set_cooldown(2029, 180)%
nop %self.bind(%actor%)%
~
$
