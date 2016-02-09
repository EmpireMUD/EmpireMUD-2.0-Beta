#10500
Glowkra consume~
1 s 100
~
if !(eat /= %command%)
halt
end
%send% %actor% As you consume %self.shortdesc%, your skin starts to glow slightly.
dg_affect %actor% INFRA on 300
~
#10501
Magiterranean Terracrop~
0 ab 15
~
if (%self.fighting% || %self.disabled%)
  halt
end
eval room %self.room%
if !%instance.location%
  %echo% %self.name% vanishes in a swirl of leaf-green mana!
  %purge% %self%
  halt
end
eval dist %%room.distance(%instance.location%)%%
if (%room.template% == 10500 || %dist% > 3 || %room.building% == Fence || %room.building% == Wall)
  %echo% %self.name% vanishes in a swirl of leaf-green mana!
  mgoto %instance.location%
  %echo% %self.name% appears in a swirl of leaf-green mana!
  * Flag for despawn if distance was too far
  if (%dist% > 3)
    nop %self.add_mob_flag(SPAWNED)%
  end
elseif (%room.empire_id% || (%room.sector% != Plains && %room.sector% != Crop && %room.sector% != Seeded Field && %room.sector% != Jungle Crop && %room.sector% != Jungle Field && !(%room.sector% ~= Forest) && !(%room.sector% ~= Jungle)))
  * No Work
  halt
elseif (%room.sector% ~= Crop && (%room.crop% == glowkra || %room.crop% == spadebrush || %room.crop% == magic mushrooms || %room.crop% == pudding figs || %room.crop% == dragonsteeth || %room.crop% == giant beanstalks))
  * No Work -- split in 2 parts for line length limit
  halt
elseif (%room.sector% ~= Crop && (%room.crop% == axeroot || %room.crop% == puppy plants || %room.crop% == gemfruit || %room.crop% == bladegrass || %room.crop% == pickthorn || %room.crop% == vigilant vines))
  * No Work -- split in 2 parts for line length limit
  halt
else
  * pick a crop -- use start of time as jan 1, 2015: 1420070400
  * 2628288 seconds in a month
  eval month ((%timestamp% - 1420070400) / 2628288) // 12
  eval vnum -1
  switch (%month% + 1)
    case 1
      eval vnum 10501
    break
    case 2
      eval vnum 10508
    break
    case 3
      eval vnum 10504
    break
    case 4
      eval vnum 10505
    break
    case 5
      eval vnum 10511
    break
    case 6
      eval vnum 10503
    break
    case 7
      eval vnum 10507
    break
    case 8
      eval vnum 10509
    break
    case 9
      eval vnum 10500
    break
    case 10
      eval vnum 10502
    break
    case 11
      eval vnum 10510
    break
    case 12
      eval vnum 10506
    break
  done
  if (%vnum% == -1)
    halt
  end
  * do!
  %terracrop% %room% %vnum%
  %echo% %self.name% spreads mana over the land and crops begin to grow!
end
~
#10502
Magic Mushroom eat~
1 s 100
~
dg_affect %actor% STONED on 75
~
#10503
Basket of Magic Mushrooms eat~
1 s 100
~
dg_affect %actor% STONED on 900
~
#10504
Puppy receive treat~
0 j 100
~
eval treat 10528
if %object.vnum% == %treat%
  %echo% %self.name% chomps down happily on %object.shortdesc%!
  %purge% %object%
  wait 1 sec
  switch %random.3%
    case 1
      emote sits obediently and barks!
    break
    case 2
      emote wags its tail happily!
    break
    case 3
      %send% %actor% %self.name% runs circles around your legs!
      %echoaround% %actor% %self.name% runs circles around %actor.name%'s legs!
    break
  done
end
~
#10505
Dragontooth Sceptre Summon~
1 c 1
use~
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
eval varname summon%target%
eval test %%actor.varexists(%varname%)%%
* Change this if trigger vnum != mob vnum
eval target %self.vnum%
* once per 30 minutes
if %test%
  eval tt %%actor.%varname%%%
  if (%timestamp% - %tt%) < 1800
    eval diff (%tt% - %timestamp%) + 1800
    eval diff2 %diff%/60
    eval diff %diff%//60
    if %diff%<10
      set diff 0%diff%
    end
    %send% %actor% You must wait %diff2%:%diff% to use %self.shortdesc% again.
    halt
  end
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
%load% m %target%
eval room_var %self.room%
eval mob %room_var.people%
if (%mob% && %mob.vnum% == %target%)
  %own% %mob% %actor.empire%
  %send% %actor% You use %self.shortdesc% and %mob.name% appears!
  %echoaround% %actor% %actor.name% uses %self.shortdesc% and %mob.name% appears!
  eval %varname% %timestamp%
  remote %varname% %actor.id%
end
~
#10506
Puppy Plant use~
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
* check too many mobs
eval mobs 0
eval found 0
eval room_var %self.room%
eval ch %room_var.people%
while %ch% && !%found%
  if (%ch.is_npc% && %ch.vnum% >= 10501 && %ch.vnum% <= 10504 && %ch.master% && %ch.master% == %actor%)
    eval found 1
  elseif %ch.is_npc%
    eval mobs %mobs% + 1
  end
  eval ch %ch.next_in_room%
done
if %found%
  %send% %actor% You already have a puppy plant pet.
elseif %mobs% > 4
  %send% %actor% There are too many mobs here already.
else
  %send% %actor% You use %self.shortdesc%...
  %echoaround% %actor% %actor.name% uses %self.shortdesc%...
  eval vnum 10501 + %random.4% - 1
  %load% m %vnum%
  eval pet %room_var.people%
  if (%pet% && %pet.vnum% == %vnum%)
    %force% %pet% mfollow %actor%
    %echo% %pet.name% appears!
  end
end
~
#10507
Interdimensional Whirlwind Cleanup~
2 e 100
~
* pick a crop -- use start of time as jan 1, 2015: 1420070400
* 2628288 seconds in a month
eval month ((%timestamp% - 1420070400) / 2628288) // 12
eval vnum -1
switch (%month% + 1)
  case 1
    eval vnum 10501
  break
  case 2
    eval vnum 10508
  break
  case 3
    eval vnum 10504
  break
  case 4
    eval vnum 10505
  break
  case 5
    eval vnum 10511
  break
  case 6
    eval vnum 10503
  break
  case 7
    eval vnum 10507
  break
  case 8
    eval vnum 10509
  break
  case 9
    eval vnum 10500
  break
  case 10
    eval vnum 10502
  break
  case 11
    eval vnum 10510
  break
  case 12
    eval vnum 10506
  break
done
if (%vnum% == -1)
  halt
end
* do!
%terracrop% %room% %vnum%
%echo% As the whirlwind fades away, crops burst from the soil here!
~
#10508
Dragonstooth sceptre equip first~
1 c 2
use~
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
%send% %actor% You must wield %self.shortdesc% to use it.
return 1
~
#10514
Gemfruit decay~
1 f 0
~
eval object nothing
switch %random.4%
  case 1
    * iris
    eval object an iridescent blue iris
    %load% o 1206 %self.carried_by%
  break
  case 2
    * bloodstone
    eval object a red bloodstone
    %load% o 104 %self.carried_by%
  break
  case 3
    * seashell
    eval object a glowing green seashell
    %load% o 1300 %self.carried_by%
  break
  case 4
    * lightning stone
    eval object a yellow lightning stone
    %load% o 103 %self.carried_by%
  break
done
if %self.carried_by%
  %send% %self.carried_by% %self.shortdesc% disintegrates, leaving behind %object%!
else
  %echo% %self.shortdesc% disintegrates, leaving behind %object%!
end
%purge% %self%
~
$
