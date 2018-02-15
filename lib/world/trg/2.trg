#221
Stealth GM Bribe coins: Prevent~
0 m 0
~
%send% %actor% %self.name% doesn't take bribes like that any more. Complete %self.hisher% quest instead.
return 0
~
#222
Stealth GM Bribe item: Prevent~
0 j 100
~
%send% %actor% %self.name% doesn't take bribes like that any more. Complete %self.hisher% quest instead.
return 0
~
#250
City Guard: Distract~
0 k 50
~
if %self.cooldown(250)%
  halt
end
nop %self.set_cooldown(250, 30)%
%send% %actor% %self.name%'s attacks leave you distracted.
%echoaround% %actor% %actor.name% looks distracted by %self.name%'s attacks.
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
set person %room.people%
set ally_guards_present 0
while %person%
  eval test %%person.is_enemy(%self%)%%
  if %test% && %person.is_pc% && %person.empire% != %self.empire%
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
%echo% %self.name% calls for reinforcements!
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
    %echo% %self.name% looks confused.
    halt
  end
  if %self.empire%
    %own% %summon% %self.empire%
  end
  %echo% %summon.name% arrives!
  %force% %summon% mkill %actor%
  eval num %num% + 1
done
~
#256
Hestian Trinket~
1 c 2
use~
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
set home %actor.home%
if !%home%
  %send% %actor% You have no home to teleport back to with this trinket.
  halt
end
set veh %home.in_vehicle%
if %veh%
  set outside_room %veh.room%
  eval test %%actor.canuseroom_guest(%outside_room%)%%
  eval test2 %%actor.can_teleport_room(%outside_room%)%%
  if !%test%
    %send% %actor% You can't teleport home to a vehicle that's parked on foreign territory you don't have permission to use!
    halt
  elseif !%test2%
    %send% %actor% You can't teleport to your home's current location.
    halt
  end
end
* once per 60 minutes
if %actor.cooldown(256)%
  %send% %actor% Your %cooldown.256% is on cooldown!
  halt
end
set room_var %actor.room%
%send% %actor% You touch %self.shortdesc% and it begins to swirl with light...
%echoaround% %actor% %actor.name% touches %self.shortdesc% and it begins to swirl with light...
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)%
  halt
end
%send% %actor% %self.shortdesc% glows a bright blue and the light begins to envelop you!
%echoaround% %actor% %self.shortdesc% glows a bright blue and the light begins to envelop %actor.name%!
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)%
  halt
end
%echoaround% %actor% %actor.name% vanishes in a flash of light!
%teleport% %actor% %actor.home%
%force% %actor% look
%echoaround% %actor% %actor.name% appears in a flash of light!
nop %actor.set_cooldown(256, 3600)%
nop %actor.cancel_adventure_summon%
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
set found 0
eval found %%actor.obj_target(%arg%)%%
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
%send% %actor% You smoke %found.shortdesc% in %self.shortdesc%.
%echoaround% %actor% %actor.name% smokes %found.shortdesc% in %self.shortdesc%.
%purge% %found%
~
#262
Trinket of Conveyance~
1 c 2
use~
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
* once per 30 minutes
if %actor.cooldown(262)%
  %send% %actor% Your %cooldown.262% is still on cooldown!
  halt
end
set room_var %actor.room%
%send% %actor% You touch %self.shortdesc% and it begins to swirl with light...
%echoaround% %actor% %actor.name% touches %self.shortdesc% and it begins to swirl with light...
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)%
  halt
end
%send% %actor% Yellow light begins to whirl around you...
%echoaround% %actor% Yellow light begins to whirl around %actor.name%...
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)%
  halt
end
%echoaround% %actor% %actor.name% vanishes in a flourish of yellow light!
%teleport% %actor% %startloc%
%force% %actor% look
%echoaround% %actor% %actor.name% appears in a flourish of yellow light!
nop %actor.set_cooldown(262, 1800)%
nop %actor.cancel_adventure_summon%
%purge% %self%
~
#263
Letheian Icon use~
1 c 2
use~
set item %arg.car%
set sk %arg.cdr%
eval test %%actor.obj_target(%item%)%%
if (%test% != %self%) && (use /= %cmd%)
  return 0
  halt
end
if !%sk%
  %send% %actor% Usage: use icon <skill>
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
eval test %%skill.validate(%sk%)%%
if !%test%
  %send% %actor% No such skill '%sk%'.
  halt
end
eval name %%skill.name(%sk%)%%
%send% %actor% You use %self.shortdesc% and gain a skill reset in %name%!
%echoaround% %actor% %actor.name% uses %self.shortdesc%.
eval grant %%actor.give_skill_reset(%sk%)%%
nop %grant%
%purge% %self%
~
$
