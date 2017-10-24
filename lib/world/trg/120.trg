#12000
Purge hint on cleanup~
2 e 100
~
eval item %room.contents%
while %item%
  eval next_item %item.next_in_list%
  if %item.vnum% == 12000
    %purge% %item%
  end
  eval item %next_item%
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
  eval difficulty 2
elseif group /= %arg%
  eval difficulty 3
elseif boss /= %arg%
  eval difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure. (Hard, Group or Boss)
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
eval mob %self%
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
eval tokens 0
if %self.mob_flagged(GROUP)%
  eval tokens %tokens% + 1
  if %self.mob_flagged(HARD)%
    eval tokens %tokens% + 1
  end
end
if %tokens% == 0
  halt
end
eval room %self.room%
eval person %room.people%
while %person%
  if %person.is_pc%
    eval name %%currency.12000(%tokens%)%%
    %send% %person% As %self.name% is defeated, %self.hisher% essence spills forth, bestowing upon you %tokens% %name%!
    if %person.level% >= (%self.level% - 75)
      eval operation %%person.give_currency(12000, %tokens%)%%
      nop %operation%
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
  eval person %person.next_in_room%
done
~
#12003
Anat load event~
0 n 100
~
eval loc %instance.location%
if !%loc%
  halt
end
mgoto %loc%
if %loc.building_vnum% == 12001
  * bound here
  %self.add_mob_flag(SENTINEL)%
else
  * not bound
  %load% obj 12000
  mmove
  mmove
end
~
#12004
Old Gods loot load boe/bop~
1 n 100
~
eval actor %self.carried_by%
if !%actor%
  eval actor %self.worn_by%
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
Delayed despawn of Anat~
1 f 0
~
%adventurecomplete%
~
#12006
Anat death~
0 f 100
~
eval loc %instance.location%
if %loc%
  %at% %loc% %load% obj 12002
end
eval tokens 0
if %self.mob_flagged(GROUP)%
  eval tokens %tokens% + 1
  if %self.mob_flagged(HARD)%
    eval tokens %tokens% + 1
  end
end
if %tokens% == 0
  halt
end
eval room %self.room%
eval person %room.people%
while %person%
  if %person.is_pc%
    eval name %%currency.12000(%tokens%)%%
    %send% %person% As %self.name% is defeated, %self.hisher% essence spills forth, bestowing upon you %tokens% %name%!
    if %person.level% >= (%self.level% - 75)
      eval operation %%person.give_currency(12000, %tokens%)%%
      nop %operation%
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
  eval person %person.next_in_room%
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
eval room %self.room%
eval person %room.people%
while %person%
  if %person.vnum% == 12001
    eval yatpan %person%
  end
  eval person %person.next_in_room%
done
if !%self.cooldown(12005)% && !%yatpan%
  * Summon Yatpan
  shout Yatpan! Come to me!
  nop %self.set_cooldown(12005, 300)%
  wait 2 sec
  %load% mob 12001 ally
  eval summon %room.people%
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
  eval cycle 1
  while %cycle% <= 3
    wait 5 sec
    %echo% &r%self.name%'s arrows rain down upon you!
    %aoe% 250 physical
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
%echo% &r%self.name% charges forward, hacking and slicing at everyone!
%send% %actor% &rYou take the brunt of %self.name%'s assault!
%echoaround% %actor% %actor.name% takes the brunt of %self.name%'s assault!
%aoe% 50 physical
%damage% %actor% 300 physical
%dot% #12008 %actor% 250 30 physical
wait 5 sec
eval amount 1000
eval room %self.room%
eval person %room.people%
while %person%
  eval check %%person.is_enemy(%self%)%%
  if %check%
    %send% %person% &rA fountain of blood suddenly bursts from the wounds left by %self.name%'s assault!
    %damage% %person% 150
    eval amount %amount% + 250
  end
  eval person %person.next_in_room%
done
%echo% %self.name% bathes in the blood of %self.hisher% enemies, and %self.hisher% wounds close!
%damage% %self% -%amount%
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
dg_affect %self% STUNNED on 5
wait 2 sec
%send% %actor% &r%self.name% drives %self.hisher% spear through your chest, impaling you upon it!
%echoaround% %actor% %self.name% drives %self.hisher% spear through %actor.name%'s chest, impaling %actor.himher% upon it!
if %self.mob_flagged(GROUP)%
  %damage% %actor% 1000 physical
  dg_affect #12009 %actor% STUNNED on 15
  %dot% #12009 %actor% 500 15 physical
else
  %damage% %actor% 750 physical
  dg_affect #12009 %actor% STUNNED on 10
  %dot% #12009 %actor% 300 15 physical
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
dg_affect %self% STUNNED on 10
wait 1 sec
%echo% &Y%self.name% raises her hammer overhead... (Jump now!)
eval running 1
remote running %self.id%
wait 10 sec
eval running 0
remote running %self.id%
shout Hammer down!
%echo% &Y%self.name% slams %self.hisher% hammer into the earth with an deafening bang, shattering the ground underfoot!
eval room %self.room%
eval person %room.people%
while %person%
  if %person.is_pc%
    eval test %%self.varexists(jumped_%person.id%)%%
    if %test%
      eval test %%self.jumped_%person.id%%%
    end
    if !%test%
      %send% %person% &rThe force of the blow, traveling through the ground, leaves you stunned!
      %echoaround% %person% %person.name% is stunned!
      %damage% %person% 100 physical
      dg_affect #12010 %person% STUNNED on 20
    else
      %send% %person% You leap into the air, barely avoiding the shockwave.
      %echoaround% %person% %person.name% dodges the shockwave.
      unset jumped_%person.id%
    end
  end
  eval person %person.next_in_room%
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
eval room %self.room%
eval person %room.people%
while %person%
  if %person.vnum% == 12000
    eval anat %person%
  end
  eval person %person.next_in_room%
done
if !%anat%
  %echo% %self.name% gives a mournful screech, and vanishes in a flash of light!
  %purge% %self%
  halt
end
eval target %random.enemy%
if !%target%
  %echo% %self.name% looks very confused.
  halt
end
if %target.is_pc%
  eval target_string %target.firstname%
else
  eval name %target.name%
  eval target_string the %name.cdr%
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
  if !%target.aff_flagged(!STUN)%
    * Respect stun immunity resulting from Anat's stuns
    dg_affect #12013 %target% STUNNED on 10
  end
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
eval room %self.room%
eval person %room.people%
while %person%
  if %person.vnum% == 12000
    eval anat %person%
  end
  eval person %person.next_in_room%
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
$
