#121
Start Handle Tutorial~
2 u 0
~
* start flint tutorial
if !%actor.completed_quest(100)% && !%actor.on_quest(100)%
  %quest% %actor% start 100
end
~
#130
Water Tutorial: Detect fill~
1 c 2
fill~
return 0
set prev %self.val1%
wait 1
if %self.val1% > %prev% && %actor.on_quest(130)%
  %quest% %actor% trigger 130
  %send% %actor% You've filled @%self%! Type 'finish Water Tutorial' to complete the quest.
end
~
$
