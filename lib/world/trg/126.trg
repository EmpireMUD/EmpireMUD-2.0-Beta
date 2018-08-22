#12600
Elemental trap~
1 c 2
trap~
if !%arg%
  %send% %actor% Trap whom?
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
elseif %target.vnum% < 12601 || %target.vnum% > 12605
  %send% %actor% You can only use this trap on elementals from the rift.
  halt
elseif %target.fighting%
  %send% %actor% You can't trap someone who is fighting.
  halt
else
  %send% %actor% You capture %target.name% with %self.shortdesc%!
  %echoneither% %actor% %target% %actor.name% captures %target.name% with %self.shortdesc%!
  * Essences have the same vnum as the mob
  %load% obj %target.vnum% %actor% inv
  set obj %actor.inventory%
  %send% %actor% You receive %obj.shortdesc%!
  %purge% %target%
  %purge% %self%
end
~
#12601
Earth Elemental: Burrow Charge~
0 k 100
~
if %self.cooldown(12605)%
  halt
end
nop %self.set_cooldown(12605, 20)%
%send% %actor% %self.name% burrows into the ground and starts shifting through the earth towards you!
%echoaround% %actor% %self.name% burrows into the ground and starts shifting through the earth towards %actor.name%!
%echo% (Type 'stomp' to interrupt it.)
set running 1
remote running %self.id%
set success 0
remote success %self.id%
dg_affect #12601 %self% DODGE 50 10
nop %self.add_mob_flag(NO-ATTACK)%
wait 10 seconds
set running 0
remote running %self.id%
if %self.varexists(success)%
  if %self.success%
    set success 1
  end
end
if %success%
  nop %self.remove_mob_flag(NO-ATTACK)%
  dg_affect #12601 %self% off
else
  %send% %actor% %self.name% bursts from the ground beneath your feet, knocking you over!
  %echoaround% %actor% %self.name% bursts from the ground beneath %actor.name%'s feet, knocking %actor.himher% over!
  %damage% %actor% 75 physical
  dg_affect #12606 %actor% STUNNED on 5
  nop %self.remove_mob_flag(NO-ATTACK)%
  dg_affect #12601 %self% off
end
~
#12602
Fire Elemental: Summon Ember~
0 k 100
~
if %self.cooldown(12605)%
  halt
end
nop %self.set_cooldown(12605, 20)%
%echo% %self.name% grabs a handful of soil from the ground and crushes it in %self.hisher% fist!
%load% mob 12605 ally
%echo% A floating ember is formed!
~
#12603
Air Elemental: Dust~
0 k 100
~
if %self.cooldown(12605)%
  halt
end
nop %self.set_cooldown(12605, 20)%
%send% %actor% %self.name% whips up a cloud of dust and launches it into your face, blinding you!
%echoaround% %actor% %self.name% whips up a cloud of dust and launches it into %actor.name%'s face, blinding %actor.himher%!
dg_affect #12603 %actor% BLIND on 5
~
#12604
Water Elemental: Envelop~
0 k 100
~
if %self.cooldown(12605)%
  halt
end
nop %self.set_cooldown(12605, 20)%
%send% %actor% %self.name% surges forward and envelops you!
%echoaround% %actor% %self.name% surges forward and envelops %actor.name%!
%send% %actor% (Type 'struggle' to break free.)
dg_affect #12604 %actor% HARD-STUNNED on 20
dg_affect #12607 %self% HARD-STUNNED on 20
while %actor.affect(12604)%
  %send% %actor% %self.name% pummels and crushes you!
  %echoaround% %actor% %self.name% pummels and crushes %actor.name%!
  %damage% %actor% 50 physical
  %send% %actor% (Type 'struggle' to break free.)
  wait 5 sec
done
~
#12605
Elemental Rift spawn~
0 n 100
~
set room %instance.location%
if !%room%
  halt
end
mgoto %room%
if %self.vnum% != 12600
  mmove
  mmove
  mmove
  mmove
  mmove
  mmove
  mmove
  mmove
  mmove
  mmove
end
if %self.vnum% == 12602
  %load% obj 12611 %self% inv
end
~
#12606
Elemental Death~
0 f 100
~
if %instance.start%
  %at% %instance.start% %load% obj 12610
end
set obj %self.inventory%
while %obj%
  set next_obj %obj.next_in_list%
  %purge% %obj%
  set obj %next_obj%
done
~
#12607
Stomp earth elemental~
0 c 0
stomp~
if !%self.varexists(success)%
  set success 0
  remote success %self.id%
end
if !%self.varexists(running)%
  set running 0
  remote running %self.id%
end
if !%self.running% || %self.success%
  %send% %actor% You don't need to do that right now.
  halt
end
%send% %actor% You stomp down on %self.name%, interrupting %self.hisher% burrowing charge and stunning %self.himher%.
%echoaround% %actor% %actor.name% stomps down on %self.name%, interrupting %self.hisher% burrowing charge and stunning %self.himher%.
set success 1
remote success %self.id%
dg_affect #12606 %self% STUNNED on 5
nop %self.remove_mob_flag(NO-ATTACK)%
dg_affect #12601 %self% off
%echo% %self.name% emerges from the earth, dazed.
~
#12608
Ember: Attack~
0 k 100
~
%send% %actor% %self.name% shoots a small bolt of fire at you.
%echoaround% %actor% %self.name% shoots a small bolt of fire at %actor.name%.
%damage% %actor% 25
~
#12609
Delayed completion on quest start~
0 uv 0
~
if %instance.start%
  %at% %instance.start% %load% obj 12610
end
~
#12610
Delayed Completer~
1 f 0
~
%adventurecomplete%
~
#12611
Water elemental: Struggle~
0 c 0
struggle~
set break_free_at 1
if !%actor.affect(12604)%
  return 0
  halt
end
if !%actor.varexists(struggle_counter)%
  set struggle_counter 0
  remote struggle_counter %actor.id%
else
  set struggle_counter %actor.struggle_counter%
end
eval struggle_counter %struggle_counter% + 1
if %struggle_counter% >= %break_free_at%
  %send% %actor% You break free of %self.name%'s grip!
  %echoaround% %actor% %actor.name% breaks free of %self.name%'s grip!
  dg_affect #12604 %actor% off
  dg_affect #12607 %self% off
  rdelete struggle_counter %actor.id%
  halt
else
  %send% %actor% You struggle against your bindings, but fail to break free.
  %echoaround% %actor% %actor.name% struggles against %actor.hisher% bindings!
  remote struggle_counter %actor.id%
  halt
end
~
#12612
Give rejection~
0 j 100
~
%send% %actor% Don't give the item to %self.name%, use 'quest finish <quest name>' instead (or 'quest finish all').
return 0
~
#12650
Mob block higher template id (Grove 2.0)~
0 s 100
~
* One quick trick to get the target room
eval room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if %actor.is_npc%
  halt
end
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
if %self.aff_flagged(STUNNED)% || %self.aff_flagged(HARD-STUNNED)% || %self.aff_flagged(!ATTACK)%
  halt
end
if %actor.aff_flagged(SNEAK)%
  halt
end
if %actor.on_quest(12650)%
  halt
end
set actor_clothes %actor.eq(clothes)%
if %actor_clothes%
  if %actor_clothes.vnum% == 12667
    halt
  end
end
%send% %actor% %self.name% bars your way.
return 0
~
#12651
Grove 2.0: Druid death~
0 f 100
~
if %actor.on_quest(12650)%
  %quest% %actor% drop 12650
  %send% %actor% You fail the quest Tranquility of the Grove for killing a druid.
  set fox %instance.mob(12676)%
  if %fox%
    %at% %fox.room% %load% mob 12677
    set new_mob %fox.room.people%
    %purge% %fox% $n vanishes, replaced with %new_mob.name%.
  end
end
if %self.vnum% < 12654 || %self.vnum% > 12657
  * Trash
  halt
end
* Tokens for everyone
set person %self.room.people%
while %person%
  if %person.is_pc%
    * You get a token, and you get a token, and YOU get a token!
    if %self.mob_flagged(HARD)%
      nop %person.give_currency(12650, 2)%
      %send% %person% As %self.name% dies, 2 %currency.12650(2)% fall to the ground!
      %send% %person% You take them.
    else
      nop %person.give_currency(12650, 1)%
      %send% %person% As %self.name% dies, a %currency.12650(1)% falls to the ground!
      %send% %person% You take the newly created token.
    end
  end
  set person %person.next_in_room%
done
~
#12652
Grove difficulty selector~
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
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
set vnum 12654
while %vnum% <= 12657
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
set level 1
set person %self.room.people%
while %person%
  if %person.is_pc%
    if %person.level% > %level%
      set level %person.level%
    end
  end
  set person %person.next_in_room%
done
%scale% instance %level%
%send% %actor% %self.shortdesc% parts before you.
%echoaround% %actor% %self.shortdesc% parts before %actor.name%.
set newroom i12652
if !%result%
  * global var not found
  set result up
end
%door% %self.room% %result% room %newroom%
%load% obj 12653
%load% mob 12661
%load% mob 12661
%load% mob 12661
%purge% %self%
~
#12653
Grove delayed despawner~
1 f 0
~
%adventurecomplete%
~
#12654
Magiterranean Grove environment~
2 bw 10
~
switch %random.4%
  case 1
    %echo% You hear the howl of nearby animals.
  break
  case 2
    %echo% A treebranch brushes your back.
  break
  case 3
    %echo% You hear strange chanting through the trees.
  break
  case 4
    %echo% Your footprints seem to vanish behind you.
  break
done
~
#12655
Difficulty selector load~
1 n 100
~
set tofind 12652
set room %self.room%
set north %room.north(room)%
set east %room.east(room)%
set west %room.west(room)%
set south %room.south(room)%
set northeast %room.northeast(room)%
set northwest %room.northwest(room)%
set southeast %room.southeast(room)%
set southwest %room.southwest(room)%
if %north%
  if %north.template% >= %tofind%
    set result north
  end
end
if %east%
  if %east.template% >= %tofind%
    set result east
  end
end
if %west%
  if %west.template% >= %tofind%
    set result west
  end
end
if %northeast%
  if %northeast.template% >= %tofind%
    set result northeast
  end
end
if %northwest%
  if %northwest.template% >= %tofind%
    set result northwest
  end
end
if %southeast%
  if %southeast.template% >= %tofind%
    set result southeast
  end
end
if %southwest%
  if %southwest.template% >= %tofind%
    set result southwest
  end
end
if %south%
  if %south.template% >= %tofind%
    set result south
  end
end
if %result%
  %door% %room% %result% purge
  global result
end
~
#12656
Wildling Ambusher reveal~
0 gi 100
~
if %actor%
  * Actor entered room - valid target?
  if (%actor.is_pc% && !%actor.nohassle%)
    set target %actor%
  end
else
  * entry - look for valid target in room
  set person %room.people%
  while %person%
    * validate
    if (%person.is_pc% && !%person.nohassle%)
      set target_clothes %person.eq(clothes)%
      if %target_clothes%
        eval clothes_valid (%target_clothes.vnum% == 12667)
      end
      if %target.on_quest(12650)% || %target.ability(Hide)% || %clothes_valid%
      else
        set target %person%
      end
    end
    set person %person.next_in_room%
  done
end
if !%target%
  halt
end
set clothes_valid 0
set target_clothes %target.eq(clothes)%
if %target_clothes%
  eval clothes_valid (%target_clothes.vnum% == 12667)
end
if %target.on_quest(12650)% || %target.ability(Hide)% || %clothes_valid%
  visible
  wait 2
  switch %self.vnum%
    case 12659
      %send% %target% You spot %self.name% swimming around nearby...
      %echoaround% %target% You spot %self.name% swimming around nearby...
    break
    case 12658
      %send% %target% You spot %self.name% clinging to the ceiling of the tunnel...
      %echoaround% %target% You spot %self.name% clinging to the ceiling of the tunnel...
    break
    default
      %send% %target% You spot %self.name% in the branches of a tree nearby...
      %echoaround% %target% You spot %self.name% in the branches of a tree nearby...
    break
  done
  * %echo% %self.name% does not react to your presence.
  halt
end
visible
wait 2
if %target.room% != %self.room% || %self.disabled% || %self.fighting%
  halt
end
switch %self.vnum%
  case 12659
    %send% %target% You spot %self.name% swimming around above you...
    %echoaround% %target% You spot %self.name% swimming around above %target.name%...
  break
  case 12658
    %send% %target% You spot %self.name% clinging to the ceiling above you...
    %echoaround% %target% You spot %self.name% clinging to the ceiling above %target.name%...
  break
  default
    %send% %target% You spot %self.name% in the branches of a tree above you...
    %echoaround% %target% You spot %self.name% in the branches of a tree above %target.name%...
  break
done
wait 3 sec
if %target.room% != %self.room% || %self.disabled% || %self.fighting%
  halt
end
switch %self.vnum%
  case 12659
    %send% %target% %self.name% swims down and attacks you!
    %echoaround% %target% %self.name% swims down and attacks %target.name%!
  break
  case 12658
    %echoaround% %target% %self.name% drops from the ceiling and attacks %target.name%!
    %send% %target% %self.name% drops from the ceiling and attacks you!
  break
  default
    %echoaround% %target% %self.name% leaps down and attacks %target.name%!
    %send% %target% %self.name% leaps down and attacks you!
  break
done
%aggro% %target%
~
#12657
Grove underground environment~
2 bw 10
~
switch %random.4%
  case 1
    %echo% There's a soft scratching sound just behind the tunnel wall.
  break
  case 2
    %echo% Something skitters past your ankle.
  break
  case 3
    %echo% A strange chant echoes through the tunnel.
  break
  case 4
    %echo% A root slithers slowly across the floor.
  break
done
~
#12658
Wildling combat: Nasty Bite~
0 k 100
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
* Nasty bite: low damage over time
%send% %actor% %self.name% snaps %self.hisher% teeth and takes off a piece of your skin!
%echoaround% %actor% %self.name% snaps %self.hisher% teeth at %actor.name% and takes off a piece of %actor.hisher% skin!
%damage% %actor% 50 physical
%dot% #12658 %actor% 100 30 physical
~
#12659
Faun Druid 2.0: Rejuvenate~
0 k 100
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
%echo% %self.name% glows faintly with magical vitality, and %self.hisher% wounds start to close.
eval amount (%level%/10) + 1
dg_affect #12659 %self% HEAL-OVER-TIME %amount% 30
~
#12660
Grove Druid 2.0: Firebolt~
0 k 100
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
wait 1 sec
dg_affect #12670 %self% HARD-STUNNED on 5
if %actor.trigger_counterspell%
  %send% %actor% %self.name% launches a bolt of fire at you, but your counterspell blocks it completely.
  %echoaround% %actor% %self.name% launches a bolt of fire at %actor.name%, but it fizzles out in the air in front of %actor.himher%.
  halt
else
  %send% %actor% &r%self.name% thrusts out %self.hisher% hand at you, and blasts you with a bolt of searing flames!
  %echoaround% %actor% %self.name% thrusts out %self.hisher% hand at %actor.name%, and blasts %actor.himher% with a bolt of searing flames!
  %damage% %actor% 100 fire
  %dot% #12660 %actor% 150 15 fire
end
~
#12661
Escaped wildling load~
0 n 100
~
mgoto %instance.location%
mmove
mmove
mmove
~
#12662
Squirrel Druid: Morph/Nibble~
0 k 100
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
wait 1
set old_name %self.name%
if !%self.morph%
  %morph% %self% 12662
  %send% %actor% %old_name%'s robe drops to the ground as you try to fight %self.himher%, and %self.name% scrambles out, attacking you!
  %echoaround% %actor% %old_name%'s robe drops to the ground as %actor.name% tries to fight %self.himher%, and %self.name% scrambles out, attacking %actor.himher%!
else
  %send% %actor% %self.name% nips at your ankles, drawing blood!
  %echoaround% %actor% %self.name% nips at %actor.name%'s ankles, drawing blood!
  %damage% %actor% 25 physical
  %dot% #12662 %actor% 25 10 physical
end
~
#12663
Badger Druid: Morph~
0 k 50
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
if !%self.morph%
  set current %self.name%
  %morph% %self% 12663
  %echo% %current% rapidly morphs into %self.name%!
end
wait 1 sec
%send% %actor% &r%self.name% sinks %self.hisher% teeth into your leg!
%echoaround% %actor% %self.name% sinks %self.hisher% teeth into %actor.name%'s leg!
%damage% %actor% 100 physical
~
#12664
Badger Druid: Earthen Claws~
0 k 100
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
dg_affect #12670 %self% HARD-STUNNED on 5
if %self.morph%
  set current %self.name%
  %morph% %self% normal
  %echo% %current% rapidly morphs into %self.name%!
end
wait 1 sec
if %actor.trigger_counterspell%
  %send% %actor% %self.name% gestures, and earthen claws burst from the soil, dissolving as they meet your counterspell!
  %echoaround% %actor% %self.name% gestures, and earthen claws burst from the soil, dissolving as they near %actor.name%!
  halt
else
  %send% %actor% &r%self.name% gestures, and earthen claws burst from the soil, tearing into you!
  %echoaround% %actor% &r%self.name% gestures, and earthen claws burst from the soil, tearing into %actor.name%!
  %damage% %actor% 50 physical
  eval scale %self.level%/15 + 1
  dg_affect #12664 %actor% RESIST-PHYSICAL -%scale% 15
end
~
#12665
Archdruid: Grand Fireball~
0 k 50
~
if %self.affect(12666)%
  %send% %actor% &rThe flames wreathing %self.name%'s staff burn you as %self.heshe% swings at you!
  %echoaround% %actor% The flames wreathing %self.name%'s staff burn %actor.name% as %self.name% swings at %actor.himher%!
  %damage% %actor% 50 fire
end
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
wait 1 sec
dg_affect #12670 %self% HARD-STUNNED on 20
%echo% %self.name% stakes %self.hisher% staff into the ground and begins chanting...
wait 5 sec
%echo% The bonfire flickers and dims as a glowing light builds in %self.name%'s cupped hands...
wait 5 sec
%echo% %self.name% holds %self.hisher% hands skyward, and the growing radiance of the large fireball held in %self.hisher% hands eclipses the dimming bonfire...
wait 5 sec
set actor %self.fighting%
if !%actor%
  %echo% %self.name% discharges the fireball in a blinding flare of light!
  halt
end
%send% %actor% %self.name% hurls the grand fireball at you!
%echoaround% %actor% %self.name% hurls the grand fireball at %actor.name%!
if %actor.trigger_counterspell%
  %send% %actor% The fireball strikes your counterspell and explodes with a fiery roar!
  %echoaround% %actor% The fireball explodes in front of %actor.name% with a fiery roar!
else
  %send% %actor% &rThe fireball crashes into you and explodes, bowling you over!
  %echoaround% %actor% The fireball crashes into %actor.name% and explodes, bowling %actor.himher% over!
  dg_affect #12665 %actor% STUNNED on 5
  %damage% %actor% 300 fire
end
%echo% &rA wave of scorching heat from the explosion washes over you!
%aoe% 75 fire
~
#12666
Archdruid: Ignite Weapon~
0 k 100
~
if %self.affect(12666)%
  %send% %actor% &rThe flames wreathing %self.name%'s staff burn you as %self.heshe% swings at you!
  %echoaround% %actor% The flames wreathing %self.name%'s staff burn %actor.name% as %self.name% swings at %actor.himher%!
  %damage% %actor% 50 fire
end
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
wait 1 sec
%echo% %self.name% thrusts %self.hisher% staff skyward!
dg_affect #12666 %self% SLOW on 15
~
#12667
Crow Druid: Morph~
0 k 50
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
if !%self.morph%
  set current %self.name%
  %morph% %self% 12667
  %echo% %current% rapidly morphs into %self.name% and takes flight!
  wait 1 sec
end
%send% %actor% &r%self.name% swoops down and knocks your weapon from your hand!
%echoaround% %actor% %self.name% swoops down and knocks %actor.name%'s weapon from %actor.hisher% hand!
%damage% %actor% 5 physical
dg_affect #12667 %actor% DISARM on 5
~
#12668
Crow Druid: Squall~
0 k 100
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
dg_affect #12670 %self% HARD-STUNNED on 5
if %self.morph%
  set current %self.name%
  %morph% %self% normal
  %echo% %current% lands and rapidly morphs into %self.name%!
  wait 1 sec
end
%echo% %self.name% gestures, and a sudden squall of wind knocks you off-balance!
set person %self.room.people%
while %person%
  if %person.is_enemy(%self%)%
    dg_affect #12663 %actor% TO-HIT -10 15
  end
  set person %person.next_in_room%
done
~
#12669
Turtle Druid: Morph~
0 k 50
~
if %self.aff_flagged(IMMUNE-DAMAGE)% && %random.4% == 4
  %echo% %self.name% has retreated into %self.hisher% shell.
  %echo% &YYou could 'stomp' on %self.hisher% shell to draw %self.himher% out.
  halt
end
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
if !%self.morph%
  set current %self.name%
  %morph% %self% 12666
  %echo% %current% rapidly morphs into %self.name%!
  wait 1 sec
end
%echo% %self.name% retreats into the safety of %self.hisher% hard shell.
%echo% &YYou could 'stomp' on %self.hisher% shell to draw %self.himher% out.
dg_affect #12669 %self% IMMUNE-DAMAGE on -1
nop %self.add_mob_flag(NO-ATTACK)%
~
#12670
Turtle Druid: Riptide~
0 k 100
~
if %self.cooldown(12657)%
  if %self.affect(12669) && %random.4% == 4
    %echo% %self.name% has retreated into %self.hisher% shell.
    %echo% &YYou could 'stomp' on %self.hisher% shell to draw %self.himher% out.
    halt
  end
  halt
end
nop %self.set_cooldown(12657, 30)%
dg_affect #12670 %self% HARD-STUNNED on 5
if %self.morph%
  set current %self.name%
  %morph% %self% normal
  if %self.affect(12669)%
    %echo% %self.name% emerges from %self.hisher% shell.
    nop %self.remove_mob_flag(NO-ATTACK)%
    dg_affect #12669 %self% off
  end
  %echo% %current% rapidly morphs into %self.name%!
  wait 1 sec
end
%echo% %self.name% moves %self.hisher% arms in sweeping motions, stirring up the water into a riptide!
set cycle 1
while %cycle% <= 3
  wait 5 sec
  %echo% &r%self.name%'s riptide pummels you violently!
  %aoe% 40 physical
  eval cycle %cycle% + 1
done
%echo% %self.name%'s riptide dissipates.
~
#12671
Stomp turtle druid~
0 c 0
stomp~
if !%self.affect(12669)%
  %send% %actor% You don't need to do that right now.
  halt
end
%send% %actor% You stomp down on %self.name%'s shell, and %self.heshe% emerges, dazed.
%echoaround% %actor% %actor.name% stomps down on %self.name%'s shell, and %self.heshe% emerges, dazed.
dg_affect #12669 %self% off
nop %self.remove_mob_flag(NO-ATTACK)%
dg_affect #12671 %self% HARD-STUNNED on 5
~
#12672
Grove Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12650)%
end
~
#12673
Grove 2.0: Tranquility Chant~
1 c 2
chant~
if (!(tranquility /= %arg%) || %actor.position% != Standing)
  return 0
  halt
end
set room %actor.room%
set cycles_left 5
while %cycles_left% >= 0
  eval sector_valid (%room.template% >= 12652 && %room.template% <= 12699)
  set mob %room.people%
  while %mob%
    if %mob.vnum% == 12686
      set rage_spirit_here 1
    end
    set mob %mob.next_in_room%
  done
  set object %room.contents%
  while %object% && !%already_done%
    if %object.vnum% == 12674
      set already_done 1
    end
    set object %object.next_in_list%
  done
  if (%actor.room% != %room%) || !%sector_valid% || %already_done% || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing) || %rage_spirit_here%
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 5
      %echoaround% %actor% %actor.name%'s chant is interrupted.
      %send% %actor% Your chant is interrupted.
    elseif %rage_spirit_here%
      %send% %actor% You can't perform the chant while the spirit of rage is here!
    elseif !%sector_valid%
      if %room.template% == 12650 || %room.template% == 12651
        %send% %actor% You must venture deeper into the Grove before performing the chant.
      else
        %send% %actor% You must perform the chant inside the Magiterranean Grove.
      end
    elseif %already_done%
      %send% %actor% The tranquility chant has already been performed here.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% %actor.name% closes %actor.hisher% eyes and starts a peaceful chant...
      %send% %actor% You clutch the totem of tranquility and begin to chant...
    break
    case 4
      %echoaround% %actor% %actor.name% sways as %actor.heshe% whispers strange words...
      %send% %actor% You sway as you whisper the words of the tranquil chant...
    break
    case 3
      %echoaround% %actor% %actor.name%'s totem of tranquility takes on a soft white glow, and the area around it seems to cool...
      %send% %actor% Your totem of tranquility takes on a soft white glow, and the area around it seems to cool...
    break
    case 2
      %echoaround% %actor% A peaceful feeling fills the area...
      %send% %actor% A peaceful feeling fills the area...
    break
    case 1
      %echoaround% %actor% %actor.name% whispers into the air, and the feeling of tranquility spreads...
      %send% %actor% You whisper into the air, and the feeling of tranquility spreads...
    break
    case 0
      if %random.2% == 2
        %echoaround% %actor% %actor.name%'s chant is interrupted as a spirit of rage materializes in front of %actor.himher%!
        %send% %actor% Your chant is interrupted as a spirit of rage materializes in front of you!
        %load% mob 12686 %instance.level%
        set spirit %self.room.people%
        set word_1 calm
        set word_2 relax
        set word_3 pacify
        set word_4 slumber
        eval correct_word %%word_%random.4%%%
        remote correct_word %spirit.id%
        %echo% A voice in your head whispers, 'Tell it to %correct_word%!'
        halt
      end
      %echoaround% %actor% %actor.name% completes %actor.hisher% chant, and the area is tranquilized!
      %send% %actor% You complete your chant, and the area is tranquilized!
      %load% obj 12674 %room%
      set person %room.people%
      while %person%
        set next_person %person.next_in_room%
        if %person.vnum% == 12660 || %person.vnum% == 12659 || %person.vnum% == 12658 || %person.vnum% == 12661
          %purge% %person% $n slinks away.
        elseif %person.vnum% >= 12650 && %person.vnum% <= 12657 || %person.vnum% == 12663
          dg_affect #12673 %person% !ATTACK on -1
          %echo% %person.name% looks pacified.
          if %person.vnum% >= 12654 && %person.vnum% <= 12657
            set give_token 1
          end
        end
        set person %next_person%
      done
      set done 1
      set vnum 12654
      while %vnum% <= 12657
        set druid %instance.mob(%vnum%)%
        if !%druid%
          set done 0
        else
          if !%druid.affect(12673)%
            set done 0
          end
        end
        eval vnum %vnum%+1
      done
      set person %self.room.people%
      while %person%
        if %person.is_pc% && %person.on_quest(12650)%
          if %give_token%
            %send% %person% You receive a %currency.12650(1)%.
            nop %person.give_currency(12650, 1)%
          end
          if %done%
            %send% %person% You have tranquilized all four of the druid leaders.
            %quest% %person% trigger 12650
          end
        end
        set person %person.next_in_room%
      done
      halt
    break
  done
  * Shortcut for immortals
  if !%actor.nohassle%
    wait 5 sec
  end
  eval cycles_left %cycles_left% - 1
done
~
#12674
Underwater (Grove 2.0)~
2 bgw 100
~
if !%actor%
  * Random
  if %random.10% == 10
    switch %random.4%
      case 1
        %echo% Something brushes against your leg.
      break
      case 2
        %echo% You hear a wild screech burble through the water.
      break
      case 3
        %echo% Your lungs feel like they might collapse.
      break
      case 4
        %echo% You try to swim toward the surface, but something drags you down again.
      break
    done
  end
  set person %room.people%
  while %person%
    if %person.is_pc%
      if %person.varexists(breath)%
        if %person.breath% < 0
          if %person.is_god% || %person.is_immortal% || %person.health% < 0
            halt
          end
          %send% %person% &rYou are drowning!
          eval amount (%person.breath%) * (-250)
          %damage% %person% %amount%
        end
      end
    end
    set person %person.next_in_room%
  done
  halt
end
if %actor.is_npc%
  halt
end
if !%actor.varexists(breath)%
  set breath 3
  remote breath %actor.id%
end
set breath %actor.breath%
eval breath %breath% - 1
if %breath% < 0
  eval amount (%breath%) * (-250)
  %damage% %actor% %amount% direct
end
remote breath %actor.id%
%load% obj 12675 %actor% inv
~
#12675
Breath Messaging (Grove 2.0 underwater)~
1 n 100
~
set actor %self.carried_by%
wait 1
if %actor.varexists(breath)%
  set breath %actor.breath%
  if %breath% == 0
    %send% %actor% You are about to drown!
  elseif %breath% == 1
    %send% %actor% You cannot hold your breath much longer! You should find air, and soon!
  elseif %breath% < 0
    %send% %actor% &rYou are drowning!
  elseif %breath% <= 5
    %send% %actor% You can't hold your breath much longer... You think you could swim for another %breath% rooms.
  end
end
%purge% %self%
~
#12676
Air Supply (Grove 2.0)~
2 g 100
~
* change based on quest, etc
set breath 5
if %actor.varexists(breath)%
  if %actor.breath% < %breath%
    %send% %actor% You take a deep breath of air, refreshing your air supply.
  end
end
remote breath %actor.id%
~
#12677
Apply snake oil to grove gear~
1 c 2
oil~
if !%arg%
  %send% %actor% Apply %self.shortdesc% to what?
  halt
end
set target %actor.obj_target_inv(%arg%)%
if !%target%
  %send% %actor% You don't seem to have a '%arg%'. (You can only apply %self.shortdesc% to items in your inventory.)
  halt
end
if %target.vnum% < 12657 || %target.vnum% > 12665
  %send% %actor% You can only apply %self.shortdesc% to equipment from the Magiterranean: The Grove shop.
  halt
end
if %target.is_flagged(SUPERIOR)%
  %send% %actor% %target.shortdesc% is already superior; applying %self.shortdesc% would have no benefit.
  halt
end
if %actor.level% < 50
  %send% %actor% You must be at least level 50 to use %self.shortdesc%.
  halt
end
%send% %actor% You apply %self.shortdesc% to %target.shortdesc%...
%echoaround% %actor% %actor.name% applies %self.shortdesc% to %target.shortdesc%...
%echo% As the oil soaks in, %target.shortdesc% takes on a faint glow and earthy smell... and looks a LOT more powerful.
nop %target.flag(SUPERIOR)%
%scale% %target% 75
%purge% %self%
~
#12678
Seeker Stone: Grove~
1 c 2
seek~
if !%arg%
  %send% %actor% Seek what?
  halt
end
set room %self.room%
if %room.rmt_flagged(!LOCATION)%
  %send% %actor% %self.shortdesc% spins gently in a circle.
  halt
end
if !(grove /= %arg%)
  return 0
  halt
else
  if %actor.cooldown(12678)%
    %send% %actor% %self.shortdesc% is on cooldown.
    halt
  end
  eval adv %instance.nearest_adventure(12650)%
  if !%adv%
    %send% %actor% Could not find a Grove instance.
    halt
  end
  nop %actor.set_cooldown(12678, 1800)%
  %send% %actor% You hold %self.shortdesc% aloft...
  eval real_dir %%room.direction(%adv%)%%
  eval direction %%actor.dir(%real_dir%)%%
  eval distance %%room.distance(%adv%)%%
  if %distance% == 0
    %send% %actor% There is a Magiterranean Grove right here.
    halt
  elseif %distance% == 1
    set plural tile
  else
    set plural tiles
  end
  if %actor.ability(Navigation)%
    %send% %actor% There is a Magiterranean Grove at %adv.coords%, %distance% %plural% to the %direction%.
  else
    %send% %actor% There is a Magiterranean Grove %distance% %plural% to the %direction%.
  end
  %echoaround% %actor% %actor.name% holds %self.shortdesc% aloft...
end
~
#12679
Druid spawner~
1 n 100
~
if %random.2% == 2
  %load% mob 12651
else
  %load% mob 12652
end
%purge% %self%
~
#12680
Grove track~
2 c 0
track~
if (!%actor.ability(Track)% || !%actor.ability(Navigation)%)
  * Fail through to ability message
  return 0
  halt
end
if !%arg%
  * has its own message which it's funnier to not interfere with
  return 0
  halt
end
if badger /= %arg%
  set target badger
elseif druid /= %arg% || grove /= %arg%
  set target druid
elseif wildling /= %arg% || ambusher /= %arg%
  set target wildling
elseif archdruid /= %arg%
  set target archdruid
elseif crow /= %arg%
  set target crow
elseif turtle /= %arg%
  set target turtle
elseif gopher /= %arg%
  set target gopher
else
  %send% %actor% You can't seem to find a trail.
  halt
end
eval already_done %%track_%target%%%
if %already_done%
  set result %already_done%
else
  set direction_1 northwest
  set direction_2 north
  set direction_3 northeast
  set direction_4 east
  set direction_5 southeast
  set direction_6 south
  set direction_7 southwest
  set direction_8 west
  eval result %%direction_%random.8%%%
  set track_%target% %result%
  global track_%target%
end
%send% %actor% You find a trail to the %result%!
~
#12681
Grove 2.0 Quest Finish completes adventure~
2 v 0
~
%adventurecomplete%
~
#12683
Grove Druid 2.0 Underwater: Water Blast~
0 k 100
~
if %self.cooldown(12657)%
  halt
end
nop %self.set_cooldown(12657, 30)%
dg_affect #12670 %self% HARD-STUNNED on 5
%send% %actor% %self.name% flicks %self.hisher% wrist and launches a blast of pressurized water at you!
%echoaround% %actor% %self.name% flicks %self.hisher% wrist and launches a blast of pressurized water at %actor.name%!
if %actor.trigger_counterspell%
  %send% %actor% The water blast hits your counterspell and dissipates.
  %echoaround% %actor% The water blast dissipates in front of %actor.name%.
  halt
else
  %send% %actor% &rThe water blast crashes into you, sending you spinning!
  %echoaround% %actor% The water blast crashes into %actor.name%, sending %actor.himher% spinning!
  %damage% %actor% 150 physical
end
~
#12685
Give Grove 2.0 chant item~
2 u 0
~
if %questvnum% == 12650
  %load% obj 12673 %actor% inv
end
~
#12686
Grove rage spirit speech~
0 d 1
*~
set word_1 calm
set word_2 relax
set word_3 pacify
set word_4 slumber
set iterator 1
while %iterator% <= 4
  eval current_word %%word_%iterator%%%
  if %current_word% == %correct_word%
    eval success %speech% ~= %current_word%
  elseif %speech% ~= %current_word%
    set failure 1
  end
  eval iterator %iterator% + 1
done
if %success% && !%failure%
  %echo% %self.name% begins to calm, and fades from view.
  %echo% The area is tranquilized!
  set room %self.room%
  %load% obj 12674 %room%
  set person %room.people%
  while %person%
    set next_person %person.next_in_room%
    if %person.vnum% == 12660 || %person.vnum% == 12659 || %person.vnum% == 12658 || %person.vnum% == 12661
      %purge% %person% $n slinks away.
    elseif %person.vnum% >= 12650 && %person.vnum% <= 12657 || %person.vnum% == 12663
      dg_affect #12673 %person% !ATTACK on -1
      %echo% %person.name% looks pacified.
      if %person.vnum% >= 12654 && %person.vnum% <= 12657
        set give_token 1
      end
    end
    set person %next_person%
  done
  set done 1
  set vnum 12654
  while %vnum% <= 12657
    set druid %instance.mob(%vnum%)%
    if !%druid%
      set done 0
    else
      if !%druid.affect(12673)%
        set done 0
      end
    end
    eval vnum %vnum%+1
  done
  set person %self.room.people%
  while %person%
    if %person.is_pc% && %person.on_quest(12650)%
      if %give_token%
        %send% %person% You receive a %currency.12650(1)%.
        nop %person.give_currency(12650, 1)%
      end
      if %done%
        %send% %person% You have tranquilized all four of the druid leaders.
        %quest% %person% trigger 12650
      end
    end
    set person %person.next_in_room%
  done
  %purge% %self%
else
  %send% %actor% &r%self.name% attacks you with a painful bolt of crimson light, and flies off into the trees!
  %echoaround% %actor% %self.name% attacks %actor.name% with a bolt of crimson light, and flies off into the trees!
  %damage% %actor% 200 magical
  %purge% %self%
end
~
#12687
Grove rage spirit time limit~
0 bnw 100
~
wait 6 sec
%echo% A voice in your head urges, 'Say it out loud!'
wait 8 sec
%echo% &r%self.name% releases a painful surge of crimson energy and flies off into the trees, looking furious!
%aoe% 150 magical
%purge% %self%
~
$
