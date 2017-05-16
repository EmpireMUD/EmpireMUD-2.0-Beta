#10300
Flame Dragon Terrorize~
0 ab 10
~
if (%self.fighting% || %self.disabled%)
  halt
end
eval room %self.room%
if (%instance.location% && (%room.template% == 10300 || (%room% != %instance.location% && %random.10% == 10)))
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
elseif ((%room.sector% ~= Forest || %room.sector% ~= Jungle) && %room.sector% != Enchanted Forest)
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
Flame Dragon delay-completer~
0 f 100
~
if %instance.start%
  * Attempt delayed despawn
  %at% %instance.start% %load% o 10316
else
  %adventurecomplete%
end
~
#10304
Flame Dragon environmental~
0 bw 5
~
* This script is no longer used. It was replaced by custom strings.
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
#10307
Flame dragon despawn timer~
1 f 0
~
%adventurecomplete%
~
#10330
Abandoned Dragon Fly Home~
0 ab 10
~
if (%self.fighting% || %self.disabled%)
  halt
end
eval room %self.room%
if (%instance.location% && %room% != %instance.location% && (%room.template% == 10330 || %room.sector% == Ocean))
  %echo% %self.name% flies away!
  mgoto %instance.location%
  %echo% %self.name% flies into the nest!
end
~
#10331
Abandoned Nest Spawner~
1 n 100
~
eval vnum 10330 + %random.4% - 1
%load% m %vnum%
%purge% %self%
~
#10334
Abandoned Dragon animation~
0 bw 5
~
if (%self.fighting% || %self.disabled%)
  halt
end
switch %random.4%
  case 1
    %echo% %self.name% flies in circles overhead.
  break
  case 2
    %echo% %self.name% spurts fire into the air.
  break
  case 3
    %echo% %self.name% eyes you warily.
  break
  case 4
    %echo% %self.name% swoops low, then soars back into the air.
  break
done
~
#10335
Dragon Whistle use~
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
%load% m %self.val0%
%send% %actor% You use %self.shortdesc% and a dragon mount appears!
%echoaround% %actor% %actor.name% uses %self.shortdesc% and a dragon mount appears!
%purge% %self%
~
#10336
Non-Mount Summon~
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
%load% m %self.val0%
eval room_var %self.room%
eval mob %room_var.people%
if (%mob% && %mob.vnum% == %self.val0%)
  %send% %actor% You use %self.shortdesc% and %mob.name% appears!
  %echoaround% %actor% %actor.name% uses %self.shortdesc% and %mob.name% appears!
  nop %mob.unlink_instance%
end
%purge% %self%
~
#10337
Fire Ox animation~
0 bw 5
~
if (%self.fighting% || %self.disabled%)
  halt
end
if (%random.2% == 2)
  %echo% %self.name% releases a demonic moo, and fire spurts from %self.hisher% nostrils.
else
  * We need the current terrain.
  eval room %self.room%
  if (%room.sector% == Plains || %room.sector% ~= Forest)
    %echo% %self.name% scorches some grass, and eats it.
  else
    %echo% %self.name% spurts fire from %self.hisher% nostrils.
  end
end
~
#10338
Dragonguard animation~
0 bw 5
~
if (%self.fighting% || %self.disabled%)
  halt
end
switch %random.5%
  case 1
    say The only thing that would make this day more beautiful is fire, raining from the sky.
  break
  case 2
    say Pleasant and eternal greetings.
  break
  case 3
    say May the burning eye watch over you.
  break
  case 4
    say All humans must die. Eventually.
  break
  case 5
    say Mortals tremble before the Dragonguard.
  break
done
~
#10339
Empire Non-Mount Summon~
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
%load% m %self.val0%
eval room_var %self.room%
eval mob %room_var.people%
if (%mob% && %mob.vnum% == %self.val0%)
  %own% %mob% %actor.empire%
  %send% %actor% You use %self.shortdesc% and %mob.name% appears!
  %echoaround% %actor% %actor.name% uses %self.shortdesc% and %mob.name% appears!
end
%purge% %self%
~
$
