#205
Bodyguard refresh and cooldown on death~
0 ft 100
~
* partial copy of 9803
* This script deletes a companion's entry when it dies.
* If the companion comes from an ability (like Bodyguard) the player will
* get a fresh copy of the companion right away.
set ch %self.companion%
if %self.is_npc% && %ch% && !%ch.is_npc%
  %ch.remove_companion(%self.vnum%)%
  nop %ch.set_cooldown(2030,180)%
end
~
#221
Stealth GM Bribe coins: Prevent~
0 m 0
~
%send% %actor% ~%self% doesn't take bribes like that any more. Complete ^%self% quest instead.
return 0
~
#222
Stealth GM Bribe item: Prevent~
0 j 100
~
%send% %actor% ~%self% doesn't take bribes like that any more. Complete ^%self% quest instead.
return 0
~
#230
Summon Thug load script~
0 n 100
~
* cancels follow and sets loyalty to current room's empire
if %self.leader%
  mfollow self
end
set room %self.room%
if %room.empire%
  %own% %self% %room.empire%
else
  %own% %self% none
end
detach 230 %self.id%
~
#232
Barrel of Fun: Setup command~
1 c 6
setup~
set room %actor.room%
if %actor.obj_target(%arg.argument1%)% != %self%
  return 0
  halt
elseif %actor.fighting%
  %send% %actor% You're a little busy right now.
  halt
elseif %room.sector_flagged(OCEAN)% || %room.sector_flagged(FRESH-WATER)%
  %send% %actor% You can't set it up here.
  halt
elseif !%actor.canuseroom_guest%
  %send% %actor% You can't set it up here.
  halt
end
* Check items in room?
set count 0
set iter %room.contents%
while %iter% && %count% < 10
  eval count %count% + 1
  set iter %iter.next_in_list%
done
if %count% >= 10
  %send% %actor% There's too much here to set up @%self%.
  halt
end
* ok: ensure I'm in the room
%teleport% %self% %room%
otimer 168
nop %self.remove_wear(TAKE)%
%mod% %self% longdesc &Z%self.shortdesc% has been set up here.
if !%self.is_flagged(NO-BASIC-STORAGE)%
  nop %self.flag(NO-BASIC-STORAGE)%
end
* and message
%send% %actor% You set up @%self% here.
%echoaround% %actor% ~%actor% sets up @%self% here.
* and remove triggers
detach 232 %self.id%
~
#233
Barrel of Fun: Block 2nd setup~
1 c 4
setup~
if %actor.obj_target(%arg.argument1%)% == %self%
  %send% %actor% It has already been set up.
  halt%
else
  return 0
end
~
#250
City Guard: Distract~
0 k 50
~
if %self.cooldown(250)%
  halt
end
nop %self.set_cooldown(250, 30)%
%send% %actor% |%self% attacks leave you distracted.
%echoaround% %actor% ~%actor% looks distracted by |%self% attacks.
dg_affect #251 %actor% DISTRACTED on 30
~
#251
City Guard: Reinforcements~
0 k 100
~
if %self.cooldown(250)%
  halt
end
set room %self.room%
if %room.empire% != %self.empire% || !%room.in_city%
  * Not my empire / not my city
  halt
end
set person %room.people%
set ally_guards_present 0
while %person%
  if %person.is_enemy(%self%)% && %person.is_pc% && %person.empire% != %self.empire%
    set found_hostile_player 1
  elseif %person.vnum% == %self.vnum%
    eval ally_guards_present %ally_guards_present% + 1
  end
  set person %person.next_in_room%
done
if !%found_hostile_player%
  halt
end
if %ally_guards_present% >= 3
  halt
end
nop %self.set_cooldown(250, 30)%
%echo% ~%self% calls for reinforcements!
wait 10 sec
if !%self.fighting%
  halt
end
set count %random.3%
set num 1
while %num% <= %count%
  %load% mob %self.vnum%
  set summon %room.people%
  if %summon.vnum% != %self.vnum%
    %echo% ~%self% looks confused.
    halt
  end
  if %self.empire%
    %own% %summon% %self.empire%
  end
  %echo% ~%summon% arrives!
  %force% %summon% mkill %actor%
  eval num %num% + 1
done
~
#256
Hestian Trinket~
1 c 2
use~
* HESTIAN TRINKET: basic timed teleport home; paramenters:
* val0: cooldown vnum (-1 for none, defaults to 256 if 0)
* val1: cooldown time (defaults to 3600 if 0)
* script1: the teleport sequence sent to the character, sent in order
*   first script1 message is when the actor touches/uses it
*   remaining script1 messages are sent to the actor every 5 seconds
* script2: the teleport sequence sent to the room, sent in order
*   script1 and script2 must match up with the same number of messages
* script3 (custom): the actor-disappears message, sent to the room
* script4 (custom): the actor-appears message, sent to the room
* script5 (custom): cancel mesasge (to actor)
set default_cooldown_vnum 256
set default_cooldown_time 3600
* checks
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if %actor.position% != Standing || %actor.action% || %actor.fighting%
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
set home %actor.home%
if !%home%
  %send% %actor% You have no home to teleport back to with @%self% (see HELP HOME).
  halt
end
set veh %home.in_vehicle%
if %veh%
  set outside_room %veh.room%
  if !%actor.canuseroom_guest(%outside_room%)%
    %send% %actor% You can't teleport home to a vehicle that's parked on foreign territory you don't have permission to use!
    halt
  elseif !%actor.can_teleport_room(%outside_room%)%
    %send% %actor% You can't teleport to your home's current location.
    halt
  end
end
* detect configs
if %self.val0% == -1
  set cooldown_vnum 0
elseif %self.val0% == 0
  set cooldown_vnum %default_cooldown_vnum%
else
  set cooldown_vnum %self.val0%
end
if %self.val1% == -1
  set cooldown_time 0
elseif %self.val1% == 0
  set cooldown_time %default_cooldown_time%
else
  set cooldown_time %self.val1%
end
* once per 60 minutes
if %cooldown_vnum% && %actor.cooldown(%cooldown_vnum%)%
  eval cooldown_name %%cooldown.%cooldown_vnum%%%
  %send% %actor% Your %cooldown_name% is still on cooldown!
  halt
end
set room_var %actor.room%
* set up 'stop' command
set stop_command 0
set stop_message_char You stop using %self.shortdesc%.
set stop_message_room ~%actor% stops using %self.shortdesc%.
set needs_stop_command 1
remote stop_command %actor.id%
remote stop_message_char %actor.id%
remote stop_message_room %actor.id%
remote needs_stop_command %actor.id%
* start going
set to_ch %self.custom(script1,0)%
if !%to_ch.empty%
  %send% %actor% %to_ch.process%
else
  %send% %actor% You touch @%self% and it begins to swirl with light...
end
set to_room %self.custom(script2,0)%
if !%to_room.empty%
  %echoaround% %actor% %to_room.process%
else
  %echoaround% %actor% ~%actor% touches @%self% and it begins to swirl with light...
end
set message 1
set done 0
while !%done%
  wait 5 sec
  * check cancellation
  if %actor.stop_command% || %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)% || %actor.action%
    %force% %actor% stop cleardata
    set to_ch %self.custom(script5)%
    if !%to_ch.empty%
      %send% %actor% %to_ch.process%
    else
      %send% %actor% The swirling light from @%self% fades.
    end
    halt
  end
  * check next message
  set to_ch %self.custom(script1,%message%)%
  set to_room %self.custom(script2,%message%)%
  if %to_ch.empty% || %to_room.empty%
    set done 1
  else
    %send% %actor% %to_ch.process%
    %echoaround% %actor% %to_room.process%
  end
  eval message %message% + 1
done
* vanish message
set to_room %self.custom(script3)%
if !%to_room.empty%
  %echoaround% %actor% %to_room.process%
else
  %echoaround% %actor% ~%actor% vanishes in a flash of light!
end
* teleport
%teleport% %actor% %actor.home%
%force% %actor% look
* arrive message
set to_room %self.custom(script4)%
if !%to_room.empty%
  %echoaround% %actor% %to_room.process%
else
  %echoaround% %actor% ~%actor% appears in a flash of light!
end
* cleanup
if %cooldown_vnum% > 0 && %cooldown_time% > 0
  nop %actor.set_cooldown(%cooldown_vnum%, %cooldown_time%)%
end
nop %actor.cancel_adventure_summon%
%force% %actor% stop cleardata
~
#257
Start Brick Tutorial~
2 u 0
~
* start chop tutorial
if !%actor.completed_quest(2100)% && !%actor.on_quest(2100)%
  %quest% %actor% start 2100
end
* start firesticks tutorial
if !%actor.completed_quest(102)% && !%actor.on_quest(102)%
  %quest% %actor% start 102
end
~
#258
Peace Pipe Toker~
1 c 2
smoke~
if !%arg%
  %send% %actor% Smoke what?
  halt
end
set item %actor.inventory()%
set found %actor.obj_target(%arg%)%
if !%found%
  %send% %actor% You don't seem to have that.
  halt
elseif %found.vnum% == 1201
  * Fiveleaf
  dg_affect %actor% STONED on 450
else
  %send% %actor% You can't smoke that!
  halt
end
* Smokables drop through to here
%send% %actor% You smoke @%found% in @%self%.
%echoaround% %actor% ~%actor% smokes @%found% in @%self%.
%purge% %found%
~
#262
Trinket of Conveyance~
1 c 2
use~
* TRINKET OF CONVEYANCE: teleports the player to a starting location; params:
* val0: cooldown vnum (-1 for none, defaults to 262 if 0)
* val1: cooldown time (defaults to 1800 if 0)
* script1: the teleport sequence sent to the character, sent in order
*   first script1 message is when the actor touches/uses it
*   remaining script1 messages are sent to the actor every 5 seconds
* script2: the teleport sequence sent to the room, sent in order
*   script1 and script2 must match up with the same number of messages
* script3 (custom): the actor-disappears message, sent to the room
* script4 (custom): the actor-appears message, sent to the room
* script5 (custom): cancel mesasge (to actor)
set default_cooldown_vnum 262
set default_cooldown_time 1800
* checks
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if %actor.position% != Standing || %actor.action% || %actor.fighting%
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
* detect configs
if %self.val0% == -1
  set cooldown_vnum 0
elseif %self.val0% == 0
  set cooldown_vnum %default_cooldown_vnum%
else
  set cooldown_vnum %self.val0%
end
if %self.val1% == -1
  set cooldown_time 0
elseif %self.val1% == 0
  set cooldown_time %default_cooldown_time%
else
  set cooldown_time %self.val1%
end
* once per 30 minutes
if %cooldown_vnum% && %actor.cooldown(%cooldown_vnum%)%
  eval cooldown_name %%cooldown.%cooldown_vnum%%%
  %send% %actor% Your %cooldown_name% is still on cooldown!
  halt
end
set room_var %actor.room%
* set up 'stop' command
set stop_command 0
set stop_message_char You stop using %self.shortdesc%.
set stop_message_room ~%actor% stops using %self.shortdesc%.
set needs_stop_command 1
remote stop_command %actor.id%
remote stop_message_char %actor.id%
remote stop_message_room %actor.id%
remote needs_stop_command %actor.id%
* start going
set to_ch %self.custom(script1,0)%
if !%to_ch.empty%
  %send% %actor% %to_ch.process%
else
  %send% %actor% You touch @%self% and it begins to swirl with light...
end
set to_room %self.custom(script2,0)%
if !%to_room.empty%
  %echoaround% %actor% %to_room.process%
else
  %echoaround% %actor% ~%actor% touches @%self% and it begins to swirl with light...
end
set message 1
set done 0
while !%done%
  wait 5 sec
  * check cancellation
  if %actor.stop_command% || %actor.room% != %room_var% || %actor.fighting% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)% || %actor.action%
    %force% %actor% stop cleardata
    set to_ch %self.custom(script5)%
    if !%to_ch.empty%
      %send% %actor% %to_ch.process%
    else
      %send% %actor% The swirling light from @%self% fades.
    end
    halt
  end
  * check next message
  set to_ch %self.custom(script1,%message%)%
  set to_room %self.custom(script2,%message%)%
  if %to_ch.empty% || %to_room.empty%
    set done 1
  else
    %send% %actor% %to_ch.process%
    %echoaround% %actor% %to_room.process%
  end
  eval message %message% + 1
done
* vanish message
set to_room %self.custom(script3)%
if !%to_room.empty%
  %echoaround% %actor% %to_room.process%
else
  %echoaround% %actor% ~%actor% vanishes in a flourish of yellow light!
end
* teleport
%teleport% %actor% %startloc%
%force% %actor% look
* arrive message
set to_room %self.custom(script4)%
if !%to_room.empty%
  %echoaround% %actor% %to_room.process%
else
  %echoaround% %actor% ~%actor% appears in a flourish of yellow light!
end
* cleanup
if %cooldown_vnum% > 0 && %cooldown_time% > 0
  nop %actor.set_cooldown(%cooldown_vnum%, %cooldown_time%)%
end
nop %actor.cancel_adventure_summon%
%force% %actor% stop cleardata
%purge% %self%
~
#263
Letheian Icon use~
1 c 2
use~
set item %arg.argument1%
set sk %arg.argument2%
if (%actor.obj_target(%item%)% != %self%) && (use /= %cmd%)
  return 0
  halt
end
if !%sk%
  set name %self.name%
  %send% %actor% You must specify which skill you want a reset for.
  %send% %actor% Usage: use %name.car% <skill>
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
if !%skill.validate(%sk%)%
  %send% %actor% No such skill '%sk%'.
  halt
end
%send% %actor% You use @%self% and gain a skill reset in %skill.name(%sk%)%!
%echoaround% %actor% ~%actor% uses @%self%.
nop %actor.give_skill_reset(%sk%)%
%purge% %self%
~
$
