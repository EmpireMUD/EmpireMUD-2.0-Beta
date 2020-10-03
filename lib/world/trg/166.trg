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
set player_name %actor.name%
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
      %send% %actor% As you place %self.shortdesc% on top, the snowman wiggles as though it is going to fall over, but then begins to dance around!
      %echoaround% %actor% As %player_name% places %self.shortdesc% on top, the snowman wiggles as though it is going to fall over, but then begins to dance around!
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
  elseif (snowman /= %arg%)
    %send% %actor% You don't have enough snowballs yet.
    halt
  end
end
if %cmd% == scoop && snowball /= %arg%
  %send% %actor% You can only make snowballs when scooping.
  halt
end
if %actor.has_resources(16605,3)%
  %send% %actor% You already have three snowballs, just make the snowman.
  halt
end
set room %self.room%
if %room.function(DRINK-WATER)% || %room.sector_flagged(DRINK)%
  %send% %actor% You dip %self.shortdesc% into the water and it freezes the liquid into snow.
  %echoaround% %actor% %player_name% dips %self.shortdesc% into the water and it freezes the liquid into snow.
else
  %send% %actor% You can't find snow worthy water here. Try somewhere you can drink fresh water.
  halt
end
set BuildRoom %self.room%
wait 5 s
if !%actor.fighting% && %BuildRoom% == %self.room%
  %send% %actor% You shape the newly made snow into a perfect snowball for your snowman.
  %echoaround% %actor% %player_name% shapes the newly made snow into a perfect snowball for %actor.hisher% snowman.
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
  %send% %actor% You don't need %self.shortdesc%, might as well just toss it.
else
  set target %actor.char_target(%arg.cdr%)%
  if %actor.on_quest(16602)% && !(%target.vnum% == 10703)
    %send% %actor% That isn't the right blood!
    halt
  elseif %actor.on_quest(16603)%
    if %target.vnum% != 10700 && %target.vnum% != 10705 && %target.vnum% != 9175
      %send% %actor% That isn't the right blood!
      halt
    end
  end
  %send% %actor% You use all of your vampire know-how to prick %target.name%'s neck and hide the vial of blood before anyone notices.
  %echoaround% %actor% %actor.name% does something near %target.name%, but it happens so fast you can't tell what.
  %load% obj 16603 %actor% inv
  set vial %self.carried_by.obj_target_inv(vial)%
  %mod% %vial% keywords vial blood %target.name%
  %mod% %vial% shortdesc a vial of %target.name%'s blood
  %mod% %vial% longdesc A vial of %target.name%'s blood is here for the taking.
end
%purge% %self%
~
#16602
start a winter holiday quest~
2 u 0
~
if %questvnum% == 16607
  %load% obj 16608 %actor% inv
elseif %questvnum% == 16602 || %questvnum% == 16603
  %load% obj 16601 %actor% inv
elseif %questvnum% == 16604 || %questvnum% == 16605
  %load% obj 16604 %actor% inv
elseif %questvnum% == 16606
  %load% obj 16600 %actor% inv
elseif %questvnum% == 16607
  %load% obj 16608 %actor% inv
elseif %questvnum% == 16610
  %load% obj 16610 %actor% inv
elseif %questvnum% == 16611
  %load% obj 16611 %actor% inv
elseif %questvnum% == 16613
  %load% obj 16613 %actor% inv
elseif %questvnum% == 16618
  %load% obj 16618 %actor% inv
elseif %questvnum% == 16617
  %load% obj 16625 %actor% inv
elseif %questvnum% == 16620
  %load% obj 16620 %actor% inv
elseif %questvnum% == 16626
  %load% obj 16627 %actor% inv
elseif %questvnum% == 16628
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
end
~
#16603
grinchy death~
0 f 100
~
return 0
%echo% %self.name% throws ornaments everywhere and vanishes in the chaos!
set person %self.room.people%
while %person%
  %quest% %person% trigger 16613
  if %person.is_pc% && %person.quest_finished(16613)%
    %quest% %person% finish 16613
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
%echo% %self.name% shoves christmas fudge in %self.hisher% face.
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
  %send% %actor% You light %old_xmas_tree.shortdesc% on fire!
  %echoaround% %actor% %actor.name% lights %old_xmas_tree.shortdesc% on fire!
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
if %self.varexists(winter_holiday_sect_check)%
  set winter_holiday_sect_check %self.winter_holiday_sect_check%
  if %winter_holiday_sect_check% == 10565 || %winter_holiday_sect_check% == 10564 || %winter_holiday_sect_check% == 10563 || %winter_holiday_sect_check% == 10562
    %mod% %xmas_tree% keywords tree spruce Christmas tall
    %mod% %xmas_tree% shortdesc a spruce Christmas tree
    %mod% %xmas_tree% longdesc A tall spruce Christmas tree cheers the citizens.
  elseif %winter_holiday_sect_check% == 604
    %mod% %xmas_tree% keywords tree enchanted Christmas tall
    %mod% %xmas_tree% shortdesc an enchanted Christmas tree
    %mod% %xmas_tree% longdesc A tall enchanted Christmas tree cheers the citizens.
  elseif %winter_holiday_sect_check% == 4
    %mod% %xmas_tree% keywords tree Christmas unimpressive tall
    %mod% %xmas_tree% shortdesc an unimpressive Christmas tree
    %mod% %xmas_tree% longdesc A tall but unimpressive Christmas tree cheers the citizens.
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
return 0
if %actor.inventory(16606)%
  %send% %actor% You really should get this tree back to your city center and plant it.
  halt
end
set winter_holiday_sect_check %self.room.sector_vnum%
if !(%winter_holiday_sect_check% == 4 || %winter_holiday_sect_check% == 604 || (%winter_holiday_sect_check% >= 10562 && %winter_holiday_sect_check% <= 10565))
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
if !%actor.on_quest(16607)%
  halt
end
if %actor.inventory(16606)%
  halt
end
eval maxcarrying %actor.maxcarrying% - 1
if %actor.carrying% >= %maxcarrying%
  halt
end
while %arg%
  if %arg.cdr% == tree.
    set tree_type %actor.inventory()%
    set tree_type %tree_type.vnum%
  end
  set arg %arg.cdr%
done
set winter_holiday_sect_check %self.winter_holiday_sect_check%
set tree_roll %random.100%
if !(%tree_type% == 10558 || %tree_type% == 603 || %tree_type% == 120)
  halt
end
if %winter_holiday_sect_check% == 10565 && %tree_roll% <= 60
  %purge% %actor.inventory(10558)%
elseif %winter_holiday_sect_check% == 10564 && %tree_roll% <= 45
  %purge% %actor.inventory(10558)%
elseif %winter_holiday_sect_check% == 10563 && %tree_roll% <= 30
  %purge% %actor.inventory(10558)%
elseif %winter_holiday_sect_check% == 10562 && %tree_roll% <= 15
  %purge% %actor.inventory(10558)%
elseif %winter_holiday_sect_check% == 604 && %tree_roll% <= 13
  %purge% %actor.inventory(603)%
elseif %winter_holiday_sect_check% == 4 && %tree_roll% <= 30
  %purge% %actor.inventory(120)%
else
  halt
end
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
%send% %actor% You spread %self.shortdesc%, lay down, and swiftly make a snow angel on the ground.
%echoaround% %actor% %actor.name% spreads %self.shortdesc%, lays down, and swiftly makes a snow angel on the ground.
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
set MoveDir %actor.parse_dir(%arg%)%
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
  %send% %actor% %self.name% is highly upset with you and tosses you out!
  %echoaround% %actor% %self.name% grabs %actor.name% and throws %actor.himher% out!
  %teleport% %actor% %instance.location%
  %force% %actor% look
  halt
elseif %actor.cooldown(16601)%
  %send% %actor% You probably shouldn't do that, %self.name% almost caught you once already.
  nop %actor.set_cooldown(16601, 30)%
  halt
end
set theft_roll %random.100%
if %actor.skill(stealth)% >= %theft_roll%
  %load% obj 16612 %actor% inv
  set item %actor.inventory()%
  %send% %actor% You find %item.shortdesc%!
else
  nop %actor.set_cooldown(16601, 5)%
  %send% %actor% %self.name% turns in your direction and you have to abort your theft.
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
  set difficulty 1
elseif hard /= %arg%
  %send% %actor% Setting difficulty to Hard...
  set difficulty 2
elseif group /= %arg%
  %send% %actor% Setting difficulty to Group...
  set difficulty 3
elseif boss /= %arg%
  %send% %actor% Setting difficulty to Boss...
  set difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
set cycles_left 3
while %cycles_left% >= 0
  if (%actor.room% != %room%) || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 3
      %echoaround% %actor% %actor.name%'s summoning is interrupted.
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
%echo% %mob.name% sidles into view and you rush forward with %self.shortdesc%!
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
%echo% %self.shortdesc% bursts into blue flames and rapidly crumbles to ash.
%purge% %self%
~
#16614
grinchy demon combat 1~
0 bw 40
~
if !%self.fighting%
  halt
end
if %self.cooldown(16617)%
  halt
end
set person %self.room.people%
while %person%
  if %person.vnum% == 16614
    halt
  end
  set person %person.next_in_room%
done
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
set grinch_roll %random.100%
if %grinch_level% == 1 && %grinch_roll% > 30
  halt
elseif %grinch_level% == 2 && %grinch_roll% > 60
  halt
end
%echo% %self.name% shouts, "get 'hem Max!"
%load% mob 16614 ally
nop %self.set_cooldown(16617, 90)%
~
#16615
grinchy combat 2~
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
        %send% %actor% %self.name% pulls out a gift and throws it at you!
        %echoaround% %actor% %self.name% pulls out a gift and throws it at %actor.name%!
        %damage% %actor% 100
      elseif %grinch_level% == 2
        set grinch_target %random.enemy%
        %send% %grinch_target% %self.name% pulls out a gift and throws it at you!
        %echoaround% %grinch_target% %self.name% pulls out a gift and throws it at %grinch_target.name%!
        %damage% %grinch_target% 100
      elseif %grinch_level% == 3
        %echo% %self.name% pulls out a gift and throws it on the ground, causing everyone to go flying!
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
        %send% %actor% %self.name% glares at you and you shiver in fear.
        %echoaround% %actor% %self.name% glares at %actor.name% and %actor.heshe% shudders in fear.
        dg_affect #16612 %actor% dodge -%actor.level% %grinch_timer%
      end
    end
  break
  case 3
    if !%self.cooldown(16614)%
      nop %self.set_cooldown(16614, 45)%
      set running 1
      remote running %self.id%
      %echo% %self.name% starts to swing a thirty-nine-and-a-half foot pole.
      %echo% &&YYou'd better duck!&&0
      wait 10 sec
      %echo% %self.name% swings the pole like a bat!
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
            %echoaround% %person% %person.name% is knocked senseless by the thirty-nine-and-a-half foot pole!
            eval grinch_damage %grinch_level% * 50 + 20
            %damage% %person% %grinch_damage% physical
            dg_affect #16616 %person% STUNNED on 5
          else
            %send% %person% You limbo under the pole and safely straighten back up.
            %echoaround% %person% %person.name% safely limbos under the pole.
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
%echoaround% %actor% %actor.name% stands %actor.hisher% ground and prepares to duck...
remote command_%actor.id% %self.id%
~
#16617
melting snowman~
0 i 100
~
wait 1 s
if !%self.room.in_city%
  %echo% %self.name% melts into a puddle from the heat.
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
  %echo% %self.name% melts into a puddle from the heat.
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
  if %target.is_pc% || %target.pc_name.car% != horse
    %send% %actor% You're going to need a horse for this outfit.
    halt
  else
    set morphnum 16618
    if !%self.varexists(dress_up_counter)%
      set dress_up_counter 5
    else
      set dress_up_counter %self.dress_up_counter%
    end
  end
end
%send% %actor% You dress %target.name% with %self.shortdesc%.
%echoaround% %actor% %actor.name% dresses %target.name% with %self.shortdesc%.
%morph% %target% %morphnum%
eval dress_up_counter %dress_up_counter% - 1
if %dress_up_counter% > 0
  remote dress_up_counter %self.id%
else
  %quest% %actor% trigger %morphnum%
end
if %actor.quest_finished(%morphnum%)%
  %quest% %actor% finish %morphnum%
  %purge% %self%
end
~
#16619
Max the reindeer dog no-death~
0 f 100
~
return 0
%echo% %self.name% yelps and runs off before you can stop him!
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
    %echoaround% %actor% %actor.name% begins %actor.hisher% decorating by stringing strands of garland all about the area.
    %load% obj 16621 room
  break
  case 2
    %send% %actor% You decide it's time to place the candy cane hanging somewhere, and here looks like the perfect spot.
    %echoaround% %actor% As you watch, %actor.name% strings up a candy cane hanging as part of %actor.hisher% efforts to prepare for the winter holiday.
    %load% obj 16622 room
  break
  case 3
    %send% %actor% You carefully carry the faerie lantern to the center of the area and place it on the ground, then back away and admire the bright illumination it gives off.
    %echoaround% %actor% %actor.name% slowly walks to the center of the area with a faerie lantern in %actor.hisher% hands. As %actor.heshe% backs away, you can't help but admire the illumination it gives off.
    %load% obj 16623 room
  break
  case 4
    %send% %actor% You lift the reindeer hoofprint stamp and begin to deliver hammerblows to the ground, leaving behind realistic tracks, giving the impression there was a live reindeer here.
    %echoaround% %actor% %actor.name% takes out a reindeer hoofprint stamp and begins delivering hammerblows to the ground. The end result is some realistic tracks, giving the impression there was a live reindeer here.
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
grinchy greeting~
0 h 0
~
if %actor.is_pc% && !%self.fighting%
  dg_affect #16606 %self% off
end
~
#16622
winter pixy spawn~
0 n 100
~
switch %random.4%
  case 1
    %echo% You see %self.name% out of the corner of your eye.
  break
  case 2
    %echo% What looks like a flying snowball wizzes by.
  break
  case 3
    %echo% %self.name% dances in midair.
  break
  case 4
    %echo% Snow flies everywhere as %self.name% spins in circles.
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
  %send% %actor% What do you want to blast with %self.shortdesc%?
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
  %send% %actor% You point %self.shortdesc% at %target.name% and unleash a blast of icy magic!
  %echoaround% %actor% %actor.name% points %self.shortdesc% at %target.name% and unleashes a blast of icy magic!
  %echo% %target.name% is covered in a layer of ice and becomes motionless.
  return 1
else
  %send% %actor% You can only use %self.shortdesc% on the winter pixies.
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
  %send% %carried_by% %self.shortdesc% thaws out and suddenly vanishes in a poof of dust!
  %echoaround% %carried_by% %carried_by.name% is mysteriously engulfed in a cloud of dust!
elseif %pixy_rng% <= 80
  %send% %carried_by% %self.shortdesc% melts in your hands, but the cold seems to intensify until you're frozen yourself!
  %echoaround% %carried_by% %carried_by.name% starts turning blue, and eventually comes to a halt as though frozen in place!
  dg_affect #16625 %carried_by% HARD-STUNNED on 60
elseif %pixy_rng% <= 95
  %send% %carried_by% %self.shortdesc% warms and angrily covers you in dust, you feel drunk all of a sudden!
  %echoaround% %carried_by% %carried_by.name% sparkles for a moment and begins to stagger.
  nop %carried_by.drunk(30)%
else
  %send% %carried_by% %self.shortdesc% has apparently thawed out and is quite furious with you!
  %echoaround% %carried_by% A furious pixy begins to attack %carried_by.name% out of no where!
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
  %send% %actor% There's no Christmas tree here for you to place %self.shortdesc% on top of.
  halt
end
if %actor.obj_target(%arg%)% != %self%
  %send% %actor% Only %self.shortdesc% can be used to top a Christmas tree.
  halt
end
if !%christmas_tree.varexists(pixy_topped_off)%
  %mod% %christmas_tree% append-lookdesc-noformat On top %self.shortdesc% sits, frozen in place.
  %send% %actor% You place %self.shortdesc% on top of %christmas_tree.shortdesc%.
  %echoaround% %actor% %actor.name% places %self.shortdesc% on top of %christmas_tree.shortdesc%.
  set pixy_topped_off 1
  remote pixy_topped_off %christmas_tree.id%
else
  %send% %actor% You remove %christmas_tree.player_topped_tree%'s pixy from %christmas_tree.shortdesc% and throw it away, before placing your own.
  %echoaround% %actor% %actor.name% removes %christmas_tree.player_topped_tree%'s pixy from %christmas_tree.shortdesc% and throws it away, before placing %actor.hisher% own on top.
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
%send% %actor% You buff %buffing.shortdesc% with a cloth, extending the beauty.
%echoaround% %actor% %actor.name% buffs %buffing.shortdesc% with a cloth, extending the beauty.
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
  %send% %actor% Fine, but who or what did you want to throw %self.shortdesc% at?
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
  %send% %actor% You can only freeze the abominable snowman with %self.shortdesc%.
  halt
end
%send% %actor% You throw %self.shortdesc% at %AbominableSnowmanHere.name% with all your might...
%echoaround% %actor% %actor.name% throws %self.shortdesc% at %AbominableSnowmanHere.name% with all %actor.hisher% might...
%load% obj 16630 room
wait 1
%echo% Instantly %AbominableSnowmanHere.name% freezes solid!
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
  %send% %actor% You must enchant %self.shortdesc% with freezing to subdue the abominable snowman.
  halt
end
if !%actor.has_resources(1300,6)%
  %send% %actor% It will take six seashells to enchant %self.shortdesc% with freezing.
  halt
end
nop %actor.add_resources(1300, -6)%
%load% obj 16628 %actor% inv
%send% %actor% You enchant %self.shortdesc% with freezing.
%echoaround% %actor% %actor.name% enchants %self.shortdesc% with freezing.
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
    %echo% %AbominableSnowman.name% roars and begins to swing at %self.name%!
  break
  case 2
    %echo% Suddenly, a white blur lunges at %self.name% and you see %AbominableSnowman.name% hit it!
  break
  case 3
    %echo% %AbominableSnowman.name% comes out of nowhere and begins to deal blows to %self.name%!
  break
  case 4
    %echo% Snow flies everywhere as %AbominableSnowman.name% pops up and violently strikes %self.name%!
  break
done
%teleport% %self% %self.room%
%force% %AbominableSnowman% kill %self.pc_name%
wait 1
%send% %self.PlayerOnAbominableQuest% %self.name% tells you, 'The abominable snowman is here at %self.room.name%!'
~
#16631
snowman target will not escape~
0 s 100
~
if %actor.id% == %self.SnowmanUnderAttack%
  return 0
  %echo% %self.name% growls and drags %actor.name% back.
  mkill %actor%
end
~
#16632
abominable kills regular snowman~
0 z 100
~
if %actor.vnum% == 16600
  %send% %actor.PlayerOnAbominableQuest% %self.name% tells you, 'You obviously aren't a very good protector, %actor.name% is mush.'
  wait 1
  %echo% %self.name% runs off!
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
  %echo% %self.name% melts after such a long time of standing around.
  %purge% %self%
end
if %SinceLoaded% >= 3 || !%event.running(10700)%
  %echo% %self.name% melts after such a long time of standing around.
  %purge% %self%
end
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
  %send% %actor% You can only use %self.shortdesc% to enchant an ordinary red sleigh from the Winter Wonderland adventure.
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
  %echoaround% %actor% %actor.name% empties out %sleigh.shortdesc%...
  nop %sleigh.dump%
end
if %sleigh.animals_harnessed% > 1
  %send% %actor% You unharness the animals from %sleigh.shortdesc%...
  %echoaround% %actor% %actor.name% unharnesses the animals from %sleigh.shortdesc%...
  nop %sleigh.unharness%
elseif %sleigh.animals_harnessed% > 0
  %send% %actor% You unharness the animal from %sleigh.shortdesc%...
  %echoaround% %actor% %actor.name% unharnesses the animal from %sleigh.shortdesc%...
  nop %sleigh.unharness%
end
%load% veh 16650
set upgr %self.room.vehicles%
if %upgr.vnum% == 16650
  %own% %upgr% %sleigh.empire%
  %send% %actor% You polish %sleigh.shortdesc% with %self.shortdesc%...
  %echoaround% %actor% %actor.name% polishes %sleigh.shortdesc% with %self.shortdesc%...
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
  %send% %actor% You can only use %self.shortdesc% at a stable.
  halt
elseif (%actor.has_mount(10700)% && !%actor.has_mount(16652)%)
  %send% %actor% You use %self.shortdesc% and upgrade your flying reindeer mount to a red-nosed reindeer!
  nop %actor.remove_mount(10700)%
  nop %actor.add_mount(16652)%
elseif (%actor.has_mount(10705)% && !%actor.has_mount(10700)%)
  %send% %actor% You use %self.shortdesc% and upgrade your reindeer mount to a flying reindeer!
  nop %actor.remove_mount(10705)%
  nop %actor.add_mount(10700)%
elseif (%actor.has_mount(9175)% && !%actor.has_mount(10700)%)
  %send% %actor% You use %self.shortdesc% and upgrade your reindeer mount to a flying reindeer!
  nop %actor.remove_mount(9175)%
  nop %actor.add_mount(10700)%
elseif (%actor.has_mount(9176)% && !%actor.has_mount(10700)%)
  %send% %actor% You use %self.shortdesc% and upgrade your barded reindeer mount to a flying reindeer!
  nop %actor.remove_mount(9176)%
  nop %actor.add_mount(10700)%
elseif %actor.has_mount(16652)%
  %send% %actor% You already have a red-nosed reindeer mount. You can't do anything with %self.shortdesc%.
  halt
else
  %send% %actor% You don't have a reindeer mount you can use the polish on. Try looking around the tundra.
  halt
end
* if we get here, we used it successfully
%echoaround% %actor% %actor.name% uses %self.shortdesc%!
%purge% %self%
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
  * %send% %actor% You don't seem to have a '%arg%'. (You can only use %self.shortdesc% on items in your inventory.)
  return 0
  halt
end
* All other cases return 1
return 1
if %target.vnum% != 16653 && %target.vnum% != 16654 && %target.vnum% != 10711 && %target.vnum% != 10712
  %send% %actor% You can only use %self.shortdesc% on the gift sack, sweater, and hat from Winter Wonderland.
  halt
end
if %target.is_flagged(SUPERIOR)% || %target.vnum% == 16654
  %send% %actor% %target.shortdesc% is already upgraded; using %self.shortdesc% would have no benefit.
  halt
end
%send% %actor% You sprinkle %self.shortdesc% onto %target.shortdesc%...
%echoaround% %actor% %actor.name% sprinkles %self.shortdesc% onto %target.shortdesc%...
%echo% %target.shortdesc% begins to shimmer and glow!
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
%send% %pers1% You notice you're under the mistletoe with %pers2.name%...
%send% %pers2% You notice you're under the mistletoe with %pers1.name%...
%send% %pers1% You lean over and kiss %pers2.name% on the cheek!
%send% %pers2% %pers1.name% leans over and kisses you on the cheek!
%echoneither% %pers1% %pers2% %pers1.name% leans over and kisses %pers2.name% on the cheek!
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
%send% %actor% You spin %self.shortdesc%...
%echoaround% %actor% %actor.name% spins %self.shortdesc%...
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
elseif %roll% <= 105
  * 6% chance: minipet whistle
  set vnum 10727
elseif %roll% <= 355
  * 25% chance: some chocolate coins (+stats)
  set vnum 16660
elseif %roll% <= 605
  * 25% chance: some snowball cookies (+inventory)
  set vnum 16661
elseif %roll% <= 855
  * 25% chance: some thumbprint cookies (+move regen)
  set vnum 16662
else
  * Remaining %: small resource shipment
  set vnum 11513
end
%send% %actor% You open %self.shortdesc%...
%echoaround% %actor% %actor.name% opens %self.shortdesc%...
%load% obj %vnum% %actor%
set gift %actor.inventory%
if %gift.vnum% == %vnum%
  %echo% It contained %gift.shortdesc%!
else
  %echo% It's empty!
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
%send% %actor% You have modified %target.name%'s event points by %arg.cdr%.
%send% %actor% %target.heshe% now has %val% points.
~
$
