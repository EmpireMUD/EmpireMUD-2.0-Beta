#9106
Jungle Bird Animation~
0 bw 3
~
* Jungle Bird Animation (9106)
switch (%random.8%)
  case 1
    %echo% %self.name% flies up and away, disappearing into the distance.
    %purge% %self%
  break
  case 2
    %echo% %self.name% squawks loudly.
  break
  case 3
    if (%self.varexists(last_phrase)%)
      %echo% %self.name% says, *squawk '%self.last_phrase%' *squawk*
    end
  break
  default
    %echo% %self.name% ruffles %self.hisher% feathers.
  break
done
~
#9107
Jungle Bird Speech~
0 d 0
*~
set last_phrase %speech%
remote last_phrase %self.id%
~
#9133
Great Horned Owl Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Great Horned Owl Animation (9133)
if (%random.2% == 1)
  %echo% %self.name% hoots loudly.
else
  %echo% %self.name% dives, then takes to the air again with a mouse held in %self.hisher% talons.
end
~
#9148
Songbird Animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* songbird Animation (9148)
* Works for 148 and 149
eval room %self.room%
if ((%room.sector% /= Forest) || (%room.sector% /= Orchard))
  %echo% %self.name% Sings sweetly from a near by tree.
end
~
#9150
woodpecker animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Woodpecker Animation (9150)
eval room %self.room%
if ((%room.sector% /= Forest) || (%room.sector% /= Orchard))
  %echo% %self.name% hammers into a tree with its beak, looking for food.
end
~
#9183
Penguin Kill Tracker~
0 f 100
~
if !%actor.is_pc%
  halt
end
if %actor.varexists(penguins_killed)%
  eval penguins_killed %actor.penguins_killed% + 1
else
  eval penguins_killed 1
end
if %penguins_killed% > 5 && %random.2% == 2
  %load% mob 9187
  %echo% Suddenly, the Emperor of Penguins appears!
  * reset it
  eval penguins_killed 0
end
remote penguins_killed %actor.id%
~
#9188
Tiny Critter Despawn~
0 bw 10
~
%echo% %self.name% vanishes down a hole.
%purge% %self%
~
#9198
Critter Flutters Off~
0 bw 10
~
%echo% %self.name% flutters off.
%purge% %self%
~
$
