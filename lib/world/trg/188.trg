#18800
Summon ghost with candy~
1 c 2
sacrifice~
* This is no longer used as of Oct 2020
return 0
halt
* discard arguments after the first
set arg %arg.car%
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %self.room%
set person %room.people%
while %person%
  if %person.vnum% == 18800
    %send% %actor% %person.name% is already here.
    halt
  end
  set person %person.next_in_room%
done
if %room.function(TOMB)% && !%room.bld_flagged(OPEN)% && %room.complete%
  * OK
else
  return 0
  halt
end
%send% %actor% You toss %self.shortdesc% into an open grave...
%echoaround% %actor% %actor.name% tosses %self.shortdesc% into an open grave...
%load% mob 18800
set mob %room.people%
if %mob.vnum% != 18800
  %echo% Something went wrong.
  halt
end
%echo% %mob.name% rises from the grave and devours %self.shortdesc%!
%purge% %self%
~
#18801
Summon Headless Centaur~
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
set found_grave 0
set room %self.room%
if !%room.function(TOMB)% || !%room.bld_flagged(OPEN)% || !%room.complete%
  %send% %actor% You can only do that at an outdoor tomb building.
  halt
end
set item %room.contents%
while %item%
  if %item.vnum% == 18800
    set found_grave 1
  end
  set item %item.next_in_list%
done
if %found_grave%
  %send% %actor% You cannot summon another Headless Horse Man here yet.
  halt
end
set arg2 %arg.cdr%
if !%arg2%
  %send% %actor% What difficulty would you like to summon the Headless Horse Man at? (Normal, Hard, Group or Boss)
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
      %send% %actor% You light %self.shortdesc% and hold it aloft!
      %echoaround% %actor% %actor.name% lights %self.shortdesc% and holds it aloft!
    break
    case 2
      %echo% A thick, clammy fog begins to blow in from all directions...
    break
    case 1
      %echo% You hear the muffled sound of hoofbeats...
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
%load% mob 18801
%load% obj 18800 room
set mob %room.people%
if %mob.vnum% != 18801
  %echo% Something went wrong...
  halt
end
%echo% %mob.name% bursts out of the fog in front of you!
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
%echo% %self.shortdesc% bursts into blue flames and rapidly crumbles to ash.
%purge% %self%
~
#18802
Offer Jammy Dodger~
0 j 100
~
if %object.vnum% == 18802
  return 0
  * Jammy dodger
  %send% %actor% You offer %self.name% a jammy dodger.
  %echoaround% %actor% %actor.name% offers %self.name% a jammy dodger.
  %purge% %object%
  wait 2
  say Oh! Thank you.
  wait 5
  %load% obj 18826 %actor% inventory
  set item %actor.inventory(18826)%
  %send% %actor% %self.name% gives you %item.shortdesc%!
  %echoaround% %actor% %self.name% gives %actor.name% %item.shortdesc%!
else
  %send% %actor% %self.name% politely declines your gift.
  return 0
end
~
#18803
Headless Centaur: Prance~
0 k 33
~
if %self.cooldown(18801)%
  halt
end
nop %self.set_cooldown(18801, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
%echo% %self.name% rears up and prances!
wait 2 sec
if %heroic_mode%
  %echo% &r%self.name% slams %self.hisher% hooves down on the ground, creating a shockwave!
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      dg_affect #18803 %person% HARD-STUNNED on 5
      %damage% %person% 50
    end
    set person %person.next_in_room%
  done
else
  %send% %actor% &r%self.name%'s hooves crash down on you!
  %echoaround% %actor% %self.name%'s hooves crash down on %actor.name%!
  %damage% %actor% 100
end
~
#18804
Headless Centaur: Neck Chop~
0 k 50
~
if %self.cooldown(18801)%
  halt
end
nop %self.set_cooldown(18801, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
if %heroic_mode%
  %echo% &r%self.name% swings %self.hisher% sword in a wide arc at neck level, causing bleeding wounds!
  %aoe% 100 physical
  set person %self.room.people%
  while %person%
    if %person.is_enemy(%self%)%
      %dot% #18804 %person% 100 30 physical
      %damage% %person% 50 physical
    end
    set person %person.next_in_room%
  done
else
  %send% %actor% &r%self.name% swings %self.hisher% sword, slashing at your neck and opening a bleeding wound!
  %echoaround% %actor% %self.name% swings %self.hisher% sword, slashing at %actor.name%'s neck and opening a bleeding wound!
  %damage% %actor% 150 physical
  %dot% #18804 %actor% 75 30 physical
end
~
#18805
Headless Centaur: Attack-O-Lantern~
0 k 100
~
if %self.cooldown(18801)%
  halt
end
if !%self.mob_flagged(HARD)% && !%self.mob_flagged(GROUP)%
  halt
end
nop %self.set_cooldown(18801, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
set room %self.room%
set person %room.people%
while %person%
  if %person.vnum% == 18805
    set lantern %person%
  end
  set person %person.next_in_room%
done
if %lantern%
  %echo% %self.name% urges %lantern.name% to attack faster!
  dg_affect %lantern% HASTE on 30
else
  %load% mob 18805 ally
  set summon %room.people%
  if %summon.vnum% == 18805
    %echo% %self.name% thrusts %self.hisher% sword into the sky!
    %echo% %summon.name% appears in a flash of blue fire!
    %force% %summon% %aggro% %actor%
  end
end
~
#18806
Headless Centaur death~
0 f 100
~
set person %self.room.people%
set loot 0
while %person%
  if %person.is_pc%
    if %person.on_quest(18801)%
      %quest% %person% finish 18801
      set loot 1
    end
    if %person.empire%
      nop %person.empire.start_progress(18800)%
    end
  elseif %person.vnum% == 18805
    %purge% %person% $n vanishes in a flash of blue fire!
  end
  set person %person.next_in_room%
done
if !%loot%
  nop %self.add_mob_flag(!LOOT)%
end
~
#18807
attack-o-lantern aoe~
0 k 100
~
set person %self.room.people%
while %person%
  if %person.vnum% == 18801
    set heroic_mode %person.mob_flagged(GROUP)%
  end
  set person %person.next_in_room%
done
%echo% &rBeams of magical energy fly from %self.name%'s eyes!
if !%heroic_mode%
  %aoe% 25 magical
else
  %aoe% 50 magical
end
~
#18818
Learn Halloween Costumes~
1 c 2
learn~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if %actor.learned(18812)%
  %send% %actor% You already know those recipes.
  halt
end
%send% %actor% You learn how to make Halloween costumes.
set craft_vnum 18812
while %craft_vnum% <= 18817
  nop %actor.add_learned(%craft_vnum%)%
  eval craft_vnum %craft_vnum% + 1
done
%purge% %self%
~
#18819
apply costume to citizens~
1 c 2
costume~
if %self.val0% == 0
  %send% %actor% You are out of costumes.
  halt
end
if %actor.cooldown(18819)% > 0
  %send% %actor% You need to wait %actor.cooldown(18819)% more seconds to do that again.
  halt
end
if !%arg%
  %send% %actor% Whom?
  eval costumes_left 18818-%self.val0%
  %send% %actor% You have %costumes_left% costumes left.
  halt
end
if !%actor.canuseroom_guest%
  %send% %actor% You don't have permission to do that here.
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% They're not here.
  halt
end
if %target.is_pc%
  %send% %actor% You can't costume players with %self.shortdesc%.
  halt
elseif !%target.mob_flagged(HUMAN)%
  %send% %actor% You can't put a costume on %target.name%.
  halt
elseif %target.mob_flagged(AGGR)% || %target.mob_flagged(CITYGUARD)%
  %send% %actor% You don't think %target.name% would be very pleased if you did that.
  halt
elseif !%target.mob_flagged(EMPIRE)%
  %send% %actor% %target.name% declines.
  halt
elseif %target.morph% >= 18812 && %target.morph% <= 18817
  %send% %actor% %target.heshe%'s already wearing a costume!
  halt
elseif %target.morph%
  %send% %actor% You can't put a costume on %target.name%.
  halt
else
  set prev_name %target.name%
  set costume_vnum %self.val0%
  %morph% %target% %costume_vnum%
  %send% %actor% You dress up %prev_name% as %target.name%!
  %echoaround% %actor% %actor.name% dresses up %prev_name% as %target.name%!
  eval costume_vnum %costume_vnum% + 1
  if %costume_vnum% > 18817
    %quest% %actor% trigger 18819
    nop %self.val0(0)%
    %quest% %actor% finish 18819
  else
    nop %self.val0(%costume_vnum%)%
    nop %actor.set_cooldown(18819,30)%
  end
end
~
#18820
Halloween event quest items~
2 u 0
~
switch %questvnum%
  case 18819
    * trunk
    %load% obj 18820 %actor% inv
  break
  case 18821
    * wand
    %load% obj 18821 %actor% inv
  break
  case 18823
    * ritual notes
    %load% obj 18823 %actor% inv
  break
  case 18824
    * bog roll
    %load% obj 18824 %actor% inv
  break
  case 18827
    * dracula morpher
    %load% obj 18827 %actor% inv
  break
  case 18801
    * turnip lantern
    %load% obj 18801 %actor% inv
  break
  case 18828
    * haunted house recipe
    %load% obj 18828 %actor% inv
  break
  case 18829
    %load% obj 18850 %actor% inv
  break
  case 18830
    %load% obj 18851 %actor% inv
  break
  case 18831
    %load% obj 18852 %actor% inv
  break
  case 18832
    %load% obj 18853 %actor% inv
  break
  case 18854
    %load% obj 18854 %actor% inv
  break
  case 18856
    %load% obj 18856 %actor% inv
  break
  case 18857
    %load% obj 18857 %actor% inv
    set owner %actor%
    remote owner %actor.inventory().id%
  break
  case 18869
    %load% obj 18869 %actor% inv
  break
  case 18870
    %load% obj 18870 %actor% inv
  break
  case 18873
    %load% obj 18873 %actor% inv
  break
  case 18880
    %load% obj 18880 %actor% inv
  break
done
~
#18821
toad citizen~
1 c 2
polymorph~
if %self.val0% == 0
  %send% %actor% %self.shortdesc% is out of charges.
  halt
end
if %actor.cooldown(18821)% > 0
  %send% %actor% You need to wait %actor.cooldown(18821)% more seconds to do that again.
  halt
end
if !%arg%
  %send% %actor% Whom?
  halt
end
if !%actor.canuseroom_guest%
  %send% %actor% You don't have permission to do that here.
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% They're not here.
  halt
end
if %target.is_pc%
  %send% %actor% That seems kind of mean, don't you think?
  halt
elseif %target.vnum% != 202 && %target.vnum% != 203
  %send% %actor% You can only use %self.shortdesc% on generic empire citizens.
  halt
elseif %target.morph%
  %send% %actor% You can't use %self.shortdesc% on people who are already disguised or transformed.
  halt
else
  set prev_name %target.name%
  set costume_vnum 18821
  %morph% %target% %costume_vnum%
  %send% %actor% You wave %self.shortdesc% at %prev_name%, who turns into %target.name%!
  %echoaround% %actor% %actor.name% waves %self.shortdesc% at %prev_name%, who turns into %target.name%!
  set charges %self.val0%
  eval charges %charges% - 1
  if %charges% == 0
    %quest% %actor% trigger 18821
    %quest% %actor% finish 18821
  else
    nop %self.val0(%charges%)%
    nop %actor.set_cooldown(18821,30)%
  end
end
~
#18822
spawn ghosts~
1 b 25
~
set room %self.room%
* room population check
set person %room.people%
set ghosts 0
set people 0
while %person%
  eval people %people% + 1
  if %person.vnum% == 18822
    eval ghosts %ghosts% + 1
  end
  set person %person.next_in_room%
done
if %people% > 5 || %ghosts% > 0
  halt
end
%load% mob 18822
set mob %room.people%
if %mob.vnum% == 18822
  %echo% %mob.name% emerges from %self.shortdesc%!
end
~
#18823
Ritual of Spirits~
1 c 2
ritual rite~
if !(spirits /= %arg%)
  return 0
  halt
end
set room %actor.room%
set cycles_left 5
while %cycles_left% >= 0
  eval sector_valid (%room.building_vnum% == 5009)
  if (%actor.room% != %room%) || !%sector_valid% || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 5
      %echoaround% %actor% %actor.name%'s ritual is interrupted.
      %send% %actor% Your ritual is interrupted.
    elseif !%sector_valid%
      %send% %actor% You must perform the ritual at a city center.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% %actor.name% pulls out some chalk and begins the rite of spirits...
      %send% %actor% You pull out some chalk and begin the rite of spirits...
    break
    case 4
      %echoaround% %actor% %actor.name% draws mystic symbols in a circle on the ground...
      %send% %actor% You draw mystic symbols in a circle on the ground...
    break
    case 2
      %echoaround% %actor% %actor.name%'s eyes go white as %actor.heshe% channels the spirits of the departed...
      %send% %actor% You fall into a deep trance as you channel the spirits of the departed...
    break
    case 1
      %echoaround% %actor% %actor.name% whispers into the void...
      %send% %actor% You whisper powerful words into the void, awakening the souls beyond...
    break
    case 0
      %echoaround% %actor% %actor.name% completes %actor.hisher% ritual as spirits begin to fill the air!
      %send% %actor% You finish your ritual as spirits begin to fill the air!
      %load% obj 18822 room
      %quest% %actor% trigger 18823
      %send% %actor% %self.shortdesc% bursts into flames!
      %echoaround% %actor% %self.shortdesc% bursts into flames!
      %quest% %actor% finish 18823
      * Leave the loop
    break
  done
  if %cycles_left% > 0
    wait 5 sec
  end
  eval cycles_left %cycles_left% - 1
done
%purge% %self%
~
#18824
toiletpaper houses~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if %self.val0% < 1
  %send% %actor% %self.shortdesc% has run out.
  halt
end
if %actor.cooldown(18824)% > 0
  %send% %actor% You need to wait %actor.cooldown(18824)% more seconds to do that again.
  halt
end
set room %self.room%
set item %room.contents%
while %item%
  if %item.vnum% == 18825
    %send% %actor% Someone has beaten you to this house.
    halt
  end
  set item %item.next_in_list%
done
set vnum %room.building_vnum%
if !%vnum%
  %send% %actor% You can only use %self.shortdesc% inside a house.
  halt
end
if %room.max_citizens% < 1
  %send% %actor% You can only use %self.shortdesc% inside a house.
  halt
end
%send% %actor% You start applying %self.shortdesc% to the walls of the building...
%echoaround% %actor% %actor.name% starts applying %self.shortdesc% to the walls of the building...
wait 1 sec
* Skill check
set chance %random.100%
if %chance% > %actor.skill(Stealth)%
  * Fail
  %send% %actor% You hear someone approaching!
  wait 1 sec
  %at% %room% %echo% A guard arrives!
  %at% %room% %load% mob 18824 %actor.level%
  set guard %room.people%
  if %guard.vnum% == 18824
    %force% %guard% mhunt %actor%
  end
else
  %send% %actor% You finish bogrolling the building.
  %echoaround% %actor% %actor.name% finishes bogrolling the building.
  %send% %actor% Your Stealth skill ensures nobody notices your mischief.
  %load% obj 18825 room
  set charges %self.val0%
  if %charges% == 1
    %send% %actor% %self.shortdesc% runs out!
    %quest% %actor% trigger 18824
    %quest% %actor% finish 18824
  else
    eval charges %charges% - 1
    nop %self.val0(%charges%)%
    nop %actor.set_cooldown(18824,30)%
  end
end
~
#18827
Scare citizens as Dracula~
1 c 2
scare~
if %actor.morph% != 18827
  * Only in Dracula morph
  return 0
  halt
end
if %actor.cooldown(18827)% > 0
  %send% %actor% You need to wait %actor.cooldown(18827)% more seconds to do that again.
  halt
end
if !%arg%
  %send% %actor% Who would you like to scare?
  return 1
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% It must have worked, because they're not here!
  return 1
  halt
end
if %target.is_pc%
  %send% %actor% You stalk toward %target.name%, baring your fangs as %target.heshe% cowers in fear!
  %send% %target% %actor.name% stalks towards you, fangs bared in a terrifying snarl!
  %echoneither% %actor% %target% %actor.name% bares %actor.hisher% fangs and stalks towards %target.name%, who cowers in fear!
  halt
elseif %target.vnum% != 202 && %target.vnum% != 203
  * wrong target
  %send% %actor% You bare your fangs and snarl at %target.name%, who flinches away.
  %echoaround% %actor% %actor.name% bares %actor.hisher% fangs and snarls at %target.name%, who flinches away.
else
  %send% %actor% Bats swirl around you as you spread your flowing cape dramatically, bare your fangs, and snarl at %target.name%!
  %echoaround% %actor% Bats swirl around %actor.name% as %actor.heshe% spreads %actor.hisher% flowing cape dramatically, bares %actor.hisher% fangs, and snarls at %target.name%!
  %echo% %target.name% panics, and attempts to flee!
  %force% %target% mmove
  %force% %target% mmove
  %force% %target% mmove
  eval times %self.val0% + 1
  if %times% == 5
    %send% %actor% You have scared enough citizens... but you can keep going if you want to.
    %quest% %actor% trigger 18827
    %quest% %actor% finish 18827
  else
    nop %self.val0(%times%)%
    nop %actor.set_cooldown(18827,30)%
  end
end
~
#18828
Unearthly Manor Interior~
2 o 100
~
eval greatrm %%room.%room.enter_dir%(room)%%
* Add great room
if !%greatrm%
  %door% %room% %room.enter_dir% add 18829
end
eval greatrm %%room.%room.enter_dir%(room)%%
if !%greatrm%
  * Failed to add
  halt
end
* Add cellar
if !%greatrm.down(room)%
  %door% %greatrm% down add 18830
end
detach 18828 %self.id%
~
#18829
Halloween: Open goody bag~
1 c 2
open~
* clear actor's event currency-- this is no longer used
set tokens %actor.currency(18800)%
if %tokens% > 0
  eval doit %%actor.give_currency(18800, -%tokens%)%%
end
* validate arg
if !%arg% || %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
if !%event.running(18800)%
  %echo% It's empty! You must have missed Halloween.
  return 1
  %purge% %self%
  halt
end
set roll %random.1000%
if %roll% <= 5
  * 0.5% chance: a strange crystal seed
  set vnum 600
elseif %roll% <= 11
  * 0.6% chance: ghoul whistle
  set vnum 18841
elseif %roll% <= 17
  * 0.6% chance: haunted mask
  set vnum 18847
elseif %roll% <= 26
  * 0.9% chance: mausoleum cornerstone
  set vnum 18844
elseif %roll% <= 35
  * 0.9% chance: nightmare whistle
  set vnum 18840
elseif %roll% <= 45
  * 1% chance: candy seeds
  set vnum 18846
elseif %roll% <= 58
  * 1.3% chance: headless horse whistle
  set vnum 18839
elseif %roll% <= 108
  * 5% chance: giant tarantula whistle
  set vnum 18879
elseif %roll% <= 208
  * 10% chance: zombie terrier whistle
  set vnum 18885
elseif %roll% <= 370
  * 16.2% chance: roll of pennies!
  set vnum 18886
elseif %roll% <= 630
  * 26% chance: marigold
  set vnum 18887
elseif %roll% <= 890
  * 26% chance: chrysanthemums
  set vnum 18888
else
  * Remaining 11%: black rose
  set vnum 18889
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
#18838
Halloween: Ectoplasm upgrades items~
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
if %target.vnum% != 18836 && %target.vnum% != 18883 && %target.vnum% != 18884 && %target.vnum% != 18847
  %send% %actor% You can only use %self.shortdesc% on the plague doctor mask, oversized candy bag, ghastly shackles, or haunted mask.
  halt
end
if %target.is_flagged(SUPERIOR)%
  %send% %actor% %target.shortdesc% is already upgraded; using %self.shortdesc% would have no benefit.
  halt
end
%send% %actor% You carefully pour %self.shortdesc% onto %target.shortdesc%...
%echoaround% %actor% %actor.name% pours %self.shortdesc% onto %target.shortdesc%...
%echo% %target.shortdesc% takes on a spooky glow!
nop %target.flag(SUPERIOR)%
if %target.level% > 0
  %scale% %target% %target.level%
else
  %scale% %target% 1
end
%purge% %self%
~
#18848
make offering to the spirits~
1 c 2
offer~
* Value0 tracks sacrifices remaining
switch %self.vnum%
  case 18850
    set component_base 6720
    set sacrifice_amount 10
    set display_str 10x common metal
    set qvnum 18829
  break
  case 18851
    set component_base 6075
    set sacrifice_amount 10
    set display_str 10x block
    set qvnum 18830
  break
  case 18852
    set component_base 6050
    set sacrifice_amount 10
    set display_str 10x rock
    set qvnum 18831
  break
  case 18853
    set component_base 6420
    set sacrifice_amount 10
    set display_str 10x plant fibers
    set qvnum 18832
  break
done
set found_grave 0
set room %self.room%
if !%room.function(TOMB)% || !%room.complete%
  %send% %actor% You can only do that at a tomb building.
  halt
end
set item %room.contents%
while %item%
  if %item.vnum% == 18849
    set found_grave 1
  end
  set item %item.next_in_list%
done
if %found_grave%
  %send% %actor% Someone has already appeased the spirits at this tomb.
  halt
end
set sacrifices_left %self.val0%
if %sacrifices_left% < 1
  %send% %actor% You have already appeased the spirits of the dead.
  halt
end
* actual sacrifice
if !%actor.has_component(%component_base%, %sacrifice_amount%)%
  %send% %actor% You don't have the (%display_str%) required for this sacrifice...
  halt
end
%send% %actor% You offer up (%display_str%) to appease the spirits of the dead...
%echoaround% %actor% %actor.name% offers up (%display_str%) to appease the spirits of the dead...
nop %actor.charge_component(%component_base%, %sacrifice_amount%)%
eval sacrifices_left %sacrifices_left% - 1
nop %self.val0(%sacrifices_left%)%
%load% obj 18849 room
if %sacrifices_left% == 0
  %quest% %actor% trigger %qvnum%
  %quest% %actor% finish %qvnum%
end
~
#18854
pick or treat action~
1 c 2
pickpocket~
if !%actor.on_quest(18854)%
  return 0
  halt
end
if !%actor.ability(pickpocket)%
  return 0
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  return 0
  halt
end
if %target.empire% != %actor.empire%
  return 0
  halt
end
if %target.mob_flagged(*PICKPOCKETED)%
  return 0
  halt
end
if !%target.mob_flagged(human)%
  return 0
  halt
end
if %actor.cooldown(18854)%
  %send% %actor% You can't do that right now, wait until the heat is off of you.
  eval new_cdt %actor.cooldown(18854)% + 5
  nop %actor.set_cooldown(18854, %new_cdt%)%
  return 1
  halt
end
set person %self.room.people%
while %person%
  if %person.mob_flagged(cityguard)%
    %force% %person% shout Stop %actor.name%! You aren't going to steal candy on my watch!
    nop %actor.set_cooldown(18854, 10)%
    return 1
    halt
  end
  set person %person.next_in_room%
done
eval stealth_against %actor.skill(stealth)% + 10
eval stealth_roll %%random.%stealth_against%%%
if %stealth_roll% > 10
  %send% %actor% You pick %target.name%'s pocket...
  nop %target.add_mob_flag(*PICKPOCKETED)%
  %load% obj 18855 %actor% inv
  set item %actor.inventory()%
  %send% %actor% You find %item.shortdesc%!
  return 1
else
  return 0
end
if %actor.quest_finished(18854)%
  %quest% %actor% finish 18854
end
~
#18856
look in magic mirror~
1 c 2
look examine~
if !%actor.on_quest(18856)%
  %send% %actor% %self.shortdesc%? What about %self.shortdesc%?
  %purge% %self%
  halt
end
if !(%actor.obj_target(%arg%)% == %self%)
  return 0
  halt
end
if %actor.disabled%
  return 0
  halt
end
if !%actor.eq(clothes)%
  %send% %actor% You are not wearing any clothing. Maybe you should get dressed?
  return 1
  halt
end
set clothing %actor.eq(clothes)%
%send% %actor% You see yourself, %actor.name%, wearing %clothing.shortdesc%.
%echoaround% %actor% You see %actor.name% look into %self.shortdesc%.
set vnum %clothing.vnum%
set clothing_count 1
if !%self.varexists(list_clothing)%
  set list_clothing %vnum%
  remote list_clothing %self.id%
  set update 1
else
  set list_clothing %self.list_clothing%
  set temp_clothing %list_clothing%
  while %temp_clothing%
    if %vnum% == %temp_clothing.car%
      set update 1
    else
      eval clothing_count %clothing_count% + 1
    end
    set temp_clothing %temp_clothing.cdr%
  done
end
if %update% != 1
  set list_clothing %list_clothing% %vnum%
  remote list_clothing %self.id%
end
if %clothing_count% == 1
  set shadows figure wearing
else
  set shadows figures. The first wearing
end
switch %clothing_count%
  case 1
    set clothing_count a single
    set clothing1 %clothing.shortdesc%
    set build_up %clothing1%
    if !%self.varexists(clothing1)%
      remote clothing1 %self.id%
    end
    set left You've got one shadowy reflection in there, now just to get four more, each in a unique outfit of its own!
  break
  case 2
    set clothing_count two
    if %update% != 1
      set clothing2 %clothing.shortdesc%
      remote clothing2 %self.id%
    end
    set left You still need three more unique outfits for your shadowy reflection to wear.
    set build_up %self.clothing1% and the new one %self.clothing2%.
  break
  case 3
    set clothing_count three
    if %update% != 1
      set clothing3 %clothing.shortdesc%
      remote clothing3 %self.id%
    end
    set left You still need two more unique outfits for your shadowy reflection to wear.
    set build_up %self.clothing1%, the second %self.clothing2%, and the latest %self.clothing3%.
  break
  case 4
    set clothing_count four
    if %update% != 1
      set clothing4 %clothing.shortdesc%
      remote clothing4 %self.id%
    end
    set left You still need one more unique outfit for your shadowy reflection to wear.
    set build_up %self.clothing1%, the second %self.clothing2%, the third %self.clothing3%, and the latest %self.clothing4%.
  break
  case 5
    set clothing_count five
    set clothing5 %clothing.shortdesc%
    remote clothing5 %self.id%
    %quest% %actor% trigger 18856
    set build_up %self.clothing1%, the second %self.clothing2%, the third %self.clothing3%, the fourth %self.clothing4%, and the latest %self.clothing5%.
  break
done
%send% %actor% In the background of %self.shortdesc% you see %clothing_count% shadowy %shadows% %build_up%
if !%actor.quest_finished(18856)%
  %send% %actor% %left%
else
  %quest% %actor% finish 18856
end
~
#18857
apple bobbing challenge~
1 c 4
challenge accept~
if %cmd% == challenge
  set owner %self.owner%
  if %actor% != %owner%
    %send% %actor% If you wanted to challenge someone, perhaps you should get your own %self.shortdesc%?
    return 1
    halt
  end
  if !%actor.on_quest(18857)%
    %send% %actor% You hear a ghostly voice whisper, 'You should not have this any longer.'
    %echoaround% %actor% You hear a ghostly voice whispering, but can't make out the words.
    %echo% The %self.shortdesc% vanishes into a puff of smoke!
    return 1
    %purge% %self%
    halt
  end
  if !%arg%
    %send% %actor% who did you want to challenge?
    return 1
    halt
  else
    set target %actor.char_target(%arg%)%
  end
  if !(%actor.can_see(%target%)%)
    %send% %actor% You don't see anyone like that here to challenge.
    return 1
    halt
  end
  if %owner% == %target%
    %send% %actor% Sort of silly to challenge yourself don't you think?
    return 1
    halt
  end
  if %target.is_npc%
    %send% %actor% You can only challenge other players.
    return 1
    halt
  end
  set challenged %target%
  remote challenged %self.id%
  %send% %target% %actor.pc_name% is challenging you to bob for apples. Whoever gets the largest apple wins!
  %send% %target% Type 'accept %owner.pc_name%' to accept.
  %send% %actor% You challenge %target.pc_name% to bob for apples. Whoever gets the largest apple wins!
  %echoneither% %actor% %target% You see %actor.pc_name% challenge %target.pc_name% to an apple bobbing contest!
end
if %cmd% == accept
  set owner %self.owner%
  set challenged %self.challenged%
  if !%arg%
    %send% %actor% If you meant to accept the apple bobbing challenge, then type 'accept %owner.pc_name%'.
    return 0
    halt
  end
  if %arg% != %owner.pc_name%
    return 0
    halt
  end
  if %actor% != %challenged%
    %send% %actor% You weren't the one being challenged.
    return 1
    halt
  end
  if !%owner.on_quest(18857)%
    %send% %actor% You here spirits whisper, '%owner.pc_name% should no longer have this.'
    %echo% The %self.shortdesc% vanishes in a puff of smoke!
    return 1
    halt
  end
  if !(%actor.can_see(%owner%)%)
    %send% %actor% You don't see %owner.pc_name% around here any longer.
    return 1
    halt
  end
  set game_on 1
  remote game_on %self.id%
  %send% %challenged% You are up first. Type 'bob bucket' to begin, and 'stand' to complete your turn.
  %send% %owner% %challenged.pc_name% has accepted your challenge, %challenged.heshe% is up first.
  set turn %challenged%
  remote turn %self.id%
end
~
#18858
apple bobbing bob~
1 c 4
bob~
if !(%actor.obj_target(%arg%)% == %self%)
  %send% %actor% You can only bob in the %self.shortdesc%.
  return 1
  halt
end
if !%self.varexists(turn)%
  %send% %actor% A challenge must be offered and accepted before anyone can bob for apples from this %self.shortdesc%.
  return 1
  halt
end
if %actor% != %self.turn%
  %send% %actor% It isn't your turn right now.
  return 1
  halt
end
if %self.varexists(same_round)%
  %send% %actor% Let the water calm a bit first.
  return 1
  halt
end
%send% %actor% You dip your head into the water of the bucket and start looking for an apple.
%echoaround% %actor% %actor.pc_name% sticks %actor.hisher% head into the bucket and starts looking for an apple.
set start_bob %timestamp%
remote start_bob %self.id%
set timer_running 1
remote timer_running %self.id%
set same_round 1
remote same_round %self.id%
wait 10 s
if %self.varexists(timer_running)%
  %send% %actor% You can't hold your breath much longer! You need to 'stand' soon!
end
wait 4 s
if %self.varexists(timer_running)%
  set turn %self.turn%
  %send% %turn% You can't hold your breath any longer and pull your head out of the water.
  %echoaround% %turn% %turn.pc_name% suddenly pulls %turn.hisher% head out of the water gasping for air!
  rdelete timer_running %self.id%
  %send% %turn% Try again? And maybe, make sure you stand up before you can no longer hold your breath.
end
rdelete same_round %self.id%
~
#18859
apple bobbing bucket was left behind~
1 b 100
~
if %self.carried_by%
  set actor %self.carried_by%
  if !%actor.on_quest(18857)%
    %send% %actor% The %self.shortdesc% vanishes from your arms in a poof of smoke!
    %purge% %self%
    halt
  end
end
set person %self.room.people%
set owner %self.owner%
while %person%
  if %person% == %owner%
    halt
  end
  set person %person.next_in_room%
done
%echo% The %self.shortdesc% vanishes in a poof of smoke!
%purge% %self%
~
#18860
bobbing please stand up~
1 c 4
bob stand~
if %cmd% == bob
  if !(%actor.obj_target(%arg%)% == %self%)
    %send% %actor% You can only bob for apples in the %self.shortdesc%.
    return 1
    halt
  end
  if %actor% != %self.turn%
    %send% %actor% It isn't your turn right now.
    return 1
    halt
  end
  if %self.varexists(same_round)%
    %send% %actor% Let the water calm a bit first.
    return 1
    halt
  end
end
if %cmd% == stand
  if %actor% != %turn%
    return 0
    halt
  end
  if !%self.varexists(timer_running)%
    return 0
    halt
  end
  return 1
  rdelete timer_running %self.id%
  eval time %timestamp% - %self.start_bob%
  if %time% == 0
    %echoaround% %actor% %actor.pc_name% stands back up immediately with no apple in %actor.hisher% mouth.
    return 1
    halt
  end
  switch %time%
    case 1
      set apple_val 1
      set apple_size bitty
    break
    case 2
      set apple_val 2
      set apple_size tiny
    break
    case 3
      set apple_val 3
      set apple_size small
    break
    case 4
      set apple_val 4
      set apple_size medium
    break
    case 5
      set apple_val 4
      set apple_size medium
    break
    case 6
      set apple_val 5
      set apple_size large
    break
    case 7
      set apple_val 6
      set apple_size huge
    break
    case 8
      set apple_val 5
      set apple_size large
    break
    case 9
      set apple_val 4
      set apple_size medium
    break
    case 10
      set apple_val 4
      set apple_size medium
    break
    case 11
      set apple_val 3
      set apple_size small
    break
    case 12
      set apple_val 2
      set apple_size tiny
    break
    case 13
      set apple_val 1
      set apple_size bitty
    break
  done
  %send% %actor% You managed to get a %apple_size% apple from the bucket!
  %echoaround% %actor% %actor.pc_name% straightens up with a %apple_size% apple in %actor.hisher% mouth!
  if %actor% == %self.challenged%
    set ch_apple %apple_val%
    remote ch_apple %self.id%
    set turn %self.owner%
    remote turn %self.id%
  elseif %actor% == %self.owner%
    set ow_apple %apple_val%
    set ch_apple %self.ch_apple%
    if %ow_apple% > %ch_apple%
      %send% %actor% You win!
      %echoaround% %actor% %actor.pc_name% wins!
      %quest% %actor% finish 18857
      %purge% %self%
    elseif %ow_apple% < %ch_apple%
      %send% %actor% You've lost this time.
      %echoaround% %actor% %self.challenged.pc_name% wins!
    else
      %echo% It's a tie!
    end
    rdelete turn %self.id%
    rdelete ch_apple %self.id%
    rdelete challenged %self.id%
  end
end
~
#18869
play them off johny~
1 c 2
play~
return 0
set music_score 0
remote music_score %self.id%
if !%self.has_trigger(18870)%
  attach 18870 %self.id%
end
~
#18870
are they still playing~
1 b 100
~
set questid %self.vnum%
set actor %self.carried_by%
if %actor.action% == playing
  switch %questid%
    case 18869
      set music_score %self.music_score%
      set roll %random.100%
      if %music_score% >= %roll%
        set person %self.room.people%
        while %person%
          if %actor.empire% != %person.empire%
            if !%person.mob_flagged(animal)%
              if %person.mob_flagged(hard)% || %person.mob_flagged(group)%
                set target %person%
                unset person
              end
            end
          end
          set person %person.next_in_room%
        done
        if !%target%
          %send% %actor% You don't notice anyone here dangerous enough to soothe.
          halt
        else
          %force% %target% madventurecomplete
          wait 1
          %force% %target% say Well, it isn't the Transylvania Twist, but it will do.
          %echo% %target.name% starts to energetically monster mash away from you.
          %purge% %target%
          %quest% %actor% trigger %questid%
        end
      else
        eval music_score %music_score% + 25
        remote music_score %self.id%
      end
    break
    case 18870
      if !%actor.room.in_city%
        %send% %actor% You will only reach your fallen citizens while within the city of your empire.
        halt
      end
      set flip_coin %random.2%
      if %flip_coin% == 2
        set counter 1
        if !%self.varexists(places)%
          set places %actor.room.coords%
          set places %places.car%%places.cdr%
        else
          set places %self.places%
          set loc %actor.room.coords%
          set loc %loc.car%%loc.cdr%
          while %places%
            if %loc% == %places.car%
              %send% %actor% You've already called forth the spirit of a fallen citizen here today.
              halt
            else
              eval counter %counter% + 1
            end
            set places %places.cdr%
          done
          set places %self.places% %loc%
        end
        if %counter% >= 5
          %quest% %actor% trigger %questid%
        else
          remote places %self.id%
        end
        %load% mob 18871
      end
    break
  done
else
  detach 18870 %self.id%
end
if %actor.quest_finished(%questid%)%
  %quest% %actor% finish %questid%
end
~
#18871
ghostly citizen spawns~
0 n 100
~
set who %random.1000%
if %who% == 1000
  %mod% %self% shortdesc the ghostly Khufu
  %mod% %self% longdesc A ghostly column of sand twists and coils in the shape of Khufu!
  %mod% %self% keywords khufu ghostly
  %mod% %self% lookdesc If you can read this, I have brief control of your thoughts.
elseif %who% >= 800
  %mod% %self% shortdesc a ghostly %self.room.empire_adjective% citizen
  %mod% %self% keywords citizen ghostly
  %mod% %self% longdesc A ghostly %self.room.empire_adjective% citizen hovers here.
  %mod% %self% lookdesc A ghostly citizen of %self.room.empire_name% is floating above the ground as it wanders.
elseif %who% >= 600
  %mod% %self% shortdesc a ghostly %self.room.empire_adjective% guard
  %mod% %self% longdesc A ghostly %self.room.empire_adjective% guard hovers here floating its ground.
  %mod% %self% keywords guard ghostly
  %mod% %self% lookdesc A ghostly guard of %self.room.empire_name% seems to still be attempting to stand guard over the city, no matter how long he or she has been dead.
elseif %who% >= 501
  %mod% %self% shortdesc a ghostly adventurer
  %mod% %self% longdesc A ghostly adventurer, complete with ghostly pack, hovers about.
  %mod% %self% keywords adventurer ghostly
  %mod% %self% lookdesc A ghostly adventurer of %self.room.empire_name% is still dressed in adventurering clothes, wearing a large pack, and ready for anything.
  switch %random.4%
    case 1
      %mod% %self% append-lookdesc However, they clearly ran a foul of some type of dragon. There's clear scorching all over their spectral gear.
    break
    case 2
      %mod% %self% append-lookdesc Except where chunks of their body have been chomped away by serpentine mouths.
    break
    case 3
      %mod% %self% append-lookdesc Even if they are a bit flattened, as though stepped on by a massive foot.
    break
    case 4
      %mod% %self% append-lookdesc Every bit of visible spectral flesh is tinged blue, as though they met their end under water.
    break
  done
elseif %who% == 500
  %mod% %self% shortdesc a ghostly Domino
  %mod% %self% longdesc A ghostly Domino tile slowly rotates in place in midair.
  %mod% %self% keywords domino ghostly tile
  %mod% %self% lookdesc This ghostly Domino tile is constantly in motion, and the dots constantly changing.
elseif %who% >= 400
  %mod% %self% shortdesc a ghostly %self.room.empire_adjective% child
  %mod% %self% longdesc A ghostly %self.room.empire_adjective% child flits in circles.
  %mod% %self% keywords child ghostly
  %mod% %self% lookdesc A child of %self.room.empire_name% is in constant movement, just as they were in life.
elseif %who% >= 300
  %mod% %self% shortdesc a ghostly %self.room.empire_adjective% farmer
  %mod% %self% longdesc A ghostly %self.room.empire_adjective% farmer desperately tries to continue working the fields.
  %mod% %self% keywords farmer ghostly
  %mod% %self% lookdesc A farmer of %self.room.empire_name% swings tool after tool in an attempt to harvest crops, hoe the ground, or really anything relating to their profession in life.
elseif %who% >= 200
  %mod% %self% shortdesc a ghostly %self.room.empire_adjective% scout
  %mod% %self% longdesc A ghostly %self.room.empire_adjective% scout maintains its vigil, hovering above the ground.
  %mod% %self% keywords scout ghostly
  %mod% %self% lookdesc Originally sticking to the outskirts of the empire, the scout of %self.room.empire_name% has chosen to stick closer to the city in death and see the sights it mostly missed in life.
elseif %who% >= 50
  %mod% %self% shortdesc a ghostly %self.room.empire_adjective% builder
  %mod% %self% longdesc A ghostly %self.room.empire_adjective% builder swings a hammer through the nearest wall to no effect.
  %mod% %self% keywords builder ghostly
  %mod% %self% lookdesc A builder of %self.room.empire_name% continues to swing their hammer, no matter how little impact it has on the structures the tool passes through.
else
  set person %self.room.people%
  set empire_check %self.room.empire%
  while %person%
    if %person.is_pc%
      if %person.empire% == %empire_check%
        set target %person%
      end
    end
    set person %person.next_in_room%
  done
  %mod% %self% shortdesc a ghostly %target.pc_name%
  %mod% %self% longdesc A ghostly %target.pc_name% floats above the ground.
  %mod% %self% keywords ghostly
  %mod% %self% lookdesc this ghostly version of %target.pc_name% is a citizen of %self.room.empire_name% the same as the living one.
end
%echo% A chill comes over you as %self.name% fades into view.
set day_count %dailycycle%
remote day_count %self.id%
%own% %self% %self.room.empire%
~
#18872
ghost can't stick around forever~
0 ab 10
~
if %self.varexists(day_count)%
  if %dailycycle% <= %self.day_count%
    halt
  end
end
if %self.vnum% == 18881
  %echo% %self.name% returns to the realm of the dead.
else
  %echo% %self.name% whispers, 'Thank you for this time to make peace,' and then fades away.
end
%purge% %self%
~
#18873
play the victim to death~
0 bw 100
~
set person %self.room.people%
while %person%
  if %person.is_pc%
    set actor %person%
  end
  set person %person.next_in_room%
done
if !%actor%
  halt
end
if %actor.action% != playing
  halt
end
if !%self.varexists(count_up)%
  set count_up 0
else
  set count_up %self.count_up%
end
eval count_up %count_up% + 1
switch %count_up%
  case 1
    %send% %actor% %self.name% locks %self.hisher% eyes on you as you play.
    remote count_up %self.id%
    halt
  break
  case 2
    %send% %actor% You watch as %self.name%'s eyes closely droop and %self.hisher% body begins to relax.
    remote count_up %self.id%
    halt
  break
  case 3
    %send% %actor% %self.name% finally gives up the ghost and healers usher you from the tent!
    %force% %actor% exit
    %quest% %actor% finish 18873
  break
done
if !%self.varexists(tent)%
  set tent %self.room.in_vehicle%
  remote tent %self.id%
end
%regionecho% %tent% 0 Healers sadly dismantle %tent.shortdesc%.
%purge% %tent%
~
#18874
reject those not on quest~
5 c 0
enter~
if !%actor.veh_target(%arg%)%
  return 0
  halt
end
if %actor.is_npc%
  return 1
  halt
end
set person %self.interior.people%
while %person%
  if %person.is_pc%
    %send% %actor% there's already someone in there playing music!
    return 1
    halt
  end
  set person %person.next_in_room%
done
if %actor.on_quest(18873)%
  return 0
  halt
end
%send% %actor% You have no business entering %self.shortdesc% at this time!
return 1
~
#18875
load the victim~
5 n 100
~
%at% %self.interior% %load% mob 18873
set victim %self.interior.people.id%
set spawn_time %timestamp%
set tent %self%
remote tent %victim%
remote spawn_time %victim%
~
#18876
tent should not persist~
5 ab 10
~
* No script
~
#18878
Halloween: Magi-genic Ooze mount upgrader~
1 c 2
use~
if %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
* All other results return 1
return 1
set vnum 0
if !%arg.cdr%
  %send% %actor% Use the ooze on which mount (giant tarantula or headless horse)?
  halt
elseif tarantula /= %arg.cdr% || giant tarantula /= %arg.cdr%
  set vnum 18879
else if horse /= %arg.cdr% || headless horse /= %arg.cdr%
  set vnum 18839
else
  %send% %actor% Use the ooze on which mount (giant tarantula or headless horse)?
  halt
end
if !%vnum% || !%actor.has_mount(%vnum%)%
  %send% %actor% You don't have that mount to upgrade.
  halt
end
if !%self.room.function(STABLE)%
  %send% %actor% You can only use %self.shortdesc% at a stable.
  halt
end
* validate upgrade
if %vnum% == 18879 && %actor.has_mount(18878)%
  %send% %actor% You can't use it on the tarantula -- you already have the tarantula hawk mount.
  halt
elseif %vnum% == 18839 && %actor.has_mount(18838)%
  %send% %actor% You can't use it on the headless horse -- you already have a flying one.
  halt
end
* ok do it
if %vnum% == 18879
  nop %actor.remove_mount(18879)%
  %send% %actor% You pour %self.shortdesc% onto your giant tarantula... It curls into a little ball...
  %send% %actor% Your giant tarantula splits open and a giant tarantula hawk flies out! You gain a new mount.
  %echoaround% %actor% %actor.name% pours %self.shortdesc% onto a giant tarantula... It splits open and a giant tarantula hawk flies out!
  nop %actor.add_mount(18878)%
elseif %vnum% == 18839
  nop %actor.remove_mount(18839)%
  %send% %actor% You pour %self.shortdesc% onto your headless horse... It sprouts bat wings and begins to fly!
  %echoaround% %actor% %actor.name% pours %self.shortdesc% onto a headless horse... It sprouts bat wings and begins to fly!
  nop %actor.add_mount(18838)%
else
  %send% %actor% It didn't seem to work.
  halt
end
* if we get here, we used it successfully
%purge% %self%
~
#18880
Ancestor's Offering: Invoke/Paint Ancestor~
1 c 2
invoke paint~
set room %actor.room%
* determine random name?
if %actor.varexists(halloween_grandma)%
  set halloween_grandma %actor.halloween_grandma%
else
  * pick a random name
  set names Agnes Ethel Martha Gertrude Esther Lottie Nannie Beulah Flossie Gladys Mildred Mamie Myrtle Bertha Irma Edith Ruth Carol Muriel
  eval pos %random.19% - 1
  while %pos% > 0
    set names %names.cdr%
    eval pos %pos% - 1
  done
  set halloween_grandma %names.car%
  remote halloween_grandma %actor.id%
end
* ok: which command?
if invoke /= %cmd%
  *** INVOKE COMMAND: Validate arg
  if !(ancestor /= %arg%)
    %send% %actor% Invoke whom?
    halt
  end
  if !%room.function(TOMB)%
    %send% %actor% You need to do that at a tomb.
    halt
  end
  * Check for existing spirit
  set ch %room.people%
  while %ch%
    if %ch.vnum% == 18880 && %ch.master% == %actor%
      %send% %actor% You already have an ancestor's spirit following you.
      halt
    end
    set ch %ch.next_in_room%
  done
  * load spirit
  %load% mob 18880
  set mob %room.people%
  if %mob.vnum% == 18880
    %mod% %mob% keywords spirit faded grandmother %halloween_grandma%
    %force% %mob% mfollow %actor%
    %send% %actor% You drip some blood on the ground and invoke the name of your ancestor, %halloween_grandma%!
    %echoaround% %actor% %actor.name% drips some blood on the name and shouts, 'Grandmother %halloween_grandma%, I invoke you!'
    %echo% %mob.name% rises from the grave.
  else
    %send% %actor% Something went wrong.
  end
  halt
elseif paint /= %cmd%
  *** PAINT COMMAND: Validate arg
  set mob %actor.char_target(%arg.car%)%
  if !%arg% || !%mob% || %mob.vnum% != 18880
    return 0
    halt
  end
  if %mob.master% != %actor%
    %send% %actor% You can only paint your own grandmother.
    halt
  end
  if %room.max_citizens% < 1
    %send% %actor% You can't really see Grandmother %halloween_grandma%'s spirit. You need to take her home to paint her.
    halt
  end
  %echo% The faded spirit is suddenly as crisp and sharp as she was in life. You can now clearly see that she is Grandmother %halloween_grandma%!
  %send% %actor% Grandmother %halloween_grandma% poses for you as you paint her picture on the old parchment, but quickly fades again after.
  %echoaround% %actor% Grandmother %halloween_grandma% poses for %actor.name% while %actor.heshe% paints her, but quickly fades again after.
  %load% obj 18881 %actor% inv
  set obj %actor.inventory(18881)%
  if %obj%
    %mod% %obj% keywords portrait Grandmother %halloween_grandma%
    %mod% %obj% shortdesc a portrait of Grandmother %halloween_grandma%
    %mod% %obj% longdesc A portrait of Grandmother %halloween_grandma% is lying here.
    %purge% %self%
    halt
  else
    %send% %actor% Something went wrong.
  end
else
  return 0
  halt
end
~
#18881
Ancestor's Offering: Place Portrait~
1 c 2
place~
* usage: place <portrait>
if !%arg% || %actor.obj_target(%arg.car%)% != %self%
  %send% %actor% Place what?
  halt
end
* pull vars
set room %actor.room%
if %actor.varexists(halloween_grandma)%
  set halloween_grandma %actor.halloween_grandma%
else
  * Failsafe
  set halloween_grandma Meemaw
end
* check room
if !%room.function(TOMB)%
  %send% %actor% You need to place %self.shortdesc% at Grandmother %halloween_grandma%'s tomb.
  halt
end
* load room obj
%load% obj 18882 room
set obj %room.contents(18882)%
if %obj%
  %mod% %obj% keywords portrait Grandmother %halloween_grandma%
  %mod% %obj% shortdesc a portrait of Grandmother %halloween_grandma%
  %mod% %obj% longdesc A portrait of Grandmother %halloween_grandma% has been set atop a tomb.
end
* find faded mob, if any
set old 0
set ch %room.people%
while %ch% && !%old%
  if %ch.vnum% == 18880 && %ch.master% == %actor%
    set old %ch%
  end
  set ch %ch.next_in_room%
done
* load new mob
%load% mob 18881
set new %room.people%
if %new.vnum% == 18881%
  %mod% %new% keywords %halloween_grandma% grandmother spirit
  %mod% %new% shortdesc Grandmother %halloween_grandma%
  %mod% %new% longdesc The spirit of Grandmother %halloween_grandma% is standing here.
  %own% %new% %actor.empire%
  set day_count %dailycycle%
  remote day_count %new.id%
  * messaging
  %send% %actor% You place %self.shortdesc% on the tomb with care...
  %echoaround% %actor% %actor.name% carefully places %self.shortdesc% on a tomb...
  if %old%
    %echo% %old.name% snaps suddenly into the world of the living, still semi-transparent but very clearly Grandmother %halloween_grandma%!
  else
    %echo% %new.name% emerges from the tomb, semi-transparent but whole!
  end
else
  %send% %actor% Something went wrong.
end
if %old%
  %purge% %old%
end
%quest% %actor% finish 18880
~
#18884
Plague Doctor Mask: Coughers abound~
1 b 3
~
* Randomly makes other humans in the room cough, in your own territory
set ch %self.worn_by%
if !%ch%
  halt
end
set room %ch.room%
if !ch.empire% || %room.empire% != %ch.empire%
  halt
end
* find a human target
set targ %random.char%
if %targ.is_npc% && !%targ.mob_flagged(HUMAN)%
  halt
end
* Ensure target is not wearing a mask
if %targ.eq(head)% && %targ.eq(head).vnum% == 18884
  halt
end
switch %random.5%
  case 1
    %send% %targ% You cough.
    %echoaround% %targ% %targ.name% coughs.
  break
  case 2
    %send% %targ% You cough violently.
    %echoaround% %targ% %targ.name% coughs violently.
  break
  case 3
    %send% %targ% You feel a little ill.
    %echoaround% %targ% %targ.name% looks a little ill.
  break
  case 4
    %send% %targ% You cough.
    %echoaround% %targ% %targ.name% coughs and you wish %targ.heshe% would stand further away.
  break
  case 5
    %send% %targ% You cough.
    %echoaround% %targ% %targ.name% coughs and you wish %targ.heshe% would cover %targ.hisher% mouth.
  break
done
~
#18887
Halloween: Dropped flower buff~
1 h 100
~
set room %actor.room%
dg_affect #18887 %actor% off
%send% %actor% You drop %self.shortdesc%, which crumbles to dust as it falls.
%echoaround% %actor% %actor.name% drops %self.shortdesc%, which crumbles to dust as it falls.
if %room.function(TOMB)%
  switch %self.vnum%
    case 18887
      dg_affect #18887 %actor% INVENTORY 15 3600
    break
    case 18888
      dg_affect #18887 %actor% MAX-MOVE 100 3600
    break
    case 18889
      dg_affect #18887 %actor% INFRA on 3600
    break
  done
end
return 0
%purge% %self%
~
#18890
Great Pumpkin wrong-month despawn~
0 n 100
~
* Despawns if it's not October in-game
if %time.month% != 10
  %echo% %self.name% returns to the Pumpkinverse.
  %purge% %self%
end
~
#18898
Flying pumpkin coach spell~
1 c 2
enchant~
* targeting
set coach %actor.veh_target(%arg.car%)%
if (!%arg% || !%coach%)
  return 0
  halt
end
* everything else will return 1
return 1
if %coach.vnum% != 18897
  %send% %actor% You can only use %self.shortdesc% to enchant an ordinary pumpkin coach from the Halloween event.
  halt
end
if (!%actor.empire% || %actor.empire% != %coach.empire%)
  %send% %actor% You can only use this on a coach you own.
  halt
end
if !%coach.complete%
  %send% %actor% You need to finish making the coach first.
  halt
end
if (%coach.sitting_in% || %coach.led_by%)
  %send% %actor% You can't do that while anyone is %coach.in_on% it!
  halt
end
* READY:
if %coach.animals_harnessed% > 1
  %send% %actor% You unharness the animals from %coach.shortdesc%...
  %echoaround% %actor% %actor.name% unharnesses the animals from %coach.shortdesc%...
  nop %coach.unharness%
elseif %coach.animals_harnessed% > 0
  %send% %actor% You unharness the animal from %coach.shortdesc%...
  %echoaround% %actor% %actor.name% unharnesses the animal from %coach.shortdesc%...
  nop %coach.unharness%
end
if %coach.contents%
  %send% %actor% You empty out %coach.shortdesc%...
  %echoaround% %actor% %actor.name% empties out %coach.shortdesc%...
end
nop %coach.dump%
%load% veh 18898
set upgr %self.room.vehicles%
if %upgr.vnum% == 18898
  %own% %upgr% %coach.empire%
  %send% %actor% You say, 'Salagadoola... mechicka... boola!'
  %echoaround% %actor% %actor.name% says, 'Salagadoola... mechicka... boola!'
  %send% %actor% You enchant %coach.shortdesc% with %self.shortdesc%...
  %echoaround% %actor% %actor.name% enchants %coach.shortdesc% with %self.shortdesc%...
  %echo% It begins to fly!
  %purge% %coach%
  %purge% %self%
else
  %send% %actor% The spell didn't seem to work!
end
~
#18899
Haunted Mansion interior~
2 o 100
~
* Add gallery west
if !%room.west(room)%
  %door% %room% west add 18896
end
* Add library east
if !%room.east(room)%
  %door% %room% east add 18895
end
* Add stair north
set stair %room.north(room)%
if !%stair%
  %door% %room% north add 18894
  set stair %room.north(room)%
end
* Rooms off the staicase
if %stair%
  * Ossuary down
  if !%stair.down(room)%
    %door% %stair% down add 18892
  end
  * Attic up
  if !%stair.up(room)%
    %door% %stair% up add 18893
  end
end
detach 18899 %room.id%
~
$
