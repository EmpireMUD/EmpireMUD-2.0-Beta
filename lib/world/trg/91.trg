#9103
Deprecated command trigger~
0 c 0 4
L f 9104
L f 9600
L f 9601
L f 9604
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
0 k 100 6
L f 9600
L f 9601
L f 9604
L w 9103
L w 9108
L w 9602
~
* Requires scripts 9600, 9601, and 9604. Optionally add 9108 for constrict damage.
if %self.cooldown(9103)%
  halt
end
* prevent normal hit
return 0
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
if %target.affect(9602)%
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
scfight setup struggle %target% 30
set scf_strug_char You struggle, but fail to break free.
set scf_strug_room ~%%actor%% struggles to break free!
set scf_free_char You squirm out of the snake's coils!
set scf_free_room ~%%actor%% squirms out of the snake's coils!
remote scf_strug_char %target.id%
remote scf_strug_room %target.id%
remote scf_free_char %target.id%
remote scf_free_room %target.id%
* and stun me
dg_affect #9108 %self% HARD-STUNNED on 30
~
#9105
Snake: Venom~
0 k 100 1
L w 9105
~
if %actor.has_tech(!Poison)% || !%hit%
  halt
end
%dot% #9105 %actor% 100 15 poison 5
~
#9106
Jungle Bird Animation~
0 bw 20 0
~
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
0 d 0 0
*~
set last_phrase %speech%
remote last_phrase %self.id%
~
#9108
Snake: Constrict Damage (requires 9104, 9600, 9601, 9604)~
0 bw 100 6
L f 9104
L f 9600
L f 9601
L f 9604
L w 9108
L w 9602
~
* Deals damage to constricted players every 6-7 seconds
* This pairs with scripts 9104 to add damage to the struggle.
set times 2
while %times% > 0
  set any 0
  set ch %self.room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch% != %self% && %ch.affect(9602)%
      * being constricted
      %send% %ch% &&G**** &&Z|%self% grip tightens around you, slowly crushing you! ****&&0 (struggle)
      %echoaround% %ch% ~%ch% cracks as &%ch%'s slowly crushed by ~%self%.
      set any 1
      %damage% %ch% 200 direct
    end
    set ch %next_ch%
  done
  if !%any%
    dg_affect #9108 %self% off
  end
  eval times %times% - 1
  wait 6 s
done
~
#9117
Animal Becomes Hidden Over Time~
0 ab 20 1
L f 9117
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
0 e 1 1
L f 9118
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
0 l 20 0
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
0 k 100 1
L w 9131
~
if %actor.has_tech(!Poison)% || !%hit%
  halt
end
%dot% #9131 %actor% 100 15 poison 5
~
#9133
Great Horned Owl Animation~
0 bw 3 0
~
* This script is no longer used. It was replaced by custom strings.
if (%random.2% == 1)
  %echo% ~%self% hoots loudly.
else
  %echo% ~%self% dives, then takes to the air again with a mouse held in ^%self% talons.
end
~
#9148
Songbird Animation~
0 bw 3 0
~
* This script is no longer used. It was replaced by custom strings.
if ((%self.room.sector% /= Forest) || (%self.room.sector% /= Orchard))
  %echo% ~%self% Sings sweetly from a near by tree.
end
~
#9150
woodpecker animation~
0 bw 3 0
~
* This script is no longer used. It was replaced by custom strings.
if ((%self.room.sector% /= Forest) || (%self.room.sector% /= Orchard))
  %echo% ~%self% hammers into a tree with its beak, looking for food.
end
~
#9183
Penguin Kill Tracker~
0 f 100 1
L b 9187
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
0 bw 10 0
~
%echo% ~%self% vanishes down a hole.
%purge% %self%
~
#9198
Critter Flutters Off~
0 bw 10 0
~
%echo% ~%self% flutters off.
%purge% %self%
~
$
