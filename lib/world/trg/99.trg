#9900
Mini-pet Use~
1 c 3
use~
* DEPRECATED: This controlled mini-pets before b5.47 when they got a code-based collection
return 0
halt
if %actor.obj_target(%arg%)% != %self% && !(%self.is_name(%arg%)% && %self.worn_by%)
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
set varname minipet%self.val0%
* once per 30 minutes
if %actor.cooldown(%self.val0%)%
  %send% %actor% %self.shortdesc% is on cooldown.
  halt
end
* check too many mobs
set mobs 0
set found 0
set found_pet 0
set ch %self.room.people%
while %ch% && !%found%
  if (%ch.is_npc% && %ch.vnum% == %self.val0% && %ch.master% && %ch.master% == %actor%)
    set found 1
  elseif (%ch.is_npc% && %ch.master% && %ch.master% == %actor% && !%ch.mob_flagged(FAMILIAR)%)
    set found_pet 1
  elseif %ch.is_npc%
    eval mobs %mobs% + 1
  end
  set ch %ch.next_in_room%
done
if %found%
  %send% %actor% You already have this mini-pet.
elseif %found_pet% then
  %send% %actor% You already have another non-familiar follower.
elseif %mobs% > 4
  %send% %actor% There are too many mobs here already.
else
  %send% %actor% You use %self.shortdesc%...
  %echoaround% %actor% %actor.name% uses %self.shortdesc%...
  nop %self.bind(%actor%)%
  %load% m %self.val0%
  set pet %self.room.people%
  if (%pet% && %pet.vnum% == %self.val0%)
    %force% %pet% mfollow %actor%
    %echo% %pet.name% appears!
    nop %actor.set_cooldown(%self.val0%, 1800)%
    dg_affect %pet% *CHARM on -1
    nop %pet.add_mob_flag(!EXP)%
    nop %pet.unlink_instance%
  end
end
~
#9901
Dismissable~
0 ct 0
dismiss~
if %arg% != pet
  if %actor.char_target(%arg%)% != %self%
    return 0
    halt
  end
end
if (%self.master% && %self.master% == %actor%)
  %send% %actor% You dismiss %self.name%.
  %echoaround% %actor% %actor.name% dismisses %self.name%.
  %purge% %self%
else
  if %arg% == pet
    return 0
    halt
  else
    %send% %actor% That's not your pet.
    return 1
    halt
  end
end
~
#9902
Lonely Despawn~
0 abt 5
~
set count 0
set target_char %self.room.people%
while %target_char%
  if (%target_char.is_pc%)
    eval count %count% + 1
  end
  set target_char %target_char.next_in_room%
done
* Despawn if no players present
if %count% < 1
  %purge% %self%
end
~
#9910
Use: Summon Mob / Mount Whistle~
1 c 2
use~
* use <self>: loads mob vnum val0 and purges self (single-use)
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.canuseroom_member%
  %send% %actor% You can't use %self.shortdesc% here because someone else owns this location.
  halt
end
%load% m %self.val0%
set mob %self.room.people%
if (%mob% && %mob.vnum% == %self.val0%)
  %send% %actor% You use %self.shortdesc% and %mob.name% appears!
  %echoaround% %actor% %actor.name% uses %self.shortdesc% and %mob.name% appears!
  nop %mob.unlink_instance%
else
  %send% %actor% You use %self.shortdesc% but nothing happens.
  %echoaround% %actor% %actor.name% uses %self.shortdesc% but nothing happens.
end
%purge% %self%
~
#9926
Heisenbug!~
0 btw 25
~
set master %self.master%
if !%master%
  %purge% %self%
  halt
end
if %self.aff_flagged(!SEE)%
  dg_affect %self% !SEE off 1
  dg_affect %self% SNEAK off 1
  %echo% %self.name% appears out of nowhere and starts following %master.name%.
else
  %echo% %self.name% vanishes into thin air.
  dg_affect %self% !SEE on -1
  dg_affect %self% SNEAK on -1
end
~
#9999
Iterative mini-pet reward~
1 c 2
use~
set pet_found 0
set vnum 9900
while !%pet_found%
  if %vnum% >= 9923
    %send% %actor% You already have all the mini-pets %self.shortdesc% can provide!
    %send% %actor% Keep it for now, and pester Yvain to add more.
    halt
  elseif %vnum% == 9919 || %actor.has_minipet(%vnum%)%
    eval vnum %vnum%+1
  else
    set pet_found %vnum%
  end
done
if %pet_found%
  %load% mob %pet_found%
  set mob %self.room.people%
  if %mob.vnum% != %pet_found%
    * Uh-oh.
    %echo% Something went horribly wrong while granting a mini-pet. Please bug-report this error.
    halt
  end
  set mob_string %mob.name%
  %purge% %mob%
  %send% %actor% You open %self.shortdesc% and find a whistle inside!
  %send% %actor% You gain '%mob_string%' as a mini-pet. Use the minipets command to summon it.
  %echoaround% %actor% %actor.name% opens %self.shortdesc% and takes %mob_string% whistle out.
  nop %actor.add_minipet(%vnum%)%
  %purge% %self%
end
~
$
