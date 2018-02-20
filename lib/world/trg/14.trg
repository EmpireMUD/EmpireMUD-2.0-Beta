#1400
Paint Quest: Alchemist Research~
1 c 2
research~
if !%actor.on_quest(1400)%
  return 0
  halt
end
if !%actor.room.function(ALCHEMIST)%
  return 0
  halt
end
if !%actor.canuseroom_guest()%
  %send% %actor% You don't have permission to research here.
  return 1
  halt
end
%quest% %actor% trigger 1400
if %actor.quest_finished(1400)%
  %quest% %actor% finish 1400
end
~
#1401
Paint Quests: Starting Items~
2 u 0
~
switch %questvnum%
  case 1400
    * Research Notes (Alchemist)
    %load% obj 1499 %actor% inv
  break
  case 1402
    * Experimental Paint Recipe
    %load% obj 1497 %actor% inv
  break
  case 1403
    * ?
  break
  case 1404
    * Research Notes (Library)
    %load% obj 1496 %actor% inv
  break
  case 1406
    * ?
  break
done
~
#1404
Paint Quest: Library Research~
1 c 2
research~
if !%actor.on_quest(1404)%
  return 0
  halt
end
if !%actor.room.function(LIBRARY)%
  return 0
  halt
end
if !%actor.canuseroom_guest()%
  %send% %actor% You don't have permission to research here.
  return 1
  halt
end
%quest% %actor% trigger 1404
if %actor.quest_finished(1404)%
  %quest% %actor% finish 1404
end
~
#1496
Teach secondary paint recipes~
2 v 0
~
nop %actor.add_learned(1406)%
nop %actor.add_learned(1408)%
nop %actor.add_learned(1410)%
~
#1497
Experimental Paint quest finish~
2 v 0
~
rdelete painting_level %actor.id%
nop %actor.add_learned(1400)%
nop %actor.add_learned(1402)%
nop %actor.add_learned(1404)%
~
#1498
Experimental Paint consume~
1 s 100
~
* Does not block use
return 1
if !%actor.varexists(painting_level)%
  set painting_level 0
  remote painting_level %actor.id%
end
set painting_level %actor.painting_level%
if %painting_level% < 2
  * Random color
  eval new_color 1 + %random.12%
  * Advance progress tracker
  eval painting_level %painting_level% + 1
  remote painting_level %actor.id%
else
  * Blue
  set new_color 1
  * Quest can be completed
  %quest% %actor% trigger 1402
end
nop %self.val0(%new_color%)%
~
$
