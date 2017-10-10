#18800
Summon ghost with candy~
1 c 2
sacrifice~
* discard arguments after the first
eval arg %arg.car%
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
  return 0
  halt
end
eval room %self.room%
eval person %room.people%
while %person%
  if %person.vnum% == 18800
    %send% %actor% %person.name% is already here.
    halt
  end
  eval person %person.next_in_room%
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
eval mob %room.people%
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
eval arg1 %arg.car%
eval target %%actor.obj_target(%arg1%)%%
if %target% != %self%
  return 0
  halt
end
set found_grave 0
eval room %self.room%
if !%room.function(TOMB)% || !%room.bld_flagged(OPEN)% || !%room.complete%
  %send% %actor% You can only do that at an outdoor tomb building.
  halt
end
eval item %room.contents%
while %item%
  if %item.vnum% == 18800
    set found_grave 1
  end
  eval item %item.next_in_list%
done
if %found_grave%
  %send% %actor% You cannot summon another Headless Horse Man here yet.
  halt
end
eval arg2 %arg.cdr%
if !%arg2%
  %send% %actor% What difficulty would you like to summon the Headless Horse Man at? (Normal, Hard, Group or Boss)
  return 1
  halt
end
eval arg %arg2%
if normal /= %arg%
  %send% %actor% Setting difficulty to Normal...
  eval difficulty 1
elseif hard /= %arg%
  %send% %actor% Setting difficulty to Hard...
  eval difficulty 2
elseif group /= %arg%
  %send% %actor% Setting difficulty to Group...
  eval difficulty 3
elseif boss /= %arg%
  %send% %actor% Setting difficulty to Boss...
  eval difficulty 4
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
%load% mob 18801 %actor.level%
%load% obj 18800 room
eval mob %room.people%
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
  eval item %actor.inventory(18826)%
  %send% %actor% %self.name% gives you %item.shortdesc%!
  %echoaround% %actor% %self.name% gives %actor.name% %item.shortdesc%!
else
  %send% %actor% %self.name% politely declines your gift.
  return 0
end
~
#18818
Learn Halloween Costumes~
1 c 2
learn~
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
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
  eval learn %%actor.add_learned(%craft_vnum%)%%
  eval craft_vnum %craft_vnum% + 1
done
~
#18819
apply costume to citizens~
1 c 2
costume~
if %self.val0% == 0
  %send% %actor% You are out of costumes.
  halt
end
if !%arg%
  %send% %actor% Whom?
  eval costumes_left 18818-%self.val0%
  %send% %actor% You have %costumes_left% costumes left.
  halt
end
eval room %self.room%
eval permission %actor.canuseroom_guest%
if !%permission%
  %send% %actor% You don't have permission to do that here.
  halt
end
eval target %%actor.char_target(%arg%)%%
if !%target%
  %send% %actor% They're not here.
  halt
end
if %target.is_pc%
  %send% %actor% You can't costume players with %self.shortdesc%.
  halt
elseif %target.vnum% < 200 || %target.vnum% > 299 || !%target.mob_flagged(HUMAN)%
  %send% %actor% You can't put a costume on %target.name%.
  halt
elseif %target.mob_flagged(AGGR)% || %target.mob_flagged(CITYGUARD)%
  %send% %actor% You don't think %target.name% would be very pleased if you did that.
  halt
elseif %target.mob_flagged(!EXP)%
  %send% %actor% %target.name% declines.
  halt
elseif %target.morph% >= 18812 && %target.morph% <= 18817
  %send% %actor% %target.heshe%'s already wearing a costume!
  halt
elseif %target.morph%
  %send% %actor% You can't put a costume on %target.name%.
  halt
else
  eval prev_name %target.name%
  eval costume_vnum %self.val0%
  %morph% %target% %costume_vnum%
  %send% %actor% You dress up %prev_name% as %target.name%!
  %echoaround% %actor% %actor.name% dresses up %prev_name% as %target.name%!
  eval costume_vnum %costume_vnum% + 1
  if %costume_vnum% > 18817
    %quest% %actor% trigger 18819
    nop %self.val0(0)%
  else
    eval operate %%self.val0(%costume_vnum%)%%
    nop %operate%
  end
end
~
#18820
Halloween event quest items~
2 u 0
~
switch %questvnum%
  case 18820
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
if !%arg%
  %send% %actor% Whom?
  halt
end
eval room %self.room%
eval permission %actor.canuseroom_guest%
if !%permission%
  %send% %actor% You don't have permission to do that here.
  halt
end
eval target %%actor.char_target(%arg%)%%
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
  eval prev_name %target.name%
  eval costume_vnum 18821
  %morph% %target% %costume_vnum%
  %send% %actor% You wave %self.shortdesc% at %prev_name%, who turns into %target.name%!
  %echoaround% %actor% %actor.name% waves %self.shortdesc% at %prev_name%, who turns into %target.name%!
  eval charges %self.val0%
  eval charges %charges% - 1
  if %charges% == 0
    %quest% %actor% trigger 18821
  end
  eval operate %%self.val0(%charges%)%%
  nop %operate%
end
~
#18822
spawn ghosts~
1 b 25
~
eval room %self.room%
* room population check
eval person %room.people%
set ghosts 0
set people 0
while %person%
  eval people %people% + 1
  if %person.vnum% == 18822
    eval ghosts %ghosts% + 1
  end
  eval person %person.next_in_room%
done
if %people% > 5 || %ghosts% > 0
  halt
end
%load% mob 18822
eval mob %room.people%
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
eval room %actor.room%
eval cycles_left 5
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
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
  return 0
  halt
end
if %self.val0% < 1
  %send% %actor% %self.shortdesc% has run out.
  halt
end
eval room %self.room%
eval item %room.contents%
while %item%
  if %item.vnum% == 18825
    %send% %actor% Someone has beaten you to this house.
    halt
  end
  eval item %item.next_in_list%
done
eval vnum %room.building_vnum%
if !%vnum%
  %send% %actor% You can only use %self.shortdesc% inside a house.
  halt
end
switch %vnum%
  case 5100
    * Hut
  break
  case 5103
    * Mud Hut
  break
  case 5157
    * Swamp Hut
  break
  case 5102
    * Small house
  break
  case 5101
    * Cabin
  break
  case 5104
    * Pueblo
  break
  case 5124
    * Stone house
  break
  case 5125
    * Large house
  break
  case 5127
  break
  case 5128
  break
  case 5129
  break
  case 5159
  break
  case 5158
  break
  default
    %send% %actor% You can only use %self.shortdesc% inside a house.
    halt
  break
done
%send% %actor% You start applying %self.shortdesc% to the walls of the building...
%echoaround% %actor% %actor.name% starts applying %self.shortdesc% to the walls of the building...
wait 1 sec
* Skill check
eval chance %random.100%
if %chance% > %actor.skill(Stealth)%
  * Fail
  %send% %actor% You hear someone approaching!
  wait 1 sec
  %at% %room% %echo% A guard arrives!
  %at% %room% %load% mob 18824 %actor.level%
  eval guard %room.people%
  if %guard.vnum% == 18824
    %force% %guard% mhunt %actor%
  end
else
  %send% %actor% You finish bogrolling the building.
  %echoaround% %actor% %actor.name% finishes bogrolling the building.
  %send% %actor% Your Stealth skill ensures nobody notices your mischief.
  %load% obj 18825 room
  eval charges %self.val0%
  if %charges% == 1
    %send% %actor% %self.shortdesc% runs out!
    %quest% %actor% trigger 18824
  end
  eval charges %charges% - 1
  eval operate %%self.val0(%charges%)%%
  nop %operate%
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
if !%arg%
  %send% %actor% Who would you like to scare?
  return 1
  halt
end
eval target %%actor.char_target(%arg%)%%
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
  end
  eval operate %%self.val0(%times%)%%
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
eval basement %%greatrm.down(room)%%
if !%basement%
  %door% %greatrm% down add 18830
end
detach 18828 %self.id%
~
#18848
make offering to the spirits~
1 c 2
offer~
* Values:
* Sacrifices remaining; Sacrifice item vnum; ?
set sacrifice_amount %self.val2%
set found_grave 0
eval room %self.room%
if !%room.function(TOMB)% || !%room.complete%
  %send% %actor% You can only do that at a tomb building.
  halt
end
eval item %room.contents%
while %item%
  if %item.vnum% == 18849
    set found_grave 1
  end
  eval item %item.next_in_list%
done
if %found_grave%
  %send% %actor% Someone has already appeased the spirits at this tomb.
  halt
end
eval sacrifices_left %self.val0%
if %sacrifices_left% < 1
  %send% %actor% You have already appeased the spirits of the dead. Return and hand in the quest.
  halt
end
* actual sacrifice
eval sacrifice_vnum %self.val1%
eval has_items %%actor.has_resources(%sacrifice_vnum%, %sacrifice_amount%)%%
if !%has_items%
  %send% %actor% You don't have the items required for this sacrifice...
  halt
end
eval item %%actor.inventory(%sacrifice_vnum%)%%
eval item_name %item.shortdesc%
%send% %actor% You offer up %item_name% (x%sacrifice_amount%) to appease the spirits of the dead...
%echoaround% %actor% %actor.name% offers up %item_name% (x%sacrifice_amount%) to appease the spirits of the dead...
eval take %%actor.add_resources(%sacrifice_vnum%, -%sacrifice_amount%)%%
nop %take%
eval sacrifices_left %sacrifices_left% - 1
eval op %%self.val0(%sacrifices_left%)%%
nop %op%
if %sacrifices_left% == 0
  eval first_quest 18828
  eval last_quest 18832
  while %first_quest% <= %last_quest%
    %quest% %actor% trigger %first_quest%
    eval first_quest %first_quest% + 1
  done
end
%load% obj 18849 room
~
$
