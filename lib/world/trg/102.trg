#10200
Goblin Challenge Must Fight~
0 s 100
~
%send% %actor% You have begun the Goblin Challenge and cannot leave without fighting ~%self%.
return 0
~
#10201
Goblin Challenge No Flee~
0 c 0
flee~
%send% %actor% You cannot flee the Goblin Challenge!
~
#10202
GC Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10200)%
end
~
#10203
Goblin Challenge: Better error message when attacking early~
0 B 0
~
if %self.aff_flagged(!ATTACK)%
  %send% %actor% You'll have to wait a moment. ~%self% is still getting ready.
  return 0
else
  detach 10203 %self.id%
  return 1
end
~
#10204
Zelkab Bruiser Combat~
0 k 10
~
switch %random.2%
  case 1
    kick
  break
  case 2
    * Stun
    %send% %actor% Zelkab bashes you in the head with his club!
    %echoaround% %actor% Zelkab bashes ~%actor% in the head with his club!
    dg_affect %actor% STUNNED on 10
    %damage% %actor% 50 physical
  break
done
~
#10205
Zelkab Death~
0 f 100
~
if %self.mob_flagged(NO-CORPSE)% || !%self.varexists(difficulty)%
  * This is probably a summoned copy.
  halt
end
return 0
set difficulty %self.difficulty%
%load% mob 10201
set mob %self.room.people%
remote difficulty %mob.id%
set mob_diff %difficulty%
if %mob.vnum% >= 10204 && %mob.vnum% <= 10205
  eval mob_diff %mob_diff% + 1
end
dg_affect %mob% !ATTACK on 5
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %mob_diff% == 1
  * Then we don't need to do anything
elseif %mob_diff% == 2
  nop %mob.add_mob_flag(HARD)%
elseif %mob_diff% == 3
  nop %mob.add_mob_flag(GROUP)%
elseif %mob_diff% == 4
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
%scale% %mob% %mob.level%
set self_name %self.alias%
set self_name %self_name.car%
set mob_name %mob.alias%
set mob_name %mob_name.car%
%echo% As %self_name% dies, %mob_name% steps into the nest!
~
#10206
Garlgarl Shaman Combat~
0 k 10
~
switch %random.4%
  case 1
    * Flame wave AoE
    %echo% Garlgarl begins spinning spirals of flame...
    * Give the healer (if any) time to prepare for group heals
    wait 3 sec
    %echo% Garlgarl unleashes a flame spiral!
    %aoe% 50 fire
  break
  default
    * Goblinfire
    %send% %actor% Garlgarl splashes goblinfire at you, causing serious burns!
    %echoaround% %actor% Garlgarl splashes goblinfire at ~%actor%, causing serious burns!
    %dot% %actor% 33 15 fire
    %damage% %actor% 50 fire
  break
done
~
#10207
Garlgarl Death~
0 f 100
~
if %self.mob_flagged(NO-CORPSE)% || !%self.varexists(difficulty)%
  * This is probably a summoned copy.
  halt
end
return 0
set difficulty %self.difficulty%
set mob_diff %difficulty%
if %mob.vnum% >= 10204 && %mob.vnum% <= 10205
  eval mob_diff %mob_diff% + 1
end
set mob_num 10202
while %mob_num% <= 10203
  %load% mob %mob_num%
  set mob %self.room.people%
  remote difficulty %mob.id%
  dg_affect %mob% !ATTACK on 5
  nop %mob.remove_mob_flag(HARD)%
  nop %mob.remove_mob_flag(GROUP)%
  if %mob_diff% == 1
    * Then we don't need to do anything
  elseif %mob_diff% == 2
    nop %mob.add_mob_flag(HARD)%
  elseif %mob_diff% == 3
    nop %mob.add_mob_flag(GROUP)%
  elseif %mob_diff% == 4
    nop %mob.add_mob_flag(HARD)%
    nop %mob.add_mob_flag(GROUP)%
  end
  %scale% %mob% %mob.level%
  set self_name %self.alias%
  set self_name %self_name.car%
  eval mob_num %mob_num% + 1
done
%echo% As %self_name% dies, Filks and Walts step into the nest!
~
#10208
Filks Archer Combat~
0 k 10
~
%send% %actor% Filks dashes backwards, draws her bow, and shoots you with a poison arrow!
%echoaround% %actor% Filks dashes backwards, draws her bow, and shoots ~%actor% with a poison arrow!
%dot% %actor% 75 15 poison
%damage% %actor% 50 physical
~
#10209
Filks Death~
0 f 100
~
if %self.mob_flagged(NO-CORPSE)% || !%self.varexists(difficulty)%
  * This is probably a summoned copy.
  halt
end
set ch %self.room.people%
set found 0
while %ch% && !%found%
  if (%ch.vnum% == 10203)
    set found 1
  end
  set ch %ch.next_in_room%
done
if !%found%
  return 0
  set difficulty %self.difficulty%
  %load% mob 10204
  set mob %self.room.people%
  remote difficulty %mob.id%
  set mob_diff %difficulty%
  if %mob.vnum% >= 10204 && %mob.vnum% <= 10205
    eval mob_diff %mob_diff% + 1
  end
  dg_affect %mob% !ATTACK on 5
  nop %mob.remove_mob_flag(HARD)%
  nop %mob.remove_mob_flag(GROUP)%
  if %mob_diff% == 1
    * Then we don't need to do anything
  elseif %mob_diff% == 2
    nop %mob.add_mob_flag(HARD)%
  elseif %mob_diff% == 3
    nop %mob.add_mob_flag(GROUP)%
  elseif %mob_diff% == 4
    nop %mob.add_mob_flag(HARD)%
    nop %mob.add_mob_flag(GROUP)%
  end
  set self_name %self.alias%
  %scale% %mob% %mob.level%
  set self_name %self_name.car%
  set mob_name %mob.alias%
  set mob_name %mob_name.car%
  %echo% As %self_name% dies, %mob_name% steps into the nest!
end
~
#10210
Walts Sapper Combat~
0 k 8
~
%echo% Walts runs to the edge of the nest, pulls out a bomb, and hurls it at you!
%aoe% 100 physical
~
#10211
Walts Death~
0 f 100
~
if %self.mob_flagged(NO-CORPSE)% || !%self.varexists(difficulty)%
  * This is probably a summoned copy.
  halt
end
set ch %self.room.people%
set found 0
while %ch% && !%found%
  if (%ch.vnum% == 10202)
    set found 1
  end
  set ch %ch.next_in_room%
done
if !%found%
  return 0
  set difficulty %self.difficulty%
  %load% mob 10204
  set mob %self.room.people%
  remote difficulty %mob.id%
  set mob_diff %difficulty%
  if %mob.vnum% >= 10204 && %mob.vnum% <= 10205
    eval mob_diff %mob_diff% + 1
  end
  dg_affect %mob% !ATTACK on 5
  nop %mob.remove_mob_flag(HARD)%
  nop %mob.remove_mob_flag(GROUP)%
  if %mob_diff% == 1
    * Then we don't need to do anything
  elseif %mob_diff% == 2
    nop %mob.add_mob_flag(HARD)%
  elseif %mob_diff% == 3
    nop %mob.add_mob_flag(GROUP)%
  elseif %mob_diff% == 4
    nop %mob.add_mob_flag(HARD)%
    nop %mob.add_mob_flag(GROUP)%
  end
  %scale% %mob% %mob.level%
  set self_name %self.alias%
  set self_name %self_name.car%
  set mob_name %mob.alias%
  set mob_name %mob_name.car%
  %echo% As %self_name% dies, %mob_name% steps into the nest!
end
~
#10212
Nilbog Champion Combat~
0 k 8
~
switch %random.3%
  case 1
    bash
  break
  case 2
    disarm
  break
  case 3
    * Axe spin hits all
    %echo% Nilbog begins spinning around the room with her axe!
    wait 3 sec
    %echo% Nilbog's axe spin hits everyone!
    %aoe% 75 physical
  break
done
~
#10213
Nilbog Death~
0 f 100
~
if %self.mob_flagged(NO-CORPSE)% || !%self.varexists(difficulty)%
  * This is probably a summoned copy.
  halt
end
return 0
set difficulty %self.difficulty%
%load% mob 10205
set mob %self.room.people%
remote difficulty %mob.id%
set mob_diff %difficulty%
if %mob.vnum% >= 10204 && %mob.vnum% <= 10205
  eval mob_diff %mob_diff% + 1
end
dg_affect %mob% !ATTACK on 5
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %mob_diff% == 1
  * Then we don't need to do anything
elseif %mob_diff% == 2
  nop %mob.add_mob_flag(HARD)%
elseif %mob_diff% == 3
  nop %mob.add_mob_flag(GROUP)%
elseif %mob_diff% == 4
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
%scale% %mob% %mob.level%
set self_name %self.alias%
set self_name %self_name.car%
set mob_name %mob.alias%
set mob_name %mob_name.car%
%echo% As %self_name% dies, %mob_name% steps into the nest!
~
#10214
Furl War Shaman Combat~
0 k 10
~
switch %random.4%
  case 1
    slow
  break
  case 2
    hasten
  break
  case 3
    lightningbolt
  break
  case 4
    skybrand
  break
done
~
#10216
Filks Respawn~
0 b 100
~
* Respawns Walts if needed
if (%self.fighting% || %self.disabled%)
  halt
end
set ch %self.room.people%
set found 0
while %ch% && !%found%
  if (%ch.is_npc% && %ch.vnum% == 10203)
    set found 1
  end
  set ch %ch.next_in_room%
done
if (!%found%)
  %load% mob 10203
  makeuid walts mob walts
  if %walts%
    %echo% ~%walts% respawns.
    nop %walts.add_mob_flag(!LOOT)%
  end
end
~
#10217
Walts Respawn~
0 b 100
~
* Respawns Filks if needed
if (%self.fighting% || %self.disabled%)
  halt
end
set ch %self.room.people%
set found 0
while (%ch% && !%found%)
  if (%ch.vnum% == 10202)
    set found 1
  end
  set ch %ch.next_in_room%
done
if (!%found%)
  %load% mob 10202
  makeuid filks mob filks
  if %filks%
    %echo% ~%filks% respawns.
    nop %filks.add_mob_flag(!LOOT)%
  end
end
~
#10218
Filks and Walts respawn~
2 b 100
~
set filks_present 0
set walts_present 0
set fighting 0
set person %room.people%
while %person%
  if %person.vnum% == 10202
    if %person.fighting% || %person.mob_flagged(NO-CORPSE)%
      set fighting 1
    end
    set goblin %person%
    set filks_present 1
  elseif %person.vnum% == 10203
    if %person.fighting% || %person.mob_flagged(NO-CORPSE)%
      set fighting 1
    end
    set walts_present 1
    set goblin %person%
  end
  set person %person.next_in_room%
done
if %filks_present% && !%walts_present% && !%fighting%
  * Respawn Walts
  %load% mob 10203
  set new_mob %room.people%
  if %new_mob.vnum% == 10203
    %echo% ~%new_mob% respawns.
    nop %new_mob.add_mob_flag(!LOOT)%
  end
elseif %walts_present% && !%filks_present% && !%fighting%
  * Respawn Filks
  %load% mob 10202
  set new_mob %room.people%
  if %new_mob.vnum% == 10202
    %echo% ~%new_mob% respawns.
    nop %new_mob.add_mob_flag(!LOOT)%
  end
end
if %new_mob%
  set difficulty %goblin.difficulty%
  remote difficulty %new_mob.id%
  set mob_diff %difficulty%
  if %mob.vnum% >= 10204 && %mob.vnum% <= 10205
    eval mob_diff %mob_diff% + 1
  end
  nop %new_mob.remove_mob_flag(HARD)%
  nop %new_mob.remove_mob_flag(GROUP)%
  if %mob_diff% == 1
    * Then we don't need to do anything
  elseif %mob_diff% == 2
    nop %new_mob.add_mob_flag(HARD)%
  elseif %mob_diff% == 3
    nop %new_mob.add_mob_flag(GROUP)%
  elseif %mob_diff% == 4
    nop %new_mob.add_mob_flag(HARD)%
    nop %new_mob.add_mob_flag(GROUP)%
  end
end
~
#10225
Druid greeting~
0 g 100
~
wait 5
%send% %actor% ~%self% greets you warmly.
* only if they don't have one
if (%actor.has_item(10233)% || %actor.has_item(10234)% || %actor.has_item(10235)% || %actor.has_item(10236)% || %actor.has_item(10237)%)
  halt
end
wait 5
%send% %actor% ~%self% tells you, 'Here, take this. I could use a hand here.'
%send% %actor% ~%self% hands you a list.
%load% obj 10237 %actor% inv
~
#10226
Druid Offer~
0 c 0
offer~
if (%actor.has_resources(3002,4)% && %actor.has_resources(3004,4)% && %actor.has_resources(3008,4)% && %actor.has_resources(3010,4)%)
  * fruits
  nop %actor.add_resources(3002,-4)%
  nop %actor.add_resources(3004,-4)%
  nop %actor.add_resources(3008,-4)%
  nop %actor.add_resources(3010,-4)%
  %load% obj 10233 %actor% inv
  %send% %actor% You give ~%self% the fruits you have collected, and &%self% gives you a wooden fruit token!
  %echoaround% %actor% ~%actor% gives ~%self% several baskets of fruits, and receives a wooden fruit token.
  if !%actor.has_item(10238)%
    %load% obj 10238 %actor% inv
    %send% %actor% &%self% also gives you another list.
  end
elseif (%actor.has_resources(141,4)% && %actor.has_resources(3005,4)% && %actor.has_resources(3011,4)% && %actor.has_resources(145,4)%)
  * grains
  nop %actor.add_resources(141,-4)%
  nop %actor.add_resources(3005,-4)%
  nop %actor.add_resources(3011,-4)%
  nop %actor.add_resources(145,-4)%
  %load% obj 10234 %actor% inv
  %send% %actor% You give ~%self% the grains you have collected, and &%self% gives you a copper grain token!
  %echoaround% %actor% ~%actor% gives ~%self% several baskets of grains, and receives a copper grain token.
  if !%actor.has_item(10239)%
    %load% obj 10239 %actor% inv
    %send% %actor% &%self% also gives you another list.
  end
elseif (%actor.has_resources(143,4)% && %actor.has_resources(144,4)% && %actor.has_resources(3023,4)% && %actor.has_resources(3019,4)%)
  * tradegoods
  nop %actor.add_resources(143,-4)%
  nop %actor.add_resources(144,-4)%
  nop %actor.add_resources(3023,-4)%
  nop %actor.add_resources(3019,-4)%
  %load% obj 10235 %actor% inv
  %send% %actor% You give ~%self% the goods you have collected, and &%self% gives you a silver tradegoods token!
  %echoaround% %actor% ~%actor% gives ~%self% several baskets of goods, and receives a silver tradegoods token.
else
  %send% %actor% You have nothing to offer.
  wait 5
  if !%actor.has_item(10237)%
    %load% obj 10237 %actor% inv
    %send% %actor% ~%self% hands you a list.
  end
end
~
#10227
M:HG Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10225)%
end
~
#10232
Tumbleweed mount spawn~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
%load% m 10227
%echo% The enchanted tumbleweed comes to life!
set mob %self.room.people%
if (%mob% && %mob.vnum% == 10227)
  nop %mob.unlink_instance%
end
%purge% %self%
~
#10233
Gardener passive~
0 bw 15
~
if %self.varexists(msg_pos)%
  eval msg_pos %self.msg_pos% + 1
else
  set msg_pos 1
end
switch %msg_pos%
  case 1
    %echo% ~%self% carefully waters the plants.
  break
  case 2
    %echo% ~%self% trims unwanted growth from the plants.
  break
  case 3
    %echo% ~%self% trims unwanted growth from the plants.
  break
  case 4
    %echo% ~%self% whistles a strange tune.
  break
done
if %msg_pos% >= 4
  set msg_pos 0
end
remote msg_pos %self.id%
~
#10235
Hidden Token combine~
1 c 2
combine~
if %actor.position% != Standing
  return 0
  halt
end
if !(tokens /= %arg%)
  return 0
  halt
end
set copper_token %actor.inventory(10234)%
if !%copper_token
  %send% %actor% You require a copper grain token to combine.
  halt
end
set wooden_token %actor.inventory(10233)%
if !%wooden_token%
  %send% %actor% You require a wooden fruit token to combine.
  halt
end
%purge% %wooden_token%
%purge% %copper_token%
%send% %actor% You magically combine all three tokens into a gnarled wooden token!
%echoaround% %actor% ~%actor% magically combines three strange tokens into a gnarled wooden token!
%load% obj 10236 %actor% inv
%purge% %self%
~
#10236
Hidden Garden teleport chant~
1 c 2
chant~
set room %self.room%
* Only know the 'gardens' chant if the have the token.
if !(gardens /= %arg%)
  return 0
  halt
end
if %actor.position% != Standing
  return 0
  halt
end
if (%room.template% != 10225)
  %send% %actor% You can only use that chant in the entrance room of the Hidden Garden adventure.
  halt
end
%send% %actor% You begin the chant of gardens...
%echoaround% %actor% ~%actor% begins the chant of gardens...
wait 4 sec
if !(%self.carried_by% == %actor%) || %room% != %self.room%
  halt
end
%send% %actor% You rhythmically speak the words to the chant of gardens...
%echoaround% %actor% ~%actor% rhythmically speaks the words to the chant of gardens...
wait 4 sec
if !(%self.carried_by% == %actor%) || %room% != %self.room%
  halt
end
%echoaround% %actor% ~%actor% vanishes in a swirl of dust!
%teleport% %actor% i10226
%echoaround% %actor% ~%actor% appears in a swirl of dust!
%force% %actor% look
~
#10250
Chronomancer intro~
0 g 100
~
* Only care about portal entries
if %direction% != none
  halt
end
wait 5
say Oh great, more tourists. Listen, this isn't just some fancy jungle getaway.
wait 2 sec
say And that wasn't any ordinary portal. You've traveled back in time.
wait 1 sec
say This is the primeval jungle, completely unconquered. There are creatures here that could swallow the likes of you in one bite.
wait 1 sec
say Just watch out for the huge, primitive dragons that walk these parts. They aren't friendly.
wait 3 sec
say Oh, and watch out for Archsorcerer Malfernes. I think the chroniportation has done something to his brain!
~
#10251
Block chop, give error~
2 c 0
chop~
%send% %actor% A tangle of sharp vines too thick to pass stops you from getting close enough to the trees.
return 1
~
#10252
King of the Dracosaurs grievous bite~
0 k 7
~
%send% %actor% ~%self% takes a grievous bite out of you!
%echoaround% %actor% ~%self% takes a grievous bite out of ~%actor%!
%dot% %actor% 100 30 physical
%damage% %actor% 60 physical
~
#10253
Terrosaur combat~
0 k 5
~
dg_affect %self% BONUS-PHYSICAL 5 120
%echo% ~%self% seems to get angrier!
~
#10254
Malfernes combat~
0 k 7
~
if !%self.affect(foresight)%
  foresight
elseif !%actor.affect(colorburst)%
  colorburst
elseif !%actor.affect(slow)%
  slow
else
  sunshock
end
~
#10255
Primeval Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10250)%
end
~
#10256
Primeval adventure completer~
0 f 100
~
%buildingecho% %self.room% A bone-shattering roar echoes through the air!
%adventurecomplete%
return 0
~
#10257
Dracosaur miniboss spawner~
1 n 100
~
switch %random.3%
  case 1
    %load% mob 10255
  break
  case 2
    %load% mob 10258
  break
  case 3
    %load% mob 10256
    %load% mob 10257
    %load% mob 10257
  break
done
%purge% %self%
~
#10258
Primeval difficulty selector~
0 c 0
difficulty~
return 1
* Configs
set start_room 10250
set end_room 10258
set boss_mobs 10252 10253 10254
set mini_mobs 10255 10258
* NOTE: The miniboss that spawns in a pack is unlisted; it's always Normal.
* Check args...
if %self.varexists(scaled)%
  %send% %actor% The difficulty has already been set.
  halt
end
if !%arg%
  %send% %actor% You must specify a level of difficulty (normal \| hard \| group \| boss).
  halt
end
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set difficulty 1
  set hard_mini 0
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set difficulty 2
  set hard_mini 0
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  set difficulty 3
  set hard_mini 1
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  set difficulty 4
  set hard_mini 1
else
  %send% %actor% That is not a valid difficulty level for this adventure (normal \| hard \| group \| boss).
  halt
end
* Clear existing difficulty flags and set new ones.
set vnum %start_room%
while %vnum% <= %end_room%
  set there %instance.nearest_rmt(%vnum%)%
  if %there%
    set mob %there.people%
    set last 0
    while %mob%
      set next_mob %mob.next_in_room%
      if %mob.is_npc%
        if %mob.vnum% >= 10250 && %mob.vnum% <= 10299
          * wipe difficulty flags
          nop %mob.remove_mob_flag(HARD)%
          nop %mob.remove_mob_flag(GROUP)%
        end
        if %boss_mobs% ~= %mob.vnum%
          * scale as boss
          if %difficulty% == 2
            nop %mob.add_mob_flag(HARD)%
          elseif %difficulty% == 3
            nop %mob.add_mob_flag(GROUP)%
          elseif %difficulty% == 4
            nop %mob.add_mob_flag(HARD)%
            nop %mob.add_mob_flag(GROUP)%
          end
        elseif %hard_mini% && %mini_mobs% ~= %mob.vnum%
          * scale as miniboss
          nop %mob.add_mob_flag(HARD)%
        end
        * on normal, remove duplicate mobs
        if %difficulty% == 1
          if %mob.vnum% == %last%
            %purge% %mob%
          else
            set last %mob.vnum%
          end
        end
      end
      * and loop
      set mob %next_mob%
    done
  end
  * and repeat
  eval vnum %vnum% + 1
done
* messaging:
if %difficulty% > 2
  say Be very careful... the scouts say it's dangerous.
end
say Ok, open the gate!
* mark me as scaled
set scaled 1
remote scaled %self.id%
~
#10259
Priveal block exit until selected~
0 s 100
~
if %direction% != portal && !%actor.nohassle% && !%self.varexists(scaled)%
  %send% %actor% The gate is shut. Choose a difficulty to proceed.
  %send% %actor% Usage: difficulty <normal \| hard \| group \| boss>
  return 0
end
~
#10260
Dracosaur trash spawner~
1 n 100
~
switch %random.3%
  case 1
    %load% mob 10261
  break
  case 2
    %load% mob 10262
  break
  case 3
    %load% mob 10259
    %load% mob 10260
    %load% mob 10260
  break
done
%purge% %self%
~
#10261
Malfernes greet/aggro~
0 g 100
~
if (%self.fighting% || %self.disabled% || %actor.nohassle%)
  halt
end
wait 5
%echo% ~%self% cackles insanely!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%echo% A strange violet glow encircles ~%self%, as if some arcane magic is controlling him.
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
say How dare you follow me here!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
say Did the Academy send you? Are you here to take me back? I'm not going back there!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%aggro% %actor%
~
#10262
Primeval must-fight~
0 s 100
~
if (%actor.nohassle% || %direction% == south)
  halt
end
%send% %actor% You can't seem to get away from ~%self%!
return 0
~
#10263
DEPRECATED: mount whistle use~
1 c 2
use~
* DEPRECATED: Use 9910 instead
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
%load% m %self.val0%
set mob %self.room.people%
%send% %actor% You use @%self% and ~%mob% appears!
%echoaround% %actor% ~%actor% uses @%self% and ~%mob% appears!
if (%mob% && %mob.vnum% == %self.val0%)
  nop %mob.unlink_instance%
end
%purge% %self%
* DEPRECATED: Use 9910 instead
~
#10264
Dracosaur boss spawner~
1 n 100
~
switch %random.3%
  case 1
    %load% mob 10252
  break
  case 2
    %load% mob 10253
  break
  case 3
    %load% mob 10254
  break
done
%purge% %self%
~
#10265
Primeval no-flee~
0 c 0
flee~
%send% %actor% There's nowhere to flee!
return 1
~
#10266
Primeval track ability~
2 c 0
track~
* check abils and arg
if !%arg% || !%actor.ability(Track)% || !%actor.ability(Navigation)%
  * Fail through to ability message
  return 0
  halt
end
* determine who they're tracking from available targets
set target 0
if dracosaur /= %arg%
  set keyword dracosaur
  * ambiguous keyword
  if %instance.mob(10255)%
    set target %instance.mob(10255)%
  elseif %instance.mob(10256)%
    set target %instance.mob(10256)%
  elseif %instance.mob(10257)%
    set target %instance.mob(10257)%
  elseif %instance.mob(10252)%
    * king last (he appears last in the adventure)
    set target %instance.mob(10252)%
  end
elseif king /= %arg%
  set keyword king
  set target %instance.mob(10252)%
elseif terrosaur /= %arg% || fearsome /= %arg%
  set keyword terrosaur
  set target %instance.mob(10253)%
elseif archsorcerer /= %arg% || malfernes /= %arg%
  set keyword malfernes
  set target %instance.mob(10254)%
elseif three-horned /= %arg% || horned /= %arg% || frilled /= %arg%
  set keyword frilled
  set target %instance.mob(10255)%
elseif feathered /= %arg%
  set keyword feathered
  * ambiguous keyword
  if %instance.mob(10256)%
    set target %instance.mob(10256)%
  elseif %instance.mob(10257)%
    set target %instance.mob(10257)%
  end
elseif small /= %arg%
  set keyword small
  set target %instance.mob(10257)%
elseif druid /= %arg%
  set keyword druid
  set target %instance.mob(10258)%
end
* see if that keyword was already tracked here
eval found %%track_%keyword%%%
if !%found%
  if !%target%
    * no target: fall through to normal track
    return 0
    halt
  end
  * ok find it
  set my_id %room.template%
  * It's never south -- that's always backtrack
  set dir_list north east west northeast northwest southeast southwest
  set found 0
  set override 0
  while %dir_list% && !%override%
    set dir %dir_list.car%
    set dir_list %dir_list.cdr%
    eval to_room %%room.%dir%(room)%%
    if %to_room% == %target.room%
      * target is in that room
      set override %dir%
    elseif %to_room% && !%found% && %to_room.template% > %my_id%
      * room has higher template id
      set found %dir%
    end
  done
  if %override%
    set found %override%
  end
end
if !%found%
  * did not find any valid room: fall through to normal track command for error
  return 0
  halt
end
* ok: now we have found a direction
%send% %actor% You find a trail to the %found%!
* save for later
set track_%keyword% %found%
global track_%keyword%
return 1
~
#10267
Primeval base camp track hint~
2 c 0
track~
* shows up after they fail to find tracks
return 0
wait 1
if %actor.room% == %room% && %actor.ability(Track)% && %actor.ability(Navigation)%
  %send% %actor% It might be easier to find tracks further from the base camp.
end
~
#10268
Primeval backtracking track hint~
2 c 0
track~
* when they try to track from the "backtracking" room
return 0
wait 1
if %actor.room% == %room% && %actor.ability(Track)% && %actor.ability(Navigation)%
  %send% %actor% You probably need to go back to the base camp to get your bearings before you can track anything around here.
end
~
#10270
King of the Dracosaurs greet/aggro~
0 g 100
~
if (%self.fighting% || %self.disabled% || %actor.nohassle%)
  halt
end
wait 5
%echo% The earth itself tremors in fear as ~%self% stomps toward you.
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%echo% ~%self% stops and lets out a terrifying roar!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%echo% ~%self% swipes ^%self% massive tail, and a tree goes flying!
%load% obj 120 room
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%echo% ~%self% looms close, and it's only as &%self% bends down to bite you that you realize how big &%self% is!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%aggro% %actor%
~
#10271
Terrosaur greet/aggro~
0 g 100
~
if (%self.fighting% || %self.disabled% || %actor.nohassle%)
  halt
end
wait 5
%echo% A wicked screech pierces the air!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%echo% ~%self% drops the crocodile &%self% was carrying in its talons, and swoops toward you!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%echo% ~%self% vanishes as &%self% swoops too close to the trees for you to see exactly which way &%self%'s coming from.
%load% obj 120
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%echo% ~%self% reappears over the clearing, suddenly so close &%self% blots out the sun!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.room% != %self.room%)
  halt
end
%aggro% %actor%
~
#10272
Primeval item BOE/BOP craft/loot twiddler~
1 n 100
~
* items default to BOP but are set BOE if they come from a shop or craft
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
end
if !%actor%
  halt
end
if %actor% && %actor.is_pc%
  * Item was crafted or bought
  if %self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if !%self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
  * Probably need to unbind when BOE
  nop %self.bind(nobody)%
else
  * Item was probably dropped as loot
  if !%self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if %self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
end
detach 10272 %self.id%
~
#10273
Primeval shop: sell resources to shop~
0 c 0
sell~
* Usage: sell <all | item>
set valid_vnums 10252 10253 10254 10265 10267
set premium_value 10252 10253 10254
return 1
if %arg% == all
  set all 1
  set targ 0
else
  set all 0
  set targ %actor.obj_target_inv(%arg.car%)%
  if !%arg%
    %send% %actor% Sell what? (or try 'sell all')
    halt
  end
end
* ready to loop
set found 0
set item %actor.inventory%
while %item% && (%all% || !%found%)
  set next_item %item.next_in_list%
  * use %ok% to control what we do in this loop
  if %all%
    set ok 1
  else
    * single-target: make sure this was the target
    if %targ% == %item%
      set ok 1
    else
      set ok 0
    end
  end
  * next check the obj type if we got the ok
  if %ok%
    if !(%valid_vnums% ~= %item.vnum%)
      if %all%
        set ok 0
      else
        %send% %actor% You can't sell @%item% here.
        * Break out of the loop early since it was a single-target fail
        halt
      end
    end
  end
  * still ok? go ahead and sell it.
  if %ok%
    * determine value
    if %premium_value% ~= %item.vnum%
      set value 10
    else
      set value 1
    end
    * give token
    %actor.give_currency(10250,%value%)%
    * messaging
    %send% %actor% # You sell @%item% for %value% %currency.10250(%value%)%.
    %echoaround% %actor% # ~%actor% sells @%item%.
    * increment counter
    eval found %found% + 1
    %purge% %item%
  end
  * and repeat the loop
  set item %next_item%
done
if %found%
  set amount %actor.currency(10250)%
  %send% %actor% # You now have %amount% %currency.10250(%amount%)%.
else
  * didn't sell any?
  if %all%
    %send% %actor% You didn't have anything to sell.
  else
    %send% %actor% You don't seem to have %arg.ana% %arg%.
  end
end
~
#10274
Primeval environment~
2 bw 5
~
switch %random.4%
  case 1
    %echo% The sky grows darker and darker as the volcano in the distance erupts!
  break
  case 2
    %echo% A thunderous roar shakes the trees.
  break
  case 3
    %echo% You have the ever-present feeling that someone is watching you through the trees.
  break
  case 4
    %echo% The ground rumbles beneath you.
  break
done
~
#10275
Primeval Portal: learn craft book~
1 c 2
learn~
* Usage: learn <self>
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
* all other cases will return 1
* switch MUST set vnum_list, abil_list, and name_list
switch %self.vnum%
  case 10269
    * book of primordial weapon schematics
    set vnum_list 10268 10270 10272 10282 10266
    set abil_list 196 196 170 170 196
    set name_list primordial axe, dracosaur bone spear, jeweled bone staff, edge of extinction, and Tome of the Primordium
  break
  case 10273
    * book of dracosaur armor schematics
    set vnum_list 10274 10287 10288 10289 10290
    set abil_list 199 197 182 199 197
    set name_list dracosaur scale robe, dracoscale armor, dracosaur hide armor, dracosaur tooth robe, and dracosaur fang armor
  break
  case 10275
    * book of primordial footwear designs
    set vnum_list 10276 10278 10291 10292 10293
    set abil_list 197 197 199 199 182
    set name_list dracosaur scale boots, primordial boots, dracosaur scale sandals, dracosaur tooth boots, and dracosaur fang shoes
  break
  case 10277
    * book of dracosaur wristwear designs
    set vnum_list 10255 10280 10258 10294 10256
    set abil_list 182 197 199 197 199
    set name_list dracosaur bone bracelet, stunning dracosaur wristguard, obsidian bracelet, dracoscale vambraces, dracosaur scale wristband
  break
  case 10281
    * book of dracosaur belt patterns
    set vnum_list 10297 10298 10299
    set abil_list 182 199 197
    set name_list dracosaur fang belt, obsidian dracosaur waistband, and mighty dracoscale belt
  break
  default
    %send% %actor% There's nothing you can learn from this.
    halt
  break
done
* check if the player needs any of them
set any 0
set error 0
set vnum_copy %vnum_list%
while %vnum_copy%
  * pluck first numbers out
  set vnum %vnum_copy.car%
  set vnum_copy %vnum_copy.cdr%
  set abil %abil_list.car%
  set abil_list %abil_list.cdr%
  if !%actor.learned(%vnum%)%
    if %actor.ability(%abil%)%
      set any 1
    else
      set error 1
    end
  end
done
* how'd we do?
if %error% && !%any%
  %send% %actor% You don't have the right ability to learn anything new from @%self%.
  halt
elseif !%any%
  %send% %actor% There aren't any recipes you can learn from @%self%.
  halt
end
* ok go ahead and teach them
while %vnum_list%
  * pluck first numbers out
  set vnum %vnum_list.car%
  set vnum_list %vnum_list.cdr%
  if !%actor.learned(%vnum%)%
    nop %actor.add_learned(%vnum%)%
  end
done
* and report...
%send% %actor% You study @%self% and learn: %name_list%
%echoaround% %actor% ~%actor% studies @%self%.
%purge% %self%
~
#10296
Primeval Portal loot replacer~
1 n 100
~
* list of vnums and size of the list: WARNING this line must not pass 255 characters
set vnum_list 10268 10270 10272 10282 10266 10274 10287 10288 10289 10290 10276 10278 10291 10292 10293 10255 10280 10258 10294 10256 10297 10298 10299
set list_size 23
set mob %self.carried_by%
* pick one at random
eval pos %%random.%list_size%%%
set vnum 0
while %pos% > 0 && %vnum_list%
  set vnum %vnum_list.car%
  set vnum_list %vnum_list.cdr%
  eval pos %pos% - 1
done
* ensure we found one
if !%vnum%
  %echo% [Portal to the Primeval] Error determining loot.
  wait 1
  %purge% %self%
  halt
end
* load it
if %mob%
  %load% obj %vnum% %mob% inv
  set loaded %mob.inventory%
else
  %load% obj %vnum% %self.room%
  set loaded %self.room.contents%
end
* check it
if !%loaded% || %loaded.vnum% != %vnum%
  %echo% [Portal to the Primeval] Error loading loot.
  wait 1
  %purge% %self%
  halt
end
* flags/binding
if %loaded.is_flagged(BOP)%
  nop %loaded.bind(%self%)%
end
if %mob% && %mob.is_npc%
  * hard/group flags
  if %mob.mob_flagged(HARD)% && !%loaded.is_flagged(HARD-DROP)%
    nop %loaded.flag(HARD-DROP)%
  end
  if %mob.mob_flagged(GROUP)% && !%loaded.is_flagged(GROUP-DROP)%
    nop %loaded.flag(GROUP-DROP)%
  end
  %scale% %loaded% %self.level%
end
wait 1
%purge% %self%
~
$
