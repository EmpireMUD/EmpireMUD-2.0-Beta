#12000
Purge hint on cleanup~
2 e 100
~
set item %room.contents%
while %item%
  set next_item %item.next_in_list%
  if %item.vnum% == 12000
    %purge% %item%
  elseif %item.vnum% == 12030
    %purge% %item%
  end
  set item %next_item%
done
~
#12001
Old Gods difficulty selector~
0 c 0
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty. (Hard, Group or Boss)
  return 1
  halt
end
if %self.fighting%
  %send% %actor% You can't change %self.name%'s difficulty while %self.heshe% is in combat!
  return 1
  halt
end
%send% %actor% You set the difficulty...
%echoaround% %actor% %actor.name% sets the difficulty...
if hard /= %arg%
  set difficulty 2
elseif group /= %arg%
  set difficulty 3
elseif boss /= %arg%
  set difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure. (Hard, Group or Boss)
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
set mob %self%
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %difficulty% == 1
  * Then we don't need to do anything
elseif %difficulty% == 2
  %echo% %self.name% has been set to Hard.
  nop %mob.add_mob_flag(HARD)%
elseif %difficulty% == 3
  %echo% %self.name% has been set to Group.
  nop %mob.add_mob_flag(GROUP)%
elseif %difficulty% == 4
  %echo% %self.name% has been set to Boss.
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
%restore% %mob%
~
#12002
Old God death generic (unused)~
0 f 100
~
set tokens 0
if %self.mob_flagged(GROUP)%
  eval tokens %tokens% + 1
  if %self.mob_flagged(HARD)%
    eval tokens %tokens% + 1
  end
end
if %tokens% == 0
  halt
end
set person %self.room.people%
while %person%
  if %person.is_pc%
    %send% %person% As %self.name% is defeated, %self.hisher% essence spills forth, bestowing upon you %tokens% %currency.12000(%tokens%)%!
    if %person.level% >= (%self.level% - 75)
      nop %person.give_currency(12000, %tokens%)%
      if %tokens% == 1
        %send% %person% You take it.
      else
        %send% %person% You take them.
      end
    else
      if %tokens% == 1
        %send% %person% ...You try to take it, but you are too weak!
      else
        %send% %person% ...You try to take them, but you are too weak!
      end
    end
  end
  set person %person.next_in_room%
done
~
#12003
Old God load event~
0 n 100
~
set loc %instance.location%
if !%loc%
  halt
end
mgoto %loc%
if %loc.building_vnum% == 12001
  * bound here
  %self.add_mob_flag(SENTINEL)%
else
  * not bound
  if %self.vnum% == 12000
    %load% obj 12000
  elseif %self.vnum% == 12030
    %load% obj 12030
  end
  mmove
  mmove
end
~
#12004
Old Gods loot load boe/bop~
1 n 100
~
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
end
if !%actor%
  halt
end
if %actor% && %actor.is_pc%
  * Item was crafted
  if %self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if !%self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
  * Default flag is BOP so need to unbind when setting BOE
  nop %self.bind(nobody)%
else
  * Item was probably dropped
  if !%self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if %self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
end
~
#12005
Old Gods delayed despawn~
1 f 0
~
%adventurecomplete%
~
#12006
Old God death~
0 f 100
~
set loc %instance.location%
if %loc%
  if %self.vnum% == 12000
    %at% %loc% %load% obj 12002
  elseif %self.vnum% == 12030
    %at% %loc% %load% obj 12031
  end
end
set tokens 0
if %self.mob_flagged(GROUP)%
  eval tokens %tokens% + 1
  if %self.mob_flagged(HARD)%
    eval tokens %tokens% + 1
  end
end
if %tokens% == 0
  halt
end
set person %self.room.people%
while %person%
  if %person.is_pc%
    %send% %person% As %self.name% is defeated, %self.hisher% essence spills forth, bestowing upon you %tokens% %currency.12000(%tokens%)%!
    if %person.level% >= (%self.level% - 75)
      nop %person.give_currency(12000, %tokens%)%
      if %tokens% == 1
        %send% %person% You take it.
      else
        %send% %person% You take them.
      end
    else
      if %tokens% == 1
        %send% %person% ...You try to take it, but you are too weak!
      else
        %send% %person% ...You try to take them, but you are too weak!
      end
    end
  end
  set person %person.next_in_room%
done
~
#12007
Anat: Summon Yatpan / Rain of Arrows~
0 k 25
~
if %self.cooldown(12002)%
  halt
end
nop %self.set_cooldown(12002, 30)%
if %self.affect(BLIND)%
  %echo% %self.name%'s eyes flash red, and %self.hisher% vision clears!
  dg_affect %self% BLIND off 1
end
set person %self.room.people%
while %person%
  if %person.vnum% == 12001
    set yatpan %person%
  end
  set person %person.next_in_room%
done
if !%self.cooldown(12005)% && !%yatpan%
  * Summon Yatpan
  shout Yatpan! Come to me!
  nop %self.set_cooldown(12005, 300)%
  wait 2 sec
  %load% mob 12001 ally
  set summon %self.room.people%
  if %summon.vnum% == 12001
    %echo% %summon.name% drops from the sky, holding a bow, which %self.name% takes.
    %force% %summon% %aggro% %actor%
  end
else
  * Rain of Arrows
  %echo% %self.name% draws %self.hisher% bow.
  dg_affect %self% STUNNED on 5
  wait 2 sec
  %echo% %self.name% nocks hundreds of arrows and fires them into the air with blinding speed!
  set cycle 1
  while %cycle% <= 3
    wait 5 sec
    %echo% &r%self.name%'s arrows rain down upon you!
    %aoe% 85 physical
    eval cycle %cycle% + 1
  done
end
~
#12008
Anat: Bloodbath~
0 k 33
~
if %self.cooldown(12002)%
  halt
end
nop %self.set_cooldown(12002, 30)%
* Might need to clear blind?
%echo% %self.name% draws %self.hisher% axe.
dg_affect %self% STUNNED on 5
wait 2 sec
%echo% %self.name% raises her axe over one shoulder in a two-handed grip...
wait 3 sec
if %self.aff_flagged(ENTANGLE)%
  %echoaround% %actor% &r%self.name% hacks at you between attempts to free %self.himher%self from the entanglement!
  %echoaround% %actor% %self.name% hacks at %actor.name% between attempts to free %self.himher%self from the entanglement.
  %damage% %actor% 250 physical
  %dot% #12008 %actor% 150 30 physical
  halt
end
%echo% &r%self.name% charges forward, hacking and slicing at everyone!
%send% %actor% &rYou take the brunt of %self.name%'s assault!
%echoaround% %actor% %actor.name% takes the brunt of %self.name%'s assault!
%aoe% 25 physical
%damage% %actor% 200 physical
%dot% #12008 %actor% 150 30 physical
wait 5 sec
set amount 100
if %self.mob_flagged(GROUP)%
  set amount 200
  if %self.mob_flagged(HARD)%
    set amount 300
  end
end
set person %self.room.people%
while %person%
  if %person.is_enemy(%self%)%
    %send% %person% &rA fountain of blood suddenly bursts from the wounds left by %self.name%'s assault!
    %damage% %person% 100
    if %self.mob_flagged(GROUP)%
      eval amount %amount% + 75
    end
  end
  set person %person.next_in_room%
done
%echo% %self.name% bathes in the blood of %self.hisher% enemies, and %self.hisher% wounds close!
%heal% %self% health %amount%
~
#12009
Anat: Impale~
0 k 50
~
if %self.cooldown(12002)%
  halt
end
nop %self.set_cooldown(12002, 30)%
%echo% %self.name% draws %self.hisher% spear.
dg_affect %self% HARD-STUNNED on 5
wait 2 sec
%send% %actor% &r%self.name% drives %self.hisher% spear through your chest, impaling you upon it!
%echoaround% %actor% %self.name% drives %self.hisher% spear through %actor.name%'s chest, impaling %actor.himher% upon it!
if %self.mob_flagged(GROUP)%
  %damage% %actor% 500 physical
  dg_affect #12009 %actor% HARD-STUNNED on 15
  %dot% #12009 %actor% 400 15 physical
else
  %damage% %actor% 250 physical
  dg_affect #12009 %actor% HARD-STUNNED on 10
  %dot% #12009 %actor% 200 10 physical
end
wait 3 sec
%send% %actor% %self.name% releases %self.hisher% spear, allowing you to slump to the ground.
%echoaround% %actor% %self.name% releases %self.hisher% spear, allowing %actor.name% to slump to the ground.
~
#12010
Anat: Earthshatter~
0 k 100
~
if %self.cooldown(12002)%
  halt
end
nop %self.set_cooldown(12002, 30)%
%echo% %self.name% draws %self.hisher% hammer.
dg_affect %self% HARD-STUNNED on 10
wait 1 sec
%echo% &Y%self.name% raises her hammer overhead... (Jump now!)
set running 1
remote running %self.id%
wait 10 sec
set running 0
remote running %self.id%
shout Hammer down!
%echo% &Y%self.name% slams %self.hisher% hammer into the earth with a deafening bang, shattering the ground underfoot!
set person %self.room.people%
while %person%
  if %person.is_pc%
    set test %self.varexists(jumped_%person.id%)%
    if %test%
      eval test %%self.jumped_%person.id%%%
    end
    if !%test%
      %send% %person% &rThe force of the blow, traveling through the ground, leaves you stunned!
      %echoaround% %person% %person.name% is stunned!
      %damage% %person% 100 physical
      dg_affect #12010 %person% HARD-STUNNED on 20
    else
      %send% %person% You leap into the air, barely avoiding the shockwave.
      %echoaround% %person% %person.name% dodges the shockwave.
      unset jumped_%person.id%
    end
  end
  set person %person.next_in_room%
done
~
#12011
Yatpan: Hawk Dive~
0 k 100
~
if %self.cooldown(12003)%
  halt
end
nop %self.set_cooldown(12003, 30)%
if %self.affect(BLIND)%
  %echo% %self.name%'s eyes flash red, and %self.hisher% vision clears!
  dg_affect %self% BLIND off 1
end
set person %self.room.people%
while %person%
  if %person.vnum% == 12000
    set anat %person%
  end
  set person %person.next_in_room%
done
if !%anat%
  %echo% %self.name% gives a mournful screech, and vanishes in a flash of light!
  %purge% %self%
  halt
end
set target %random.enemy%
if !%target%
  %echo% %self.name% looks very confused.
  halt
end
if %target.is_pc%
  set target_string %target.firstname%
else
  set name %target.name%
  set target_string the %name.cdr%
end
%force% %anat% say Yatpan! Strike %target_string% down!
%send% %target% %self.name% screeches and dives at you!
%echoaround% %target% %self.name% screeches and dives at %target.name%!
* Give the tank time to prepare
wait 5 sec
if !%anat%
  %echo% %self.name% gives a mournful cry, and vanishes in a flash of light!
  %purge% %self%
  halt
end
%send% %target% &r%self.name% crashes into you, talons tearing, and seizes your weapon!
%echoaround% %target% %self.name% crashes into %target.name%, talons tearing, and seizes %target.hisher% weapon!
if %anat.mob_flagged(GROUP)%
  %damage% %target% 200 physical
  dg_affect #12013 %target% HARD-STUNNED on 10
  dg_affect #12012 %target% DISARM on 30
  %dot% #12011 %target% 150 60 physical
else
  %damage% %target% 100 physical
  dg_affect #12012 %target% DISARM on 15
end
~
#12012
Detect jump~
0 c 0
jump~
if !%self.varexists(running)%
  set running 0
  remote running %self.id%
end
if !%self.running%
  %send% %actor% There's nothing to jump over right now.
  return 1
  halt
end
%send% %actor% You crouch down and prepare to jump...
%echoaround% %actor% %actor.name% crouches down and prepares to jump...
set jumped_%actor.id% 1
remote jumped_%actor.id% %self.id%
~
#12013
Yatpan: Unsummon Self~
0 b 33
~
if %self.fighting%
  halt
end
set person %self.room.people%
while %person%
  if %person.vnum% == 12000
    set anat %person%
  end
  set person %person.next_in_room%
done
if !%anat%
  %echo% %self.name% gives a mournful screech, and vanishes in a flash of light!
  %purge% %self%
  halt
end
%force% %anat% say Begone, Yatpan!
%echo% %self.name% flies away.
nop %anat.set_cooldown(12005, 0)%
%purge% %self%
~
#12014
Anat Start Progression: room~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12000)%
end
~
#12015
Anat Start Progression: mob~
0 h 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12000)%
end
~
#12030
Hadad phase change: 1 to 2~
0 l 50
~
if %self.varexists(phase)%
  if %self.phase% > 2
    * Never mind
    halt
  elseif %self.phase% == 2
    %echo% %self.name% is suddenly rejuvenated by the power of the storm within!
    halt
  end
end
%echo% %self.name% is surrounded by a swirling mass of storm clouds, protecting %self.himher% from your attacks!
%echo% (Type 'enter storm')
set phase 2
remote phase %self.id%
* todo: make him invincible and not just insanely hard to kill
dg_affect #12031 %self% IMMUNE-DAMAGE on -1
if !%self.mob_flagged(GROUP)%
  nop %self.add_mob_flag(NO-ATTACK)%
end
detach 12035 %self.id%
detach 12036 %self.id%
detach 12037 %self.id%
detach 12038 %self.id%
attach 12039 %self.id%
makeuid loc room i12031
if %loc%
  set room %loc%
else
  %echo% Error finding storm chamber; please report a bug.
  halt
end
%at% %room% %load% mob 12031
~
#12031
Hadad phase reset~
0 ab 100
~
if !%self.fighting% && %self.varexists(phase)%
  if %self.phase% == 2
    %echo% %self.name%, no longer distracted by combat, turns his full attention inward!
    %echo% %self.name% is restored!
    * Uh oh
    makeuid loc room i12031
    if %loc%
      set room %loc%
    else
      %echo% Error finding storm chamber; please report a bug.
      halt
    end
    set person %room.people%
    while %person%
      set next_person %person.next_in_room%
      if %person.is_pc%
        %send% %person% %self.name% turns his gaze inward upon you!
        %send% %person% &rA torrent of lightning flows through you, blasting you out of the eye of the storm!
        %damage% %person% 99999 direct
        %teleport% %person% %self%
      elseif %person.vnum% == 12031
        %purge% %person% $n fades away.
      else
        * What are you doing in here?
        * Maybe a familiar?
        %echo% %person.name% is expelled from the storm chamber.
        %teleport% %person% %self%
      end
      set person %next_person%
    done
    dg_affect #12031 %self% off
    %restore% %self%
    unset phase
    detach 12039 %self.id%
    attach 12035 %self.id%
    attach 12036 %self.id%
    attach 12037 %self.id%
    attach 12038 %self.id%
    nop %self.remove_mob_flag(NO-ATTACK)%
  elseif %self.phase% > 2
    %echo% %self.name% is restored!
    %restore% %self%
    nop %self.remove_mob_flag(NO-ATTACK)%
    unset phase
  end
end
~
#12032
Hadad: Enter Storm~
0 c 0
enter~
set correct_phase 0
if %self.varexists(phase)%
  if %self.phase% == 2
    set correct_phase 1
  end
end
if storm /= %arg%
  if %actor.position% == Standing || %actor.position% == Fighting
    if %correct_phase%
      %send% %actor% You step into the swirling storm clouds and find yourself elsewhere!
      %echoaround% %actor% %actor.name% steps into the swirling storm clouds and vanishes!
      %teleport% %actor% i12031
      %echoaround% %actor% %actor.name% emerges from the swirling storm clouds!
      %force% %actor% look
      %force% %actor% kill baalhadadstormcloud
    else
      %send% %actor% You can't do that during the current phase of combat.
      return 1
      halt
    end
  else
    %send% %actor% You're not really in a position to be charging off into the clouds.
    return 1
    halt
  end
else
  return 0
  halt
end
~
#12033
Hadad: Coming Storm death~
0 f 100
~
set hadad %instance.mob(12030)%
if !%hadad%
  %echo% Ba'al Hadad is missing. Please report a bug.
  halt
end
if %hadad.varexists(phase)%
  if %hadad.phase% == 2
    set phase 3
    remote phase %hadad.id%
  end
end
dg_affect #12031 %hadad% off
nop %hadad.remove_mob_flag(NO-ATTACK)%
detach 12039 %hadad.id%
attach 12038 %hadad.id%
attach 12037 %hadad.id%
attach 12036 %hadad.id%
attach 12035 %hadad.id%
set destination %hadad.room%
set person %self.room.people%
%at% %destination% %echo% The eye of the storm suddenly collapses, as if destroyed from within!
%echo% As %self.name% is destroyed, the eye of the storm collapses!
while %person%
  set next_person %person.next_in_room%
  if %person.is_pc% || (%person.is_npc% && %person.master%)
    %send% %person% You are expelled from the eye of the storm!
    %echoaround% %person% %person.name% is expelled from the eye of the storm!
    %teleport% %person% %destination%
    %echoaround% %person% %person.name% is expelled from the eye of the storm!
  end
  set person %next_person%
done
~
#12034
Storm Chamber: Flee~
2 c 0
flee~
set mob %instance.mob(12030)%
if %mob%
  if %mob.fighting%
    %send% %actor% You can't! The fight is still raging outside.
    halt
  elseif %mob.varexists(phase)%
    if %mob.phase% > 1
      %send% %actor% You can't!
      halt
    end
  else
    set destination %mob.room%
  end
else
  set destination %instance.location%
end
%send% %actor% You flee from the heart of the storm.
%echoaround% %actor% %actor.name% flees from the heart of the storm.
%teleport% %actor% %destination%
%echoaround% %actor% %actor.name% appears seemingly from nowhere.
%force% %actor% %look%
~
#12035
Ba'al Hadad: Sonic Roar~
0 k 25
~
if %self.cooldown(12030)%
  halt
end
nop %self.set_cooldown(12030, 30)%
* Might need to clear blind?
%echo% %self.name% takes a deep breath...
wait 2 sec
%echo% %self.name% lets out a mighty leonine roar!
%echo% &rYou are stunned by the loudness of %self.name%'s roar!
set person %self.room.people%
while %person%
  if %person.is_enemy(%self%)%
    if %self.mob_flagged(GROUP)%
      dg_affect #12035 %person% HARD-STUNNED on 10
    else
      dg_affect #12035 %person% STUNNED on 5
    end
    %damage% %person% 125 physical
  end
  set person %person.next_in_room%
done
if %self.varexists(phase)%
  if %self.phase% == 3
    dg_affect #12040 %self% off
    %echo% %self.name% charges forward, foaming at the mouth with rage!
    dg_affect #12040 %self% HASTE on 30
    dg_affect #12040 %self% BONUS-PHYSICAL 50 30
    dg_affect #12040 %self% BONUS-MAGICAL 50 30
  end
end
~
#12036
Ba'al Hadad: Bull Charge~
0 k 33
~
if %self.cooldown(12030)%
  halt
end
nop %self.set_cooldown(12030, 30)%
if %self.varexists(phase)%
  if %self.phase% == 3
    set target %random.enemy%
  end
end
if !%target%
  set target %actor%
end
%send% %target% %self.name% lowers %self.hisher% head and charges you!
%echoaround% %target% %self.name% lowers %self.hisher% head and charges %target.name%!
wait 2 sec
if !%self.fighting%
  halt
end
if (%self.aff_flagged(ENTANGLED)% || %target.room% != %self.room%) && (%target% != %self.fighting%)
  %send% %target% Unable to reach you, %self.name% turns %self.hisher% attention to %self.fighting.name%!
  %send% %self.fighting% Unable to reach %target.name%, %self.name% turns %self.hisher% attention to you!
  %echoneither% %self.fighting% %target% Unable to reach %target.name%, %self.name% turns %self.hisher% attention to %self.fighting.name%!
  set target %self.fighting%
end
%send% %target% &r%self.name% crashes into you, sending you flying!
%echoaround% %target% %self.name% crashes into %target.name%, sending %target.himher% flying!
if %self.mob_flagged(GROUP)%
  %damage% %target% 400 physical
  dg_affect #12036 %target% HARD-STUNNED on 15
  dg_affect #12036 %target% DODGE -100 15
else
  %damage% %target% 200 physical
  dg_affect #12036 %target% STUNNED on 10
end
~
#12037
Ba'al Hadad: Thunderbolt~
0 k 50
~
if %self.cooldown(12030)%
  halt
end
nop %self.set_cooldown(12030, 30)%
set times 1
if %self.varexists(phase)%
  if %self.phase% == 3
    set times 2
  end
end
set time 1
while %time% <= %times%
  set target %random.enemy%
  if !%target%
    set target %actor%
  end
  %send% %target% %self.name% draws back an arm and takes aim at you...
  %echoaround% %self.name% draws back an arm and takes aim at %target.name%...
  wait 2 sec
  %send% %target% %self.name% hurls a thunderbolt at you!
  %echoaround% %target% %self.name% hurls a thunderbolt at %target.name%!
  %send% %target% &rThe thunderbolt explodes in your face!
  %echoaround% %target% The thunderbolt explodes in front of %target.name%'s face!
  %damage% %target% 100 magical
  %dot% #12037 %target% 200 30 magical
  if %self.mob_flagged(GROUP)%
    %send% %target% You are blinded by the brightness of the blast!
    dg_affect #12038 %target% BLIND on 15
  end
  eval time %time% + 1
  wait 3 sec
done
~
#12038
Ba'al Hadad: Raging Storm~
0 k 100
~
if %self.cooldown(12030)%
  halt
end
nop %self.set_cooldown(12030, 30)%
%echo% %self.name% raises %self.hisher% arms to the sky and lets out a mighty bellow!
%echo% A great storm forms overhead!
set cycle 1
set cycles 3
set scale 50
if %self.varexists(phase)%
  if %self.phase% == 3
    set scale 50
    set cycles 5
  end
end
while %cycle% <= %cycles%
  wait 5 sec
  %echo% &rLightning and hail rain down upon you!
  %aoe% 25 physical
  %aoe% %scale% magical
  eval cycle %cycle% + 1
done
~
#12039
Ba'al Hadad / Coming Storm: Bolt from the Blue~
0 k 100
~
if %self.cooldown(12030)%
  halt
end
nop %self.set_cooldown(12030, 20)%
if %self.affect(BLIND)%
  %echo% %self.name%'s vision clears!
  dg_affect %self% BLIND off 1
end
%echo% %self.name% starts casting a spell!
%echo% (Type 'interrupt'.)
set running 1
set interrupted 0
remote running %self.id%
remote interrupted %self.id%
wait 5 sec
if %self.interrupted%
  %echo% %self.name%'s spell is interrupted.
  unset running
  unset interrupted
  halt
end
unset running
unset interrupted
%echo% %self.name%'s spell goes off... but nothing seems to happen.
if %self.vnum% == 12030
  set ally %instance.mob(12031)%
else
  set ally %instance.mob(12030)%
end
if %ally%
  set target %ally.fighting%
  if %target%
    if %target.trigger_counterspell%
      %send% %target% A bolt of lightning flies out of nowhere and explodes against your counterspell!
      %send% %target% &rThe bolt's explosion burns you!
      %echoaround% %target% A bolt of lightning flies out of nowhere and explodes in front of %target.name%!
      %damage% %target% 75 magical
      halt
    else
      %send% %target% &rA bolt of lightning flies out of nowhere and strikes you!
      %echoaround% %target% A bolt of lightning flies out of nowhere and strikes %target.name%!
      if %self.vnum% == 12030
        %damage% %target% 200 magical
      else
        %damage% %target% 300 magical
      end
      dg_affect #12039 %target% HARD-STUNNED on 10
      dg_affect #12039 %target% BLIND on 10
      halt
    end
  end
end
* no ally, or no target
~
#12040
Hadad Start Progression: room~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12030)%
end
~
#12041
Ba'al Hadad / Coming Storm: Interrupt~
0 c 0
interrupt~
if !%self.varexists(running)%
  set running 0
  remote running %self.id%
end
if %actor.disabled%
  %send% %actor% You can't do that! You're stunned!
  halt
end
if %self.varexists(interrupted)%
  if %self.interrupted%
    %send% %actor% Someone beat you to it.
    halt
  end
end
if !%self.running%
  %send% %actor% There is nothing to interrupt right now.
  return 1
  halt
end
%send% %actor% You trip %self.name%.
%echoaround% %actor% %actor.name% trips %self.name%.
set interrupted 1
remote interrupted %self.id%
~
#12042
Hadad Start Progression: mob~
0 h 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12030)%
end
~
#12049
Call Storm~
1 c 3
use~
if !(%self.is_name(%arg%)% && %self.worn_by%) && !(%actor.obj_target(%arg%)% == %self% && %self.carried_by%)
  return 0
  halt
end
if %actor.cooldown(12049)%
  %send% %actor% Your %cooldown.12049% is on cooldown.
  halt
end
set room %actor.room%
set cycles_left 3
while %cycles_left% >= 0
  eval sector_valid (!%room.building% && !%room.template%) || (%room.building% && %room.bld_flagged(OPEN)%)
  if !%sector_valid% || %actor.room% != %room% || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing) || %actor.aff_flagged(DISTRACTED)%
    if %cycles_left% < 3
      %echoaround% %actor% %actor.name%'s stormcall is interrupted.
      %send% %actor% Your stormcall is interrupted.
    elseif !%sector_valid%
      %send% %actor% You can only use %self.shortdesc% outdoors.
    else
      %send% %actor% You can't do that now.
    end
    halt
  end
  switch %cycles_left%
    case 3
      %echoaround% %actor% %actor.name% raises the stormcaller wand high into the air...
      %send% %actor% You raise your stormcaller wand high into the air...
    break
    case 2
      %echoaround% %actor% A swirl of rain and wind shoots up from %actor.name%'s stormcaller wand...
      %send% %actor% A swirl of rain and wind shoots up from your stormcaller wand...
    break
    case 1
      %echoaround% %actor% %actor.name% shouts out the name of Ba'al Hadad, God of Storms!
      %send% %actor% You shout out the name of Ba'al Hadad, God of Storms!
    break
    case 0
      %echo% Lightning cascades across the darkening sky...
      %echoaround% %actor% %actor.name% falls to the ground as the stormcloud fills the sky and rain begins to fall!
      %send% %actor% You struggle to catch your breath as the stormcloud fills the sky and rain begins to fall!
      nop %actor.set_cooldown(12049, 300)%
      %load% obj 12050 %room%
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
#12050
Stormcloud weather~
1 c 4
weather~
%send% %actor% The rainfall is so heavy you can barely see your hand in front of your face.
~
#12051
Stormcloud echoes~
1 bw 10
~
switch %random.4%
  case 1
    %echo% Lightning crawls from cloud to cloud like spiderwebs.
  break
  case 2
    %echo% Rain drops like sheets from the clouds, until you can barely see where you're going.
  break
  case 3
    %echo% The sky flashes as a brilliant column of lightning splits it in two.
  break
  case 4
    %echo% Rolling thunder shakes you to your core.
  break
done
~
$
