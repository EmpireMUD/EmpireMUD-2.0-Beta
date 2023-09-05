#11100
Hermit Greeting~
0 bgw 25
~
if %self.mob_flagged(SILENT)%
  halt
end
wait 1 sec
say Are you going to eat that?
~
#11101
Hermit Food Exchange~
0 j 100
~
if %object.type% != FOOD || %self.varexists(gave%actor.id%)%
  %send% %actor% ~%self% doesn't want @%object%!
  %send% %actor% (You have already completed this quest in this instance.)
  return 0
  halt
end
wait 1 sec
if %object.type% == FOOD
  say My thanks to you, %actor.name%!
  * We used to actually load it and then give it but it was not being given to players with full inventories
  %send% %actor% ~%self% gives you a snakeskin bag.
  %echoaround% %actor% ~%self% gives ~%actor% a snakeskin bag.
  %load% o 11100 %actor% inventory
  %echo% ~%self% gleefully eats @%object%!
  mjunk %object.name%
  set gave%actor.id% 1
  remote gave%actor.id% %self.id%
end
~
#11102
Befriend Pegasus~
0 j 100
~
* Reject items other than the 6 crop items from this adventure
if (%object.vnum% < 11102 || %object.vnum% > 11107)
  wait 1 sec
  %echo% ~%self% does not seem interested.
  mjunk all
  halt
end
* Block NPCs
if !(%actor.is_pc%)
  halt
end
* Get current befriend value or set to 0
if (%actor.varexists(befriend_pegasus)%)
  set befriend_pegasus %actor.befriend_pegasus%
else
  set befriend_pegasus 0
end
* Add value
eval befriend_pegasus %befriend_pegasus% + 1
* Save for later
remote befriend_pegasus %actor.id%
* Behavior
wait 1 sec
if (%befriend_pegasus% < 3)
  %send% %actor% ~%self% eats @%object% and nuzzles you.
  %echoaround% %actor% ~%self% eats @%object% and nuzzles ~%actor%.
  mjunk all
else
  %echo% ~%self% eats @%object% and nickers.
  %send% %actor% It looks like you could ride *%self%!
  mjunk all
  * reset to 0
  set befriend_pegasus 0
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
set count 0
set target_char %self.room.people%
while %target_char%
  if (%target_char.is_pc%)
    eval count %count% + 1
  end
  set target_char %target_char.next_in_room%
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
%send% %actor% ~%self% strikes out and bites your leg!
%echoaround% %actor% ~%self% strikes out and bites |%actor% leg!
if %actor.poison_immunity%
  %damage% %actor% 30 physical
elseif %actor.resist_poison%
  %damage% %actor% 50 poison
else
  %dot% %actor% 100 30 poison
  %damage% %actor% 50 poison
end
~
#11105
Venomous Skink Combat~
0 k 10
~
%send% %actor% ~%self% bites down and latches onto your arm!
%echoaround% %actor% ~%self% bites down and latches onto |%actor% arm!
if !%actor.poison_immunity% && !%actor.resist_poison%
  %send% %actor% You don't feel so good...
  %echoaround% %actor% ~%actor% doesn't look so good...
  dg_affect %actor% slow on 30
end
%damage% %actor% 50
~
#11106
Lean Left~
2 c 0
left~
context %actor.room.vnum%
%send% %actor% You lean hard to the left!
%echoaround% %actor% ~%actor% leans hard to the left!
set lean_left 1
remote lean_left %actor.id%
set lean_right 0
remote lean_right %actor.id%
~
#11107
Lean Right~
2 c 0
right~
context %actor.room.vnum%
%send% %actor% You lean hard to the right!
%echoaround% %actor% ~%actor% leans hard to the right!
set lean_left 0
remote lean_left %actor.id%
set lean_right 1
remote lean_right %actor.id%
~
#11108
Duck!~
2 c 0
duck~
context %actor.room.vnum%
%send% %actor% You duck in the boat!
%echoaround% %actor% ~%actor% ducks!
set has_ducked 1
remote has_ducked %actor.id%
~
#11109
Rapids Start 11112~
2 g 100
~
wait 1 sec
set room_var %actor.room%
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
if %actor.is_pc%
  set lean_left 0
  remote lean_left %actor.id%
  set lean_right 0
  remote lean_right %actor.id%
end
wait 1 sec
eval rock_left (%random.2% == 2)
global rock_left
if (%rock_left%)
  %echo% &&RA huge rock is coming up on the left! Which way should you lean?&&0
else
  %echo% &&RA huge rock is coming up on the right! Which way should you lean?&&0
end
wait 8 sec
set correct 0
set fail 0
set ch %room.people%
while %ch%
  if %ch.is_pc% && !%ch.nohassle%
    set this_loop_correct 0
    if %rock_left%
      if %ch.varexists(lean_right)%
        if %ch.lean_right%
          set this_loop_correct 1
        end
      end
    else
      if %ch.varexists(lean_left)%
        if %ch.lean_left%
          set this_loop_correct 1
        end
      end
    end
    if %this_loop_correct%
      eval correct %correct% + 1
    else
      eval fail %fail% + 1
    end
  end
  set ch %ch.next_in_room%
done
if %correct% > %fail%
  %echo% The boat leans away from the huge rock!
  %teleport% all i11121
else
  %echo% The boat smacks into the rock, hurling you out of the rapids!
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    %teleport% %ch% i11123
    %force% %ch% look
    %damage% %ch% 25
    set ch %next_ch%
  done
end
~
#11111
Tree Branch 11121~
2 g 100
~
if %actor.is_pc%
  set has_ducked 0
  remote has_ducked %actor.id%
end
wait 1 sec
%echo% &&RThe boat is coming up on a low-hanging tree branch!&&0
wait 8 sec
set ch %room.people%
set wins 0
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_pc%
    set ducked 0
    if %ch.varexists(has_ducked)%
      if %ch.has_ducked%
        set ducked 1
      end
    end
    if !%ducked%
      %send% %actor% You smack into the tree branch and are knocked from the boat!
      %echoaround% %actor% ~%actor% smacks into the tree branch and is knocked from the boat!
      %teleport% %ch% i11123
      %force% %ch% look
      %damage% %ch% 25
    else
      eval wins %wins% + 1
    end
  end
  set ch %next_ch%
done
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
if %actor.is_pc%
  set lean_left 0
  remote lean_left %actor.id%
  set lean_right 0
  remote lean_right %actor.id%
end
wait 1 sec
eval opening_left (%random.2% == 2)
global opening_left
if (%opening_left%)
  %echo% &&RThere is a narrow opening on the left! Which way should you lean?&&0
else
  %echo% &&RThere is a narrow opening on the right! Which way should you lean?&&0
end
wait 8 sec
set correct 0
set fail 0
set ch %room.people%
while %ch%
  if %ch.is_pc% && !%ch.nohassle%
    set this_loop_correct 0
    if %opening_left%
      if %ch.varexists(lean_left)%
        if %ch.lean_left%
          set this_loop_correct 1
        end
      end
    else
      if %ch.varexists(lean_right)%
        if %ch.lean_right%
          set this_loop_correct 1
        end
      end
    end
    if %this_loop_correct%
      eval correct %correct% + 1
    else
      eval fail %fail% + 1
    end
  end
  set ch %ch.next_in_room%
done
if %correct% > %fail%
  %echo% The boat leans into the narrow opening!
  %teleport% all i11113
  %at% i11113 %force% all look
else
  %echo% The boat misses the narrow opening, hurling you out of the rapids!
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    %teleport% %ch% i11123
    %force% %ch% look
    %damage% %ch% 25
    set ch %next_ch%
  done
end
~
#11113
Raptor greet/aggro~
0 gw 100
~
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%) || !%actor.is_pc%)
  halt
end
wait 5
%echo% A raspy screech echoes through the caves and canyons!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%))
  halt
end
%echo% An ominous shadow passes overhead!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%))
  halt
end
%echo% You spot ~%self% in the air above!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%))
  halt
end
%send% %actor% ~%self% goes into a dive, heading straight for you!
%echoaround% %actor% ~%self% goes into a dive, heading straight for ~%actor%!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%))
  halt
end
%aggro% %actor%
~
#11114
Loch colossus greet/aggro~
0 gw 100
~
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%) || !%actor.is_pc%)
  halt
end
wait 5
%echo% Ripples spread slowly across the surface of the loch...
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%))
  halt
end
%echo% An ominous shadow circles below the water...
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%))
  halt
end
%echo% ~%self% emerges from the water!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%))
  halt
end
%echoaround% %actor% ~%self% charges toward ~%actor%!
%send% %actor% ~%self% charges toward you!
wait 3 sec
if (%self.fighting% || %self.disabled% || %actor.nohassle% || !(%actor.room%==%self.room%))
  halt
end
%aggro% %actor%
~
#11115
Vehicle Coupon Summon~
1 c 2
use~
if !%self.is_name(%arg%)%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
set target %self.val0%
%load% v %target%
* Todo: eval vehicle %self.room.first_vehicle_in_room% / etc
* and use %vehicle.name% instead of "a rib-bone boat"
* (see minipet use for an example)
%send% %actor% You use @%self% and a rib-bone boat appears!
%echoaround% %actor% ~%actor% uses @%self% and a rib-bone boat appears!
%purge% %self%
~
#11116
Loch Colossus block door~
0 r 100
~
%send% %actor% You cannot reach that while ~%self% is in the way.
return 0
~
#11117
Burrow Canyons: Check mob difficulty on-load~
0 n 100
~
set boss_mobs 11103 11105
set mini_mobs 11100 11102
set start %instance.start%
if %start%
  set difficulty %start.var(difficulty,0)%
else
  set difficulty 0
end
if %difficulty% > 0
  nop %self.remove_mob_flag(!TELEPORT)%
  nop %self.remove_mob_flag(HARD)%
  nop %self.remove_mob_flag(GROUP%
  * boss?
  if %boss_mobs% ~= %self.vnum%
    if %difficulty% == 2
      nop %self.add_mob_flag(HARD)%
    elseif %difficulty% == 3
      nop %self.add_mob_flag(GROUP)%
    elseif %difficulty% == 4
      nop %self.add_mob_flag(HARD)%
      nop %self.add_mob_flag(GROUP)%
    end
  elseif %mini_mobs% ~= %self.vnum% && %difficulty% > 2
    nop %self.add_mob_flag(HARD)%
  end
end
detach 11117 %self.id%
~
#11118
Burrow Canyons: Purge diff-sel on load~
1 n 100
~
set start %instance.start%
if %start.var(difficulty,0)% > 0
  * already scaled
  %purge% %self%
end
~
#11123
Start despawn time~
2 g 100
~
* This was formerly used to start the despawn timer when a player entered.
* The difficulty selector now handles this.
if !%self.contents(11124)%
  * start despawn
  %load% obj 11124
end
detach 11123 %self.id%
~
#11124
Burrow Canyons Timed Despawn~
1 f 0
~
* After 2 hours, ends the instance
%adventurecomplete%
~
#11125
Add up exit to BC~
2 n 100
~
set loc %instance.location%
if %loc%
  %door% %self% up room %loc.vnum%
end
~
#11127
Burrow Canyons: difficulty selector~
1 c 4
difficulty~
return 1
* Configs
set start_room 11100
set end_room 11123
set boss_mobs 11103 11105
set mini_mobs 11100 11102
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
        if %mob.vnum% >= 11100 && %mob.vnum% <= 11129
          * allow shadowstep
          nop %mob.remove_mob_flag(!TELEPORT)%
          * wipe difficulty flags
          if %mob.vnum% != 11101
            nop %mob.remove_mob_flag(HARD)%
            nop %mob.remove_mob_flag(GROUP)%
          end
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
      end
      * and loop
      set mob %next_mob%
    done
  end
  * and repeat
  eval vnum %vnum% + 1
done
* link room
set room %self.room%
if !%room.east(room)%
  %door% %room% northeast room i11101
end
* messaging:
%send% %actor% You shift the debris out of the way to reveal an open cavern.
%echoaround% %actor% ~%actor% shifts some debris out of the way to reveal an open cavern.
if %difficulty% > 2
  %echo% ... it looks dangerous.
end
* mark adventure as scaled
set start %instance.start%
if %start%
  remote difficulty %start.id%
end
if !%room.contents(11124)%
  * start despawn
  %load% obj 11124
end
%purge% %self%
~
#11130
Caretaker Replacement~
2 q 100
~
if !%actor.is_pc%
  halt
end
makeuid atroom room i11142
set found 0
set exists 0
set ch %atroom.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.vnum% == 11135
    %purge% %ch%
    set found 1
  elseif %ch.vnum% == 11136
    set exists 1
  end
  set ch %next_ch%
done
if %found% && !%exists%
  %at% i11142 wload mob 11136
  set mob %atroom.people%
  if %mob.vnum% == 11136
    set start %instance.start%
    if %start%
      set diff %start.var(difficulty,1)%
    else
      set diff 1
    end
    nop %mob.remove_mob_flag(HARD)%
    nop %mob.remove_mob_flag(GROUP)%
    if (%diff% // 2) == 0
      nop %mob.add_mob_flag(HARD)%
    end
    if %diff% >= 3
      nop %mob.add_mob_flag(GROUP)%
    end
    nop %mob.unscale_and_reset%
  end
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
  set hex_box_open 0
end
* Message
%send% %actor% You push the image of the garden on the side of @%self%.
%echoaround% %actor% ~%actor% pushes the image of the garden on the side of @%self%.
* Fail: wrong order
if %hex_box_open% != 0
  if %hex_box_open% > 0
    %send% %actor% To your dismay, the sigils fade on the sides of @%self%.
    %echoaround% %actor% To |%actor% dismay, the sigils fade on the sides of @%self%.
  else
    %send% %actor% ... nothing seems to happen.
  end
  set hex_box_open 0
  global hex_box_open
  halt
end
%send% %actor% To your surprise, the image of the garden begins to glow!
%echoaround% %actor% To |%actor% surprise, the image of the garden begins to glow!
set hex_box_open 1
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
  set hex_box_open 0
end
* Message
%send% %actor% You push the image of the mill on the side of @%self%.
%echoaround% %actor% ~%actor% pushes the image of the mill on the side of @%self%.
* Fail: wrong order
if %hex_box_open% != 1
  if %hex_box_open% > 0
    %send% %actor% To your dismay, the sigils fade on the sides of @%self%.
    %echoaround% %actor% To |%actor% dismay, the sigils fade on the sides of @%self%.
  else
    %send% %actor% ... nothing seems to happen.
  end
  set hex_box_open 0
  global hex_box_open
  halt
end
%send% %actor% The image of the mill begins to glow!
%echoaround% %actor% The image of the mill begins to glow!
set hex_box_open 2
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
  set hex_box_open 0
end
* Message
%send% %actor% You push the image of the stable on the side of @%self%.
%echoaround% %actor% ~%actor% pushes the image of the stable on the side of @%self%.
* Fail: wrong order
if %hex_box_open% != 2
  if %hex_box_open% > 0
    %send% %actor% To your dismay, the sigils fade on the sides of @%self%.
    %echoaround% %actor% To |%actor% dismay, the sigils fade on the sides of @%self%.
  else
    %send% %actor% ... nothing seems to happen.
  end
  set hex_box_open 0
  global hex_box_open
  halt
end
%send% %actor% The image of the stable begins to glow!
%echoaround% %actor% The image of the stable begins to glow!
set hex_box_open 3
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
  set hex_box_open 0
end
* Message
%send% %actor% You push the image of the estate on the side of @%self%.
%echoaround% %actor% ~%actor% pushes the image of the estate on the side of @%self%.
* Fail: wrong order
if %hex_box_open% != 3
  if %hex_box_open% > 0
    %send% %actor% To your dismay, the sigils fade on the sides of @%self%.
    %echoaround% %actor% To |%actor% dismay, the sigils fade on the sides of @%self%.
  else
    %send% %actor% ... nothing seems to happen.
  end
  set hex_box_open 0
  global hex_box_open
  halt
end
%send% %actor% The image of the estate begins to glow!
%echoaround% %actor% The image of the estate begins to glow!
wait 1 sec
%echo% The lid of @%self% dissolves and the box opens!
* replace with new item
%load% obj 11141
makeuid box obj openhexbox
%load% obj 11131 %box%
set chalice %box.contents%
if %chalice.vnum% == 11131%
  set start %instance.start%
  if %start%
    set difficulty %start.var(difficulty,1)%
  else
    set difficulty 1
  end
  remote difficulty %chalice.id%
end
%purge% %self%
~
#11135
Sarcophagus open~
1 c 4
open~
context %instance.id%
* Did they target me?
if %sarcophagus_running% || !%self.is_name(%arg%)%
  return 0
  halt
end
set sarcophagus_running 1
global sarcophagus_running
%send% %actor% You cautiously open @%self%...
%echoaround% %actor% ~%actor% cautiously opens @%self%...
%echo% A giant shape lurches at you from the darkness inside!
set room %self.room%
%load% mob 11138
%load% obj 11140
set snake %room.people%
if %snake.vnum% == 11138
  * difficulty
  set start %instance.start%
  if %start%
    set diff %start.var(difficulty,1)%
  else
    set diff 1
  end
  nop %snake.remove_mob_flag(HARD)%
  nop %snake.remove_mob_flag(GROUP)%
  if (%diff% // 2) == 0
    nop %snake.add_mob_flag(HARD)%
  end
  if %diff% >= 3
    nop %snake.add_mob_flag(GROUP)%
  end
  nop %snake.unscale_and_reset%
  * and aggro
  %force% %snake% %aggro% %actor%
end
%purge% %self%
set sarcophagus_running 0
global sarcophagus_running
~
#11136
Chalice combine~
1 b 100
~
set found_cutting 0
set found_pod 0
set found_berries 0
set found_redthorn 0
set iter %self.contents%
while %iter%
  set next_iter %iter.next_in_list%
  if (%iter.vnum% == 11142)
    set found_cutting 1
  elseif (%iter.vnum% == 11143)
    set found_pod 1
  elseif (%iter.vnum% == 11144)
    set found_berries 1
  elseif (%iter.vnum% == 1202)
    set found_redthorn 1
  else
    * not one of our ingredients -- destroy it
    if %self.carried_by%
      %send% %self.carried_by% @%iter% dissolves in @%self%.
    else
      %echo% @%iter% dissolves in @%self%.
    end
    %purge% %iter%
  end
  set iter %next_iter%
done
* Victory against chalice!
if (%found_cutting% == 1 && %found_pod% == 1 && %found_berries% == 1 && %found_redthorn% == 1)
  %echo% @%self% bursts into flames and melts into a slag of jewels and gold!
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
  %load% mob 11140
  set dragonfly %self.room.people%
  if %dragonfly.vnum% == 11140
    set diff %self.var(difficulty,1)%
    nop %dragonfly.remove_mob_flag(HARD)%
    nop %dragonfly.remove_mob_flag(GROUP)%
    if (%diff% // 2) == 0
      nop %dragonfly.add_mob_flag(HARD)%
    end
    if %diff% >= 3
      nop %dragonfly.add_mob_flag(GROUP)%
    end
    nop %dragonfly.unscale_and_reset%
    if %self.carried_by%
      %force% %dragonfly% %aggro% %self.carried_by%
    else
      %force% %dragonfly% %aggro%
    end
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
  %echo% @%self% has vanished.
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
if (%self.aff_flagged(IMMOBILIZED)% || %self.disabled%)
  halt
end
set target %random.enemy%
if !%target%
  * In case of blindness
  set target %actor%
end
%send% %target% ~%self% encircles you and constricts!
%echoaround% %target% ~%self% encircles ~%target% and constricts *%target%!
if %target.affect(3050)% || %target.level% >= (%self.level% + 100)
  * stun immunity or high level
  %damage% %target% 150
  dg_affect %self% HARD-STUNNED on 15
  wait 15 sec
  halt
else
  dg_affect %target% STUNNED on 15
  dg_affect %self% HARD-STUNNED on 15
  %damage% %target% 50
end
set verify_target %target.id%
wait 5 sec
if %verify_target% != %target.id%
  halt
end
%send% %target% ~%self% constricts and crushes you!
%echoaround% %target% ~%self% constricts and crushes ~%target%!
%damage% %target% 50
wait 5 sec
if %verify_target% != %target.id%
  halt
end
%send% %target% ~%self% constricts and crushes you!
%echoaround% %target% ~%self% constricts and crushes ~%target%!
%damage% %target% 50
wait 5 sec
if %verify_target% != %target.id%
  halt
end
%send% %target% ~%self% releases you!
%echoaround% %target% ~%self% releases ~%target%!
~
#11139
Sleeping Ivy combat~
0 k 50
~
* This is a mini-version of Titanconda's combat script (11138), and should
* train the player for it.
* Countered by good entangle (not always desirable)
if (%self.aff_flagged(IMMOBILIZED)% || %self.disabled%)
  halt
end
set target %random.enemy%
if !%target%
  halt
end
%send% %target% ~%self% wraps *%self%self around you, mummifying you and dragging you off to blissful slumber...
%echoaround% %target% ~%self% wraps *%self%self around ~%target%, mummifying *%target% and dragging *%target% off to blissful slumber...
if %target.affect(3050)% || %target.level% >= (%self.level% + 100)
  * stun immunity or high level
  %damage% %target% 50
  dg_affect %self% HARD-STUNNED on 15
  wait 15 sec
  halt
else
  dg_affect %target% STUNNED on 15
  dg_affect %self% HARD-STUNNED on 15
end
* prevents re-running of this script until affect ends
wait 15 sec
~
#11140
Lulling songbird combat~
0 k 20
~
set target %random.enemy%
if %target%
  %send% %target% ~%self% sings a sweet tune, lulling you and making you sluggish!
  %echoaround% %target% ~%self% sings a sweet tune, lulling ~%target% and making *%target% sluggish!
  dg_affect %target% SLOW on 15
  dg_affect %target% WITS -1 15
end
~
#11141
MM Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(11130)%
end
~
#11150
Mill Manor: Difficulty selector~
1 c 4
difficulty~
return 1
* Configs
set start_room 11131
set end_room 11151
set boss_mobs 11135 11136 11138
set mini_mobs 11130 11131 11132 11133 11134 11137 11139 11140
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
        if %mob.vnum% >= 11130 && %mob.vnum% <= 11159
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
      end
      * and loop
      set mob %next_mob%
    done
  end
  * and repeat
  eval vnum %vnum% + 1
done
* link room
set room %self.room%
if !%room.east(room)%
  %door% %room% east room i11131
end
* messaging:
%send% %actor% You cut your way through the thick vegetation on the path.
%echoaround% %actor% ~%actor% cuts ^%actor% way through the thick vegetation on the path.
if %difficulty% > 2
  %echo% ... it looks dangerous.
end
* mark adventure as scaled
set start %instance.start%
if %start%
  remote difficulty %start.id%
end
%purge% %self%
~
$
