#16600
make the snowman~
1 c 2
scoop make~
if !%arg%
  return 0
  halt
end
if !(snowball /= %arg%) && !(snowman /= %arg%)
  return 0
  halt
end
if %actor.cooldown(16600)%
  %send% %actor% Your %cooldown.16600% is on cooldown!
  halt
end
set player_name %actor.firstname%
if %cmd% == make
  if (snowman /= %arg%) && %actor.has_resources(16605,3)%
    set BuildRoom %self.room%
    if %BuildRoom.sector_flagged(FRESH-WATER)% || %BuildRoom.sector_flagged(ocean)% || %BuildRoom.sector_flagged(SHALLOW-WATER)%
      %send% %actor% If you built a snowman in the water, it would probably just wash away before you could stack the snowballs.
      halt
    end
    %send% %actor% You begin to build a snowman.
    %echoaround% %actor% %player_name% begins to build a snowman.
    wait 5 s
    if !%actor.fighting% && %BuildRoom% == %self.room%
      %send% %actor% You stack the snowballs you've collected on top of one another carefully, making sure they don't fall.
      %echoaround% %actor% %player_name% stacks the snowballs on top of each other.
    else
      %send% %actor% You've stopped building the snowman.
      halt
    end
    wait 5 s
    if !%actor.fighting% && %BuildRoom% == %self.room%
      %send% %actor% You meticulously stick the facial features in place.
      %echoaround% %actor% %player_name% meticulously presses the facial features into the snowball head.
    else
      %send% %actor% You've stopped building the snowman.
      halt
    end
    wait 5 s
    if !%actor.fighting% && %BuildRoom% == %self.room%
      %send% %actor% As you place @%self% on top, the snowman wiggles as though it is going to fall over, but then begins to dance around!
      %echoaround% %actor% As %player_name% places @%self% on top, the snowman wiggles as though it is going to fall over, but then begins to dance around!
      %load% mob 16600 %actor.level%
      %quest% %actor% trigger 16606
      set snowman %self.room.people%
      %mod% %snowman% shortdesc a snow %player_name%
      %mod% %snowman% longdesc A snow %player_name% dances around in a circle.
      %mod% %snowman% keywords snowman %player_name%
      set IWasBornOn %dailycycle%
      remote IWasBornOn %snowman.id%
      if %actor.quest_finished(16606)%
        %quest% %actor% finish 16606
      end
      halt
    else
      %send% %actor% You've stopped building the snowman.
      halt
    end
  elseif (snowman /= %arg%) && !(snow /= %arg%)
    %send% %actor% You don't have enough snowballs yet.
    halt
  end
end
if %cmd% == scoop && !(snowball /= %arg%)
  %send% %actor% You can only make snowballs when scooping.
  halt
end
if %actor.has_resources(16605,3)%
  %send% %actor% You already have three snowballs, just make the snowman.
  halt
end
set room %self.room%
if %room.function(DRINK-WATER)% || %room.sector_flagged(DRINK)%
  %send% %actor% You dip @%self% into the water and it freezes the liquid into snow.
  %echoaround% %actor% %player_name% dips @%self% into the water and it freezes the liquid into snow.
else
  %send% %actor% You can't find snow worthy water here. Try somewhere you can drink fresh water.
  halt
end
set BuildRoom %self.room%
wait 5 s
if !%actor.fighting% && %BuildRoom% == %self.room%
  %send% %actor% You shape the newly made snow into a perfect snowball for your snowman.
  %echoaround% %actor% %player_name% shapes the newly made snow into a perfect snowball for ^%actor% snowman.
  %load% obj 16605 %actor% inv
  nop %actor.set_cooldown(16600, 30)%
else
  %send% %actor% You've stopped working on a snowball.
end
~
#16601
steal victim's blood~
1 c 2
use~
if !%arg%
  return 0
  halt
end
if !(%actor.obj_target(%arg.car%)% == %self%)
  return 0
  halt
end
if !%actor.on_quest(16602)% && !%actor.on_quest(16603)%
  %send% %actor% You don't need @%self%, might as well just toss it.
else
  set target %actor.char_target(%arg.cdr%)%
  set OnQuest 0
  if %actor.on_quest(16602)%
    if !(%target.vnum% == 10703)
      %send% %actor% That isn't the right blood!
      halt
    else
      set OnQuest 16602
    end
  elseif %actor.on_quest(16603)%
    if %target.vnum% != 10700 && %target.vnum% != 10705 && %target.vnum% != 9175 && %target.vnum% != 16652
      %send% %actor% That isn't the right blood!
      halt
    else
      set OnQuest 16603
    end
  end
  %send% %actor% You use all of your vampire know-how to prick |%target% neck and hide the vial of blood before anyone notices.
  %echoaround% %actor% %actor% does something near %target%, but it happens so fast you can't tell what.
end
if %OnQuest% > 0
  %send% %actor% A voice whispers, "thanks for the blood,' as the syringe vanishes from your hand.
  %quest% %actor% finish %OnQuest%
  %purge% %self%
end
~
#16602
start a winter holiday quest~
2 u 0
~
switch %questvnum%
  case 16607
    %load% obj 16608 %actor% inv
  break
  case 16602
    %load% obj 16601 %actor% inv
  break
  case 16603
    %load% obj 16601 %actor% inv
  break
  case 16604
    %load% obj 16604 %actor% inv
  break
  case 16605
    %load% obj 16604 %actor% inv
  break
  case 16606
    %load% obj 16600 %actor% inv
  break
  case 16607
    %load% obj 16608 %actor% inv
  break
  case 16610
    %load% obj 16610 %actor% inv
  break
  case 16611
    %load% obj 16611 %actor% inv
  break
  case 16613
    %load% obj 16613 %actor% inv
  break
  case 16618
    %load% obj 16618 %actor% inv
  break
  case 16617
    %load% obj 16625 %actor% inv
  break
  case 16620
    %load% obj 16620 %actor% inv
  break
  case 16626
    %load% obj 16627 %actor% inv
  break
  case 16628
    %load% obj 16629 %actor% inv
    set activate 1
    set SnowmanInRoom %self.people%
    while %activate%
      if %SnowmanInRoom.is_npc% && %SnowmanInRoom.vnum% == 16600
        set activate 0
      else
        set SnowmanInRoom %SnowmanInRoom.next_in_room%
      end
    done
    set SnowmanInRoomID %SnowmanInRoom.id%
    detach 16617 %SnowmanInRoomID%
    attach 16630 %SnowmanInRoomID%
    attach 16633 %SnowmanInRoomID%
    %teleport% %SnowmanInRoom% %self%
    %force% %actor% follow snowman
    set PlayerOnAbominableQuest %actor%
    remote PlayerOnAbominableQuest %SnowmanInRoomID%
  break
  case 16676
    %load% obj 16676 %actor% inv
  break
  case 16677
    %load% obj 16677 %actor% inv
  break
  case 16690
    %load% obj 16690 %actor% inv
  break
  case 16680
    %load% obj 16680 %actor% inv
  break
  case 16643
    %load% mob 16643
  break
  case 16644
    %load% mob 16644
  break
  case 16660
    %load% obj 16616 %actor% inv
  break
done
~
#16603
winter boss deaths~
0 f 100
~
return 0
switch %self.vnum%
  case 16613
    %echo% ~%self% throws ornaments everywhere and vanishes in the chaos!
  break
  case 16680
  break
done
set person %self.room.people%
while %person%
  %quest% %person% trigger 16613
  %quest% %person% trigger 16680
  if %person.is_pc% && %person.quest_finished(16613)%
    %quest% %person% finish 16613
  end
  if %person.is_pc% && %person.quest_finished(16680)%
    %quest% %person% finish 16680
  end
  set person %person.next_in_room%
done
~
#16604
post a letter to father christmas~
1 c 2
post~
if !(%actor.obj_target(%arg%)% == %self%)
  return 0
  halt
end
set CarriedBy %self.carried_by%
if !%self.room.function(mail)%
  %send% %CarriedBy% You can't mail anything here.
  halt
end
if %CarriedBy.on_quest(16604)%
  set questnum 16604
elseif %CarriedBy.on_quest(16605)%
  set questnum 16605
else
  %send% %CarriedBy% Why have you stolen a child's letter to Father Christmas? Have you no shame?
  wait 1
  %send% %CarriedBy% The letter magically vanishes from your hands.
  %purge% %self%
  halt
end
if %questnum%
  if %CarriedBy.quest_finished(%questnum%)%
    %quest% %CarriedBy% finish %questnum%
    %purge% %self%
  else
    %send% %CarriedBy% You don't meet the requirements to finish the quest.
  end
end
~
#16605
grinchy buff~
0 l 20
~
if %self.cooldown(16605)%
  halt
end
nop %self.set_cooldown(16605, 300)%
set grinch_level 0
if %self.mob_flagged(hard)%
  eval grinch_level %grinch_level% + 1
end
if %self.mob_flagged(group)%
  eval grinch_level %grinch_level% + 2
end
if %grinch_level% == 0
  halt
end
set SelfLevel %self.level%
eval grinch_dam %SelfLevel% / 15 * ( %grinch_level% + 1 )
%echo% ~%self% shoves christmas fudge in ^%self% face.
wait 3 s
say Is that all you got!
dg_affect #16606 %self% BONUS-PHYSICAL %grinch_dam% -1
dg_affect #16606 %self% DODGE %SelfLevel% -1
~
#16606
plant the christmas tree~
1 c 2
plant~
* Check if they're interacting with the fallen xmas tree object or stand.
set targ %actor.obj_target(%arg%)%
if (%targ% != %self% && %targ.vnum% != 16602)
  return 0
  halt
end
set room %self.room%
* See if they're trying to set it up in the city center or not.
if %room.building_vnum% != 5009
  %send% %actor% You can only set up your Christmas tree in a city center.
  halt
end
set act_emp %actor.empire%
set act_emp_id %act_emp.id%
* Make sure they're setting it up in their own empire's city center.
if !(%act_emp% == %room.empire%)
  %send% %actor% This doesn't seem to be your empire.
  halt
end
* check if the empire has a tree here already.
set old_xmas_tree %room.contents(16607)%
if %old_xmas_tree%
  %send% %actor% Your empire already has a Christmas tree in this city center.
  set light_with_mind %actor.has_tech(Light-Fire)%
  set use_a_lighter %actor.find_lighter%
  if !%light_with_mind% && !%use_a_lighter%
    %send% %actor% And you don't have any way to burn the old tree.
    halt
  elseif %use_a_lighter% && !%light_with_mind%
    nop %use_a_lighter.used_lighter(%actor%)%
  end
  %send% %actor% You light @%old_xmas_tree% on fire!
  %echoaround% %actor% ~%actor% lights @%old_xmas_tree% on fire!
  %load% obj 1002 room
  %purge% %old_xmas_tree%
end
* see if they have a xmas tree stand to hold up the tree.
if !%actor.inventory(16602)%
  %send% %actor% You should probably craft a Christmas tree stand first.
  halt
end
%load% obj 16607 room
%quest% %actor% trigger 16607
set xmas_tree %room.contents(16607)%
* update descriptions? each entry must end in a colon
set unimpressive_sects 4: 26: 45: 54: 71:
set giant_sects 28: 55:
set spruce_sects 10562: 10563: 10564: 10565:
set magic_sects 602: 603: 604: 612: 613: 614: 16698: 16699:
if %self.varexists(winter_holiday_sect_check)%
  set compare %self.winter_holiday_sect_check%:
  if %spruce_sects% ~= %compare%
    %mod% %xmas_tree% keywords tree spruce Christmas tall
    %mod% %xmas_tree% shortdesc a spruce Christmas tree
    %mod% %xmas_tree% longdesc A tall spruce Christmas tree cheers the citizens.
  elseif %magic_sects% ~= %compare%
    %mod% %xmas_tree% keywords tree enchanted Christmas tall
    %mod% %xmas_tree% shortdesc an enchanted Christmas tree
    %mod% %xmas_tree% longdesc A tall enchanted Christmas tree cheers the citizens.
  elseif %unimpressive_sects% ~= %compare%
    %mod% %xmas_tree% keywords tree Christmas unimpressive tall
    %mod% %xmas_tree% shortdesc an unimpressive Christmas tree
    %mod% %xmas_tree% longdesc A tall but unimpressive Christmas tree cheers the citizens.
  elseif %giant_sects% ~= %compare%
    %mod% %xmas_tree% keywords tree Christmas enormous
    %mod% %xmas_tree% shortdesc an enormous Christmas tree
    %mod% %xmas_tree% longdesc An enormous Christmas tree cheers the citizens.
  end
end
* See if the player's empire has the twelve trees of xmas progress or not.
if !%act_emp.has_progress(16600)%
  * See if the empire has a tree count already.
  if !%act_emp.varexists(empire_trees)%
    set empire_trees 1
    set xmas_trees_planted %dailycycle%
    remote xmas_trees_planted %act_emp_id%
    remote empire_trees %act_emp_id%
  else
    eval SinceTrees %dailycycle% - %act_emp.xmas_trees_planted%
    * Has it been a full day since the last tree counted toward this progress?
    if %SinceTrees% >= 1
      set xmas_trees_planted %dailycycle%
      remote xmas_trees_planted %act_emp_id%
      eval empire_trees %act_emp.empire_trees% + 1
      remote empire_trees %act_emp_id%
    end
  end
  * See if the empire have succeeded in the twelve full days.
  if %act_emp.empire_trees% >= 12
    rdelete empire_trees %act_emp_id%
    rdelete xmas_trees_planted %act_emp_id%
    nop %act_emp.add_progress(16600)%
  end
end
if %actor.quest_finished(16607)%
  %quest% %actor% finish 16607
end
~
#16607
setting the sect_check variable~
0 e 1
swings~
set winter_holiday_sect_check %self.room.sector_vnum%
remote winter_holiday_sect_check %self.id%
~
#16608
xmas tree chopping~
1 c 2
chop~
* config valid sects (must also update trig 16609)
set valid_sects 4 26 28 45 54 55 71 602 603 604 612 613 614 10562 10563 10564 10565 16698 16699
return 0
if %actor.inventory(16606)%
  %send% %actor% You really should get this tree back to your city center and plant it.
  halt
end
set winter_holiday_sect_check %self.room.sector_vnum%
if !(%valid_sects% ~= %winter_holiday_sect_check%)
  halt
end
eval maxcarrying %actor.maxcarrying% - 1
if %actor.carrying% >= %maxcarrying%
  %send% %actor% Maybe you should unload before trying to chop down a Christmas tree?
  halt
end
set person %self.room.people%
while %person%
  if %person.vnum% == 16609
    set elf_count 1
  end
  set person %person.next_in_room%
done
if !%elf_count%
  %load% mob 16609
end
~
#16609
xmas tree replacer~
0 e 1
collects~
if !%actor.on_quest(16607)% || %actor.inventory(16606)% || %actor.carrying% >= (%actor.maxcarrying% - 1)
  * exit early if not on quest or already has tree, or inventory full
  halt
end
* attempt to determine if they just got a tree
if (%arg% ~= tree. || %arg% ~= wood.)
  set tree_type %actor.inventory()%
  set tree_type %tree_type.vnum%
end
set winter_holiday_sect_check %self.winter_holiday_sect_check%
set valid_tree_types 10558 603 120 122 128 618 16697
if !(%valid_tree_types% ~= %tree_type%)
  * not a valid tree
  halt
end
* chances of success: each entry should end in a colon
set 30_percent_sects 4: 26: 71: 10562: 10563:
set 40_percent_sects 28: 45: 54: 55: 602: 612: 10564:
set 50_percent_sects 603: 613:
set 60_percent_sects 604: 614: 10565:
set 100_percent_sects 16698: 16699:
set compare %winter_holiday_sect_check%:
if %30_percent_sects% ~= %compare%
  set chance 30
elseif %40_percent_sects% ~= %compare%
  set chance 40
elseif %50_percent_sects% ~= %compare%
  set chance 50
elseif %60_percent_sects% ~= %compare%
  set chance 60
elseif %100_percent_sects% ~= %compare%
  set chance 100
else
  switch %winter_holiday_sect_check%
    default
      * invalid sect
      halt
    break
  done
end
* determine if we have a valid tree
if %random.100% > %chance%
  * fail
  halt
end
* success
%purge% %actor.inventory(%tree_type%)%
%load% obj 16606 %actor% inv
set mod_tree %actor.inventory()%
remote winter_holiday_sect_check %mod_tree.id%
%force% %actor% stop
%send% %actor% You've found it! The perfect tree for your city center!
%echoaround% %actor% It looks perfect to be used as a Christmas tree!
~
#16610
make snow angel~
1 c 2
make~
if %arg% == angel
  %send% %actor% You have to make a snow angel specifically.
  halt
end
if !(snow angel /= %arg%)
  return 0
  halt
end
if %actor.cooldown(16610)%
  %send% %actor% You're still a little chilled from the last snow angel.
  halt
end
set room %self.room%
if %room.contents(16609)%
  %send% %actor% Someone has already made a snow angel here.
  halt
end
if %actor.fighting%
  %send% %actor% You can't do that while fighting!
  halt
end
if %actor.riding%
  %send% %actor% You can't do that while mounted.
  halt
end
if !%room.in_city%
  %send% %actor% You are supposed to be beautifying your city.
  halt
end
if !%room.is_outdoors%
  %send% %actor% You can only make a snow angel outside.
  halt
end
%send% %actor% You spread @%self%, lay down, and swiftly make a snow angel on the ground.
%echoaround% %actor% ~%actor% spreads @%self%, lays down, and swiftly makes a snow angel on the ground.
%load% obj 16609 room
nop %actor.set_cooldown(16610, 30)%
if !%self.varexists(angel_count)%
  set angel_count 1
else
  eval angel_count %self.angel_count% + 1
end
if %angel_count% < 5
  remote angel_count %self.id%
  eval angel_left 5 - %angel_count%
  switch %angel_count%
    case 1
      set angel_count One
    break
    case 2
      set angel_count Two
    break
    case 3
      set angel_count Three
    break
    case 4
      set angel_count Four
    break
  done
  switch %angel_left%
    case 1
      set angel_left one
    break
    case 2
      set angel_left two
    break
    case 3
      set angel_left three
    break
    case 4
      set angel_left four
    break
  done
  %send% %actor% %angel_count% down, %angel_left% to go!
else
  %quest% %actor% trigger 16610
  if %actor.quest_finished(16610)%
    %quest% %actor% finish 16610
  end
  %purge% %self%
end
~
#16611
stealthy gift giving~
1 c 2
sneak~
if !%actor.ability(sneak)%
  return 0
  halt
end
if !%arg%
  return 0
  halt
end
set MoveDir %actor.parse_dir(%arg.car%)%
set SelfRoom %self.room%
if !%SelfRoom.in_city%
  %send% %actor% While you have a gift to deliver, you should probably be sneaking around your own city.
  return 0
  halt
end
if %actor.cooldown(16611)%
  %send% %actor% You're drawing too much attention to yourself. You need to cool off.
  nop %actor.set_cooldown(16611, 30)%
  halt
end
if !%MoveDir%
  %send% %actor% That's not a valid direction.
  halt
end
eval to_room %%SelfRoom.%MoveDir%(room)%%
if !%to_room%
  %send% %actor% Sorry, you're not quite that stealthy. I think someone would notice the hole left behind.
  halt
end
set person %SelfRoom.people%
set CitizenCount 0
while %person%
  if %person.is_npc% && %person.mob_flagged(human)%
    eval CitizenCount %CitizenCount% + 1
  end
  set person %person.next_in_room%
done
if %CitizenCount% > 0
  eval sneak_out %actor.skill(stealth)% - %CitizenCount% * 10
  set sneak_check %random.99%
  if %sneak_out% < %sneak_check%
    %send% %actor% You were caught! You'll have to try again later.
    nop %actor.set_cooldown(16611, 5)%
    halt
  end
end
%force% %actor% %arg%
wait 1
set SecondaryRoom %self.room%
if %SelfRoom% == %SecondaryRoom%
  %send% %actor% Doesn't seem you managed to go anywhere.
  halt
end
if !%SecondaryRoom.function(bedroom)%
  %send% %actor% You're supposed to be leaving it in a citizen's bedroom, not somewhere anyone can find it.
  halt
end
set person %SecondaryRoom.people%
set CitizenCount 0
while %person%
  if %person.is_npc% && %person.mob_flagged(human)%
    eval CitizenCount %CitizenCount% + 1
  end
  set person %person.next_in_room%
done
if %CitizenCount% > 0
  %send% %actor% You made it in, but they'll see you place the gift. Have to try again later.
  nop %actor.set_cooldown(16611, 5)%
  halt
else
  %quest% %actor% trigger 16611
  if %actor.quest_finished(16611)%
    %quest% %actor% finish 16611
  end
end
~
#16612
pickpocket father xmas's hat~
0 p 100
~
if !(%abilityname%==pickpocket)
  halt
end
if !%actor.on_quest(16612)%
  return 1
  halt
else
  return 0
end
if %actor.inventory(16612)%
  %send% %actor% You already have his hat, why would you need to bother him again?
  halt
end
if %actor.cooldown(16601)% > 5
  %send% %actor% ~%self% is highly upset with you and tosses you out!
  %echoaround% %actor% ~%self% grabs ~%actor% and throws *%actor% out!
  %teleport% %actor% %instance.location%
  %force% %actor% look
  halt
elseif %actor.cooldown(16601)%
  %send% %actor% You probably shouldn't do that, ~%self% almost caught you once already.
  nop %actor.set_cooldown(16601, 30)%
  halt
end
set theft_roll %random.100%
if %actor.skill(stealth)% >= %theft_roll%
  %load% obj 16612 %actor% inv
  set item %actor.inventory()%
  %send% %actor% You find @%item%!
else
  nop %actor.set_cooldown(16601, 5)%
  %send% %actor% ~%self% turns in your direction and you have to abort your theft.
end
~
#16613
summon grinchy demon~
1 c 2
use~
if !%arg%
  return 0
  halt
end
if %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
set room %self.room%
if !%room.function(BEDROOM)% || !%room.in_city% || !%room.complete%
  %send% %actor% You can only do that in a bedroom.
  halt
end
set item %room.contents%
while %item%
  if %item.vnum% == 16614
    set FoundSense 1
  end
  set item %item.next_in_list%
done
if %FoundSense%
  %send% %actor% The rage of the last time you confronted the grinchy demon is still strong here.
  halt
end
set arg2 %arg.cdr%
if !%arg2%
  %send% %actor% What difficulty would you like to summon the Grinchy Demon at? (Normal, Hard, Group or Boss)
  return 1
  halt
end
set arg %arg2%
if normal /= %arg%
  %send% %actor% Setting difficulty to Normal...
  set diff 1
elseif hard /= %arg%
  %send% %actor% Setting difficulty to Hard...
  set diff 2
elseif group /= %arg%
  %send% %actor% Setting difficulty to Group...
  set diff 3
elseif boss /= %arg%
  %send% %actor% Setting difficulty to Boss...
  set diff 4
else
  %send% %actor% That is not a valid difficulty level for this encounter.
  halt
  return 1
end
set cycles_left 3
while %cycles_left% >= 0
  if (%actor.room% != %room%) || !%actor.can_act%
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 3
      %echoaround% %actor% |%actor% summoning is interrupted.
      %send% %actor% Your summoning is interrupted.
    else
      %send% %actor% You can't do that now.
    end
    halt
  end
  switch %cycles_left%
    case 3
      %echo% You start to look for a good hiding place to observe the room from.
    break
    case 2
      %echo% A thump, a bump, and a clunk can be heard.
    break
    case 1
      %echo% A shadow of someone or something tall can be seen in the middle of the room.
    break
    case 0
      * Leave the loop
    break
  done
  if %cycles_left% > 0
    wait 5 sec
  end
  eval cycles_left %cycles_left% - 1
done
%load% mob 16613
%load% obj 16614 room
set mob %room.people%
if %mob.vnum% != 16613
  %echo% Something went wrong...
  halt
end
%echo% ~%mob% sidles into view and you rush forward with @%self%!
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
remote diff %mob.id%
%echo% @%self% bursts into blue flames and rapidly crumbles to ash.
%purge% %self%
~
#16614
Winter Wonderland: Grinchy demon combat 1~
0 bw 40
~
if !%self.fighting% || %self.cooldown(16617)%
  halt
end
set diff %self.var(diff,1)%
set room %self.room%
set person %room.people%
set mob %room.people(16614)%
if %mob%
  * already have a dog
  halt
elseif %diff% == 1
  * normal = no summon
  nop %self.set_cooldown(16617, 90)%
  halt
end
set grinch_level 0
if %self.mob_flagged(hard)%
  eval grinch_level %grinch_level% + 1
end
if %self.mob_flagged(group)%
  eval grinch_level %grinch_level% + 2
end
set grinch_roll %random.100%
if %diff% == 2 && %grinch_roll% > 30
  * chance to fail on hard
  halt
elseif %diff% == 3 && %grinch_roll% > 60
  * chance to fail on group
  halt
end
%echo% &&G&&Z~%self% shouts, "get 'em Max!"&&0
%load% mob 16614 ally
nop %self.set_cooldown(16617, 90)%
~
#16615
Winter Wonderland: Grinchy combat 2~
0 k 25
~
set grinch_level 0
if %self.mob_flagged(hard)%
  eval grinch_level %grinch_level% + 1
end
if %self.mob_flagged(group)%
  eval grinch_level %grinch_level% + 2
end
if %grinch_level% == 0
  halt
end
switch %random.5%
  case 1
    set grinch_gift_is neck ties?
  break
  case 2
    set grinch_gift_is A Christmas sweater?
  break
  case 3
    set grinch_gift_is Fluffy woolen socks?
  break
  case 4
    set grinch_gift_is Is that a pet rock?
  break
  case 5
    set grinch_gift_is A segmented thirty-nine-and-a-half foot pole?
  break
done
switch %random.3%
  case 1
    if !%self.cooldown(16615)%
      say %grinch_gift_is%
      if %grinch_level% == 1
        %send% %actor% ~%self% pulls out a gift and throws it at you!
        %echoaround% %actor% ~%self% pulls out a gift and throws it at ~%actor%!
        %damage% %actor% 100
      elseif %grinch_level% == 2
        set grinch_target %random.enemy%
        if !%grinch_target%
          set grinch_target %actor%
        end
        if %grinch_target%
          %send% %grinch_target% ~%self% pulls out a gift and throws it at you!
          %echoaround% %grinch_target% ~%self% pulls out a gift and throws it at ~%grinch_target%!
          %damage% %grinch_target% 100
        end
      elseif %grinch_level% == 3
        %echo% ~%self% pulls out a gift and throws it on the ground, causing everyone to go flying!
        %aoe% 100
      end
      nop %self.set_cooldown(16615, 30)%
    end
  break
  case 2
    if !%self.cooldown(16613)%
      if %grinch_level% == 1
        set grinch_timer 15
      elseif %grinch_level% == 2 || %grinch_level% == 3
        set grinch_timer 30
      end
      nop %self.set_cooldown(16613, 30)%
      if %grinch_level% != 0
        %send% %actor% ~%self% glares at you and you shiver in fear.
        %echoaround% %actor% ~%self% glares at ~%actor% and &%actor% shudders in fear.
        dg_affect #16612 %actor% dodge -%actor.level% %grinch_timer%
      end
    end
  break
  case 3
    if !%self.cooldown(16614)%
      nop %self.set_cooldown(16614, 45)%
      set running 1
      remote running %self.id%
      %echo% ~%self% starts to swing a thirty-nine-and-a-half foot pole.
      %echo% &&YYou'd better duck!&&0
      wait 10 sec
      %echo% ~%self% swings the pole like a bat!
      set running 0
      remote running %self.id%
      set room %self.room%
      set person %room.people%
      while %person%
        if %person.is_pc%
          if %person.health% <= 0
            * Lying down is basically ducking right?
            set command 1
          else
            set command %self.varexists(command_%person.id%)%
            if %command%
              eval command %%self.command_%person.id%%%
              rdelete command_%person.id% %self.id%
            end
          end
          if %command% != duck
            %send% %person% &&rYou are knocked senseless by the thirty-nine-and-a-half foot pole!
            %echoaround% %person% ~%person% is knocked senseless by the thirty-nine-and-a-half foot pole!
            eval grinch_damage %grinch_level% * 50 + 20
            %damage% %person% %grinch_damage% physical
            dg_affect #16616 %person% STUNNED on 5
          else
            %send% %person% You limbo under the pole and safely straighten back up.
            %echoaround% %person% ~%person% safely limbos under the pole.
          end
        end
        set person %person.next_in_room%
      done
    end
  break
done
~
#16616
grinchy fight commands~
0 c 0
duck~
if !%self.varexists(running)%
  return 0
  halt
end
if !%self.running%
  %send% %actor% You don't need to do that right now.
  return 1
  halt
end
if %self.varexists(command_%actor.id%)%
  eval current_command %%self.command_%actor.id%%%
  if %current_command% /= %cmd%
    %send% %actor% You already did.
    halt
  end
end
set command_%actor.id% duck
%send% %actor% You stand your ground and prepare to duck...
%echoaround% %actor% ~%actor% stands ^%actor% ground and prepares to duck...
remote command_%actor.id% %self.id%
~
#16617
melting snowman~
0 i 100
~
wait 1 s
if !%self.room.in_city%
  %echo% ~%self% melts into a puddle from the heat.
  %purge% %self%
end
if !%self.varexists(melt_counter)%
  set melt_counter 0
else
  set melt_counter %self.melt_counter%
end
if !%self.room.is_outdoors%
  set AddMelt 3
else
  eval AddMelt %random.2% - 1
end
eval melt_counter %melt_counter% + %AddMelt%
if %melt_counter% >= 20
  %echo% ~%self% melts into a puddle from the heat.
  %purge% %self%
else
  remote melt_counter %self.id%
end
~
#16618
winter dress up~
1 c 2
dress~
if !%arg%
  %send% %actor% Who are you trying to dress?
  halt
end
if !%actor.on_quest(16617)% && !%actor.on_quest(16618)%
  %purge% %self%
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% You don't see them here.
  halt
end
if %target.morph%
  %send% %actor% Looks like they're already dressed up.
  halt
end
if %actor.on_quest(16617)%
  set questnum 16617
  if %target.vnum% != 223
    %send% %actor% You're looking for a terrier to dress up.
    halt
  elseif %actor.empire% != %target.empire%
    %send% %actor% You probably shouldn't dress up a terrier if it doesn't belong to your own empire.
    halt
  else
    set morphnum 16617
    if !%self.varexists(dress_up_counter)%
      set dress_up_counter 2
    else
      set dress_up_counter %self.dress_up_counter%
    end
  end
end
if %actor.on_quest(16618)%
  set questnum 16618
  set horse_list horse warhose
  set camel_list camel warcamel
  if %target.is_pc% || !(%horse_list% ~= %target.pc_name.car% || %camel_list% ~= %target.pc_name.car%)
    %send% %actor% You're going to need a horse or camel for this outfit.
    halt
  else
    * determine morph num
    if (%horse_list% ~= %target.pc_name.car%)
      set morphnum 16618
    else
      set morphnum 16619
    end
    if !%self.varexists(dress_up_counter)%
      set dress_up_counter 5
    else
      set dress_up_counter %self.dress_up_counter%
    end
  end
end
%send% %actor% You dress ~%target% with @%self%.
%echoaround% %actor% ~%actor% dresses ~%target% with @%self%.
%morph% %target% %morphnum%
eval dress_up_counter %dress_up_counter% - 1
if %dress_up_counter% > 0
  remote dress_up_counter %self.id%
else
  %quest% %actor% trigger %questnum%
end
if %actor.quest_finished(%questnum%)%
  %quest% %actor% finish %questnum%
  %purge% %self%
end
~
#16619
Max the reindeer dog no-death~
0 f 100
~
return 0
%echo% ~%self% yelps and runs off before you can stop him!
~
#16620
hanging winter holiday ornaments~
1 c 2
hang~
if !(%actor.obj_target(%arg%)% == %self%)
  return 0
  halt
end
set room %self.room%
if !%room.in_city% || !%room.is_outdoors%
  %send% %actor% You need to decorate the outdoor areas of your city.
  halt
end
if %room.contents(16621)% || %room.contents(16622)% || %room.contents(16623)% || %room.contents(16624)%
  %send% %actor% Someone has already put ornaments up here. You should probably find somewhere else.
  halt
end
if !%self.varexists(ornament_counter)%
  set ornament_counter 1
else
  eval ornament_counter %self.ornament_counter% + 1
end
switch %ornament_counter%
  case 1
    %send% %actor% You begin your decorating by stringing strands of garland all about the area.
    %echoaround% %actor% ~%actor% begins ^%actor% decorating by stringing strands of garland all about the area.
    %load% obj 16621 room
  break
  case 2
    %send% %actor% You decide it's time to place the candy cane hanging somewhere, and here looks like the perfect spot.
    %echoaround% %actor% As you watch, ~%actor% strings up a candy cane hanging as part of ^%actor% efforts to prepare for the winter holiday.
    %load% obj 16622 room
  break
  case 3
    %send% %actor% You carefully carry the faerie lantern to the center of the area and place it on the ground, then back away and admire the bright illumination it gives off.
    %echoaround% %actor% ~%actor% slowly walks to the center of the area with a faerie lantern in ^%actor% hands. As &%actor% backs away, you can't help but admire the illumination it gives off.
    %load% obj 16623 room
  break
  case 4
    %send% %actor% You lift the reindeer hoofprint stamp and begin to deliver hammerblows to the ground, leaving behind realistic tracks, giving the impression there was a live reindeer here.
    %echoaround% %actor% ~%actor% takes out a reindeer hoofprint stamp and begins delivering hammerblows to the ground. The end result is some realistic tracks, giving the impression there was a live reindeer here.
    %load% obj 16624 room
    %quest% %actor% trigger 16620
  break
done
if %ornament_counter% >= 4 && %actor.quest_finished(16620)%
  %quest% %actor% finish 16620
  %purge% %self%
else
  remote ornament_counter %self.id%
end
~
#16621
Grinchy Demon and Krampus: Reset on greeting~
0 h 100
~
if %self.fighting%
  halt
end
set room %self.room%
* Buffs
dg_affect #16618 %self% off
dg_affect #16684 %self% off
dg_affect #16687 %self% off
dg_affect #16689 %self% off
* Max?
rdelete summoned_max %self.id%
set max %room.people(16614)%
if %max%
  %echo% ~%max% runs off.
  %purge% %max%
end
~
#16622
winter pixy spawn~
0 n 100
~
switch %random.4%
  case 1
    %echo% You see ~%self% out of the corner of your eye.
  break
  case 2
    %echo% What looks like a flying snowball wizzes by.
  break
  case 3
    %echo% ~%self% dances in midair.
  break
  case 4
    %echo% Snow flies everywhere as ~%self% spins in circles.
  break
done
eval movement %random.11% - 1
while %movement%
  mmove
  eval movement %movement% - 1
done
~
#16623
pixy spawning~
1 b 25
~
set carrying %self.carried_by%
if !%carrying%
  halt
end
if %carrying.inventory(16626)%
  halt
end
set person %self.room.people%
while %person%
  if %person.is_npc% && %person.vnum% == 16624
    halt
  end
  set person %person.next_in_room%
done
%load% mob 16624 room
~
#16624
freeze the pixy~
1 c 2
freeze~
if !%arg%
  %send% %actor% What do you want to blast with @%self%?
  return 1
  halt
end
if %actor.inventory(16626)%
  %send% %actor% You already have a pixy on ice. Should probably get it back to the tree before it thaws out.
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% They must have fled from your awesome power, because they're not here.
  return 1
  halt
end
if %target.is_npc% && %target.vnum% == 16624
  %send% %actor% You point @%self% at ~%target% and unleash a blast of icy magic!
  %echoaround% %actor% ~%actor% points @%self% at ~%target% and unleashes a blast of icy magic!
  %echo% ~%target% is covered in a layer of ice and becomes motionless.
  return 1
else
  %send% %actor% You can only use @%self% on the winter pixies.
  return 1
  halt
end
%purge% %target%
%load% obj 16626 %actor% inv
~
#16625
pixy thaws out~
1 f 0
~
if !%self.carried_by%
  halt
end
return 0
set carried_by %self.carried_by%
set pixy_rng %random.100%
if %pixy_rng% <= 60
  %send% %carried_by% @%self% thaws out and suddenly vanishes in a poof of dust!
  %echoaround% %carried_by% ~%carried_by% is mysteriously engulfed in a cloud of dust!
elseif %pixy_rng% <= 80
  %send% %carried_by% @%self% melts in your hands, but the cold seems to intensify until you're frozen yourself!
  %echoaround% %carried_by% ~%carried_by% starts turning blue, and eventually comes to a halt as though frozen in place!
  dg_affect #16625 %carried_by% HARD-STUNNED on 60
elseif %pixy_rng% <= 95
  %send% %carried_by% @%self% warms and angrily covers you in dust, you feel drunk all of a sudden!
  %echoaround% %carried_by% ~%carried_by% sparkles for a moment and begins to stagger.
  nop %carried_by.drunk(30)%
else
  %send% %carried_by% @%self% has apparently thawed out and is quite furious with you!
  %echoaround% %carried_by% A furious pixy begins to attack ~%carried_by% out of no where!
  %load% mob 16625 room
  %force% %self.room.people% kill %carried_by.name%
end
%purge% %self%
~
#16626
pixy placement~
1 c 2
place~
if !%arg%
  return 0
  halt
end
set christmas_tree %self.room.contents(16607)%
if !%christmas_tree%
  %send% %actor% There's no Christmas tree here for you to place @%self% on top of.
  halt
end
if %actor.obj_target(%arg%)% != %self%
  %send% %actor% Only @%self% can be used to top a Christmas tree.
  halt
end
if !%christmas_tree.varexists(pixy_topped_off)%
  %mod% %christmas_tree% append-lookdesc-noformat On top %self.shortdesc% sits, frozen in place.
  %send% %actor% You place @%self% on top of @%christmas_tree%.
  %echoaround% %actor% ~%actor% places @%self% on top of @%christmas_tree%.
  set pixy_topped_off 1
  remote pixy_topped_off %christmas_tree.id%
else
  %send% %actor% You remove %christmas_tree.player_topped_tree%'s pixy from @%christmas_tree% and throw it away, before placing your own.
  %echoaround% %actor% ~%actor% removes %christmas_tree.player_topped_tree%'s pixy from @%christmas_tree% and throws it away, before placing ^%actor% own on top.
end
set player_topped_tree %actor.name%
remote player_topped_tree %christmas_tree.id%
%quest% %actor% trigger 16626
if %actor.quest_finished(16626)%
  %quest% %actor% finish 16626
end
~
#16627
ornament extention~
1 c 4
buff~
if !%arg%
  return 0
  halt
end
set buffing %actor.obj_target(%arg%)%
if !%buffing%
  return 0
  halt
end
set buffnum %buffing.vnum%
if %buffnum% != 16621 && %buffnum% != 16622 && %buffnum% != 16623 && %buffnum% != 16624
  %send% %actor% You can only buff the ornaments from the winter ornament set.
  halt
end
otimer 1152
%send% %actor% You buff @%buffing% with a cloth, extending the beauty.
%echoaround% %actor% ~%actor% buffs @%buffing% with a cloth, extending the beauty.
~
#16628
throw the enchanted snowball~
1 c 2
throw~
if !%arg%
  return 0
  halt
elseif !(%actor.obj_target(%arg.car%)% == %self%)
  return 0
  halt
end
if !%arg.cdr%
  %send% %actor% Fine, but who or what did you want to throw @%self% at?
  halt
end
if (abominable /= %arg.cdr%) || (snowman /= %arg.cdr%)
  set person %self.room.people%
  while %person%
    if %person.vnum% == 16628
      set AbominableSnowmanHere %person%
    end
    set person %person.next_in_room%
  done
  if !%AbominableSnowmanHere%
    %send% %actor% Doesn't seem the abominable snowman is here for you to freeze with your snowball.
    halt
  end
else
  %send% %actor% You can only freeze the abominable snowman with @%self%.
  halt
end
%send% %actor% You throw @%self% at ~%AbominableSnowmanHere% with all your might...
%echoaround% %actor% ~%actor% throws @%self% at ~%AbominableSnowmanHere% with all ^%actor% might...
%load% obj 16630 room
wait 1
%echo% Instantly ~%AbominableSnowmanHere% freezes solid!
%purge% %AbominableSnowmanHere%
%quest% %actor% trigger 16628
if %actor.quest_finished(16628)%
  %quest% %actor% finish 16628
end
~
#16629
enchant the snowball~
1 c 2
enchant~
if !%arg%
  return 0
  halt
end
if !(%actor.obj_target(%arg.car%)% == %self%)
  return 0
  halt
end
if %arg.cdr% != freezing
  %send% %actor% You must enchant @%self% with freezing to subdue the abominable snowman.
  halt
end
if !%actor.has_resources(1300,6)%
  %send% %actor% It will take six seashells to enchant @%self% with freezing.
  halt
end
nop %actor.add_resources(1300, -6)%
%load% obj 16628 %actor% inv
%send% %actor% You enchant @%self% with freezing.
%echoaround% %actor% ~%actor% enchants @%self% with freezing.
%purge% %self%
~
#16630
snowman summons abominable snowman~
0 i 50
~
wait 1
%load% mob 16628 %self.level%
set SnowmanUnderAttack %self.id%
remote SnowmanUnderAttack %self.room.people.id%
set AbominableSnowman %self.room.people%
switch %random.4%
  case 1
    %echo% ~%AbominableSnowman% roars and begins to swing at ~%self%!
  break
  case 2
    %echo% Suddenly, a white blur lunges at ~%self% and you see ~%AbominableSnowman% hit it!
  break
  case 3
    %echo% ~%AbominableSnowman% comes out of nowhere and begins to deal blows to ~%self%!
  break
  case 4
    %echo% Snow flies everywhere as ~%AbominableSnowman% pops up and violently strikes ~%self%!
  break
done
%teleport% %self% %self.room%
%force% %AbominableSnowman% kill %self.pc_name%
wait 1
%send% %self.PlayerOnAbominableQuest% ~%self% tells you, 'The abominable snowman is here at %self.room.name%!'
~
#16631
snowman target will not escape~
0 s 100
~
if %actor.id% == %self.SnowmanUnderAttack%
  return 0
  %echo% ~%self% growls and drags ~%actor% back.
  mkill %actor%
end
~
#16632
abominable kills regular snowman~
0 z 100
~
if %actor.vnum% == 16600
  %send% %actor.PlayerOnAbominableQuest% ~%self% tells you, 'You obviously aren't a very good protector, ~%actor% is mush.'
  wait 1
  %echo% ~%self% runs off!
  %purge% %self%
else
  set person %self.room.people%
  while %person%
    if %person.id% == %self.SnowmanUnderAttack%
      mkill %person%
      unset person
    end
    set person %person.next_in_room%
  done
end
~
#16633
protected the snowman~
0 v 0
~
if %questvnum% != 16628
  halt
end
if %self.PlayerOnAbominableQuest% == %actor%
  set MyID %self.id%
  detach 16630 %MyID%
  attach 16617 %MyID%
  detach 16633 %MyID%
end
~
#16634
snowman has lived too long~
0 ab 2
~
if %self.varexists(IWasBornOn)%
  eval SinceLoaded %dailycycle% - %self.IWasBornOn%
else
  %echo% ~%self% melts after such a long time of standing around.
  %purge% %self%
end
if %SinceLoaded% >= 3 || !%event.running(10700)%
  %echo% ~%self% melts after such a long time of standing around.
  %purge% %self%
end
~
#16635
Holiday pet never dies~
0 ft 100
~
Commands:
if %self.varexists(deaths)%
  eval deaths %self.deaths% + 1
else
  set deaths 1
end
remote deaths %self.id%
%echo% ~%self% runs away, scared and injured!
if %self.leader%
  nop %self.leader.set_cooldown(16635,1800)%
end
return 0
~
#16636
Holiday pet load trigger / flee when scared~
0 nt 100
~
set pc %self.companion%
* Part 1: Update strings etc: (with script 16638)
xmas_pet_setup
* Part 2: announce if needed
if %pc%
  if %pc.varexists(xmas_pet_announce)% && %pc.varexists(xmas_pet_type)% && %pc.varexists(xmas_pet_name)%
    rdelete xmas_pet_announce %pc.id%
    switch %self.vnum%
      case 16637
        if %pc.xmas_pet_name%
          %echo% %pc.xmas_pet_name% has grown! You notice &%self% seems more... heroic!
        else
          %echo% ~%self% has grown! You notice &%self% seems more... heroic!
        end
      break
      case 16640
        if %pc.xmas_pet_name%
          %echo% %pc.xmas_pet_name% has grown! You notice &%self% seems more... heroic!
        else
          %echo% ~%self% has grown! You notice &%self% seems more... heroic!
        end
      break
      default
        if %pc.xmas_pet_name%
          %echo% %pc.xmas_pet_name% has grown into a %pc.xmas_pet_type%!
        else
          %echo% ~%self% has grown into a %pc.xmas_pet_type%!
        end
      break
    done
  end
end
* Part 3: flee if on-cooldown
wait 1
if !%pc%
  %echo% ~%self% runs away.
  %purge% %self%
  halt
end
if %pc.cooldown(16635)%
  %echo% ~%self% looks scared and runs away to hide.
  %purge% %self%
  halt
end
~
#16637
Holiday pet name command~
0 ct 0
name~
* NOTE: This script only allows naming one time, then detaches.
* Admins may re-attach this script to a pet to allow the player to rename it.
return 1
set usage Usage: name <pet> <new name>
set targ %arg.car%
set name %arg.cdr.trim%
set pet %actor.char_target(%targ%)%
* validate?
if (!%targ% || !%name%)
  %send% %actor% This command lets you name your winter holiday pet. Be sure to summon them first.
  %send% %actor% %usage%
  halt
end
if !%pet%
  %send% %actor% You don't see %targ.ana% %targ% here.
  halt
end
if (%pet.is_pc% || %pet.vnum% < 16635 || %pet.vnum% > 16640)
  %send% %actor% You can only name a dog or cat from the winter holiday pet adoption.
  halt
end
if %pet% != %actor.companion%
  %send% %actor% You can only name your OWN pet.
  halt
end
if %name.strlen% > 15
  %send% %actor% That name is too long.
  halt
end
* ensure we have all the data we need
if %actor.varexists(xmas_pet_type)%
  set xmas_pet_type %actor.xmas_pet_type%
else
  %send% %actor% Error: You are missing pet type data. Contact an administrator.
  halt
end
if %actor.varexists(xmas_pet_coat)%
  set xmas_pet_coat %actor.xmas_pet_coat%
else
  %send% %actor% Error: You are missing pet coat data. Contact an administrator.
  halt
end
if %actor.varexists(xmas_pet_sex)%
  set xmas_pet_sex %actor.xmas_pet_sex%
else
  %send% %actor% Error: You are missing pet sex data. Contact an administrator.
  halt
end
if %actor.varexists(xmas_pet_name)%
  set xmas_pet_name %actor.xmas_pet_name%
else
  %send% %actor% Error: You are missing pet name data. Contact an administrator.
  halt
end
* final checks
set name %name.cap%
if %name% == %xmas_pet_name%
  %send% %actor% &%pet% is already called that.
  halt
end
* messaging...
if %xmas_pet_name%
  %send% %actor% You rename %xmas_pet_name% %name%.
  %echoaround% %actor% ~%actor% renames ~%pet% %name%.
else
  %send% %actor% You name your %xmas_pet_type% %name%!
  %echoaround% %actor% ~%actor% names ^%actor% %xmas_pet_coat% %xmas_pet_type% %name%.
end
* store the name
set xmas_pet_name %name%
remote xmas_pet_name %actor.id%
* this will pull the vars and do the actual naming (with script 16638)
xmas_pet_setup
* don't allow renaming
detach 16637 %self.id%
~
#16638
Holiday pet self-naming helper~
0 ct 0
xmas_pet_setup~
* Note: Some of this script is very similar to the load trigger 16636
* Note: this requires that self is currently a companion and that its player
* has these vars: xmas_pet_type, xmas_pet_coat, xmas_pet_sex, and
* xmas_pet_name (which may be 0/empty)
* Note: This script is ONLY called by the mob itself
return 0
set pc %self.companion%
if %actor% != %self%
  halt
elseif !%pc%
  halt
elseif (!%pc.varexists(xmas_pet_type)% || !%pc.varexists(xmas_pet_coat)%)
  halt
elseif (!%pc.varexists(xmas_pet_sex)% || !%pc.varexists(xmas_pet_name)%)
  halt
end
* ok ready to proceed
return 1
set xmas_pet_type %pc.xmas_pet_type%
set xmas_pet_coat %pc.xmas_pet_coat%
set xmas_pet_sex %pc.xmas_pet_sex%
set xmas_pet_name %pc.xmas_pet_name%
* sex first, just in case
%mod% %self% sex %xmas_pet_sex%
* two versions based on whether or not it's named
if %xmas_pet_name%
  %mod% %self% keywords %xmas_pet_name% %xmas_pet_type% %xmas_pet_coat%
  %mod% %self% shortdesc %xmas_pet_name% the %xmas_pet_coat% %xmas_pet_type%
else
  * not named
  %mod% %self% keywords %xmas_pet_type% %xmas_pet_coat%
  %mod% %self% shortdesc %xmas_pet_coat.ana% %xmas_pet_coat% %xmas_pet_type%
end
* vnum-based adjustments
switch %self.vnum%
  case 16637
    * heroic dog
    if %xmas_pet_name%
      %mod% %self% longdesc %xmas_pet_name% the %xmas_pet_coat% %xmas_pet_type% stands guard.
    else
      %mod% %self% longdesc %xmas_pet_coat.ana.cap% %xmas_pet_coat% %xmas_pet_type% stands guard.
    end
    %mod% %self% lookdesc This gallant dog has a stunning %xmas_pet_coat% coat and a bow around
    %mod% %self% append-lookdesc %self.hisher% neck. Though %self.heshe% looks worn with age,
    %mod% %self% append-lookdesc %self.heshe% hasn't lost the spark in %self.hisher% eye.
  break
  case 16640
    * heroic cat
    if %xmas_pet_name%
      %mod% %self% longdesc %xmas_pet_name% the %xmas_pet_coat% %xmas_pet_type% watches from a safe vantage point.
    else
      %mod% %self% longdesc %xmas_pet_coat.ana.cap% %xmas_pet_coat% %xmas_pet_type% watches from a safe vantage point.
    end
    %mod% %self% lookdesc This sleek cat has a luxurious %xmas_pet_coat% coat and a bow around
    %mod% %self% append-lookdesc %self.hisher% neck. Though %self.heshe% looks tattered with age,
    %mod% %self% append-lookdesc %self.heshe% hasn't lost the fire in %self.hisher% eye.
  break
  default
    if %xmas_pet_name%
      %mod% %self% longdesc %xmas_pet_name% the %xmas_pet_coat% %xmas_pet_type% sits here with a bow around %self.hisher% neck.
    else
      * not named
      %mod% %self% longdesc %xmas_pet_coat.ana.cap% %xmas_pet_coat% %xmas_pet_type% sits here with a bow around %self.hisher% neck.
    end
    %mod% %self% lookdesc This adorable little %xmas_pet_type% with a beautiful %xmas_pet_coat% coat
    %mod% %self% append-lookdesc has a bow around %self.hisher% neck.
  break
done
* and append notes
if %xmas_pet_name%
  %mod% %self% append-lookdesc-noformat A tag on %self.hisher% bow says '%xmas_pet_name%'.
  %mod% %self% append-lookdesc-noformat You can 'feed <meat> %xmas_pet_name%'.
else
  %mod% %self% append-lookdesc-noformat To feed your pet: feed <meat> %xmas_pet_type%
  %mod% %self% append-lookdesc-noformat To name your pet: name %xmas_pet_type% <name>
end
~
#16639
Holiday pet upgrade ticker~
0 bt 50
~
* configs
set progress_to_level 10000
if (%self.vnum% != 16635 && %self.vnum% != 16638)
  eval progress_to_level %progress_to_level% * 3
end
* find person
set pc %self.companion%
if !%pc% || !%pc.is_pc%
  halt
end
* get current progress
if %pc.varexists(xmas_pet_progress)%
  set xmas_pet_progress %pc.xmas_pet_progress%
else
  set xmas_pet_progress 0
end
* update progress
if %xmas_pet_progress% < %progress_to_level%
  eval xmas_pet_progress %xmas_pet_progress% + 1
  remote xmas_pet_progress %pc.id%
else
  * done?
  set new_vnum 0
  switch %self.vnum%
    case 16635
      set new_vnum 16636
      set xmas_pet_type dog
    break
    case 16636
      set new_vnum 16637
      set xmas_pet_type dog
    break
    case 16638
      set new_vnum 16639
      set xmas_pet_type cat
    break
    case 16639
      set new_vnum 16640
      set xmas_pet_type cat
    break
  done
  * did we find one to evolve to?
  if %new_vnum%
    * update data
    remote xmas_pet_type %pc.id%
    set xmas_pet_progress 0
    remote xmas_pet_progress %pc.id%
    set xmas_pet_announce 1
    remote xmas_pet_announce %pc.id%
    * silently swap for the next pet
    dg_affect %self% HIDE on 1
    * switch companions
    nop %pc.add_companion(%new_vnum%)%
    nop %pc.remove_companion(%self.vnum%)%
    * theoretically this will interrupt the script because it will un-load this companion
    %mod% %pc% companion %new_vnum%
  end
end
~
#16640
Holiday pet adoption certificate~
1 c 2
adopt~
return 1
set usage Usage: adopt <coat> <puppy/kitten>
set dog_coats chestnut silver beige gold brown chocolate golden wheaten cream rusty lilac orange striped brindle spotted dotted black white green violet purple magenta red yellow blue orange indigo pink grey gray
set cat_coats ginger cream brown cinnamon tuxedo tabby mackerel tortoiseshell seal-point blue-point flame-point black white green violet purple magenta red yellow blue orange indigo pink grey gray
* basic validation
if %actor.is_npc%
  %send% %actor% Sorry, no.
  halt
end
if %actor.varexists(xmas_pet_type)%
  if %actor.varexists(xmas_pet_name)%
    if %actor.xmas_pet_name%
      %send% %actor% You can't do that -- you already adopted %actor.xmas_pet_name%!
    else
      %send% %actor% You can't do that -- you already adopted a %actor.xmas_pet_type%!
    end
  else
    %send% %actor% You can't do that -- you already adopted a %actor.xmas_pet_type%!
  end
  halt
end
* pre-processing and argument validation
set coat %arg.car%
set type %arg.cdr%
if !%type% || !%coat%
  %send% %actor% %usage%
  %send% %actor% Dog coats available: %dog_coats%
  %send% %actor% Cat coats available: %cat_coats%
  halt
end
if (%type% == dog || %type% == puppy)
  set type puppy
  set coat_list %dog_coats%
elseif (%type% == cat || %type% == kitten)
  set type kitten
  set coat_list %cat_coats%
else
  %send% %actor% Unknown pet type '%type%'.
  %send% %actor% %usage%
  halt
end
* find coat in list
set found_coat 0
while (!%found_coat% && %coat_list%)
  set word %coat_list.car%
  set coat_list %coat_list.cdr%
  if %coat% == %word%
    set found_coat %word%
  end
done
if !%found_coat%
  %send% %actor% There's no %type% with a '%coat%' coat available.
  if %type% == puppy
    %send% %actor% Dog coats available: %dog_coats%
  else
    %send% %actor% Cat coats available: %cat_coats%
  end
  halt
end
* set up vars to store
set xmas_pet_type %type%
set xmas_pet_coat %found_coat%
set xmas_pet_name 0
if %random.2% == 2
  set xmas_pet_sex male
else
  set xmas_pet_sex female
end
* store vars
remote xmas_pet_type %actor.id%
remote xmas_pet_coat %actor.id%
remote xmas_pet_sex %actor.id%
remote xmas_pet_name %actor.id%
* ensure they don't already have any of these companions set
nop %actor.remove_companion(16635)%
nop %actor.remove_companion(16636)%
nop %actor.remove_companion(16637)%
nop %actor.remove_companion(16638)%
nop %actor.remove_companion(16639)%
nop %actor.remove_companion(16640)%
* messaging
%send% %actor% You adopt %found_coat.ana% %found_coat% %type%! It comes running up to you.
%send% %actor% You can dismiss or re-summon it with the 'companions' command.
%echoaround% %actor% ~%actor% adopts %found_coat.ana% %found_coat% %type%! It comes running up to *%actor%.
* remove old companion
if %actor.companion%
  %echoaround% %actor.companion% ~%actor.companion% leaves.
  %purge% %actor.companion%
end
* grant the pet
if %type% == puppy
  nop %actor.add_companion(16635)%
  %mod% %actor% companion 16635
else
  nop %actor.add_companion(16638)%
  %mod% %actor% companion 16638
end
* check that the pet loaded
set pet %actor.companion%
if !%pet%
  * ERROR somehow... remove all data
  %send% %actor% Something went wrong and your pet couldn't be set up.
  rdelete xmas_pet_type %actor.id%
  rdelete xmas_pet_coat %actor.id%
  rdelete xmas_pet_sex %actor.id%
  rdelete xmas_pet_name %actor.id%
  nop %actor.remove_companion(16635)%
  nop %actor.remove_companion(16638)%
  halt
end
%purge% %self%
~
#16641
Holiday Pet Leash (admin tool)~
1 c 2
leash~
set usage Usage: leash <person> <command>
set valid_commands Valid commands: check (shows data), clear (wipes data), progress (view/change progress), repair (tries to fix)
set pet_vnums 16635 16636 16637 16638 16639 16640
return 1
if !%actor.is_immortal%
  %send% %actor% You may not use this.
  halt
end
* args: <person> <command> <arg>
set person %arg.car%
set arg %arg.cdr%
set command %arg.car%
set arg %arg.cdr%
set target %actor.char_target(%person%)%
if (!%person% || !%command%)
  %send% %actor% %usage%
  %send% %actor% %valid_commands%
  halt
end
if !%target%
  %send% %actor% Nobody named '%person%' here.
  halt
elseif %target.is_npc%
  %send% %actor% This tool only works on players.
  halt
end
switch %command%
  case check
    * show pet data
    if %target.varexists(xmas_pet_type)%
      %send% %actor% |%target% xmas_pet_type: %target.xmas_pet_type%
    else
      %send% %actor% |%target% xmas_pet_type: no data
    end
    if %target.varexists(xmas_pet_name)%
      %send% %actor% |%target% xmas_pet_name: %target.xmas_pet_name%
    else
      %send% %actor% |%target% xmas_pet_name: no data
    end
    if %target.varexists(xmas_pet_sex)%
      %send% %actor% |%target% xmas_pet_sex: %target.xmas_pet_sex%
    else
      %send% %actor% |%target% xmas_pet_sex: no data
    end
    if %target.varexists(xmas_pet_coat)%
      %send% %actor% |%target% xmas_pet_coat: %target.xmas_pet_coat%
    else
      %send% %actor% |%target% xmas_pet_coat: no data
    end
    if %target.varexists(xmas_pet_progress)%
      %send% %actor% |%target% xmas_pet_progress: %target.xmas_pet_progress%
    else
      %send% %actor% |%target% xmas_pet_progress: no data
    end
    * show pet vnums
    set vnums %pet_vnums%
    while %vnums%
      set vnum %vnums.car%
      set vnums %vnums.cdr%
      if %target.has_companion(%vnum%)%
        %send% %actor% %actor.name% has pet: %vnum%
      end
    done
  break
  case clear
    * remove any companions
    set vnums %pet_vnums%
    set compan %target.companion%
    while %vnums%
      set vnum %vnums.car%
      set vnums %vnums.cdr%
      if (%compan% && %compan.vnum% == %vnum%)
        %echoaround% %compan% ~%compan% leaves.
        %purge% %compan%
        set %compan% 0
      end
      nop %target.remove_companion(%vnum%)%
    done
    * clear vars
    rdelete xmas_pet_type %target.id%
    rdelete xmas_pet_coat %target.id%
    rdelete xmas_pet_sex %target.id%
    rdelete xmas_pet_name %target.id%
    rdelete xmas_pet_progress %target.id%
    * messaging
    %send% %actor% You have wiped holiday pet data for ~%target%.
  break
  case progress
    if %target.varexists(xmas_pet_progress)%
      * see if they gave a number
      if %arg.strlen% > 0 && %arg% >= 0
        set was %target.xmas_pet_progress%
        eval xmas_pet_progress %arg%
        remote xmas_pet_progress %target.id%
        %send% %actor% |%target% xmas_pet_progress: %target.xmas_pet_progress% (was %was%)
      else
        %send% %actor% |%target% xmas_pet_progress: %target.xmas_pet_progress%
      end
    else
      %send% %actor% %target.name% has no xmas_pet_progress data.
    end
  break
  case repair
    * forces the pet to re-name itself
    set compan %target.companion%
    if %compan%
      if %pet_vnums% ~= %compan.vnum%
        %force% %compan% xmas_pet_setup
        %send% %actor% Attempting to repair %compan%...
      else
        %send% %actor% The player has the wrong companion out.
      end
    else
      %send% %actor% The player must have their holiday pet out for you to repair it.
    end
  break
  default
    %send% %actor% Unknown command '%command%'.
    %send% %actor% %usage%
    %send% %actor% %valid_commands%
  break
done
~
#16642
Holiday pet feeding~
0 ct 0
feed~
* configs
set safe_max_progress 10000000
set prog_per_food 50
set food_cooldown 1800
set buff_time 900
* args
set food_targ %arg.car%
set mob_targ %arg.cdr%
if (!%food_targ% || !%mob_targ% || %actor.char_target(%mob_targ%)% != %self%)
  return 0
  halt
end
* all other results should return 1
return 1
* requires a companion
set pc %self.companion%
if (!%pc% || !%pc.is_pc% || %pc% != %actor%)
  %send% %actor% ~%self% doesn't seem to want @%food%.
  halt
end
* look for the food
set food %actor.obj_target(%food_targ%)%
if !%food%
  %send% %actor% You don't seem to have %food_targ.ana% %food_targ%.
  halt
elseif %food.carried_by% != %actor%
  %send% %actor% You need to have @%food% in your inventory to feed it to ~%self%.
  halt
elseif (%food.type% != FOOD || !%food.is_component(6200,6220)% )
  %send% %actor% ~%self% doesn't seem to want @%food%.
  halt
end
* get current progress
if %pc.varexists(xmas_pet_progress)%
  set xmas_pet_progress %pc.xmas_pet_progress%
else
  set xmas_pet_progress 0
end
* update progress
if !%pc.cooldown(16642)% && %xmas_pet_progress% < %safe_max_progress%
  eval xmas_pet_progress %xmas_pet_progress% + %prog_per_food%
  remote xmas_pet_progress %pc.id%
  nop %pc.set_cooldown(16642,%food_cooldown%)%
else
  * doesn't need food but can be buffed
  dg_affect #16641 %self% off
  switch %random.3%
    case 1
      dg_affect #16641 %self% BONUS-PHYSICAL 10 %buff_time%
    break
    case 2
      dg_affect #16641 %self% TO-HIT 30 %buff_time%
    break
    case 3
      dg_affect #16641 %self% HEAL-OVER-TIME 10 %buff_time%
    break
  done
end
* messaging
%send% %actor% You feed @%food% to ~%self%.
%echoaround% %actor% ~%actor% feeds @%food% to ~%self%.
* and purge
%purge% %food%
~
#16643
Straw goat spawn trigger~
0 n 100
~
if %self.vnum% == 16643
  * Small mob goat
  * not currently echoing when it runs off
  * %echo% ~%self% runs off as fast as its little legs will carry it!
  dg_affect %self% SNEAK on -1
  set tries 50
  while %tries% > 0
    eval tries %tries% - 1
    mmove
  done
  dg_affect %self% SNEAK off
  %echo% ~%self% scampers in.
elseif %self.vnum% == 16644
  * Large object goat version: this uses a helper mob
  set tries 50
  while %tries% > 0
    eval tries %tries% - 1
    mmove
  done
  %load% obj 16644 room
  %echo% Some citizens have erected a giant straw goat!
  %purge% %self%
end
~
#16644
Burning straw goat~
1 n 100
~
wait 1
set ch %self.room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.on_quest(16643)%
    %quest% %ch% trigger 16643
    %quest% %ch% finish 16643
  end
  if %ch.on_quest(16644)%
    %quest% %ch% trigger 16644
    %quest% %ch% finish 16644
  end
  set ch %next_ch%
done
~
#16645
Straw goat leash: keep it outside~
0 i 100
~
* This is used for both the small goat and the spawner for the large goat
* keeps the mob outside
if %self.room.is_outdoors%
  return 1
else
  return 0
end
~
#16646
Command: Burn or Light the small straw goat~
0 c 0
burn light~
* targeting
set target %actor.char_target(%arg%)%
if %target% != %self%
  return 0
  halt
else
  * all other cases
  return 1
end
* lighter?
set lighter %actor.find_lighter%
if !%lighter% && !%actor.has_tech(Light-Fire)%
  %send% %actor% You don't have anything to light the straw goat with.
  halt
end
* chance to fail
if %random.2% == 2 && !%self.disabled% && %actor.skill(Stealth)% < 100 && !%actor.aff_flagged(HIDE)% && !%self.aff_flagged(IMMOBILIZED)% && %self.position% == Standing
  %send% %actor% You try to light the little straw goat on fire but it darts away!
  %echoaround% %actor% ~%actor% tries to light the little straw goat on fire but it darts away!
  * replace with fresh copy
  %load% mob %self.vnum%
  %purge% %self%
  halt
end
* ok
if %actor.has_tech(Light-Fire)% || !%lighter%
  %send% %actor% You light the little straw goat on fire! It stops running and begins to crackle and pop.
  %echoaround% %actor% ~%actor% lights the little straw goat on fire! It stops running and begins to crackle and pop.
elseif %lighter%
  %send% %actor% You use @%lighter% to set the little straw goat on fire! It stops running and begins to crackle and pop.
  %echoaround% %actor% ~%actor% uses @%lighter% to set the little straw goat on fire! It stops running and begins to crackle and pop.
  nop %lighter.used_lighter(%actor%)%
end
%load% obj 16643 room
%purge% %self%
~
#16647
Prevent burning of straw goat in front of witnesses~
1 c 4
light burn~
* check targeting
if !%arg% || %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
* check witnesses
if !%ch.aff_flagged(HIDE)% && %ch.skill(Stealth)% < 100
  set witnesses 0
  set ch %actor.room.people%
  while %ch% && %witnesses% < 2
    if %ch.position% == Standing && !%ch.leader% && !%ch.disabled% && !%ch.leader% && %ch.mob_flagged(HUMAN)%
      eval witnesses %witnesses% + 1
    end
    set ch %ch.next_in_room%
  done
  if %witnesses% > 1
    %send% %actor% You can't set it on fire in front of witnesses!
    return 1
    halt
  elseif %witnesses% > 0
    %send% %actor% You can't set it on fire in front of a witness!
    return 1
    halt
  end
end
* if we made it this far, just allow it
return 0
~
#16648
Holiday pet interactions and emotes~
0 bt 10
~
* basics
set vnum %self.vnum%
if %vnum% >= 16635 && %vnum% <= 16637
  set dog 1
  set cat 0
elseif %vnum% >= 16638 && %vnum% <= 16640
  set dog 0
  set cat 1
else
  * unknown animal
  halt
end
if %vnum% == 16635 || %vnum% == 16638
  set small 1
else
  set small 0
end
if %vnum% == 16637 || %vnum% == 16640
  set heroic 1
else
  set heroic 0
end
* ensure companion here (companionless messages)
set pc %self.companion%
if !%pc% || %pc.room% != %self.room%
  switch %random.4%
    case 1
      %echo% ~%self% looks around, lonely.
    break
    case 2
      %echo% ~%self% paces back and forth.
    break
    case 3
      %echo% ~%self% curls up but doesn't sleep.
    break
    case 4
      %echo% ~%self% seems wary.
    break
  done
  halt
end
* normal messages (random)
switch %random.8%
  case 1
    %echo% ~%self% nuzzles up against ~%pc%.
  break
  case 2
    if %cat%
      %echo% ~%self% rubs up against ~%pc% and purrs.
    elseif %dog%
      %echo% ~%self% whacks ~%pc% with ^%self% tail.
    end
  break
  case 3
    if %cat%
      %echo% ~%self% cleans *%self%self.
    elseif %dog%
      %echo% ~%self% chases ^%self% tail in circles.
    end
  break
  case 4
    if %pc.position% == Standing || %pc.position% == Sitting || %pc.position% == Resting
      if %dog%
        set thing a little ball
      else
        set thing a ball of string
      end
      %send% %pc% You throw %thing% and ~%self% chases after it and brings it back!
      %echoaround% %pc% ~%pc% throws %thing% and ~%self% chases after it and brings it back to *%pc%!
    end
  break
  * remaining numbers: normal meow/bark
  default
    if %cat%
      if %small%
        %echo% ~%self% mews.
      elseif %heroic% && %random.2% == 2
        %echo% ~%self% lets out a mighty roar!
      else
        %echo% ~%self% meows.
      end
    elseif %dog%
      if %small%
        %echo% ~%self% whimpers.
      elseif %heroic% && %random.2% == 2
        %echo% ~%self% lets out a mighty howl!
      else
        %echo% ~%self% barks.
      end
    end
  break
done
~
#16649
Only harness flying mobs~
5 c 0
harness~
set anim_arg %arg.car%
set veh_arg %arg.cdr%
if (!%anim_arg% || !%veh_arg%)
  return 0
  halt
end
set mob %actor.char_target(%anim_arg%)%
set veh %actor.veh_target(%veh_arg%)%
if (!%mob% || !%veh% || %veh% != %self%)
  return 0
  halt
end
* check flying (block harness if not)
if !%mob.is_flying%
  %send% %actor% You can only harness flying animals to %veh.shortdesc%.
  return 1
  halt
end
* otherwise, just return 0 (allow the harness)
return 0
~
#16650
Elfish flying sleigh wax: enchant command~
1 c 2
enchant~
* targeting
set sleigh %actor.veh_target(%arg.car%)%
if (!%arg% || !%sleigh%)
  return 0
  halt
end
* everything else will return 1
return 1
if %sleigh.vnum% != 10715
  %send% %actor% You can only use @%self% to enchant an ordinary red sleigh from the Winter Wonderland adventure.
  halt
end
if (!%actor.empire% || %actor.empire% != %sleigh.empire%)
  %send% %actor% You can only use this on a sleigh you own.
  halt
end
if !%sleigh.complete%
  %send% %actor% You need to finish building the sleigh first.
  halt
end
if (%sleigh.sitting_in% || %sleigh.led_by%)
  %send% %actor% You can't do that while anyone is %sleigh.in_on% it!
  halt
end
* READY:
if %sleigh.contents%
  %send% %actor% You empty out %sleigh.shortdesc%...
  %echoaround% %actor% ~%actor% empties out %sleigh.shortdesc%...
  nop %sleigh.dump%
end
if %sleigh.animals_harnessed% > 1
  %send% %actor% You unharness the animals from %sleigh.shortdesc%...
  %echoaround% %actor% ~%actor% unharnesses the animals from %sleigh.shortdesc%...
  nop %sleigh.unharness%
elseif %sleigh.animals_harnessed% > 0
  %send% %actor% You unharness the animal from %sleigh.shortdesc%...
  %echoaround% %actor% ~%actor% unharnesses the animal from %sleigh.shortdesc%...
  nop %sleigh.unharness%
end
%load% veh 16650
set upgr %self.room.vehicles%
if %upgr.vnum% == 16650
  %own% %upgr% %sleigh.empire%
  %send% %actor% You polish %sleigh.shortdesc% with @%self%...
  %echoaround% %actor% ~%actor% polishes %sleigh.shortdesc% with @%self%...
  %echo% It begins to fly!
  %purge% %sleigh%
  %purge% %self%
else
  %send% %actor% The wax didn't seem to work!
end
~
#16651
Dreidel: drop~
1 h 100
~
* Reset the dreidel
%mod% %self% longdesc A wooden dreidel is lying on the ground.
%mod% %self% lookdesc The dreidel is a four-sided spinning top that's often used during the December holiday of Hanukkah. The sides of the dreidel have letters on them: nun (nothing), hei (half), gimel (all), and shin (put in).
%mod% %self% append-lookdesc-noformat Type 'spin dreidel' to use it.
~
#16652
Flying reindeer polish: use polish~
1 c 2
use~
if %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
* All other results return 1
return 1
if !%self.room.function(STABLE)%
  %send% %actor% You can only use @%self% at a stable.
  halt
elseif (%actor.has_mount(10700)% && !%actor.has_mount(16652)%)
  %send% %actor% You use @%self% and upgrade your flying reindeer mount to a red-nosed reindeer!
  nop %actor.remove_mount(10700)%
  nop %actor.add_mount(16652)%
elseif (%actor.has_mount(10705)% && !%actor.has_mount(10700)%)
  %send% %actor% You use @%self% and upgrade your reindeer mount to a flying reindeer!
  nop %actor.remove_mount(10705)%
  nop %actor.add_mount(10700)%
elseif (%actor.has_mount(9175)% && !%actor.has_mount(10700)%)
  %send% %actor% You use @%self% and upgrade your reindeer mount to a flying reindeer!
  nop %actor.remove_mount(9175)%
  nop %actor.add_mount(10700)%
elseif (%actor.has_mount(9176)% && !%actor.has_mount(10700)%)
  %send% %actor% You use @%self% and upgrade your barded reindeer mount to a flying reindeer!
  nop %actor.remove_mount(9176)%
  nop %actor.add_mount(10700)%
elseif %actor.has_mount(16652)%
  %send% %actor% You already have a red-nosed reindeer mount. You can't do anything with @%self%.
  halt
else
  %send% %actor% You don't have a reindeer mount you can use the polish on. Try looking around the tundra.
  halt
end
* if we get here, we used it successfully
%echoaround% %actor% ~%actor% uses @%self%!
%purge% %self%
~
#16653
Grinchy combat: Present Toss, Pole Swing, Summon/Buff Max~
0 k 100
~
if %self.cooldown(16680)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 1 2 2 3
  set num_left 5
end
* pick
eval which %%random.%num_left%%%
set old %moves_left%
set moves_left
set move 0
while %which% > 0
  set move %old.car%
  if %which% != 1
    set moves_left %moves_left% %move%
  end
  set old %old.cdr%
  eval which %which% - 1
done
set moves_left %moves_left% %old%
* store
eval num_left %num_left% - 1
remote moves_left %self.id%
remote num_left %self.id%
* perform move
scfight lockout 16680 30 35
if %move% == 1
  * Present Toss
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  scfight clear dodge
  set targ %random.enemy%
  if !%targ%
    set targ %actor%
  end
  set id %targ.id%
  * random obj
  set object_1 a gaudy neck ties
  set object_2 a hideous Christmas sweater
  set object_3 fluffy woolen socks
  set object_4 a pet rock
  set which %random.4%
  eval obj %%object_%which%%%
  if %which% == 1 || %which% == 3
    set itthem them
  else
    set itthem it
  end
  if %diff% < 3
    * normal/hard
    %send% %targ% &&G**** &&Z~%self% grabs %obj% out of ^%self% sack and throws %itthem% at you! ****&&0 (dodge)
    %echoaround% %targ% &&G~%self% grabs %obj% out of ^%self% sack and throws %itthem% at ~%targ%!&&0
    scfight setup dodge %targ%
    wait 5 s
    nop %self.remove_mob_flag(NO-ATTACK)%
    wait 3 s
    if %self.disabled%
      nop %self.remove_mob_flag(NO-ATTACK)%
      halt
    end
    if !%targ% || %targ.id% != %id%
      * gone
      %echo% &&G%obj.cap% misses and lands on the ground.&&0
      unset targ
    elseif %targ.var(did_scfdodge)%
      * dodged: switch targ
      %echo% &&G%obj.cap% bounces and rebounds back toward ~%self%!&&0
      set targ %self%
      if %diff% == 1
        dg_affect #16686 %targ% TO-HIT 25 20
      end
    end
    * hit either them or me
    if %targ%
      * hit
      switch %random.2%
        case 1
          %echo% &&G%obj% hits ~%targ% in the head!&&0
          eval ouch 75 * %diff%
          %damage% %targ% %ouch% physical
          if %diff% > 1 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
            dg_affect #16619 %targ% STUNNED on 5
          end
        break
        case 2
          %echo% &&G%obj% hits ~%targ% in the face, covering ^%targ% eyes!&&0
          eval ouch 50 * %diff%
          %damage% %targ% %ouch% physical
          dg_affect #16620 %targ% BLIND on 5
        break
      done
    end
    scfight clear dodge
  else
    * group/boss
    %echo% &&G**** &&Z~%self% grabs %obj% out of ^%self% sack and prepares to slam %itthem% to the ground... ****&&0 (dodge)
    if %diff% == 1
      nop %self.add_mob_flag(NO-ATTACK)%
    end
    scfight setup dodge all
    eval wait 8 - %diff%
    wait %wait% s
    if %self.disabled%
      nop %self.remove_mob_flag(NO-ATTACK)%
      halt
    end
    %echo% &&G~%self% slams %obj% to the ground, releasing a snowy shockwave!&&0
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfdodge)%
          set hit 1
          %echo% &&GThe shockwave knocks ~%ch% back!&&0
          %send% %ch% That really hurt!
          %damage% %ch% 100 magical
        elseif %ch.is_pc%
          %send% %ch% &&GYou manage to take cover as the snowy shockwave barely misses you!&&0
          if %diff% == 1
            dg_affect #16686 %ch% TO-HIT 25 20
          end
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
    if !%hit%
      if %diff% < 3
        %echo% &&G~%self% is blown back by the shockwave!&&0
        dg_affect #16687 %self% HARD-STUNNED on 10
      end
    end
  end
elseif %move% == 2 && !%self.aff_flagged(BLIND)%
  * Pole Swing
  scfight clear dodge
  %echo% &&G~%self% pulls a thirty-nine-and-a-half foot pole from his gift sack... This can't be good.&&0
  eval dodge %diff% * 40
  dg_affect #16684 %self% DODGE %dodge% 20
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  scfight setup dodge all
  wait 3 s
  if %self.disabled%
    dg_affect #16684 %self% off
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %echo% &&G**** &&Z~%self% starts to swing the thirty-nine-and-a-half foot pole! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    scfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfdodge)%
          set hit 1
          %echo% &&G~%self% whacks ~%ch% in the head with the pole!&&0
          %send% %ch% That really hurt!
          %damage% %ch% 100 physical
          if %diff% > 1 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
            dg_affect #16616 %ch% STUNNED on 5
          end
        elseif %ch.is_pc%
          %send% %ch% &&GYou limbo under the pole and safely straighten back up.&&0
          if %diff% == 1
            dg_affect #16686 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&G**** Here comes the thirty-nine-and-a-half foot pole again... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
    eval cycle %cycle% + 1
  done
  dg_affect #16684 %self% off
  if !%hit%
    if %diff% < 3
      %echo% &&G~%self% looks tired from all that swinging.&&0
      dg_affect #16687 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
elseif %move% == 3
  * Summon or Buff Max (no interrupt)
  set max %room.people(16614)%
  if !%max% && %self.var(summoned_max,0)%
    %echo% &&G&&Z~%self% says, 'This is for Max, you monster!'&&0
    %echo% &&G&&Z~%self% turns even greener with rage!&&0
    set amount %diff% * 4
    dg_affect #16618 %self% BONUS-PHYSICAL %diff% 300
    halt
  elseif %max%
    %echo% &&G&&Z~%self% says, 'That's it, Max, bite 'em! Gore 'em with the old antler!'&&0
    set amount %diff% * 10
    dg_affect #16618 %max% BONUS-PHYSICAL %diff% 300
  else
    set summoned_max 1
    remote summoned_max %self.id%
    %echo% &&G&&Z~%self% says, 'Get 'em, Max!'&&0
    %load% mob 16614 ally
    set max %room.people(16614)%
    if %max%
      %force% %max% maggro %self.fighting%
    end
  end
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#16655
Upgrade Glitter: upgrade Winter Wonderland items~
1 c 2
upgrade~
if !%arg%
  * Pass through to upgrade command
  return 0
  halt
end
set target %actor.obj_target_inv(%arg%)%
if !%target%
  * Pass through to upgrade command (upgrading building)
  * %send% %actor% You don't seem to have %arg.ana% '%arg%'. (You can only use @%self% on items in your inventory.)
  return 0
  halt
end
* All other cases return 1
return 1
if %target.vnum% != 16653 && %target.vnum% != 16654 && %target.vnum% != 10711 && %target.vnum% != 10712 && %target.vnum% != 16666
  %send% %actor% You can only use @%self% on the gift sack, sweater, omni-tool, and hat from Winter Wonderland.
  halt
end
if %target.is_flagged(SUPERIOR)% || %target.vnum% == 16654
  %send% %actor% @%target% is already upgraded; using @%self% would have no benefit.
  halt
end
%send% %actor% You sprinkle @%self% onto @%target%...
%echoaround% %actor% ~%actor% sprinkles @%self% onto @%target%...
%echo% @%target% begins to shimmer and glow!
if %target.vnum% == 16653
  * sweater version: replace item
  %load% obj 16654 %actor% inv
  set sweat %actor.inventory%
  if %sweat% && %sweat.vnum% == %16654
    nop %sweat.bind(%target%)%
  end
  %purge% %target%
else
  * non-sweater: just make superior
  nop %target.flag(SUPERIOR)%
  if %target.level% > 0
    %scale% %target% %target.level%
  else
    %scale% %target% 1
  end
end
%purge% %self%
~
#16656
Mistletoe Kiss sequence~
1 bw 4
~
if !%self.is_inroom%
  halt
end
set pers1 0
set pers2 0
set iter %self.room.people%
* Attempt to randomly pick 2 people
while %iter%
  if ((%iter.is_pc% || %iter.mob_flagged(HUMAN)%) && !%iter.nohassle%)
    if !%pers1%
      set pers1 %iter%
    elseif !%pers2%
      set pers2 %iter%
    elseif %random.3% == 3
      set pers1 %iter%
    elseif %random.2% == 2
      set pers2 %iter%
    end
  end
  set iter %iter.next_in_room%
done
if !%pers1% || !%pers2%
  halt
end
if %pers1.is_ignoring(%pers2%)% || %pers2.is_ignoring(%pers1%)%
  halt
end
* randomy swap them
if %random.2% == 2
  set temp %pers1%
  set pers1 %pers2%
  set pers2 %temp%
end
* found 2 people!
%send% %pers1% You notice you're under the mistletoe with ~%pers2%...
%send% %pers2% You notice you're under the mistletoe with ~%pers1%...
%send% %pers1% You lean over and kiss ~%pers2% on the cheek!
%send% %pers2% ~%pers1% leans over and kisses you on the cheek!
%echoneither% %pers1% %pers2% ~%pers1% leans over and kisses ~%pers2% on the cheek!
~
#16657
Spin Dreidel~
1 c 6
spin~
if (!%arg% || %actor.obj_target(%arg.car%)% != %self%)
  return 0
  halt
end
* succeed from here
return 1
if %actor.cooldown(16657)% > 0
  %send% %actor% You can't spin it again yet.
  halt
end
* Choose side
switch %random.4%
  case 1
    set sign Shin
  break
  case 2
    set sign Nun
  break
  case 3
    set sign Gimel
  break
  case 4
    set sign Hei
  break
done
* Main work
nop %actor.set_cooldown(16657, 3)%
%send% %actor% You spin @%self%...
%echoaround% %actor% ~%actor% spins @%self%...
%echo% It falls down showing '%sign%'.
remote sign %self.id%
* Strings
%mod% %self% longdesc A wooden dreidel on the ground is showing '%sign%'.
%mod% %self% lookdesc The dreidel is a four-sided spinning top that's often used during the December holiday of Hanukkah. The sides of the dreidel have letters on them: nun (nothing), hei (half), gimel (all), and shin (put in).
%mod% %self% append-lookdesc-noformat Type 'spin dreidel' to use it.
%mod% %self% append-lookdesc-noformat It's currently showing '%sign%'.
~
#16658
Ice Palace completion~
2 o 100
~
set edir %room.bld_dir(east)%
set wdir %room.bld_dir(west)%
* Add hallway
eval hall %%room.%room.enter_dir%(room)%%
if !%hall%
  %door% %room% %room.enter_dir% add 16655
end
eval hall %%room.%room.enter_dir%(room)%%
if %hall%
  * Add throne room
  eval throne %%hall.%room.enter_dir%(room)%%
  if !%throne%
    %door% %hall% %room.enter_dir% add 16656
  end
  * Add ballroom
  eval ball %%hall.%wdir%(room)%%
  if !%ball%
    %door% %hall% %wdir% add 16654
  end
end
* Add 'east' gallery 1
eval egal %%room.%edir%(room)%%
if !%egal%
  %door% %room% %edir% add 16657
end
* Add 'east' gallery 2
eval egal %%room.%edir%(room)%%
if %egal%
  eval eegal %%egal.%edir%(room)%%
  if !%eegal%
    %door% %egal% %edir% add 16657
  end
end
* Add 'west' gallery 1
eval wgal %%room.%wdir%(room)%%
if !%wgal%
  %door% %room% %wdir% add 16657
end
* Add 'east' gallery 2
eval wgal %%room.%wdir%(room)%%
if %wgal%
  eval wwgal %%wgal.%wdir%(room)%%
  if !%wwgal%
    %door% %wgal% %wdir% add 16657
  end
end
detach 16658 %room.id%
~
#16659
Open Stocking (winter wonderland dailies)~
1 c 2
open~
if !%arg% || %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
%echo% It's empty!
return 1
%purge% %self%
~
#16660
Straw Goat vandalism driver~
5 ab 50
~
* configs
set quest_vnum 16660
set protect_times 3
* name lists: count must be the number of words in the list; all list entries must be 1-word
set adj_list dastardly timid wily devious despicable mischievous vexatious roguish puckish fiendish rotten
set adj_count 11
set age_list young older
set age_count 2
set male_list boy man gentleman farmhand merchant apprentice
set male_count 6
set female_list girl woman lady milkmaid merchant apprentice
set female_count 6
* ensure a crowd is present or just spawn one
set found 0
set room %self.room%
set ch %room.people%
while %ch% && !%found%
  if %ch.vnum% == 16660
    set found 1
  end
  set ch %ch.next_in_room%
done
if !%found%
  %load% mob 16660
  %echo% A crowd has formed around the straw goat.
  halt
end
* ensure goat is complete
if !%self.complete%
  halt
end
* pull or set up variables
set is_active 1
if %self.varexists(protect_count)%
  set protect_count %self.protect_count%
else
  set protect_count 0
end
* generate a random vandal: leading adjective
eval pos %%random.%adj_count%%%
while %pos% > 1
  set adj_list %adj_list.cdr%
  eval pos %pos% - 1
done
set vandal %adj_list.car%
* random vandal: optional age
if %random.2% == 2
  eval pos %%random.%age_count%%%
  while %pos% > 1
    set age_list %age_list.cdr%
    eval pos %pos% - 1
  done
  set vandal %vandal% %age_list.car%
end
* random vandal: gender-based name
if %random.2% == 1
  set vandal_sex male
  set vandal_hisher his
  set name_list %male_list%
  set name_count %male_count%
else
  set vandal_sex female
  set vandal_hisher her
  set name_list %female_list%
  set name_count %female_count%
end
eval pos %%random.%name_count%%%
while %pos% > 1
  set name_list %name_list.cdr%
  eval pos %pos% - 1
done
* this is the final step in the name:
set vandal %vandal% %name_list.car%
* store vars now (while running)
remote protect_count %self.id%
remote is_active %self.id%
remote vandal %self.id%
remote vandal_sex %self.id%
* determine cycle count: check for early-protect
if %self.varexists(early_protect)%
  set cycle 5
  rdelete early_protect %self.id%
else
  set cycle 10
end
* BEGIN BIG LOOP
while %cycle%
  * pull these each cycle as they are changed by another script
  set protect_count %self.protect_count%
  set is_active %self.is_active%
  * checks to do on each loop
  if %self.is_flagged(ON-FIRE)%
    wait 60s
    halt
  elseif %protect_count% >= %protect_times%
    * QUEST COMPLETE
    %echo% The celebration comes to an end and giant straw goat is safe from vandals! The citizens take down the goat and the crowd disperses.
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %ch.on_quest(%quest_vnum%)%
        %quest% %ch% trigger %quest_vnum%
        %quest% %ch% finish %quest_vnum%
      elseif %ch.vnum% == 16660
        * despawn crowd
        %purge% %ch%
      end
      set ch %next_ch%
    done
    * done with the goat
    %purge% %self%
    halt
  elseif !%is_active%
    * stopped by the command trigger
    halt
  end
  switch %cycle%
    * cycle counts down; 1 will be the last cycle
    case 10
      %echo% You think you spot %vandal.ana% %vandal% sneaking through the crowd.
    break
    * skip case 9
    case 8
      %echo% You can definitely see %vandal.ana% %vandal% creeping toward the giant straw goat.
    break
    * skip case 7
    case 6
      %echo% You've lost sight of the %vandal%...
    break
    * skip case 5
    case 4
      %echo% %vandal.ana.cap% %vandal% has made it to the giant straw goat!
    break
    case 3
      %echo% The %vandal% crouches behind one leg of the straw goat and begins trying to set it alight!
    break
    case 2
      %echo% The %vandal% strikes at %vandal_hisher% fire starter...
    break
    case 1
      * last cycle: burn it down!
      %echo% The %vandal% manages to light the giant straw goat and it goes up in flames!
      %load% obj 16643 room
      %purge% %self%
    break
  done
  * end of cycle:
  eval cycle %cycle% - 1
  remote cycle %self.id%
  wait %random.2%s
done
~
#16661
Straw Goat protect command~
5 c 0
protect~
* this pairs with trigger 16660 to handle quest 16660
* pull vars
if %self.varexists(protect_count)%
  set protect_count %self.protect_count%
else
  set protect_count 0
end
if %self.varexists(is_active)%
  set is_active %self.is_active%
else
  set is_active 0
end
if %self.varexists(vandal)%
  set vandal %self.vandal%
else
  set vandal vandal
end
if %self.varexists(vandal_sex)%
  set vandal_sex %self.vandal_sex%
else
  set vandal_sex male
end
switch %vandal_sex%
  case male
    set heshe he
  break
  case female
    set heshe she
  break
done
* check args
if !%arg%
  %send% %actor% Protect what?
  halt
elseif %actor.veh_target(%arg%) != %self% && !(giant straw goat /= %arg%) && !(straw goat /= %arg%) && !(goat /= %arg%)
  %send% %actor% You can't protect that.
  halt
elseif %self.is_flagged(ON-FIRE)%
  %send% %actor% It's on fire! You need to douse it before it burns down!
  halt
elseif !%is_active%
  %send% %actor% There's nobody coming for the straw goat right now, but you manage to cause a small distraction by pointing fingers at no one.
  %echoaround% %actor% ~%actor% points into the crowd and you watch as &%actor% searches fruitlessly for someone who isn't there.
  * this will speed up the next vandal
  set early_protect 1
  remote early_protect %self.id%
  halt
end
* successfully thwarted
eval protect_count %protect_count% + 1
remote protect_count %self.id%
set is_active 0
remote is_active %self.id%
* messaging
switch %random.4%
  case 1
    %send% %actor% You push through the crowd to stop the %vandal% but %heshe% sees you and runs off!
    %echoaround% %actor% ~%actor% pushes through the crowd to stop the %vandal%, who runs off when %heshe% sees *%actor%.
  break
  case 2
    %send% %actor% You signal to the city guards, who stop the %vandal% from burning the goat.
    %echoaround% %actor% ~%actor% signals the city guards, who stop the %vandal% from burning the goat.
  break
  case 3
    %send% %actor% You grab the %vandal% before %heshe% can vandalize the goat, but %heshe% slips free and runs off.
    %echoaround% %actor% ~%actor% grabs the %vandal% before %heshe% can vandalize the goat, but %heshe% slips free and runs off.
  break
  case 4
    %send% %actor% The %vandal% sees you coming and runs off.
    %echoaround% %actor% ~%actor% heads toward the %vandal%, who sees *%actor% coming and runs off.
  break
done
~
#16666
Floating lantern expiry~
1 f 0
~
set mob %self.carried_by%
if !%mob%
  halt
end
if !%mob.is_pc%
  halt
end
%echo% The floating lantern burns out.
%mod% %mob% keywords lantern little floating unlit
%mod% %mob% longdesc An unlit lantern floats in the air.
%mod% %mob% shortdesc a floating lantern
%mod% %mob% lookdesc This little paper lantern seems to float all on its own!
%mod% %mob% append-lookdesc Unfortunately, you can't see a way to light the lantern without burning it up.
%mod% %mob% append-lookdesc-noformat (Glowing lanterns can only be summoned once every 6 hours.)
* and purge self silently
return 0
%purge% %self%
~
#16667
Floating lantern~
0 n 100
~
set ch %self.leader%
* determine whether it's a lit lantern this time or not
if %ch%
  if %ch.cooldown(16667)%
    set lit 0
  else
    set lit 1
    nop %ch.set_cooldown(16667,21600)%
  end
else
  * no ch
  set lit 0
end
* set up self
if %lit%
  %load% obj 16667 %self% inv
  %mod% %self% keywords lantern little floating glowing
  %mod% %self% longdesc A glowing lantern floats in the air.
  * %mod% %self% shortdesc a floating lantern
  %mod% %self% lookdesc This little paper lantern, which seems to float all on its own, is bright enough to light up the whole area!
  %mod% %self% append-lookdesc It fills you with a warm cheer, even when it's cold out.
else
  * not lit
  %mod% %self% keywords lantern little floating unlit
  %mod% %self% longdesc An unlit lantern floats in the air.
  * %mod% %self% shortdesc a floating lantern
  %mod% %self% lookdesc This little paper lantern seems to float all on its own!
  %mod% %self% append-lookdesc Unfortunately, you can't see a way to light the lantern without burning it up.
  %mod% %self% append-lookdesc-noformat (Glowing lanterns can only be summoned once every 6 hours.)
end
~
#16668
Eat the gingerbread man~
0 ct 0
eat bite nibble taste~
* check targeting
if %actor.char_target(%arg%)% != %self%
  return 0
  halt
end
* check ownership: pass through to normal commands for everyone else
if %self.leader% != %actor%
  return 0
  halt
end
* check cooldown
if !%actor.cooldown(16668)%
  * lastly, in here, check hunger
  if %actor.hunger% <= 0
    %send% %actor% You eye the gingerbread man, but you're not really hungry.
    return 1
    halt
  end
  * ok: start the cooldown
  %actor.set_cooldown(16668,21600)%
  * load the food version
  %load% obj 16668 %actor% inv
  * make the player eat the object version
  %force% %actor% eat gingerbread-man
  return 1
else
  %send% %actor% You lunge for the gingerbread man but he slips through your grasp...
  %echoaround% %actor% ~%actor% lunges for ~%self% but he slips through ^%actor% grasp...
  %echo% 'Run, run as fast as you can. You can't catch me. I'm the Gingerbread Man!' he shouts as he runs off.
  return 1
end
* purge either way
%purge% %self%
~
#16669
Snowmother spawn/despawn~
0 btw 10
~
set melt 0
set room %self.room%
switch %room.season%
  case winter
    * normal behavior here: spawn a snowman
    if !%room.contents(16669)%
      %load% obj 16669 room
      %echo% A smaller snowman rolls itself out of ~%self%.
    end
  break
  case summer
    set melt 1
  break
  case spring
    if %random.4% == 4
      set melt 1
    end
  break
  case autumn
    if %random.2% == 2
      set melt 1
    end
  break
done
if %melt%
  %echo% ~%self% melts away to nothing!
  if %self.leader%
    if !%self.leader.cooldown(16669)%
      nop %self.leader.set_cooldown(16669,3600)%
      %load% obj 16615 room
    end
  end
  %purge% %self%
end
~
#16675
Citizen dances~
0 bw 50
~
switch %random.5%
  case 1
    %echo% ~%self% dances around!
  break
  case 2
    %echo% ~%self% sings along!
  break
  case 3
    %echo% ~%self% twirls around with glee!
  break
  case 4
    %echo% ~%self% leaps around as &%self% dances!
  break
  case 5
    %echo% ~%self% stomps to the beat!
  break
done
~
#16676
Winter Wonderland music quests: play~
1 c 3
play~
return 0
set music_score 0
remote music_score %self.id%
if !%self.varexists(music_count)%
  set music_count 0
  remote music_count %self.id%
end
if !%self.has_trigger(16677)%
  attach 16677 %self.id%
end
~
#16677
Winter Wonderland music quests: detect playing~
1 b 100
~
set questid %self.vnum%
if %self.carried_by%
  set actor %self.carried_by%
elseif %self.worn_by%
  set actor %self.worn_by%
end
if %actor.action% != playing
  detach 16677 %self.id%
  halt
end
set music_score %self.music_score%
set music_count %self.music_count%
switch %questid%
  case 16676
    * 10x citizens must dance
    set any 0
    set person %self.room.people%
    while %person%
      if %music_score% >= %random.100%
        if %person.is_npc% && !%person.disabled% && %person.empire% == %actor.empire% && %person.mob_flagged(SPAWNED)% && %person.mob_flagged(HUMAN)% && !%person.has_trigger(16675)%
          set any 1
          eval music_count %music_count% + 1
          %echo% ~%person% starts dancing!
          attach 16675 %person.id%
          %morph% %person% 16675
          eval loss %%random.%music_score%%%
          eval music_score %music_score% - %loss%
        end
      end
      set person %person.next_in_room%
    done
    if %any%
      remote music_count %self.id%
      remote music_score %self.id%
      %send% %actor% (That's %music_count%!)
    else
      eval music_score %music_score% + 25
      remote music_score %self.id%
    end
    * check done
    if %music_count% >= 10
      %quest% %actor% trigger %questid%
    end
  break
  case 16677
    * 4x dedicate buildings
    set room %actor.room%
    * check already played here
    set mob %room.people%
    while %mob%
      if %mob.vnum% >= 16675 && %mob.vnum% <= 16677
        halt
      end
      set mob %mob.next_in_room%
    done
    * ensure dedicate here
    set any 0
    if %room.bld_flagged(DEDICATE)% && %actor.canuseroom_member(%room%)%
      set any 1
    elseif %room.in_vehicle% && %actor.empire% == %room.in_vehicle.empire%
      if %room.in_vehicle.is_flagged(DEDICATE)%
        set any 1
      end
    end
    if !%any%
      set veh %room.vehicles%
      while %veh% && !%any%
        if %veh.is_flagged(DEDICATE)% && %actor.empire% == %veh.empire%
          set any 1
        end
        set veh %veh.next_in_room%
      done
    end
    if !%any%
      %send% %actor% You need to play the instrument at a dedicate-able building.
      halt
    end
    * check music score
    if %music_score% >= %random.100%
      * success!
      eval mobv 16674 + %random.3%
      %load% mob %mobv%
      eval mobv 16674 + %random.3%
      %load% mob %mobv%
      eval music_count %music_count% + 1
      remote music_count %self.id%
      %send% %actor% Your music has drawn a crowd (that's %music_count%)!
      %echoaround% %actor% |%actor% music has drawn a crowd!
    else
      eval music_score %music_score% + 25
      remote music_score %self.id%
    end
    if %music_count% >= 4
      %quest% %actor% trigger %questid%
    end
  break
done
if %actor.quest_finished(%questid%)%
  %quest% %actor% finish %questid%
end
~
#16678
Open Stocking (winter wonderland dailies) 2021-2022~
1 c 2
open~
if !%arg% || %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
if !%event.running(10700)%
  %echo% It's empty! You must have missed Christmas.
  return 1
  %purge% %self%
  halt
end
set roll %random.1000%
if %roll% <= 5
  * 0.5% chance: a strange crystal seed
  set vnum 600
elseif %roll% <= 20
  * 1.5% chance: a shimmering magewood jar (nordlys forest)
  set vnum 16696
elseif %roll% <= 120
  * 6% chance: minipet whistle
  set vnum 10729
elseif %roll% <= 370
  * 25% chance: some chocolate coins (+stats)
  set vnum 16660
elseif %roll% <= 620
  * 25% chance: some snowball cookies (+inventory)
  set vnum 16661
elseif %roll% <= 870
  * 25% chance: some thumbprint cookies (+move regen)
  set vnum 16662
else
  * Remaining %: small resource shipment
  set vnum 11513
end
%send% %actor% You open @%self%...
%echoaround% %actor% ~%actor% opens @%self%...
%load% obj %vnum% %actor%
set gift %actor.inventory%
if %gift.vnum% == %vnum%
  %echo% It contained @%gift%!
else
  %echo% It's empty!
end
return 1
%purge% %self%
~
#16680
summon winter demons~
1 c 2
use~
if !%arg%
  return 0
  halt
end
if %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
set room %self.room%
if %room.template% != 10706
  %send% %actor% You must find the icy cave in the Winter Adventure to do this.
  halt
end
* check if mob present
set ch %room.people%
while %ch%
  if (%ch.vnum% == 16613 || %ch.vnum% == 16680)
    %send% %actor% You better deal with the demon that's already here before invoking another one.
    halt
  end
  set ch %ch.next_in_room%
done
* check if summoned too recently
if %room.contents(16614)%
  %send% %actor% There is still a strong sense of rage in this place.
  halt
end
set arg2 %arg.cdr%
if !%arg2%
  %send% %actor% What difficulty would you like to summon a winter demon at? (Normal, Hard, Group or Boss)
  return 1
  halt
end
set arg %arg2%
if normal /= %arg%
  %send% %actor% Setting difficulty to Normal...
  set diff 1
elseif hard /= %arg%
  %send% %actor% Setting difficulty to Hard...
  set diff 2
elseif group /= %arg%
  %send% %actor% Setting difficulty to Group...
  set diff 3
elseif boss /= %arg%
  %send% %actor% Setting difficulty to Boss...
  set diff 4
else
  %send% %actor% That is not a valid difficulty level for this encounter.
  halt
  return 1
end
eval WhichDemon %random.2%
switch %WhichDemon%
  case 1
    set WhichDemon 16613
  break
  case 2
    set WhichDemon 16680
  break
done
set cycles_left 3
while %cycles_left% >= 0
  if (%actor.room% != %room%) || !%actor.can_act%
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 3
      %echoaround% %actor% |%actor% summoning is interrupted.
      %send% %actor% Your summoning is interrupted.
    else
      %send% %actor% You can't do that now.
    end
    halt
  end
  switch %cycles_left%
    case 3
      %echo% You invoke the demon and start to look for a good hiding place to observe from.
    break
    case 2
      %echo% A thump, a bump, and a clunk can be heard.
    break
    case 1
      if %WhichDemon% == 16613
        set DemonDesc tall
      elseif %WhichDemon% == 16680
        set DemonDesc broad
      end
      %echo% A shadow of someone or something %DemonDesc% can be seen in the middle of the cave.
    break
    case 0
      * Leave the loop
    break
  done
  if %cycles_left% > 0
    wait 5 sec
  end
  eval cycles_left %cycles_left% - 1
done
%load% mob %WhichDemon%
%load% obj 16614 room
set mob %room.people%
if %mob.vnum% != 16613 && %mob.vnum% != 16680
  %echo% Something went wrong...
  halt
end
%echo% ~%mob% sidles into view and you rush forward to engage!
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
remote diff %mob.id%
%echo% @%self% bursts into blue flames and rapidly crumbles to ash.
%purge% %self%
~
#16681
krampus healing tracker~
0 e 1
rejuvenation healing~
if !%self.varexists(LastHealer)%
  set LastHealer %actor.id%
  remote LastHealer %self.id%
end
if %actor.id% == %self.LastHealer%
  if %self.cooldown(16680)%
    halt
  end
  nop %self.set_cooldown(16680, 25)%
  set LastHealer %actor.id%
  remote LastHealer %self.id%
end
if %self.varexists(CatchAHeal)%
  set CatchAHeal %self.CatchAHeal%
end
eval CatchAHeal %CatchAHeal% + 1
remote CatchAHeal %self.id%
wait 2 sec
say They deserved that pain. Now I have to make you hurt instead!
set difficulty 0
if %self.mob_flagged(hard)%
  set difficulty 1
end
if %self.mob_flagged(group)%
  eval difficulty %difficulty% + 1
end
switch %random.3%
  case 1
    %echo% ~%self% lashes ~%actor% with a birch switch!
    eval DamVal 50 + %difficulty% * 5
    %dot% #16681 %actor% %DamVal% 110 physical 4
  break
  case 2
    %echo% ~%self% slams a chain-wrapped fist into |%actor% torso!
    %damage% %actor% 100 physical
  break
  case 3
    %send% %actor% ~%self% snaps ^%self% fingers and ~%actor% begin to glow red.
    %echoaround% %actor% ~%self% snaps ^%self% fingers and ~%actor% begins to glow red.
    eval debuff %actor.bonus_healing% / 3
    dg_affect #16682 %actor% bonus-healing %debuff% 25
  break
done
~
#16682
krampus low health recovery~
0 l 20
~
if %self.cooldown(16683)%
  halt
end
if !%self.varexists(CatchAHeal)%
  halt
end
if %self.CatchAHeal% < 1
  halt
end
if !%self.varexists(RunHeal)%
  set RunHeal 0
else
  set RunHeal %self.RunHeal%
end
eval RunHeal %RunHeal% + 1
remote RunHeal %self.id%
eval RunHeal 110 - %RunHeal% * 10
set CatchAHeal %self.CatchAHeal%
if %RunHeal% <= 20
  halt
end
while %CatchAHeal%
  eval percent %self.health% * 100 / %self.maxhealth%
  if %percent% < %RunHeal%
    %heal% %self% health
    eval CatchAHeal %CatchAHeal% - 1
  else
    set CatchAHeal 0
    %heal% %self% debuffs
  end
done
wait 1
%echo% ~%self% seems to swell with rage as &%self% fights with new vigour!
nop %self.set_cooldown(16683, 60)%
~
#16683
snow cube summon~
1 c 0
~
* No script
~
#16684
Krampus combat: Birch Bundle, Sack Up, Hornbutt, Rage heal/buff~
0 k 100
~
if %self.cooldown(16680)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4
  set num_left 4
end
* pick
eval which %%random.%num_left%%%
set old %moves_left%
set moves_left
set move 0
while %which% > 0
  set move %old.car%
  if %which% != 1
    set moves_left %moves_left% %move%
  end
  set old %old.cdr%
  eval which %which% - 1
done
set moves_left %moves_left% %old%
* store
eval num_left %num_left% - 1
remote moves_left %self.id%
remote num_left %self.id%
* perform move
scfight lockout 16680 30 35
if %move% == 1
  * Birch bundle AOE
  scfight clear dodge
  %regionecho% %room% 25 &&y~%self% shouts, 'I see everyone has been naughty!'&&0
  %echo% &&G~%self% whips out a bundle of birch rods and begins winding up...&&0
  eval dodge %diff% * 40
  dg_affect #16684 %self% DODGE %dodge% 20
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  scfight setup dodge all
  wait 3 s
  if %self.disabled%
    dg_affect #16684 %self% off
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %echo% &&G**** &&Z~%self% starts spinning... it's now or never if you want to avoid those birch rods! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    scfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfdodge)%
          set hit 1
          %echo% &&G~%self% smacks ~%ch% with ^%self% bundle of birch rods!&&0
          %send% %ch% That really hurt! You seem to be bleeding.
          %dot% #16685 %ch% 100 40 physical 25
        elseif %ch.is_pc%
          %send% %ch% &&G~%self% whirls past you, narrowly missing you with the birch rods!&&0
          if %diff% == 1
            dg_affect #16686 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&G**** Here come those birch rods again... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
    eval cycle %cycle% + 1
  done
  dg_affect #16684 %self% off
  if !%hit%
    if %diff% < 3
      %echo% &&G~%self% seems to exhaust *%self%self from all that spinning.&&0
      dg_affect #16687 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
elseif %move% == 2 && !%self.aff_flagged(BLIND)%
  * Sack up
  scfight clear struggle
  %echo% &&G~%self% opens an oversized burlap sack...&&0
  wait 3 s
  if %self.disabled%
    halt
  end
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  if %self.fighting% == %targ% && %diff% < 4
    dg_affect #16687 %self% HARD-STUNNED on 20
  end
  %send% %targ% &&G**** &&Z~%self% snatches you up in the burlap sack! You can't move! ****&&0 (struggle)
  %echoaround% %targ% &&G~%self% snatches ~%targ% up in the burlap sack!&&0
  scfight setup struggle %targ% 20
  * messages
  set scf_strug_char You try to wiggle out of the burlap sack...
  set scf_strug_room You hear ~%%actor%% trying to wiggle out of the burlap sack...
  remote scf_strug_char %actor.id%
  remote scf_strug_room %actor.id%
  set scf_free_char You wiggle out of the burlap sack!
  set scf_free_room ~%%actor%% manages to wiggle out of the burlap sack!
  remote scf_free_char %actor.id%
  remote scf_free_room %actor.id%
  set cycle 0
  set done 0
  while %cycle% < 5 && !%done%
    wait 4 s
    if %targ.id% == %targ_id% && %targ.affect(9602)% && %diff% > 1
      %send% %targ% &&GThis is the scratchiest burlap sack you've ever been stuck in! It really hurts!&&0
      %dot% #16688 %targ% 100 40 physical 25
    else
      set done 1
    end
    eval cycle %cycle% + 1
  done
  scfight clear struggle
  dg_affect #16687 %self% off
elseif %move% == 3
  * Hornbutt
  scfight clear dodge
  %echo% &&G~%self% sharpens ^%self% horn with a piece of coal...&&0
  wait 3 s
  if %self.disabled%
    halt
  end
  if %self.aff_flagged(BLIND)%
    halt
  end
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  if %self.fighting% == %targ% && %diff% == 1
    dg_affect #16687 %self% HARD-STUNNED on 20
  end
  %send% %targ% &&G**** &&Z~%self% leans forward and takes aim... at you! ****&&0 (dodge)
  %echoaround% %targ% &&G~%self% leans forward and takes aim... at ~%targ%!&&0
  scfight setup dodge %targ%
  if %diff% > 1
    set ouch 100
  else
    set ouch 50
  end
  set cycle 0
  set done 0
  while %cycle% < %diff% && !%done%
    wait 4 s
    if %targ.id% != %targ_id%
      set done 1
    elseif !%targ.var(did_scfdodge)%
      %echo% &&G~%self% hornbutts ~%targ%!&&0
      %send% %targ% That really hurts!
      %damage% %targ% %ouch% physical
    end
    eval cycle %cycle% + 1
    if %cycle% < %diff% && !%done%
      %send% %targ% &&G**** &&Z~%self% looks like &%self%'s going to try to hit you again... ****&&0 (dodge)
      scfight setup dodge %targ%
    end
  done
  scfight clear dodge
  dg_affect #16687 %self% off
elseif %move% == 4
  * Rage heal or buff
  scfight clear interrupt
  %echo% &&G**** &&Z~%self% is fuming with rage... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  scfight setup interrupt all
  if %self.diff% < 3 || %room.players_present% < 2
    set requires 1
  else
    set requires 2
  end
  wait 4 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if %self.var(sfinterrupt_count,0)% < %requires%
    %echo% &&G**** &&Z~%self% shouts incoherently as the rage builds up within *%self%... ****&&0 (interrupt)
  end
  wait 4 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if %self.var(sfinterrupt_count,0)% >= %requires%
    %echo% &&G~%self% is distracted from whatever &%self% was doing... thankfully!&&0
    if %diff% == 1
      dg_affect #16687 %self% HARD-STUNNED on 5
    end
    wait 30 s
  else
    %echo% &&G~%self% seems to swell with rage as &%self% fights with new vigor!
    eval hitprc %self.health% * 100 / %self.maxhealth%
    if %health% <= 20
      * heal
      %heal% %self% health 100
    else
      * buff
      eval amount %diff% * 20
      dg_affect #16689 %self% BONUS-PHYSICAL %amount% 300
    end
  end
  scfight clear interrupt
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#16686
Winter Wonderland: Boss fight tester~
1 c 2
test~
return 1
if !%arg% \|\| !%arg.cdr%
  %send% %actor% Usage: test <grinch \| krampus> <normal \| hard \| group \| boss>
  halt
end
* mob?
set which %arg.car%
if grinchy /= %which%
  set vnum 16613
elseif krampus /= %which%
  set vnum 16680
else
  %send% %actor% Usage: test <grinch \| krampus> <normal \| hard \| group \| boss>
  halt
end
* diff?
set arg2 %arg.cdr%
if normal /= %arg2%
  set diff 1
elseif hard /= %arg2%
  set diff 2
elseif group /= %arg2%
  set diff 3
elseif boss /= %arg2%
  set diff 4
else
  %send% %actor% Usage: test <grinch \| krampus> <normal \| hard \| group \| boss>
  halt
end
* remove old mob?
set room %self.room%
set old %room.people(16613)%
if %old%
  %echo% ~%old% vanishes...
  %purge% %old%
end
set old %room.people(16680)%
if %old%
  %echo% ~%old% vanishes...
  %purge% %old%
end
* new mob
%load% mob %vnum%
set mob %room.people%
if %mob.vnum% != %vnum%
  %send% %actor% An error occurred when loading the mob to test.
  halt
end
* messaging
%echo% ~%mob% appears for testing!
* Clear existing difficulty flags and set new ones.
remote diff %mob.id%
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %diff% == 1
  * Then we don't need to do anything
  %echo% ~%mob% has been set to Normal.
elseif %diff% == 2
  %echo% ~%mob% has been set to Hard.
  nop %mob.add_mob_flag(HARD)%
elseif %diff% == 3
  %echo% ~%mob% has been set to Group.
  nop %mob.add_mob_flag(GROUP)%
elseif %diff% == 4
  %echo% ~%mob% has been set to Boss.
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
nop %mob.unscale_and_reset%
* remove no-attack
if %mob.aff_flagged(!ATTACK)%
  dg_affect %mob% !ATTACK off
end
* unscale and restore me
nop %mob.unscale_and_reset%
~
#16690
feed the vortex~
1 c 2
look put~
set Needs 50
if %actor.aff_flagged(blind)%
  return 0
  halt
end
if !%actor.on_quest(16690)% && (%actor.obj_target(%arg.cdr%)% == %self% || %actor.obj_target(%arg.car%)% == %self%)
  %send% %actor% @%self% suddenly vanishes!
  %purge% %self%
  halt
end
set Cookie16660 %self.Cookie16660%
set Cookie16661 %self.Cookie16661%
set Cookie16662 %self.Cookie16662%
* only looking at it?
if %cmd% == look
  if %arg.car% == in || %arg.car% == at
    * pull off a leading in/at
    set arg %arg.cdr%
  end
  if %actor.obj_target(%arg.car%)% != %self%
    return 0
    halt
  end
  * put in a variable check and echo it.
  set num 4
  if %Cookie16660% > 1
    set string1 %Cookie16660% chocolate coins
  elseif %Cookie16660% == 1
    set string1 a chocolate coin
  else
    eval num %num% - 1
  end
  if %Cookie16661% > 1
    set string2 %Cookie16661% clusters of snowball cookies
  elseif %Cookie16661% == 1
    set string2 a cluster of snowball cookies
  else
    eval num %num% - 1
  end
  if %Cookie16662% > 1
    set string3 %Cookie16662% handfuls of thumbprint cookies
  elseif %Cookie16662% == 1
    set string3 a handful of thumbprint cookies
  else
    eval num %num% - 1
  end
  switch %num%
    case 1
      set string a mess of snow
    break
    case 2
      if %Cookie16660% >= 1
        set string %string1%
      elseif %Cookie16661% >= 1
        set string %string2%
      else
        set string %string3%
      end
    break
    case 3
      if %Cookie16660% == 0
        set string %string2% and %string3%
      elseif %Cookie16661% == 0
        set string %string1% and %string3%
      else
        set string %string1% and %string2%
      end
    break
    case 4
      set string %string1%, %string2%, and %string3%
    break
  done
  %send% %actor% You see %string% floating around inside @%self%.
  %echoaround% %actor% ~%actor% looks into @%self%.
  halt
end
* otherwise the command was 'put'
if %actor.obj_target(%arg.cdr%)% != %self%
  return 0
  halt
end
* detect arg
set PutObj %arg.car%
set CookieCount 0
* check for "all" arg
if (%PutObj% == all || %PutObj% == all.Cookies || %putObj% == all.Cookie)
  set all 1
else
  set all 0
end
* and loop
eval CookieTotal %Cookie16660% + %Cookie16661% + %Cookie16662%
set item %actor.inventory%
while (%item% && (%all% || %CookieCount% == 0) && %CookieTotal% < %Needs%)
  set next_item %item.next_in_list%
  * use %ok% to control what we do in this loop
  if %all%
    set ok 1
  else
    * single-target: make sure this was the target
    if %actor.obj_target_inv(%PutObj%)% == %item%
      set ok 1
    else
      set ok 0
    end
  end
  * next check the obj type if we got the ok
  if %ok%
    if %item.vnum% < 16660 || %item.vnum% > 16662
      if %all%
        set ok 0
      else
        %send% %actor% You can't put @%item% in @%self%... Only treats from the stockings can pass through the bubble!
        * Break out of the loop early since it was a single-target fail
        halt
      end
    end
  end
  * still ok? see if we need one of these
  if %ok% && !%item.is_flagged(*KEEP)%
    set WhatCookie Cookie%item.vnum%
    eval CookieCount %CookieCount% + 1
    %send% %actor% # You stash @%item% in @%self%.
    %echoaround% %actor% # ~%actor% puts @%item% in @%self%.
    eval %WhatCookie% %%self.%WhatCookie%%% + 1
    remote %WhatCookie% %self.id%
    %purge% %item%
    eval CookieTotal %CookieTotal% + 1
  end
  * and repeat the loop
  set item %next_item%
done
* did we fail?
if !%CookieCount%
  if %all%
    %send% %actor% You didn't have anything you could put into @%self%.
  else
    %send% %actor% You don't seem to have %PutObj.ana% %PutObj%.
  end
end
* get a Cookie total and see if the quest is over
if %CookieTotal% >= %Needs%
  %quest% %actor% finish 16690
  %purge% %self%
end
~
#16691
set the start variables on the vortex~
1 n 100
~
set Cookie16660 0
set Cookie16661 0
set Cookie16662 0
remote Cookie16660 %self.id%
remote Cookie16661 %self.id%
remote Cookie16662 %self.id%
~
#16695
Capture nordlys in jar~
1 c 2
recapture~
* recapture nordlys
set room %actor.room%
if !%arg% || !(nordlys /= %arg%)
  return 0
  halt
end
if !%actor.canuseroom_member(%room%)%
  %send% %actor% You don't have permission to recapture anything here.
  return 1
  halt
end
switch %room.sector_vnum%
  case 16699
    set new_v 10564
    set new_t The trees shrink and shake as the magic leaves them and evergreen needles sprout from their branches.
  break
  case 16698
    %send% %actor% You must let the nordlys trees grow to full strength before you can recapture the lights.
    return 1
    halt
  break
  case 16697
    %send% %actor% You must let the nordlys trees grow before you can recapture the lights.
    return 1
    halt
  break
  default
    %send% %actor% There's no nordlys to recapture here.
    return 1
    halt
  break
done
%send% %actor% You open the lid of the magewood jar and hold it up toward the sky...
%echoaround% %actor% ~%actor% opens the lid of @%self% and holds it up toward the sky...
%echo% %new_t%
%terraform% %room% %new_v%
* return full jar
%load% obj 16696 %actor% inv
set jar %actor.inventory(16696)%
if %jar% && %self.is_flagged(*KEEP)%
  nop %jar.flag(*KEEP)%
end
return 1
%purge% %self%
~
#16696
Open nordlys jar~
1 c 2
open~
* open <self>
set room %actor.room%
set targ %actor.obj_target(%arg%)%
if %targ% != %self% && %targ.vnum% != 16695
  return 0
  halt
end
if !%actor.canuseroom_member(%room%)%
  %send% %actor% You don't have permission to use @%self% here.
  return 1
  halt
end
%send% %actor% You open @%self%...
%echoaround% %actor% ~%actor% opens @%self%...
switch %room.sector_vnum%
  case 10562
    set new_v 16698
    %echo% The evergreen shifts and twists into a nordlys tree as the light from the jar is captured in its boughs!
  break
  case 10563
    set new_v 16698
    %echo% The evergreens shift and twist into a massive nordlys tree as the light from the jar is captured in their boughs!
  break
  case 10564
    set new_v 16699
    %echo% The evergreens shift and twist into a grove of massive nordlys trees as the light from the jar is captured in their boughs!
  break
  case 10565
    set new_v 16699
    %echo% The evergreens shift and twist into a grove of massive nordlys trees as the light from the jar is captured in their boughs!
  break
  case 10566
    set new_v 16697
    %echo% The stumps grow pale and begin leaking green and magenta mana as the light from the jar soaks into their roots!
  break
  default
    %echo% The nordlys lights swirl through the air before streaming back into their jar!
    return 1
    halt
  break
done
%terraform% %room% %new_v%
%load% obj 16695 %actor% inv
set jar %actor.inventory(16695)%
if %jar% && %self.is_flagged(*KEEP)%
  nop %jar.flag(*KEEP)%
end
return 1
%purge% %self%
~
#16699
immortal event point modifier~
1 c 2
modify~
if !%actor.is_immortal%
  %send% %actor% This isn't for your use!
  %purge% %self%
  halt
end
if !%arg%
  %send% %actor% Who are you attempting to modify the event points of and by how much?
  %send% %actor% Use (modify <player name> <possative or negative number>
  halt
end
if !%event.running(10700)%
  %send% %actor% The winter holiday event isn't currently running.
  halt
end
set target %actor.char_target(%arg.car%)%
if !%target%
  %send% %actor% You don't see them here.
  halt
end
set val %target.gain_event_points(10700,%arg.cdr%)%
%send% %actor% You have modified |%target% event points by %arg.cdr%.
%send% %actor% &%target% now has %val% points.
~
$
