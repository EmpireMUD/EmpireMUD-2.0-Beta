#11100
Hermit Greeting~
0 bg 25
~
wait 1 sec
say Are you going to eat that?
~
#11101
Hermit Food Exchange~
0 j 100
~
wait 1 sec
if %object.type% == FOOD
  say My thanks to you, %actor.name%!
  %load% o 11100
  give bag %actor.name%
  %echo% %self.name% gleefully eats %object.shortdesc%!
  mjunk %object.name%
  tdetach mob %self% all
end
~
#11102
Befriend Pegasus~
0 j 100
~
* Reject items other than the 6 crop items from this adventure
if (%object.vnum% < 11102 || %object.vnum% > 11107)
  wait 1 sec
  %echo% %self.name% does not seem interested.
  mjunk all
  halt
end
* Block NPCs
if !(%actor.is_pc%)
  halt
end
* Get current befriend value or set to 0
if (%actor.varexists(befriend_pegasus)%)
  eval befriend_pegasus %actor.befriend_pegasus%
else
  eval befriend_pegasus 0
end
* Add value
eval befriend_pegasus %befriend_pegasus% + 1
* Save for later
remote befriend_pegasus %actor.id%
* Behavior
wait 1 sec
if (%befriend_pegasus% < 3)
  %send% %actor% %self.name% eats %object.shortdesc% and nuzzles you.
  %echoaround% %actor% %self.name% eats %object.shortdesc% and nuzzles %actor.name%.
  mjunk all
else
  %echo% %self.name% eats %object.shortdesc% and nickers.
  %send% %actor% It looks like you could ride %self.himher%!
  mjunk all
  * reset to 0
  eval befriend_pegasus 0
  remote befriend_pegasus %actor.id%
  * Switcheroo
  %load% mob 11104
  %purge% %self%
  halt
end
~
#11103
Pegasus Fly Away~
0 ab 5
~
eval count 0
eval room_var %self.room%
eval target_char %room_var.people%
while %target_char%
  if (%target_char.is_pc%)
    eval count %count% + 1
  end
  eval target_char %target_char.next_in_room%
done
* Depsawn if no players present
if %count% < 1
  %regionecho% %self.room% -50 A pegasus flies away overhead!
  %purge% %self%
end
~
#11104
Cave Viper Combat~
0 k 10
~
%send% %actor% %self.name% strikes out and bites your leg!
%echoaround% %actor% %self.name% strikes out and bites %actor.name%'s leg!
%dot% %actor% 100 30 poison
~
#11105
Venomous Skink Combat~
0 k 10
~
%send% %actor% %self.name% bites down and latches onto your arm! You don't feel so good...
%echoaround% %actor% %self.name% bites down and latches onto %actor.name%'s arm! %actor.name% doesn't look so good...
dg_affect %actor% slow on 30
%damage% %actor% 50
~
#11106
Lean Left~
2 c 0
left~
eval room_var %actor.room%
context %room_var.vnum%
%send% %actor% You lean hard to the left!
%echoaround% %actor% %actor.name% leans hard to the left!
eval lean_left 1
remote lean_left %actor.id%
eval lean_right 0
remote lean_right %actor.id%
~
#11107
Lean Right~
2 c 0
right~
eval room_var %actor.room%
context %room_var.vnum%
%send% %actor% You lean hard to the right!
%echoaround% %actor% %actor.name% leans hard to the right!
eval lean_left 0
remote lean_left %actor.id%
eval lean_right 1
remote lean_right %actor.id%
~
#11108
Duck!~
2 c 0
duck~
eval room_var %actor.room%
context %room_var.vnum%
%send% %actor% You duck in the boat!
%echoaround% %actor% %actor.name% ducks!
eval has_ducked 1
remote has_ducked %actor.id%
~
#11109
Rapids Start 11112~
2 g 100
~
wait 1 sec
eval room_var %actor.room%
%echo% The boat begins to accelerate!
wait 10 sec
if (%actor.room% != %room_var%)
  halt
end
%teleport% all i11120
~
#11110
Rock Obstacle 11120~
2 g 100
~
context %room.vnum%
if %actor.is_pc%
  eval lean_left 0
  remote lean_left %actor.id%
  eval lean_right 0
  remote lean_right %actor.id%
end
if (%rock_obstacle_running% && %rock_obstacle_running% + 60 > %timestamp%)
  halt
end
eval rock_obstacle_running %timestamp%
global rock_obstacle_running
wait 1 sec
eval rock_left (%random.2% == 2)
global rock_left
if (%rock_left%)
  %echo% &RA huge rock is coming up on the left! Which way should you lean?&0
else
  %echo% &RA huge rock is coming up on the right! Which way should you lean?&0
end
wait 8 sec
eval correct 0
eval fail 0
eval ch %room.people%
while %ch%
  if %ch.is_pc% && !%ch.nohassle%
    if (%rock_left% && %ch.varexists(lean_right)% && %ch.lean_right%) || (!%rock_left% && %ch.varexists(lean_left)% && %ch.lean_left%)
      eval correct %correct% + 1
    else
      eval fail %fail% + 1
    end
  end
  eval ch %ch.next_in_room%
done
eval rock_obstacle_running 0
global rock_obstacle_running
unset rock_obstacle_running
if %correct% > %fail%
  %echo% The boat leans away from the huge rock!
  %teleport% all i11121
else
  %echo% The boat smacks into the rock, hurling you out of the rapids!
  eval ch %room.people%
  while %ch%
    eval next_ch %ch.next_in_room%
    %teleport% %ch% i11123
    %force% %ch% look
    %damage% %ch% 25
    eval ch %next_ch%
  done
end
~
#11111
Tree Branch 11121~
2 g 100
~
context %room.vnum%
if %actor.is_pc%
  eval has_ducked 0
  remote has_ducked %actor.id%
end
if (%tree_branch_running% && %tree_branch_running% + 60 > %timestamp%)
  halt
end
eval tree_branch_running %timestamp%
global tree_branch_running
wait 1 sec
%echo% &RThe boat is coming up on a low-hanging tree branch!&0
wait 8 sec
eval ch %room.people%
eval wins 0
while %ch%
  eval next_ch %ch.next_in_room%
  if %ch.is_pc%
    if !(%ch.varexists(has_ducked)% && %ch.has_ducked%)
      %send% %actor% You smack into the tree branch and are knocked from the boat!
      %echoaround% %actor% %actor.name% smacks into the tree branch and is knocked from the boat!
      %teleport% %ch% i11123
      %force% %ch% look
      %damage% %ch% 25
    else
      eval wins %wins% + 1
    end
  end
  eval ch %next_ch%
done
eval tree_branch_running 0
global tree_branch_running
unset tree_branch_running
* send npcs to fail room if no players won
if !(%wins%)
  %teleport% all i11123
else
  %teleport% all i11122
end
~
#11112
Narrow Opening 11122~
2 g 100
~
context %room.vnum%
if %actor.is_pc%
  eval lean_left 0
  remote lean_left %actor.id%
  eval lean_right 0
  remote lean_right %actor.id%
end
if (%narrow_opening_running% && %narrow_opening_running% + 60 > %timestamp%)
  halt
end
eval narrow_opening_running %timestamp%
global narrow_opening_running
wait 1 sec
eval opening_left (%random.2% == 2)
global opening_left
if (%opening_left%)
  %echo% &RThere is a narrow opening on the left! Which way should you lean?&0
else
  %echo% &RThere is a narrow opening on the right! Which way should you lean?&0
end
wait 8 sec
eval correct 0
eval fail 0
eval ch %room.people%
while %ch%
  if %ch.is_pc% && !%ch.nohassle%
    if (%opening_left% && %ch.varexists(lean_left)% && %ch.lean_left%) || (!%opening_left% && %ch.varexists(lean_right)% && %ch.lean_right%)
      eval correct %correct% + 1
    else
      eval fail %fail% + 1
    end
  end
  eval ch %ch.next_in_room%
done
eval narrow_opening_running 0
global narrow_opening_running
unset narrow_opening_running
if %correct% > %fail%
  %echo% The boat leans into the narrow opening!
  %teleport% all i11113
  %at% i11113 %force% all look
else
  %echo% The boat misses the narrow opening, hurling you out of the rapids!
  eval ch %room.people%
  while %ch%
    eval next_ch %ch.next_in_room%
    %teleport% %ch% i11123
    %force% %ch% look
    %damage% %ch% 25
    eval ch %next_ch%
  done
end
~
#11130
Caretaker Replacement~
2 q 100
~
if !%actor.is_pc%
  halt
end
makeuid atroom room i11142
eval found 0
eval exists 0
eval ch %atroom.people%
while %ch%
  eval next_ch %ch.next_in_room%
  if %ch.vnum% == 11135
    %purge% %ch%
    eval found 1
  elseif %ch.vnum% == 11136
    eval exists 1
  end
  eval ch %next_ch%
done
if %found% && !%exists%
  %at% i11142 wload mob 11136
end
~
#11131
Hex Box Open 1: Garden~
1 c 4
push~
context %instance.id%
if !(garden /= %arg%)
  return 0
  halt
end
* Load current position
if !%hex_box_open%
  eval hex_box_open 0
end
* Message
%send% %actor% You push the image of the garden on the side of %self.shortdesc%.
%echoaround% %actor% %actor.name% pushes the image of the garden on the side of %self.shortdesc%.
* Fail: wrong order
if %hex_box_open% != 0
  if %hex_box_open% > 0
    %send% %actor% To your dismay, the sigils fade on the sides of %self.shortdesc%.
    %echoaround% %actor% To %actor.name%'s dismay, the sigils fade on the sides of %self.shortdesc%.
  else
    %send% %actor% ... nothing seems to happen.
  end
  eval hex_box_open 0
  global hex_box_open
  halt
end
%send% %actor% To your surprise, the image of the garden begins to glow!
%echoaround% %actor% To %actor.name%'s surprise, the image of the garden begins to glow!
eval hex_box_open 1
global hex_box_open
~
#11132
Hex Box Open 2: Mill~
1 c 4
push~
context %instance.id%
if !(mill /= %arg%)
  return 0
  halt
end
* Load current position
if !%hex_box_open%
  eval hex_box_open 0
end
* Message
%send% %actor% You push the image of the mill on the side of %self.shortdesc%.
%echoaround% %actor% %actor.name% pushes the image of the mill on the side of %self.shortdesc%.
* Fail: wrong order
if %hex_box_open% != 1
  if %hex_box_open% > 0
    %send% %actor% To your dismay, the sigils fade on the sides of %self.shortdesc%.
    %echoaround% %actor% To %actor.name%'s dismay, the sigils fade on the sides of %self.shortdesc%.
  else
    %send% %actor% ... nothing seems to happen.
  end
  eval hex_box_open 0
  global hex_box_open
  halt
end
%send% %actor% The image of the mill begins to glow!
%echoaround% %actor% The image of the mill begins to glow!
eval hex_box_open 2
global hex_box_open
~
#11133
Hex Box Open 3: Stable~
1 c 4
push~
context %instance.id%
if !(stable /= %arg%)
  return 0
  halt
end
* Load current position
if !%hex_box_open%
  eval hex_box_open 0
end
* Message
%send% %actor% You push the image of the stable on the side of %self.shortdesc%.
%echoaround% %actor% %actor.name% pushes the image of the stable on the side of %self.shortdesc%.
* Fail: wrong order
if %hex_box_open% != 2
  if %hex_box_open% > 0
    %send% %actor% To your dismay, the sigils fade on the sides of %self.shortdesc%.
    %echoaround% %actor% To %actor.name%'s dismay, the sigils fade on the sides of %self.shortdesc%.
  else
    %send% %actor% ... nothing seems to happen.
  end
  eval hex_box_open 0
  global hex_box_open
  halt
end
%send% %actor% The image of the stable begins to glow!
%echoaround% %actor% The image of the stable begins to glow!
eval hex_box_open 3
global hex_box_open
~
#11134
Hex Box Open 4: Estate~
1 c 4
push~
context %instance.id%
if !(estate /= %arg%)
  return 0
  halt
end
* Load current position
if !%hex_box_open%
  eval hex_box_open 0
end
* Message
%send% %actor% You push the image of the estate on the side of %self.shortdesc%.
%echoaround% %actor% %actor.name% pushes the image of the estate on the side of %self.shortdesc%.
* Fail: wrong order
if %hex_box_open% != 3
  if %hex_box_open% > 0
    %send% %actor% To your dismay, the sigils fade on the sides of %self.shortdesc%.
    %echoaround% %actor% To %actor.name%'s dismay, the sigils fade on the sides of %self.shortdesc%.
  else
    %send% %actor% ... nothing seems to happen.
  end
  eval hex_box_open 0
  global hex_box_open
  halt
end
%send% %actor% The image of the estate begins to glow!
%echoaround% %actor% The image of the estate begins to glow!
wait 1 sec
%echo% The lid of %self.shortdesc% dissolves and the box opens!
* replace with new item
%load% obj 11141
makeuid box obj openhexbox
%load% obj 11131 %box%
%purge% %self%
~
#11135
Sarcophagus open~
1 c 4
open~
context %instance.id%
* Did they target me?
eval test %%self.is_name(%arg%)%%
if %sarcophagus_running% || !(%test%)
  return 0
  halt
end
eval sarcophagus_running 1
global sarcophagus_running
%send% %actor% You cautiously open %self.shortdesc%...
%echoaround% %actor% %actor.name% cautiously opens %self.shortdesc%...
%echo% A giant shape lurches at you from the darkness inside!
%load% mob 11138
%load% obj 11140
makeuid snake mob titanaconda
%force% %snake% mkill %actor%
%purge% %self%
eval sarcophagus_running 0
global sarcophagus_running
~
#11136
Chalice combine~
1 b 100
~
eval found_cutting 0
eval found_pod 0
eval found_berries 0
eval found_redthorn 0
eval iter %self.contents%
while %iter%
  eval next_iter %iter.next_in_list%
  if (%iter.vnum% == 11142)
    eval found_cutting 1
  elseif (%iter.vnum% == 11143)
    eval found_pod 1
  elseif (%iter.vnum% == 11144)
    eval found_berries 1
  elseif (%iter.vnum% == 1202)
    eval found_redthorn 1
  else
    * not one of our ingredients -- destroy it
    if %self.carried_by%
      %send% %self.carried_by% %iter.shortdesc% dissolves in %self.shortdesc%.
    else
      %echo% %iter.shortdesc% dissolves in %self.shortdesc%.
    end
    %purge% %iter%
  end
  eval iter %next_iter%
done
* Victory against chalice!
if (%found_cutting% == 1 && %found_pod% == 1 && %found_berries% == 1 && %found_redthorn% == 1)
  %echo% %self.shortdesc% bursts into flames and melts into a slag of jewels and gold!
  * move players if still in instance
  if %instance.id%
    %adventurecomplete%
    * swap in i11151 for the old stone marker
    %door% i11130 east room i11151
    %echo% You find yourself back at the stone marker, where the adventure began!
    %teleport% adventure i11151
  end
  * replace with slag (do this after teleporting
  if %self.carried_by%
    %load% obj 11134 %self.carried_by% inv
  else
    %load% obj 11134
  end
  %purge% %self%
  halt
end
* Timer isn't up and chalice isn't done...
if %random.4% == 4
  %echo% A darting dragonfly appears and attacks!
  %load% mob 11137
  makeuid dragonfly mob dragonfly
  if %self.carried_by%
    %force% %dragonfly% mkill %self.carried_by%
  else
    %force% %dragonfly% outrage
  end
end
~
#11137
Chalice Expiration~
1 f 0
~
* Only works if still in the dungeon, otherwise decays naturally
if %instance.id%
  %adventurecomplete%
  * swap in i11150 for the old stone marker
  %door% i11130 east room i11150
  %echo% You find yourself back at the stone marker, where the adventure began!
  %echo% %self.name% has vanished.
  %teleport% adventure i11150
  return 0
  %purge% %self%
end
~
#11138
Titanaconda combat~
0 k 50
~
* Countered by good entangle (not always desirable)
if (%self.aff_flagged(ENTANGLED)% || %self.disabled%)
  halt
end
eval target %random.enemy%
%send% %target% %self.name% encircles you and constricts!
%echoaround% %target% %self.name% encircles %target.name% and constricts %target.himher%!
dg_affect %target% STUNNED on 15
dg_affect %self% STUNNED on 15
%damage% %target% 50
wait 5 sec
if !%target% || !self || !%self.fighting%
  halt
end
%send% %target% %self.name% constricts and crushes you!
%echoaround% %target% %self.name% constricts and crushes %target.name%!
%damage% %target% 50
wait 5 sec
if !%target% || !self || !%self.fighting%
  halt
end
%send% %target% %self.name% constricts and crushes you!
%echoaround% %target% %self.name% constricts and crushes %target.name%!
%damage% %target% 50
wait 5 sec
if !%target% || !self || !%self.fighting%
  halt
end
%send% %target% %self.name% releases you!
%echoaround% %target% %self.name% releases %target.name%!
~
#11139
Sleeping Ivy combat~
0 k 50
~
* This is a mini-version of Titanconda's combat script (11138), and should
* train the player for it.
* Countered by good entangle (not always desirable)
if (%self.aff_flagged(ENTANGLED)% || %self.disabled%)
  halt
end
eval target %random.enemy%
if !%target%
  halt
end
%send% %target% %self.name% wraps %self.himher%self around you, mummifying you and dragging you off to blissful slumber...
%echoaround% %target% %self.name% wraps %self.himher%self around %target.name%, mummifying %target.himher% and dragging %target.himher% off to blissful slumber...
dg_affect %target% STUNNED on 15
dg_affect %self% STUNNED on 15
* prevents re-running of this script until affect ends
wait 15 sec
~
#11140
Lulling songbird combat~
0 k 20
~
eval target %random.enemy%
%send% %target% %self.name% sings a sweet tune, lulling you and making you sluggish!
%echoaround% %target% %self.name% sings a sweet tune, lulling %target.name% and making %target.himher% sluggish!
dg_affect %target% SLOW on 15
dg_affect %target% WITS -1 15
~
$
