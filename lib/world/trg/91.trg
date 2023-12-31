#9103
Deprecated command trigger~
0 c 0
struggle~
* This was a helper for 9104 Snake: Constrict before b5.165
* DEPRECATED: replace with new version instead
if !%self.has_trigger(9104)%
  attach 9104 %self.id%
end
if !%self.has_trigger(9600)%
  attach 9600 %self.id%
end
if !%self.has_trigger(9601)%
  attach 9601 %self.id%
end
if !%self.has_trigger(9604)%
  attach 9604 %self.id%
end
* repeat the struggle attempt
return 1
%force% %actor% struggle
* and remove this trigger
detach 9104 %self.id%
~
#9104
Snake: Constrict (requires 9600, 9601, 9604)~
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
while %target.affect(9602)% && %person%
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
* Valid target found, start attack
scfight lockout 9103 20 25
scfight clear struggle
%echo% ~%self% starts to wrap around ~%target%...
set verify_target %target.id%
wait 3 sec
if %target.id% != %verify_target%
  %echo% ~%self% releases the newly strangled corpse!
  halt
end
if (!%target% || %target.room% != %self.room%)
  halt
end
%send% %target% &&G**** &&Z~%self% squeezes around you, constricting until you cannot move! ****&&0 (struggle)
%echoaround% %target% &&G~%self% constricts around ~%target%!&&0
scfight setup struggle %target% 15
set scf_strug_char You struggle, but fail to break free.
set scf_strug_room ~%%actor%% struggles to break free!
set scf_free_char You squirm out of the snake's coils!
set scf_free_room ~%%actor%% squirms out of the snake's coils!
remote scf_strug_char %target.id%
remote scf_strug_room %target.id%
remote scf_free_char %target.id%
remote scf_free_room %target.id%
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
    %echo% ~%self% flies up and away, disappearing into the distance.
    %purge% %self%
  break
  case 2
    %echo% ~%self% squawks loudly.
  break
  case 3
    if (%self.varexists(last_phrase)%)
      %echo% ~%self% says, Squawk! '%self.last_phrase%' Squawk!
    end
  break
  default
    %echo% ~%self% ruffles ^%self% feathers.
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
eval times_hidden %self.var(times_hidden,0)% + 1
if %times_hidden% > 50
  * stop trying to hide -- this would otherwise run forever on some mobs
  detach 9117 %self.id%
else
  * store for next time
  remote times_hidden %self.id%
end
* try to hide
if !%self.fighting% && !%self.disabled%
  hide
end
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
if %self.disabled% || %self.aff_flagged(IMMOBILIZED)%
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
  %echo% ~%self% hoots loudly.
else
  %echo% ~%self% dives, then takes to the air again with a mouse held in ^%self% talons.
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
  %echo% ~%self% Sings sweetly from a near by tree.
end
~
#9150
woodpecker animation~
0 bw 3
~
* This script is no longer used. It was replaced by custom strings.
* Woodpecker Animation (9150)
if ((%self.room.sector% /= Forest) || (%self.room.sector% /= Orchard))
  %echo% ~%self% hammers into a tree with its beak, looking for food.
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
%echo% ~%self% vanishes down a hole.
%purge% %self%
~
#9198
Critter Flutters Off~
0 bw 10
~
%echo% ~%self% flutters off.
%purge% %self%
~
$
