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
%junk% %object%
~
#256
Hestian Trinket~
1 c 2
use~
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
if !%actor.home%
  %send% %actor% You have no home to teleport back to with this trinket.
  halt
end
* once per 60 minutes
if %actor.varexists(last_hestian_time)%
  if (%timestamp% - %actor.last_hestian_time%) < 3600
    %send% %actor% You have used a hestian trinket too recently.
    halt
  end
end
eval room_var %actor.room%
%send% %actor% You touch %self.shortdesc% and it begins to swirl with light...
%echoaround% %actor% %actor.name% touches %self.shortdesc% and it begins to swirl with light...
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor%
  halt
end
%send% %actor% %self.shortdesc% glows a bright blue and the light begins to envelop you!
%echoaround% %actor% %self.shortdesc% glows a bright blue and the light begins to envelop %actor.name%!
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor%
  halt
end
%echoaround% %actor% %actor.name% vanishes in a flash of light!
%teleport% %actor% %actor.home%
%force% %actor% look
eval last_hestian_time %timestamp%
remote last_hestian_time %actor.id%
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
while %item% && !%found
  eval test %%item.is_name(%arg%)%%
  if %test%
    eval found %item%
  end
  eval item %item.next_in_list%
done
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
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
* once per 30 minutes
if %actor.varexists(last_conveyance_time)%
  if (%timestamp% - %actor.last_conveyance_time%) < 1800
    %send% %actor% You have used a trinket of conveyance too recently.
    halt
  end
end
eval room_var %actor.room%
%send% %actor% You touch %self.shortdesc% and it begins to swirl with light...
%echoaround% %actor% %actor.name% touches %self.shortdesc% and it begins to swirl with light...
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || %self.carried_by% != %actor%
  halt
end
%send% %actor% Yellow light begins to whirl around you...
%echoaround% %actor% Yellow light begins to whirl around %actor.name%...
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || %self.carried_by% != %actor%
  halt
end
%echoaround% %actor% %actor.name% vanishes in a flourish of yellow light!
%teleport% %actor% %startloc%
%force% %actor% look
eval last_conveyance_time %timestamp%
remote last_conveyance_time %actor.id%
%purge% %self%
~
$
