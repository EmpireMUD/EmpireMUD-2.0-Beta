#10400
Hamlet guard intro speech~
0 g 100
~
if (%direction% != south)
  halt
end
eval intro_running 1
global intro_running
wait 5
say Oh, thank goodness, a hero!
wait 3 sec
say There are cultists burning the hamlet. Necromancers, I think.
wait 3 sec
say The hamlet is burning. Most of it is probably lost.
wait 3 sec
say Maybe you could at least clear out the undead, so we can salvage what's left in our homes?
wait 3 sec
say Please, if you could. There's no army out here. We need a hero!
wait 3 sec
say Northwest is Market Street. There's two giant skeletons roaming the street, but that seems to be the easiest of the bunch.
wait 3 sec
say To the North is Main Street. It's covered in demons, led by an actual Infernomancer. Looks to be a bit more of a challenge.
wait 3 sec
say Lastly, Northeast is Temple Street, overrun by the Necromancers and their risen minions. You'll need a group for that lot!
eval intro_running 0
global intro_running
~
#10401
Hamlet intro exit block~
0 s 100
~
if (!%actor.nohassle% && %intro_running%)
  %send% %actor% You can't leave during the intro.
  return 0
end
~
#10402
Market Street trash spawner~
1 n 100
~
* Ensure no mobs here
eval room_var %self.room%
eval ch %room_var.people%
eval found 0
while %ch% && !%found%
  if (%ch.is_npc%)
    eval found %found% + 1
  end
  eval ch %ch.next_in_room%
done
if (%found% == 0)
  switch %random.3%
    case 1
      %load% mob 10401
      %load% mob 10401
    break
    case 2
      %load% mob 10402
    break
    case 3
      %load% mob 10403
    break
  done
end
%purge% %self%
~
#10403
Main Street trash spawner~
1 n 100
~
* Ensure no mobs here
eval room_var %self.room%
eval ch %room_var.people%
eval found 0
while %ch% && !%found%
  if (%ch.is_npc%)
    eval found %found% + 1
  end
  eval ch %ch.next_in_room%
done
if (%found% == 0)
  switch %random.3%
    case 1
      %load% mob 10406
    break
    case 2
      %load% mob 10407
    break
    case 3
      %load% mob 10408
    break
  done
end
%purge% %self%
~
#10404
Temple Street trash spawner~
1 n 100
~
* Ensure no mobs here
eval room_var %self.room%
eval ch %room_var.people%
eval found 0
while %ch% && !%found%
  if (%ch.is_npc%)
    eval found %found% + 1
  end
  eval ch %ch.next_in_room%
done
if (%found% == 0)
  switch %random.3%
    case 1
      %load% mob 10411
      %load% mob 10411
    break
    case 2
      %load% mob 10412
    break
    case 3
      %load% mob 10413
    break
  done
end
%purge% %self%
~
#10405
Mob block higher template id~
0 s 100
~
* One quick trick to get the target room
eval room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
%send% %actor% You can't seem to get past %self.name%!
return 0
~
#10406
Spawn portal home on death~
0 f 100
~
* Just spawn, no message
%load% obj 10400
~
#10407
Blockade environmental~
1 bw 3
~
%echo% The makeshift blockade crackles in the fire.
~
#10408
Hamlet of the Undead env~
2 bw 3
~
switch %random.4%
  case 1
    %echo% The shrieks of fleeing citizens pierce the air.
  break
  case 2
    %echo% Flames dance up the faces of the buildings on all sides.
  break
  case 3
    %echo% The low chanting of necrocultists can barely be heard over the burning hamlet.
  break
  case 4
    %echo% The stench of death permeates the air.
  break
end
~
#10409
Necromancer combat~
0 k 20
~
if !%self.affect(counterspell)%
  counterspell
else
  switch %random.4%
    case 1
      if %actor.trigger_counterspell%
        %send% %actor% %self.name% shoots a bolt of violet energy at you, but it breaks on your counterspell!
        %echoaround% %actor% %self.name% shoots a bolt of violet energy at %actor.name%, but it breaks on %actor.hisher% counterspell!
      else
        %send% %actor% %self.name% shoots a bolt of violet energy at you!
        %echoaround% %actor% %self.name% shoots a bolt of violet energy at %actor.name%!
        %damage% %actor% 100 magical
        %dot% %actor% 100 15 magical
      end
    break
    case 2
      %send% %actor% %self.name% stares into your eyes. You can't move!
      %echoaround% %actor% %self.name% stares into %actor.name%'s eyes. %actor.name% seems unable to move!
      dg_affect %actor% STUNNED on 10
    break
    case 3
      %echo% %self.name% raises %self.hisher% arms and the floor begins to rumble...
      wait 2 sec
      %echo% Bone hands rise from the floor and claw at you!
      %aoe% 100 physical
    break
    case 4
      eval target %random.enemy%
      if (%target%)
        if %target.trigger_counterspell%
          %send% %target% %self.name% shoots a bolt of violet energy at you, but it breaks on your counterspell!
          %echoaround% %target% %self.name% shoots a bolt of violet energy at %target.name%, but it breaks on %target.hisher% counterspell!
        else
          %send% %target% %self.name% shoots a bolt of violet energy at you!
          %echoaround% %target% %self.name% shoots a bolt of violet energy at %target.name%!
          %damage% %target% 100 magical
          %dot% %target% 100 15 magical
        end
      end
    break
  done
end
~
#10410
Skeletal combat~
0 k 8
~
bash
~
#10411
Necrofiend Combat~
0 k 10
~
switch %random.2%
  case 1
    %send% %actor% %self.name% shoots a poisoned spine at you!
    %echoaround% %actor% %self.name% shoots a poisoned spine at %actor.name%!
    %damage% %actor% 50 physical
    if (%actor.poison_immunity% || (%actor.resist_poison% && %random.100% < 50))
      %dot% %actor% 100 15 poison
    end
  break
  case 2
    eval target %random.enemy%
    if (%target%)
      %send% %target% %self.name% shoots a poisoned spine at you!
      %echoaround% %target% %self.name% shoots a poisoned spine at %target.name%!
      %damage% %target% 50 physical
      if (%target.poison_immunity% || (%target.resist_poison% && %random.100% < 50))
        %dot% %target% 100 15 poison
      end
    end
  break
done
~
#10412
Infernomancer combat~
0 k 8
~
if !%self.affect(counterspell)%
  counterspell
else
  switch %random.3%
    case 1
      if %actor.trigger_counterspell%
        %send% %actor% %self.name% hurls a flaming meteor at you, but it fizzles on your counterspell!
        %echoaround% %actor% %self.name% hurls a flaming meteor at %actor.name%, but it fizzles on %actor.hisher% counterspell!
      else
        %send% %actor% %self.name% hurls a flaming meteor at you!
        %echoaround% %actor% %self.name% hurls a flaming meteor at %actor.name%!
        %damage% %actor% 100 fire
        %dot% %actor% 100 15 fire
      end
    break
    case 2
      %echo% %self.name% raises %self.hisher% arms and flames swirl around %self.himher%!
      wait 2 sec
      %echo% Waves of flame wash over you, and smoke chokes your lungs!
      %aoe% 100 fire
    break
    case 3
      eval target %random.enemy%
      if (%target%)
        if %target.trigger_counterspell%
          %send% %target% %self.name% hurls a flaming meteor at you, but it fizzles on your counterspell!
          %echoaround% %target% %self.name% hurls a flaming meteor at %target.name%, but it fizzles on %target.hisher% counterspell!
        else
          %send% %target% %self.name% hurls a flaming meteor at you!
          %echoaround% %target% %self.name% hurls a flaming meteor at %target.name%!
          %damage% %target% 100 fire
          %dot% %target% 100 15 fire
        end
      end
    break
  done
end
~
#10416
Fleeing Citizen env~
0 bw 10
~
switch %random.4%
  case 1
    say The undead are everywhere!
  break
  case 2
    say The necrocultists are burning our city!
  break
  case 3
    say They've burnt down everything!
  break
  case 4
    say There's a skeletal mammoth in the town square!
  break
end
~
#10417
Shambling Zombie env~
0 bw 10
~
switch %random.4%
  case 1
    say Urrrrgggghh...
  break
  case 2
    %echo% %self.name% looks hungry.
  break
  case 3
    %echo% %self.name% picks its jaw up off the ground.
  break
  case 4
    say Brrrnnnn...
  break
end
~
#10418
Mount summon use~
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
%load% m %self.val0%
%send% %actor% You use %self.shortdesc% and a new mount appears!
%echoaround% %actor% %actor.name% uses %self.shortdesc% and a new mount appears!
eval room_var %self.room%
eval mob %room_var.people%
if (%mob% && %mob.vnum% == %self.val0%)
  nop %mob.unlink_instance%
end
%purge% %self%
~
#10450
Sewer Ladder: Exit~
1 c 4
exit~
%force% %actor% enter exit
~
#10451
Sewer Ladder: Climb~
1 c 4
climb~
%force% %actor% enter exit
~
#10452
Sewer Environment~
2 bw 5
~
switch %random.4%
  case 1
    %echo% The sound of dripping echoes in the distance.
  break
  case 2
    %echo% The stench of raw sewage nearly makes you gag.
  break
  case 3
    %echo% Something furry brushes past your ankle!
  break
  default
    %echo% The rancid smell of waste is thick in the air.
  break
done
~
#10453
Goblin Outpost Melee~
0 k 10
~
switch %random.2%
  case 1
    kick
  break
  case 2
    jab
  break
done
~
#10456
Wargreyn combat~
0 k 8
~
switch %random.2%
  case 1
    bash
  break
  case 2
    disarm
  break
done
~
#10457
Dryleef purchase~
0 c 0
buy~
eval vnum -1
set named a thing
if (!%arg%)
  %send% %actor% Type 'look sign' to see what's available.
  halt
elseif war tent /= %arg%
  eval vnum 10479
  set named a goblin war tent
elseif message post /= %arg%
  eval vnum 10480
  set named a goblin message post
elseif lab tent /= %arg%
  eval vnum 10481
  set named a goblin lab tent
elseif dolmen stone /= %arg%
  eval vnum 10482
  set named a goblin dolmen stone
elseif ticket /= %arg% && %actor.has_item(18260)%
  * Fall through to adventurer guild quest command trigger
  return 0
  halt
else
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
if !%actor.can_afford(500)%
  %send% %actor% %self.name% tells you, 'Human needs 500 coin to buy that.'
  halt
end
nop %actor.charge_coins(500)%
%load% obj %vnum% %actor% inv
%send% %actor% You buy %named% for 500 coins.
%echoaround% %actor% %actor.name% buys %named%.
~
#10458
Pimmin purchase~
0 c 0
buy~
eval vnum -1
set named a thing
if (!%arg%)
  %send% %actor% Type 'look sign' to see what's available.
  halt
elseif violet leopard shawl pattern /= %arg%
  eval vnum 10473
  set named the violet leopard shawl pattern
elseif wildfire gloves pattern /= %arg%
  eval vnum 10474
  set named the wildfire gloves pattern
else
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
if !%actor.can_afford(500)%
  %send% %actor% %self.name% tells you, 'Human needs 500 coin to buy that.'
  halt
end
nop %actor.charge_coins(500)%
%load% obj %vnum% %actor% inv
%send% %actor% You buy %named% for 500 coins.
%echoaround% %actor% %actor.name% buys %named%.
~
#10459
Shivsper Purchase~
0 c 0
buy~
eval vnum -1
set named a thing
set ratfur rat-fur cloak pattern
if (!%arg%)
  %send% %actor% Type 'look sign' to see what's available.
  halt
elseif %ratfur% /= %arg%
  eval vnum 10475
  set named the rat-fur cloak pattern
elseif wooden gauntlets pattern /= %arg%
  eval vnum 10476
  set named the wooden gauntlets pattern
else
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
if !%actor.can_afford(500)%
  %send% %actor% %self.name% tells you, 'Human needs 500 coin to buy that.'
  halt
end
nop %actor.charge_coins(500)%
%load% obj %vnum% %actor% inv
%send% %actor% You buy %named% for 500 coins.
%echoaround% %actor% %actor.name% buys %named%.
~
#10460
Wargreyn buy~
0 c 0
buy~
eval vnum -1
set named a thing
if (!%arg%)
  %send% %actor% Type 'look sign' to see what's available.
  halt
elseif mesh cloak pattern /= %arg%
  eval vnum 10477
  set named the mesh cloak pattern
elseif heavy goblin gauntlets pattern /= %arg%
  eval vnum 10478
  set named the heavy goblin gauntlets pattern
else
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
if !%actor.can_afford(500)%
  %send% %actor% %self.name% tells you, 'Human needs 500 coin to buy that.'
  halt
end
nop %actor.charge_coins(500)%
%load% obj %vnum% %actor% inv
%send% %actor% You buy %named% for 500 coins.
%echoaround% %actor% %actor.name% buys %named%.
~
$
