#9103
Snake Constrict: Struggle~
0 c 0
struggle~
set break_free_at 1
if !%actor.affect(9104)%
  return 0
  halt
end
if !%actor.varexists(struggle_counter)%
  set struggle_counter 0
  remote struggle_counter %actor.id%
else
  set struggle_counter %actor.struggle_counter%
end
eval struggle_counter %struggle_counter% + 1
if %struggle_counter% >= %break_free_at%
  %send% %actor% You break free!
  %echoaround% %actor% %actor.name% breaks free!
  dg_affect #9104 %actor% off
  rdelete struggle_counter %actor.id%
  halt
else
  %send% %actor% You struggle, but fail to break free.
  %echoaround% %actor% %actor.name% struggles to break free!
  remote struggle_counter %actor.id%
  halt
end
~
#9104
Snake: Constrict~
0 k 100
~
if %self.cooldown(9103)%
  halt
end
* Find a non-bound target
set target %actor%
set person %self.room.people%
set target_found 0
set no_targets 0
while %target.affect(9104)% && %person%
  if %person.is_pc% && %person.is_enemy(%self%)%
    set target %person%
  end
  set person %person.next_in_room%
done
if !%target%
  * Sanity check
  halt
end
if %target.affect(9104)%
  * No valid targets
  halt
end
nop %self.cooldown(9103, 20)%
* Valid target found, start attack
%send% %target% %self.name% starts to wrap around you...
%echoaround% %target% %self.name% starts to wrap around %target.name%...
wait 3 sec
if (!%target% || %target.room% != %self.room%)
  halt
end
%send% %target% %self.name% squeezes around you, constricting until you cannot move!
%echoaround% %target% %self.name% constricts around %target.name%!
%send% %target% Type 'struggle' to break free!
dg_affect #9104 %actor% STUNNED on 20
~
#9105
Snake: Venom~
0 k 100
~
if %actor.has_tech(!Poison)%
  halt
end
%dot% #9105 %actor% 100 15 poison 5
~
#9106
Jungle Bird Animation~
0 bw 20
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
      %echo% %self.name% says, Squawk! '%self.last_phrase%' Squawk!
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
#9117
Animal Becomes Hidden Over Time~
0 ab 20
~
if %self.fighting% || %self.disabled%
  halt
end
hide
~
#9118
Mob Becomes Hostile on Interaction~
0 e 1
you~
* Mob becomes hostile after a player pays attention to it.
if %actor.is_npc%
  halt
end
wait 2 sec
nop %self.add_mob_flag(AGGR)%
detach 9118 %self.id%
~
#9121
Wimpy Flee~
0 l 20
~
if %self.disabled% || %self.aff_flagged(ENTANGLED)%
  halt
end
if %random.3% == 3
  fleet
end
~
#9131
Scorpion: Venom~
0 k 100
~
if %actor.has_tech(!Poison)%
  halt
end
%dot% #9131 %actor% 100 15 poison 5
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
if ((%self.room.sector% /= Forest) || (%self.room.sector% /= Orchard))
  %echo% %self.name% Sings sweetly from a near by tree.
end
~
#9150
woodpecker animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Woodpecker Animation (9150)
if ((%self.room.sector% /= Forest) || (%self.room.sector% /= Orchard))
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
  set penguins_killed 1
end
if %penguins_killed% > 5 && %random.2% == 2
  %load% mob 9187
  %echo% Suddenly, the Emperor of Penguins appears!
  * reset it
  set penguins_killed 0
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
