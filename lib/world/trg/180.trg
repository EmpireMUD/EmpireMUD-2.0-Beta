#18075
Fissure eruption~
2 ab 1
~
%regionecho% %room% 10 You feel a sudden wave of heat from the nearby fissure!
wait 2 sec
%regionecho% %room% 5 There is a deep, rumbling roar from far beneath your feet!
%asound% The fissure erupts!
%echo% A pillar of molten rock the size of a castle flies right past you!
~
#18076
Fiend Battle~
0 k 25
~
eval test 100*%self.health%/%self.maxhealth%
set phase 1
if %test%<67
  set phase 2
end
if %test%<34
  set phase 3
end
if !%lastphase%
  set lastphase phase
  remote lastphase %self.id%
end
if %lastphase%!=%phase%
  if %lastphase% < %phase%
    *we've advanced a phase
    %echo% ~%self% roars with fury!
    if %phase% == 2
      %echo% One of |%self% chains shatters!
    end
    if %phase% == 3
      %echo% |%self% second chain shatters, freeing it!
    end
  end
  set lastphase %phase%
  remote lastphase %self.id%
  halt
end
if %phase%==1
  switch %random.3%
    case 1
      if !%self.affect(counterspell)%
        counterspell
      else
        if !%actor.affect(colorburst)%
          colorburst
        else
          heal
        end
      end
    break
    case 2
      if !%actor.affect(colorburst)%
        colorburst
      else
        heal
      end
    break
    case 3
      heal
    break
  done
end
if %phase%==2
  switch %random.3%
    case 1
      set room %self.room%
      %echo% ~%self% whistles with a sound like escaping steam...
      %echo% A pair of fire elementals appear!
      %load% mob 18077 ally %self.level%
      %force% %room.people% %aggro% %actor%
      %load% mob 18077 ally %self.level%
      %force% %room.people% %aggro% %actor%
      %echo% ~%self% looks exhausted!
      *Ensure that no other abilities are used while the elementals are up:
      dg_affect %self% STUNNED on 30
      wait 25 sec
    break
    case 2
      %echo% ~%self% raises a hand, leaning back from the platform...
      wait 5 sec
      if %self.disabled%
        %echo% |%self% attack is interrupted!
        halt
      end
      *Buff dodge massively but debuff own tohit massively...
      *should be a breather for average geared parties:
      dg_affect %self% TO-HIT -100 30
      dg_affect %self% SLOW on 30
      dg_affect %self% DODGE 100 30
      %echo% A flickering barrier of force surrounds ~%self%!
      wait 30 sec *Make sure he doesn't do anything else while the shield is up.
      %echo% The magical shield around ~%self% fades!
    break
    case 3
      %echo% ~%self% draws back a hand, curling it into a fist...
      wait 2 sec
      if %self.disabled%
        %echo% |%self% attack is interrupted!
        halt
      end
      %echo% The lava below begins to rise!
      wait 3 sec
      if %self.disabled%
        %echo% |%self% attack is interrupted!
        halt
      end
      %echo% &&rA wave of magma rolls over the platform!&&0
      %aoe% 50 fire
      %send% %actor% &&rYou take the full force of the wave, and are badly burned!&&0
      %echoaround% %actor% ~%actor% takes the full force of the wave!
      %damage% %actor% 100 fire
      %dot% %actor% 100 20 fire
      wait 1 sec
      %echo% The lava sinks back down.
    break
  done
end
if %phase%==3
  switch %random.3%
    case 1
      %echo% ~%self% raises a huge, molten fist!
      wait 5 sec
      *(dex*5)% chance to not get hit
      if %actor.dex%<%random.20%
        %send% %actor% &&rThe earth shakes as ~%self% punches you, stunning you!&&0
        %echoaround% %actor% The earth shakes as ~%self% punches ~%actor%, stunning *%actor%!
        %damage% %actor% 100 physical
        %damage% %actor% 50 fire
        dg_affect %actor% HARD-STUNNED on 10
      else
        %send% %actor% You leap out of the way of |%self% fist at the last second!
        %echoaround% %actor% ~%actor% leaps out of the way of |%self% fist at the last second!
        %send% %actor% &&rThe wave of heat from |%self% fist burns you!&&0
        %damage% %actor% 50 fire
      end
    break
    case 2
      set target %random.enemy%
      * Unable to find a target = return self
      if !%target% || target == %self%
        halt
      end
      %send% %target% ~%self% makes a heretical gesture at you!
      %echoaround% %target% ~%self% makes a heretical gesture at ~%target%!
      wait 5 sec
      if %target%&&(%target.room%==%self.room%)
        if %target.trigger_counterspell%
          %send% %target% A tornado of harmless smoke briefly surrounds you!
          %echoaround %target% A tornado of harmless smoke briefly surrounds %target%!
        else
          %send% %target% &&rYour body bursts into infernal flames!&&0
          %echoaround% %target% |%target% body bursts into infernal flames!
          %damage% %target% 25 fire
          %dot% %target% 300 30 fire
        end
      end
    break
    case 3
      %echo% ~%self% rises up out of the lava entirely!
      wait 5 sec
      *Should not be hitting much during this attack
      dg_affect %self% TO-HIT -100 25
      %echo% &&RMana begins condensing around ~%self%!&&0
      wait 5 sec
      %echo% &&RThe platform starts shaking beneath your feet!&&0
      wait 5 sec
      %echo% &&rWaves of fire flood the room!&&0
      %aoe% 100 fire
      wait 5 sec
      %echo% &&rA barrage of sharp rocks falls from the ceiling!&&0
      %aoe% 100 physical
      wait 5 sec
      %echo% &&rThe floating motes of mana in the air transform into ethereal blades!&&0
      %aoe% 50 magical
      %aoe% 50 physical
      wait 5 sec
      %echo% ~%self% sinks back into the lava...
    break
  done
end
*global cooldown
wait 10 sec
~
#18077
Fiend adds timer~
0 n 100
~
wait 30 sec
%echo% ~%self% burns out, disappearing with a puff of smoke!
%purge% %self%
~
#18078
Fiend adds battle~
0 k 20
~
makeuid fiend mob chained
if %random.3%<3
  %echo% ~%self% transfers some of its heat to ~%fiend%!
  %damage% %fiend% -100
  %damage% %self% 100
else
  %send% %actor% &&rThere is a destructive explosion as ~%self% hurls itself suicidally at you!&&0
  %echoaround% %actor% &&rThere is a destructive explosion as ~%self% hurls itself suicidally at ~%actor%!&&0
  %damage% %actor% 200 fire
  %aoe% 50 fire
  %damage% %self% 1000
end
wait 5 sec *Global cooldown
~
#18079
Fiend No Leave~
0 s 100
~
if %actor.nohassle%
  halt
end
%send% %actor% You try to leave, but a wall of fire blocks your escape!
%echoaround% %actor% ~%actor% tries to leave, but a wall of fire blocks ^%actor% escape!
return 0
~
#18080
Fiend Immunities~
0 p 100
~
if !(%abilityname%==disarm)
  halt
end
%send% %actor% You cannot disarm ~%self% - ^%self% magic is innate!
return 0
~
#18081
Fiend No Flee~
0 c 0
flee~
%send% %actor% You turn to flee, but a wall of fire blocks your escape!
%echoaround% %actor% ~%actor% turns to flee, but a wall of fire blocks ^%actor% escape!
~
#18082
Fissure drop destruction~
2 h 100
~
if %object.vnum% == 18097
  * Block people from wasting essence by accident
  %send% %actor% There's no point in dropping essence into the fissure.
  return 0
  halt
end
%send% %actor% You drop @%object% into the fissure!
%echoaround% %actor% ~%actor% drops @%object% into the fissure!
%echo% The fire and darkness below your feet swallow @%object%...
if (%object.vnum% >= 18075) && (%object.vnum% <= 18094)
  %load% obj 18097 %actor% inv %object.level%
  %send% %actor% The molten essence released by the destruction of @%object% gathers around you.
  %echoaround% %actor% The molten essence released by the destruction of @%object% gathers around ~%actor%.
end
%purge% %object%
return 0
~
#18083
Molten essence identify~
1 c 6
identify~
if !%self.is_name(%arg%)%
  halt
end
return 0
wait 1
set level 250
%send% %actor% This essence is from an item of level: %level%.
~
#18084
Fissure shop list~
2 c 0
list~
* If player has no essence, ignore the command
if !%actor.has_resources(18097,1)%
  return 0
  halt
end
%send% %actor% &&0All items cost 10 essence each and will be the average level of the
%send% %actor% &&0essence spent to make them. All items are created exactly the same as if the
%send% %actor% &&0fiend dropped that item, but bound to you alone.
%send% %actor% You can create (with 'buy <item>') any of the following items:
%send% %actor% &&0
%send% %actor% firecloth skirt             (caster legs)
%send% %actor% firecloth pauldrons         (caster arms)
%send% %actor% hearthflame skirt           (healer legs)
%send% %actor% hearthflame sleeves         (healer arms)
%send% %actor% magma-forged pauldrons      (tank arms)
%send% %actor% core-forged plate armor     (tank armor)
%send% %actor% huge spiked pauldrons       (melee arms)
%send% %actor% fiendskin jerkin            (melee armor)
%send% %actor% bottomless pouch            (general pack)
%send% %actor% igneous shield              (hybrid shield)
%send% %actor% cold lava ring              (caster ring)
%send% %actor% mantle of flames            (caster about)
%send% %actor% frozen flame earrings       (healer ears)
%send% %actor% emberweave gloves           (healer hands)
%send% %actor% molten gauntlets            (tank hands)
%send% %actor% molten fiend earrings       (tank ears)
%send% %actor% Flametalon                  (melee dagger)
%send% %actor% obsidian earring            (melee ears)
%send% %actor% crown of abyssal flames     (healer head)
%send% %actor% fiendhorn bladed bow        (tank ranged)
~
#18085
Molten fiend shop buy~
2 c 0
buy~
* This trigger is REALLY LONG.
* If player has no essence, ignore the command
if !%actor.has_resources(18097,1)%
  return 0
  halt
end
if !%arg%
  %send% %actor% Create what from essence?
  halt
end
* If player doesn't have enough essence, give an error
if !%actor.has_resources(18097,10)%
  %send% %actor% You need 10 essence.
  return 1
  halt
end
* What are we buying?
set check %arg%
set vnum 0
set name nothing
if firecloth /= %check%
  %send% %actor% Did you mean firecloth skirt or firecloth pauldrons?
  halt
elseif hearthflame s /= %check%
  %send% %actor% Did you mean hearthflame skirt or hearthflame sleeves?
  halt
elseif molten /= %check%
  %send% %actor% Did you mean molten gauntlets or molten fiend earrings?
  halt
elseif fiend /= %check%
  %send% %actor% Did you mean fiendskin jerkin or fiendhorn bladed bow?
  halt
elseif firecloth skirt /= %check%
  set name a firecloth skirt
  set vnum 18075
elseif firecloth pauldrons /= %check%
  set name firecloth pauldrons
  set vnum 18076
elseif hearthflame skirt /= %check%
  set name a hearthflame skirt
  set vnum 18077
elseif hearthflame sleeves /= %check%
  set name hearthflame sleeves
  set vnum 18078
elseif magma-forged pauldrons /= %check%
  set name magma-forged pauldrons
  set vnum 18079
elseif core-forged plate armor /= %check%
  set name core-forged plate armor
  set vnum 18080
elseif huge spiked pauldrons /= %check%
  set name huge spiked pauldrons
  set vnum 18081
elseif fiendskin jerkin /= %check%
  set name a fiendskin jerkin
  set vnum 18082
elseif bottomless pouch /= %check%
  set name a bottomless pouch
  set vnum 18083
elseif igneous shield /= %check%
  set name an igneous shield
  set vnum 18084
elseif cold lava ring /= %check%
  set name a cold lava ring
  set vnum 18085
elseif mantle of flames /= %check%
  set name a mantle of flames
  set vnum 18086
elseif frozen flame earrings /= %check%
  set name frozen flame earrings
  set vnum 18087
elseif emberweave gloves /= %check%
  set name emberweave gloves
  set vnum 18088
elseif molten gauntlets /= %check%
  set name molten gauntlets
  set vnum 18089
elseif molten fiend earrings /= %check%
  set name the molten fiend's earrings
  set vnum 18090
elseif Flametalon /= %check%
  set name Flametalon
  set vnum 18091
elseif obsidian earring /= %check%
  set name an obsidian earring
  set vnum 18092
elseif crown of abyssal flames /= %check%
  set name a crown of abyssal flames
  set vnum 18093
elseif fiendhorn bladed bow /= %check%
  set name a fiendhorn bladed bow
  set vnum 18094
end
if !%vnum%
  %send% %actor% You can't create that from essence.
  halt
end
* We have a valid item
%send% %actor% You bring the ten motes of demonic essence together with a fiery explosion!
%echoaround% %actor% ~%actor% brings ten motes of fiery light together, causing an explosion!
* Charge essence cost and check level to scale to
set total_level 0
set essence_count 0
while %essence_count%<10
  set next_essence %actor.inventory(18097)%
  if !%next_essence%
    * We accidentally ran out of essence in the middle of taking them away... oops
    %send% %actor% Something has gone horribly wrong while creating your item! Please submit a bug report containing this message.
    eval total_level %total_level% + 250
  else
    eval total_level %total_level% + %next_essence.level%
    %purge% %next_essence%
  end
  eval essence_count %essence_count% + 1
done
* If unscaled essence was used, the level may be 0
eval avg_level %total_level% / 10
if %avg_level% < 1
  set avg_level 1
end
* Create the item
%send% %actor% You create %name%!
%echoaround% %actor% ~%actor% creates %name%!
%load% obj %vnum% %actor% inv
set made %actor.inventory(%vnum%)%
if %made%
  nop %made.flag(HARD-DROP)%
  nop %made.flag(GROUP-DROP)%
  %scale% %made% %avg_level%
else
  %send% %actor% Error setting flags and scaling.
end
~
#18086
Fiend Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(18075)%
end
~
$
