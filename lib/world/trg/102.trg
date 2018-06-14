#10200
Goblin Challenge Must Fight~
0 s 100
~
%send% %actor% You have begun the Goblin Challenge and cannot leave without fighting %self.name%.
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
    %echoaround% %actor% Zelkab bashes %actor.name% in the head with his club!
    dg_affect %actor% STUNNED on 10
    %damage% %actor% 50 physical
  break
done
~
#10205
Zelkab Death~
0 f 100
~
if %self.mob_flagged(UNDEAD)% || !%self.varexists(difficulty)%
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
    %echoaround% %actor% Garlgarl splashes goblinfire at %actor.name%, causing serious burns!
    %dot% %actor% 33 15 fire
    %damage% %actor% 50 fire
  break
done
~
#10207
Garlgarl Death~
0 f 100
~
if %self.mob_flagged(UNDEAD)% || !%self.varexists(difficulty)%
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
%echoaround% %actor% Filks dashes backwards, draws her bow, and shoots %actor.name% with a poison arrow!
%dot% %actor% 75 15 poison
%damage% %actor% 50 physical
~
#10209
Filks Death~
0 f 100
~
if %self.mob_flagged(UNDEAD)% || !%self.varexists(difficulty)%
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
if %self.mob_flagged(UNDEAD)% || !%self.varexists(difficulty)%
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
if %self.mob_flagged(UNDEAD)% || !%self.varexists(difficulty)%
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
    %echo% %walts.name% respawns.
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
    %echo% %filks.name% respawns.
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
    if %person.fighting% || %person.mob_flagged(UNDEAD)%
      set fighting 1
    end
    set goblin %person%
    set filks_present 1
  elseif %person.vnum% == 10203
    if %person.fighting% || %person.mob_flagged(UNDEAD)%
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
    %echo% %new_mob.name% respawns.
    nop %new_mob.add_mob_flag(!LOOT)%
  end
elseif %walts_present% && !%filks_present% && !%fighting%
  * Respawn Filks
  %load% mob 10202
  set new_mob %room.people%
  if %new_mob.vnum% == 10202
    %echo% %new_mob.name% respawns.
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
%send% %actor% %self.name% greets you warmly.
* only if they don't have one
if (%actor.has_item(10233)% || %actor.has_item(10234)% || %actor.has_item(10235)% || %actor.has_item(10236)% || %actor.has_item(10237)%)
  halt
end
wait 5
%send% %actor% %self.name% tells you, 'Here, take this. I could use a hand here.'
%send% %actor% %self.name% hands you a list.
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
  %send% %actor% You give %self.name% the fruits you have collected, and %self.heshe% gives you a wooden fruit token!
  %echoaround% %actor% %actor.name% gives %self.name% several baskets of fruits, and receives a wooden fruit token.
  if !%actor.has_item(10238)%
    %load% obj 10238 %actor% inv
    %send% %actor% %self.heshe% also gives you another list.
  end
elseif (%actor.has_resources(141,4)% && %actor.has_resources(3005,4)% && %actor.has_resources(3011,4)% && %actor.has_resources(145,4)%)
  * grains
  nop %actor.add_resources(141,-4)%
  nop %actor.add_resources(3005,-4)%
  nop %actor.add_resources(3011,-4)%
  nop %actor.add_resources(145,-4)%
  %load% obj 10234 %actor% inv
  %send% %actor% You give %self.name% the grains you have collected, and %self.heshe% gives you a copper grain token!
  %echoaround% %actor% %actor.name% gives %self.name% several baskets of grains, and receives a copper grain token.
  if !%actor.has_item(10239)%
    %load% obj 10239 %actor% inv
    %send% %actor% %self.heshe% also gives you another list.
  end
elseif (%actor.has_resources(143,4)% && %actor.has_resources(144,4)% && %actor.has_resources(3023,4)% && %actor.has_resources(3019,4)%)
  * tradegoods
  nop %actor.add_resources(143,-4)%
  nop %actor.add_resources(144,-4)%
  nop %actor.add_resources(3023,-4)%
  nop %actor.add_resources(3019,-4)%
  %load% obj 10235 %actor% inv
  %send% %actor% You give %self.name% the goods you have collected, and %self.heshe% gives you a silver tradegoods token!
  %echoaround% %actor% %actor.name% gives %self.name% several baskets of goods, and receives a silver tradegoods token.
else
  %send% %actor% You have nothing to offer.
  wait 5
  if !%actor.has_item(10237)%
    %load% obj 10237 %actor% inv
    %send% %actor% %self.name% hands you a list.
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
    %echo% %self.name% carefully waters the plants.
  break
  case 2
    %echo% %self.name% trims unwanted growth from the plants.
  break
  case 3
    %echo% %self.name% trims unwanted growth from the plants.
  break
  case 4
    %echo% %self.name% whistles a strange tune.
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
%echoaround% %actor% %actor.name% magically combines three strange tokens into a gnarled wooden token!
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
%echoaround% %actor% %actor.name% begins the chant of gardens...
wait 4 sec
if !(%self.carried_by% == %actor%) || %room% != %self.room%
  halt
end
%send% %actor% You rhythmically speak the words to the chant of gardens...
%echoaround% %actor% %actor.name% rhythmically speaks the words to the chant of gardens...
wait 4 sec
if !(%self.carried_by% == %actor%) || %room% != %self.room%
  halt
end
%echoaround% %actor% %actor.name% vanishes in a swirl of dust!
%teleport% %actor% i10226
%echoaround% %actor% %actor.name% appears in a swirl of dust!
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
%send% %actor% %self.name% takes a grievous bite out of you!
%echoaround% %actor% %self.name% takes a grievous bite out of %actor.name%!
%dot% %actor% 100 30 physical
%damage% %actor% 60 physical
~
#10253
Terrosaur combat~
0 k 5
~
dg_affect %self% BONUS-PHYSICAL 5 120
%echo% %self.name% seems to get angrier!
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
%echo% %self.name% cackles insanely!
wait 3 sec
if (%self.fighting% || %self.disabled%)
  halt
end
%echo% A strange violet glow encircles %self.name%, as if some arcane magic is controlling him.
wait 3 sec
if (%self.fighting% || %self.disabled%)
  halt
end
say How dare you follow me here!
wait 3 sec
if (%self.fighting% || %self.disabled%)
  halt
end
say Did the Academy send you? Are you here to take me back? I'm not going back there!
wait 3 sec
if (%self.fighting% || %self.disabled%)
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
%send% %actor% You can't seem to get away from %self.name%!
return 0
~
#10263
Mount whistle use~
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
%load% m %self.val0%
set mob %self.room.people%
%send% %actor% You use %self.shortdesc% and %mob.name% appears!
%echoaround% %actor% %actor.name% uses %self.shortdesc% and %mob.name% appears!
if (%mob% && %mob.vnum% == %self.val0%)
  nop %mob.unlink_instance%
end
%purge% %self%
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
Hint to 10252~
2 c 0
track~
set tofind 10252
if (!%actor.ability(Track)% || !%actor.ability(Navigation)%)
  * Fail through to ability message
  return 0
  halt
end
* It's never south -- that's always backtrack
set north %room.north(room)%
set east %room.east(room)%
set west %room.west(room)%
set northeast %room.northeast(room)%
set northwest %room.northwest(room)%
set southeast %room.southeast(room)%
set southwest %room.southwest(room)%
if (%north% && %north.template% == %tofind%)
  %send% %actor% You sense a trail to the north!
  return 1
elseif (%east% && %east.template% == %tofind%)
  %send% %actor% You sense a trail to the east!
  return 1
elseif (%west% && %west.template% == %tofind%)
  %send% %actor% You sense a trail to the west!
  return 1
elseif (%northeast% && %northeast.template% == %tofind%)
  %send% %actor% You sense a trail to the northeast!
  return 1
elseif (%northwest% && %northwest.template% == %tofind%)
  %send% %actor% You sense a trail to the northwest!
  return 1
elseif (%southeast% && %southeast.template% == %tofind%)
  %send% %actor% You sense a trail to the southeast!
  return 1
elseif (%southwest% && %southwest.template% == %tofind%)
  %send% %actor% You sense a trail to the southwest!
  return 1
end
~
#10267
Hint to 10253~
2 c 0
track~
set tofind 10253
if (!%actor.ability(Track)% || !%actor.ability(Navigation)%)
  * Fail through to ability message
  return 0
  halt
end
* It's never south -- that's always backtrack
set north %room.north(room)%
set east %room.east(room)%
set west %room.west(room)%
set northeast %room.northeast(room)%
set northwest %room.northwest(room)%
set southeast %room.southeast(room)%
set southwest %room.southwest(room)%
if (%north% && %north.template% == %tofind%)
  %send% %actor% You sense a trail to the north!
  return 1
elseif (%east% && %east.template% == %tofind%)
  %send% %actor% You sense a trail to the east!
  return 1
elseif (%west% && %west.template% == %tofind%)
  %send% %actor% You sense a trail to the west!
  return 1
elseif (%northeast% && %northeast.template% == %tofind%)
  %send% %actor% You sense a trail to the northeast!
  return 1
elseif (%northwest% && %northwest.template% == %tofind%)
  %send% %actor% You sense a trail to the northwest!
  return 1
elseif (%southeast% && %southeast.template% == %tofind%)
  %send% %actor% You sense a trail to the southeast!
  return 1
elseif (%southwest% && %southwest.template% == %tofind%)
  %send% %actor% You sense a trail to the southwest!
  return 1
end
~
#10268
Hint to 10254~
2 c 0
track~
set tofind 10254
if (!%actor.ability(Track)% || !%actor.ability(Navigation)%)
  * Fail through to ability message
  return 0
  halt
end
* It's never south -- that's always backtrack
set north %room.north(room)%
set east %room.east(room)%
set west %room.west(room)%
set northeast %room.northeast(room)%
set northwest %room.northwest(room)%
set southeast %room.southeast(room)%
set southwest %room.southwest(room)%
if (%north% && %north.template% == %tofind%)
  %send% %actor% You sense a trail to the north!
  return 1
elseif (%east% && %east.template% == %tofind%)
  %send% %actor% You sense a trail to the east!
  return 1
elseif (%west% && %west.template% == %tofind%)
  %send% %actor% You sense a trail to the west!
  return 1
elseif (%northeast% && %northeast.template% == %tofind%)
  %send% %actor% You sense a trail to the northeast!
  return 1
elseif (%northwest% && %northwest.template% == %tofind%)
  %send% %actor% You sense a trail to the northwest!
  return 1
elseif (%southeast% && %southeast.template% == %tofind%)
  %send% %actor% You sense a trail to the southeast!
  return 1
elseif (%southwest% && %southwest.template% == %tofind%)
  %send% %actor% You sense a trail to the southwest!
  return 1
end
~
#10269
Hint to 10255~
2 c 0
track~
set tofind 10255
if (!%actor.ability(Track)% || !%actor.ability(Navigation)%)
  * Fail through to ability message
  return 0
  halt
end
* It's never south -- that's always backtrack
set north %room.north(room)%
set east %room.east(room)%
set west %room.west(room)%
set northeast %room.northeast(room)%
set northwest %room.northwest(room)%
set southeast %room.southeast(room)%
set southwest %room.southwest(room)%
if (%north% && %north.template% == %tofind%)
  %send% %actor% You sense a trail to the north!
  return 1
elseif (%east% && %east.template% == %tofind%)
  %send% %actor% You sense a trail to the east!
  return 1
elseif (%west% && %west.template% == %tofind%)
  %send% %actor% You sense a trail to the west!
  return 1
elseif (%northeast% && %northeast.template% == %tofind%)
  %send% %actor% You sense a trail to the northeast!
  return 1
elseif (%northwest% && %northwest.template% == %tofind%)
  %send% %actor% You sense a trail to the northwest!
  return 1
elseif (%southeast% && %southeast.template% == %tofind%)
  %send% %actor% You sense a trail to the southeast!
  return 1
elseif (%southwest% && %southwest.template% == %tofind%)
  %send% %actor% You sense a trail to the southwest!
  return 1
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
%echo% The earth itself tremors in fear as %self.name% stomps toward you.
wait 3 sec
if (%self.fighting% || %self.disabled%)
  halt
end
%echo% %self.name% stops and lets out a terrifying roar!
wait 3 sec
if (%self.fighting% || %self.disabled%)
  halt
end
%echo% %self.name% swipes %self.hisher% massive tail, and a tree goes flying!
%load% obj 120 room
wait 3 sec
if (%self.fighting% || %self.disabled%)
  halt
end
%echo% %self.name% looms close, and it's only as %self.heshe% bends down to bite you that you realize how big %self.heshe% is!
wait 3 sec
if (%self.fighting% || %self.disabled%)
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
if (%self.fighting% || %self.disabled%)
  halt
end
%echo% %self.name% drops the crocodile %self.heshe% was carrying in its talons, and swoops toward you!
wait 3 sec
if (%self.fighting% || %self.disabled%)
  halt
end
%echo% %self.name% vanishes as %self.heshe% swoops too close to the trees for you to see exactly which way %self.heshe%'s coming from.
%load% obj 120
wait 3 sec
if (%self.fighting% || %self.disabled%)
  halt
end
%echo% %self.name% reappears over the clearing, suddenly so close %self.heshe% blots out the sun!
wait 3 sec
if (%self.fighting% || %self.disabled%)
  halt
end
%aggro% %actor%
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
$
