#5511
Sorcery Tower: Study Sorcery~
2 c 0
study~
if !%arg%
  %send% %actor% Study what? (try 'study sorcery')
  halt
end
if !(sorcery /= %arg%)
  return 0
  halt
end
if !%actor.canuseroom_guest(%room%)%
  %send% %actor% You don't have permission to study here!
  return 1
  halt
end
if %actor.has_item(5511)%
  %send% %actor% You already have a High Sorcery textbook.
  return 1
  halt
end
if %room.bld_flagged(IN-CITY-ONLY)% && !%room.in_city(true)%
  %send% %actor% This Tower of Sorcery is not in a city, so you can't study at it.
  return 1
  halt
end
%load% obj 5511 %actor% inv
set item %actor.inventory(5511)%
%send% %actor% You pick up %item.shortdesc%. Reading this would surely start you on the path of High Sorcery.
%echoaround% %actor% %actor.name% takes %item.shortdesc%.
~
#5512
High Sorcery skillbook~
1 t 100
~
set room %actor.room%
if !%self.is_flagged(ENCHANTED)%
  halt
end
if %self.room.building_vnum% != 5511
  %send% %actor% %self.shortdesc% suddenly snaps shut before you can finish reading!
  %send% %actor% (You must be at the top of a Tower of Sorcery to gain High Sorcery from it.)
  halt
end
if %room.bld_flagged(IN-CITY-ONLY)% && !%room.in_city(true)%
  %send% %actor% %self.shortdesc% suddenly snaps shut before you can finish reading!
  %send% %actor% (This Tower of Sorcery doesn't work correctly when it's not in a city.)
  halt
end
if %actor.can_gain_new_skills% && %actor.skill(High Sorcery)% == 0 && !%actor.noskill(High Sorcery)%
  %send% %actor% &mYour mind begins to open to the ways of High Sorcery, and you are now an apprentice to this school.&0
  nop %actor.gain_skill(High Sorcery,1)%
end
~
#5513
Block disenchant~
1 p 100
~
if %ability% == 180
  %send% %actor% You can't disenchant that.
  return 0
  halt
end
~
$
