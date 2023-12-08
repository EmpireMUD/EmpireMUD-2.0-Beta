#10400
Hamlet guard intro speech~
0 g 100
~
if %self.varexists(scaled)%
  * skip intro once scaled
  halt
end
* brief pause
wait 1
* show messages
set cycle 10
while %cycle% > 0
  if %self.varexists(scaled)%
    * exit early
    halt
  end
  switch %cycle%
    case 9
      say Oh, thank goodness, a hero!
    break
    case 8
      say There are cultists burning the hamlet. Necromancers, I think.
    break
    case 7
      say The hamlet is burning. Most of it is probably lost.
    break
    case 6
      say Maybe you could at least clear out the undead, so we can salvage what's left in our homes?
    break
    case 5
      say Please, if you could. There's no army out here. We need a hero!
    break
    case 4
      say Northwest is Market Street. There's two giant skeletons roaming the street!
    break
    case 3
      say To the North is Main Street. It's covered in demons, led by an actual Infernomancer!
    break
    case 2
      say Lastly, Northeast is Temple Street, overrun by the Necromancers and their risen minions. Good luck!
    break
    case 1
      say Just let me know what kind of challenge you're in for!
      %echo% Usage: difficulty <normal \| hard \| group \| boss>
    break
  done
  * end loop
  wait 3 sec
  eval cycle %cycle% - 1
done
~
#10401
Hamlet intro exit block~
0 s 100
~
if %direction% != south && !%actor.nohassle% && !%self.varexists(scaled)%
  %send% %actor% You can't proceed until you choose a difficulty.
  %send% %actor% Usage: difficulty <normal \| hard \| group \| boss>
  return 0
end
~
#10402
Hamlet unified trash spawner~
1 n 100
~
* Ensure no mobs here
set template %self.room.template%
set ch %self.room.people%
while %ch%
  if %ch.is_npc%
    * already a mob here
    %purge% %self%
    halt
  end
  set ch %ch.next_in_room%
done
* determine trash mob vnum range
if %template% >= 10401 && %template% <= 10406
  set base 10401
elseif %template% >= 10407 && %template% <= 10412
  set base 10406
else
  set base 10411
end
* spawns
switch %random.3%
  case 1
    %load% mob %base%
    %load% mob %base%
  break
  case 2
    eval vnum %base% + 1
    %load% mob %vnum%
  break
  case 3
    eval vnum %base% + 2
    %load% mob %vnum%
  break
done
%purge% %self%
~
#10403
Hamlet of the Undead difficulty selector~
0 c 0
difficulty~
return 1
* Configs
set start_room 10401
set end_room 10418
set boss_mobs 10404 10405 10409 10410 10414 10415
set trash_mobs 10401 10402 10403 10406 10407 10408 10411 10412 10413
* Check args...
if %self.varexists(scaled)%
  %send% %actor% The difficulty has already been set.
  halt
end
if !%arg%
  %send% %actor% You must specify a level of difficulty.
  halt
end
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set difficulty 1
  set trash 0
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set difficulty 2
  set trash 0
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  set difficulty 3
  set trash 1
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  set difficulty 4
  set trash 1
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
end
* Clear existing difficulty flags and set new ones.
set vnum %start_room%
while %vnum% <= %end_room%
  set there %instance.nearest_rmt(%vnum%)%
  if %there%
    set mob %there.people%
    set last 0
    while %mob%
      set next_mob %mob.next_in_room%
      if %mob.vnum% >= 10400 && %mob.vnum% <= 10449
        * wipe difficulty flags
        nop %mob.remove_mob_flag(HARD)%
        nop %mob.remove_mob_flag(GROUP)%
      end
      if %boss_mobs% ~= %mob.vnum%
        * scale as boss
        if %difficulty% == 2
          nop %mob.add_mob_flag(HARD)%
        elseif %difficulty% == 3
          nop %mob.add_mob_flag(GROUP)%
        elseif %difficulty% == 4
          nop %mob.add_mob_flag(HARD)%
          nop %mob.add_mob_flag(GROUP)%
        end
      elseif %trash% && %trash_mobs% ~= %mob.vnum%
        * scale as trash
        nop %mob.add_mob_flag(HARD)%
      end
      * on normal, remove duplicate mobs
      if %difficulty% == 1
        if %mob.vnum% == %last%
          %purge% %mob%
        else
          set last %mob.vnum%
        end
      end
      * and loop
      set mob %next_mob%
    done
  end
  * and repeat
  eval vnum %vnum% + 1
done
* messaging:
if %difficulty% > 2
  say Are you certain? Okay...
end
shout Now you infernals are in for it... the heroes have arrived!
* mark me as scaled
set scaled 1
remote scaled %self.id%
~
#10404
Hamlet burn-down (cleanup)~
2 e 100
~
* check if instance was scaled at all -- if not, don't bother with ruins
set guard %instance.mob(10400)%
if !%guard%
  * not even loaded? no players have been inside
  halt
elseif !%guard.varexists(scaled)%
  * never been scaled
  halt
end
* check if any bosses remain
set boss_mobs 10405 10410 10415
set any 0
while %boss_mobs% && !%any%
  set vnum %boss_mobs.car%
  set boss_mobs %boss_mobs.cdr%
  if %instance.mob(%vnum%)%
    set any 1
  end
done
if %any%
  * Bad ending
  return 0
  %regionecho% %room% 15 The hamlet at %room.coords% has burned down!
  %build% %room% 10401
end
~
#10405
Mob block higher template id~
0 s 100
~
* list of mobs you can sneak past
set sneakable_list 10401 10402 10403 10406 10407 10408 10411 10412 10413
* One quick trick to get the target room
set room_var %self.room%
eval to_room %%room_var.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%to_room% || %to_room.template% < %room_var.template%)
  halt
elseif %actor.aff_flagged(SNEAK)% && %sneakable_list% ~= %self.vnum%
  * allow sneak
  halt
end
%send% %actor% You can't seem to get past ~%self%!
return 0
~
#10406
Hamlet boss death (portal, complete, and echo)~
0 f 100
~
* Spawn town portal; no message
%load% obj 10400
* Mark adventure complete
%adventurecomplete%
* And echo..
%regionecho% %self.room% 1 The citizens of the hamlet rejoice as %self.name% is defeated!
~
#10407
Hamlet environmental object echoes~
1 bw 3
~
if %self.varexists(message_type)%
  set message_type %self.message_type%
else
  detach 10407 %self.id%
  halt
end
switch %messsage_type%
  case 1
    %echo% The makeshift blockade crackles in the fire.
  break
done
~
#10408
Hamlet of the Undead env~
2 bw 3
~
* check if any bosses remain
set boss_mobs 10405 10410 10415
set any 0
while %boss_mobs% && !%any%
  set vnum %boss_mobs.car%
  set boss_mobs %boss_mobs.cdr%
  if %instance.mob(%vnum%)%
    set any 1
  end
done
if !%any%
  halt
end
* message only if bosses remain
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
done
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
        %send% %actor% ~%self% shoots a bolt of violet energy at you, but it breaks on your counterspell!
        %echoaround% %actor% ~%self% shoots a bolt of violet energy at ~%actor%, but it breaks on ^%actor% counterspell!
      else
        %send% %actor% ~%self% shoots a bolt of violet energy at you!
        %echoaround% %actor% ~%self% shoots a bolt of violet energy at ~%actor%!
        %damage% %actor% 100 magical
        %dot% %actor% 100 15 magical
      end
    break
    case 2
      %send% %actor% ~%self% stares into your eyes. You can't move!
      %echoaround% %actor% ~%self% stares into |%actor% eyes. ~%actor% seems unable to move!
      dg_affect %actor% HARD-STUNNED on 10
    break
    case 3
      %echo% ~%self% raises ^%self% arms and the floor begins to rumble...
      wait 2 sec
      %echo% Bone hands rise from the floor and claw at you!
      %aoe% 100 physical
    break
    case 4
      set target %random.enemy%
      if (%target%)
        if %target.trigger_counterspell%
          %send% %target% ~%self% shoots a bolt of violet energy at you, but it breaks on your counterspell!
          %echoaround% %target% ~%self% shoots a bolt of violet energy at ~%target%, but it breaks on ^%target% counterspell!
        else
          %send% %target% ~%self% shoots a bolt of violet energy at you!
          %echoaround% %target% ~%self% shoots a bolt of violet energy at ~%target%!
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
    %send% %actor% ~%self% shoots a poisoned spine at you!
    %echoaround% %actor% ~%self% shoots a poisoned spine at ~%actor%!
    %damage% %actor% 50 physical
    if (%actor.poison_immunity% || (%actor.resist_poison% && %random.100% < 50))
      %dot% %actor% 100 15 poison
    end
  break
  case 2
    set target %random.enemy%
    if (%target%)
      %send% %target% ~%self% shoots a poisoned spine at you!
      %echoaround% %target% ~%self% shoots a poisoned spine at ~%target%!
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
        %send% %actor% ~%self% hurls a flaming meteor at you, but it fizzles on your counterspell!
        %echoaround% %actor% ~%self% hurls a flaming meteor at ~%actor%, but it fizzles on ^%actor% counterspell!
      else
        %send% %actor% ~%self% hurls a flaming meteor at you!
        %echoaround% %actor% ~%self% hurls a flaming meteor at ~%actor%!
        %damage% %actor% 100 fire
        %dot% %actor% 100 15 fire
      end
    break
    case 2
      %echo% ~%self% raises ^%self% arms and flames swirl around *%self%!
      wait 2 sec
      %echo% Waves of flame wash over you, and smoke chokes your lungs!
      %aoe% 100 fire
    break
    case 3
      set target %random.enemy%
      if (%target%)
        if %target.trigger_counterspell%
          %send% %target% ~%self% hurls a flaming meteor at you, but it fizzles on your counterspell!
          %echoaround% %target% ~%self% hurls a flaming meteor at ~%target%, but it fizzles on ^%target% counterspell!
        else
          %send% %target% ~%self% hurls a flaming meteor at you!
          %echoaround% %target% ~%self% hurls a flaming meteor at ~%target%!
          %damage% %target% 100 fire
          %dot% %target% 100 15 fire
        end
      end
    break
  done
end
~
#10413
Hamlet Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10400)%
end
~
#10414
Hamlet environmental object setup~
1 n 100
~
* this script combines multiple environmental objects into one to conserve vnums
set id %self.room.template%
set message_type 0
if %id% == 10403 || %id% == 10409
  %mod% %self% keywords blockade makeshift
  %mod% %self% shortdesc the blockade
  %mod% %self% longdesc A makeshift blockade bars passage down the street.
  %mod% %self% lookdesc It looks like the citizens managed to pile up street stalls and carts to
  %mod% %self% append-lookdesc form a blockade before the necromancers advanced on the city. Now the blockade
  %mod% %self% append-lookdesc is on fire, and you won't be able to pass it.
  set message_type 1
elseif %id% == 10401 || %id% == 10402 || %id% == 10408
  %mod% %self% keywords cart overturned
  %mod% %self% shortdesc an overturned cart
  %mod% %self% longdesc An overturned cart is lying in the street.
  %mod% %self% lookdesc In his haste to flee the hamlet, some poor fool lost his cart!
elseif %id% == 10411 || %id% == 10414
  %mod% %self% keywords corpse citizen
  %mod% %self% shortdesc the corpse of a citizen
  %mod% %self% longdesc The mangled, half-chewed corpse of a citizen is lying here.
  %mod% %self% lookdesc It looks like something has already started to eat this one.
else
  * all other rooms
  %mod% %self% keywords rubble pile
  %mod% %self% shortdesc a pile of rubble
  %mod% %self% longdesc A pile of rubble cascades down the street.
  %mod% %self% lookdesc This rubble looks like it was once the front of a building, perhaps torn
  %mod% %self% append-lookdesc down by a stray catapult shot -- or the club of the skeletal giant down the
  %mod% %self% append-lookdesc street.
end
* and store any message_type
remote message_type %self.id%
detach 10414 %self.id%
~
#10415
Hamlet portal announcement~
1 n 100
~
wait 1
%echo% A portal back to the city gates spins open.
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
if !%self.mob_flagged(HARD)% && !%self.mob_flagged(GROUP)%
  halt
end
switch %random.2%
  case 1
    kick
  break
  case 2
    jab
  break
done
~
#10454
GO Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10450)%
end
~
#10455
Goblin Outpost per-mob difficulty selector~
0 c 0
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty. (Normal, Hard, or Group)
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
  set str normal
elseif hard /= %arg%
  set difficulty 2
  set str hard
elseif group /= %arg%
  set difficulty 3
  set str group
else
  %send% %actor% That is not a valid difficulty level for this adventure. (Normal, Hard, or Group)
  halt
  return 1
end
* messaging
%send% %actor% You set the difficulty to %str%...
%echoaround% %actor% ~%actor% sets the difficulty to %str%...
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
wait 1
if %mob.aff_flagged(!ATTACK)%
  dg_affect %mob% !ATTACK off
  switch %mob.vnum%
    case 10451
      * Dryleef
      say Now you're in for it!
    break
    case 10452
      * Pimmin
      say Pimmin has plans for you now!
    break
    case 10453
      * Shivsper
      say Oh boy, stabbing time!
    break
    case 10454
      * Wargreyn
      say A lot of other humans have tried...
    break
    case 10457
      * Haxaw
      say Human seems real brave for someone whose guts are worth so much copper.
    break
    case 10455
      * giant rat
      %echo% ~%self% bares its sharp teeth!
    break
    default
      say Oh! A challenger!
    break
  done
end
~
#10456
Wargreyn combat~
0 k 8
~
if !%self.mob_flagged(HARD)% && !%self.mob_flagged(GROUP)%
  halt
end
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
Goblin Outpost mob setup~
0 n 100
~
dg_affect %self% !ATTACK on -1
if %self.vnum% == 10457
  wait 1
  enter tent
end
~
#10458
Goblin Outpost attack info~
0 B 0
~
if %self.aff_flagged(!ATTACK)%
  if %actor.char_target(%arg%)% != %self%
    return 1
    halt
  end
  %send% %actor% You need to choose a difficulty before you can challenge ~%self%.
  %send% %actor% Usage: difficulty <normal \| hard \| group>
  %echoaround% %actor% ~%actor% considers attacking ~%self%.
  return 0
else
  * no need for this script anymore
  detach 10458 %self.id%
  return 1
end
~
#10459
Goblin Outpost death replacements~
0 f 100
~
switch %self.vnum%
  case 10451
    * Dryleef
    shout This city not worth the trouble!
    %echo% ~%self% ducks behind the throne and scurries down the blocked tunnel, leaving something behind.
  break
  case 10452
    * Pimmin
    shout No! Bad human! Pimmin going back to mother.
    %echo% ~%self% grabs what &%self% can take and runs off, leaving most of it behind.
  break
  case 10453
    * Shivsper
    shout Oh no, oh no, oh no, no, no, no, no, no! Shivsper not getting paid enough.
    %echo% ~%self% drops a smokebomb and vanishes, but &%self% seems to drop something in the process.
  break
  case 10454
    * Wargreyn
    shout Wargreyn not live this long by dying to humans. Goblins retreat!
    %echo% ~%self% walks away solemnly, leaving most of ^%self% things behind.
  break
  case 10457
    * Haxaw
    shout Haxaw will wait till you die on your own and then use your bones to summon rats!
    %echo% ~%self% pushes past you and scurries out of the tent.
  break
  default
    %echo% ~%self% scuries away down a dark tunnel.
  break
done
* prevent death cry
return 0
~
#10460
Goblin Outpost item BOE/BOP craft/loot twiddler~
1 n 100
~
* items default to BOP but are set BOE if they come from a shop or craft
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
end
if !%actor%
  halt
end
if %actor% && %actor.is_pc%
  * Item was crafted or bought
  if %self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if !%self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
  * Probably need to unbind when BOE
  nop %self.bind(nobody)%
else
  * Item was probably dropped as loot
  if !%self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if %self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
end
detach 10460 %self.id%
~
#10461
Goblin Outpost: Pimmin combat~
0 k 10
~
if !%self.mob_flagged(HARD)% && !%self.mob_flagged(GROUP)%
  halt
end
if !%self.aff_flagged(HASTE)%
  hasten
elseif !%actor.aff_flagged(SLOW)%
  slow
else
  switch %random.4%
    case 1
      counterspell
    break
    case 2
      entangle
    break
    case 3
      lightningbolt
    break
    case 4
      skybrand
    break
  done
end
~
#10462
Goblin Outpost: Haxaw combat~
0 k 8
~
if !%self.mob_flagged(HARD)% && !%self.mob_flagged(GROUP)%
  halt
end
if !%self.aff_flagged(HASTE)%
  hasten
elseif !%actor.aff_flagged(SLOW)%
  slow
else
  heal
end
~
#10499
Learn Goblin Outpost craft book~
1 c 2
learn~
* Usage: learn <self>
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
* all other cases will return 1
* switch MUST set vnum_list, abil_list, and name_list
switch %self.vnum%
  case 10495
    * Haxaw
    set vnum_list 10473 10474 10485 10486 10487 10488
    set abil_list 54 199 196 264 199 199
    set name_list Haxaw's poison ring, goblin neck smile, flail of many blessings, goose-beak totem, goblin deathshroud, and doubleskin gloves
  break
  case 10496
    * Pimmin
    set vnum_list 10452 10453 10454 10455 10456 10457
    set abil_list 54 199 196 196 199 199
    set name_list Pimmin's ring, bone charm necklace, chicken-bone wand, goblinward tome, violet leopard shawl, and wildfire gloves
  break
  case 10497
    * Shivsper
    set vnum_list 10458 10459 10460 10461 10462 10463
    set abil_list 54 54 196 196 182 182
    set name_list Shivsper's poison ring, gaudy copper chain, someone else's axe, giant dagger, rat-fur cloak, and wooden gauntlets
  break
  case 10498
    * Wargreyn
    set vnum_list 10464 10465 10466 10467 10468 10469
    set abil_list 54 54 196 197 197 197
    set name_list Wargreyn's ring, blacksmith's charm, goblinsmith's hammer, wargskull shield, mesh cloak, and heavy goblin gauntlets
  break
  case 10499
    * Dryleef
    set vnum_list 10489 10490 10491 10492 10493 10494
    set abil_list 54 54 196 197 182 182
    set name_list Dryleef's pitfighting ring, zirconian amulet of goblin bravery, bog-iron pitfighter's sword, Dryleef's pitfighting buckler, goblin pitfighter's cape, and goblin punching bags
  break
  default
    %send% %actor% There's nothing you can learn from this.
    halt
  break
done
* check if the player needs any of them
set any 0
set error 0
set vnum_copy %vnum_list%
while %vnum_copy%
  * pluck first numbers out
  set vnum %vnum_copy.car%
  set vnum_copy %vnum_copy.cdr%
  set abil %abil_list.car%
  set abil_list %abil_list.cdr%
  if !%actor.learned(%vnum%)%
    if %actor.ability(%abil%)%
      set any 1
    else
      set error 1
    end
  end
done
* how'd we do?
if %error% && !%any%
  %send% %actor% You don't have the right ability to learn anything new from @%self%.
  halt
elseif !%any%
  %send% %actor% There aren't any recipes you can learn from @%self%.
  halt
end
* ok go ahead and teach them
while %vnum_list%
  * pluck first numbers out
  set vnum %vnum_list.car%
  set vnum_list %vnum_list.cdr%
  if !%actor.learned(%vnum%)%
    nop %actor.add_learned(%vnum%)%
  end
done
* and report...
%send% %actor% You study @%self% and learn: %name_list%
%echoaround% %actor% ~%actor% studies @%self%.
%purge% %self%
~
$
