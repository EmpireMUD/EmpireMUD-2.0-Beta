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
