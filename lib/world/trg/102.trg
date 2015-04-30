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
$
