#221
Stealth GM Bribe coins~
0 m 100
~
%echoaround% %actor% %self.name% and %actor.name% seem to be whispering to each other.
if (%actor.skill(Stealth)% > 0)
  %send% %actor% %self.name% whispers, 'It looks like you're already a member of the Guild.'
elseif (!%actor.can_gain_new_skills%)
  %send% %actor% %self.name% whispers, 'It doesn't look like you can learn any new skills.'
else
  %actor.set_skill(Stealth, 1)%
  %send% %actor% %self.name% whispers, 'I suppose I could let you into the Guild, provisionally.'
end
~
#222
Stealth GM Bribe item~
0 j 100
~
%echoaround% %actor% %self.name% and %actor.name% seem to be whispering to each other.
if (%object.material% != GOLD)
  %send% %actor% %self.name% whispers, 'Thanks, mate!'
elseif (%actor.skill(Stealth)% > 0)
  %send% %actor% %self.name% whispers, 'It looks like you're already a member of the Guild.'
elseif (!%actor.can_gain_new_skills%)
  %send% %actor% %self.name% whispers, 'It doesn't look like you can learn any new skills.'
else
  %actor.set_skill(Stealth, 1)%
  %send% %actor% %self.name% whispers, 'I suppose I could let you into the Guild, provisionally.'
end
%purge% %object%
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
eval home %actor.home%
if !%home%
  %send% %actor% You have no home to teleport back to with this trinket.
  halt
end
eval veh %home.in_vehicle%
if %veh%
  eval outside_room %veh.room%
  eval test %%actor.canuseroom_guest(%outside_room%)%%
  eval test2 eval test %%actor.can_teleport_room(%outside_room%)%%
  if !%test%
    %send% %actor% You can't teleport home to a vehicle that's parked on foreign territory you don't have permission to use!
    halt
  elseif !%test2%
    %send% %actor% You can't teleport to your home's current location.
    halt
  end
end
* once per 60 minutes
if %actor.varexists(last_hestian_time)%
  if (%timestamp% - %actor.last_hestian_time%) < 3600
    eval diff (%actor.last_hestian_time% - %timestamp%) + 3600
    eval diff2 %diff%/60
    eval diff %diff%//60
    if %diff%<10
      set diff 0%diff%
    end
    %send% %actor% You must wait %diff2%:%diff% to use %self.shortdesc% again.
    halt
  end
end
eval room_var %actor.room%
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
eval last_hestian_time %timestamp%
remote last_hestian_time %actor.id%
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
eval item %actor.inventory()%
eval found 0
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
if %actor.varexists(last_conveyance_time)%
  if (%timestamp% - %actor.last_conveyance_time%) < 1800
    eval diff (%actor.last_conveyance_time% - %timestamp%) + 1800
    eval diff2 %diff%/60
    eval diff %diff%//60
    if %diff%<10
      set diff 0%diff%
    end
    %send% %actor% You must wait %diff2%:%diff% to use %self.shortdesc%.
    halt
  end
end
eval room_var %actor.room%
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
eval last_conveyance_time %timestamp%
remote last_conveyance_time %actor.id%
nop %actor.cancel_adventure_summon%
%purge% %self%
~
#263
Letheian Icon use~
1 c 2
use~
eval item %arg.car%
eval sk %arg.cdr%
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
