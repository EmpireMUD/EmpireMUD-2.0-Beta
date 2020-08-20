#9800
Generic No Sacrifice~
1 h 100
~
if %command% == sacrifice
  %send% %actor% %self.shortdesc% cannot be sacrificed.
  return 0
  halt
end
return 1
~
#9801
Read looks at target~
1 c 6
read~
if %actor.obj_target(%arg%)% == %self%
  %force% %actor% look %arg%
  return 1
else
  return 0
end
~
#9802
Give rejection~
0 j 100
~
%send% %actor% You can't give items to %self.name%.
return 0
~
#9803
Companion dies permanently~
0 f 100
~
set ch %self.companion%
if %self.is_npc% && %ch% && !%ch.is_npc%
  %ch.remove_companion(%self.vnum%)%
end
~
#9850
Equip imm-only~
1 j 0
~
if !%actor.is_immortal%
  %send% %actor% You can't wear %self.name%.
  return 0
else
  return 1
end
~
#9851
Obj rescale on load~
1 n 100
~
%scale% %self% %self.level%
~
$
