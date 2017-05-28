#11000
No Portal Inside~
1 n 100
~
eval room %self.room%
if %room.template% == 11000
  * This object is inside the adventure
  %purge% %self%
else
  detach 11000 %self.id%
end
~
#11001
Add Nest Exit~
2 n 100
~
eval loc %instance.location%
if %loc%
  %door% %room% down room %loc.vnum%
end
~
#11002
Smash / Steal Roc fight~
0 k 100
~
eval healthprct (100 * %actor.health%) / %actor.maxhealth%
if %healthprct% < 90
  * Destroy the egg
  eval egg %actor.inventory(11053)%
  if %egg%
    %send% %actor% The fight with %self.name% destroys the stolen egg!
    %purge% %egg%
  end
end
if %self.vnum% == 11002
  nop %actor.move(-5)%
end
* If we want a regular fight trigger, put it down here
if %random.5% != 1
  halt
end
switch %random.4%
  case 1
    %echo% %self.name% flaps %self.hisher% wings violently...
    %echo% You are battered by strong winds!
    %aoe% 50 physical
  break
  case 2
    %send% %actor% %self.name% buffets you with %self.hisher% wing, knocking your weapon away!
    %echoaround% %actor% %self.name% buffets %actor.name% with %self.hisher% wing, knocking %actor.hisher% weapon away!
    dg_affect %actor% DISARM on 5
  break
  case 3
    %send% %actor% %self.name% stabs at you with a series of powerful pecks!
    %echoaround% %actor% %self.name% stabs at %actor.name% with a series of powerful pecks!
    %damage% %actor% 50 physical
    %send% %actor% You are bleeding!
    %dot% %actor% 50 15 physical 1
  break
  case 4
    %echo% %self.name% lets out a piercing screech!
    eval ch %room.people%
    while %ch%
      eval test %%self.is_enemy(%ch%)%%
      if %test%
        dg_affect %ch% DODGE -10 10
        dg_affect %ch% TO-HIT -10 10
        %send% %actor% You are momentarily deafened by the loud noise!
      end
    done
  break
done
~
#11003
Start Smash Quest~
2 u 100
~
%load% mob 11000
%load% obj 11021
eval mob %room.people%
%force% %mob% %aggro% %actor%
~
#11006
Hatch/Protect Finisher~
2 v 100
~
%load% mob 11004
%load% mob 11005
* Delay-complete
%load% obj 11058
~
#11007
Cattails unclaimed decay~
0 ab 100
~
eval room %self.room%
eval cycles_left 3
while %cycles_left% >= 0
  if (%self.room% != %room%) || %room.empire% || %room.crop% != zephyr cattails
    * We've moved or someone else harvested
    %echo% %self.name% looks around for more cattails to steal.
    nop %self.add_mob_flag(SPAWNED)%
    nop %self.remove_mob_flag(SENTINEL)%
    detach 11007 %self.id%
    halt
  end
  if %self.fighting% || %self.disabled%
    * Combat interrupts the ritual
    %echo% %self.name%'s harvesting is interrupted.
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 3
      %echo% %self.name% begins to harvest the zephyr cattails.
    break
    case 2
      %echo% %self.name% carefully harvests the zephyr cattails.
    break
    case 1
      %echo% %self.name% walks through the field, harvesting the zephyr cattails.
    break
    case 0
      %echo% %self.name% finishes harvesting the zephyr cattails!
      %terraform% %room% 0
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
%echo% %self.name% looks confused...
~
#11008
Give seeds if no seeds or cattails~
2 u 100
~
if !%actor.inventory(11008)% && !%actor.inventory(11009)%
  nop %actor.add_resources(11008, 1)%
  eval item %actor.inventory()%
  %send% %actor% You find %item.shortdesc%.
end
~
#11009
Roc nest forage for trees~
2 c 0
forage~
eval num 4
%send% %actor% You forage around and find a large tree (x%num%)!
%echoaround% %actor% %actor.name% forages around and finds a large tree (x%num%)
eval give %%actor.add_resources(120, %num%)%%
nop %give%
detach 11009 %self.id%
~
#11010
Scatter random corpses~
0 b 50
~
eval room %self.room%
eval distance %%room.distance(%instance.location%)%%
if %distance% > 10
  mgoto %instance.location%
end
eval room %self.room%
eval item %room.contents%
while %item%
  if %item.vnum% == 11022 || %item.vnum% == 11023
    * Already a corpse here
    halt
  end
  eval item %item.next_in_list%
done
eval vnum (11022-1) + %random.2%
%load% obj %vnum% %self.room%
eval item %room.contents%
%echo% You find %item.shortdesc% nearby!
* Look for the corpses made variable
if %self.varexists(corpses_made)%
  eval corpses_made %self.corpses_made% + 1
else
  eval corpses_made 1
end
* Corpse limit
if %corpses_made% >= 10
  %purge% %self%
else
  remote corpses_made %self.id%
end
~
#11011
Escape adventure and mmove~
0 n 100
~
eval room %self.room%
if (!%instance.location% || %room.template% != 11000)
  halt
end
mgoto %instance.location%
mmove
mmove
mmove
mmove
~
#11012
Roc Hatchling break egg on hatch~
0 n 100
~
%echo% The egg begins to vibrate and crack...
wait 1
%echo% %self.name% hatches from the egg!
eval room %self.room%
eval obj %room.contents%
while %obj%
  eval next_obj %obj.next_in_list%
  if %obj.vnum% == 11001
    %purge% %obj%
  end
  eval obj %next_obj%
done
detach 11012 %self.id%
~
#11017
Baby Ostrich emotes~
0 btw 5
~
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    %echo% %self.name% runs circles around you.
  break
  case 2
    %echo% %self.name% hops up and down, flapping its fluffy wings.
  break
  case 3
    %echo% %self.name% scratches at the ground with its oversized feet.
  break
  case 4
    %echo% %self.name% sticks its head in a little hole in the ground, perhaps looking for food.
  break
done
~
#11018
Stealth quest start~
2 u 100
~
* Egg
%load% obj 11053 %actor% inv
* Delayed despawner, removes egg from nest
%load% obj 11021
* Move the thief out
* %teleport% %actor% %instance.location%
* %force% %actor% look
%force% %actor% down
* Load pursuing roc (who leaves in trig 11058)
%load% mob 11002
eval mob %room.people%
%force% %mob% mhunt %actor%
* Alert region
%regionecho% %instance.location% 10 An angry screech echoes across the land!
~
#11019
Start Protect Egg~
2 u 100
~
%load% obj 11054 %actor% inv
~
#11020
Begin hunt~
0 n 100
~
wait 5 sec
* exit nest
down
~
#11021
Delayed Completer~
1 f 0
~
%adventurecomplete%
~
#11022
Delayed despawner remove roc egg~
1 n 100
~
wait 1
eval room %self.room%
eval obj %room.contents%
while %obj%
  eval next_obj %obj.next_in_list%
  if %obj.vnum% == 11001
    %purge% %obj%
  end
  eval obj %next_obj%
done
~
#11024
Combat Roc Death~
0 f 100
~
%load% mob 11001
eval room %self.room%
eval mob %room.people%
%echo% %mob.name% shows up just at the last second!
~
#11025
Late adventurer announce~
0 n 100
~
wait 1 sec
say Ach! I'm always late to the party. Ah well, if you want to trade your corrupted talons for some goods, type list.
~
#11026
Shady Thief announce~
0 n 100
~
wait 1 sec
say This egg is exactly what the buyer wanted. While I'm here, type list if you want to trade your corrupted talons for some goods.
~
#11027
Block Nest Entry~
2 g 100
~
if %actor.inventory(11053)%
  %send% %actor% You can't get back into the nest right now!
  return 0
end
~
#11032
Wings of Daedalus decay~
1 j 0
~
if %self.timer% > 0
  halt
end
%send% %actor% The wax holding %self.shortdesc% together begins to slowly melt...
otimer 24
~
#11034
Clockwork Roc Interior~
5 n 100
~
eval inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.fore%)
  %door% %inter% fore add 11035
end
if (!%inter.aft%)
  %door% %inter% aft add 11036
end
eval cage %inter.aft(room)%
if (%cage% && !%cage.down%)
  %door% %cage% down add 11037
end
~
#11052
incredible reward replacer~
1 n 100
~
wait 1
eval actor %self.carried_by%
if %actor%
  * Roll for mount
  eval percent_roll %random.100%
  if %percent_roll% <= 4
    * Minipet
    eval vnum 11017
  else
    eval percent_roll %percent_roll% - 4
    if %percent_roll% <= 4
      * Land mount
      eval vnum 11019
    else
      eval percent_roll %percent_roll% - 4
      if %percent_roll% <= 2
        * Sea mount
        eval vnum 11020
      else
        eval percent_roll %percent_roll% - 2
        if %percent_roll% <= 2
          * Flying mount
          eval vnum 11018
        else
          eval offset (%random.8%-1)*2
          eval vnum 11035 + %offset%
        end
      end
    end
  end
  if %self.level%
    eval level %self.level%
  else
    eval level 100
  end
  %load% obj %vnum% %actor% inv %level%
  eval item %%actor.inventory(%vnum%)%%
  %send% %actor% %self.shortdesc% turns out to be %item.shortdesc%!
  if %item.is_flagged(BOE)%
    nop %item.flag(BOE)%
  end
  if !%item.is_flagged(BOP)%
    nop %item.flag(BOP)%
  end
  eval do_bind %%item.bind(%actor%)%%
  nop %do_bind%
end
%purge% %self%
~
#11053
Stolen egg expiry~
1 f 0
~
eval actor %self.carried_by%
if !%actor%
  return 1
  halt
end
if %actor.fighting%
  * If the egg timer expires: if the player is fighting, they lose the egg.
  eval enemy %actor.fighting%
  %send% %actor% The fight with %enemy.name% destroys the stolen egg!
  %purge% %self%
  halt
else
  * Otherwise, spawn mob 11003, the shady thief.
  %load% mob 11003
  eval room %actor.room%
  eval mob %room.people%
  * Despawn the pursuing roc...
  eval pursuer %instance.mob(11002)%
  if %pursuer%
    %at% %pursuer.room% %echo% %self.name% flies away.
    %purge% %pursuer%
  end
  * Take the egg and set the steal quest complete.
  %quest% %actor% trigger 11002
  %send% %actor% %mob.name% takes the egg from you.
  %send% %actor% Type 'quest finish Steal the Egg' to complete the quest.
  %purge% %self%
  halt
end
* Impossible to get here
~
$
