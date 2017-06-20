#1264
Searing Poison Hit~
1 s 100
~
set scale 50
if %actor.ability(Deadly Poisons)%
  set scale 100
end
%send% %actor% &y%target.name% winces in pain as %target.heshe% is burned from the inside by your searing poison!
%send% %target% &rYou wince in pain as you are burned from the inside by %actor.name%'s searing poison!
%echoneither% %target% %actor% %target.name% winces in pain as %target.heshe% is burned from the inside by %actor.name%'s searing poison!
%damage% %target% %scale% poison
~
$
