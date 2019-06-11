#11500
Clicky Pen~
1 c 2
click~
if !%arg%
  %send% %actor% Click what?
  halt
end
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
%send% %actor% You click %self.shortdesc%... ah, that feels good.
%echoaround% %actor% %actor.name% makes an annoying clicky sound with %actor.hisher% pen.
nop %actor.gain_event_points(11500,1)%
~
#11505
Start Supplies for GoA quests~
2 u 0
~
if !%actor.inventory(11505)%
  %load% obj 11505 %actor%
  %send% %actor% You receive a list of supplies for the Guild.
end
~
#11513
small shipment of resources~
1 n 100
~
wait 0
* uses val0 (quantity of common items), val1 (quantity of rare items)
if !%self.carried_by%
  %purge% %self%
  halt
end
set pers %self.carried_by%
set prc %random.1000%
set amt %self.val0%
set vnum -1
if %prc% <= 51
  * copper bar
  set vnum 179
  set amt %self.val1%
elseif %prc% <= 124
  * lettuce oil
  set vnum 3362
elseif %prc% <= 197
  * rope
  set vnum 2038
elseif %prc% <= 270
  * lumber
  set vnum 125
elseif %prc% <= 343
  * pillar
  set vnum 129
elseif %prc% <= 416
  * tin ingot
  set vnum 167
elseif %prc% <= 489
  * nails
  set vnum 1306
elseif %prc% <= 562
  * obsidian
  set vnum 106
  set amt %self.val1%
elseif %prc% <= 635
  * rock
  set vnum 100
elseif %prc% <= 708
  * small leather
  set vnum 1356
elseif %prc% <= 781
  * block
  set vnum 105
elseif %prc% <= 854
  * bricks
  set vnum 257
elseif %prc% <= 927
  * fox fur
  set vnum 1352
  set amt %self.val1%
else
  * big cat pelt
  set vnum 1353
  set amt %self.val1%
end
if %amt% < 0 || %vnum% == -1
  halt
end
nop %pers.add_resources(%vnum%,%amt%)%
set obj %pers.inventory(%vnum%)%
if %obj%
  %send% %pers% You find %amt%x %obj.shortdesc% in %self.shortdesc%!
end
%purge% %self%
~
#11514
great shipment of resources~
1 n 100
~
wait 0
* uses val0 (quantity of common items), val1 (quantity of rare items)
if !%self.carried_by%
  %purge% %self%
  halt
end
set pers %self.carried_by%
set prc %random.1000%
set amt %self.val0%
set vnum -1
if %prc% <= 5
  * crystal seed (1 only)
  set vnum 600
  set amt 1
elseif %prc% <= 20
  * gold bar
  set vnum 173
  set amt %self.val1%
elseif %prc% <= 90
  * silver bar
  set vnum 172
  set amt %self.val1%
elseif %prc% <= 160
  * magewood
  set vnum 604
elseif %prc% <= 230
  * sparkfiber
  set vnum 612
elseif %prc% <= 300
  * manasilk
  set vnum 613
elseif %prc% <= 370
  * briar hide
  set vnum 614
elseif %prc% <= 440
  * absinthian leather
  set vnum 615
elseif %prc% <= 510
  * nocturnium
  set vnum 176
elseif %prc% <= 580
  * imperium
  set vnum 177
elseif %prc% <= 650
  * distilled whitegrass
  set vnum 1210
elseif %prc% <= 720
  * fiveleaf
  set vnum 1211
elseif %prc% <= 790
  * redthorn
  set vnum 1212
elseif %prc% <= 860
  * magewhisper
  set vnum 1213
elseif %prc% <= 930
  * daggerbite
  set vnum 1214
else
  * bileberry
  set vnum 1215
end
if %amt% < 0 || %vnum% == -1
  halt
end
nop %pers.add_resources(%vnum%,%amt%)%
set obj %pers.inventory(%vnum%)%
if %obj%
  %send% %pers% You find %amt%x %obj.shortdesc% in %self.shortdesc%!
end
%purge% %self%
~
#11520
start Pixy Pursuit quest~
2 u 0
~
* Give enchanted jars
%load% obj 11520 %actor% inv
~
#11521
Pixy Pursuit: catch~
1 c 2
catch~
* This is the command for capturing pixies for the Pixy Pursuit event
if %actor.fighting%
  %send% %actor% You can't do that while fighting!
  return 1
  halt
end
if %actor.carrying% >= %actor.maxcarrying%
  %send% %actor% You can't catch anything because your inventory is full!
  return 1
  halt
end
if !%arg%
  %send% %actor% What do you want to catch with %self.shortdesc%?
  return 1
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% They must have flown away when they saw you coming, because they're not here.
  return 1
  halt
end
if !%target.is_npc%
  %send% %actor% You can really only catch pixies with those jars.
  return 1
  halt
end
if %target.fighting%
  %send% %actor% You can't catch someone who's in combat!
  return 1
  halt
end
* switch will validate the target vnum and set up chances: needs, has -- default needs is random.15 (attribute roll)
set needs %random.15%
set jar_vnum 11522
switch %target.vnum%
  case 615
    * feral pixy / enchanted forest (harder because common)
    set needs %random.500%
    set has %actor.level%
  break
  case 616
    * pixy queen / enchanted forest (easier because rare)
    set needs %random.200%
    set has %actor.level%
    set jar_vnum 11535
  break
  case 10042
    * pixy / skycleave
    set needs %random.200%
    set has %actor.level%
  break
  case 11520
    * royal pixy
    set needs %random.400%
    set has %actor.level%
    set jar_vnum 11535
  break
  case 11521
    * colt/str pixy
    set has %actor.strength%
  break
  case 11522
    * mouse/dex pixy
    set has %actor.dexterity%
  break
  case 11523
    * steam/cha pixy
    set has %actor.charisma%
  break
  case 11524
    * golden/grt pixy
    set has %actor.greatness%
  break
  case 11525
    * owl/int pixy
    set has %actor.intelligence%
  break
  case 11526
    * fox/wits pixy
    set has %actor.wits%
  break
  default
    * all other vnums
    %send% %actor% That doesn't appear to be a pixy.
    halt
  break
done
%send% %actor% You swoop toward %target.name% with an enchanted jar...
%echoaround% %actor% %actor.name% swoops toward %target.name% with an enchanted jar...
* short wait, then re-validate
wait 2 sec
if !%target%
  %send% %actor% The pixy got away!
  return 1
  halt
end
if %target.fighting% || %actor.fighting% || %actor.room% != %target.room% || %actor.disabled%
  * fail with no message (reason should be obvious)
  return 1
  halt
end
if %has% >= %needs%
  * Success!
  %load% obj %jar_vnum% %actor% inv
  %send% %actor% You catch %target.name% in a jar!
  %echoaround% %actor% %actor.name% catches %target.name% in a jar!
else
  * Fail
  %load% obj 11521 %actor% inv
  %send% %actor% You miss and %target.name% gets away, leaving behind a little pixy dust.
  %echoaround% %actor% %actor.name% misses %target.name%, who gets away!
end
* Purge the target either way
%purge% %target%
return 1
~
#11522
Pixy Hunt: exchange with alchemist~
1 c 2
exchange~
set alch 0
set pers %actor.room.people%
* Find the alchemist
while %pers% && !%alch%
  if %pers.is_npc% && %pers.vnum% == 231
    set alch %pers%
  end
  set pers %pers.next_in_room%
done
if !%alch%
  %send% %actor% You can only exchange pixies with the Alchemist.
  return 1
  halt
end
if %alch.fighting% || %alch.disabled%
  %send% %actor% The alchemist can't help you right now!
  return 1
  halt
end
* Okay, the alchemist is valid, now look for items to turn in
set value 0
set item %actor.inventory%
while %item%
  set next_item %item.next_in_list%
  if %item.vnum% == 11521 || %item.vnum% == 11522 || %item.vnum% == 11535
    eval value %value% + %item.val0%
    %purge% %item%
  end
  * otherwise it's not a pixy
  set item %next_item%
done
if %value% == 0
  %send% %actor% You don't have any jarred pixies or dust to exchange.
else
  %send% %actor% %alch.name% gives you %value% points for all your jarred pixies and pixy dust.
  %echoaround% %actor% %actor.name% exchanges jarred pixies and pixy dust with %alch.name%.
  nop %actor.gain_event_points(11520,%value%)%
end
~
#11523
Pixy Pursuit: spawn on move~
0 i 100
~
* ensure master present and event running
set master %self.master%
if !%master% || !%event.running(11520)%
  halt
end
if %master.room% != %self.room% || %master.is_npc%
  halt
end
* short delay
wait 5
* Check room for the item that blocks spawns
if %self.room.contents(11523)%
  halt
end
* check for last-spawn timer
if %master.varexists(last_11520_spawn)%
  eval timesince %timestamp% - %master.last_11520_spawn%
else
  * Fake 120 seconds since last spawn
  set timesince 120
end
* Random chance to fail and do nothing (chance drops with time)
if %random.10% > 1 && %timesince% < %random.180%
  halt
end
* Figure out which pixy to spawn
if %master.room.template% > 0 && %random.5% == 5
  * royal pixy, only in adventures
  set vnum 11520
else
  eval vnum 11520 + %random.6%
end
%load% mob %vnum%
set pix %master.room.people%
if %pix.vnum% == %vnum%
  switch %random.4%
    case 1
      %echo% You've found %pix.name%!
    break
    case 2
      %echo% %pix.name% appears out of nowhere!
    break
    case 3
      %echo% You spot %pix.name%, who immediately tries to fly away!
    break
    case 4
      %echo% %pix.name% flies down from above!
    break
  done
end
* store last spawn time
set last_11520_spawn %timestamp%
remote last_11520_spawn %master.id%
* block next spawn
%load% obj 11523 room
* try to move
%force% %pix% mmove
~
#11524
Pixy Pursuit combat script~
0 k 100
~
wait 1
if %self.disabled%
  halt
end
* Randomly morph the actor
if %actor.is_pc% && %random.5% == 5 && %actor.morph% != 11528
  %send% %actor% %self.name% casts a spell and shrinks you to the size of a mouse! (type 'morph normal' to return to full size)
  %echoaround% %actor% %self.name% casts a spell at %actor.name% and shrinks %actor.himher% to the size of a mouse!
  %morph% %actor% 11528
  halt
end
* otherwise 80% chance of vanishing
if %random.5% != 5
  %echo% %self.name% vanishes into thin air!
  %purge% %self%
  halt
end
~
#11525
Pixy Pursuit: check end~
1 b 33
~
* Ensure event is still running / lose jars and drop quest if not
if !%event.running(11520)% || !%self.carried_by%
  %send% %self.carried_by% %event.name(11520)% has ended and your enchanted pixy jars vanish!
  %quest% %self.carried_by% drop 11520
  rdelete last_11520_spawn %self.carried_by.id%
  %purge% %self%
  halt
end
~
#11526
Enchanted rainbow~
1 n 100
~
wait 1
%load% veh 11526
set veh %self.room.vehicles%
if %veh% && %veh.vnum% == 11526
  if %self.carried_by% && !%veh.empire_id%
    %own% %veh% %self.carried_by.empire%
  end
  %echo% %veh.shortdesc% appears from the sky!
  if %self.carried_by%
    %send% %self.carried_by% Be sure you don't lose it!
  end
end
%purge% %self%
~
#11527
Pixy House interior~
2 o 100
~
eval hall %%room.%room.enter_dir%(room)%%
* Add hallway
if !%hall%
  %door% %room% %room.enter_dir% add 11528
end
eval hall %%room.%room.enter_dir%(room)%%
if !%hall%
  * Failed to add
  halt
end
* Add bedroom
eval bed %%hall.%room.enter_dir%(room)%%
if !%bed%
  %door% %hall% %room.enter_dir% add 11531
end
* Add workshop
set edir %room.bld_dir(east)%
eval workshop %%hall.%edir%(room)%%
if !%workshop%
  %door% %hall% %edir% add 11529
end
* Add kitchen
set wdir %room.bld_dir(west)%
eval kitch %%hall.%wdir%(room)%%
if !%kitch%
  %door% %hall% %wdir% add 11530
end
* attempt to morph anyone here
set ch %room.people%
while %ch%
  if (%ch.is_pc% && !%ch.nohassle% && %ch.morph% != 11528)
    %send% %ch% You shrink to tiny size! (type 'morph normal' to return to full size)
    %echoaround% %ch% %ch.name% shrinks to tiny size!
    %morph% %ch% 11528
  end
  set ch %ch.next_in_room%
done
* And remove this trigger
detach 11527 %room.id%
~
#11528
Tiny Morph on Enter~
2 g 100
~
if %actor.is_npc% || %actor.nohassle% || !%room.complete%
  halt
end
if %actor.morph% == 11528
  halt
end
%send% %actor% You shrink to tiny size! (type 'morph normal' to return to full size)
%echoaround% %actor% %actor.name% shrinks to tiny size!
%morph% %actor% 11528
~
#11529
Block morph~
2 c 0
morph fastmorph~
if %actor.nohassle% || !%room.complete%
  halt
end
%send% %actor% You can't seem to morph here!
~
#11530
Pixy Hunt: Whistle Hog~
1 c 2
whistle~
if %arg% != hog && %arg% != pig
  return 0
  halt
end
set ch %actor.room.people%
set found 0
* Check only 1
while %ch% && !%found%
  if %ch.is_npc% && %ch.vnum% == 11527 && %ch.master% == %actor%
    set found 1
  end
  set ch %ch.next_in_room%
done
if %found%
  %send% %actor% You already have a sprite-sniffing hog.
  return 1
  halt
end
* Load it
%load% mob 11527
set hog %actor.room.people%
if %hog% && %hog.vnum% == 11527 && !%hog.master%
  %force% %hog% mfollow %actor%
  %echo% %hog.name% comes trotting up with its belly flapping side to side!
  nop %hog.unlink_instance%
end
return 1
~
#11534
Upgrade pixy hunt gear~
1 c 2
upgrade~
if !%arg%
  %send% %actor% Use %self.shortdesc% on what? (only works on elf-made leather shoes and wishing bags)
  halt
end
set target %actor.obj_target_inv(%arg%)%
if !%target%
  %send% %actor% You don't seem to have a '%arg%'. (You can only use %self.shortdesc% on items in your inventory.)
  halt
end
if %target.vnum% != 11531 && %target.vnum% != 11533
  %send% %actor% You can only use %self.shortdesc% on elf-made leather shoes and wishing bags.
  halt
end
if %target.is_flagged(SUPERIOR)%
  %send% %actor% %target.shortdesc% is already upgraded; using %self.shortdesc% would have no benefit.
  halt
end
%send% %actor% You use %self.shortdesc% on %target.shortdesc%...
%echoaround% %actor% %actor.name% uses %self.shortdesc% on %target.shortdesc%...
%echo% %target.shortdesc% takes on a faint glow and floral smell... and looks a LOT better.
nop %target.flag(SUPERIOR)%
%scale% %target% %target.level%
%purge% %self%
~
$
