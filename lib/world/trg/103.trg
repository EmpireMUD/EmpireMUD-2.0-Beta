#10300
Flame Dragon Terrorize~
0 ab 10
~
if (%self.fighting% || %self.disabled%)
  halt
end
eval room %self.room%
if (%instance.location% && (%room.template% == 10300 || (%room% != %instance.location% && %random.15% == 15)))
  %echo% %self.name% flies away!
  mgoto %instance.location%
  %echo% %self.name% flies into the cave!
elseif (%room.sector% == Crop || %room.sector% == Seeded Field || %room.sector% == Jungle Crop || %room.sector% == Jungle Field)
  %echo% %self.name% scorches the crops!
  %terraform% %room% 10303
elseif (%room.sector% == Desert Crop || %room.sector% == Sandy Field)
  %echo% %self.name% scorches the crops!
  %terraform% %room% 10304
elseif (%room.sector% == Desert)
  %echo% %self.name% scorches the desert!
  %terraform% %room% 10305
elseif (%room.sector% ~= Forest || %room.sector% ~= Jungle)
  %echo% %self.name% scorches the trees!
  %terraform% %room% 10300
elseif (%room.sector% == Grove)
  %echo% %self.name% scorches the grove!
  %terraform% %room% 10301
elseif (%room.sector% == Plains)
  %echo% %self.name% scorches the plains!
  %terraform% %room% 10302
end
~
#10301
Mini-pet Use~
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
eval varname minipet%self.val0%
eval test %%actor.varexists(%varname%)%%
* once per 30 minutes
if %test%
  eval tt %%actor.%varname%%%
  if (%timestamp% - %tt%) < 1800
    %send% %actor% You have used %self.shortdesc% too recently.
    halt
  end
end
* check too many mobs
eval mobs 0
eval found 0
eval room_var %self.room%
eval ch %room_var.people%
while %ch% && !%found%
  if (%ch.is_npc% && %ch.vnum% == %self.val0% && %ch.master% && %ch.master% == %actor%)
    eval found 1
  elseif %ch.is_npc%
    eval mobs %mobs% + 1
  end
  eval ch %ch.next_in_room%
done
if %found%
  %send% %actor% You already have this mini-pet.
elseif %mobs% > 4
  %send% %actor% There are too many mobs here already.
else
  %send% %actor% You use %self.shortdesc%...
  %echoaround% %actor% %actor.name% uses %self.shortdesc%...
  %load% m %self.val0%
  eval pet %room_var.people%
  if (%pet% && %pet.vnum% == %self.val0%)
    %force% %pet% mfollow %actor%
    %echo% %pet.name% appears!
    eval %varname% %timestamp%
    remote %varname% %actor.id%
  end
end
~
#10302
Flame Dragon combat~
0 k 5
~
eval chance %random.3%
if (chance < 3)
  * Searing burns on tank
  %send% %actor% %self.name% spits fire at you, causing searing burns!
  %echoaround% %actor% %self.name% spits fire at %actor.name%, causing searing burns!
  %dot% %actor% 100 60 fire
  %damage% %actor% 75 fire
else
  * Flame wave AoE
  %echo% %self.name% begins puffing smoke and spinning in circles!
  * Give the healer (if any) time to prepare for group heals
  wait 3 sec
  %echo% %self.name% unleashes a flame wave!
  %aoe% 100 fire
end
~
#10303
Flame Dragon overcompleter~
0 f 100
~
%adventurecomplete%
* in case:
if %instance.start%
  %at% %instance.start% %adventurecomplete%
end
~
#10304
Flame Dragon environmental~
0 b 5
~
if (%self.fighting% || %self.disabled%)
  halt
end
switch %random.4%
  case 1
    %echo% %self.name% spurts fire into the air.
  break
  case 2
    %echo% %self.name% curls up and begins puffing clouds of smoke.
  break
  case 3
    %echo% %self.name% hunkers down and starts coughing out bits of ash.
  break
  case 4
    %echo% %self.name% coughs up some charred bone fragments.
  break
done
~
#10305
Dismissable~
0 c 0
dismiss~
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
if (%self.master% && %self.master% == %actor%)
  %send% %actor% You dismiss %self.name%.
  %echoaround %actor% %actor.name% dismisses %self.name%.
  %purge% %self%
else
  * ordinary dismiss error
  return 0
end
~
#10306
Lonely Despawn~
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
* Despawn if no players present
if %count% < 1
  %purge% %self%
end
~
$
