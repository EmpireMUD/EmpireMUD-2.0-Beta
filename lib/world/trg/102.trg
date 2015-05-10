#10200
Goblin Challenge Must Fight~
0 q 100
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
%echo% As Zelkab dies, Garlgarl steps into the nest!
%load% mob 10201
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
%echo% As Garlgarl dies, Filks and Walts step into the nest!
%load% mob 10202
%load% mob 10203
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
eval room %self.room%
eval ch %room.people%
eval found 0
while %ch% && !%found%
  if (%ch.vnum% == 10203)
    eval found 1
  end
  eval ch %ch.next_in_room%
done
if !%found%
  %echo% As Filks dies, Nilbog steps into the nest!
  %load% mob 10204
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
eval room %self.room%
eval ch %room.people%
eval found 0
while %ch% && !%found%
  if (%ch.vnum% == 10202)
    eval found 1
  end
  eval ch %ch.next_in_room%
done
if !%found%
  %echo% As Walts dies, Nilbog steps into the nest!
  %load% mob 10204
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
%echo% As Nilbog dies, Furl steps into the nest!
%load% mob 10205
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
#10215
Furl Death Rarespawn~
0 f 5
~
%echo% As Furl dies, you notice his trusty mount enter the nest!
%load% mob 10206
~
#10216
Filks Respawn~
0 b 100
~
* Respawns Walts if needed
if (%self.fighting% || %self.disabled%)
  halt
end
eval room_var %self.room%
eval ch %room_var.people%
eval found 0
while %ch% && !%found%
  if (%ch.is_npc% && %ch.vnum% == 10203)
    eval found 1
  end
  eval ch %ch.next_in_room%
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
eval room %self.room%
eval ch %room.people%
eval found 0
while (%ch% && !%found%)
  if (%ch.vnum% == 10202)
    eval found 1
  end
  eval ch %ch.next_in_room%
done
if (!%found%)
  %load% mob 10202
  makeuid filks mob filks
  if %filks%
    %echo% %filks.name% respawns.
    nop %filks.add_mob_flag(!LOOT)%
  end
end/f
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
  %echoaround% %actor% %atcor.name% gives %self.name% several baskets of fruits, and receives a wooden fruit token.
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
  %echoaround% %actor% %atcor.name% gives %self.name% several baskets of grains, and receives a copper grain token.
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
  %echoaround% %actor% %atcor.name% gives %self.name% several baskets of goods, and receives a silver tradegoods token.
else
  %send% %actor% You have nothing to offer.
  wait 5
  if !%actor.has_item(10237)%
    %load% obj 10237 %actor% inv
    %send% %actor% %self.name% hands you a list.
  end
end
~
#10232
Tumbleweed mount spawn~
1 c 2
use~
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
%load% m 10227
%echo% The enchanted tumbleweed comes to life!
%purge% %self%
~
#10233
Gardener passive~
0 b 15
~
if %self.varexists(msg_pos)%
  eval msg_pos %self.msg_pos%
else
  eval msg_pos 0
end
eval msg_pos %msg_pos% + 1
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
  eval msg_pos 0
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
  %send% %actor% You can't combine that.
  halt
end
if !(%actor.has_item(10234)%)
  %send% %actor% You require a copper grain token to combine.
  halt
elseif !(%actor.has_item(10233)%)
  %send% %actor% You require a wooden fruit token to combine.
  halt
end
nop %actor.add_resources(10233,-1)%
nop %actor.add_resources(10234,-1)%
%send% %actor% You magically combine all three tokens into a gnarled wooden token!
%echoaround% %actor% %actor.name% magically combines three strange tokens into a gnarled wooden token!
%load% obj 10236 %actor% inv
%purge% %self%
~
#10236
Hidden Garden teleport chant~
2 c 0
chant~
* Only know the 'gardens' chant if the have the token.
if (!(gardens /= %arg%) || !(%actor.has_item(10236)%) || %actor.position% != Standing)
  return 0
  halt
end
%send% %actor% You begin the chant of gardens...
%echoaround% %actor% %actor.name% begins the chant of gardens...
wait 4 sec
if !(%actor.has_item(10236)%)
  halt
end
%send% %actor% You rhythmically speak the words to the chant of gardens...
%echoaround% %actor% %actor.name% rhythmically speaks the words to the chant of gardens...
wait 4 sec
if !(%actor.has_item(10236)%)
  halt
end
%echoaround% %actor% %actor.name% vanishes in a swirl of dust!
%teleport% %actor% i10226
%echoaround% %actor% %actor.name% appears in a swirl of dust!
%force% %actor% look
~
$
