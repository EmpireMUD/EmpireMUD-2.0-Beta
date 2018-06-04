#10500
Glowkra consume~
1 s 100
~
* This script is no longer used. The glowkra has a buff food affect instead.
if !(eat /= %command%)
  halt
end
%send% %actor% As you consume %self.shortdesc%, your skin starts to glow slightly.
dg_affect %actor% INFRA on 300
~
#10501
Magiterranean Terracrop~
0 in 100
~
* pick a crop -- use start of time as jan 1, 2015: 1420070400
* 2628288 seconds in a month
eval month ((%timestamp% - 1420070400) / 2628288) // 12
set vnum -1
switch (%month% + 1)
  case 1
    set vnum 10501
  break
  case 2
    set vnum 10508
  break
  case 3
    set vnum 10504
  break
  case 4
    set vnum 10505
  break
  case 5
    set vnum 10511
  break
  case 6
    set vnum 10503
  break
  case 7
    set vnum 10507
  break
  case 8
    set vnum 10509
  break
  case 9
    set vnum 10500
  break
  case 10
    set vnum 10502
  break
  case 11
    set vnum 10510
  break
  case 12
    set vnum 10506
  break
done
if (%vnum% == -1)
  halt
end
* month change detection
if %self.varexists(starting_crop_vnum)%
  if %self.starting_crop_vnum% != %vnum%
    * Uh-oh
    set month_change 1
  end
else
  set starting_crop_vnum %vnum%
  remote starting_crop_vnum %self.id%
end
set room %self.room%
if !%instance.location%
  %echo% %self.name% vanishes in a swirl of leaf-green mana!
  %purge% %self%
  halt
end
if %room.sector_vnum% <= 4 || (%room.sector_vnum% >= 40 && %room.sector_vnum% <= 45) || %room.sector_vnum% == 50 || (%room.sector_vnum% >= 54 && %room.sector_vnum% <= 56)
  * valid, plains/forest or shore-jungle (55)
  set sector_valid 1
elseif %room.sector_vnum% == 7 || %room.sector_vnum% == 13 || %room.sector_vnum% == 15 || %room.sector_vnum% == 16 || %room.sector_vnum% == 27 || %room.sector_vnum% == 28 || %room.sector_vnum% == 34
  * valid, jungle / crop
  set sector_valid 1
end
if (%room.template% == 10500 || %room.distance(%instance.location%)% > 3 || %room.building% == Fence || %room.building% == Wall)
  %echo% %self.name% vanishes in a swirl of leaf-green mana!
  mgoto %instance.location%
  %echo% %self.name% appears in a swirl of leaf-green mana!
  halt
elseif %room.empire_id% || (%room.crop_vnum% >= 10500 && %room.crop_vnum% <= 10549)
  * Claimed or already a whirlwind crop
  set no_work 1
elseif !%sector_valid%
  * No Work
  set no_work 1
end
if !%no_work%
  * do!
  if !%month_change%
    %terracrop% %room% %vnum%
    %echo% %self.name% spreads mana over the land and crops begin to grow!
  end
end
if %month_change%
  %at% %instance.location% %echo% The whirlwind collapses in on itself, leaving behind crops!
  %terracrop% %instance.location% %starting_crop_vnum%
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
set treat 10528
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
* Check this item was the one used
if !%self.is_name(%arg%)%
  return 0
  halt
end
* Cooldown
set varname summon%target%
* Change this if trigger vnum != mob vnum
set target %self.vnum%
* once per 30 minutes
if %actor.varexists(%varname%)%
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
* Must be standing
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  return 1
  halt
end
* We now only allow summoning of these on claimed tiles
set room_var %self.room%
if %room_var.empire% != %actor.empire%
  %send% %actor% You can only summon skeleton guards on your own territory.
  return 1
  halt
end
* Summon and message
%load% m %target%
set mob %room_var.people%
if (%mob% && %mob.vnum% == %target%)
  %own% %mob% %actor.empire%
  %send% %actor% You raise %self.shortdesc% high in the air, then drive it into the ground!
  %echoaround% %actor% %actor.name% raises %self.shortdesc% high in the air, then drives it into the ground!
  %echo% %mob.name% bursts from the ground!
  set %varname% %timestamp%
  remote %varname% %actor.id%
end
~
#10506
Puppy Plant use~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
* check too many mobs
set mobs 0
set found 0
set ch %self.room.people%
while %ch% && !%found%
  if (%ch.is_npc% && %ch.vnum% >= 10501 && %ch.vnum% <= 10504 && %ch.master% && %ch.master% == %actor%)
    set found 1
  elseif %ch.is_npc%
    eval mobs %mobs% + 1
  end
  set ch %ch.next_in_room%
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
  set pet %self.room.people%
  if (%pet% && %pet.vnum% == %vnum%)
    %force% %pet% mfollow %actor%
    %echo% %pet.name% appears!
  end
  %purge% %self%
end
~
#10507
Interdimensional Whirlwind Cleanup~
2 e 100
~
set main_direction_1 west
set main_direction_2 east
set branch_direction_1 north
set branch_direction_2 south
set dir_var 1
eval row_direction %%main_direction_%dir_var%%%
set row_room %room%
set loop 1
* mirror left/right
while %dir_var% <= 2
  * terraform horizontally
  while %room.distance(%row_room%)% <= 3
    * %regionecho% %room% 5 %row_room.sector% @ %row_room.coords%
    if ((%row_room.crop_vnum% >= 10500) && (%row_room.crop_vnum% <= 10549) && (!%row_room.empire_id%)) && %row_room% != %room%
      %terraform% %row_room% 0
    end
    set branch_dir_var 1
    * mirror up/down
    while %branch_dir_var% <= 2
      eval column_direction %%branch_direction_%branch_dir_var%%%
      eval target_room %%row_room.%column_direction%(map)%%
      * terraform vertically
      while %target_room% && %room.distance(%target_room%)% <= 3
        * %regionecho% %room% 5 %target_room.sector% @ %target_room.coords%
        if ((%target_room.crop_vnum% >= 10500) && (%target_room.crop_vnum% <= 10549) && (!%target_room.empire_id%))
          %terraform% %target_room% 0
        end
        eval target_room %%target_room.%column_direction%(map)%%
      done
      eval branch_dir_var %branch_dir_var% + 1
    done
    eval row_room %%row_room.%row_direction%(map)%%
  done
  eval dir_var %dir_var% + 1
  eval row_direction %%main_direction_%dir_var%%%
  eval row_room %%room.%row_direction%(map)%%
done
%regionecho% %room% -3 The nearby whirlwind implodes, sucking in the crops surrounding it.
%terraform% %room% 0
~
#10508
Dragonstooth sceptre equip first~
1 c 6
use~
if %actor.obj_target(%arg%)% != %self%
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
set actor %self.carried_by%
set gem 0
if %actor%
  if %actor.is_pc%
    if %random.4% != 1
      set gem %random.4%
    end
  end
end
set object nothing
switch %gem%
  case 1
    * iris
    set object an iridescent blue iris
    %load% o 1206 %self.carried_by%
  break
  case 2
    * bloodstone
    set object a red bloodstone
    %load% o 104 %self.carried_by%
  break
  case 3
    * seashell
    set object a glowing green seashell
    %load% o 1300 %self.carried_by%
  break
  case 4
    * lightning stone
    set object a yellow lightning stone
    %load% o 103 %self.carried_by%
  break
  default
    * failure - do nothing
  break
done
if %self.carried_by%
  %send% %self.carried_by% %self.shortdesc% in your inventory disintegrates, leaving behind %object%!
else
  %echo% %self.shortdesc% in the room disintegrates, leaving behind %object%!
end
%purge% %self%
~
#10550
Aquilo Combat~
0 k 100
~
if %self.cooldown(10560)%
  halt
end
if %self.affect(BLIND)%
  %echo% %self.name%'s eyes flash blue, and %self.hisher% vision clears!
  dg_affect %self% BLIND off 1
end
set heroic_mode %self.mob_flagged(GROUP)%
* Always uses this ability:
* Bitter Cold
* Stacking DoT on tank
* Lasts 75 seconds, should be refreshed every 45-55
* Normal/Hard: 25%, stacks up to 5
* Group/Boss: 75%, stacks up to 10
%send% %actor% &r%self.name%'s icy blasts leave a lingering cold which chills you to the bone...
%echoaround% %actor% %actor.name% shivers under %self.name%'s icy onslaught.
if %heroic_mode%
  %dot% #10551 %actor% 75 75 magical 10
else
  %dot% #10551 %actor% 25 75 magical 5
end
wait 1 s
if !%actor% || (%actor.vnum% == %self.vnum%)
  * Actor died during the delay
  set actor %self.fighting%
  if !%actor%
    halt
  end
end
switch %random.3%
  case 1
    * Perfect Freeze
    * Slow entire party for 20 seconds
    * Minor AoE damage (25%)
    * Group/Boss: Also stun entire party for 5 seconds
    %echo% %self.name% begins gathering magical energy...
    wait 3 sec
    %echo% &rThere is a blinding flash of light, and everything is encased in ice!
    %aoe% 25 magical
    set person %self.room.people%
    while %person%
      if %person.is_enemy(%self%)%
        dg_affect #10552 %person% SLOW on 20
        if %heroic_mode%
          dg_affect #10552 %person% HARD-STUNNED on 5
        else
          %echo% The ice shatters, but leaves a lingering chill...
        end
      end
      set person %person.next_in_room%
    done
  break
  case 2
    * Comet
    * Normal/Hard: 50% AoE damage, 200% extra to tank
    * Group/Boss: 100% AoE damage, 400% extra to tank, tank is stunned for 10 seconds
    say Behold the might of the Polar Baron!
    %echo% %self.name% gestures dramatically to the sky, where a huge portal opens!
    wait 3 sec
    %echo% A torrent of water pours through %self.name%'s portal, flash-freezing into a huge icy comet!
    wait 2 sec
    set actor %self.fighting%
    if !%self.fighting%
      %echo% The comet crashes to the ground, hitting nobody.
      halt
    end
    if %actor.trigger_counterspell%
      set counterspell 1
    end
    if %heroic_mode%
      if %counterspell%
        %send% %actor% &rThe comet triggers your counterspell and briefly slows, before crashing into you and exploding!
        %echoaround% %actor% The comet briefly slows before crashing into %actor.name% and exploding!
        %damage% %actor% 200 physical
        %echo% &rFragments fly in all directions!
        %aoe% 100 physical
      else
        %send% %actor% &rThe comet crashes into you, smashing you to the ground, and explodes!
        %echoaround% %actor% The comet crashes into %actor.name%, smashing %actor.himher% to the ground, and explodes!
        %damage% %actor% 400 physical
        dg_affect #10553 %actor% HARD-STUNNED on 10
        %echo% &rFragments fly in all directions!
        %aoe% 100 physical
      end
    else
      if %counterspell%
        %send% %actor% The comet crashes into your counterspell and explodes!
        %echoaround% %actor% The comet explodes against an invisible shield in front of %actor.name%!
        %echo% &rFragments fly in all directions!
        %aoe% 50 physical
      else
        %send% %actor% &rThe comet crashes into you, knocking you back, and explodes!
        %echoaround% %actor% The comet crashes into %actor.name%, knocking %actor.himher% back, and explodes!
        %damage% %actor% 200 physical
        %echo% &rFragments fly in all directions!
        %aoe% 50 physical
      end
    end
  break
  case 3
    * Enchanted Sword
    * Summon attacks tank for 30 seconds
    * Normal/Hard: Aquilo is stunned for 15 seconds
    * Group/Boss: Aquilo keeps attacking
    %send% %actor% %self.name% forms a sword out of ice and hurls it at you!
    %echoaround% %actor% %self.name% forms a sword out of ice and hurls it at %actor.name%!
    %load% mob 10560 ally %self.level%
    set summon %self.room.people%
    if %summon.vnum% == 10560
      %send% %actor% %summon.name% begins attacking you with a malevolent will of %summon.hisher% own!
      %echoaround% %actor% %summon.name% begins attacking %actor.name% with a malevolent will of %summon.hisher% own!
      %force% %summon% %aggro% %actor%
    end
    if !%heroic_mode%
      %echo% %self.name% steps back and folds %self.hisher% arms.
      dg_affect %self% HARD-STUNNED on 15
    end
  break
done
nop %self.set_cooldown(10560, 30)%
~
#10551
Permafrost Cryomancer combat~
0 k 100
~
if %self.cooldown(10560)%
  halt
end
set heroic_mode %self.mob_flagged(GROUP)%
switch %random.3%
  case 1
    * Blizzard
    * Duration: 20 seconds
    * Normal/Hard: AoE to-hit penalty [level/10]
    * Hard/Group: AoE to-hit penalty [level/5] and DoT [50%]
    %echo% %self.name% raises %self.hisher% arms to the sky and starts chanting.
    wait 3 sec
    %echo% An intense blizzard suddenly forms in the violet sky above the glacier!
    set person %self.room.people%
    eval magnitude %self.level%/5
    if !%heroic_mode%
      eval magnitude %magnitude%/2
    end
    while %person%
      if %person.is_enemy(%self%)%
        if %heroic_mode%
          %send% %person% &rYou are chilled to the bone, and can barely see!
          %dot% %person% 50 20 magical
          dg_affect #10554 %person% TO-HIT -%magnitude% 20
        else
          %send% %person% The snow makes it hard to see!
          dg_affect #10554 %person% TO-HIT -%magnitude% 20
        end
      end
      set person %person.next_in_room%
    done
  break
  case 2
    * Icy Burial
    * Blind and stun tank
    * Normal/Hard: 5 seconds
    * Group/Boss: 20 seconds, and adds 50% DoT
    * If too easy, consider adding HIDE and looking for a new target...
    %send% %actor% &r%self.name% makes an arcane gesture at you, and the snow beneath your feet swallows you!
    %echoaround% %actor% %self.name% makes an arcane gesture at %actor.name%, and the snow beneath %actor.hisher% feet swallows %actor.himher%!
    if %heroic_mode%
      %send% %actor% You attempt to dig yourself out of the deep, painfully cold snowdrift!
      %dot% %actor% 50 20 magical
      dg_affect #10555 %actor% BLIND on 20
      dg_affect #10555 %actor% HARD-STUNNED on 20
    else
      %send% %actor% You scramble to pull yourself out of the pile of snow.
      dg_affect #10555 %actor% BLIND on 5
      dg_affect #10555 %actor% HARD-STUNNED on 5
    end
  break
  case 3
    * Summon Ice Elemental
    * Normal/Hard: Summon a temporary ice sprite
    * Summon a permanent ice elemental (DPS flag)
    %echo% %self.name% puts %self.hisher% fingers to %self.hisher% mouth and releases a piercing whistle!
    if !%heroic_mode%
      set summon_vnum 10561
    else
      set summon_vnum 10562
    end
    %load% mob %summon_vnum% ally %self.level%
    set summon %self.room.people%
    if %summon.vnum% == %summon_vnum%
      %echo% %summon.name% soars down from the clear violet sky!
      %force% %summon% %aggro% %actor%
    end
  break
done
nop %self.set_cooldown(10560, 30)%
~
#10552
Permafrost Rime Mage Combat~
0 k 100
~
if %self.cooldown(10560)%
  halt
end
set heroic_mode %self.mob_flagged(GROUP)%
switch %random.3%
  case 1
    * Freezing Touch (targets tank)
    * Normal/Hard: Slow[20]+dot[75%]
    * Group/Boss: Stun[5]+slow[20]+dot[100%]
    %send% %actor% %self.name% makes an arcane gesture at you, and hoar frost suddenly encases you!
    %echoaround% %actor% %self.name% makes an arcane gesture at %actor.name%, and hoar frost suddenly encases %actor.himher%!
    dg_affect #10557 %actor% SLOW on 20
    if %heroic_mode%
      dg_affect #10556 %actor% HARD-STUNNED on 5
      %dot% #10557 %actor% 100 20 magical
    else
      %dot% #10557 %actor% 75 20 magical
    end
  break
  case 2
    * Rime Blades (targets tank/all)
    * All difficulties: 150% damage, 100% DoT
    * DoT duration: 20 seconds
    * Normal/Hard: Tank only
    * Group/Boss: Entire party
    %echo% %self.name% makes a sweeping arcane gesture, and hoar frost spreads over the cavern walls!
    wait 3 sec
    %echo% %self.name% releases a pulse of icy magic, and the hoar frost explodes off the walls!
    if %heroic_mode%
      %echo% &rEveryone is slashed by %self.name%'s rime blades!
      %aoe% 150 physical
      set person %self.room.people%
      while %person%
        if %person.is_enemy(%self%)%
          %send% %person% You receive dozens of painful, bleeding wounds!
          %dot% #10558 %person% 100 20 physical
        end
        set person %person.next_in_room%
      done
    else
      %send% %actor% &rThe rime blades slash you, opening dozens of painful, bleeding wounds!
      %echoaround% %actor% The rime blades slash %actor.name%, opening dozens of bleeding wounds!
      %damage% %actor% 150 physical
      %dot% #10558 %actor% 100 20 physical
    end
  break
  case 3
    * Violet Fire (targets tank)
    * Duration: 20 seconds
    * Applies heal-over-time to self, damage-over-time to tank
    * Normal/Hard: 50% DoT, level/20 HoT
    * Group/Boss: 100% DoT, level/10 HoT
    %echo% %self.name% reaches into the cold, violet flames, and %self.hisher% wounds start to close...
    %send% %actor% &r%self.name% lashes out at you with a whip of violet fire, which clings to you and draws your heat away!
    %echoaround% %actor% %self.name% lashes out at %actor.name% with a whip of violet fire! %actor.name% bursts into cold violet flames!
    if %heroic_mode%
      %dot% #10559 %actor% 100 20 magical
      eval magnitude %actor.level%/10
      dg_affect #10559 %self% HEAL-OVER-TIME %magnitude% 20
    else
      %dot% #10559 %actor% 50 20 magical
      eval magnitude %actor.level%/20
      dg_affect #10559 %self% HEAL-OVER-TIME %magnitude% 20
    end
  break
done
* Universal cooldown - Fight trigger can't run again until this amount of time has passed.
nop %self.set_cooldown(10560, 30)%
~
#10553
Permafrost trash combat~
0 k 10
~
switch %random.3%
  case 1
    * Ice shard
    %send% %actor% %self.name% spits a bolt of icy magic at you, chilling you to the bone!
    %echoaround% %actor% %self.name% spits a bolt of icy magic at %actor.name%, who starts shivering violently.
    %damage% %actor% 25 magical
    %dot% %actor% 50 30 magical 1
    * 7 ~ 13
    eval amnt (%self.level%/10)
    if %amnt% > 0
      dg_affect %actor% DODGE -%amnt% 30
    end
  break
  case 2
    * Snow flurry
    %echo% %self.name% kicks up a flurry of snow!
    set person %self.room.people%
    while %person%
      if %self.is_enemy(%person%)%
        %send% %person% The snow gets in your eyes!
        * 7 ~ 13
        eval amnt (%self.level%/10)
        if %amnt% > 0
          dg_affect %person% TO-HIT -%amnt% 15
        end
      end
      set person %person.next_in_room%
    done
  break
  case 3
    * Mob-specific
    if %self.vnum% == 10553
      %send% %actor% %self.name% puts its head down and charges you!
      %echoaround% %actor% %self.name% puts its head down and charges %actor.name%!
      wait 2 sec
      if %self.aff_flagged(ENTANGLED)% || %self.aff_flagged(STUNNED)%
        %echo% %self.name%'s attack is interrupted.
        halt
      end
      %send% %actor% %self.name% crashes into you, leaving you briefly stunned!
      %echoaround% %actor% %self.name% crashes into %actor.name%, stunning %actor.himher%!
      if !%actor.aff_flagged(!STUN)%
        dg_affect %actor% STUNNED on 5
      end
      %damage% %actor% 50
    elseif %self.vnum% == 10554
      %echo% %self.name% glows gently, and fights with renewed vitality!
      %damage% %self% -150
    elseif %self.vnum% == 10555
      %send% %actor% %self.name% nips viciously at your ankles, drawing blood and slowing you down!
      %echoaround% %actor% %self.name% nips viciously at %actor.name%'s ankles, drawing blood and slowing %actor.himher% down!
      %dot% %actor% 50 15 physical
      dg_affect %actor% DEXTERITY -1 15
    end
  break
done
~
#10554
Frosty difficulty select~
1 c 4
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty.
  return 1
  halt
end
* TODO: Check nobody's in the adventure before changing difficulty
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set difficulty 1
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set difficulty 2
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  set difficulty 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  set difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
set vnum 10550
while %vnum% <= 10552
  set mob %instance.mob(%vnum%)%
  if !%mob%
    * This was for debugging. We could do something about this.
    * Maybe just ignore it and keep on setting?
  else
    nop %mob.remove_mob_flag(HARD)%
    nop %mob.remove_mob_flag(GROUP)%
    if %difficulty% == 1
      * Then we don't need to do anything
    elseif %difficulty% == 2
      nop %mob.add_mob_flag(HARD)%
    elseif %difficulty% == 3
      nop %mob.add_mob_flag(GROUP)%
    elseif %difficulty% == 4
      nop %mob.add_mob_flag(HARD)%
      nop %mob.add_mob_flag(GROUP)%
    end
  end
  eval vnum %vnum% + 1
done
%echo% The glacier cracks and splits, revealing two paths.
set newroom i10552
if %newroom%
  %at% %newroom% %load% obj 10555
end
set exitroom i10550
if %exitroom%
  %door% %exitroom% north room %newroom%
end
set person %self.room.people%
while %person%
  set next_person %person.next_in_room%
  %teleport% %person% %newroom%
  set person %next_person%
done
~
#10555
Frosty delayed despawn~
1 f 0
~
%adventurecomplete%
~
#10556
PF Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10550)%
end
~
#10558
Frosty trash block higher template id~
0 s 100
~
* One quick trick to get the target room
set room_var %self.room%
eval to_room %%room_var.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%to_room% || %to_room.template% < %room_var.template%)
  halt
end
if %actor.aff_flagged(SNEAK)%
  * Can sneak past
  halt
end
%send% %actor% Frozen winds block your progress.
return 0
~
#10559
River freezer~
0 i 100
~
set room %self.room%
if !%instance.location%
  %purge% %self%
  halt
end
if (%room.distance(%instance.location%)% > 2)
  mgoto %instance.location%
elseif %room.aff_flagged(*HAS-INSTANCE)%
  halt
elseif (%room.sector% == River)
  %terraform% %room% 10550
  %echo% The river freezes over!
elseif (%room.sector% == Estuary)
  %terraform% %room% 10551
  %echo% The estuary freezes over!
elseif (%room.sector% == Canal)
  %terraform% %room% 10552
  %echo% The canal freezes over!
elseif (%room.sector% == Lake)
  %terraform% %room% 10553
  %echo% The lake freezes over!
end
~
#10560
Permafrost mob teleport out on load~
0 n 100
~
if !%instance.location%
  %purge% %self%
  halt
end
mgoto %instance.location%
mmove
mmove
mmove
~
#10561
Frost flower fake plant~
1 c 2
plant~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %actor.room%
if %room.sector% != Overgrown Forest
  return 0
  halt
end
if !%actor.canuseroom_member(%room%)%
  %send% %actor% You don't have permission to use %self.shortdesc% here.
  return 1
  halt
end
%send% %actor% You plant %self.shortdesc%...
%echoaround% %actor% %actor.name% plants %self.shortdesc%...
%echo% The forest around you shifts slowly into an evergreen forest!
%terraform% %room% 10565
return 1
%purge% %self%
~
#10562
Permafrost boss death~
0 f 100
~
* It's a token party and everyone's invited! ...No, not toking.
set person %self.room.people%
while %person%
  if %person.is_pc%
    * You get a token, and you get a token, and YOU get a token!
    %send% %person% As %self.name% dies, %self.hisher% power crystallizes, creating a permafrost token!
    %send% %person% You take the newly created token.
    nop %person.give_currency(10550, 1)%
  end
  set person %person.next_in_room%
done
* Was this Aquilo?
if %self.vnum% == 10550
  * Quietly stop freezing the river outside
  set mob %instance.mob(10559)%
  if %mob%
    %purge% %mob%
  end
  * And message anyone outside
  %regionecho% %self.room% -3 You feel a sudden gust of warm wind...
end
~
#10563
Permafrost weather~
2 c 0
weather look~
if %cmd% /= look
  if out == %arg%
    %force% %actor% look snow
    return 1
    halt
  elseif up /= %arg%
    * Escape to the "weather" section
  else
    return 0
    halt
  end
end
%force% %actor% look sky
return 1
~
#10564
Permafrost boss minion timer~
0 bnw 100
~
if %self.vnum% == 10560
  * attached mob is Aquilo's summon (30 seconds)
  wait 30 s
  %echo% %self.name% falls to the ground and shatters.
  %purge% %self%
elseif %self.vnum% == 10561
  * Attached mob is Cryomancer's summon on normal/hard (15 seconds)
  wait 15 s
  %echo% %self.name% flies away, bored.
  %purge% %self%
elseif %self.vnum% == 10562
  * Attached mob is Cryomancer's summon on group/boss (permanent until end of combat)
  * make sure we're not despawning right away
  wait 5 sec
  if !%self.fighting%
    %echo% %self.name% flies away.
    %purge% %self%
  end
end
~
#10566
Polar Wind ship setup~
5 n 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 10562
end
if (!%inter.fore%)
  %door% %inter% fore add 10561
end
detach 10566 %self.id%
~
#10569
Permafrost store appear-check~
0 hw 100
~
set cheapest_item_cost 14
if !%actor.is_pc%
  halt
end
if %actor.currency(10550)% < %cheapest_item_cost%
  halt
end
nop %self.remove_mob_flag(SILENT)%
if %self.aff_flagged(HIDE)%
  visible
  wait 1
  %echo% %self.name% shuffles out of the portal and sets up a shop on the ice.
end
~
#10570
Boss loot replacer~
1 n 100
~
set actor %self.carried_by%
if %actor%
  if %actor.mob_flagged(GROUP)%
    * Roll for mount
    set percent_roll %random.100%
    if %percent_roll% <= 2
      * Minipet
      set vnum 10567
    else
      eval percent_roll %percent_roll% - 2
      if %percent_roll% <= 2
        * Land mount
        set vnum 10564
      else
        eval percent_roll %percent_roll% - 2
        if %percent_roll% <= 2
          * Sea mount
          set vnum 10565
        else
          eval percent_roll %percent_roll% - 2
          if %percent_roll% <= 1
            * Flying mount
            set vnum 10566
          else
            eval percent_roll %percent_roll% - 1
            if %percent_roll% <= 1
              * Morpher
              set vnum 10568
            else
              eval percent_roll %percent_roll% - 1
              if %percent_roll% <= 1
                * Vehicle pattern item
                set vnum 10569
              else
                * Token
                set vnum 10557
              end
            end
          end
        end
      end
    end
    if %self.level%
      set level %self.level%
    else
      set level 100
    end
    %load% obj %vnum% %actor% inv %level%
    set item %actor.inventory(%vnum%)%
    if %item.is_flagged(BOP)%
      nop %item.bind(%self%)%
    end
    * %send% %actor% %self.shortdesc% turns out to be %item.shortdesc%!
  end
end
%purge% %self%
~
#10593
Wand of Polar Power activation~
1 c 2
freeze~
if %actor.varexists(permafrost_mobs_iced)%
  if %actor.permafrost_mobs_iced% >= 6
    * Quest is completed but we currently allow you to keep zapping if you like
  end
end
if !%arg%
  %send% %actor% What do you want to blast with %self.shortdesc%?
  return 1
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% They must have fled from your awesome power, because they're not here.
  return 1
  halt
end
if %target.is_npc% && %target.vnum% >= 10553 && %target.vnum% <= 10555
  %send% %actor% You point %self.shortdesc% at %target.name% and unleash a blast of icy magic!
  %echoaround% %actor% %actor.name% points %self.shortdesc% at %target.name% and unleashes a blast of icy magic!
  %echo% %target.name% is encased in a huge block of ice!
  %purge% %target%
  %load% obj 10595 %actor.room%
elseif %target.is_npc% && %target.vnum% >= 10550 && %target.vnum% <= 10552
  %send% %actor% You point %self.shortdesc% at %target.name% and unleash a blast of icy magic!
  %echoaround% %actor% %actor.name% points %self.shortdesc% at %target.name% and unleashes a blast of icy magic!
  %echo% %target.name% absorbs the icy blast harmlessly.
  return 1
  halt
else
  %send% %actor% %self.shortdesc% only works on enemies from the Permafrost area of the Magiterranean.
  return 1
  halt
end
set person %actor.room.people%
while %person%
  if %person.is_pc%
    if %person.on_quest(10550)%
      if %person.varexists(permafrost_mobs_iced)%
        eval permafrost_mobs_iced %person.permafrost_mobs_iced% + 1
      else
        set permafrost_mobs_iced 1
      end
      remote permafrost_mobs_iced %person.id%
      if %permafrost_mobs_iced% >= 6
        %quest% %person% trigger 10550
        %send% %person% You have frozen enough enemies to complete your quest.
      end
    end
  end
  set person %person.next_in_room%
done
~
#10594
Permafrost trash spawner~
1 n 100
~
switch %random.3%
  case 1
    %load% mob 10553
  break
  case 2
    %load% mob 10554
  break
  case 3
    %load% mob 10555
  break
done
%purge% %self%
~
#10595
Pacify the Permafrost quest start~
2 u 100
~
set permafrost_mobs_iced 0
remote permafrost_mobs_iced %actor.id%
%load% obj 10593 %actor% inv
~
#10598
Gem ice melt~
1 f 0
~
set actor %self.carried_by%
set object nothing
switch %random.4%
  case 1
    * iris
    set object an iridescent blue iris
    %load% o 1206 %actor%
  break
  case 2
    * bloodstone
    set object a red bloodstone
    %load% o 104 %actor%
  break
  case 3
    * seashell
    set object a glowing green seashell
    %load% o 1300 %actor%
  break
  case 4
    * lightning stone
    set object a yellow lightning stone
    %load% o 103 %actor%
  break
  default
    * failure - do nothing
  break
done
if %actor%
  %send% %actor% %self.shortdesc% in your inventory melts, turning into %object%!
else
  %echo% %self.shortdesc% in the room melts, turning into %object%!
end
%purge% %self%
~
#10599
Frostblood Consecration Ritual~
2 c 0
consecrate~
if (%actor.position% != Standing)
  return 0
  halt
end
if %actor.has_item(10599)%
  %send% %actor% You already have the frostblood you need.
  return 1
  halt
end
if !(%actor.on_quest(10555)%
  %send% %actor% You don't need to do any consecrating right now.
  return 1
  halt
end
if %instance.mob(10550)%
  %send% %actor% You cannot perform the consecration while Aquilo still lives.
  return 1
  halt
end
if %actor.blood()% <= 50
  %send% %actor% Losing enough blood to complete the ritual would kill you.
  return 1
  halt
end
set room %actor.room%
set cycles_left 5
while %cycles_left% >= 0
  if (%actor.room% != %room%) || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the action
    if %cycles_left% < 5
      %echoaround% %actor% %actor.name%'s ritual is interrupted.
      %send% %actor% Your ritual is interrupted.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% %actor.name% draws blood from %actor.hisher% wrist...
      %send% %actor% You begin the consecration by drawing blood from your wrist...
      nop %actor.blood(-50)%
    break
    case 4
      %echoaround% %actor% %actor.name% collects Aquilo's blood from the ground...
      %send% %actor% You collect Aquilo's blood from the ground...
    break
    case 3
      %echoaround% %actor% %actor.name% pours %actor.hisher% blood and Aquilo's blood onto the altar...
      %send% %actor% You pour your blood and Aquilo's blood onto the altar...
    break
    case 2
      %echoaround% %actor% %actor.name% watches as the blood slowly mixes, and starts to freeze...
      %send% %actor% You watch as the blood slowly mixes, and starts to freeze...
    break
    case 1
      %echoaround% %actor% %actor.name% starts to collect the consecrated frostblood...
      %send% %actor% You start to collect the consecrated frostblood...
    break
    case 0
      %echoaround% %actor% %actor.name% completes %actor.hisher% ritual!
      %send% %actor% You complete your consecration ritual!
      * Quest complete
      %load% obj 10599 %actor% inv
      * %quest% %actor% trigger 10555
      * next quest stage message?
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
~
$
