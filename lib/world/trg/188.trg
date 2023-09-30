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
    %send% %actor% ~%person% is already here.
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
%send% %actor% You toss @%self% into an open grave...
%echoaround% %actor% ~%actor% tosses @%self% into an open grave...
%load% mob 18800
set mob %room.people%
if %mob.vnum% != 18800
  %echo% Something went wrong.
  halt
end
%echo% ~%mob% rises from the grave and devours @%self%!
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
  %send% %actor% That is not a valid difficulty level for this adventure.
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
      %send% %actor% You light @%self% and hold it aloft!
      %echoaround% %actor% ~%actor% lights @%self% and holds it aloft!
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
%echo% ~%mob% bursts out of the fog in front of you!
remote diff %mob.id%
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
nop %mob.unscale_and_reset%
%echo% @%self% bursts into blue flames and rapidly crumbles to ash.
%purge% %self%
~
#18802
Offer Jammy Dodger~
0 j 100
~
if %object.vnum% == 18802
  return 0
  * Jammy dodger
  %send% %actor% You offer ~%self% a jammy dodger.
  %echoaround% %actor% ~%actor% offers ~%self% a jammy dodger.
  %purge% %object%
  wait 2
  say Oh! Thank you.
  wait 5
  %load% obj 18826 %actor% inventory
  set item %actor.inventory(18826)%
  %send% %actor% ~%self% gives you @%item%!
  %echoaround% %actor% ~%self% gives ~%actor% @%item%!
else
  %send% %actor% ~%self% politely declines your gift.
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
set diff %self.var(diff,1)%
scfight clear all
if %diff% > 1
  * heroic mode: AOE
  %echo% &&o&&Z~%self% rears up and prances!&&0
  wait 2 s
  %echo% &&o**** &&Z~%self% slams ^%self% hooves down on the ground, creating a shockwave! ****&0 (dodge)
  scfight setup dodge all
  wait 8 s
  set hit 0
  set ch %self.room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)%
      if %ch.var(did_scfdodge)%
        * dodged
      else
        * hit
        eval hit %hit% + 1
        %echo% &&o... the shockwave knocks ~%ch% off ^%ch% feet!&&0
        dg_affect #18803 %ch% STUNNED on 5
        %damage% %ch% 100 physical
      end
    end
    set ch %next_ch%
  done
  if %hit% < 1
    %echo% &&o... |%self% shockwave misses!&&0
  end
else
  * normal mode: single-target
  set actor_id %actor.id%
  %echo% &&o&&Z~%self% rears up and prances!&&0
  wait 2 s
  if %actor_id% != %actor.id%
    * gone
    halt
  end
  %send% %actor% &&o**** &&Z~%self% is prancing toward you! ****&&0 (dodge)
  %echoaround% %actor% &&o&&Z~%self% is prancing toward ~%actor%!&&0
  scfight setup dodge %actor%
  wait 6 s
  if %actor_id% != %actor.id%
    * gone
    %echo% &&o&&Z~%self% shrugs and lands ^%self% hooves back on the ground.&&0
    halt
  end
  * made it?
  if %actor.did_scfdodge%
    %echo% &&o&&Z~%self% comes crashing down but narrowly misses ~%actor%!&&0
    dg_affect #18802 %self% HARD-STUNNED on 8
  else
    %echo% &&o&&Z|%self% hooves crash down on ~%actor%!&&0
    dg_affect #18803 %actor% STUNNED on 5
    %damage% %actor% 100 physical
  end
end
scfight clear all
~
#18804
Headless Centaur: Neck Chop~
0 k 50
~
if %self.cooldown(18801)%
  halt
end
nop %self.set_cooldown(18801, 30)%
set diff %self.var(diff,1)%
scfight clear all
if %diff% > 1
  * heroic mode: AOE
  %echo% &&o**** &&Z~%self% swings ^%self% sword in a wide arc at neck level! ****&0 (dodge)
  scfight setup dodge all
  wait 8 s
  set hit 0
  eval pain %diff% * 75
  set ch %self.room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)%
      if %ch.var(did_scfdodge)%
        * dodged
      else
        * hit
        eval hit %hit% + 1
        %echo% &&o... the sword slices through ~%ch%, causing bleeding wounds!&&0
        %dot% #18804 %ch% 100 60 physical 10
        %damage% %ch% %pain% physical
      end
    end
    set ch %next_ch%
  done
  if %hit% < 1
    %echo% &&o... |%self% neck chop misses!&&0
  end
else
  * normal mode: single-target
  set actor_id %actor.id%
  %send% %actor% &&o**** &&Z~%self% swings ^%self% sword, slashing toward your neck! ****&&0 (dodge)
  %echoaround% %actor% &&o&&Z~%self% swings ^%self% sword toward |%actor% neck!&&0
  scfight setup dodge %actor%
  wait 6 s
  if %actor_id% != %actor.id%
    * gone
    halt
  end
  * made it?
  if %actor.did_scfdodge%
    %echo% &&o&&Z~%self% spins *%self%self around, wildly missing ~%actor%!&&0
    dg_affect #18802 %self% HARD-STUNNED on 8
  else
    %echo% &&o&&Z|%self% sword swings through |%actor% neck, opening a bleeding wound!&&0
    %damage% %actor% 150 physical
    %dot% #18804 %actor% 75 60 physical 10
  end
end
scfight clear all
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
set diff %self.var(diff,1)%
set room %self.room%
set lantern %self.room.people(18805)%
if %lantern%
  %echo% &&o~%self% urges ~%lantern% to attack faster!&&0
  dg_affect %lantern% HASTE on 30
else
  %load% mob 18805 ally
  set summon %room.people%
  if %summon.vnum% == 18805
    remote diff %summon.id%
    %echo% &&o~%self% thrusts ^%self% sword into the sky!&&0
    %echo% &&o~%summon% appears in a flash of blue fire!&&0
    %force% %summon% maggro %actor%
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
if %loot% == 1
  nop %self.remove_mob_flag(!LOOT)%
end
~
#18807
attack-o-lantern aoe~
0 k 100
~
if %self.cooldown(18805)%
  halt
end
scfight lockout 18805 30 30
set diff %self.var(diff,1)%
scfight clear all
%echo% &&o**** &&Z|%self% triangular eyes glow with infernal light... ****&&0 (interrupt)
scfight setup interrupt all
wait 8 s
if %diff% == 1
  set needed 1
else
  set needed %self.room.players_present%
end
if %self.var(count_scfinterrupt,0)% >= %needed%
  * miss
  %echo% &&o... |%self% eyes sputter and go out.&&0
  if %diff% == 1
    dg_affect #18802 %self% HARD-STUNNED on 8
  end
else
  * hit
  %echo% &&oBeams of magical energy blast forth from |%self% eyes!&&0
  eval pain %diff% * 75
  %aoe% %pain% magical
end
scfight clear all
~
#18808
put candy in pillowcase~
1 c 2
look examine put~
set Needs 31
if %actor.aff_flagged(blind)%
  return 0
  halt
end
if !%actor.on_quest(18808)% && (%actor.obj_target(%arg.cdr%)% == %self% || %actor.obj_target(%arg.car%)% == %self%)
  %send% %actor% @%self% suddenly vanishes!
  %purge% %self%
  halt
end
set Candy18802 %self.Candy18802%
set Candy18803 %self.Candy18803%
set Candy18804 %self.Candy18804%
set Candy18805 %self.Candy18805%
set Candy18806 %self.Candy18806%
set Candy18807 %self.Candy18807%
set Candy18808 %self.Candy18808%
set Candy18809 %self.Candy18809%
set Candy18810 %self.Candy18810%
set Candy18811 %self.Candy18811%
* only looking at it?
if %cmd% == look || %cmd% == examine
  if %arg.car% == in
    set arg %arg.cdr%
  end
  if %actor.obj_target(%arg.car%)% != %self%
    return 0
    halt
  end
  eval Need18802 %Needs% - %Candy18802%
  eval Need18803 %Needs% - %Candy18803%
  eval Need18804 %Needs% - %Candy18804%
  eval Need18805 %Needs% - %Candy18805%
  eval Need18806 %Needs% - %Candy18806%
  eval Need18807 %Needs% - %Candy18807%
  eval Need18808 %Needs% - %Candy18808%
  eval Need18809 %Needs% - %Candy18809%
  eval Need18810 %Needs% - %Candy18810%
  eval Need18811 %Needs% - %Candy18811%
  eval tot %Need18802% + %Need18803% + %Need18804% + %Need18805% + %Need18806% + %Need18807% + %Need18808% + %Need18809% + %Need18810% + %Need18811%
  if %tot% > 1
    set tot candies
  else
    set tot candy
  end
  %send% %actor% As you look into @%self%, you realize you still need the following %tot% to fill it:
  if %Need18802% > 0
    %send% %actor% &&0 %Need18802% jammy dodgers
  end
  if %Need18803% > 0
    %send% %actor% &&0 %Need18803% candy bonkers
  end
  if %Need18804% > 0
    %send% %actor% &&0 %Need18804% handfuls of smarties
  end
  if %Need18805% > 0
    %send% %actor% &&0 %Need18805% full-size candy bars
  end
  if %Need18806% > 0
    %send% %actor% &&0 %Need18806% stolen candies
  end
  if %Need18807% > 0
    %send% %actor% &&0 %Need18807% pixy sticks
  end
  if %Need18808% > 0
    %send% %actor% &&0 %Need18808% sour demon heads
  end
  if %Need18809% > 0
    %send% %actor% &&0 %Need18809% everlasting goblin stoppers
  end
  if %Need18810% > 0
    %send% %actor% &&0 %Need18810% manatomic fireball candies
  end
  if %Need18811% > 0
    %send% %actor% &&0 %Need18811% necro wafers
  end
  halt
end
* otherwise the command was 'put'
if %actor.obj_target(%arg.cdr%)% != %self%
  return 0
  halt
end
* detect arg
set PutObj %arg.car%
* check for "all" arg
if (%PutObj% == all || %PutObj% == all.Candies || %putObj% == all.Candy)
  set all 1
else
  set all 0
end
set CandyCount 0
eval Needs %Needs% * 10
* and loop
eval CandyTotal %Candy18802% + %Candy18803% + %Candy18804% + %Candy18805% + %Candy18806% + %Candy18807% + %Candy18808% + %Candy18809% + %Candy18810% + %Candy18811%
set item %actor.inventory%
eval ActorItem %actor.obj_target_inv(%PutObj%)%
while (%item% && (%all% || %CandyCount% == 0) && %CandyTotal% < %Needs%)
  set next_item %item.next_in_list%
  * use %ok% to control what we do in this loop
  if %all%
    set ok 1
  else
    * single-target: make sure this was the target
    if %ActorItem% == %item%
      set ok 1
    else
      set ok 0
    end
  end
  * next check the obj type if we got the ok
  if %ok%
    if %item.vnum% < 18802 || %item.vnum% > 18811
      if %all%
        set ok 0
      else
        %send% %actor% You can't put @%item% in @%self%... Only Halloween candy can be collected in your @%self%!
        * Break out of the loop early since it was a single-target fail
        halt
      end
    end
  end
  * still ok? see if we need one of these
  if %ok%
    set WhatCandy Candy%item.vnum%
    eval myCandy %%self.Candy%item.vnum%%%
    if %MyCandy% < 31 && !%item.is_flagged(*KEEP)%
      eval CandyCount %CandyCount% + 1
      %send% %actor% # You stash @%item% in @%self%.
      %echoaround% %actor% # ~%actor% puts @%item% in @%self%.
      eval %WhatCandy% %%self.%WhatCandy%%% + 1
      remote %WhatCandy% %self.id%
      %purge% %item%
      eval CandyTotal %CandyTotal% + 1
    else
      set SpecificCandy @%item%
    end
  end
  * and repeat the loop
  set item %next_item%
done
* did we fail?
if !%CandyCount%
  if %all%
    %send% %actor% You didn't have anything you could put into @%self%.
  elseif %SpecificCandy%
    %send% %actor% You can't fit %SpecificCandy% in @%self%.
  else
    %send% %actor% You don't seem to have %PutObj.ana% %PutObj%.
  end
end
wait 0
* get a candy total and see if the quest is over
if %CandyTotal% >= %Needs%
  %quest% %actor% finish 18808
  %purge% %self%
end
~
#18809
set variables on the pillowcase~
1 n 100
~
set Candy18802 0
set Candy18803 0
set Candy18804 0
set Candy18805 0
set Candy18806 0
set Candy18807 0
set Candy18808 0
set Candy18809 0
set Candy18810 0
set Candy18811 0
set target %self.id%
remote Candy18802 %target%
remote Candy18803 %target%
remote Candy18804 %target%
remote Candy18805 %target%
remote Candy18806 %target%
remote Candy18807 %target%
remote Candy18808 %target%
remote Candy18809 %target%
remote Candy18810 %target%
remote Candy18811 %target%
~
#18810
randomly trash the candy pillowcase if event isn't running~
1 b 20
~
if %event.running(18800)%
  halt
end
if %self.carried_by%
  %send% %self.carried_by% @%self% suddenly vanishes!
end
%purge% %self%
~
#18811
risen guard combat~
0 k 75
~
if %self.cooldown(18812)%
  halt
end
eval atk %random.2%
if %atk% == 1
  switch %random.5%
    case 1
      set BodyPart left arm
    break
    case 2
      set BodyPart right arm
    break
    case 3
      set BodyPart left leg
    break
    case 4
      set BodyPart right leg
    break
    case 5
      set BodyPart cheek
    break
  done
end
switch %atk%
  case 1
    if %self.level% >= 150
      set duration 90
    elseif %self.level% >= 140
      set duration 80
    elseif %self.level% >= 130
      set duration 70
    elseif %self.level% >= 120
      set duration 60
    elseif %self.level% >= 110
      set duration 50
    elseif %self.level% >= 100
      set duration 40
    else
      set duration 30
    end
    %echo% ~%self% lunges forward and bites ~%actor% on the %BodyPart%!
    %damage% %actor% 90 physical
    %dot% #18811 %actor% 30 %duration% poison 6
  break
  case 2
    %echo% ~%self% zombie stomps ~%actor% in the chest!
    if !%actor.disabled%
      if %self.level% >= 125
        set timer 20
      elseif %self.level% >= 100
        set timer 15
      elseif %self.level% >= 75
        set timer 10
      else
        set timer 5
      end
      dg_affect %actor% stunned on %timer%
    else
      %damage% %actor% 110 physical
    end
  break
done
nop %self.set_cooldown(18812, 25)%
~
#18812
corpse wagon is destroyed~
5 f 100
~
%load% veh 18859 %self.level%
if %self.empire%
  set burnt %self.room.vehicles%
  %own% %burnt% %self.empire%
end
return 0
%echo% The scent of burning flesh fills the air as the wagon's assortment of corpses is destroyed!
%purge% %self%
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
  %send% %actor% You can't costume players with @%self%.
  halt
elseif !%target.mob_flagged(HUMAN)%
  %send% %actor% You can't put a costume on ~%target%.
  halt
elseif %target.mob_flagged(AGGR)% || %target.mob_flagged(CITYGUARD)%
  %send% %actor% You don't think ~%target% would be very pleased if you did that.
  halt
elseif !%target.mob_flagged(EMPIRE)%
  %send% %actor% ~%target% declines.
  halt
elseif %target.morph% >= 18812 && %target.morph% <= 18817
  %send% %actor% &%target%'s already wearing a costume!
  halt
elseif %target.morph%
  %send% %actor% You can't put a costume on ~%target%.
  halt
else
  set prev_name %target.name%
  set costume_vnum %self.val0%
  %morph% %target% %costume_vnum%
  %send% %actor% You dress up %prev_name% as ~%target%!
  %echoaround% %actor% ~%actor% dresses up %prev_name% as ~%target%!
  eval costume_vnum %costume_vnum% + 1
  if %costume_vnum% > 18817
    %quest% %actor% trigger 18819
    nop %self.val0(0)%
    %quest% %actor% finish 18819
  else
    nop %self.val0(%costume_vnum%)%
    nop %actor.set_cooldown(18819,20)%
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
  case 18860
    %load% obj 18860 %actor% inv
  break
  case 18861
    %load% obj 18861 %actor% inv
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
  case 18808
    %load% obj 18848 %actor% inv
  break
  case 18866
    %load% obj 18866 %actor% inv
  break
done
~
#18821
toad citizen~
1 c 2
polymorph~
if %self.val0% == 0
  %send% %actor% @%self% is out of charges.
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
  %send% %actor% You can only use @%self% on generic empire citizens.
  halt
elseif %target.morph%
  %send% %actor% You can't use @%self% on people who are already disguised or transformed.
  halt
else
  set prev_name %target.name%
  set costume_vnum 18821
  %morph% %target% %costume_vnum%
  %send% %actor% You wave @%self% at %prev_name%, who turns into ~%target%!
  %echoaround% %actor% ~%actor% waves @%self% at %prev_name%, who turns into ~%target%!
  set charges %self.val0%
  eval charges %charges% - 1
  if %charges% == 0
    %quest% %actor% trigger 18821
    %quest% %actor% finish 18821
  else
    nop %self.val0(%charges%)%
    nop %actor.set_cooldown(18821,20)%
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
  %echo% ~%mob% emerges from @%self%!
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
  if (%actor.room% != %room%) || !%sector_valid% || !%actor.can_act%
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 5
      %echoaround% %actor% |%actor% ritual is interrupted.
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
      %echoaround% %actor% ~%actor% pulls out some chalk and begins the rite of spirits...
      %send% %actor% You pull out some chalk and begin the rite of spirits...
    break
    case 4
      %echoaround% %actor% ~%actor% draws mystic symbols in a circle on the ground...
      %send% %actor% You draw mystic symbols in a circle on the ground...
    break
    case 2
      %echoaround% %actor% |%actor% eyes go white as &%actor% channels the spirits of the departed...
      %send% %actor% You fall into a deep trance as you channel the spirits of the departed...
    break
    case 1
      %echoaround% %actor% ~%actor% whispers into the void...
      %send% %actor% You whisper powerful words into the void, awakening the souls beyond...
    break
    case 0
      %echoaround% %actor% ~%actor% completes ^%actor% ritual as spirits begin to fill the air!
      %send% %actor% You finish your ritual as spirits begin to fill the air!
      %load% obj 18822 room
      %quest% %actor% trigger 18823
      %send% %actor% @%self% bursts into flames!
      %echoaround% %actor% @%self% bursts into flames!
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
  %send% %actor% @%self% has run out.
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
  %send% %actor% You can only use @%self% inside a house.
  halt
end
if %room.max_citizens% < 1
  %send% %actor% You can only use @%self% inside a house.
  halt
end
set person %self.room.people%
while %person%
  if %person.vnum% == 18824 || %person.vnum% == 251 || %person.vnum% == 252 || %person.vnum% == 254
    %force% %person% shout Halt %actor.name%! What do you think you're doing?
    halt
  end
  set person %person.next_in_room%
done
%send% %actor% You start applying @%self% to the walls of the building...
%echoaround% %actor% ~%actor% starts applying @%self% to the walls of the building...
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
  %echoaround% %actor% ~%actor% finishes bogrolling the building.
  %send% %actor% Your Stealth skill ensures nobody notices your mischief.
  %load% obj 18825 room
  set charges %self.val0%
  if %charges% == 1
    %send% %actor% @%self% runs out!
    %quest% %actor% trigger 18824
    %quest% %actor% finish 18824
  else
    eval charges %charges% - 1
    nop %self.val0(%charges%)%
    nop %actor.set_cooldown(18824,20)%
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
  %send% %actor% You stalk toward ~%target%, baring your fangs as &%target% cowers in fear!
  %send% %target% ~%actor% stalks towards you, fangs bared in a terrifying snarl!
  %echoneither% %actor% %target% ~%actor% bares ^%actor% fangs and stalks towards ~%target%, who cowers in fear!
  halt
elseif %target.vnum% == 251
  %send% %actor% ~%actor% try to scare ~%target%, but take a gauntlet to the face for your effort!
  %echoaround% %actor% ~%actor% tries to scare ~%target% and takes a gauntlet to the face for ^%actor% effort!
  %damage% %actor% %random.20%
  halt
elseif %target.vnum% != 200 && %target.vnum% != 201 && %target.vnum% != 202 && %target.vnum% != 203 && %target.vnum% != 237
  * wrong target
  %send% %actor% You bare your fangs and snarl at ~%target%, who flinches away.
  %echoaround% %actor% ~%actor% bares ^%actor% fangs and snarls at ~%target%, who flinches away.
else
  %send% %actor% Bats swirl around you as you spread your flowing cape dramatically, bare your fangs, and snarl at ~%target%!
  %echoaround% %actor% Bats swirl around ~%actor% as &%actor% spreads ^%actor% flowing cape dramatically, bares ^%actor% fangs, and snarls at ~%target%!
  if %target.varexists(VampFear)%
    %echo% ~%target% immediately clutches ^%target% chest and drops to the ground, dead!
    %slay% %target%
    nop %actor.set_cooldown(18827,20)%
    halt
  end
  %echo% ~%target% panics, and attempts to flee!
  set VampFear 1
  remote VampFear %target.id%
  %force% %target% mmove
  %force% %target% mmove
  %force% %target% mmove
  eval times %self.val0% + 1
  if %times% == 5
    %send% %actor% You have scared enough citizens, but you can keep pretending to be Dracula if you want to.
    %quest% %actor% trigger 18827
    %quest% %actor% finish 18827
  else
    nop %self.val0(%times%)%
    nop %actor.set_cooldown(18827,20)%
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
elseif %roll% <= 118
  * 1% chance: zombie terrier whistle (2021)
  set vnum 18885
elseif %roll% <= 128
  * 1% chance: skeleton ghost whistle (2022)
  set vnum 18859
elseif %roll% <= 218
  * 9% chance: antique doll (2023)
  set vnum 18863
elseif %roll% <= 380
  * 16.2% chance: roll of pennies!
  set vnum 18886
elseif %roll% <= 635
  * 25.5% chance: marigold
  set vnum 18887
elseif %roll% <= 890
  * 25.5% chance: chrysanthemums
  set vnum 18888
else
  * Remaining 11%: black rose
  set vnum 18889
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
  * %send% %actor% You don't seem to have %arg.ana% '%arg%'. (You can only use @%self% on items in your inventory.)
  return 0
  halt
end
* All other cases return 1
return 1
if %target.vnum% != 18836 && %target.vnum% != 18883 && %target.vnum% != 18884 && %target.vnum% != 18847
  %send% %actor% You can only use @%self% on the plague doctor mask, oversized candy bag, ghastly shackles, or haunted mask.
  halt
end
if %target.is_flagged(SUPERIOR)%
  %send% %actor% @%target% is already upgraded; using @%self% would have no benefit.
  halt
end
%send% %actor% You carefully pour @%self% onto @%target%...
%echoaround% %actor% ~%actor% pours @%self% onto @%target%...
%echo% @%target% takes on a spooky glow!
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
%echoaround% %actor% ~%actor% offers up (%display_str%) to appease the spirits of the dead...
nop %actor.charge_component(%component_base%, %sacrifice_amount%)%
eval sacrifices_left %sacrifices_left% - 1
nop %self.val0(%sacrifices_left%)%
%load% obj 18849 room
if %sacrifices_left% == 0
  %quest% %actor% trigger %qvnum%
  %quest% %actor% finish %qvnum%
end
~
#18849
Halloween: Bylda Bear behavior~
0 bw 10
~
if %self.fighting% || %self.disabled%
  halt
end
set fright_list 200 201 202 203 222 223
set react_list 206 207 208 209 210 211 212 213 214 215 216 217 218 219 220 221 224 225 226 227 229 231 232 254 256 257 258 259 260 262 263 266 267 268 269 270 271 272 273 274 275 276 277 278 279 280 281 282 283 284 285
set eat_list 204 228 230 233 234 235 236 237 238 239 240 241 242 243 244 245 246 247 248 249 250 251 252 253 255 264 265 18841 18885 18861
set room %self.room%
%regionecho% %room% 10 A Bylda lets out a FEARSOME ROAR!!!
wait 1
set eaten 0
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_pc%
    * do nothing
  elseif (%fright_list%) ~= %ch.vnum%
    %force% %ch% flee
  elseif (%react_list%) ~= %ch.vnum%
    set type %random.4%
    if %type% == 1
      %echo% ~%ch% quivers with fear!
    elseif %type% == 2
      %echo% ~%ch% ducks in terror!
    elseif %type% == 3
      %echo% ~%ch% winces.
    else
      %echo% ~%ch% gulps nervously.
    end
  elseif !%eaten%
    if (%eat_list%) ~= %ch.vnum%
      set eaten 1
      %echo% ~%self% grabs ~%ch%, tosses *%ch% into the air, and eats *%ch% whole!
      %purge% %ch%
    end
  end
  set ch %next_ch%
done
~
#18850
Halloween: Bylda Bear finish~
5 n 100
~
wait 1
%echo% The Bylda lets out a powerful roar as it comes to life and smashes out of its frame!
set ch %self.room.people%
while %ch%
  if %ch.is_pc% && %ch.on_quest(18860)%
    %quest% %ch% trigger 18860
    %send% %ch% \&0
    %quest% %ch% finish 18860
  end
  set ch %ch.next_in_room%
done
%load% mob 18860
set mob %self.room.people(18860)%
if %mob%
  set day %dailycycle%
  remote day %mob.id%
end
%purge% %self%
~
#18851
Halloween: Purge Bylda at the end of the day~
0 ab 10
~
if %self.var(day,%dailycycle%)% != %dailycycle%
  %echo% ~%self% lets out one last roar and then disolves into wisps of glittering dust!
  %purge% %self%
end
~
#18852
demons are scared off~
0 g 33
~
set banish 0
if %actor.morph% == 18827
  set banish 1
elseif %actor.morph% >= 18812 && %actor.morph% <= 18817
  set banish 1
elseif %actor.eq(clothes)%
  set clothing %actor.eq(clothes)%
  if %clothing.vnum% >= 18812 && %clothing.vnum% <= 18817
    set banish 1
  end
end
if %banish% == 1
  %echo% ~%self% shreaks in terror and vanishes back to the realm whence &%self% came!
  %purge% %self%
end
~
#18853
dressing up the small demons~
0 n 100
~
* switch and random set:
switch %random.2%
  case 1
    set sex male
  break
  case 2
    set sex female
  break
done
set lead lead
switch %random.9%
  case 1
    set on_head singular black horn
    set lead leads
  break
  case 2
    set on_head two curving ram's horns
  break
  case 3
    set on_head blood coated spikes
  break
  case 4
    set on_head rusty iron horns
  break
  case 5
    set on_head patch of writhing tentacles
    set lead leads
  break
  case 6
    set on_head hair made of flames
    set lead leads
  break
  case 7
    set on_head cap of bone
    set lead leads
  break
  case 8
    set on_head ridge of bone
    set lead leads
  break
  case 9
    set on_head crown of thorns
    set lead leads
  break
done
* configs:
set skin_color_list black blue bronze brown gold green orange pink purple red silver white yellow
set skin_color_count 13
set eye_color_list black gold green red violet white yellow
set eye_color_count 7
set skin_type1_list feathered furred scaled skinned
set skin_type2_list feathers fur scales skin
set skin_type_count 4
set body_type_list brawny bulky chunky lean lithe muscular scrawny skeletal skinny stocky thin
set body_type_count 11
eval random_pos %%random.%skin_color_count%%%
while %random_pos% > 0
  set skin_color %skin_color_list.car%
  set skin_color_list %skin_color_list.cdr%
  eval random_pos %random_pos% - 1
done
if !%skin_color%
  * somehow?
  set skin_color black
end
eval random_pos %%random.%eye_color_count%%%
while %random_pos% > 0
  set eye_color %eye_color_list.car%
  set eye_color_list %eye_color_list.cdr%
  eval random_pos %random_pos% - 1
done
if !%eye_color%
  * somehow?
  set eye_color white
end
eval random_pos %%random.%body_type_count%%%
while %random_pos% > 0
  set body_type %body_type_list.car%
  set body_type_list %body_type_list.cdr%
  eval random_pos %random_pos% - 1
done
if !%body_type%
  * somehow?
  set body_type stocky
end
eval random_pos %%random.%skin_type_count%%%
while %random_pos% > 0
  set skin_type1 %skin_type1_list.car%
  set skin_type1_list %skin_type1_list.cdr%
  set skin_type2 %skin_type2_list.car%
  set skin_type2_list %skin_type2_list.cdr%
  eval random_pos %random_pos% - 1
done
if !%skin_type1%
  * somehow?
  set skin_type1 skinned
  set skin_type2 skin
end
%mod% %self% shortdesc a %eye_color%-eyed demon
%mod% %self% longdesc A small %skin_color%-%skin_type1% demon with %eye_color% eyes crouches to spring!
%mod% %self% keyword demon small %skin_color%
%mod% %self% sex %sex%
%mod% %self% lookdesc This small demon sports %skin_color% %skin_type2% covering a %body_type% build.
%mod% %self% append-lookdesc As you watch, %self.hisher% %eye_color% eyes find you and the %on_head% on %self.hisher% head %lead% the way toward you!
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
  %send% %actor% You pick |%target% pocket...
  nop %target.add_mob_flag(*PICKPOCKETED)%
  %load% obj 18855 %actor% inv
  set item %actor.inventory()%
  nop %actor.set_cooldown(18854,20)%
  %send% %actor% You find @%item%!
  return 1
else
  return 0
end
if %actor.quest_finished(18854)%
  %quest% %actor% finish 18854
end
~
#18855
nether portal closes~
5 ab 30
~
if %self.varexists(spawn_time)%
  eval check_time %timestamp% - %self.spawn_time%
  if %check_time% < 420
    halt
  end
end
%echo% %self.shortdesc% implodes with an other-worldly hiss!
%purge% %self%
~
#18856
look in magic mirror~
1 c 2
look examine~
if !%actor.on_quest(18856)%
  %send% %actor% @%self%? What about @%self%?
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
%send% %actor% You see yourself, %actor.name%, wearing @%clothing%.
%echoaround% %actor% You see ~%actor% look into @%self%.
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
%send% %actor% In the background of @%self% you see %clothing_count% shadowy %shadows% %build_up%
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
    %send% %actor% If you wanted to challenge someone, perhaps you should get your own @%self%?
    return 1
    halt
  end
  if !%actor.on_quest(18857)%
    %send% %actor% You hear a ghostly voice whisper, 'You should not have this any longer.'
    %echoaround% %actor% You hear a ghostly voice whispering, but can't make out the words.
    %echo% The @%self% vanishes into a puff of smoke!
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
    %echo% The @%self% vanishes in a puff of smoke!
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
  %send% %owner% %challenged.pc_name% has accepted your challenge, &%challenged% is up first.
  set turn %challenged%
  remote turn %self.id%
end
~
#18858
apple bobbing bob~
1 c 4
bob~
set otarg %actor.obj_target(%arg%)%
if !%otarg% || %otarg.vnum% != 18857
  %send% %actor% You can only bob in the @%self%.
  return 1
  halt
end
if !%self.varexists(turn)%
  %send% %actor% A challenge must be offered and accepted before anyone can bob for apples from this @%self%.
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
%echoaround% %actor% %actor.pc_name% sticks ^%actor% head into the bucket and starts looking for an apple.
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
  %echoaround% %turn% %turn.pc_name% suddenly pulls ^%turn% head out of the water gasping for air!
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
    %send% %actor% The @%self% vanishes from your arms in a poof of smoke!
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
%echo% The @%self% vanishes in a poof of smoke!
%purge% %self%
~
#18860
bobbing please stand up~
1 c 4
bob stand~
if %cmd% == bob
  if !(%actor.obj_target(%arg%)% == %self%)
    %send% %actor% You can only bob for apples in the @%self%.
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
    %echoaround% %actor% %actor.pc_name% stands back up immediately with no apple in ^%actor% mouth.
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
  %echoaround% %actor% %actor.pc_name% straightens up with a %apple_size% apple in ^%actor% mouth!
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
#18861
Set up Nether Portal~
2 g 100
~
set rn %room.north(room)%
* Add north first
if !%rn%
  %door% %room% north add 18862
  set rn %room.north(room)%
end
* If north worked:
if %rn% && !%room.east(room)%
  %door% %room% east room %rn%
end
if %rn% && !%room.west(room)%
  %door% %room% west room %rn%
end
if %rn% && !%room.south(room)%
  %door% %room% south room %rn%
end
* reset timer if possible
if %room.in_vehicle%
  set spawn_time %timestamp%
  remote spawn_time %room.in_vehicle.id%
end
* load the demon
%load% mob 18862
set mob %room.people%
if %mob.vnum% == 18862
  wait 1 sec
  %force% %mob% maggro
end
detach 18861 %room.id%
~
#18862
ritual of demon summoning~
1 c 2
ritual rite~
set room %actor.room%
if !%arg%
  return 0
  halt
end
if !(demons /= %arg%)
  return 0
  halt
end
if %room.empire% != %actor.empire% || !%room.in_city%
  %send% %actor% You can really only perform the demon ritual in one of your empire's cities.
  return 1
  halt
end
if %actor.cooldown(18862)%
  %send% %actor% You haven't recovered from the strain of the last portal yet.
  return 1
  halt
end
set veh %room.vehicles%
while %veh%
  if %veh.vnum% == 18861
    %send% %actor% Someone has already attempted to summon a demon here.
    halt
  end
  set veh %veh.next_in_room%
done
* Ritual portion
set cycles_left 5
while %cycles_left% >= 0
  if %actor.room% != %room% || !%actor.can_act%
    * We've either moved or the room's no longer suitable for the ritual
    if %cycles_left% < 5
      %echo% |%actor% ritual is interrupted.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that right now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% ~%actor% lowers ^%actor% head and ^%actor% eyes roll back in ^%actor% head.
      %send% %actor% You lower your head reverently...
    break
    case 4
      %echoaround% %actor% ~%actor% speaks in a low, droning tone with words you cannot comprehend...
      %send% %actor% You recite the names of the guardians of the Nether in your throatiest voice...
    break
    case 3
      %echoaround% %actor% ~%actor% seems to float a few inches above the ground as &%actor% speaks the words of the ritual...
      %send% %actor% You feel the Nether tugging at your soul as you speak into the void...
    break
    case 2
      %echo% A violet spark flickers to life in the air...
    break
    case 1
      %echo% The violet spark scrapes a 31-pointed star out of the air with the piercing sound of fingernails on slate...
    break
    case 0
      %echoaround% %actor% |%actor% eyes snap back as the star becomes a twisting portal!
      %send% %actor% You black out for a moment, and when your vision returns, you see the star has become a twisting portal!
      * Leave the loop
    break
  done
  if %cycles_left% > 0
    wait 5 sec
  end
  eval cycles_left %cycles_left% - 1
done
* After the ritual finishes:
nop %actor.set_cooldown(18862, 30)%
%load% veh 18861
set veh %self.room.vehicles%
set spawn_time %timestamp%
remote spawn_time %veh.id%
if !%actor.has_component(6200, 1)%
  %send% %actor% A clawed hand reaches out and yanks you off your feet and pulls you into the newly opened portal!
  %echoaround% %actor% You watch as a clawed hand reaches out and yanks ~%actor% off ^%actor% feet and into the newly opened portal!
  %teleport% %actor% %veh.interior%
  %force% %actor% look
else
  %load% mob 18861
  set demon %self.room.people%
  %echo% ~%demon% squeezes out of the portal and snatches the meat from |%actor% hands!
  nop %actor.charge_component(6200, 1)%
end
if !%self.varexists(portals)%
  set portals 0
else
  set portals %self.portals%
end
eval portals %portals% + 1
if %portals% < 3
  remote portals %self.id%
  halt
end
%quest% %actor% finish 18861
~
#18863
nether damage inside portal~
2 bw 100
~
set person %self.people%
while %person%
  if %person.is_pc% && %person.health% > 0
    eval dam_val 99 + %random.200%
    %send% %person% You feel an emptiness engulf your body!
    %damage% %person% %dam_val%
  end
  set person %person.next_in_room%
done
~
#18864
Nether portal - Block further entry~
2 q 100
~
if %actor.nohassle% || %direction% == none
  return 1
  halt
end
%send% %actor% Each time you try to walk away from the portal, you end up right back next to it.
return 0
~
#18865
small demons come from the nether~
5 b 30
~
set how_many %random.3%
switch %how_many%
  case 1
    %load% mob 18861
    set demon %self.room.people%
    %echo% ~%demon% squeezes out of %self.shortdesc%!
    %force% %demon% mmove
    %force% %demon% mmove
    %force% %demon% mmove
  break
  case 2
    %echo% A pair of demons burst their way out of %self.shortdesc%!
    switch %how_many%
      %load% mob 18861
      set demon %self.room.people%
      %force% %demon% mmove
      %force% %demon% mmove
      %force% %demon% mmove
      set how_many %how_many% - 1
    done
  break
  case 3
    set hell %random.10%
    if %hell% == 6
      set how_many 6
      %echo% A hoard of demons rip their way out of %self.shortdesc%!
    else
      %echo% A trio of demons muscle their way out of %self.shortdesc%!
    end
    switch %how_many%
      %load% mob 18861
      set demon %self.room.people%
      %force% %demon% mmove
      %force% %demon% mmove
      %force% %demon% mmove
      set how_many %how_many% - 1
    done
  break
done
~
#18866
track the blood feeding~
1 c 2
bite stop~
* make sure action is feeding
if %actor.action% != feeding
  return 0
  halt
end
set bitten %actor.biting%
return 0
if !%bitten.mob_flagged(human)% || %bitten.empire% != %actor.empire%
  halt
end
wait 0
* Did they let go?
if %actor.action% == feeding
  halt
end
if !%self.varexists(BiteList)%
  set BiteList %bitten.id%
  remote BiteList %self.id%
  %send% %actor% You managed to feed on ~%bitten% without killing *%bitten%. First one down!
  halt
end
set BiteList %self.BiteList%
set id %bitten.id%
set count 1
while %BiteList%
  set who %BiteList.car%
  if %who% == %id%
    halt
  end
  set BiteList %BiteList.cdr%
  eval count %count% + 1
done
set BiteList %self.BiteList% %id%
remote BiteList %self.id%
if %count% == 5
  %quest% %actor% trigger 18866
  %quest% %actor% finish 18866
else
  %send% %actor% You're up to %count% of 5 citizens now.
end
~
#18867
did the vampire kill them~
1 z 100
~
if !%killer.action(feeding)%
  halt
end
if !%self.varexists(BiteList)%
  halt
end
set BiteList %self.BiteList%
set bitten %actor.id%
while %BiteList%
  set check %BiteList.car%
  if %check% != %bitten%
    set hold %hold% %check%
  end
  set BiteList %BiteList.cdr%
done
set BiteList %hold%
remote BiteList %self.id%
%send% %killer% Well, ~%actor% doesn't count anymore.
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
        elseif %target.vnum% == 18801
          %send% %actor% The spirits whisper in the air, 'did you really think it would be that simple to get rid of the horseman?'
          halt
        else
          %force% %target% madventurecomplete
          wait 1
          %force% %target% say Well, it isn't the Transylvania Twist, but it will do.
          %echo% ~%target% starts to energetically monster mash away from you.
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
  %mod% %self% keywords ghostly %actor.name%
  %mod% %self% lookdesc this ghostly version of %target.pc_name% is a citizen of %self.room.empire_name% the same as the living one.
end
%echo% A chill comes over you as ~%self% fades into view.
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
  %echo% ~%self% returns to the realm of the dead.
else
  %echo% ~%self% whispers, 'Thank you for this time to make peace,' and then fades away.
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
    %send% %actor% ~%self% locks ^%self% eyes on you as you play.
    remote count_up %self.id%
    halt
  break
  case 2
    %send% %actor% You watch as |%self% eyes droop closed and ^%self% body begins to relax.
    remote count_up %self.id%
    halt
  break
  case 3
    * %send% %actor% ~%self% finally gives up the ghost and healers usher you from the tent!
    %quest% %actor% trigger 18873
  break
done
set tent %self.room.in_vehicle%
dg_affect %self% !see on -1
mgoto %tent.room%
nop %tent.dump%
%echo% Healers sadly dismantle %tent.shortdesc%.
%purge% %tent%
if %actor.quest_finished(18873)%
  %quest% %actor% finish 18873
end
%purge% %self%
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
    %send% %actor% There's already someone in there playing music!
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
0 ab 10
~
if %self.varexists(spawn_time)%
  set spawn_time %self.spawn_time%
  eval delay %timestamp% - %spawn_time%
  if %delay% <= 3600
    halt
  end
end
set tent %self.room.in_vehicle%
dg_affect %self% !see on -1
mgoto %tent.room%
nop %tent.dump%
%echo% %self% Healers sadly dismantle %tent.shortdesc%.
%purge% %tent%
%purge% %self%
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
elseif horse /= %arg.cdr% || headless horse /= %arg.cdr%
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
  %send% %actor% You can only use @%self% at a stable.
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
  %send% %actor% You pour @%self% onto your giant tarantula... It curls into a little ball...
  %send% %actor% Your giant tarantula splits open and a giant tarantula hawk flies out! You gain a new mount.
  %echoaround% %actor% ~%actor% pours @%self% onto a giant tarantula... It splits open and a giant tarantula hawk flies out!
  nop %actor.add_mount(18878)%
elseif %vnum% == 18839
  nop %actor.remove_mount(18839)%
  %send% %actor% You pour @%self% onto your headless horse... It sprouts bat wings and begins to fly!
  %echoaround% %actor% ~%actor% pours @%self% onto a headless horse... It sprouts bat wings and begins to fly!
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
if %self.vnum% == 18880
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
end
* ok: which command?
if invoke /= %cmd%
  if %self.vnum% == 18881
    %send% %actor% You've already invoked your grandmother and painted her. Just go place the portrait!
    halt
  end
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
    if %ch.vnum% == 18880 && %ch.leader% == %actor%
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
    %echoaround% %actor% ~%actor% drips some blood on the name and shouts, 'Grandmother %halloween_grandma%, I invoke you!'
    %echo% ~%mob% rises from the grave.
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
  if %self.vnum% == 18881
    %send% %actor% You've already got a nice portrait of your grandmother painted. Just go place it at her tomb!
    halt
  end
  if %mob.leader% != %actor%
    %send% %actor% You can only paint your own grandmother.
    halt
  end
  if %room.max_citizens% < 1
    %send% %actor% You can't really see Grandmother %halloween_grandma%'s spirit. You need to take her home to paint her.
    halt
  end
  %echo% The faded spirit is suddenly as crisp and sharp as she was in life. You can now clearly see that she is Grandmother %halloween_grandma%!
  %send% %actor% Grandmother %halloween_grandma% poses for you as you paint her picture on the old parchment, but quickly fades again after.
  %echoaround% %actor% Grandmother %halloween_grandma% poses for ~%actor% while &%actor% paints her, but quickly fades again after.
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
  %send% %actor% You need to place @%self% at Grandmother %halloween_grandma%'s tomb.
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
  if %ch.vnum% == 18880 && %ch.leader% == %actor%
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
  %send% %actor% You place @%self% on the tomb with care...
  %echoaround% %actor% ~%actor% carefully places @%self% on a tomb...
  if %old%
    %echo% ~%old% snaps suddenly into the world of the living, still semi-transparent but very clearly Grandmother %halloween_grandma%!
  else
    %echo% ~%new% emerges from the tomb, semi-transparent but whole!
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
    %echoaround% %targ% ~%targ% coughs.
  break
  case 2
    %send% %targ% You cough violently.
    %echoaround% %targ% ~%targ% coughs violently.
  break
  case 3
    %send% %targ% You feel a little ill.
    %echoaround% %targ% ~%targ% looks a little ill.
  break
  case 4
    %send% %targ% You cough.
    %echoaround% %targ% ~%targ% coughs and you wish &%targ% would stand further away.
  break
  case 5
    %send% %targ% You cough.
    %echoaround% %targ% ~%targ% coughs and you wish &%targ% would cover ^%targ% mouth.
  break
done
~
#18887
Halloween: Dropped flower buff~
1 h 100
~
set room %actor.room%
%send% %actor% You drop @%self%, which crumbles to dust as it falls.
%echoaround% %actor% ~%actor% drops @%self%, which crumbles to dust as it falls.
if %room.function(TOMB)%
  dg_affect #18887 %actor% off
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
set room %self.room%
if %room.time(month)% != 10
  %echo% ~%self% returns to the Pumpkinverse.
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
  %send% %actor% You can only use @%self% to enchant an ordinary pumpkin coach from the Halloween event.
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
  %echoaround% %actor% ~%actor% unharnesses the animals from %coach.shortdesc%...
  nop %coach.unharness%
elseif %coach.animals_harnessed% > 0
  %send% %actor% You unharness the animal from %coach.shortdesc%...
  %echoaround% %actor% ~%actor% unharnesses the animal from %coach.shortdesc%...
  nop %coach.unharness%
end
if %coach.contents%
  %send% %actor% You empty out %coach.shortdesc%...
  %echoaround% %actor% ~%actor% empties out %coach.shortdesc%...
end
nop %coach.dump%
%load% veh 18898
set upgr %self.room.vehicles%
if %upgr.vnum% == 18898
  %own% %upgr% %coach.empire%
  %send% %actor% You say, 'Salagadoola... mechicka... boola!'
  %echoaround% %actor% ~%actor% says, 'Salagadoola... mechicka... boola!'
  %send% %actor% You enchant %coach.shortdesc% with @%self%...
  %echoaround% %actor% ~%actor% enchants %coach.shortdesc% with @%self%...
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
