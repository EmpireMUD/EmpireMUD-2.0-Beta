#11800
Tower Skycleave announcement~
2 ab 1
~
if %random.3% == 3
  %regionecho% %room% -100 You can see the Tower Skycleave in the distance. %room.coords%
end
~
#11801
Skycleave: Floor 2A mobs: complex leave rules~
0 s 100
~
* configs first
set sneakable_vnums 11815 11816 11817
set maze_vnums 11812 11813 11818 11819 11820 11821 11823 11824 11825 11826
set pixy_vnums 11820
* One quick trick to get the target room
set room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
if (%actor.nohassle% || !%tricky% || %self.disabled% || %self.position% != Standing)
  halt
end
* pixies allow the player to pass if they have given a gift to the queen
if %pixy_vnums% ~= %self.vnum% && %actor.varexists(skycleave_queen)%
  if %actor.skycleave_queen% == %dailycycle%
    halt
  end
end
* check difficulty if sneakable (group/boss require players to stay behind)
if %actor.aff_flagged(SNEAK)% && %sneakable_vnums% ~= %self.vnum%
  set spirit %instance.mob(11900)%
  if %spirit.diff2% < 3
    * sneak ok
    halt
  else
    * hard/group: check for another player staying here
    set ch %room_var.people%
    while %ch%
      if %ch.is_pc% && !%ch.aff_flagged(SNEAK)%
        * at least 1 player is not sneaking: allow the sneak
        halt
      end
      set ch %ch.next_in_room%
    done
  end
end
* check direction
if %maze_vnums% ~= %room_var.template%
  * in pixy maze (allow only south)
  if %direction% == south
    halt
  end
else
  * not in pixy maze: block higher template id only
  if (%tricky.template% < %room_var.template%)
    halt
  end
end
* failed
if %actor.aff_flagged(SNEAK)%
  %send% %actor% You can't seem to sneak past ~%self%!
else
  %send% %actor% You can't get past ~%self%!
end
return 0
~
#11802
Skycleave mob blocking: Sneakable~
0 q 100
~
* One quick trick to get the target room
set room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.aff_flagged(SNEAK)% || %actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
%send% %actor% You can't seem to get past ~%self%!
return 0
~
#11803
Skycleave mob blocking: Aggro~
0 s 100
~
* One quick trick to get the target room
set room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
%send% %actor% You can't seem to get past ~%self%, and &%self% doesn't look happy...
%aggro% %actor%
return 0
~
#11804
Everflowing flagon (free refills)~
1 bw 10
~
* drinkcon values: 0=capacity, 1=contents, 2=generic vnum
if %self.val2% == 0
  * convert plain water to enchanted water
  nop %self.val2(11804)%
end
* slowly refill water
if %self.val2% == 11804 && %self.val1% < %self.val0%
  eval amt %self.val1% + 12
  if %amt% > %self.val0%
    set amt %self.val0%
  end
  nop %self.val1(%amt%)%
end
~
#11805
Looker: force look on load~
1 n 100
~
wait 1
if %self.carried_by% && %self.carried_by.position% != Sleeping
  %force% %self.carried_by% look
end
%purge% %self%
~
#11806
Skycleave: One-time one-line greetings~
0 g 100
~
if %actor.is_npc% || %self.fighting%
  halt
end
set room %self.room%
* let everyone arrive
wait 0
* check for someone who needs the greeting
set any 0
set ch %room.people%
while %ch%
  if %ch.is_pc%
    set varname greet_%ch.id%
    if !%self.varexists(%varname%)%
      set any 1
      set %varname% 1
      remote %varname% %self.id%
    end
  end
  set ch %ch.next_in_room%
done
* did anyone need the intro
if !%any%
  halt
end
switch %self.vnum%
  case 11807
    * new shopkeep (1A)
    say We're not, uh, open yet. It's a crisis, if you haven't noticed.
  break
  case 11907
    * shopkeep (1B)
    say Hello, erm, and welcome to Towerwin, erm, Towerinkel's Gift Shop, your one shop stop for, erm, your one stop shop for all your, erm, gifts. Oh, that's not right.
  break
  case 11812
    * Apprentice Cosimo (2A)
    say Keep quiet and they won't find us in here.
  break
  case 11836
    * Lich Scaldorran (3A)
    %echo% ~%self% comes screaming toward you, lashing at you with snake-like wrappings, before realizing you aren't ^%self% enemy.
  break
  case 11840
    * Magineer Waltur (3A)
    say You fool, we were hiding in here. Next time leave the wall shut.
  break
  case 11940
    * Magineer Waltur (3B)
    say Now that all that nastiness is done, I have some things that might interest you. (list)
  break
  case 11941
    * Goef the Oreonic (3B)
    say Salutations, little human. Are you here for attunement? (attune)
  break
  case 11862
    * Apprentice Thorley (4A)
    say Oh, thank the Wyrd, you're not with that you-know-what of an enchantress.
  break
done
~
#11807
Skycleave: Bribe goblins to leave~
0 j 100
~
set goblin_vnums 11815 11816 11817
if %self.vnum% == 11818
  say Venjer cannot be bribed!
  %aggro% %actor%
  return 0
elseif %object.vnum% != 11976 && (!(%object.keywords% ~= goblin) || (%object.keywords% ~= wrangler))
  say Goblin doesn't want %object.shortdesc%, stupid human!
  return 0
else
  * actor has given a goblin object
  %send% %actor% You give ~%self% @%object%...
  %echoaround% %actor% ~%actor% gives ~%self% @%object%...
  if %object.vnum% == 11976
    say This is what we came for! These is not belonging to you! But thanks, you, for returning.
  else
    say Not what goblin items we came for but still a good prize...
  end
  * despawn other goblins here
  set ch %self.room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch.is_npc% && %goblin_vnums% ~= %ch.vnum%
      %echo% ~%ch% leaves.
      if %ch% != %self%
        %purge% %ch%
      end
    end
    set ch %next_ch%
  done
  * and leave
  return 0
  %purge% %object%
  %purge% %self%
end
~
#11808
Skycleave: Mob gains no-attack on load~
0 n 100
~
* turn on no-attack (until diff-sel)
dg_affect %self% !ATTACK on -1
~
#11809
Skycleave: Single mob difficulty selector~
0 c 0
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty. (Normal, Hard, Group, or Boss)
  return 1
  halt
end
if %self.fighting%
  %send% %actor% You can't change |%self% difficulty while &%self% is in combat!
  return 1
  halt
end
if normal /= %arg%
  set difficulty 1
elseif hard /= %arg%
  set difficulty 2
elseif group /= %arg%
  set difficulty 3
elseif boss /= %arg%
  set difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this encounter. (Normal, Hard, Group, or Boss)
  return 1
  halt
end
* messaging
%send% %actor% You set the difficulty...
%echoaround% %actor% ~%actor% sets the difficulty...
* Clear existing difficulty flags and set new ones.
set mob %self%
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %difficulty% == 1
  * Then we don't need to do anything
elseif %difficulty% == 2
  %echo% ~%self% has been set to Hard.
  nop %mob.add_mob_flag(HARD)%
elseif %difficulty% == 3
  %echo% ~%self% has been set to Group.
  nop %mob.add_mob_flag(GROUP)%
elseif %difficulty% == 4
  %echo% ~%self% has been set to Boss.
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
%restore% %mob%
* remove no-attack
if %mob.aff_flagged(!ATTACK)%
  dg_affect %mob% !ATTACK off
end
* if I was scaled before, need to rescale me
if %self.is_scaled%
  %scale% %self% %self.level%
end
* mark me as scaled
set scaled 1
remote scaled %self.id%
* attempt to remove (difficulty) from longdesc
set test (difficulty)
if %self.longdesc% ~= %test%
  set string %self.longdesc%
  set replace %string.car%
  set string %string.cdr%
  while %string%
    set word %string.car%
    set string %string.cdr%
    if %word% != %test%
      set replace %replace% %word%
    end
  done
  if %replace%
    %mod% %self% longdesc %replace%
  end
end
~
#11810
Skycleave: Message when no-attack mob is attacked~
0 B 0
~
if %self.aff_flagged(!ATTACK)%
  if %self.has_trigger(11809)%
    %send% %actor% You need to choose a difficulty before you can fight ~%self%.
    %send% %actor% Usage: difficulty <normal \| hard \| group \| boss>
    %echoaround% %actor% ~%actor% considers attacking ~%self%.
  else
    * no diff-sel: base it on vnum
    switch %self.vnum%
      case 11849
        * Trixton Vye
        set bleak %instance.mob(11848)%
        set kara
        if %instance.mob(11847)%
          %send% %actor% When you approach Trixton Vye, you're pushed back by a cold force... Something -- or someone -- is protecting him.
        elseif %instance.mob(11848)%
          %send% %actor% Trixton Vye ignores all attempts to harm him as he wounds instantly seal themselves. The magic comes from elsewhere; something -- or someone -- is protecting him.
        end
      break
      default
        %send% %actor% This is a peaceful area.
      break
    done
  end
  return 0
else
  * no need for this script anymore
  detach 11810 %self.id%
  return 1
end
~
#11811
Skycleave: Directory look description~
1 c 4
look examine~
return 0
if %actor.obj_target(%arg%)% != %self%
  halt
end
set spirit %instance.mob(11900)%
* detect location
set template %self.room.template%
if %template% == 11801 || %template% == 11901
  set floor Lobby
elseif %template% == 11810 || %template% == 11910
  set floor Second Floor
elseif %template% == 11830 || %template% == 11930
  set floor Third Floor
elseif %template% == 11860 || %template% == 11960
  set floor Fourth Floor
end
* base text
%mod% %self% lookdesc The directory is a standing slab of rough-cut stone with one smooth face engraved with information for each floor of the Tower Skycleave.
%mod% %self% append-lookdesc-noformat &0
%mod% %self% append-lookdesc-noformat == The Tower Skycleave: %floor% ==
%mod% %self% append-lookdesc-noformat &0
* floor 1
if %spirit.phase1%
  %mod% %self% append-lookdesc-noformat - First Floor: Welcome Center
  %mod% %self% append-lookdesc-noformat &0   - Cafe Fendreciel
  %mod% %self% append-lookdesc-noformat &0   - Study Hall
  %mod% %self% append-lookdesc-noformat &0   - Towerwinkel's Gift Shoppe
else
  %mod% %self% append-lookdesc-noformat - First Floor: Refuge Area
  %mod% %self% append-lookdesc-noformat &0   - Mess Hall
  %mod% %self% append-lookdesc-noformat &0   - Bunkroom
  %mod% %self% append-lookdesc-noformat &0   - Supply Room
end
* floor 2
if %spirit.phase2%
  %mod% %self% append-lookdesc-noformat - Second Floor: Apprentice Level
  %mod% %self% append-lookdesc-noformat &0   - Goblin Cages
  %mod% %self% append-lookdesc-noformat &0   - Pixy Races
  %mod% %self% append-lookdesc-noformat &0   - Mage Quarters
else
  %mod% %self% append-lookdesc-noformat - Second Floor: Danger!
  %mod% %self% append-lookdesc-noformat &0   - Goblins Everywhere
  %mod% %self% append-lookdesc-noformat &0   - Pixy Maze
end
* floor 3
if %spirit.phase3%
  %mod% %self% append-lookdesc-noformat - Third Floor: Residents' Labs
  %mod% %self% append-lookdesc-noformat &0   - Enchanting and Attunement Labs
  if %spirit.lab_open%
    %mod% %self% append-lookdesc-noformat &0   - Magichanical Labs
  end
  %mod% %self% append-lookdesc-noformat &0   - Eruditorium
  %mod% %self% append-lookdesc-noformat &0   - Lich Labs
else
  %mod% %self% append-lookdesc-noformat - Third Floor: Danger!
  %mod% %self% append-lookdesc-noformat &0   - Mercenaries Detected
  if %instance.mob(11829)%
    %mod% %self% append-lookdesc-noformat &0   - Loose Otherworlder Detected
  end
  if %spirit.lich_released% > 0
    %mod% %self% append-lookdesc-noformat &0   - Loose Lich Detected
  end
end
* floor 4
if %spirit.phase4%
  if %instance.mob(11968)%
    %mod% %self% append-lookdesc-noformat - Fourth Floor: High Sorcerers of Skycleave
    %mod% %self% append-lookdesc-noformat &0   - Office of High Sorcerer Celiya
    %mod% %self% append-lookdesc-noformat &0   - Office of High Sorcerer Barrosh
    %mod% %self% append-lookdesc-noformat &0   - Office of Grand High Sorcerer Knezz
  else
    %mod% %self% append-lookdesc-noformat &0   - Empty Office
    %mod% %self% append-lookdesc-noformat &0   - Office of High Sorcerer Barrosh
    %mod% %self% append-lookdesc-noformat &0   - Office of Grand High Sorcerer Celiya
  end
else
  %mod% %self% append-lookdesc-noformat - Fourth Floor: Danger!
    %mod% %self% append-lookdesc-noformat &0   - Office of High Sorcerer Celiya
  if %instance.mob(11867)% || !%spirit.diff4%
    %mod% %self% append-lookdesc-noformat &0   - Mind-Controlled High Sorcerer - WARNING
  elseif %instance.mob(11861)%
    %mod% %self% append-lookdesc-noformat &0   - Office of High Sorcerer Barrosh
  end
  if %instance.mob(11863)%
    %mod% %self% append-lookdesc-noformat &0   - Office of the Shadow Ascendant - WARNING
  elseif %instance.mob(11869)% || !%spirit.diff4%
    %mod% %self% append-lookdesc-noformat &0   - Office of Grand High Sorcerer Knezz - WARNING
  elseif %instance.mob(11870)%
    %mod% %self% append-lookdesc-noformat &0   - Office of Grand High Sorcerer Knezz
  else
    %mod% %self% append-lookdesc-noformat &0   - Empty Office
  end
end
* footer
%mod% %self% append-lookdesc-noformat &0
if %spirit.phase1%
  %mod% %self% append-lookdesc-noformat Have a pleasant visit to the Tower Skycleave.
else
  %mod% %self% append-lookdesc-noformat Have a safe and uneventful visit to the Tower Skycleave.
end
~
#11812
Skycleave 2.0 summoned monster despawn~
0 ab 100
~
if !%self.fighting%
  %purge% %self% $n leaves.
end
~
#11813
Skycleave Bosses: Summon Minions~
0 k 20
~
if %self.cooldown(11800)%
  halt
end
set difficulty 1
if %self.mob_flagged(HARD)%
  eval difficulty %difficulty% + 1
end
if %self.mob_flagged(GROUP)%
  eval difficulty %difficulty% + 2
end
if %difficulty% < 3
  return 0
  halt
end
nop %self.set_cooldown(11800, 30)%
switch %self.vnum%
  case 11819
    * Pixy queen
    set summon_vnum 11820
    %echo% ~%self% makes an eerie whistling sound...
  break
  case 11818
    * Goblin boss
    eval summon_vnum 11814+%random.3%
    %echo% ~%self% waves ^%self% staff in the air and shouts incoherently...
  break
  default
    * Mercenary bosses
    eval summon_vnum 11840 + %random.6%
    %echo% ~%self% whistles loudly...
  break
done
if !%summon_vnum%
  halt
end
%load% mob %summon_vnum% ally %self.level%
set summon %self.room.people%
if %summon.vnum% == %summon_vnum%
  attach 11812 %summon.id%
  remote difficulty %summon.id%
  nop %summon.add_mob_flag(!LOOT)%
  nop %summon.add_mob_flag(NO-CORPSE)%
  %send% %actor% ~%summon% rushes into the room and attacks you!
  %echoaround% %actor% ~%summon% rushes into the room and attacks ~%actor%!
  %force% %summon% mkill %actor%
else
  %echo% Something strange happened. Please report this bug.
end
~
#11814
Skycleave: Attach one-time intro on enter~
0 h 100
~
* config: these arrays match up mob vnum to trigger vnum 1:1
set mobs_list 11810 11818 11830 11847 11849 11869 11863 11890 11891 11892
set trig_list 11825 11825 11860 11825 11825 11860 11860 11825 11825 11860
if %actor.is_npc% || %self.fighting%
  halt
end
set room %self.room%
* figure out which intro I give
set trig_vnum 0
while %mobs_list% && !%trig_vnum%
  set mob %mobs_list.car%
  set mobs_list %mobs_list.cdr%
  set trig %trig_list.car%
  set trig_list %trig_list.cdr%
  if %mob% == %self.vnum%
    set trig_vnum %trig%
  end
done
* let everyone arrive
wait 0
if %trig_vnum% == 0 || %self.has_trigger(%trig_vnum%)%
  halt
end
* check for someone who needs the intro
set any 0
set ch %room.people%
while %ch%
  if %ch.is_pc%
    set varname intro_%ch.id%
    if !%room.varexists(%varname%)%
      set any 1
      set %varname% 1
      remote %varname% %room.id%
    end
  end
  set ch %ch.next_in_room%
done
* did anyone need the intro
if %any%
  attach %trig_vnum% %self.id%
  set line 0
  remote line %self.id%
  start_messages
end
~
#11815
Escaped Goblin combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Throw Object
  nop %self.set_cooldown(11800, 20)%
  set target %random.enemy%
  if !%target%
    set target %actor%
  end
  set object_1 a mana deconfabulator
  set object_2 an ether condenser
  set object_3 an enchanted knickknack
  set object_4 a dynamic reconfabulator
  eval chosen_object %%object_%random.4%%%
  %send% %target% ~%self% grabs %chosen_object% off the floor and throws it at you!
  %echoaround% %target% ~%self% grabs %chosen_object% off the floor and throws it at ~%target%!
  switch %random.4%
    case 1
      %send% %target% &&r%chosen_object% bonks you in the head!
      %echoaround% %target% %chosen_object% bonks ~%target% in the head!
      %damage% %target% 100 physical
      dg_affect #11851 %target% HARD-STUNNED on 5
    break
    case 2
      %send% %target% &&r%chosen_object% explodes in your face, blinding you!
      %echoaround% %target% %chosen_object% explodes in |%target% face, blinding *%target%!
      %damage% %target% 100 magical
      dg_affect #11841 %target% BLIND on 10
    break
    case 3
      if %target.trigger_counterspell%
        %send% %target% Your counterspell prevents %chosen_object% from draining your energy.
        %send% %target% %chosen_object% bounces harmlessly off you.
        %echoaround% %target% %chosen_object% bounces harmlessly off ~%target%.
        halt
      end
      %send% %target% &&rYou feel %chosen_object% drain your energy as it bounces off you!
      %echoaround% %target% ~%target% looks tired as %chosen_object% bounces off *%target%.
      %damage% %target% 50
      set magnitude %self.level%/10
      nop %target.mana(-%magnitude%)%
      nop %target.move(-%magnitude%)%
    break
    case 4
      %send% %target% %chosen_object% flies harmlessly past your head.
      %echoaround% %target% %chosen_object% flies harmlessly past |%target% head.
    break
  done
else
  * Pocket Sand
  nop %self.set_cooldown(11800, 20)%
  if %actor.aff_flagged(BLIND)%
    set target %random.enemy%
    if %target% == %actor%
      * Try one more time
      set target %random.enemy%
    end
    if !%target% || %target% == %actor%
      nop %self.set_cooldown(11800, 0)%
      halt
    end
  else
    set target %actor%
  end
  if %target%
    %send% %target% ~%self% tosses a handful of sand into your eyes!
    %echoaround% %target% ~%self% tosses a handful of sand into |%target% eyes!
    dg_affect #11841 %target% BLIND on 20
    dg_affect #11841 %target% TO-HIT -10 20
  end
end
~
#11816
Goblin Commando combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Shadow Strike
  nop %self.set_cooldown(11800, 20)%
  set target %random.enemy%
  if !%target%
    set target %actor%
  end
  %echo% ~%self% steps into the shadows and vanishes...
  %send% %target% &&r~%self% suddenly appears behind you and stabs you in the back!
  %echoaround% %target% &&r~%self% suddenly appears behind ~%target% and stabs *%target% in the back!
  %damage% %target% 150 physical
  if %self.difficulty% == 4
    %send% %target% |%self% unexpected attack leaves you off-balance!
    %echoaround% %target% |%self% unexpected attack leaves ~%target% off-balance!
    dg_affect #11818 %target% TO-HIT -15 10
    dg_affect #11818 %target% DODGE -15 10
  end
else
  * Ankle Stab
  nop %self.set_cooldown(11800, 20)%
  set bleed 50
  if %self.difficulty% == 4
    set bleed 100
  end
  %send% %actor% &&r~%self% stabs you in the ankle with ^%self% shortsword!
  %echoaround% %actor% ~%self% stabs ~%actor% in the ankle with ^%self% shortsword!
  %damage% %actor% 75
  %dot% #11816 %actor% %bleed% 20 physical
  if %self.difficulty% == 4
    %send% %actor% Your leg wound slows your movement!
    %echoaround% %actor% |%actor% leg wound slows ^%actor% movement.
    dg_affect #11816 %actor% SLOW on 20
  end
end
~
#11817
Goblin Bruiser combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Flail Wildly
  say slay the griffin!
  nop %self.set_cooldown(11800, 20)%
  set loops 1
  if %self.difficulty% == 4
    set loops 3
  end
  while %loops%
    %echo% &&r~%self% flails wildly, striking everyone around *%self%!
    %aoe% 50 physical
    if %loops%
      wait 3 sec
    end
    eval loops %loops% - 1
  done
else
  * Leaping Strike
  nop %self.set_cooldown(11800, 20)%
  %send% %actor% &&r~%self% leaps into the air and strikes you with a powerful flying smash!
  %echoaround% %actor% ~%self% leaps into the air and strikes ~%actor% with a powerful flying smash!
  if %self.difficulty% == 4
    %damage% %actor% 200 physical
  else
    %damage% %actor% 100 physical
  end
  dg_affect #11851 %actor% STUNNED on 5
end
~
#11818
Venjer the Fox: Goblin Blastmaster combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
set type %random.4%
if %type% == 1
  * Pixycraft Embiggening Elixir
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% takes a pixycraft embiggening elixir from ^%self% belt.
  %echo% (You should probably 'interrupt' *%self% before &%self% drinks it.)
  set drinking 1
  remote drinking %self.id%
  wait 3 sec
  if !%self.varexists(drinking)%
    halt
  end
  %echo% ~%self% uncorks the pixycraft embiggening elixir and licks ^%self% lips.
  wait 3 sec
  if !%self.varexists(drinking)%
    halt
  end
  %echo% ~%self% tips ^%self% head back and downs the pixycraft embiggening elixir in one go!
  %echo% ~%self% suddenly doubles in size!
  dg_affect #11817 %self% BONUS-MAGICAL 100 30
  rdelete drinking %self.id%
elseif %type% == 2
  * Staff Bolts
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% spins ^%self% staff in a circle, and a ball of light starts to grow at its tip...
  wait 5 sec
  if %actor.room% != %self.room%
    set actor %self.fighting%
  end
  if !%actor%
    halt
  end
  %echo% ~%self% sweeps ^%self% staff in an arc and screams, 'Banish!'
  if %self.mob_flagged(GROUP)%
    %echo% &&rA luminous storm of energy bolts rains down upon you!
    %aoe% 200 magical
  else
    if %actor.trigger_counterspell%
      %send% %actor% A luminous energy bolt crashes into your counterspell and explodes!
      %send% %actor% &&rYou are caught in the blast!
    else
      %send% %actor% &&rA luminous energy bolt crashes into you and explodes!
      %damage% %actor% 100 magical
    end
    %echoaround% %actor% A luminous energy bolt crashes into ~%actor% and explodes!
    %echoaround% %actor% &&rYou are caught in the blast!
    %aoe% 50 magical
  end
elseif %type% == 3
  * Blinding Bomb
  nop %self.set_cooldown(11800, 30)%
  %send% %actor% ~%self% pulls out a bomb and hurls it at you!
  %echoaround% %actor% ~%self% pulls out a bomb and hurls it at ~%actor%!
  if %self.mob_flagged(GROUP)%
    %echo% &&r|%self% bomb explodes with a deafening bang and a blinding flash!
  else
    %echo% &&r|%self% bomb explodes with a blinding flash!
  end
  %aoe% 25 physical
  %aoe% 25 fire
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      if %self.mob_flagged(GROUP)%
        %send% %person% You are briefly stunned by the deafening sound!
        dg_affect #11824 %person% STUNNED on 5
      end
      dg_affect #11823 %person% BLIND on 10
    end
    set person %person.next_in_room%
  done
elseif %type% == 4
  * Earthquake
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% raises ^%self% staff high in the air and slams it into the floor!
  wait 3 sec
  %echo% A large glowing sigil spreads across the floor from the impact of |%self% staff...
  wait 3 sec
  %echo% |%self% sigil flares brightly!
  %regionecho% %self.room% 5 The earth shakes beneath you!
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      eval magnitude %self.level% / 8
      if %self.mob_flagged(GROUP)%
        eval magnitude %magnitude% * 2
      end
      if %self.mob_flagged(GROUP)%
        %send% %person% The earthquake knocks you off your feet!
        dg_affect #11814 %person% STUNNED on 5
      else
        %send% %person% The earthquake knocks you off-balance!
      end
      dg_affect #11818 %person% TO-HIT -%magnitude% 15
      dg_affect #11818 %person% DODGE -%magnitude% 15
    end
    set person %person.next_in_room%
  done
end
~
#11819
Skycleave Pixy Queen combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
set type %random.4%
if %type% == 1
  * Shrinking Spell
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% draws a glittering wand from a pocket in ^%self% gown.
  %echo% (You should probably 'interrupt' *%self% before &%self% uses it.)
  set casting 1
  remote casting %self.id%
  wait 3 sec
  if !%self.varexists(casting)%
    halt
  end
  %echo% ~%self% swirls the glittering wand in wide circles, sending sparkles everywhere.
  wait 3 sec
  if !%self.varexists(casting)%
    halt
  end
  %echo% ~%self% whips the glittering wand in your direction and a jet of sparkles shoots out!
  set scale %self.level%/4
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      dg_affect #11820 %person% BONUS-PHYSICAL -%scale% 30
      dg_affect #11821 %person% BONUS-MAGICAL -%scale% 30
    end
    set person %person.next_in_room%
  done
  rdelete casting %self.id%
elseif %type% == 2
  * Thunderbolt
  nop %self.set_cooldown(11800, 30)%
  %send% %actor% ~%self% points a tiny hand at you, lightning crackling around ^%self% fingertips!
  %echoaround% %actor% ~%self% points a tiny hand at ~%actor%, lightning crackling around ^%self% fingertips!
  wait 3 sec
  if %actor.room% != %self.room%
    set actor %self.fighting%
  end
  if !%actor%
    halt
  end
  if %actor.trigger_counterspell%
    %send% %actor% A bolt of lightning leaps from |%self% fingertips, but your counterspell deflects it!
    %echoaround% %actor% A bolt of lightning leaps from |%self% fingertips, striking the floor in front of ~%actor%.
    wait 2 sec
    %echo% ~%self% screams with fury.
    nop %self.set_cooldown(11800, 15)%
    halt
  end
  %send% %actor% &&rA bolt of lightning leaps from |%self% fingertips, striking you!
  %echoaround% %actor% &&rA bolt of lightning leaps from |%self% fingertips, striking ~%actor%!
  %damage% %actor% 200 magical
  %send% %actor% You collapse to the floor, twitching.
  %echoaround% %actor% ~%actor% collapses to the floor, twitching.
  dg_affect #11851 %actor% HARD-STUNNED on 10
  wait 2 sec
  %echo% ~%self% folds her arms and cackles maniacally.
elseif %type% == 3
  * Pixy Gale
  nop %self.set_cooldown(11800, 30)%
  dg_affect #11819 %self% HARD-STUNNED on 10
  %echo% ~%self% raises ^%self% arms into the air and wind begins to swirl around *%self%...
  wait 3 sec
  %echo% A howling gale fills the entire chamber!
  wait 2 sec
  set difficulty 1
  if %self.mob_flagged(HARD)%
    eval difficulty %difficulty% + 1
  end
  if %self.mob_flagged(GROUP)%
    eval difficulty %difficulty% + 2
  end
  eval loops 2 + %difficulty%
  while %loops%
    %echo% &&rInvisible blades of wind slash at you!
    %aoe% 75 magical
    eval loops %loops% - 1
    if %loops%
      wait 5 sec
    end
  done
  %echo% |%self% pixy gale fades away.
elseif %type% == 4
  * Baleful Polymorph
  nop %self.set_cooldown(11800, 15)%
  set target %random.enemy%
  if !%target%
    set target %actor%
  end
  if %target.morph% == 11827
    nop %self.set_cooldown(11800, 0)%
    halt
  end
  %echo% ~%self% snaps ^%self% fingers...
  set old_shortdesc %target.name%
  %morph% %target% 11827
  %echoaround% %target% %old_shortdesc% is suddenly transformed into ~%target%!
  %send% %target% You are suddenly transformed into %target.name%!
  %send% %target% (Type 'fastmorph normal' to transform yourself back.)
end
~
#11820
Escaped Pixy combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Trip
  nop %self.set_cooldown(11800, 20)%
  if %self.difficulty%==4
    %send% %actor% ~%self% bites you on the ankle!
    %echoaround% %actor% ~%self% bites ~%actor% on the ankle!
    %damage% %actor% 50 physical
  else
    %send% %actor% ~%self% dives towards your legs!
    %echoaround% %actor% ~%self% dives towards |%actor% legs!
  end
  %send% %actor% You trip and fall!
  %echoaround% %actor% ~%actor% trips and falls!
  dg_affect #11814 %actor% HARD-STUNNED on 5
else
  * Steal Weapon
  nop %self.set_cooldown(11800, 20)%
  if %self.difficulty%==4
    %send% %actor% &&r~%self% zaps you with a tiny bolt of lightning!
    %echoaround% %actor% ~%self% zaps ~%actor% with a tiny bolt of lightning!
    %damage% %actor% 50 magical
  else
    %send% %actor% ~%self% flies circles around your head...
    %echoaround% %actor% ~%self% flies circles around |%actor% head...
  end
  %send% %actor% ~%self% snatches your weapon while you're distracted!
  %echoaround% %actor% ~%self% snatches |%actor% weapon while &%actor%'s distracted!
  dg_affect #11815 %actor% DISARMED on 20
end
~
#11821
Pixy Queen: interrupt spellcasting~
0 c 0
interrupt~
if !%self.varexists(casting)%
  %send% %actor% You don't need to do that right now.
  halt
end
%send% %actor% You knock the wand out of |%self% hand!
%echoaround% %actor% ~%actor% knocks the wand out of |%self% hand!
%echo% The glittering wand dissolves in a cloud of sparkles and ~%self% lets out a cry of rage.
rdelete casting %self.id%
nop %self.set_cooldown(11800, 15)%
~
#11822
Goblin Blastmaster: interrupt drinking~
0 c 0
interrupt~
if !%self.varexists(drinking)%
  %send% %actor% You don't need to do that right now.
  halt
end
%send% %actor% You knock the elixir out of |%self% hand before &%self% can drink it!
%echoaround% %actor% ~%actor% knocks the elixir out of |%self% hand before &%self% can drink it!
%echo% The elixir shatters on the floor and ~%self% lets out a screech of rage.
rdelete drinking %self.id%
nop %self.set_cooldown(11800, 15)%
~
#11823
Skycleave: Boss Deaths (Pixy, Kara, Barrosh, Shade)~
0 f 100
~
switch %self.vnum%
  case 11819
    * Pixy Queen
    makeuid maze room i11825
    set shield %maze.contents(11887)%
    if %shield%
      %at% %maze% %echo% %shield.shortdesc% flickers and fades!
      %purge% %shield%
    end
    %echo% The pixy queen sputters as her power fades and a look of shock -- and peace -- crosses her face.
    say Thank you for this release, be it ever so brief. We shall meet... again... in time.
    * dies with death-cry
  break
  case 11847
    * Kara Virduke
    set sanjiv %self.room.people(11835)%
    if %sanjiv%
      %echo% The camouflage around Apprentice Sanjiv fades as he collapses to his knees and vomits on the floor.
      %mod% %sanjiv% longdesc An apprentice is kneeling on the floor, heaving.
      %mod% %sanjiv% lookdesc The young apprentice is on his knees, dirtying his white kurta. His wavy brown hair is matted with sweat and he heaves as if he might vomit again.
    end
  break
  case 11863
    * Shadow Ascendant
    makeuid hall room i11870
    set shadow %hall.contents(11863)%
    if %shadow%
      %at% %hall% %echo% @%shadow% fades and dissipates.
      %purge% %shadow%
    end
    %echo% ~%self% dissolves as the shadows are cast out!
    * set room desc back
    %mod% %self.room% description This windowed office is the highest in the tower, a place for the greatest sorcerer to watch over their charges. It has a massive calamander bookshelf along one wall and plush rugs
    %mod% %self.room% append-description cover the checkerboard marble floor. An extravagant nocturnium chandelier hangs from the ceiling. Near the door, a painting of three sorcerers hangs in a gold frame.
    return 0
  break
  case 11867
    * Mind-controlled Barrosh
    set ch %self.room.people%
    while %ch%
      if %ch% != %self%
        dg_affect %ch% HARD-STUNNED on 15
      end
      set ch %ch.next_in_room%
    done
    %load% mob 11861
    return 0
  break
  case 11869
    * Shade of Mezvienne
    makeuid hall room i11870
    set shadow %hall.contents(11863)%
    if %shadow%
      %at% %hall% %echo% @%shadow% fades and dissipates.
      %purge% %shadow%
    end
    %echo% ~%self% dissolves as the shadows are cast out!
    * swap Knezzes
    set knezz %instance.mob(11868)%
    if %knezz%
      %purge% %knezz%
      %load% mob 11870
    end
    return 0
  break
done
~
#11824
Skycleave: 2A Goblin names~
0 n 100
~
* Names the goblins sequentially - 14 max each
switch %self.vnum%
  case 11815
    * uncaged goblin
    set name_list Grimscar Crunchleaf Rattlecage Quicklock Stabfriend Lootfirst Gutcheck Rocktooth Jumpdie Shivfast Biteasy Stabsall Dirtnap Littleglory
    set sex_list  male     female     female     male      male       male      female   female    male    female   male    female   male
  break
  case 11816
    * goblin commando
    set name_list Wormer Felks  Reek Lurn Myna   Cob  Jaden Yules  Gurk Hungore Bilk Mord Axer Nelbog
    set sex_list  male   female male male female male male  female male male    male male male female
  break
  case 11817
    * goblin bruiser
    set name_list Vyle Grosk Brang  Fance  Byth   Slansh Fonk Rong Shyf   Myte   Vrack Rend Worze  Chum
    set sex_list  male male  female female female male   male male female female male  male female male
  break
done
* get list pos
set varname %self.vnum%_name
set spirit %instance.mob(11900)%
if %spirit.varexists(%varname%)%
  eval pos %%spirit.%varname%%% + 1
else
  set pos 1
end
* store back to spirit
set %varname% %pos%
remote %varname% %spirit.id%
* determine random name and sex
while %pos% > 0 && %name_list%
  set name %name_list.car%
  set name_list %name_list.cdr%
  set sex %sex_list.car%
  set sex_list %sex_list.cdr%
  eval pos %pos% - 1
done
* ensure data
if !%name%
  set name Nilbog
end
if !%sex%
  set sex female
end
* and apply
%mod% %self% sex %sex%
%mod% %self% keywords %name% goblin
%mod% %self% shortdesc %name%
* longdesc/lookdesc based on vnum
switch %self.vnum%
  case 11815
    * uncaged goblin
    %mod% %self% longdesc %name% is resting here.
    %mod% %self% lookdesc This little goblin looks positively malnourished as %self.hisher% boney frame protrudes through %self.hisher%
    %mod% %self% append-lookdesc pale green skin. Dressed only in rags and shaking as %self.heshe% stands, %self.heshe% somehow raises %self.hisher% fists to fight.
  break
  case 11816
    * goblin commando
    %mod% %self% longdesc %name% is searching around for something.
    %mod% %self% lookdesc Armed with dozens of tiny knives, this forest-green goblin has come ready for war.
  break
  case 11817
    * goblin bruiser
    %mod% %self% longdesc %name% is punching a wall.
    %mod% %self% lookdesc All muscle and no quit, this green little goblin looks like %self.heshe% means business. With a war axe in one hand and a spear in the other, %self.heshe% paces back and forth, just waiting for you to make your move.
  break
done
detach 11824 %self.id%
~
#11825
Skycleave: Lower floor one-time intros~
0 abc 100
start_messages~
* plays a boss intro until it runs out of lines, then detaches
* ensure not fighting or detach
if %self.fighting% || %self.disabled%
  detach 11825 %self.id%
  halt
end
* fetch position
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
* store line back
remote line %self.id%
set done 0
set no_unflag 0
* switch: message cases goes +1 per 13 seconds and automatically ends when it hits default
if %self.vnum% == 11818
  * Venjer the Fox
  switch %line%
    case 1
      say Stupid pixies, stupid maze, stupid tower, but smart Venjer...
      wait 9 sec
      say And smart adventurers. Conquered the pixy maze just like Venjer, only slower.
      wait 9 sec
      say Now just have to figure out how to get out. Not you, just me. You die in the maze.
    break
    case 2
      say Everything upstairs is already ours, just have to get up there.
      wait 6 sec
      say Not yours. Never yours. Never theirs. Never theirs!
    break
    default
      set done 1
    break
  done
elseif %self.vnum% == 11847
  * Kara Virduke, Mercenary Archmage boss
  switch %line%
    case 1
      say Must be something in here somewhere...
      %mod% %self% longdesc Kara Virduke is digging through cabinets.
      wait 9 sec
      %echo% ~%self% opens drawer after drawer and rifles through them.
      wait 9 sec
      %echo% ~%self% finally notices you and turns.
    break
    case 2
      say This place is a treasure trove. Are you here to loot it or rescue it?
      wait 6 sec
      say You know what? I don't care which.
    break
    case 3
      say We got here first.
      wait 9 sec
      %echo% ~%self% stops and looks you up and down, making a face that seems dismissive, but perhaps also impressed.
      wait 9 sec
      say Has anyone ever told you you have a powerful aura?
    break
    case 4
      %echo% ~%self% holds the foot end her staff out toward your stomach and draws an invisible line across it.
      wait 9 sec
      say I would just love to see what portents we can augur out of your precious hero intestines.
      wait 9 sec
      %echo% ~%self% raises her staff and begins channeling mana.
      %mod% %self% longdesc Kara Virduke is holding her staff in the air, channeling mana from the tower!
    break
    default
      set done 1
    break
  done
elseif %self.vnum% == 11849
  * Trixton Vye, Mercenary Leader
  switch %line%
    case 1
      say It doesn't seem to be in here. I thought that old carcass would have...
      wait 9 sec
      %echo% ~%self% turns and notices you.
      wait 9 sec
      say Sorry, not who I was expecting. Are you even on the payroll? We can't afford to take on more mercenaries.
    break
    case 2
      say That witch Mezvienne is barely paying us. Loot the tower, she said.
      wait 6 sec
      say Sell the artifacts, she said.
    break
    case 3
      say There's a fortune stocked away in the lich's lab, she said.
      wait 6 sec
      say Just don't open the repository. Got it. Won't open it. But there's nothing in here I can sell.
    break
    case 4
      %echo% ~%self% sighs.
      wait 9 sec
      say Wait, how did you even get in here?
      wait 9 sec
      say All this loot is ours. Mezvienne doesn't want to be bothered. Why don't you leave while you're ahead?
    break
    default
      set done 1
      set no_unflag 1
    break
  done
elseif %self.vnum% == 11810
  * Watcher Annaca - phase 2A
  switch %line%
    case 1
      say Oh, not another... oh, you're not goblins. Finally some good news.
      wait 9 sec
      say I can't hold this shield much longer. The mercenaries are contained on the upper levels.
      wait 9 sec
      say Which is not necessarily a good thing. All out most powerful artifacts are up there.
    break
    case 2
      say I'm pretty sure masters Knezz and Barrosh are up there, so they may already have it handled...
      wait 9 sec
      say I need to hold this shield up here until the goblins and pixies have been dealt with down here.
      wait 9 sec
      say The pixies have enchanted one of the sculptures. It's powering the maze.
    break
    case 3
      say You'll have to be the one to find it. I can't leave this post until it's done.
    break
    default
      set done 1
    break
  done
elseif %self.vnum% == 11890
  * High Sorcerer Celiya 1A
  switch %line%
    case 1
      say It's rather a good thing you're here...
      wait 9 sec
      say There is simply too much going on down here, and too much going on up there.
      wait 9 sec
      say We are not at full strength and haven't the people to retake the tower at the moment.
    break
    case 2
      say And anyway, it might be easier for one person or even a small team, as they will not see it coming...
      wait 9 sec
      say I can lower the wards on the staircase and let you up, if you're willing to help.
      wait 9 sec
      say The second floor is overrun with goblins and pixies. See if you can find Annaca while you're up there, too. She's one of our watchers.
    break
    case 3
      say Just let me know when you're ready... (difficulty)
    break
    default
      set done 1
    break
  done
elseif %self.vnum% == 11891
  * Watcher Annaca - phase 2B
  switch %line%
    case 1
      say It looks like you did it...
      wait 9 sec
      say That creepy-crawling pixy maze is gone. Hopefully the entire floor is back to normal.
      wait 9 sec
      say I don't hear any more goblins, either, so maybe John got them back under control.
    break
    case 2
      say If you're heading upstairs, be on the lookout for cutthroat mercenaries.
    break
    default
      set done 1
    break
  done
end
* detach if done
if %done%
  if %self.aff_flagged(!ATTACK)% && !%no_unflag%
    dg_affect %self% !ATTACK off
  end
  detach 11825 %self.id%
end
~
#11826
Skycleave: Pixy Maze track dummy~
2 c 0
track~
if !%actor.ability(Track)% || !%arg%
  return 0
  halt
end
if %room.varexists(random_track)%
  set random_track %room.random_track%
elseif %room.template% == 11818
  set random_track west
else
  switch %random.3%
    case 1
      set random_track west
    break
    case 2
      set random_track north
    break
    case 3
      set random_track east
    break
  done
  remote random_track %room.id%
end
%send% %actor% You find a trail heading %random_track%!
~
#11827
Skycleave: Spawn tourists in 1B~
2 b 20
~
* spawns 1 from the list at random each time and stores the rest to the room
if %room.varexists(vnum_list)%
  set vnum_list %room.vnum_list%
  set list_size %room.list_size%
else
  set vnum_list 11800 11801 11802 11803 11808 11809 11821 11822 11823 11824 11826
  set list_size 11
end
* pick a spot in the list
eval pos %%random.%list_size%%%
while %pos% > 0
  set vnum %vnum_list.car%
  set vnum_list %vnum_list.cdr%
  if %pos% != 1
    set vnum_list %vnum_list% %vnum%
  end
  eval pos %pos% - 1
done
* store back the remaining list for next time
eval list_size %list_size% - 1
remote vnum_list %room.id%
remote list_size %room.id%
* load the mob
if %vnum%
  %load% mob %vnum%
  set mob %room.people%
  if %mob.vnum% == %vnum%
    %echo% ~%mob% walks in from the foyer.
    makeuid foyer room i11800
    %at% %foyer% %echo% ~%mob% walks through the foyer and into the tower.
  end
end
* and detach if the list is empty
if !%vnum% || !%vnum_list% || %list_size% < 1
  detach 11827 %room.id%
end
~
#11828
Skycleave: Floor 1B tourist dialogue~
0 bw 20
~
set room %self.room%
wait 2 sec
if %self.room% != %room%
  halt
end
* check time limits
if %room.varexists(speak_time)%
  if %room.speak_time% + 30 > %timestamp%
    halt
  end
end
set varname last_%room.template%
if %self.varexists(%varname%)%
  eval last %%self.%varname%%%
  if %last% + 150 > %timestamp%
    halt
  end
end
* by mob vnum
if %self.vnum% == 11824 || %self.vnum% == 11826
  * shop patrons wander until they find the shop
  if %room.template% == 11907
    nop %self.add_mob_flag(SENTINEL)%
    nop %self.remove_mob_flag(SILENT)%
    detach 11828 %self.id%
  end
  halt
elseif %self.vnum% == 11801
  * Dylane wanders until he finds the cafe
  if %room.template% == 11905
    nop %self.add_mob_flag(SENTINEL)%
    nop %self.remove_mob_flag(SILENT)%
    detach 11828 %self.id%
  end
  halt
elseif %self.vnum% == 11803
  * self-proclaimed expert
  switch %room.template%
    case 11905
      say Ah, the famous Fendreciel Cafe... They are positively known for their cocoa van.
    break
    case 11907
      say A Towerwinkel's, eh? I've been to the one in Newcastle, of course.
    break
    case 11908
      say It's a well-known fact that a demon lives in the fountain. Of course, there's no way into its realm.
    break
    default
      set line %random.4%
      if %line% == 1
        say This whole tower is built on an ancient goblin burial ground. That's why it's so cursed.
      elseif %line% == 2
        say Nobody actually knows who built the Black Tower. One of the enduring mysteries.
      elseif %line% == 3
        say Grand High Sorcerer Ness and I go way, way back. Old chums. I was the one who told him to become a wizard.
      elseif %line% == 4
        say Most of that sculpture is actually iron. They just say it's nocturnium because you can't tell the difference if it's not rusted.
      end
    break
  done
elseif %self.vnum% == 11809
  * thinks he's somewhere else
  switch %room.template%
    case 11905
      say Oh wow, I've never been to a Gordon Ram's Head restaurant before!
    break
    case 11907
      say Oh! I've been to one of these before. Where are the rides at?
    break
    case 11908
      say Wow, would you look at that fountain? That must be why they call it Fountain Heights.
    break
    default
      set line %random.4%
      if %line% == 1
        say I'm really excited to be here. I heard there was boxing.
      elseif %line% == 2
        say Do you think the king will be here or is he away on diplomacy?
      elseif %line% == 3
        say I've heard so much about this tower! Do you think they still have the portal with the dracosaurs?
      elseif %line% == 4
        say Wow, wait, does that tree mean we're on the ceiling?
      end
    break
  done
elseif %self.vnum% == 11822
  * casing the joint for next time
  switch %room.template%
    case 11902
      %echo% ~%self% pulls out a small notebook and sketches the stairs and walkway from across the chamber.
    break
    case 11903
      %echo% ~%self% leans against the wall near the door and listens non-chalantly.
    break
    case 11904
      %echo% ~%self% seems to be measuring the distance from the doorway to the stairs.
    break
    case 11905
      %echo% ~%self% sits for a few moments at a table where &%self% can see most of the central chamber through the archway.
    break
    case 11907
      %echo% ~%self% wanders around the store, checking behind the racks and under the shelves when no one else is looking.
    break
    case 11908
      %echo% ~%self% studies the fountain and makes a surreptitious note before stashing the notebook in ^%self% sleeve.
    break
  done
elseif %self.vnum% == 11827
  * Chatty couple: Apprentices Marie 11827 + Djon 11828
  set djon %room.people(11828)%
  if !%djon%
    halt
  end
  if %self.varexists(line)%
    eval line %self.line% + 1
  else
    set line 1
  end
  if %line% > 1
    set line 1
  end
  remote line %self.id%
  switch %line%
    case 1
      say Can you believe Mezvienne herself attacked the tower?
      wait 9 s
      %force% %djon% say I know, right? I heard she was possessed by a time lion.
      wait 9 s
      say Please don't tell me you believe in time lions now.
      wait 9 s
      %force% %djon% say Ok, how does a rank amateur of modest reputation...
      wait 3 s
      say Modest?
      wait 3 s
      %force% %djon% say ...of above-average reputation invade the single greatest sorcery tower in history?
      wait 9 s
      if %instance.mob(11969)%
        say Well, she defeated GHS Knezz in single combat.
      else
        say And yet she did.
      end
      wait 9 s
      %force% %djon% say Fine. Where'd you hide out?
      wait 9 s
      say Attunement lab with a gargoyle charm.
      wait 9 s
      %force% %djon% say Clever. I warded myself in Niamh's office.
      wait 9 s
      say Smart.
      wait 9 s
      %echo% ~%self% and ~%djon% sip their tea.
    break
    *** HERE
  done
end
* enforce a short wait on the room with a var
set speak_time %timestamp%
remote speak_time %room.id%
set last_%room.template% %timestamp%
remote last_%room.template% %self.id%
~
#11829
Skycleave: Otherworlder escape~
0 ab 100
~
if %self.fighting% || %self.disabled%
  halt
end
set room %self.room%
if %room.template% == 11835
  %echo% The otherworlder whips its left arm against its chest and shouts, 'Bok jel thet!'
  wait 6 sec
  %echo% A shrill, disembodied voice says, 'Mu cha shah ka pah ka.'
  wait 6 sec
  set kara %room.people(11847)%
  if %kara%
    %echo% The otherworlder shrieks and whips one of its long arms at ~%kara%, who is hit in the head and stunned!
    dg_affect %kara% STUNNED on 30
    wait 3 sec
  end
  %echo% The otherworlder swishes past you and darts out of the room!
  mgoto i11832
  %echo% ~%self% darts in from the east.
elseif %room.template% == 11832 || %room.template% == 11833
  set merc_vnums 11841 11842 11843 11844 11845 11846
  set ch %room.people%
  set done 0
  while %ch% && !%done%
    set next_ch %ch.next_in_room%
    if %ch.is_npc% && %merc_vnums% ~= %ch.vnum%
      nop %ch.add_mob_flag(!LOOT)%
      if %room.varexists(overboard)%
        * basic kill
        %echo% ~%self% swings ^%self% arms wildly, striking ~%ch% and killing *%ch%!
        %slay% %ch%
      else
        * overboard them
        makeuid downstairs room i11808
        %echo% ~%self% grabs ~%ch% and throws *%ch% over the railing!
        %echo% Your blood freezes as you hear a death cry and a splat!
        %teleport% %ch% %downstairs%
        %at% %downstairs% %echo% There's a loud scream as ~%ch% falls down from one of the floors above and splats on the tile floor!
        %at% %downstairs% %slay% %ch%
        set overboard 1
        remote overboard %room.id%
      end
      set done 1
    end
    set ch %next_ch%
  done
  if !%done%
    if %room.template% == 11832
      northwest
    elseif %room.template% == 11833
      north
    end
  end
elseif %room.template% == 11836
  %echo% The otherworlder barrels through, flailing ^%self% arms wildly, and jumps out the window!
  %echo% The otherworlder shouts, 'Bok jel thet far mesh bek tol cha... Shaw!
  %echo% You watch out the window as the otherworlder is enveloped in shimmering blue light and vanishes in midair!
  %purge% %self%
else
  * unknown location somehow? Oh well, we tried
  detach 11829 %self.id%
end
~
#11830
Skycleave: Shared quest completion script~
2 v 0
~
return 1
switch %questvnum%
  case 11802
    * Dylane/Mop rescue
    set mop %instance.mob(11933)%
    if %mop%
      if %mop.room% != %room%
        %at% %mop.room% %echo% ~%mop% hops away.
        %teleport% %mop% %room%
      end
    else
      %load% mob 11933
      set mop %room.people%
      if %mop.vnum% != 11933
        %send% %actor% This quest seems to be broken.
        return 0
        halt
      end
    end
    if !%mop.has_trigger(11833)%
      attach 11833 %mop.id%
    end
    %force% %mop% cutscene
  break
  case 11810
  case 11811
  case 11812
  case 11875
  case 11975
    * these scripts guarantee Liked reputation
    if !%actor.has_reputation(11800,Liked)%
      nop %actor.set_reputation(11800,Liked)%
    end
  break
  case 11821
    * Hugh Mann
    set hugh %room.people(11821)%
    if %hugh%
      %echo% # \&0
      %echo% # ~%hugh% grasps at some goblin trinkets and shouts, 'We did it! Victory for goblins!'
      %echo% # ~%hugh% throws off his coat to reveal he is actually two goblins, one on the other's shoulder!
      %echo% # The goblins sing and shout victory chants as they run out of the tower!
      %purge% %hugh%
    end
  break
done
~
#11831
Skycleave: Shared quest start script~
2 u 0
~
return 1
set spirit %instance.mob(11900)%
switch %questvnum%
  case 11810
    if %spirit.phase2%
      %send% %actor% You cannot start '%questname%' because floor 2 has already been rescued.
      return 0
    end
  break
  case 11811
    if %spirit.phase3%
      %send% %actor% You cannot start '%questname%' because floor 3 has already been rescued.
      return 0
    end
  break
  case 11812
    if %spirit.phase4%
      %send% %actor% You cannot start '%questname%' because floor 4 has already been rescued.
      return 0
    end
  break
done
~
#11832
Skycleave: Conditional mob visibility~
0 iC 100
~
* toggles silent, !see
* activated on greet or when moving
set see 0
set not 0
set vis 0
* special handling to catch actor when not in the room
if %actor%
  set ch %actor%
  set spec 1
else
  set ch %self.room.people%
  set spec 0
end
* determine if anyone should/shouldn't see me
while %ch%
  if %ch.is_pc%
    if %self.vnum% == 11900
      * intro spirit: shows when any floor quest complete
      if %ch.completed_quest(11810)% || %ch.completed_quest(11811)% || %ch.completed_quest(11812)%
        set see 1
      else
        set not 1
      end
    elseif %self.vnum% == 11801
      * Dylane: visible only if quest incomplete
      if %ch.completed_quest(11802)%
        set not 1
      else
       set see 1
      end
    end
  end
  if %spec%
    set spec 0
    set ch %self.room.people%
  else
    set ch %ch.next_in_room%
  end
done
* determine behavior based on vnum
switch %self.vnum%
  case 11900
    * intro spirit: any "see"
    set vis %see%
  break
  case 11801
    * Dylane
    if !%not%
      set vis 1
    end
  break
done
* and make visible
if %vis% && %self.affect(11832)%
  dg_affect #11832 %self% off
  if %self.vnum% != 11801 || %self.room.template% == 11905
    * 11801 Dylane does not remove silent unless he's in the cafe
    nop %self.remove_mob_flag(SILENT)%
  end
  %echo% ~%self% arrives.
elseif !%vis% && !%self.affect(11832)%
  dg_affect #11832 %self% !SEE on -1
  dg_affect #11832 %self% !TARGET on -1
  dg_affect #11832 %self% SNEAK on -1
  nop %self.add_mob_flag(SILENT)%
  %echo% ~%self% leaves.
end
~
#11833
Skycleave: Quest cutscenes~
0 cx 0
cutscene~
* used for mob cutscenes
if %actor% && %actor% != %self%
  return 0
  halt
end
if %self.vnum% == 11933
  * mop 11933, Dylane quest 11802
  set annelise %self.room.people(11939)%
  nop %self.add_mob_flag(SILENT)%
  nop %self.add_mob_flag(SENTINEL)%
  wait 1
  %echo% ~%self% marches in from the north.
  wait 3 sec
  %echo% ~%annelise% pulls a surprisingly large staff out of her many-layered robes...
  wait 9 sec
  %force% %annelise% say Let the weight of your service to the Tower outweigh your offense against its masters.
  %echo% She taps her staff on the slate floor three times.
  wait 9 sec
  %force% %annelise% say Ahem... In service of the Tower I restore you bodily as you once were!
  %echo% She taps her staff on the floor agian.
  wait 9 sec
  %echo% ~%annelise% sighs.
  wait 3 sec
  say By the Black Domain, by all humane, let him regain... Er, un-detain the plain Dylane!
  wait 1
  %echo% There's a blinding flash from Annelise's staff as she hits it on the floor one last time...
  * convert to boy
  detach 11935 %self.id%
  detach 11934 %self.id%
  %mod% %self% keywords Dylane younger boy apprentice
  %mod% %self% shortdesc Dylane the Younger
  %mod% %self% longdesc A boy of no more than fourteen stands in the center of the room.
  %mod% %self% lookdesc Dylane is no more than fourteen years old, tall and gangly with legs that look like a pair of sticks poking out from under white apprentice robes that
  %mod% %self% append-lookdesc are inexplicably wet at the bottom. Dark, teary eyes poke out from underneath a mop of straw-colored hair as he stares at you wordlessly.
  * and finish
  wait 9 sec
  %force% %annelise% say Rather embarassing to admit I'm dreadful at non-rhyming magic.
  wait 6 sec
  say What happened? Oh! I can speak again! I'm free! I'm free!
  wait 9 sec
  %echo% ~%self% turns and sees ~%annelise% and his face turns sheet-white.
  wait 9 sec
  %force% %annelise% say Yes, you're free, you're free. You shall find your father in the lobby. May I suggest you make a hasty escape?
  wait 9 sec
  %force% %annelise% say Okay, on with you, get out of here before the grand high sorcerer catches you lacking.
  wait 3 sec
  %echo% The boy wastes no more time and darts out of the room. The last you hear of him is clambering down the stairs.
  %purge% %self%
end
~
#11834
Skycleave: Free the otherworlder~
0 c 0
free unchain unshackle~
return 1
* validate arg
if %actor.fighting%
  %send% %actor% You're a little busy at the moment!
  halt
elseif !%arg%
  %send% %actor% Free whom?
  halt
elseif %actor.char_target(%arg%)% != %self%
  %send% %actor% You can't free that.
  halt
elseif %self.vnum% == 11934
  %send% %actor% The shackles have been magically secured. You cannot free the otherworlder.
  halt
end
* echoes
%send% %actor% You smash the violet gems that bind the shackles and they shatter... and the otherworlder breaks free!
%echoaround% %actor% ~%actor% smashes the violet gems that bind the otherworlder's shackles and they shatter... the otherworlder breaks free!
* let's get this bread
%load% mob 11829
set mob %self.room.people%
if %mob.vnum% == 11829
  set spirit %instance.mob(11900)%
  if (%spirit.diff3% // 2) == 0
    nop %mob.add_mob_flag(HARD)%
  end
  if %spirit.diff3% >= 3
    nop %mob.add_mob_flag(GROUP)%
  end
end
* check triggers
set ch %self.room.people%
while %ch%
  if %ch.on_quest(11834)%
    %quest% %ch% trigger 11834
  end
  set ch %ch.next_in_room%
done
* and me
%purge% %self%
~
#11835
Skycleave: Barricade restrings~
1 n 100
~
* not listed here: 11810 (uses default barricade), 11808 (default crate wall)
switch %self.room.template%
  case 11830
    * Third Floor West (3A)
    %mod% %self% keywords table large barricade overturned
    %mod% %self% longdesc A large table barricades the top of the steps.
    %mod% %self% lookdesc The square oak table is on its side, requiring some agility to get past it and up off of the stairs. The table is missing one leg and there are scorch marks on both the top and bottom of it.
  break
  case 11832
    * Third Floor East (3A)
    %mod% %self% keywords furniture stack barricade chairs tables
    %mod% %self% shortdesc a stack of furniture
    %mod% %self% longdesc This section of the walkway is blocked off with a stack of furniture.
    %mod% %self% lookdesc Skycleave's fancy chairs and end tables have been stacked to force would-be rescuers to move single-file through a narrow gap in order to make progress along the walkway.
  break
  case 11836
    * Lich Labs (3A)
    %mod% %self% keywords bookshelf shelf overturned massive
    %mod% %self% shortdesc an overturned bookshelf
    %mod% %self% longdesc A massive bookshelf is overturned in front of the door.
    %mod% %self% lookdesc A truly massive bookshelf has been turned on its side and pushed in front of the door to block any view of the room from the walkway outside. You're forced to squeeze by and enter at a disadvantage.
  break
  case 11865
    * Celiya Living Quarters (4A)
    %mod% %self% keywords wardrobe hefty
    %mod% %self% shortdesc the wardrobe
    %mod% %self% longdesc A hefty wardrobe has been pushed in front of the door, which opens the other way.
    %mod% %self% lookdesc It looks like someone has pushed the oversized wardrobe in front of the door in a panic, not realizing that the door opens out to the office rather than into the bedroom.
  break
  case 11803
    * Ground Floor N (1A)
    %mod% %self% keywords crates wall stacked wooden makeshift
    %mod% %self% longdesc A makeshift wall of crates blocks off the center of the chamber.
    %mod% %self% lookdesc A wall of stacked crates blocks access to the center of the chamber and its magnificent stone fountain.
  break
  case 11804
    * Ground Floor W (1A)
    %mod% %self% keywords crates wall piled stacked wooden makeshift
    %mod% %self% longdesc Crates have been piled to form a wall blocking the chamber's central fountain to the east.
    %mod% %self% lookdesc Stacks of crates create a makeshift wall that blocks off the center of the great chamber and its grand stone fountain.
  break
done
detach 11835 %self.id%
~
#11836
Release Scaldorran~
1 c 4
open release~
if %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
return 1
%load% mob 11836
set mob %self.room.people%
if %mob.vnum% != 11836
  %send% %actor% Something went wrong.
  halt
end
%send% %actor% You open the repository and release the Lich Scaldorran!
%echoaround% %actor% ~%actor% opens the repository and releases the Lich Scaldorran!
* Mark for claw game & directory
set spirit %instance.mob(11900)%
set claw3 1
remote claw3 %spirit.id%
set lich_released %actor.id%
remote lich_released %spirit.id%
* check triggers
set ch %self.room.people%
while %ch%
  if %ch.on_quest(11836)%
    %quest% %ch% trigger 11836
  end
  set ch %ch.next_in_room%
done
* and done
%purge% %self%
~
#11837
Mercenary Leader no-fight~
0 h 100
~
if %self.aff_flagged(!ATTACK) && !%instance.mob(11848)% && !%instance.mob(11847)%
  wait 1
  %echo% ~%self% has lost ^%self% protection!
  dg_affect %self% !ATTACK off
  detach 11837 %self.id%
end
~
#11838
Skycleave: Weyonomon wanders through walls in 1A~
0 b 15
~
set target 0
set dir north
set from south
switch %self.room.template%
  case 11801
    set target i11803
    set dir north
    set from south
  break
  case 11802
    set target i11807
    set dir south
    set from south
  break
  case 11803
    set target i11805
    set dir east
    set from north
  break
  case 11804
    set target i11801
    set dir south
    set from west
  break
  case 11805
    set target i11806
    set dir northwest
    set from east
  break
  case 11806
    set target i11802
    set dir southeast
    set from north
  break
  case 11807
    set target i11804
    set dir south
    set from south
  break
  case 11808
    set target i11804
    set dir west
    set from east
  break
done
if %target%
  %echo% ~%self% floats through the wall to the %dir%.
  set ch %self.room.people%
  while %ch%
    if %ch.leader% == %self% && %ch.position% == Standing
      %send% %ch% You try to follow but smack into the wall. Ouch!
      %echoaround% %ch% ~%ch% tries to follow but smacks into the wall with a thud.
      %damage% %ch% 10 physical
    end
    set ch %ch.next_in_room%
  done
  mgoto %target%
  %echo% ~%self% floats in through the wall from the %from%.
end
~
#11839
Skycleave: Scaldorran murder spree~
0 b 100
~
* the last 3 are the bosses; killing any 1 of them will stop him (only kills 1 boss)
set merc_list 11841 11842 11843 11844 11845 11846 11848 11847 11849
* check room first
set ch %self.room.people%
set target 0
set done 0
while %ch% && !%target%
  if %ch.is_npc% && %merc_list% ~= %ch.vnum%
    set target %ch%
  end
  set ch %ch.next_in_room%
done
if %target%
  * found someone to merc
  nop %target.add_mob_flag(!LOOT)%
  switch %target.vnum%
    case 11841
      * rogue merc
      %echo% Scaldorran flies in through |%target% mouth... ~%target% pulls out ^%target% dagger and stabs *%target%self in the heart!
    break
    case 11842
      * caster merc
      %echo% ~%target% panics and hurls a spell at Scaldorran, who expertly reflects it. ~%target% is hit in the head and falls to the ground!
    break
    case 11843
      * archer merc
      %echo% ~%target% shoots a dozen arrows into Scaldorran, who comes apart at the wrappings and envelops ~%target%, stabbing *%target% with every arrow!
    break
    case 11844
      * nature mage merc
      %echo% ~%target% starts to twist and morph into something enormous... until Scaldorran wraps his bandages around *%target%, whereupon &%target% dissolves into a sticky mess!
    break
    case 11845
      * vampire merc
      %echo% Scaldorran twists and whirls and whips ~%target% over and over with his bandages, slicing open thousands of tiny cuts. You watch in horror as ~%target% bleeds out and crumples to the floor.
    break
    case 11846
      * armored merc
      %echo% Scaldorran appears behind ~%target% and slides his bandages into |%target% armor... You are forced to watch as ~%target% turns blue and falls to the ground.
    break
    case 11848
      * Bleak Rojjer
      %echo% Linen wrappings whip out from the painting on the wall and you are forced to watch as Scaldorran strangles ~%target% and bashes his head against the floor.
      set done 1
    break
    case 11847
      * Kara Virduke
      %echo% Scaldorran pops out of a cabinet just as ~%target% opens it and snakes one of his long linen wrappings down her throat. You watch in horror as she claws at her mouth, unable to stop it.
      set done 1
    break
    case 11849
      * Trixton Vye
      %echo% Linen wrappings whip out from the painting on the wall and you are forced to watch as Scaldorran strangles ~%target% and bashes his head against the floor.
      set done 1
    break
  done
  %slay% %target%
  if %done%
    * last one?
    %mod% %self% longdesc The Lich Scaldorran is drawing power from the tower.
    detach 11839 %self.id%
  end
  halt
end
* didn't find someone... try to find someone in another room
while %merc_list%
  set vnum %merc_list.car%
  set merc_list %merc_list.cdr%
  set mob %instance.mob(%vnum%)%
  if %mob% && %mob.room% != %self.room%
    %echo% Scaldorran twists and whirls until his tattered wrappings fold in on themselves and he vanishes!
    mgoto %mob.room%
    %echo% Your blood freezes as the air cracks open and the Lich Scaldorran crawls out of the void!
    halt
  end
done
* nobody left?
%mod% %self% longdesc The Lich Scaldorran is drawing power from the tower.
detach 11839 %self.id%
~
#11840
Skycleave: Storytime using custom strings~
0 bw 100
~
* uses mob custom strings script1-script5 to tell short stories
* usage: .custom add script# <command> <string>
* valid commands: say, emote, do (execute command), echo (script), and skip
* also: vforce <mob vnum in room> <command>
* also: set <line_gap|story_gap> <time> sec
* NOTE: waits for %line_gap% (9 sec) after all commands EXCEPT do/vforce/set
set line_gap 9 sec
set story_gap 180 sec
* find story number
if %self.varexists(story)%
  eval story %self.story% + 1
  if %story% > 5
    set story 1
  end
else
  set story 1
end
* determine valid story number
set tries 0
set ok 0
while %tries% < 5 && !%ok%
  if %self.custom(script%story%,0)%
    set ok 1
  else
    eval story %story% + 1
    if %story% > 5
      set story 1
    end
  end
  eval tries %tries% + 1
done
if !%ok%
  wait %story_gap%
  halt
end
* story detected: prepare (storing as variables prevents reboot issues)
if !%self.mob_flagged(SENTINEL)%
  set no_sentinel 1
  remote no_sentinel %self.id%
  nop %self.add_mob_flag(SENTINEL)%
end
if !%self.mob_flagged(SILENT)%
  set no_silent 1
  remote no_silent %self.id%
  nop %self.add_mob_flag(SILENT)%
end
* tell story
set pos 0
set done 0
while !%done%
  set msg %self.custom(script%story%,%pos%)%
  if %msg%
    set mode %msg.car%
    set msg %msg.cdr%
    if %mode% == say
      say %msg%
      wait %line_gap%
    elseif %mode% == do
      %msg.process%
      * no wait
    elseif %mode% == echo
      %echo% %msg.process%
      wait %line_gap%
    elseif %mode% == vforce
      set vnum %msg.car%
      set msg %msg.cdr%
      set targ %self.room.people(%vnum%)%
      if %targ%
        %force% %targ% %msg.process%
      end
    elseif %mode% == emote
      emote %msg%
      wait %line_gap%
    elseif %mode% == set
      set subtype %msg.car%
      set msg %msg.cdr%
      if %subtype% == line_gap
        set line_gap %msg%
      elseif %subtype% == story_gap
        set story_gap %msg%
      else
        %echo% ~%self%: Invalid set type '%subtype%' in storytime script.
      end
    elseif %mode% == skip
      * nothing this round
      wait %line_gap%
    else
      %echo% %self.name%: Invalid script message type '%mode%'.
    end
  else
    set done 1
  end
  eval pos %pos% + 1
done
remote story %self.id%
* cancel sentinel/silent
if %self.varexists(no_sentinel)%
  nop %self.remove_mob_flag(SENTINEL)%
end
if %self.varexists(no_silent)%
  nop %self.remove_mob_flag(SILENT)%
end
* wait between stories
wait %story_gap%
~
#11841
Mercenary Rogue combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Blind
  nop %self.set_cooldown(11800, 30)%
  if %self.difficulty%==4
    %send% %actor% &&r~%self% slashes your forehead, opening a bleeding wound!
    %send% %actor% Blood trickles down into your eyes, blinding you!
    %echoaround% %actor% ~%self% slashes |%actor% forehead, opening a bleeding wound!
    %echoaround% %actor% Blood trickles down into |%actor% eyes!
    %damage% %actor% 50 physical
    %dot% #11842 %actor% 75 15 physical
    dg_affect #11841 %actor% BLIND on 15
  else
    %send% %actor% ~%self% grabs a handful of sand and throws it in your face, blinding you!
    %echoaround% %actor% ~%self% grabs a handful of sand and throws it in |%actor% face, blinding *%actor%!
    dg_affect #11841 %actor% BLIND on 15
  end
else
  * Dagger Throw
  nop %self.set_cooldown(11800, 30)%
  if %self.difficulty% == 4
    set loops 3
    %echo% ~%self% draws a brace of throwing knives from a concealed pouch.
  else
    set loops 1
    %echo% ~%self% draws a throwing knife from a concealed pouch.
  end
  while %loops%
    set target %random.enemy%
    if !%target%
      set target %actor%
    end
    if %target.room% == %self.room%
      %send% %target% &&r~%self% flings a knife at you!
      %echoaround% %target% ~%self% flings a knife at ~%target%!
      %damage% %target% 100 physical
    end
    eval loops %loops%-1
    if %loops%
      wait 3 sec
    end
  done
end
~
#11842
Mercenary Sorcerer combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Firebrand
  nop %self.set_cooldown(11800, 30)%
  %send% %actor% ~%self% mutters an incantation and taps your arm with ^%self% staff...
  %echoaround% %actor% ~%self% mutters an incantation and taps |%actor% arm with ^%self% staff...
  if %actor.trigger_counterspell%
    %send% %actor% A vortex of smoke swirls harmlessly around your arm for a few moments.
    %echoaround% %actor% A vortex of smoke swirls briefly around |%actor% arm.
    halt
  end
  wait 1 sec
  if %self.difficulty%==4
    if %actor.affect(11843)%
      %send% %actor% &&rThe flames surrounding you grow more intense!
      %echoaround% %actor% The flames surrounding ~%actor% grow more intense!
    else
      %send% %actor% &&rA fiery rune appears on your arm, and you burst into flames!
      %echoaround% %actor% A fiery rune appears on |%actor% arm, and &%actor% bursts into flames!
    end
    %damage% %actor% 100 fire
    %dot% #11843 %actor% 100 45 fire 3
  else
    %send% %actor% &&rA fiery rune appears on your arm, searing your flesh!
    %echoaround% %actor% &&rA fiery rune appears on |%actor% arm, searing ^%actor% flesh!
    %damage% %actor% 50 fire
    %dot% #11843 %actor% 75 30 fire
  end
else
  * Enchant Weapons
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% thrusts ^%self% staff into the air and shouts a magic word!
  set person %self.room.people%
  while %person%
    if %person.is_ally(%self%)%
      %echo% |%person% weapon glows silver.
      if %self.difficulty% == 4
        dg_affect #11844 %person% HASTE on 30
      end
      eval magnitude %self.level%/10
      dg_affect #11844 %person% BONUS-PHYSICAL %magnitude% 30
      dg_affect #11844 %person% BONUS-MAGICAL %magnitude% 30
    end
    set person %person.next_in_room%
  done
end
~
#11843
Mercenary Archer combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Rapid Fire
  nop %self.set_cooldown(11800, 30)%
  dg_affect #11845 %self% HARD-STUNNED on 30
  %echo% ~%self% draws ^%self% bow.
  if %self.difficulty%==4
    set loops 7
  else
    set loops 3
  end
  while %loops%
    set target %random.enemy%
    if !%target%
      set target %actor%
    end
    if %target.room% == %self.room%
      %send% %target% &&r~%self% fires an arrow at you!
      %echoaround% %target% ~%self% fires an arrow at ~%target%!
      %damage% %target% 65 physical
    end
    eval loops %loops%-1
    wait 3 sec
  done
  %echo% ~%self% returns ^%self% bow to ^%self% back.
  dg_affect #11845 %self% off
else
  * Rain of Arrows
  nop %self.set_cooldown(11800, 30)%
  dg_affect #11845 %self% HARD-STUNNED on 30
  %echo% ~%self% draws ^%self% bow and fires a barrage of arrows upward.
  wait 3 sec
  %echo% &&r|%self% arrows rain down upon you!
  %aoe% 75 physical
  wait 3 sec
  %echo% ~%self% returns ^%self% bow to ^%self% back.
  dg_affect #11845 %self% off
end
~
#11844
Mercenary Nature Mage combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Mass Heal
  nop %self.set_cooldown(11800, 30)%
  if %self.morph%
    set current %self.name%
    %morph% %self% normal
    %echo% %current% rapidly morphs into ~%self%!
    wait 1 sec
  end
  %echo% ~%self% thrusts ^%self% hands skyward and sends out a wave of healing light.
  set person %self.room.people%
  while %person%
    if %person.is_ally(%self%)%
      if %person.health% < %person.maxhealth%
        set magnitude 100
        if %self.difficulty% == 4
          set magnitude 200
        end
        %heal% %person% health %magnitude%
        %echo% ~%person% is healed!
      end
    end
    set person %person.next_in_room%
  done
else
  * Snake Morph / Bite
  nop %self.set_cooldown(11800, 30)%
  if !%self.morph%
    set current %self.name%
    %morph% %self% 11848
    %echo% %current% rapidly morphs into ~%self%!
    wait 1 sec
  end
  %send% %actor% &&r~%self% sinks ^%self% fangs into your leg!
  %echoaround% %actor% ~%self% sinks ^%self% fangs into |%actor% leg!
  %send% %actor% You don't feel so good...
  %echoaround% %actor% ~%actor% doesn't look so good...
  %damage% %actor% 50 physical
  %dot% #11848 %actor% 100 15 poison
  if %self.difficulty% == 4
    %damage% %actor% 50 poison
    dg_affect #11848 %actor% SLOW on 15
  end
end
~
#11845
Mercenary Vampire combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Blood Curse
  nop %self.set_cooldown(11800, 30)%
  eval magnitude %self.level%/10
  %echo% ~%self% makes a cut on the palm of ^%self% palm.
  if %self.difficulty% == 4
    %echo% ~%self% presses %self.her% palm to the floor.
    %echo% A crimson glow fills the room, and you feel weaker.
    set person %self.room.people%
    while %person%
      if %person.is_enemy(%self%)%
        dg_affect #11849 %person% BONUS-PHYSICAL -%magnitude% 30
        dg_affect #11849 %person% BONUS-MAGICAL -%magnitude% 30
        dg_affect #11849 %person% BONUS-HEALING -%magnitude% 30
      end
      set person %person.next_in_room%
    done
  else
    %send% %actor% ~%self% smears you with ^%self% blood.
    %echoaround% %actor% ~%self% smears ~%actor% with ^%self% blood.
    %send% %actor% It glows crimson, and you feel weakened.
    %echoaround% %actor% |%self% blood glows crimson, and ~%actor% flinches.
    dg_affect #11849 %actor% BONUS-PHYSICAL -%magnitude% 30
    dg_affect #11849 %actor% BONUS-MAGICAL -%magnitude% 30
    dg_affect #11849 %actor% BONUS-HEALING -%magnitude% 30
  end
else
  * Exsanguinate
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% thrusts ^%self% hand into the sky, and it glows crimson!
  %echo% &&rOld injuries all over your body suddenly begin to bleed!
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      %damage% %person% 10
      if %self.difficulty% == 4
        %dot% #11842 %person% 125 30 physical
      else
        %dot% #11842 %person% 100 15 physical
      end
    end
    set person %person.next_in_room%
  done
end
~
#11846
Mercenary Tank combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
if %random.2% == 1
  * Bash
  nop %self.set_cooldown(11800, 30)%
  %send% %actor% &&r~%self% clubs you over the head with ^%self% weapon!
  %echoaround% %actor% ~%self% clubs ~%actor% over the head with ^%self% weapon!
  %echoaround% %actor% ~%actor% looks dazed.
  %damage% %actor% 100 physical
  if %self.difficulty% == 4
    %send% %actor% The force of the blow leaves you stunned!
    dg_affect #11851 %actor% STUNNED on 10
  else
    %send% %actor% The force of the blow leaves you briefly stunned!
    dg_affect #11851 %actor% STUNNED on 5
  end
else
  * First Aid
  nop %self.set_cooldown(11800, 30)%
  dg_affect #11845 %self% HARD-STUNNED on 30
  wait 1 sec
  %echo% ~%self% sheathes ^%self% weapon and begins to bandage ^%self% injuries...
  %heal% %self% health 50
  wait 3 sec
  if %self.difficulty%==4
    set loops 4
  else
    set loops 2
  end
  while %loops%
    set target %random.enemy%
    if !%target%
      set target %actor%
    end
    %echo% ~%self% continues to bandage ^%self% injuries...
    %heal% %self% health 75
    eval loops %loops%-1
    wait 3 sec
  done
  %echo% ~%self% draws ^%self% weapon.
  dg_affect #11845 %self% off
done
~
#11847
Kara Virduke: Mercenary archmage combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
set type %random.4%
if %type% == 1
  * Chain Lightning - Shorter cooldown
  nop %self.set_cooldown(11800, 20)%
  %send% %actor% &&r~%self% points ^%self% staff at you and blasts you with a bolt of lightning!
  %damage% %actor% 150 magical
  %echoaround% %actor% ~%self% points ^%self% staff at ~%actor% and blasts *%actor% with a bolt of lightning!
  %echoaround% %actor% &&rThe lightning bolt arcs from ~%actor% to you!
  %aoe% 50 magical
elseif %type% == 2
  * Chromatic Bolts
  set bolt_1 red
  set bolt_2 orange
  set bolt_3 yellow
  set bolt_4 green
  set bolt_5 blue
  set bolt_6 indigo
  set bolt_7 violet
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% points ^%self% staff in your direction and seven chromatic bolts of light fly from its tip!
  wait 3 sec
  set bolt_number 1
  while %bolt_number% <= 7
    set target %random.enemy%
    if %target%
      eval bolt_color %%bolt_%bolt_number%%%
      if %target.trigger_counterspell%
        %send% %target% |%self% %bolt_color% bolt is deflected away from you by your counterspell.
        %echoaround% %target% |%self% %bolt_color% bolt bounces off ~%target%.
      else
        %send% %target% &&rYou are struck by |%self% %bolt_color% bolt!
        %echoaround% %target% ~%target% is struck by |%self% %bolt_color% bolt!
        %damage% %target% 100 magical
        switch %bolt_number%
          case 1
            %send% %target% The bolt leaves behind a bleeding wound!
            %dot% #11842 %target% 100 15 physical
          break
          case 2
            %send% %target% You burst into flames!
            %dot% #11843 %target% 150 10 fire
          break
          case 3
            %send% %target% Lightning courses through your body, stunning you!
            dg_affect #11851 %target% STUNNED on 5
          break
          case 4
            %send% %target% Poison courses through your veins!
            %dot% #11858 %target% 75 30 poison
          break
          case 5
            %send% %target% You are struck by a deep chill, slowing your reflexes!
            dg_affect #11851 %target% SLOW on 15
          break
          case 6
            %send% %target% You are engulfed in darkness, blinding you!
            dg_affect #11841 %target% BLIND on 10
          break
          case 7
            %echo% &&rThe violet bolt explodes, filling the room with lances of violet light!
            %aoe% 85 magical
          break
        done
      end
    end
    eval bolt_number %bolt_number% + 1
    if %bolt_number% <= 7
      wait 3 sec
    end
  done
elseif %type% == 3
  * Arcane Tattoos
  if %self.health% == %self.maxhealth%
    halt
  end
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% mutters an incantation...
  %echo% Arcane tattoos all over |%self% body glow gold, and ^%self% wounds begin to close!
  %heal% %self% health
  dg_affect #12425 %self% HEAL-OVER-TIME %self.level% 30
  wait 5 sec
  %echo% ~%self% mutters another incantation...
  %echo% |%self% arcane tattoos glow silver, and &%self% seems to speed up!
  dg_affect #11855 %self% HASTE on 25
elseif %type% == 4 && %self.mob_flagged(GROUP)%
  * Rainbow Beam
  nop %self.set_cooldown(11800, 30)%
  set room %self.room%
  set cycles_left 5
  set prism_vnum 11850
  * This fake ritual is interrupted if combat ends or the focusing prism is destroyed
  while %cycles_left% >= 0
    set person %self.room.people%
    while %person%
      if %person.vnum% == %prism_vnum%
        set prism %person%
        unset person
      else
        set person %person.next_in_room%
      end
    done
    if !%self.fighting% || !%prism%
      if %cycles_left% < 3
        %echoaround% %actor% |%actor% ritual chant is interrupted.
      else
        * combat, stun, sitting down, etc
      end
      halt
    end
    * Fake ritual messages
    switch %cycles_left%
      case 5
        %echo% ~%self% raises ^%self% staff high and slams it down!
        %load% mob %prism_vnum% ally
        set summon %self.room.people%
        %echo% ~%summon% appears above |%self% shoulder with a flash of light!
      break
      case 4
        %echo% ~%self% chants, 'Sun meets rain, illuminates the air...'
      break
      case 3
        %echo% ~%self% chants, 'Thus is shown your countenance fair...'
      break
      case 2
        %echo% ~%self% chants, 'Iris of the rainbow, I implore thee...'
      break
      case 1
        %echo% ~%self% chants, 'Annihilate all who stand before me!'
      break
      case 0
        shout Victory is mine! Rainbow Beam!
        %echo% &&rThere is an all-consuming flash of chromatic radiance, then everything goes black.
        %aoe% 9999 magical
        %purge% %prism%
      break
    done
    if %cycles_left% > 0
      wait 5 sec
    end
    eval cycles_left %cycles_left% - 1
  done
end
~
#11848
Bleak Rojjer: Mercenary assassin boss combat~
0 k 100
~
if %self.cooldown(11800)%
  halt
end
* random move
set type %random.4%
if %type% == 1
  nop %self.set_cooldown(11800, 30)%
  %send% %actor% &&r~%self% stabs you with a small knife held in ^%self% free hand!
  %echoaround% %actor% ~%self% stabs ~%actor% with a small knife held in ^%self% free hand!
  %send% %actor% The wound burns! The knife must have been poisoned...
  %echoaround% %actor% ~%actor% looks seriously ill!
  %damage% %actor% 100 physical
  %dot% #11858 %actor% 500 15 poison
elseif %type% == 2
  * Shadow Strike
  nop %self.set_cooldown(11800, 30)%
  set loops 1
  if %self.mob_flagged(GROUP)%
    set loops 3
  end
  while %loops%
    set target %random.enemy%
    if !%target%
      set target %actor%
    end
    %echo% ~%self% steps into the shadows and vanishes...
    %send% %target% &&r~%self% suddenly appears behind you and stabs you in the back!
    %echoaround% %target% &&r~%self% suddenly appears behind ~%target% and stabs *%target% in the back!
    %damage% %target% 150 physical
    dg_affect #11818 %target% TO-HIT -15 10
    dg_affect #11818 %target% DODGE -15 10
    if %loops%
      wait 3 sec
    end
    eval loops %loops% - 1
  done
  if %random.10% == 10
    wait 3 sec
    say I learned that one from a goblin.
  end
elseif %type% == 3
  * Complete Darkness
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% closes ^%self% eyes and every light in the room dims...
  wait 5 sec
  %echo% |%self% eyes snap open, and you are suddenly plunged into complete darkness!
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      dg_affect #11860 %person% BLIND on 25
    end
    set person %person.next_in_room%
  done
elseif %type% == 4 && %self.mob_flagged(GROUP)%
  * Pin the Shadow
  nop %self.set_cooldown(11800, 30)%
  wait 1 sec
  %echo% ~%self% sheathes ^%self% weapon and draws a dagger made of shadows from the folds of ^%self% cloak...
  dg_affect #11845 %self% HARD-STUNNED on 10
  wait 3 sec
  set actor %self.fighting%
  %send% %actor% ~%self% throws the shadow dagger at you, but it passes through you and strikes your shadow!
  %echoaround% %actor% ~%self% throws the shadow dagger at ~%actor%, but it passes through *%actor% and strikes ^%actor% shadow!
  %send% %actor% You can't move!
  %echoaround% %actor% ~%actor% suddenly freezes!
  %load% obj 11862 room
  dg_affect #11861 %actor% HARD-STUNNED on 25
  dg_affect #11861 %actor% DODGE -999 25
  dg_affect #11861 %actor% BLOCK -999 25
  wait 1 sec
  %echo% ~%self% draws ^%self% weapon.
  dg_affect #11845 %self% off
  wait 20 sec
  set dagger %self.room.contents(11862)%
  if %dagger%
    %echo% %dagger.shortdesc% dissolves into nothing!
    %purge% %dagger%
    %send% %actor% You can move again!
    %echoaround% %actor% ~%actor% starts moving again!
    * Extra cooldown time if the attack timed out
    nop %self.set_cooldown(11800, 30)%
  end
  dg_affect #11861 %actor% off
end
~
#11849
Trixton Vye: Mercenary leader combat~
0 k 100
~
* General cooldown
if %self.cooldown(11800)%
  halt
end
* random move
set type %random.4%
if %type% == 1
  * Hurricane Slash
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% thrusts ^%self% sword into the air and a gust of wind howls through the room!
  wait 3 sec
  %echo% ~%self% starts spinning in a circle, forming a miniature tornado with *%self%self at the base!
  wait 3 sec
  set loops 3
  while %loops%
    %echo% &&r~%self% spins furiously, battering you with gusts of wind!
    %aoe% 100 physical
    if %loops%
      wait 3 sec
    end
    eval loops %loops% - 1
  done
  %echo% ~%self% stops spinning.
elseif %type% == 2
  * Awesome Blow
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% lets out a mighty shout as &%self% gathers all of ^%self% strength!
  wait 3 sec
  %send% %actor% &&r~%self% strikes you with such force that you go flying through the air!
  %echoaround% %actor% ~%self% strikes ~%actor% with such force that &%actor% goes flying through the air!
  %damage% %actor% 200 physical
  dg_affect #11814 %actor% HARD-STUNNED on 10
  wait 2 sec
  %send% %actor% &&rYou crash painfully to the floor, stunned!
  %echoaround% %actor% ~%actor% crashes to the floor, stunned!
  %damage% %actor% 100 physical
elseif %type% == 3
  * Winged Boots
  nop %self.set_cooldown(11800, 30)%
  %echo% ~%self% crouches down, and feathered wings spring from ^%self% boots!
  wait 3 sec
  %echo% ~%self% leaps into the air and begins weaving gracefully around your attacks!
  dg_affect #11865 %self% HASTE on 30
  dg_affect #11865 %self% DODGE 50 30
elseif %type% == 4
  * Magic Sword
  nop %self.set_cooldown(11800, 30)%
  * Apply or refresh Counterspell affect
  if %self.affect(3021)%
    dg_affect #3021 %self% off
  else
    %echo% ~%self% flourishes ^%self% sword and flickers momentarily with a blue-white aura.
  end
  dg_affect #3021 %self% RESIST-MAGICAL 1 35
  wait 1 sec
  * Get and increment the variable that tracks magic sword stacks
  if %self.varexists(magic_sword_stacks)%
    set magic_sword_stacks %self.magic_sword_stacks%
  else
    set magic_sword_stacks 0
  end
  eval magic_sword_stacks %magic_sword_stacks% + 1
  remote magic_sword_stacks %self.id%
  * Apply the magic sword buffs
  if !%self.affect(11866)%
    %echo% ~%self% thrusts ^%self% sword into the air, and it shines brightly!
  else
    dg_affect #11866 %self% off
    %echo% ~%self% seems to be getting stronger the longer you fight *%self%!
  end
  eval magnitude %magic_sword_stacks% * 30
  dg_affect #11866 %self% BONUS-PHYSICAL %magnitude% -1
  if %magic_sword_stacks% >= 3
    if %magic_sword_stacks == 3
      %echo% |%self% attacks begin to strike with deadly accuracy!
    end
    eval magnitude (%magic_sword_stacks% - 2) * 15
    dg_affect #11866 %self% TO-HIT %magnitude% -1
  end
end
~
#11850
Trixton Vye: Mercenary leader magic sword reset~
0 bw 100
~
if !%self.fighting%
  if %self.affect(11866)%
    dg_affect #11866 %self% off
    %echo% |%self% sword stops glowing.
    rdelete magic_sword_stacks %self.id%
  end
end
~
#11857
Skycleave: Mercenary name setup~
0 n 100
~
* Mercenaries are spawned by trig 11900/skymerc, called in 5 rooms.
* Up to 3 mercenaries spawn in each room, for a total of 15.
* Variable setup by merc vnum: Note that lookdescs are below in another switch
switch %self.vnum%
  case 11841
    * rogue mercenary
    set name_list Mac  Iniko Trixie Eleanor Shada  Foster Gatlin Brielle Cassina Sage
    set sex_list  male male  female female  female male   male   female  male    female
    set name_size 10
    set verbiage is skulking around.
  break
  case 11842
    * caster mercenary
    set name_list Eris   Borak Shabina Sophia Magnus Regin Calista Beatrice Mirabel Booker
    set sex_list  female male  female  female male   male  female  female   female  male
    set name_size 10
    set verbiage is studying the books.
  break
  case 11843
    * archer mercenary
    set name_list Alvin Gunnar Rebel  Jack Kerenza Florian Maude  Dakarai Huntley Winnow
    set sex_list  male  male   female male female  male    female male    male    female
    set name_size 10
    set verbiage is fletching an arrow.
  break
  case 11844
    * nature mage mercenary
    set name_list Aella  Oriana Faron Aiden Blaise Briar Ariadne Aislinn Daphne Ellis Bruin
    set sex_list  female female male  male  female male  female  female  female male  male
    set name_size 11
    set verbiage is playing with fire.
  break
  case 11845
    * vampire mercenary
    set name_list Osman Draco Raven  Jareth Calypso Faye   Reina  Lucia  Sebastian Ledger
    set sex_list  male  male  female male   female  female female female male      male
    set name_size 10
    set verbiage watches you carefully.
  break
  case 11846
    * armored mercenary
    set name_list Humphrey George Roxie  Edward Gellert Titania Mabel  Wolf Saskia Ansel
    set sex_list  male     male   female male   male    female  female male female male
    set name_size 10
    set verbiage is spoiling for a fight.
  break
done
* pull count data (ensures sequential naming)
set spirit %instance.mob(11900)%
set varname merc%self.vnum%
if %spirit.varexists(%varname%)%
  eval %varname% %%spirit.%varname%%% + 1
  eval pos %%%varname%%%
else
  set %varname% 1
  set pos 1
end
if %pos% > %name_size%
  set %varname% 1
  set pos 1
end
remote %varname% %spirit.id%
* choose name at random
set name Anonymo
set sex male
while %name_list% && %pos% > 0
  set name %name_list.car%
  set name_list %name_list.cdr%
  if %sex_list%
    set sex %sex_list.car%
    set sex_list %sex_list.cdr%
  end
  eval pos %pos% - 1
done
* variable setup
%mod% %self% keywords %name% %self.alias%
%mod% %self% shortdesc %name%
%mod% %self% longdesc %name% %verbiage%
%mod% %self% sex %sex%
* look descs are done in a separate switch as they require the mods be done
switch %self.vnum%
  * NOTE: be wary of the 255-characters-per-line limit on the lookdescs
  case 11841
    * rogue mercenary
    %mod% %self% lookdesc %self.name% is standing in a shadowy corner, watching you intently. You can't get a good look at %self.himher%, but it looks like %self.heshe% is holding something sharp.
  break
  case 11842
    * caster mercenary
    %mod% %self% lookdesc %self.name% is flipping through books, stopping only occasionally when %self.heshe% finds something of interest. Although %self.heshe% isn't dressed like the Skycleavers
    %mod% %self% append-lookdesc downstairs, %self.heshe% seems quite familiar with this sort of magic. A rather large coinpurse hangs from %self.hisher% belt.
  break
  case 11843
    * archer mercenary
    %mod% %self% lookdesc %self.name% is equipped with a viciously well-made bow, currently sitting to %self.hisher% side while %self.heshe% fletches arrows. Though %self.heshe% isn't exactly
    %mod% %self% append-lookdesc well-dressed, %self.heshe% bears a large, heavy coin purse on %self.hisher% belt.
  break
  case 11844
    * nature mage mercenary
    %mod% %self% lookdesc %self.name% rolls a little ball of fire back and forth over %self.hisher% hands, occasionally singeing the cuffs of %self.hisher% robe. The robe, brown and rough,
    %mod% %self% append-lookdesc conceals whatever %self.heshe% might be carrying underneath.
  break
  case 11845
    * vampire mercenary
    %mod% %self% lookdesc %self.name% is dressed all in black, in stark contrast to %self.hisher% deathly-pale face. Though %self.hisher% clothes look expensive, they're in poor condition,
    %mod% %self% append-lookdesc with frayed edges and holes. Strange, considering the size of the coin purse %self.heshe%'s carrying.
  break
  case 11846
    * armored mercenary
    %mod% %self% lookdesc %self.name% wears a well-polished imperium breastplate over a padded shirt with leather pants. It seems %self.heshe% showed up ready for a fight.
  break
done
* and detach
detach 11857 %self.id%
~
#11860
Skycleave: Upper floor one-time intros~
0 abc 100
start_messages~
* plays a boss intro until it runs out of lines, then detaches
* ensure not fighting or detach
if %self.fighting% || %self.disabled%
  detach 11860 %self.id%
  halt
end
* fetch position
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
* store line back
remote line %self.id%
set done 0
* switch: message cases goes +1 per 13 seconds and automatically ends when it hits default
if %self.vnum% == 11830
  * High Master Caius Sirensbane 3A
  switch %line%
    case 1
      %echo% ~%self% struggles to hold up the magical ward...
      wait 9 sec
      say I can't hold this much longer.
      wait 9 sec
      say Some sort of shadow keeps attacking me from over the railing.
    break
    case 2
      say We have no chance of retaking this floor without defeating that scoundrel Trixton Vye.
      wait 9 sec
      if %instance.mob(11847)% || %instance.mob(11848)%
        say He's being protected by magic from his allies, Lady Virduke and Bleak Rojjer.
      else
        say He's unprotected; you need to get to him in the Lich Labs and finish this.
      end
    break
    default
      set done 1
    break
  done
elseif %self.vnum% == 11869
  * Shade of Mezvienne
  switch %line%
    case 1
      %echo% The shadows gather over the wounded Grand High Sorcerer...
      wait 6 sec
      %echo% The shadow takes on the human face of Mezvienne the Enchantress!
    break
    case 2
      %echo% Grand High Sorcerer Knezz's own shadow whips up into the air as the Shade of Mezvienne drinks it in.
      wait 9 sec
      say What secrets lie in this withered old sack?
      wait 9 sec
      say Let's find out together. Open your mind, Jozef, let us plumb its depths... this should only take a minute.
    break
    default
      set done 1
    break
  done
elseif %self.vnum% == 11863
  * Shadow Ascendant
  switch %line%
    case 1
      %echo% Raw mana streams from every object in the room into the Shadow Ascendant.
      wait 9 sec
      %echo% Low chanting builds from the shadows as the Ascendant grows in power.
      wait 9 sec
      %echo% You feel your own power being drained by the Shadow Ascendant as it absorbs everything in sight.
    break
    default
      set done 1
    break
  done
elseif %self.vnum% == 11892
  * High Master Caius Sirensbane 3B
  switch %line%
    case 1
      %echo% ~%self% struggles to hold up the magical ward...
      wait 9 sec
      say This is taking too much power... please tell me you're ready to take on the shadow.
      wait 9 sec
      say I'll drop the ward as soon as you're ready.
    break
    default
      set done 1
    break
  done
end
* detach if done
if %done%
  if %self.aff_flagged(!ATTACK)%
    dg_affect %self% !ATTACK off
  end
  detach 11860 %self.id%
end
~
#11861
Skycleave: Barrosh mind-control struggle scene~
0 b 100
~
if %self.fighting% || %self.disabled%
  halt
end
* fetch position
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
* store line back
remote line %self.id%
* just echo in order (resets on default)
switch %line%
  case 1
    say Noooooo!
    %at% i11862 %echo% You hear shouting from the office to the east.
    %at% i11867 %echo% You hear shouting from the office.
  break
  case 2
    %echo% High Sorcerer Barrosh clasps at his head and winces in pain.
  break
  case 3
    %echo% A vase shatters as Barrosh throws it against the wall as hard as he can!
    %at% i11862 %echo% You hear the sound of something shattering against the wall from the office to the east.
    %at% i11867 %echo% You hear the sound of something shattering against the wall out in the office.
  break
  case 4
    say I... will... burn you all down!
    %at% i11862 %echo% You hear shouting from the office to the east.
    %at% i11867 %echo% You hear shouting from the office.
    wait 6 sec
    %aggro%
  break
  case 5
    %echo% High Sorcerer Barrosh throws a book hard at you, but it misses and goes out the door.
    %at% i11862 %echo% A book comes flying out the office door and sails over the railing.
  break
  case 6
    say My mind... is not my own.
    wait 6 sec
    %aggro%
  break
  case 7
    say Flee while you can! I can't hold it off.
    wait 6 sec
    %aggro%
  break
  case 8
    %echo% High Sorcerer Barrosh grasps his staff and tries to support himself as he doubles over in agony!
  break
  default
    set line 0
    remote line %self.id%
  break
done
~
#11862
Bleak Rojjer: Get shadow dagger / unpin ally~
1 g 100
~
set person %self.room.people%
while %person%
  if %person.affect(11861)%
    set target %person%
  end
done
if !%target%
  %send% %actor% You pull %self.shortdesc% out of the ground, but it dissolves in your hand.
  %echoaround% %actor% ~%actor% pulls %self.shortdesc% out of the ground, but it dissolves in ^%actor% hand.
  %purge% %self%
  halt
else
  %send% %actor% You pull %self.shortdesc% out of |%target% shadow! ~%target% starts moving again!
  %send% %target% ~%actor% pulls %self.shortdesc% out of your's shadow! You can move again!
  %echoneither% %actor% %target% ~%actor% pulls %self.shortdesc% out of |%target% shadow! ~%target% starts moving again!
  dg_affect #11861 %target% off
  %echo% %self.shortdesc% dissolves into nothing.
  %purge% %self%
  halt
end
~
#11863
Skycleave: Shade ascension / Death of Knezz~
0 b 100
~
* This starts a timer that will kill Knezz and ascend the shade after 2 minutes
set room %self.room%
if %room.people(11868)%
  * fetch timer
  if %self.varexists(knezz_timer)%
    set knezz_timer %self.knezz_timer%
  else
    set knezz_timer 0
  end
  * check startup
  if %knezz_timer% == 0 && !%self.fighting%
    * not started
    halt
  end
  if %knezz_timer% == 0 && %self.fighting%
    set knezz_timer %timestamp%
    remote knezz_timer %self.id%
    halt
  end
  * the rest of this is based on time since start
  eval seconds %timestamp% - %knezz_timer%
else
  * no knezz at all
  set seconds 99999
end
if %seconds% > 60
  * 1 minute: Knezz dies
  nop %self.add_mob_flag(NO-ATTACK)%
  %restore% %self%
  * stun everyone
  set room %self.room%
  set ch %room.people%
  while %ch%
    if %ch% != %self%
      dg_affect %ch% HARD-STUNNED on 12
    end
    set ch %ch.next_in_room%
  done
  * messaging
  %echo% A blast of dark energy throws you backwards as a shadow claws its way out of Knezz's mouth and joins the Shade above him.
  wait 6 sec
  %echo% The Shade of Mezvienne drops Knezz's lifeless body in the chair and snatches his nocturnium wand.
  set knezz %room.people(11868)%
  if %knezz%
    %slay% %knezz%
  end
  wait 6 sec
  %echo% The Shade holds the wand high in the air and shouts, 'By the power of Skycleave... I HAVE THE POWER!'
  wait 3 sec
  %echo% The Shade of Mezvienne grows and transforms as it ascends into a higher being!
  %load% mob 11863
  set mob %room.people%
  if %mob.vnum% == 11863
    if %self.mob_flagged(HARD)%
      nop %mob.add_mob_flag(HARD)%
    end
    if %self.mob_flagged(GROUP)%
      nop %mob.add_mob_flag(GROUP)%
    end
    %scale% %mob% %self.level%
    if %self.fighting%
      %force% %mob% %aggro% %self.fighting%
    else
      %force% %mob% %aggro%
    end
  end
  * reskin room
  %mod% %room% description An inky shadow covers most of the room such that you can't see the walls or ceiling. It pours over the furniture and creeps along the floor. Only the area
  %mod% %room% append-description around your feet is free of it. The shadow seems to be alive... and worse, it appears to be drawing more power from Skycleave by the second.
  * and purge self
  %purge% %self%
elseif %seconds% > 90
  %echo% Grand High Sorcerer Knezz goes limp as darkness begins to seep from his mouth and eyes.
  dg_affect %self% BONUS-MAGICAL 5 -1
elseif %seconds% > 60
  %echo% The Shade of Mezvienne grows to fill the room as Knezz grows paler and paler.
  dg_affect %self% BONUS-MAGICAL 5 -1
elseif %seconds% > 30
  %echo% The Shade of Mezvienne grows as it draws Knezz's life force.
  dg_affect %self% BONUS-MAGICAL 5 -1
end
~
#11864
Skycleave: Barrosh post-fight cutscene~
0 n 100
~
wait 0 sec
%echo% Time moves backwards for a second as High Sorcerer Barrosh composes himself and holds his staff to the sky...
wait 9 sec
say For the honor of Skycleave!
wait 4 sec
%echo% Glass flies everywhere as a bolt of lightning streaks through the window, momentarily blinding you as it strikes the gem on Barrosh's staff!
wait 9 sec
say By the power invested in me by the Tower Skycleave, I cast out the shadow!
wait 9 sec
%echo% Barrosh's eyes turn pitch black and he drops to his knees and screams as he throws his head back...
wait 9 sec
%echo% A thick, smoky shadow streams from Barrosh's eyes with a coarse, sputtering sound...
wait 9 sec
%echo% The shadow is caught by the light of Barrosh's staff and it evaporates with an agonizing hiss!
wait 9 sec
say There. It is done. My mind is free. I hate to think of what horrors I committed in that state.
wait 9 sec
if %instance.mob(11868)%
  say You have got to free the Grand High Sorcerer. He's the only one who will be able to stop all this.
else
  say You have got to find a way to save the tower. I'm afraid I will not be any further use.
end
~
#11865
Skycleave: Knezz phase A post-fight cutscene~
0 bw 100
~
* messages starting shortly after load
* fetch position
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
* store line back
remote line %self.id%
* this goes +1 per 13 seconds and automatically ends when it hits default
switch %line%
  case 1
    say Ah... that was perilously close. Give me a moment to collect myself. She took a lot out of me.
    wait 9 sec
    %echo% Knezz draws an elegant nocturnium wand from the sleeve of his robe and turns it toward himself.
    wait 1
    say On my authority, I cast out the shadow!
    wait 8 sec
    %echo% Knezz taps his wand against the arm of the chair a few times.
  break
  case 2
    say By the power of Skycleave, I cast out the shadow!
    wait 9 sec
    %echo% Knezz bangs his wand against the arm of his chair.
    wait 9 sec
    say By the power... oh, well, blast it all, I'll do it later.
  break
  case 3
    say Oh, you're still here... good.
    wait 9 sec
    say You need to get to the top of the tower and burn the heartwood.
    wait 9 sec
    say It's the only thing that will free us now.
  break
  case 4
    say With the heartwood burned, mighty Skycleave should finally be free of that wretched time loop.
    wait 9 sec
    say I dare not guess how many times we've repeated this horrid week.
    wait 9 sec
    say To think she was here on my personal invitation. I do hope her mercenaries haven't done too much damage downstairs.
  break
  case 5
    * HE MOVES WEST TO 11869 HERE
    say With apologies, I must retire to my quarters and get some rest now.
    wait 9 sec
    west
    * ensure right spot
    if %self.room.template% != 11869
      %echo% ~%self% leaves.
      mgoto i11869
      %echo% ~%self% walks in from the east.
    end
    wait 9 sec
    %echo% Knezz sits down to rest in his easy chair.
  break
  case 6
    say I just need to sit for a spell. There will be time for stories later. Go burn that heartwood.
  break
  case 7
    say What are you waiting for? You need to burn the heartwood before the time loop repeats again.
  break
  default
    * done -- just remove script
    detach 11865 %self.id%
  break
done
~
#11866
Skycleave: Skip command (for cutscenes)~
0 c 0
skip~
if !%self.varexists(skip)%
  %send% %actor% You skip the cutscene.
  %echoaround% %actor% ~%actor% has skipped the cutscene.
  set skip 1
  remote skip %self.id%
else
  %send% %actor% Someone has already skipped the cutscene (it may still play another message or two before skipping).
end
~
#11867
Skycleave: Boss room relocator~
0 hn 100
~
set no_purge_list 11871 11872
* Teleports people/items out of a room that's restricted while the mob is there
if %actor.nohassle%
  halt
end
wait 0
set room %self.room%
set to_vnum 0
set message 0
set move_objs 1
* 'to_vnum' must be set by this switch based on the from-vnum
switch %room.template%
  case 11871
    set to_vnum 11870
    set message The swirling vortex tosses you back down the stairs!
    set move_objs 0
  break
  case 11872
    set spirit %instance.mob(11900)%
    if %spirit.phase4%
      set to_vnum 11971
    else
      set to_vnum 11871
    end
    set message The time stream collapses and you're thrown back to the tower!
  break
  default
    * unknown room: nothing to do
    %purge% %self%
    halt
  break
done
* Ensure part of the instance
nop %self.link_instance%
set to_room %instance.nearest_rmt(%to_vnum%)%
if !%to_room%
  * No destination
  %purge% %self%
  halt
end
* echo now
if %message%
  %echo% %message%
end
* Check items here
if %move_objs%
  set obj %room.contents%
  while %obj%
    set next_obj %obj.next_in_list%
    if %obj.can_wear(TAKE)%
      %teleport% %obj% %to_room%
    end
    set obj %next_obj%
  done
end
* Check people here
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.nohassle% || (%ch.vnum% >= 11890 && %ch.vnum% <= 11899)
    * nothing (11890-11899 are mobs involved in phase change)
  elseif %ch.is_pc% || %ch.vnum% < 11800 || %ch.vnum% > 11999 || %ch.aff_flagged(*CHARM)%
    * Move ch
    %teleport% %ch% %to_room%
    %load% obj 11805 %ch%
  elseif %ch% != %self% && !(%no_purge_list% ~= %ch.vnum%)
    * Adventure mob: purge, usually
    %purge% %ch%
  end
  set ch %next_ch%
done
~
#11868
Skycleave: Skithe Ler-Wyn reset trigger~
0 ab 100
~
* this resets the whole fight
if %self.fighting% || %self.disabled% || %self.health% <= 0 || %self.aff_flagged(!ATTACK)%
  halt
end
* ensure nobody is fighting me
set room %self.room%
set ch %room.people%
while %ch%
  if %ch.fighting% == %self%
    halt
  end
  set ch %ch.next_in_room%
done
* if we got to here, nobody is fighting us, start a countdown...
wait 30 sec
if %self.fighting% || %self.disabled% || %self.health% <= 0
  halt
end
* reset: remove the relocator vortex
set vortex %instance.mob(11865)%
if %vortex%
  %at% i11871 %echo% The swirling time vortex on top of the tower dissipates.
  %purge% %vortex%
end
* reset: remove move-blocking vortex
makeuid hall room i11870
while %hall.contents(11870)%
  %purge% %hall.contents(11870)%
done
%at% %hall% %echo% The swirling time vortex on top of the tower dissipates.
* ensure no lingering Mezvienne
set mez %instance.mob(11866)%
if %mez%
  %purge% %mez%
end
* put Mezvienne back
%at% i11871 %load% mob 11871
set mez %instance.mob(11871)%
if %mez%
  set spirit %instance.mob(11900)%
  if (%spirit.diff4% // 2) == 0
    nop %mez.add_mob_flag(HARD)%
  end
  if %spirit.diff4% >= 3
    nop %mez.add_mob_flag(GROUP)%
  end
end
* place a relocator
%load% mob 11899
* lastly, remove me (will be reloaded by the boss)
%purge% %self%
~
#11869
Skycleave: Skithe Ler-Wyn load trigger~
0 n 100
~
wait 0
set room %self.room%
dg_affect %self% !ATTACK on -1
if !%self.varexists(skip)%
  wait 9 sec
  %echo% Skithe Ler-Wyn, the Lion of Time, lets out a fearsome roar and lands in front of you just as you finally find your footing.
  wait 9 sec
end
* find highest visit count and raise visit counts
set ch %room.people%
set highest 1
while %ch%
  if %ch.is_pc%
    if %ch.varexists(skithe_visits)%
      set skithe_visits %ch.skithe_visits%
    else
      set skithe_visits 0
    end
    if %skithe_visits% > %highest%
      set highest %skithe_visits%
    end
    * only raise visits by 1 per instance
    if !%room.varexists(visited_%ch.id%)%
      eval skithe_visits %skithe_visits% + 1
      remote skithe_visits %ch.id%
      set visited_%ch.id% 1
      remote visited_%ch.id% %room.id%
    end
  end
  set ch %ch.next_in_room%
done
* message based on highest visit count
if %highest% >= 175
  set message Just as you have tried hundreds of times before, so too will you fail again...
elseif %highest% >= 125
  set message You will fail again here, as you have failed more than a hundred times before...
elseif %highest% >= 100
  set message You have tried a hundred times, and a hundred times you have failed...
elseif %highest% >= 75
  set message So many attempts to stop me, and yet here we are again...
elseif %highest% >= 50
  set message How many times will do this? Fifty? A hundred? A thousand? It is not within your power or purview to stop me...
elseif %highest% >= 25
  set message Again? Do you not grow tired of this dance? I have all of time. You have what, another twenty years? Don't dare think it's longer...
elseif %highest% >= 15
  set message You again? You could do this another dozen times or a hundred; nothing you have done here will matter. Surely you know it is futile...
elseif %highest% >= 10
  set message You again? Did we not settle this matter already?
elseif %highest% >= 5
  set message Your persistence is admirable, if misguided. You lack the power to stop any of this...
elseif %highest% >= 2
  set message Back again so soon? Pity, I thought you had learned a lesson...
else
  set message Ah, fresh blood has poured itself into my time stream. Have you come here to be a vessel or merely a meal?
end
%echo% Skithe Ler-Wyn, the Lion of Time, says, '%message%'
if %self.varexists(skip)%
  wait 1
else
  wait 6 sec
end
%echo% Skithe Ler-Wyn, the Lion of Time, says, 'No power you wield can stop me from digesting this tower and everything in it for all time!'
dg_affect %self% !ATTACK off
* and wait before attacking
wait 9 sec
if !%self.fighting%
  %aggro%
end
~
#11870
Skycleave: Mezvienne phase transition cutscene~
0 ab 100
~
* messaging following this mob being loaded and the players being teleported to her room
* fetch position
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
* check skip
if %self.varexists(skip)%
  set line 999
end
* store line back
remote line %self.id%
* this goes +1 per 13 seconds and automatically ends when it hits default
switch %line%
  case 1
    %echo% You struggle to find footing in the time stream as all the trappings of your world fade beyond the vortex.
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% ~%self% continues to scream as she pulls at her long hair, which is rapidly turning white again...
    %mod% %self% longdesc Mezvienne the Enchantress pulls at her hair as she struggles in the vortex!
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% ~%self% screams, 'Why?! My master, my lion, my Skithe... was I not faithful?'
  break
  case 2
    %echo% ~%self% lets out a blood-curdling screech, arches her back, and throws her head backwards as white light shoots from her eyes!
    %mod% %self% longdesc Mezvienne the Enchantress twists in the air as white light streams from her eyes!
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% ~%self% shouts, 'No! No, I did everything you asked!'
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% ~%self% wraps her scrawny, sagging arms around her body and claws at her own back as she screams...
    %mod% %self% longdesc Mezvienne the Enchantress claws at her own back as she screams in agony!
  break
  case 3
    %echo% ~%self% curls up into a ball and lets out the loudest, most anguished scream yet as a red mist spurts from her back...
    %mod% %self% longdesc Mezvienne the Enchantress is curled in a ball in the air, screaming!
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% A disembodied voice cuts through the roaring vortex, saying, 'Little lamb, you have indeed served your purpose.'
    wait 9 sec
    if %self.varexists(skip)%
      halt
    end
    %echo% The voice continues, 'You were the most magnificent vessel and you plucked out your part like an enchanted harp.'
  break
  default
    * final step: message first
    if !%self.varexists(skip)%
      %echo% The disembodied voice grows louder, saying, 'But these interlopers have cut your strings short and so, here, your song ends.'
      wait 9 sec
    end
    %echo% ~%self% lets out one final truncated scream as her back bursts open and a gigantic, glimmering lion claws its way out!
    * and move on to the next phase
    %load% mob 11872
    set mob %self.room.people%
    if %mob.vnum% == 11872
      if %self.varexists(skip)%
        set skip 1
        remote skip %mob.id%
      end
      set spirit %instance.mob(11900)%
      if (%spirit.diff4% // 2) == 0
        nop %mob.add_mob_flag(HARD)%
      end
      if %spirit.diff4% >= 3
        nop %mob.add_mob_flag(GROUP)%
      end
      %restore% %mob%
    end
    %purge% %self%
    halt
  break
done
~
#11871
Skycleave: Mezvienne starts phase transition (hitprc)~
0 l 30
~
set room %self.room%
* messaging
%echo% \&0
%echo% ~%self% screams and flies up into the air as the whole tower is engulfed in an enormous vortex!
* remove relocator if present
makeuid vortex room i11872
set mob %vortex.people(11899)%
while %mob%
  %purge% %mob%
  set mob %vortex.people(11899)%
done
* load 2nd version
%at% %vortex% %load% mob 11866
* relocate and stun everyone
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch% != %self%
    dg_affect %ch% HARD-STUNNED on 5
    %teleport% %ch% %vortex%
  end
  set ch %next_ch%
done
* load vortex blocker
%at% i11870 %load% obj 11870
* load relocator here
%load% mob 11865
* done
%purge% %self%
~
#11872
Skycleave: Mezvienne starts phase transition (death)~
0 f 100
~
set room %self.room%
* messaging
%echo% \&0
%echo% ~%self% screams and flies up into the air as the whole tower is engulfed in an enormous vortex!
* remove relocator if present
makeuid vortex room i11872
set mob %vortex.people(11899)%
while %mob%
  %purge% %mob%
  set mob %vortex.people(11899)%
done
* load 2nd version
%at% %vortex% %load% mob 11866
* relocate and stun everyone
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch% != %self%
    dg_affect %ch% HARD-STUNNED on 5
    %teleport% %ch% %vortex%
  end
  set ch %next_ch%
done
* load vortex blocker
%at% i11870 %load% obj 11870
* load relocator here
%load% mob 11865
* block death cry
return 0
~
#11873
Skithe Ler-Wyn death~
0 f 100
~
* message
%echo% Skithe Ler-Wyn, Lion of Time, dissolves into the air above you with one final roar!
makeuid tower room i11871
* remove the relocator vortex
set vortex %instance.mob(11865)%
if %vortex%
  %at% %tower% %echo% The swirling time vortex on top of the tower dissipates.
  %purge% %vortex%
end
* remove move-blocking vortex
makeuid hall room i11870
while %hall.contents(11870)%
  %purge% %hall.contents(11870)%
done
%at% %hall% %echo% The swirling time vortex on top of the tower dissipates.
* place a relocator here
%load% mob 11899
* move self
mgoto %tower%
* prevent death cry
return 0
~
#11874
Smol Nes-Pik: Torru and Nayyur~
0 bw 100
~
* controls both rot worshippers in the room; runs on 11886 Torru
* this is one big cycle of text and then repeats
* first find 11887 Nayyur or load him if he's somehow missing
set nayyur %instance.mob(11887)%
if !%nayyur%
  load mob 11887
  set nayyur %self.room.people%
elseif %nayyur.room% != %self.room%
  %at% %nayyur.room% %echo% ~%nayyur% leaves.
  %teleport% %nayyur% %self.room%
  %echo% ~%nayyur% arrives.
  wait 1
end
* fetch cycle
if %self.varexists(cycle)%
  eval cycle %self.cycle% + 1
else
  set cycle 1
end
* force a restart if the sap is gone
if !%self.room.contents(11888)% && %cycle% > 4
  set cycle 1
end
* check skip?
if %self.varexists(skip)%
  rdelete skip %self.id%
  set cycle 4
  set skip 1
end
* main messages: must be sequential starting with 1 or it will just loop back to 1
switch %cycle%
  case 1
    say Iskip wennur, faskoh vadur, Iskip sal-ash Tagra Nes.
    wait 9 sec
    say Iskip sal-ash Tagra Nes...
    wait 1
    %force% %nayyur% say Iskip sal-ash Tagra Nes.
    wait 9 sec
    say Foroh undune aspeh a wo owri...
  break
  case 2
    say Smol Nes-Pik pos ne wo owri...
    wait 9 sec
    %force% %nayyur% say Iskip sal-ash Tagra Nes.
    wait 9 sec
    say Fras kee selmo tagra tyewa...
    wait 1 sec
    %echo% Thousands of tiny splinters of rotted wood rise from the trunk and hover in the air...
  break
  case 3
    say Fras kee selmo tagra tyewa...
    wait 1 sec
    %echo% More and more splinters rise into the air until you can scarcely breath!
    wait 8 sec
    say Badur eydur a wo owri...
    wait 1 sec
    %force% %nayyur% say Iskip sal-ash Tagra Nes.
    wait 7 sec
    say Ickor carzo a kee selmo, vadur, eydur Tagra Nes!
    wait 1 sec
    %echo% ~%self% stabs the rotten tree with a flint knife as the splinters of wood fall out of the air...
  break
  case 4
    if !%skip%
      say Ickor carzo a wo owri...
      wait 1 sec
      %force% %nayyur% say Iskip sal-ash Tagra Nes.
      say Iskip sal-ash Tagra Nes.
      wait 2 sec
    end
    %echo% Sap begins to trickle from the rotting tree.
    * load putrid sap now
    if !%self.room.contents(11888)%
      %load% obj 11888 room
    end
  break
  case 5
    %echo% ~%self% and ~%nayyur% taste the putrid sap and their eyes glaze over with a milky film.
  break
  case 6
    say Doron kasko Iskip sal-ash...
    wait 1
    %force% %nayyur% say Yozol sal-ash Tagra Nes.
  break
  case 7
    say Opol kasko Iskip sal-ash...
    wait 1
    %force% %nayyur% say Yozol sal-ash Tagra Nes.
  break
  case 8
    say Tofvon kaskon Iskip sal-ash...
    wait 1
    %force% %nayyur% say Yozol sal-ash Tagra Nes.
  break
  case 9
    say Afta kasko Iskip sal-ash...
    wait 1
    %force% %nayyur% Besta sulyo tagra irrun.
  break
  case 10
    %echo% ~%self% digs ^%self% fingernails into the rotted tree until blood trickles down ^%self% arms...
    wait 6 sec
    say Devor esh pik necafuir devor recless.
  break
  case 11
    say Nayyur beo a wo tock nefor.
    wait 1 sec
    %echo% ~%nayyur% scoops up a fistful of rotted splinters and cradles them in both hands.
  break
  case 12
    %echo% Blood from |%self% hands drips onto the rotted splinters in |%nayyur%.
    wait 3 sec
    %echo% The bloody splinters in |%nayyur% hands burst into jet-black flames, releasing choking smoke.
  break
  case 13
    wait 4 sec
    say Iskip sal-ash Tagra Nes.
    wait 1
    %echo% Smoke from |%nayyur% black flame fills the hollow and diffuses into the rotten wood.
  break
  case 14
    %force% %nayyur% say Iskip sal-ash Tagra Nes.
    wait 6 sec
    %force% %nayyur% say Iskip shallas Tagra Nes.
  break
  case 15
    wait 55 sec
  break
  default
    * just restart it
    set cycle 0
  break
done
* store current cycle
remote cycle %self.id%
~
#11875
Smol Nes-Pik: Joiago gossip helper~
0 c 0
joiago~
* gossip helper for Joiago, partner to trigger 11877
if %actor.vnum% != 11877 || %arg% != gossip
  halt
end
set mob_list 11878 11880 11881 11882 11883
set ch %self.room.people%
set partner 0
set pc_partner 0
set count 0
* find a random partner present
while %ch%
  if %ch.is_pc% && %self.can_see(%ch%)%
    set pc_partner %ch%
  elseif %mob_list% ~= %ch.vnum%
    eval count %count% + 1
    eval chance %%random.%count%%%
    if %chance% == %count% || !%partner%
      set partner %ch%
    end
  end
  set ch %ch.next_in_room%
done
if !%partner%
  set partner %pc_parnter%
end
if !%partner%
  * still no partner?
  halt
end
* gossip loop pos
if %self.varexists(gossip)%
  eval gossip %self.gossip% + 1
else
  set gossip 1
end
if %gossip% > 3
  set gossip 1
end
* what to say?
if %partner.vnum% == 11880
  * Lotte
  switch %gossip%
    case 1
      say Where did you get that garish red frock?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say Well, it's all the rage in Smol Kai-Pik, but you wouldn't know.
      end
    break
    case 2
      say How's the tree?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say Never better, than you very much.
      end
    break
    case 3
      say I heard you all have a problem with Iskippians.
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say Prune your own tree, mister.
      end
    break
  done
elseif %partner.vnum% == 11882
  * High Tresydion
  if %partner.varexists(talking)%
    %echo% ~%self% watches ~%partner% orate.
  else
    switch %gossip%
      case 1
        say Better or worse today, Tresydion?
        wait 1
        if %partner.room% == %self.room%
          %force% %partner% say If you're asking about me, I feel as right as rain.
        end
      break
      case 2
        say Better or worse, though, Tresydion?
        wait 1
        if %partner.room% == %self.room%
          %force% %partner% say The Great Tree is getting better every day. Glean the dew for yourself.
        end
      break
      case 3
        say What's the good news?
        wait 1
        if %partner.room% == %self.room%
          %force% %partner% say Don't start.
        end
      break
    done
  end
elseif %partner.vnum% == 11883
  * Black Meket
  switch %gossip%
    case 1
      say Any news on your mother, Meket?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say Not a word, friend.
      end
    break
    case 2
      say Can I bring you anything?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say No, friend, I have taken the Dew Oath.
      end
    break
    case 3
      say What have you seen in the dew?
      wait 1
      if %partner.room% == %self.room%
        %force% %partner% say All I can see is that same giant eye.
      end
    break
  done
else
  * Kym, Keeper Bastain, or the player
  switch %random.10%
    case 1
      say I heard Nayyur is worshipping the rot.
      set response 1
    break
    case 2
      say Have you noticed something... off... about the Sparo?
      set response 1
    break
    case 3
      say The tresydion says all those mushrooms and lichen are natural but they look like they're eating the tree.
      set response 2
    break
    case 4
      say Why are we out here trying to repair the glimmering stair while the smol are smoldering?
      set response 2
    break
    case 5
      say I heard the giants have been clearing whole forests.
      set response 3
    break
    case 6
      say Nayyur thinks one of the giants is the Iskip.
      set response 3
    break
    case 7
      say Someone mentioned the Great Tree is budding again.
      set response 4
    break
    case 8
      say I heard the Great Tree has a new branch.
      set response 4
    break
    case 9
      say Someone told me the whole thing is a dream.
      set response 5
    break
    case 10
      say How would you even know if you're the one dreaming us?
      set response 5
    break
  done
  wait 1
  if %partner.is_npc% && %partner.room% == %self.room%
    switch %response%
      case 1
        if %partner.vnum% == 11881
          %force% %partner% say Sounds like nunya.
        else
          %force% %partner% say I don't think that's any of my business.
        end
      break
      case 2
        if %partner.vnum% == 11881
          %force% %partner% say Tagra Nes has stood since the Dawn and you know it. All this talk of rot and ruin is a little extra.
        else
          %force% %partner% say Quit trying to stir up trouble, %self.name%.
        end
      break
      case 3
        if %partner.vnum% == 11881
          %force% %partner% say You still afraid of giants, %self.name%?
          wait 1
          if %partner.room% == %self.room%
            say Yeah, and you should be, too.
          end
        else
          %force% %partner% say That's a children's story.
        end
      break
      case 4
        if %partner.vnum% == 11881
          %force% %partner% say Not that you could see it from down here the way the glow reflects off the mist.
        else
          %force% %partner% say Who would even know? Nobody has been up that high in ages. You'd almost forget we can fly.
        end
      break
      case 5
        * Having been told the whole thing is a dream, they respond with a non-sequitur as if they heard something else.
        if %partner.vnum% == 11881
          %force% %partner% say She told me it's going to be a boy.
        else
          %force% %partner% say Really? I heard it was the mushroom soup.
        end
      break
    done
  end
  * no wait 1
  * In the seashell, check if they're interrupting the Tresydion
  if %self.room.template% == 11882
    set tresydion %instance.mob(11882)%
    if %tresydion% && %tresydion.varexists(talking)%
      %echo% The tresydion makes a shushing noise.
    end
  end
end
~
#11876
Smol Nes-Pik: Reset comment count on move~
0 i 100
~
* pairs with trigger 11880 etc to reset their commentary when they move
set comment 0
remote comment %self.id%
~
#11877
Smol Nes-Pik: Joiago gossip and mirth driver~
0 bw 50
~
* pairs with trigger 11876 and 11875
set max_comment 4
* determine position in the comment loop
if %self.varexists(comment)%
  eval comment %self.comment% + 1
else
  set comment 1
end
if %comment% > %max_comment%
  halt
end
remote comment %self.id%
* the comments don't use 'elseif' because some comments will skip to later ones
set room %self.room%
set template %room.template%
if %comment% == 1
  * 1: location-based
  if %template% == 11879
    * [11879] Atop the Glimmering Stair
    say Fancy meeting you up here. I hope you're enjoying Smol Nes-Pik. The views up here are beyond compare.
  elseif %template% == 11880
    * [11880] Grand Overlook: fall through to comment 3
    set comment 3
  elseif %template% == 11881
    * [11881] Outside the Seashell
    switch %random.3%
      case 1
        say Ah, our most famous feature...
        emote $n gestures broadly toward the seashell.
      break
      case 2
        say If you look down, you can see the palace from here.
      break
    done
  elseif %template% == 11882
    * [11882] Great Glowing Seashell: fall through to comment 2
    set comment 2
  elseif %template% == 11883
    * [11883] Outside Obsidian Palace
    say Can't recommend going below here. The stair is broken, the tree is ill, and those bleak-brained rot worshippers are praising both.
  end
end
* store the comment again, in case it changed
remote comment %self.id%
* continuing...
if %comment% == 2
  * 2. gossip with someone present
  joiago gossip
end
if %comment% == 3
  * 3. musical instrument: this one can actually change rooms
  %echo% ~%self% pulls a little gemstone flute from ^%self% pocket and begins to play...
  wait 3 sec
  * must check if the Tresydion is orating
  set tresydion %instance.mob(11882)%
  set dance_list 11878 11880 11881
  * begin musical loop
  set music %random.3%
  set loop 1
  while %loop% < 10
    if %tresydion% && %tresydion.varexists(talking)%
      * always play along if the tresydion is talking
      switch %random.3%
        case 1
          %echo% ~%self% plays a soft, reverent tune.
        break
        case 2
          %echo% ~%self% plays along to the tresydion's speech.
        break
        case 3
          %echo% ~%self% plays ^%self% flute quietly.
        break
      done
    elseif %music% == 1
      * song 1: soft, sad melody
      switch %loop%
        case 1
          %echo% ~%self% plays a soft, sad melody...
        break
        case 2
          %echo% The melancholy notes from the flute dance around the tree...
        break
        case 3
          %echo% |%self% music makes you close your eyes for a moment, and imagine yourself floating on a bed of clouds...
        break
        case 4
          %echo% The world around you seems to disappear for a moment...
        break
        case 5
          %echo% The music from |%self% flute rises, like the glimmering staircase...
        break
        case 6
          %echo% The music dissolves into dissonant notes, sinking into the haze...
        break
        case 7
          %echo% The soft, sad music wells up from below, louder, brighter, and faster, until it seems to stream back into the little flute...
        break
        case 8
          %echo% ~%self% dances backward slowly as &%self% plays...
        break
        case 9
          %echo% The tune ends with a flourish and ~%self% takes a little bow.
        break
      done
    elseif %music% == 2
      * song 2: jaunty dance
      switch %loop%
        case 1
          %echo% ~%self% starts a jaunty tune...
        break
        case 2
          %echo% ~%self% begins to dance about as &%self% plays...
        break
        case 3
          %echo% The staccato notes seem to escape the little flute and begin to dance about on their own...
        break
        case 4
          if %template% == 11881 || %template% == 11882
            %echo% The seashell pulses along to the music as the notes dance around on its curves...
          else
            %echo% The bricks and blocks light up as the notes dance around on them...
          end
        break
        case 5
          %echo% The merry melody makes your legs tingle and pull, as if they want to move...
        break
        case 6
          %echo% ~%self% twirls around, playing the flute, but the music has taken on a life beyond its instrument...
        break
        case 7
          %echo% The music notes flit around you, pulling you in circles, making everyone dance...
        break
        case 8
          %echo% ~%self% begins moving faster and faster, pursuing each escaped note and pulling it back into the flute...
        break
        case 9
          %echo% ~%self% twirls into a full bow as the music dances back into the flute at long last.
        break
      done
      * dancers
      set ch %room.people%
      while %ch%
        if %ch.is_npc% && %dance_list% ~= %ch.vnum%
          %echo% ~%ch% dances along.
        end
        set ch %ch.next_in_room%
      done
    elseif %music% == 3
      * song 3: history of the tree
      switch %loop%
        case 1
          %echo% As ~%self% begins a gentle tune on the flute, the haze seems to swirl around you...
        break
        case 2
          %echo% The flute music gyrates and ripples through the haze...
        break
        case 3
          %echo% ~%self% walks slowly backward, leading the music in a circle as it follows ^%self% flute...
        break
        case 4
          %echo% You stare at the trunk of the great tree as the music paints a picture of it young, fresh, vibrant...
        break
        case 5
          %echo% As the gentle melody is teased out of |%self% flute, you imagine the tree growing, reaching toward the sky even as a tiny staircase grows around it...
        break
        case 6
          %echo% One by one, the gentle notes from the flute drop from the branches of the tree, as mushrooms, moss, and lichen climb the stairs...
        break
        case 7
          %echo% You lean back, almost floating on the music, as you watch the tree in your mind, imagining every moment of its history...
        break
        case 8
          %echo% The decresendo builds as the notes fragment and the tree begins to fade as the trunk is choked and the staircase breaks...
        break
        case 9
          %echo% And then the music ends, suddenly, on a discordant note. ~%self% takes a little bow, and a tear escapes his eye.
        break
      done
    end
    eval loop %loop% + 1
    wait 3 sec
  done
  %echo% ~%self% slides the flute back into ^%self% pocket.
end
if %comment% == 4
  * 4. sleep hints
  switch %random.4%
    case 1
      say Think of us when you wake up, will you?
    break
    case 2
      say This may be your dream, but it was our nightmare.
    break
    case 3
      say Things weren't good before Iskip came, but at least we were free.
    break
    case 4
      say Remember to dance occasionally, when you're awake. It's no good if you only do it in dreams.
    break
  done
end
* force a delay at the end
wait 20 s
~
#11878
Smol Nes-Pik: Keeper Bastain greeting~
0 g 90
~
if %direction% == none
  halt
end
wait 1
if %actor.room% == %self.room%
  %send% %actor% ~%self% greets you.
  %echoaround% %actor% ~%self% greets ~%actor%.
  * possible response
  wait 1
  if %actor.room% == %self.room% && %random.2% == 2
    switch %actor.vnum%
      case 11877
        * Joiago
        %force% %actor% say Greetings.
      break
      case 11880
        * Lotte
        %force% %actor% say Everyone is so friendly here.
      break
      case 11881
        * Kym
        %echoaround% %actor% ~%actor% nods at ~%self%.
      break
    done
  end
end
~
#11879
Great Tree: Broken Ladder Drop~
2 g 100
~
* always returns 1
return 1
if %direction% != up
  halt
end
* detect who is already in the room
set ch %room.people%
while %ch%
  if %ch.is_pc%
    set have%ch.id% 1
  end
  set ch %ch.next_in_room%
done
* very short delay for messaging
wait 1
* now message to all new players present
set ch %room.people%
while %ch%
  if %ch.is_pc%
    eval check %%have%ch.id%%%
    if %ch% == %actor% || !%check%
      %send% %ch% The ladder breaks as you climb down and you come crashing down onto the stair!
      %send% %ch% It doesn't look like there's a way back up.
    end
  end
  set ch %ch.next_in_room%
done
~
#11880
Smol Nes-Pik: Lotte running commentary~
0 bw 45
~
* pairs with trigger 11876
set max_comment 4
* determine position in the comment loop
if %self.varexists(comment)%
  eval comment %self.comment% + 1
else
  set comment 1
end
if %comment% > %max_comment%
  halt
end
remote comment %self.id%
* comment based on location, except the first one
set room %self.room%
set template %room.template%
if %comment% == %max_comment%
  * final echo is a sleep hint
  switch %random.4%
    case 1
      %echo% Lotte turns to you and says, 'You're still asleep.'
    break
    case 2
      say Hey, wake up.
    break
    case 3
      %echo% Lotte snores.
    break
    * no case 4 -- just no hint
  done
elseif %template% == 11879
  * [11879] Atop the Glimmering Stair
  switch %comment%
    case 1
      %echo% Lotte seems to be out of breath from all the stairs.
    break
    case 2
      say Everyone here has been so friendly.
    break
    case 3
      say I don't think this tree is looking so good.
    break
  done
elseif %template% == 11880
  * [11880] Grand Overlook
  switch %comment%
    case 1
      say Oh wow, you can see the whole city from up here!
    break
    case 2
      say I don't think Tagra Kai is this tall.
    break
    case 3
      say You get a little woozy looking over that edge!
    break
  done
elseif %template% == 11881
  * [11881] Glimmering Staircase
  switch %comment%
    case 1
      say Oh, wow, the whole thing is glowing!
    break
    case 2
      say Ours isn't like this. I mean, it's not even green.
    break
    case 3
      %echo% Lotte runs her hand over the outside of the carved giant seashell.
      wait 1
      if %self.room% == %room%
        say Wait, am I allowed to touch it?
      end
    break
  done
elseif %template% == 11882
  * [11882] Great Green Seashell
  switch %comment%
    case 1
      %echo% Lotte carefully gets down onto one of the rugs and kneels until her forehead almost touches the water.
      wait 2 sec
      if %self.room% == %room%
        %echo% Lotte gets back up and brushes herself off.
      end
    break
    case 2
      say I just love the way the water looks on the ceiling.
    break
    case 3
      say You all just look so green in this light.
    break
  done
  * also check if the Tresydion is orating
  set tresydion %instance.mob(11882)%
  if %tresydion% && %tresydion.varexists(talking)%
    wait 1
    if %self.room% == %room%
      %echo% Lotte says, 'Oops, sorry,' under her voice, then shushes herself.
    end
  end
elseif %template% == 11883
  * [11883] Glimmering Staircase
  switch %comment%
    case 1
      say Bet they lose the palace in the dark a lot.
    break
    case 2
      say Do they just let anyone walk in?
    break
    case 3
      say Shoddy repair work here. It looks like the stairs just end down there.
    break
  done
end
* force a delay at the end
wait 20 s
~
#11882
Smol Nes-Pik: Tresydion orations~
0 bw 25
~
set max_comment 3
set mob_list 11877 11878 11880 11881 11883
* determine position in the comment loop
if %self.varexists(comment)%
  eval comment %self.comment% + 1
else
  set comment 0
end
if %comment% > %max_comment%
  set comment 0
end
remote comment %self.id%
* turn on oration
set talking 1
remote talking %self.id%
* has a series of behaviors based on the entry in the loop
if %comment% == 0
  * oration not in english
  set cycle 0
  while %cycle% < 7
    switch %cycle%
      case 0
        say Ebru sal-ash Tagra Nes.
      break
      case 1
        say Dolo carzo, dolo olo, owri carzo Tagra Nes.
      break
      case 2
        say Breelo sal-ash Tagra Nes.
      break
      case 3
        say A wo owri avagor.
      break
      case 4
        say Ebru sal-ash Tagra Nes.
      break
      case 5
        say Ebru sal-ash corun smol.
      break
      case 6
        say Ebru sal-ash Sparo Vehl.
        wait 1
        set ch %self.room.people%
        while %ch%
          if %ch.is_npc% && %mob_list% ~= %ch.vnum%
            %force% %ch% say Ebru sal-ash Sparo Vehl.
          end
          set ch %ch.next_in_room%
        done
      break
    done
    eval cycle %cycle% + 1
    wait 4 sec
  done
elseif %comment% == 1
  * doing the directions
  set cycle 0
  while %cycle% < 10
    switch %cycle%
      case 0
        %echo% ~%self% walks slowly to the east end of the shell, stepping with his right foot first and then bringing his left foot to meet it.
      break
      case 1
        say Sacred is the East, where the dawn breaks and casts out the dark.
      break
      case 2
        %echo% ~%self% walks to the west end of the shell, beside the doorway.
      break
      case 3
        say Sacred is the West, dwelling of the sun.
      break
      case 4
        %echo% ~%self% walks to the south wall of the shell and raises his arms.
      break
      case 5
        say Sacred is the South, heart of winter and font of perpetual renewal.
      break
      case 6
        %echo% ~%self% walks to the pool in the center of the room.
      break
      case 7
        say Sacred is the dew, gem of morning, let it quench you.
      break
      case 8
        %echo% ~%self% kneels and scoops a handful of dew, and drinks it.
        wait 1
        set ch %self.room.people%
        while %ch%
          if %ch.is_npc% && %mob_list% ~= %ch.vnum%
            %echo% ~%ch% kneels and drinks from the pool.
          end
          set ch %ch.next_in_room%
        done
      break
      case 9
        say Let it quench you.
      break
    done
    eval cycle %cycle% + 1
    wait 5 sec
  done
elseif %comment% == 2
  * cleaning cycle (turn off oration; can be interrupted)
  rdelete talking %self.id%
  set cycle 0
  while %cycle% < 8
    switch %cycle%
      case 0
        %echo% ~%self% pulls a silk cloth from his pocket and dips it in the shimmering pool.
      break
      case 1
        %echo% ~%self% begins cleaning the walls with his rag.
      break
      case 2
        %echo% ~%self% scrubs the fine carvings of the seashell.
      break
      case 3
        %echo% ~%self% dips his rag in the pool and squeezes it out.
      break
      case 4
        %echo% ~%self% washes scuffmarks off the floor with his rag.
      break
      case 5
        %echo% ~%self% wipes down the edges of the pool, stopping to clean the rag from time to time.
      break
      case 6
        %echo% ~%self% sits back on his heels and wipes his brow.
      break
      case 7
        %echo% ~%self% hangs the rag to dry by the door.
      break
    done
    eval cycle %cycle% + 1
    wait 8 sec
  done
elseif %comment% == 3
  * just a pause (turn off oration; can be interrupted)
  rdelete talking %self.id%
  wait 60 sec
end
* turn off oration
rdelete talking %self.id%
wait 30 sec
~
#11884
Smol Nes-Pik: Taste putrid sap to teleport~
1 c 4
taste eat drink sip lick~
* This teleports players between templates 11887 and 11891
* Note: See below for adding requirements to use it
* 1. Basic checks
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
elseif %actor.position% != Standing
  %send% %actor% You need to be standing to taste the sap.
  halt
elseif %actor.fighting% || %actor.disabled%
  %send% %actor% You can't get to the sap right now!
  halt
end
* 2. Find target
set room %self.room%
if %room.template% == 11887
  set target %instance.nearest_rmt(11891)%
  set intro 1
else
  set target %instance.nearest_rmt(11887)%
  set intro 0
end
if !%target%
  * Note: this could log the error... but it shouldn't be possible anyway
  %send% %actor% It just tastes rotten.
  halt
end
* 3. Future use: Restrictions on who can use it, e.g. based on faction
* 4. Teleport player
%send% %actor% Before you can stop yourself, you reach out and %cmd.mudcommand% the sap...
%send% %actor% Everything goes black...
%echoaround% %actor% ~%actor%'s eyes go hazy as &%actor% reaches out and %cmd.mudcommand%s the putrid sap.
%echoaround% %actor% ~%actor% falls unconscious into the sap and vanishes beneath its sticky surface.
%teleport% %actor% %target%
if %intro%
  * player is in the intro chamber...
  dg_affect #11891 %actor% HARD-STUNNED on 104
else
  * player was exiting the sap
  %force% %actor% look
  %echoaround% %actor% ~%actor% emerges from the putrid sap.
end
* 5. Teleport followers
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_npc% && %ch.leader% == %actor% && !%ch.fighting% && !%ch.disabled%
    %send% %ch% You follow ~%actor% into the sap...
    %echoaround% %ch% ~%ch% follows ~%actor% into the sap...
    %teleport% %ch% %target%
    if !%intro%
      %force% %ch% look
      %echoaround% %ch% ~%ch% emerges from the putrid sap.
    end
  end
  set ch %next_ch%
done
* 6. If in room 11887, also set a decay timer on this sap
if %self.room.template% == 11887
  otimer 4
end
~
#11885
Mercenary Leader Death: Floor 3 completer~
0 f 100
~
%load% mob 11897
~
#11886
Statue combat~
1 c 4
break kill destroy shatter hit kick bash smash~
set target %actor.obj_target(%arg%)%
if !%target%
  %send% %actor% Break what?
  halt
end
if %target% != %self%
  %send% %actor% You can't break %target.shortdesc%.
  halt
end
set room %actor.room%
set cycles_left 3
while %cycles_left% >= 0
  if (%actor.room% != %room%) || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 3
      %echoaround% %actor% |%actor% combat is interrupted.
      %send% %actor% Your combat is interrupted.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake combat messages
  set weapon %actor.eq(wield)%
  switch %cycles_left%
    case 3
      if !%weapon%
        %echoaround% %actor% ~%actor% puts ^%actor% fists up and prepares to attack %self.shortdesc%...
        %send% %actor% You put your fists up and prepare to attack %self.shortdesc%...
      else
        %echoaround% %actor% ~%actor% readies %weapon.shortdesc% and prepares to attack %self.shortdesc%...
        %send% %actor% You ready %weapon.shortdesc% and prepare to attack %self.shortdesc%...
      end
    break
    case 2
      if !%weapon%
        %send% %actor% You punch %self.shortdesc%, sending cracks spreading across its surface!
        %echoaround% %actor% ~%actor% punches %self.shortdesc%, sending cracks spreading across its surface!
      elseif %weapon.attack(damage)% == physical
        %send% %actor% You %weapon.attack(1)% %self.shortdesc%, sending cracks spreading across its surface!
        %echoaround% %actor% ~%actor% %weapon.attack(3)% %self.shortdesc%, sending cracks spreading across its surface!
      else
        %echoaround% %actor% ~%actor% whacks %self.shortdesc% with %weapon.shortdesc%, cracking it slightly.
        %send% %actor% You whack %self.shortdesc% with %weapon.shortdesc%, cracking it slightly.
      end
    break
    case 1
      if !%weapon%
        %send% %actor% You punch %self.shortdesc%, breaking its arm off!
        %echoaround% %actor% ~%actor% punches %self.shortdesc%, breaking its arm off!
      elseif %weapon.attack(damage)% == physical
        %send% %actor% You %weapon.attack(1)% %self.shortdesc%, breaking its arm off!
        %echoaround% %actor% ~%actor% %weapon.attack(3)% %self.shortdesc%, breaking its arm off!
      else
        %echoaround% %actor% ~%actor% steps back and blasts %self.shortdesc% with a bolt from %weapon.shortdesc%, blowing its arm off!
        %send% %actor% You step back and blast %self.shortdesc% with a bolt from %weapon.shortdesc%, blowing its arm off!
      end
    break
    case 0
      if !%weapon%
        %send% %actor% You strike %self.shortdesc% with all your might, shattering it completely!
        %echoaround% %actor% ~%actor% punches %self.shortdesc% with a mighty roar, shattering it completely!
      else
        %send% %actor% You %weapon.attack(1)% %self.shortdesc% with all your might, shattering it completely!
        %echoaround% %actor% ~%actor% %weapon.attack(3)% %self.shortdesc% with a mighty roar, shattering it completely!
      end
      * Leave the loop
    break
  done
  if %cycles_left% > 0
    wait 3 sec
  end
  eval cycles_left %cycles_left% - 1
done
%load% mob 11896
%purge% %self%
~
#11887
Skycleave: Move-blocking object - Pixy Shield, Shadow Wall, Vortex~
1 q 100
~
* One quick trick to get the target room
set room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
switch %self.vnum%
  case 11863
    if %instance.mob(11869)%
      %send% %actor% You can't seem to get past %self.shortdesc%! You need to defeat the Shade of Mezvienne first!
    else
      %send% %actor% You can't seem to get past %self.shortdesc%! You need to defeat the Shadow Ascendant first!
    end
  break
  case 11887
    %send% %actor% You can't seem to get past %self.shortdesc%! You need to defeat the Pixy Queen first!
  break
  case 11870
    %send% %actor% The swirling vortex prevents you from climbing the stairs.
  break
  default
    %send% %actor% You can't seem to get past %self.shortdesc%!
  break
done
return 0
~
#11888
Skycleave: skygobpix trash loader~
0 c 0
skygobpix~
* Usage: skygobpix <difficulty 1-4> <a (any) | g (goblin) | p (pixy)>
if %actor% != %self%
  halt
end
set difficulty %arg.car%
set temp %arg.cdr%
set type %temp.car%
if !(%difficulty%)
  set difficulty 1
end
if %difficulty% > 2
  set amount 2
else
  set amount 1
end
if %type% == a
  if %random.2% == 2
    set type p
  else
    set type g
  end
end
switch %type%
  case g
    * Goblins: loads a group of mobs in the vnum range 11815, 11816, 11817
    set iter 0
    while %iter% < %amount%
      eval vnum 11814 + %random.3%
      %load% mob %vnum%
      set mob %self.room.people%
      if %mob.vnum% == %vnum%
        remote difficulty %mob.id%
      end
      eval iter %iter% + 1
    done
  break
  case p
    * Pixies: mob 11820
    set iter 0
    while %iter% < %amount%
      %load% mob 11820
      set mob %self.room.people%
      if %mob.vnum% == 11820
        remote difficulty %mob.id%
      end
      eval iter %iter% + 1
    done
  break
done
~
#11889
Skycleave: Skydel despawner~
0 c 0
skydel~
* Usage: skydel <vnum> <msg type>
*  msg 1: Name heads upstairs.
*  msg 2: Name goes back to work.
if %actor% != %self%
  halt
end
set vnum %arg.car%
set temp %arg.cdr%
set msg %temp.car%
if !(%vnum%)
  halt
end
set mob %instance.mob(%vnum%)%
if %mob%
  switch %msg%
    case 1
      %at% %mob.room% %echo% ~%mob% heads upstairs.
    break
    case 2
      %at% %mob.room% %echo% ~%mob% goes back to work.
    break
    case 3
      %at% %mob.room% %echo% ~%mob% heads downstairs.
    break
    default
      %at% %mob.room% %echo% ~%mob% leaves.
    break
  done
  %purge% %mob%
end
~
#11890
Skycleave: Difficulty selector floor 1 to 2~
0 c 0
difficulty up~
* Process argument
if %cmd.mudcommand% == up
  %send% %actor% ~%self% won't let you pass until you set a difficulty for the second floor.
  return 1
  halt
elseif !%arg%
  %send% %actor% You must specify a level of difficulty (normal, hard, group, or boss).
  return 1
  halt
end
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
* Messaging
%echo% ~%self% drops the protective wards and breathes a sigh of relief.
if !%self.room.up(room)%
  %door% %self.room% u room i11810
end
* Remove ward
set ward %self.room.contents(11831)%
if %ward%
  %purge% %ward%
end
* Store difficulty
set spirit %instance.mob(11900)%
set diff2 %difficulty%
remote diff2 %spirit.id%
* Load mobs
%at% i11810 skygobpix %difficulty% g  * trash mob
%at% i11811 skygobpix %difficulty% g  * trash mob
%at% i11812 skygobpix %difficulty% a  * trash mob
%at% i11813 skygobpix %difficulty% p  * trash mob
%at% i11814 skygobpix %difficulty% g  * trash mob
%at% i11816 %load% mob 11812  * Apprentice Cosimo (hiding)
%at% i11818 skyload 11819 %difficulty%  * Pixy Boss
%at% i11819 skygobpix %difficulty% a  * trash mob
%at% i11820 skygobpix %difficulty% p  * trash mob
%at% i11821 skygobpix %difficulty% a  * trash mob
%at% i11822 %load% mob 11810  * Watcher Annaca (phase A dummy)
%at% i11823 skygobpix %difficulty% p  * trash mob
%at% i11824 skygobpix %difficulty% a  * trash mob
%at% i11825 skyload 11818 %difficulty%  * Goblin Boss
%load% mob 11864  * Replacement Celiya
%purge% %self%
~
#11891
Skycleave: Difficulty selector floor 2 to 3~
0 c 0
difficulty up~
* Process argument
if %cmd.mudcommand% == up
  %send% %actor% ~%self% won't let you pass until you set a difficulty for the third floor.
  return 1
  halt
elseif !%arg%
  %send% %actor% You must specify a level of difficulty (normal, hard, group, or boss).
  return 1
  halt
end
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
* Messaging
%echo% ~%self% cancels ^%self% mana shield and steps away from the staircase.
if !%self.room.up(room)%
  %door% %self.room% u room i11830
end
* Remove ward
set ward %self.room.contents(11831)%
if %ward%
  %purge% %ward%
end
* Store difficulty
set spirit %instance.mob(11900)%
set diff3 %difficulty%
remote diff3 %spirit.id%
* Load mobs
%at% i11834 %load% mob 11830  * High Master Caius (phase A dummy)
%at% i11835 %load% mob 11834  * Otherworlder (chained)
%at% i11835 %load% mob 11835  * Lab assistant
%at% i11835 skyload 11847 %difficulty%  * Merc Caster boss
%at% i11839 skyload 11848 %difficulty%  * Merc Rogue boss
%at% i11837 skyload 11849 %difficulty%  * Merc Leader boss
%at% i11830 skymerc %difficulty%  * Load set of mercenaries: hall
%at% i11831 skymerc %difficulty%  *   hall
%at% i11832 skymerc %difficulty%  *   hall
%at% i11833 skymerc %difficulty%  *   hall
%at% i11836 skymerc %difficulty%  *   Lich Labs
%at% i11840 %load% mob 11831  * Resident Niamh
%at% i11840 %load% mob 11840  * Magineer
%load% mob 11910  * Replacement Watcher
%purge% %self%
~
#11892
Skycleave: Difficulty selector floor 3 to 4~
0 c 0
difficulty up~
* Process argument
if %cmd.mudcommand% == up
  %send% %actor% ~%self% won't let you pass until you set a difficulty for the fourth floor.
  return 1
  halt
elseif !%arg%
  %send% %actor% You must specify a level of difficulty (normal, hard, group, or boss).
  return 1
  halt
end
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
* Messaging
%echo% ~%self% drops ^%self% wards and warns you to be careful upstairs.
if !%self.room.up(room)%
  %door% %self.room% u room i11860
end
* Remove ward
set ward %self.room.contents(11831)%
if %ward%
  %purge% %ward%
end
* Store difficulty
set spirit %instance.mob(11900)%
set diff4 %difficulty%
remote diff4 %spirit.id%
* Load mobs
%at% i11865 %load% mob 11859  * Page Sheila
%at% i11865 %load% mob 11862  * Apprentice Thorley
%at% i11866 skyload 11867 %difficulty%  * Mind-controlled Barrosh
%at% i11867 %load% mob 11860  * Captive assistant
%at% i11867 %load% mob 11851  * Captive page Paige
%at% i11868 skyload 11869 %difficulty%  * Shade of Mezvienne
%at% i11868 %load% mob 11868  * Injured Knezz
%at% i11871 skyload 11871 %difficulty%  * Mezvienne the Enchantress
%load% mob 11930  * Replacement High Master
%purge% %self%
~
#11893
Skycleave dummy difficulty selector~
0 c 0
difficulty~
%send% %actor% You must complete this floor before you can select a difficulty for the next one.
~
#11894
Skycleave: Phase Change 1 Room~
0 hn 100
~
* Teleport all players/followers from 1 phase to the 2nd phase
* And despawn all adventure mobs here
if %actor.nohassle%
  halt
end
wait 0
set room %self.room%
if %room.template% < 11801 || %room.template% > 11899
  * Only works in phase 1 of Skycleave
  %purge% %self%
  halt
end
* Ensure part of the instance
nop %self.link_instance%
eval to_vnum %room.template% + 100
set to_room %instance.nearest_rmt(%to_vnum%)%
if !%to_room%
  * No destination
  %purge% %self%
  halt
end
* Message now
%echo% This part of Skycleave has been restored!
* Check items here
set obj %room.contents%
while %obj%
  set next_obj %obj.next_in_list%
  if %obj.can_wear(TAKE)%
    %teleport% %obj% %to_room%
  end
  set obj %next_obj%
done
* Check people here
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.nohassle% || (%ch.vnum% >= 11890 && %ch.vnum% <= 11899)
    * nothing (11890-11899 are mobs involved in phase change)
  elseif %ch.is_pc% || %ch.vnum% < 11800 || %ch.vnum% > 11988
    * Move ch (stops at 11988 because of mini-pets from this adventure)
    %teleport% %ch% %to_room%
    %load% obj 11805 %ch%
    * check quest completion
    if %room.template% >= 11810 && %room.template% <= 11826
      * Floor 2
      if %ch.on_quest(11810)%
        %quest% %ch% trigger 11810
      end
      if %ch.on_quest(11826)%
        %quest% %ch% trigger 11826
      end
    elseif %room.template% >= 11830 && %room.template% <= 11841
      * Floor 3
      if %ch.on_quest(11811)%
        %quest% %ch% trigger 11811
      end
    elseif %room.template% >= 11860 && %room.template% <= 11872
      * Floor 4
      if %ch.on_quest(11812)%
        %quest% %ch% trigger 11812
      end
    end
  elseif %ch% != %self%
    * Adventure mob: purge
    %purge% %ch%
  end
  set ch %next_ch%
done
~
#11895
Skycleave: Phase change, floor 1~
0 n 100
~
* Converts the 1st floor of Skycleave from phase A to phase B
set start_room 11801
set end_room 11808
* Verify in-Skycleave
if %self.room.template% < 11800 || %self.room.template% > 11999
  %purge% %self%
  halt
end
wait 0
* Ensure part of the instance
nop %self.link_instance%
* 1. Re-link entrance (No announcement for floor 1)
%door% i11800 north room i11901
* 2. Re-link 'down' exit from floor 2
%door% i11910 down room i11904
* 3. Mobs: Any checks based on surviving mobs go here (step 4 will purge them)
%at% i11800 %load% mob 11901  * Gossipper
%at% i11800 %load% mob 11901  * Additional Gossipper
%at% i11903 %load% mob 11904  * Page Corwin
%at% i11905 %load% mob 11905  * Barista
%at% i11905 %load% mob 11827  * chatty couple
%at% i11905 %load% mob 11828  * chatty couple
%at% i11906 %load% mob 11906  * Instructor
%at% i11906 %load% mob 11908  * Student Elamm
%at% i11906 %load% mob 11909  * Student Akeldama
%at% i11907 %load% mob 11907  * Gift Shop Keeper
set shopkeep %instance.mob(11807)%
set newshop %instance.mob(11907)%
if %shopkeep% && %newshop%
  nop %newshop.namelist(%shopkeep.namelist%)%
end
%at% i11904 %load% mob 11902  * Bucket (and sponge)
* attach some trigs
set 11937_list 11945 11929
while %11937_list%
  set vnum %11937_list.car%
  set 11937_list %11937_list.cdr%
  set mob %instance.mob(%vnum%)%
  if %mob%
    attach 11937 %mob.id%
  end
done
* attach tourist loader
makeuid entryway room i11901
attach 11827 %entryway.id%
* 4. Move people from the old rooms
set vnum %start_room%
while %vnum% <= %end_room%
  %at% i%vnum% %load% mob 11894
  eval vnum %vnum% + 1
done
* Store phase
set spirit %instance.mob(11900)%
set phase1 1
remote phase1 %spirit.id%
%purge% %self%
~
#11896
Skycleave: Phase change, floor 2~
0 n 100
~
* Converts the 2nd floor of Skycleave from phase A to phase B
set start_room 11810
set end_room 11826
* Verify in-Skycleave
if %self.room.template% < 11800 || %self.room.template% > 11999
  %purge% %self%
  halt
end
wait 0
* Ensure part of the instance
nop %self.link_instance%
* 1. Announce
%regionecho% %self.room% 0 # The second floor of the Tower Skycleave has been rescued!
* 2. Re-link 'up' exit from floor 1
%door% i11804 up room i11910
* 3. Mobs: Any checks based on surviving mobs go here (step 4 will purge them)
set spirit %instance.mob(11900)%
skydel 11811 1  * Apprentice Kayla 1A
skydel 11812 2  * Apprentice Cosimo 2A
skydel 11813 1  * Apprentice Tyrone 1A
skydel 11825 1  * Apprentice Ravinder 1A
%at% i11922 %load% mob 11891  * Watcher (difficulty selector version)
%at% i11911 %load% mob 11911  * Apprentice Kayla
%at% i11912 %load% mob 11912  * Apprentice Cosimo
%at% i11918 %load% mob 11945  * Page Amets
%at% i11921 %load% mob 11913  * Apprentice Tyrone
%at% i11925 %load% mob 11925  * Apprentice Ravinder
* check for any remaining goblins
set goblin_11815 0
set goblin_11816 0
set goblin_11817 0
set valid_goblin_locs 11810 11811 11813 11814 11815 11820 11823
set vnum 11815
while %vnum% <= 11817
  set mob %instance.mob(%vnum%)%
  if %mob%
    if %valid_goblin_locs% ~= %mob.room.template%
      set goblin_%vnum% 1
    end
    %purge% %mob%
  else
    eval vnum %vnum% + 1
  end
done
* place goblins
skydel 11814 1  * Old Wright
if %goblin_11817%
  set any_goblin 1
  %at% i11915 %load% mob 11917  * Caged Goblin
end
if %goblin_11816%
  set any_goblin 1
  %at% i11915 %load% mob 11916  * Caged Goblin Commando
end
if !%any_goblin%
  %at% i11915 %load% obj 11915  * empty cage
end
if %any_goblin% || %goblin_11815%
  set any_goblin 1
  %at% i11914 %load% mob 11914  * Goblin Wrangler
  %at% i11914 %load% mob 11915  * Caged Goblin
else
  * no goblins at all?
  %at% i11914 %load% mob 11926  * Goblin Wrangler turned into sponge
  %at% i11914 %load% obj 11915  * empty cage
end
%at% i11918 %load% mob 11918  * Race Caller
%at% i11919 %load% mob 11921  * Broom
%at% i11912 %load% mob 11922  * Duster
* 4. Move people from the old rooms
set vnum %start_room%
while %vnum% <= %end_room%
  %at% i%vnum% %load% mob 11894
  eval vnum %vnum% + 1
done
* 5. Update a description
makeuid hall room i11911
if %any_goblin%
  %mod% %hall% append-description Unsettling sounds and high-pitched shrieks come from a sturdy door with a no-nonsense sign above it marked in red lettering.
else
  %mod% %hall% append-description A sturdy oak door bears a no-nonsense sign marked in red lettering.
end
* Store phase
set spirit %instance.mob(11900)%
set phase2 1
remote phase2 %spirit.id%
%purge% %self%
~
#11897
Skycleave: Phase change, floor 3~
0 n 100
~
* Converts the 3rd floor of Skycleave from phase A to phase B
set start_room 11830
set end_room 11841
* Verify in-Skycleave
if %self.room.template% < 11800 || %self.room.template% > 11999
  %purge% %self%
  halt
end
wait 0
* Ensure part of the instance
nop %self.link_instance%
* 1. Announce
%regionecho% %self.room% 0 # The third floor of the Tower Skycleave has been rescued!
* 2. Re-link 'up' exit from floor 2
%door% i11922 up room i11930
* 3. Mobs: Any checks based on surviving mobs go here (step 4 will purge them)
%at% i11934 %load% mob 11892  * High Master difficulty selector
set niamh %instance.mob(11831)%
if %niamh%
  %at% i11931 %load% mob 11931  * Resident Niamh (if she survived)
end
%at% i11932 %load% mob 11978  * moth
skydel 11832 1  * Resident Mohammed
%at% i11935 %load% mob 11932  * Resident Mohammed
if %instance.mob(11834)%
  %at% i11935 %load% mob 11934  * Chained Otherworlder (if it survived/stayed)
end
if %instance.mob(11835)%
  if %instance.mob(11834)%
    %at% i11935 %load% mob 11935  * Apprentice Sanjiv, scanning
  else
    %at% i11935 %load% mob 11942  * Apprentice Sanjiv, no otherworlder
  end
end
%at% i11936 %load% mob 11936  * Scaldorran
skydel 11838 1  * Ghost
%at% i11938 %load% mob 11938  * Ghost
skydel 11839 1  * Enchanter Annelise
%at% i11939 %load% mob 11939  * Enchanter Annelise
set waltur %instance.mob(11840)%
if %waltur%
  %at% i11940 %load% mob 11940  * Magineer Waltur (if he survived)
end
%at% i11932 %load% mob 11933  * Walking Mop
skydel 11833 0  * Goef the shimmer
%at% i11941 %load% mob 11941  * Goef the Attuner
* 4. Move people from the old rooms
set vnum %start_room%
while %vnum% <= %end_room%
  %at% i%vnum% %load% mob 11894
  eval vnum %vnum% + 1
done
* Store phase
set spirit %instance.mob(11900)%
set phase3 1
remote phase3 %spirit.id%
%purge% %self%
~
#11898
Skycleave: Phase change, floor 4~
0 n 100
~
* Converts the 4th floor of Skycleave from phase A to phase B
set start_room 11860
set end_room 11871
* Verify in-Skycleave
if %self.room.template% < 11800 || %self.room.template% > 11999
  %purge% %self%
  halt
end
wait 0
* Ensure part of the instance
nop %self.link_instance%
* 1. Announce
%regionecho% %self.room% 0 # The fourth floor of the Tower Skycleave has been rescued!
* 2. Re-link 'up' exit from floor 3
%door% i11934 up room i11960
* 3. Mobs: Any checks based on surviving mobs go here (step 4 will purge them)
skydel 11864 1  * Celiya 1A
skydel 11862 3  * Thorley 4A
%at% i11924 %load% mob 11962  * Apprentice Thorley
%at% i11965 %load% mob 11959  * Page Sheila
if %instance.mob(11861)%
  * Barrosh fight completed: captives survive
  %at% i11966 %load% mob 11966  * Assistant to Barrosh
  %at% i11933 %load% mob 11929  * page Paige
end
%at% i11967 %load% mob 11967  * Barrosh
if %instance.mob(11868)% || %instance.mob(11870)%
  * Knezz survived
  %at% i11964 %load% mob 11964  * HS Celiya
  %at% i11964 %load% obj 11969  * Celiya's desk
  %at% i11968 %load% mob 11968  * GHS Knezz
  %at% i11968 %load% obj 11968  * Knezz's desk
  * Mark for claw game
  set spirit %instance.mob(11900)%
  set claw4 1
  remote claw4 %spirit.id%
  * shout
  set mob %instance.mob(11968)%
  if %mob%
    %force% %mob% shout By the power of Skycleave... I HAVE THE POWER!
  end
else
  * Knezz died
  %at% i11964 %load% mob 11965  * Office Cleaner
  %at% i11968 %load% mob 11969  * GHS Celiya
  %at% i11968 %load% obj 11969  * Celiya's desk
  * shout
  set mob %instance.mob(11969)%
  if %mob%
    %force% %mob% shout By the power of Skycleave... I HAVE THE POWER!
  end
end
%at% i11961 %load% mob 11960  * Filing Cabinet
%at% i11963 %load% mob 11961  * Bookshelf
%at% i11971 %load% obj 11970  * hendecagon fountain
* 4. Move people from the old rooms
set vnum %start_room%
while %vnum% <= %end_room%
  %at% i%vnum% %load% mob 11894
  eval vnum %vnum% + 1
done
* Store phase
set spirit %instance.mob(11900)%
set phase4 1
remote phase4 %spirit.id%
%purge% %self%
~
#11899
Skycleave: Skyload mobs on difficulty select~
0 c 0
skyload~
* Usage: skyload <mob vnum> <difficulty 1-4>
if %actor% != %self%
  halt
end
set vnum %arg.car%
set temp %arg.cdr%
set diff %temp.car%
if !(%vnum) || !(%diff%)
  halt
end
%load% mob %vnum%
set mob %self.room.people%
if %mob.vnum% != %vnum%
  halt
end
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %diff% == 1
  * Then we don't need to do anything
elseif %diff% == 2
  nop %mob.add_mob_flag(HARD)%
elseif %diff% == 3
  nop %mob.add_mob_flag(GROUP)%
elseif %diff% == 4
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
~
$
