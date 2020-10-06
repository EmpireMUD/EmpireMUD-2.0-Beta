#1215
Quaff bileberry juice~
1 s 100
~
* We warned you.
%dot% #1215 %actor% 50 120
~
#1250
Basic Healing Potion~
1 s 100
~
%heal% %actor% health 100
~
#1252
Basic Move Potion~
1 s 100
~
%heal% %actor% move 100
~
#1254
Basic Mana Potion~
1 s 100
~
%heal% %actor% mana 100
~
#1264
Searing Poison Hit~
1 s 100
~
set scale 50
if %actor.has_tech(Poison-Upgrade)%
  set scale 100
end
%send% %actor% &&y~%target% winces in pain as &%target% is burned from the inside by your searing poison!
%send% %target% &&rYou wince in pain as you are burned from the inside by |%actor% searing poison!
%echoneither% %target% %actor% ~%target% winces in pain as &%target% is burned from the inside by |%actor% searing poison!
%damage% %target% %scale% poison
~
#1273
Pain Poison Hit~
1 s 100
~
set scale 50
if %actor.has_tech(Poison-Upgrade)%
  set scale 100
end
%send% %actor% &&y~%target% looks nauseous as &%target% is burned from the inside by your painful poison!
%send% %target% &&rYou feel nauseous as you are burned from the inside by |%actor% painful poison!
%echoneither% %target% %actor% ~%target% looks nauseous as &%target% is burned from the inside by |%actor% painful poison!
%dot% #1273 %target% %scale% 30 poison 5
~
$
