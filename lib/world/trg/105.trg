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
* Check this item was the one used
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
* Cooldown
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
* Must be standing
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  return 1
  halt
end
* We now only allow summoning of these on claimed tiles
eval room_var %self.room%
if %room_var.empire% != %actor.empire%
  %send% %actor% You can only summon skeleton guards on your own territory.
  return 1
  halt
end
* Summon and message
%load% m %target%
eval mob %room_var.people%
if (%mob% && %mob.vnum% == %target%)
  %own% %mob% %actor.empire%
  %send% %actor% You raise %self.shortdesc% high in the air, then drive it into the ground!
  %echoaround% %actor% %actor.name% raises %self.shortdesc% high in the air, then drives it into the ground!
  %echo% %mob.name% bursts from the ground!
  eval %varname% %timestamp%
  remote %varname% %actor.id%
end
~
#10506
Puppy Plant use~
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
1 c 6
use~
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
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
eval actor %self.carried_by%
eval gem 0
if %actor%
  if %actor.is_pc%
    if %random.4% != 1
      eval gem %random.4%
    end
  end
end
eval object nothing
switch %gem%
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
  eval difficulty 1
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  eval difficulty 2
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  eval difficulty 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  eval difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
eval vnum 10550
while %vnum% <= 10552
  eval mob %%instance.mob(%vnum%)%%
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
eval newroom i10552
if %newroom%
  %at% %newroom% %load% obj 10555
end
eval exitroom i10550
if %exitroom%
  %door% %exitroom% north room %newroom%
end
eval room %self.room%
eval person %room.people%
while %person%
  eval next_person %person.next_in_room%
  %teleport% %person% %newroom%
  eval person %next_person%
done
~
#10555
Frosty delayed despawn~
1 f 0
~
%adventurecomplete%
~
#10556
Frosty tokens count~
1 c 2
count~
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
eval var_name permafrost_tokens_104
%send% %actor% You count %self.shortdesc%.
eval test %%actor.varexists(%var_name%)%%
%echoaround% %actor% %actor.name% counts %self.shortdesc%.
if %test%
  eval count %%actor.%var_name%%%
  if %count% == 1
    %send% %actor% You have one token.
  else
    %send% %actor% You have %count% tokens.
  end
else
  %send% %actor% You have no tokens.
end
~
#10557
Frosty token load/purge~
1 gn 100
~
eval var_name permafrost_tokens_104
if !%actor%
  eval actor %self.carried_by%
else
  %send% %actor% You take %self.shortdesc%.
  %echoaround% %actor% %actor.name% takes %self.shortdesc%.
  return 0
end
if %actor%
  if %actor.is_npc%
    * Oops
    halt
  end
  if !%actor.inventory(10556)%
    %load% obj 10556 %actor% inv
    %send% %actor% You find a pouch to store your permafrost tokens in.
  end
  eval test %%actor.varexists(%var_name%)%%
  if %test%
    eval val %%actor.%var_name%%%
    eval %var_name% %val%+1
    remote %var_name% %actor.id%
  else
    eval %var_name% 1
    remote %var_name% %actor.id%
  end
  %purge% %self%
end
~
#10558
Frosty trash block higher template id~
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
eval room %self.room%
if !%instance.location%
  %purge% %self%
  halt
end
eval dist %%room.distance(%instance.location%)%%
if (%dist% > 2)
  mgoto %instance.location%
elseif %room.aff_flagged(*HAS-INSTANCE)%
  halt
elseif (%room.sector% == River && %dist% <= 2)
  %terraform% %room% 10550
  %echo% The river freezes over!
end
~
#10560
Terraformer exit on load~
0 n 100
~
if !%instance.location%
  %purge% %self%
  halt
end
mgoto %instance.location%
~
#10561
Frost flower fake plant~
1 c 2
plant~
eval targ %%actor.obj_target(%arg%)%%
if %targ% != %self%
  return 0
  halt
end
eval room %actor.room%
if %room.sector% != Overgrown Forest
  return 0
  halt
end
eval check %%actor.canuseroom_member(%room%)%%
if !%check%
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
eval var_name permafrost_tokens_104
* It's a token party and everyone's invited! ...No, not toking.
eval room %self.room%
eval person %room.people%
while %person%
  if %person.is_pc%
    * You get a token, and you get a token, and YOU get a token!
    %send% %person% As %self.name% dies, %self.hisher% power crystallizes into a permafrost token!
    if !%person.inventory(10556)%
      %load% obj 10556 %person% inv
      %send% %person% You find a pouch to store your permafrost tokens in.
    end
    eval test %%person.varexists(%var_name%)%%
    if %test%
      eval val %%person.%var_name%%%
      eval %var_name% %val%+1
      remote %var_name% %person.id%
    else
      eval %var_name% 1
      remote %var_name% %person.id%
    end
  end
  eval person %person.next_in_room%
done
* Was this Aquilo?
if %self.vnum% == 10550
  * Quietly stop freezing the river outside
  eval mob %instance.mob(10559)%
  if %mob%
    %purge% %mob%
  end
  * And message anyone outside
  %regionecho% %self.room% -3 You feel a sudden gust of warm wind...
end
~
#10567
Frosty shop list - ?~
0 c 0
list~
* TEMPORARY VALUES - Probably way too expensive
%send% %actor% These are the placeholder values. If you're seeing this: Oops.
* List of items
%send% %actor% %self.name% sells the following items for permafrost tokens:
%send% %actor% - polar bear charm (14 tokens, land mount)
%send% %actor% - penguin charm (16 tokens, aquatic mount)
%send% %actor% - wyvern charm (40 tokens, flying mount)
%send% %actor% - polar cub whistle (24 tokens, minipet)
%send% %actor% - yeti charm (18 tokens, unskilled non-disguise morph)
%send% %actor% - gossamer sails of the Polar Wind (26 tokens, huge cargo ship)
%send% %actor% - whipping winds of the North pattern (20 tokens, tank weapon)
%send% %actor% - frozen pykrete sword pattern (20 tokens, melee weapon)
%send% %actor% - icy star of winter pattern (20 tokens, healer weapon)
%send% %actor% - glacial staff of the Maelstrom pattern (20 tokens, caster weapon)
~
#10570
Boss loot replacer~
1 n 100
~
eval actor %self.carried_by%
if %actor%
  if %actor.mob_flagged(GROUP)%
    * Roll for mount
    eval percent_roll %random.100%
    if %percent_roll% <= 2
      * Minipet
      eval vnum 10567
    else
      eval percent_roll %percent_roll% - 2
      if %percent_roll% <= 2
        * Land mount
        eval vnum 10564
      else
        eval percent_roll %percent_roll% - 2
        if %percent_roll% <= 2
          * Sea mount
          eval vnum 10565
        else
          eval percent_roll %percent_roll% - 2
          if %percent_roll% <= 1
            * Flying mount
            eval vnum 10566
          else
            eval percent_roll %percent_roll% - 1
            if %percent_roll% <= 1
              * Morpher
              eval vnum 10568
            else
              eval percent_roll %percent_roll% - 1
              if %percent_roll% <= 1
                * Vehicle pattern item
                eval vnum 10569
              else
                * Token
                eval vnum 10557
              end
            end
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
    * eval item %%actor.inventory(%vnum%)%%
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
eval target %%actor.char_target(%arg%)%%
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
eval room %actor.room%
eval person %room.people%
while %person%
  if %person.is_pc%
    if %person.varexists(permafrost_mobs_iced)%
      eval permafrost_mobs_iced %person.permafrost_mobs_iced% + 1
    else
      eval permafrost_mobs_iced 1
    end
    remote permafrost_mobs_iced %actor.id%
    if %permafrost_mobs_iced% >= 6
      %quest% %actor% trigger 10595
    end
  end
  eval person %person.next_in_room%
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
pacify quest-start~
2 u 100
~
eval permafrost_mobs_iced 0
remote permafrost_mobs_iced %actor.id%
%load% obj 10593 %actor% inv
~
$
