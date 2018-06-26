#12500
Board / climb colossus~
0 c 0
climb board enter up~
if !(climb /= %cmd%)
  eval helper %%actor.char_target(%arg%)%%
  if %helper% != %self%
    return 0
    halt
  end
end
if !%instance.start%
  %echo% %self.name% disappears! Or... was it ever there in the first place?
  %purge% %self%
  halt
end
eval room i12501
set start_room %actor.room%
%echoaround% %actor% %actor.name% starts climbing up %self.name%'s leg...
%teleport% %actor% %room%
%echoaround% %actor% %actor.name% climbs up from the ground below.
%send% %actor% You start climbing up %self.name%'s leg...
%force% %actor% look
eval room2 i12500
%at% %room2% %load% obj 12504
set person %start_room.people%
while %person%
  set next_person %person.next_in_room%
  if %person.is_npc% && %person.master% == %actor%
    %echoaround% %person% %person.name% follows %actor.name%.
    %teleport% %person% %actor.room%
    %echoneither% %person% %actor% %person.name% follows %actor.name%.
    %send% %actor% %person.name% follows you.
    %send% %person% You follow %actor.name%.
  end
  set person %next_person%
done
~
#12501
Disembark colossus~
2 c 0
leave exit disembark~
eval colossus %instance.mob(12500)%
if !%colossus%
  eval colossus %instance.mob(12501)%
end
set room %actor.room%
if %colossus%
  %send% %actor% You drop down from your perch on %colossus.name% to the ground below.
  %echoaround% %actor% %actor.name% drops down from %actor.hisher% perch on %colossus.name% to the ground below.
  %teleport% %actor% %colossus.room%
  %echoaround% %actor% %actor.name% drops down from a perch high up on %colossus.name%.
  %force% %actor% look
else
  %send% %actor% You drop down.
  %echoaround% %actor% %actor.name% drops down.
  %teleport% %actor% %instance.location%
  %echoaround% %actor% %actor% drops down out of nowhere.
  %force% %actor% look
end
set person %room.people%
while %person%
  set next_person %person.next_in_room%
  if %person.is_npc% && %person.master% == %actor%
    %echoaround% %person% %person.name% follows %actor.name%.
    %teleport% %person% %actor.room%
    %echoneither% %person% %actor% %person.name% follows %actor.name%.
    %send% %actor% %person.name% follows you.
    %send% %person% You follow %actor.name%.
  end
  set person %next_person%
done
~
#12502
Colossus eye lasers~
0 bw 15
~
eval mob %instance.mob(12500)%
if !%mob%
  halt
end
%echo% The colossus glowers at you and its eyes begin to glow! (dodge)
set running 1
remote running %self.id%
wait 5 sec
set running 0
remote running %self.id%
eval room %self.room%
eval person %room.people%
while %person%
  if %person.is_pc% && !%person.is_immortal%
    eval dodged %%self.varexists(dodged_%person.id%)%%
    if %dodged%
      eval dodged %%self.dodged_%person.id%%%
    end
    if !%person.aff_flagged(HIDE)%
      if %dodged%
        %send% %person% You scramble out of the way as the colossus fires its eye lasers!
        %echoaround% %person% %person.name% scrambles out of the way of the colossus's eye lasers!
      else
        %send% %person% &rThe colossus blasts you with its eye lasers!
        %echoaround% %person% &r%person.name% is blasted!
        %damage% %person% 200 magical
      end
    end
    rdelete dodged_%person.id% %self.id%
  end
  eval person %person.next_in_room%
done
~
#12503
Colossus eye lasers: dodge command~
0 c 0
dodge~
if !%self.varexists(running)%
  %send% %actor% There's nothing to dodge.
  halt
end
if !%self.running%
  %send% %actor% There's nothing to dodge.
  halt
end
%send% %actor% You prepare to dodge the eye lasers.
%echoaround% %actor% %actor.name% prepares to dodge the eye lasers.
set dodged_%actor.id% 1
remote dodged_%actor.id% %self.id%
~
#12504
CC Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12500)%
end
~
#12505
Colossus Fight: Lightning Punch (right arm)~
0 k 16
~
if %self.cooldown(12502)%
  halt
end
nop %self.set_cooldown(12502, 30)%
if !%instance.mob(12509)%
  %send% %actor% %self.name% flails at you with %self.hisher% damaged right arm!
  %echoaround% %actor% %self.name% flails at %actor.name% with %self.hisher% damaged right arm!
  %damage% %actor% 150 physical
else
  if %self.varexists(parts_destroyed)%
    set parts_destroyed %self.parts_destroyed%
  end
  if %parts_destroyed% < 2
    if %parts_destroyed% == 0
      * Tee hee
      nop %self.set_cooldown(12502, 15)%
    end
    %echo% %self.name% draws back %self.hisher% right arm, lightning flickering around %self.hisher% clenched fist!
    wait 3 sec
    set actor %self.fighting%
    %send% %actor% &r%self.name%'s lightning-charged punch smashes into you with a thunderous boom, sending you flying!
    %echoaround% %actor% %self.name%'s lightning-charged punch smashes into %actor.name% with a thunderous boom, sending %actor.himher% flying!
    %damage% %actor% 500 physical
    %damage% %actor% 350 magical
    dg_affect #12505 %actor% HARD-STUNNED on 15
    dg_affect #12505 %actor% DODGE -100 15
    dg_affect #12505 %actor% RESIST-PHYSICAL -50 15
    %echo% &rBlasts of lightning fly from the impact of %self.name%'s fist against the ground!
    %aoe% 150 magical
  else
    %echo% %self.name% draws back %self.hisher% right arm, lightning flickering around %self.hisher% clenched fist!
    wait 3 sec
    set actor %self.fighting%
    %send% %actor% &r%self.name%'s lightning-charged punch crashes into you, stunning you!
    %echoaround% %actor% %self.name%'s lightning-charged punch crashes into %actor.name%, stunning %actor.himher%!
    %damage% %actor% 150 physical
    %damage% %actor% 75 magical
    dg_affect #12505 %actor% STUNNED on 10
    dg_affect #12505 %actor% DODGE -50 10
  end
end
~
#12506
Colossus Fight: Retractable Blade (left arm)~
0 k 20
~
if %self.cooldown(12502)%
  halt
end
nop %self.set_cooldown(12502, 30)%
if !%instance.mob(12508)%
  %send% %actor% %self.name% flails at you with %self.hisher% damaged right arm!
  %echoaround% %actor% %self.name% flails at %actor.name% with %self.hisher% damaged left arm!
  %damage% %actor% 150 physical
else
  if %self.varexists(parts_destroyed)%
    set parts_destroyed %self.parts_destroyed%
  end
  if %parts_destroyed% < 2
    if %parts_destroyed% == 0
      * Tee hee
      nop %self.set_cooldown(12502, 15)%
    end
    %echo% %self.name% raises %self.hisher% left arm, and a massive blade extends from %self.hisher% clenched fist.
    wait 3 sec
    if %actor.is_pc%
      * PC
      %send% %actor% &r%self.name%'s slashes at you with %self.hisher% retractable blade, rending your armor and causing mortal injury!
      %echoaround% %actor% %self.name%'s slashes at %actor.name% with %self.hisher% retractable blade, rending %actor.hisher% armor and causing mortal injury!
      %damage% %actor% 900 physical
      %dot% #12506 %actor% 1000 15 physical 1
      dg_affect #12506 %actor% RESIST-PHYSICAL -50 15
    else
      * Tank familiar
      %send% %actor% &r%self.name% slashes at you with %self.hisher% retractable blade, decapitating you with a single strike!
      %send% %actor% %self.name% slashes at %actor.name% with %self.hisher% retractable blade, decapitating %actor.himher% with a single strike!
      %damage% %actor% 99999 physical
    end
    wait 1 sec
    %echo% %self.name%'s retractable blade slides back into its fist.
  else
    %echo% %self.name% raises %self.hisher% left arm, and a massive blade extends from %self.hisher% clenched fist.
    wait 3 sec
    %send% %actor% &r%self.name%'s slashes at you with %self.hisher% retractable blade, opening bleeding wounds!
    %echoaround% %actor% %self.name%'s slashes at %actor.name% with %self.hisher% retractable blade, opening bleeding wounds!
    %damage% %actor% 300 physical
    %dot% #12506 %actor% 200 15 physical 1
    wait 1 sec
    %echo% %self.name%'s retractable blade slides back into its fist.
  end
end
~
#12507
Colossus Fight: Rocket Barrage~
0 k 25
~
if %self.cooldown(12502)%
  halt
end
nop %self.set_cooldown(12502, 30)%
if %self.varexists(parts_destroyed)%
  set parts_destroyed %self.parts_destroyed%
end
if %parts_destroyed% < 2
  if %parts_destroyed% == 0
    * Tee hee
    nop %self.set_cooldown(12502, 15)%
  end
  %echo% %self.name%'s shoulder-mounted cannons fill the sky with dozens of fiery projectiles!
  set cycle 1
  while %cycle% <= 3
    wait 3 sec
    %echo% &r%self.name%'s missile barrage rains from the sky, exploding all around you!
    %aoe% 125 physical
    %aoe% 125 fire
    eval cycle %cycle% + 1
  done
else
  %echo% %self.name%'s shoulder-mounted cannons fire a barrage of fiery projectiles into the sky!
  wait 3 sec
  %echo% &r%self.name%'s missile barrage rains from the sky, exploding all around you!
  %aoe% 50 physical
  %aoe% 50 fire
end
~
#12508
Colossus Fight: Maintenance Spiders~
0 k 33
~
if %self.cooldown(12502)%
  halt
end
if %self.health% == %self.maxhealth%
  halt
end
nop %self.set_cooldown(12502, 30)%
if %self.varexists(parts_destroyed)%
  set parts_destroyed %self.parts_destroyed%
end
if %parts_destroyed% < 2
  if %parts_destroyed% == 0
    * Tee hee
    nop %self.set_cooldown(12502, 15)%
  end
  %echo% A small army of clockwork spiders swarms over %self.name%'s body, repairing it!
  %heal% %self% health 400
  eval magnitude (%self.level% - 50) * 5
  dg_affect #12508 %self% HEAL-OVER-TIME %magnitude% 30
else
  %echo% A handful of clockwork spiders scuttle out of %self.name%'s body and begin to repair it!
  eval magnitude (%self.level% - 50)
  if %self.vnum% == 12500
    eval magnitude %magnitude% * 2
  end
  dg_affect #12508 %self% HEAL-OVER-TIME %magnitude% 30
end
~
#12509
Colossus Fight: Leg Stomp~
0 k 50
~
if %self.cooldown(12502)%
  halt
end
nop %self.set_cooldown(12502, 30)%
if %self.varexists(parts_destroyed)%
  set parts_destroyed %self.parts_destroyed%
end
if %self.vnum% == 12501
  %echo% %self.name% draws back %self.hisher% leg for a kick!
  wait 3 sec
  %send% %actor% %self.name% kicks you hard, briefly stunning you!
  %echoaround% %actor% %self.name% kicks %actor.name%, who looks dazed.
  %damage% %actor% 200 physical
  dg_affect #12509 %actor% STUNNED on 5
else
  if %parts_destroyed% < 2
    if %parts_destroyed% == 0
      * Tee hee
      nop %self.set_cooldown(12502, 20)%
    end
    %echo% %self.name% raises one leg high in the air, pistons shifting as %self.heshe% gathers power...
    wait 3 sec
    set actor %self.fighting%
    %send% %actor% &r%self.name% brings %self.hisher% foot down on top of you with an earth-shaking crash!
    %echoaround% %actor% %self.name% brings %self.hisher% foot down on top of %actor.name% with an earth-shaking crash!
    %damage% %actor% 300 physical
    dg_affect #12509 %actor% HARD-STUNNED on 15
    dg_affect #12509 %actor% DODGE -100 15
    dg_affect #12509 %self% HARD-STUNNED on 5
    wait 3 sec
    dg_affect #12509 %self% off
    if %actor% != %self.fighting%
      halt
    end
    %echo% There is a mighty boom as %self.name% releases %self.hisher% gathered power into the ground beneath %self.hisher% foot!
    %send% %actor% &rThe force of %self.name%'s foot pressing down on you explosively redoubles, hammering you deeper into the ground!
    %damage% %actor% 600 physical
    set person %room.people%
    while %person%
      if (%person.is_enemy(%self%)% || %self.is_enemy(%person%)%) && %person% != %actor%
        %send% %person% &rThe force of %self.name%'s stomp knocks you off your feet!
        dg_affect #12509 %person% HARD-STUNNED on 5
        %damage% %person% 100 physical
      end
      set person %person.next_in_room%
    done
  else
    %echo% %self.name% raises one leg high in the air...
    wait 3 sec
    set actor %self.fighting%
    %send% %actor% &r%self.name% brings %self.hisher% foot down on top of you, pinning you to the ground!
    %echoaround% %actor% %self.name% brings %self.hisher% foot down on top of %actor.name%, pinning %actor.himher% to the ground!
    %damage% %actor% 300 physical
    dg_affect #12509 %actor% HARD-STUNNED on 10
    dg_affect #12509 %actor% DODGE -50 10
  end
end
~
#12510
Colossus Fight: Eye Lasers~
0 k 100
~
if %self.cooldown(12502)%
  halt
end
nop %self.set_cooldown(12502, 30)%
if %self.varexists(parts_destroyed)%
  set parts_destroyed %self.parts_destroyed%
end
set target %random.enemy%
* Only start if not blinded
if !%target%
  nop %self.set_cooldown(12502, 0)%
  halt
end
if %parts_destroyed% < 2
  if %parts_destroyed% == 0
    * Tee hee
    nop %self.set_cooldown(12502, 15)%
  end
  %send% %target% %self.name% glowers at you, and %self.hisher% eyes begin to glow red!
  %echoaround% %target% %self.name% glowers at %target.name%, and %self.hisher% eyes begin to glow red!
  wait 3 sec
  if !%target% || %target.room% != %self.room%
    set target %self.fighting%
  end
  %send% %target% &rThere is a blinding flash of light, and you feel unbearable heat and pain!
  %echoaround% %target% Beams of crimson energy fly from %self.name%'s eyes, engulfing %target.name% and setting %target.himher% ablaze!
  %damage% %target% 200 magical
  %damage% %target% 200 fire
  dg_affect #12511 %target% BLIND on 15
  %dot% #12511 %target% 600 15 fire
else
  %send% %target% %self.name% glowers at you, and %self.hisher% eyes begin to glow red!
  %echoaround% %target% %self.name% glowers at %target.name%, and %self.hisher% eyes begin to glow red!
  wait 3 sec
  if !%target% || %target.room% != %self.room%
    set target %self.fighting%
  end
  %send% %target% &rBeams of crimson energy fly from %self.name%'s eyes, cutting into you!
  %echoaround% %target% Beams of crimson energy fly from %self.name%'s eyes, cutting into %target.name%!
  %damage% %target% 75 magical
  %damage% %target% 75 fire
end
~
#12513
Colossus mob block higher room~
0 s 100
~
eval room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
if %actor.nohassle% || !%tricky% || %direction% == down || %actor.aff_flagged(SNEAK)%
  halt
end
%send% %actor% You can't climb anywhere while %self.name% is here!
return 0
~
#12514
Colossus trash spawner~
1 n 100
~
eval vnum 12501+%random.2%
%load% mob %vnum%
eval room %self.room%
eval mob %room.people%
%echo% %mob.name% arrives.
%purge% %self%
~
#12515
Adventurer quest start~
0 bw 10
~
if %self.fighting%
  halt
end
say You know, I could help you out here... for a price.
eval room %self.room%
eval person %room.people%
while %person%
  if %person.is_pc%
    %quest% %person% start 12504
  end
  eval person %person.next_in_room%
done
~
#12516
Colossus load~
0 n 100
~
if %self.room.template% == 12500
  mgoto %instance.real_location%
  %echo% %self.name% appears.
  set first_chant %random.4%
  set second_chant %random.3%
  if %second_chant% == %first_chant%
    set second_chant 4
  end
  remote first_chant %self.id%
  remote second_chant %self.id%
end
if !%self.affect(12501)%
  * these buffs are removed when a component is damaged
  dg_affect #12501 %self% TO-HIT 500 -1
end
~
#12517
Colossus location updater + leash~
0 i 100
~
eval room %self.room%
if %room.distance(%instance.real_location%)% > 20
  %echo% %self.name% hastily backtracks.
  return 0
  wait 1
  %echo% %self.name% hastily backtracks.
  halt
end
nop %instance.set_location(%room%)%
if %self.vnum% == 12500
  %regionecho% %room% -7 The footfalls of %self.name% shake the earth as %self.heshe% moves to %room.coords%.
end
~
#12518
wandering mob adventure command~
0 c 0
adventure~
if !(summon /= %arg.car%)
  %teleport% %actor% %instance.real_location%
  %force% %actor% adventure
  %teleport% %actor% %self%
  return 1
  halt
end
return 0
~
#12519
Colossus look out~
2 c 0
look~
if %cmd.mudcommand% == look && out == %arg%
  %send% %actor% Looking down from your perch on the colossus, you see...
  eval colossus %instance.mob(12500)%
  if !%colossus%
    eval colossus %instance.mob(12501)%
  end
  if %colossus%
    eval target %colossus.room%
  else
    eval target %startloc%
    %adventurecomplete%
  end
  %teleport% %actor% %target%
  %force% %actor% look
  %teleport% %actor% %room%
  return 1
  halt
end
return 0
~
#12520
Diagnose colossus~
0 c 0
diagnose~
eval helper %%actor.char_target(%arg%)%%
if %helper% != %self%
  return 0
  halt
end
if %self.varexists(parts_destroyed)%
  if %parts_destroyed% == 1
    %send% %actor% 1 of %self.name%'s major components has been destroyed.
  else
    %send% %actor% %self.parts_destroyed% of %self.name%'s major components have been destroyed.
  end
  if !%instance.mob(12508)%
    %send% %actor% %self.name%'s left arm has been badly damaged.
  end
  if !%instance.mob(12509)%
    %send% %actor% %self.name%'s right arm has been badly damaged.
  end
else
  %send% %actor% All of %self.name%'s major components are intact.
end
~
#12521
Colossus death~
0 f 100
~
set tokens 0
if !%self.varexists(parts_destroyed)%
  set tokens 6
elseif %self.parts_destroyed% == 1
  set tokens 5
elseif (%self.mob_flagged(GROUP)%
  set tokens 4
else
  set tokens 2
end
if %tokens% > 0
  eval room %self.room%
  eval person %room.people%
  while %person%
    if %person.is_pc%
      eval name %%currency.12500(%tokens%)%%
      %send% %person% You loot %tokens% %name% from %self.name%.
      eval op %%person.give_currency(12500, %tokens%)%%
      nop %op%
    end
    eval person %person.next_in_room%
  done
end
%load% mob 12505
set dealer %self.room.people%
%echo% %dealer.name% arrives!
* Set adventure location back to scaffold
nop %instance.set_location(%instance.real_location%)%
* Load despawn timer
eval room2 i12500
%at% %room2% %load% obj 12504
~
#12522
free cannonballs~
1 n 100
~
eval actor %self.carried_by%
if %actor%
  %load% obj 12508 %actor% inv
  %load% obj 12508 %actor% inv
else
  %load% obj 12508
  %load% obj 12508
end
if %actor%
  if %actor.mob_flagged(HARD)% && !%self.is_flagged(HARD-DROP)%
    nop %self.flag(HARD-DROP)%
  end
  if %actor.mob_flagged(GROUP)% && !%self.is_flagged(GROUP-DROP)%
    nop %self.flag(GROUP-DROP)%
  end
end
if %actor% && %actor.is_npc%
  wait 0
  %scale% %self% %self.level%
end
~
#12523
Delayed Completer~
1 f 0
~
%adventurecomplete%
~
#12524
ankle brace completion~
5 n 100
~
if !%instance.real_location%
  %echo% %self.name% vanishes uselessly.
  %purge% %self%
end
eval colossus %instance.mob(12500)%
%echo% %colossus.name% slowly staggers back to its scaffold.
if %colossus%
  %echoaround% %colossus% %colossus.name% staggers slowly back to its scaffold.
  %teleport% %colossus% %instance.real_location%
  nop %colossus.add_mob_flag(SENTINEL)%
  nop %instance.set_location(%instance.real_location%)%
  %echoaround% %colossus% %colossus.name% staggers slowly up.
end
%purge% %self%
~
#12525
Colossus eye lasers: greet~
0 h 100
~
if %self.varexists(running)%
  if %self.running%
    set dodged_%actor.id% 1
    remote dodged_%actor.id% %self.id%
  end
end
~
#12526
drop Hire/bribe quest on adv death~
0 f 100
~
set person %self.room.people%
while %person%
  if %person.is_pc%
    if %person.on_quest(12504)%
      %quest% %person% drop 12504
    end
  end
  set person %person.next_in_room%
done
~
#12527
Adventurer bribe~
0 tv 0
~
if %self.aff_flagged(*CHARM)%
  %send% %actor% Someone else has already bribed %self.name%.
  return 0
  halt
end
if %questvnum% == 12504
  return 1
  mfollow %actor%
  dg_affect %self% *CHARM on -1
  nop %self.add_mob_flag(SPAWNED)%
  %morph% %self% normal
end
~
#12528
Colossus arm combat~
0 k 100
~
if %self.cooldown(12502)%
  halt
end
nop %self.set_cooldown(12502, 30)%
%echo% The colossus starts waving its arm in the air in an attempt to shake you off! (cling)
set running 1
remote running %self.id%
wait 5 sec
set running 0
remote running %self.id%
eval room %self.room%
eval person %room.people%
while %person%
  eval next_person %person.next_in_room%
  if %person.is_pc%
    eval success %%self.varexists(cling_%person.id%)%%
    if %success%
      eval success %%self.cling_%person.id%%%
    end
    if %success%
      %send% %person% You manage to stay on the colossus's wildly flailing arm!
      %echoaround% %person% %person.name% manages to cling on.
    else
      %send% %person% The colossus's wildly flailing arm sends you flying!
      %echoaround% %person% %person.name% goes flying!
      eval colossus %instance.mob(12500)%
      if %colossus%
        rdelete cling_%person.id% %self.id%
        %teleport% %person% %colossus.room%
        %force% %person% look
        %echoaround% %person% %person.name% falls off %colossus.name%'s arm and comes crashing down!
        %send% %person% &rYou crash into the ground!
        %damage% %person% 100 physical
      else
        %teleport% %person% %instance.location%
        %force% %person% look
        %echoaround% %person% %person.name% falls off %colossus.name%'s arm and comes crashing down!
        %send% %person% &rYou crash into the ground!
        %damage% %person% 100 physical
      end
    end
  end
  eval person %next_person%
done
~
#12529
Colossus arm cling~
0 c 0
cling~
if !%self.varexists(running)%
  %send% %actor% You don't need to cling on particularly tightly right now.
  halt
end
if !%self.running%
  %send% %actor% You don't need to cling on particularly tightly right now.
  halt
end
%send% %actor% You cling on for dear life!
%echoaround% %actor% %actor.name% desperately clings on!
set cling_%actor.id% 1
remote cling_%actor.id% %self.id%
dg_affect %actor% HARD-STUNNED on 5
~
#12530
Clockwork Colossus loot bop/boe~
1 n 100
~
eval actor %self.carried_by%
if !%actor%
  eval actor %self.worn_by%
end
if !%actor%
  halt
end
if %actor% && %actor.is_npc%
  if %actor.mob_flagged(HARD)% && !%self.is_flagged(HARD-DROP)%
    nop %self.flag(HARD-DROP)%
  end
  if %actor.mob_flagged(GROUP)% && !%self.is_flagged(GROUP-DROP)%
    nop %self.flag(GROUP-DROP)%
  end
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
if %actor% && %actor.is_npc%
  wait 0
  %scale% %self% %self.level%
end
~
#12531
Colossus arm death~
0 f 100
~
%echo% The destruction of %self.name% weakens the colossus!
* Start of script fragment: colossus damage updater
eval colossus %instance.mob(12500)%
if %colossus%
  dg_affect #12501 %colossus% off
  if !%colossus.varexists(parts_destroyed)%
    set parts_destroyed 1
    remote parts_destroyed %colossus.id%
  else
    eval parts_destroyed %colossus.parts_destroyed% + 1
    remote parts_destroyed %colossus.id%
  end
  switch %parts_destroyed%
    case 3
      nop %colossus.add_mob_flag(GROUP)%
      nop %colossus.remove_mob_flag(HARD)%
    break
    case 4
      nop %colossus.remove_mob_flag(GROUP)%
      nop %colossus.add_mob_flag(HARD)%
    break
  done
  %scale% %colossus% %colossus.level%
end
* End of script fragment
~
#12532
Colossus Master Controller dance~
0 bw 100
~
if !%self.varexists(active)%
  halt
end
if %self.varexists(cycle)%
  set cycle %self.cycle%
else
  set cycle 1
end
if %cycle% >= 10
  %echo% An explosion rocks %self.name%!
  %damage% %self% 99999 direct
  halt
end
* choose a 'correct action'
set color_1 red
set color_2 green
set color_3 blue
set pattern %random.3%
eval chosen_color %%color_%pattern%%%
eval simon_says %random.3% - 1
if %simon_says%
  set message Controller commands: Pull the %chosen_color% lever!
  set correct_lever %chosen_color%
else
  set message Pull the %chosen_color% lever!
end
say %message%
set running 1
remote running %self.id%
wait 10 sec
set running 0
remote running %self.id%
set room %self.room%
set person %room.people%
set found_person 0
if !%correct_lever%
  set success 1
end
while %person%
  if %person.is_pc%
    set found_person 1
    eval act %%self.varexists(action_%person.id%)%%
    if %act%
      eval act %%self.action_%person.id%%%
    end
    if %act%
      if !%correct_lever%
        set failure 1
      else
        if %act% == %correct_lever%
          set success 1
        else
          set failure 1
        end
      end
    end
    rdelete action_%person.id% %self.id%
  end
  set person %person.next_in_room%
done
if !%found_person%
  halt
end
if %success% && !%failure%
  emote A red light on the side of %self.name% goes green.
  eval cycle %cycle% + 1
  remote cycle %self.id%
  if %cycle% >= 10
    say Access granted. Disengaging embiggening circuit...
    dg_affect %self% !ATTACK off
  end
else
  if %cycle% > 0
    emote A green light on the side of %self.name% goes red.
    eval cycle %cycle% - 1
    remote cycle %self.id%
  end
  %echo% &r%self.name% releases a blast of lightning!
  set person %room.people%
  while %person%
    if %person.is_pc%
      %damage% %person% 100 magical
    end
    set person %person.next_in_room%
  done
end
~
#12533
Colossus Master Controller dance commands~
0 c 0
pull sabotage~
if sabotage /= %cmd%
  if %self.varexists(active)%
    %send% %actor% It's already being sabotaged.
    halt
  end
  set colossus %instance.mob(12500)%
  if !%colossus%
    %send% %actor% Sabotage check failed: missing colossus. Please report this using the 'bug' command.
    halt
  end
  if !%colossus.varexists(parts_destroyed)%
    set parts_destroyed 0
  else
    set parts_destroyed %colossus.parts_destroyed%
  end
  if %parts_destroyed% < 4
    eval remaining 4 - %parts_destroyed%
    set plural components
    if %remaining% == 1
      set plural component
    end
    %send% %actor% You still need to destroy %remaining% more vital %plural% before you can sabotage the master controller.
    halt
  end
  say User not authenticated. Prepare for authentication sequence. Do as the controller commands!
  set active 1
  remote active %self.id%
  halt
end
if !%self.varexists(running)%
  %send% %actor% You don't need to do that right now.
  halt
end
if !%self.running%
  %send% %actor% You don't need to do that right now.
  halt
end
if pull /= %cmd%
  if red /= %arg%
    set command red
  elseif green /= %arg%
    set command green
  elseif blue /= %arg%
    set command blue
  else
    %send% %actor% You can't find that lever. (Try 'red', 'green' or 'blue')
  end
end
if %command%
  if %self.varexists(action_%actor.id%)%
    %send% %actor% You just pulled a lever. Pulling more levers now isn't going to help!
    halt
  end
  %send% %actor% You pull the %command% lever.
  %echoaround% %actor% %actor.name% pulls the %command% lever.
  set action_%actor.id% %command%
  remote action_%actor.id% %self.id%
end
~
#12534
Colossus Master Controller load~
0 nx 100
~
dg_affect %self% !ATTACK on -1
~
#12535
Colossus Master Controller death~
0 f 100
~
set colossus %instance.mob(12500)%
if !%colossus%
  %echo% Something is wrong and the colossus is missing
  halt
end
set target_room %colossus.room%
%at% %target_room% %load% mob 12501
set parts_destroyed 5
set new_colossus %target_room.people%
remote parts_destroyed %new_colossus.id%
%teleport% adventure %target_room%
%at% %target_room% %echo% Everyone who was climbing on the colossus falls off!
%purge% %colossus% $n is rocked by an explosion, and suddenly shrinks!
~
#12536
Adventurer quest start act~
0 e 0
you~
if %self.fighting%
  halt
end
wait 5
say You know, I could help you out here... for a price.
eval room %self.room%
eval person %room.people%
while %person%
  if %person.is_pc%
    %quest% %person% start 12504
  end
  eval person %person.next_in_room%
done
~
#12537
Master Controller Greeting~
0 h 100
~
wait 1
* Detect colossus
set colossus %instance.mob(12500)%
if !%colossus%
  halt
end
if !%colossus.varexists(parts_destroyed)%
  set parts_destroyed 0
else
  set parts_destroyed %colossus.parts_destroyed%
end
* Loop over messages
set cycle 0
while %cycle% < 6 && !%self.varexists(active)%
  switch %cycle%
    case 0
      say Intruders! Intruders!
    break
    case 1
      if (%parts_destroyed% < 4)
        %echo% Lights flash all over the master controller apparatus.
      else
        say How did you get in here, tiny adventurers?
      end
    break
    case 2
      if (%parts_destroyed% < 4)
        say No matter, you'll never get into my controls! I have too much power!
        halt
      else
        say No matter, you'll never... error...
      end
    break
    case 3
      %echo% Lights flash all over the master controller apparatus, turning from green to red.
    break
    case 4
      say Error... power levels are dangerously low.
    break
    case 5
      %echo% With most of the colossus disabled, its master controller apparatus looks ripe for sabotage.
    break
  done
  wait 2 sec
  eval cycle %cycle% + 1
done
~
#12538
Other adventurer load~
0 nx 100
~
if %self.room.template% == 12504
  %morph% %self% 12504
end
~
#12547
Clockwork Colossus super-loot~
1 n 100
~
eval clothes 12519
eval vehicle 12513
eval air_mount 12512
eval actor %self.carried_by%
eval percent_roll %random.100%
if %percent_roll% <= 10
  eval vnum %vehicle%
elseif %percent_roll% <= 40
  eval vnum %air_mount%
else
  eval vnum %clothes%
end
if %self.level%
  eval level %self.level%
else
  eval level 100
end
%load% obj %vnum% %actor% inv %level%
eval item %%actor.inventory(%vnum%)%%
if %item.is_flagged(BOP)%
  eval bind %%item.bind(%self%)%%
  nop %bind%
end
* %send% %actor% %self.shortdesc% turns out to be %item.shortdesc%!
wait 1
%purge% %self%
~
#12548
Clockwork Colossus premium loot~
1 n 100
~
eval actor %self.carried_by%
eval percent_roll %random.10000%
if %percent_roll% <= 666
  * bag
  eval vnum 12515
elseif %percent_roll% <= 1332
  * saddle
  eval vnum 12516
elseif %percent_roll% <= 1998
  * shoes
  eval vnum 12517
elseif %percent_roll% <= 2664
  * gun
  eval vnum 12518
elseif %percent_roll% <= 3330
  * pet
  eval vnum 12510
elseif %percent_roll% <= 3996
  * land mount
  eval vnum 12511
else
  * all other vnum: BoEs
  eval vnum 12519 + %random.9%
end
if %self.level%
  eval level %self.level%
else
  eval level 100
end
%load% obj %vnum% %actor% inv %level%
eval item %%actor.inventory(%vnum%)%%
if %item.is_flagged(BOP)%
  eval bind %%item.bind(%self%)%%
  nop %bind%
end
* %send% %actor% %self.shortdesc% turns out to be %item.shortdesc%!
wait 1
%purge% %self%
~
#12549
Clockwork Colossus BoP loot~
1 n 100
~
eval actor %self.carried_by%
if %self.level%
  eval level %self.level%
else
  eval level 100
end
eval vnum 12527 + (%random.9% * 2)
%load% obj %vnum% %actor% inv %level%
eval item %%actor.inventory(%vnum%)%%
if %item.is_flagged(BOP)%
  eval bind %%item.bind(%self%)%%
  nop %bind%
end
* %send% %actor% %self.shortdesc% turns out to be %item.shortdesc%!
wait 1
%purge% %self%
~
#12550
Clockwork Colossus Normal Difficulty Loot~
1 n 100
~
eval actor %self.carried_by%
if %self.level%
  eval level %self.level%
else
  eval level 100
end
if %random.2% == 2
  eval vnum 12519 + %random.9%
else
  eval vnum 12527 + (%random.9% * 2)
end
%load% obj %vnum% %actor% inv %level%
eval item %%actor.inventory(%vnum%)%%
if %item.is_flagged(BOP)%
  eval bind %%item.bind(%self%)%%
  nop %bind%
end
* %send% %actor% %self.shortdesc% turns out to be %item.shortdesc%!
wait 1
%purge% %self%
~
#12551
Clockwork Colossus Master Loot Controller~
1 n 100
~
* loot vars to be 0/1
eval normal 0
eval basic 0
eval premium 0
eval super 0
* scraps var is number to load
eval scraps 0
eval actor %self.carried_by%
if %self.level%
  eval level %self.level%
else
  eval level 100
end
if %actor%
  if 0
    * TODO superboss?
  elseif %actor.mob_flagged(GROUP)% && %actor.mob_flagged(HARD)%
    eval scraps 4
    eval basic 1
    eval premium 1
    if %random.100% <= 25
      eval super 1
    end
  elseif %actor.mob_flagged(GROUP)%
    eval scraps 3
    eval basic 1
    if %random.100% <= 67
      eval premium 1
    end
  elseif %actor.mob_flagged(HARD)%
    eval scraps 2
    eval basic 1
    if %random.100% <= 33
      eval premium 1
    end
  else
    * Normal
    eval scraps 1
    eval normal 1
  end
end
* load items
if %normal%
  %load% obj 12550 %actor% inv %level%
  eval item %%actor.inventory(12550)%%
end
if %basic%
  %load% obj 12549 %actor% inv %level%
  eval item %%actor.inventory(12549)%%
end
if %premium%
  %load% obj 12548 %actor% inv %level%
  eval item %%actor.inventory(12548)%%
end
if %super%
  %load% obj 12547 %actor% inv %level%
  eval item %%actor.inventory(12547)%%
end
* give scraps
while %scraps% > 0
  %load% obj 12506 %actor% inv %level%
  %load% obj 12507 %actor% inv %level%
  eval scraps %scraps% - 1
done
wait 1
%purge% %self%
~
#12552
Open belt panel~
1 c 4
open pry~
eval target %%actor.obj_target(%arg%)%%
if %target% != %self%
  return 0
  halt
end
%send% %actor% You pry %self.shortdesc% open, revealing a compartment filled with wires...
%echoaround% %actor% %actor.name% pries %self.shortdesc% open, revealing a compartment filled with wires...
eval first_panel 12552 + %random.6%
%load% obj %first_panel%
%force% %actor% look panel
%purge% %self%
~
#12553
Belt wires puzzle~
1 c 4
cut~
if !%arg%
  %send% %actor% Cut which wire?
  halt
end
if red /= %arg%
  set color red
elseif green /= %arg%
  set color green
elseif blue /= %arg%
  set color blue
else
  %send% %actor% You can't find that wire.
  halt
end
switch %self.vnum%
  case 12553
    set correct_color red
  break
  case 12554
    set correct_color green
  break
  case 12555
    set correct_color blue
  break
  case 12556
    set correct_color red
  break
  case 12557
    set correct_color green
  break
  case 12558
    set correct_color blue
  break
done
if %color% and %correct_color%
  %send% %actor% You cut the %color% wire...
  %echoaround% %actor% %actor.name% cuts the %color% wire...
  if %color% == %correct_color%
    if %self.val0% >= 3
      %echo% The final panel slides closed, and the colossus is rocked by an internal explosion!
      %load% obj 12559
      * Start of script fragment: colossus damage updater
      eval colossus %instance.mob(12500)%
      if %colossus%
        dg_affect #12501 %colossus% off
        if !%colossus.varexists(parts_destroyed)%
          set parts_destroyed 1
          remote parts_destroyed %colossus.id%
        else
          eval parts_destroyed %colossus.parts_destroyed% + 1
          remote parts_destroyed %colossus.id%
        end
        switch %parts_destroyed%
          case 3
            nop %colossus.add_mob_flag(GROUP)%
            nop %colossus.remove_mob_flag(HARD)%
          break
          case 4
            nop %colossus.remove_mob_flag(GROUP)%
            nop %colossus.add_mob_flag(HARD)%
          break
          * 5 is only when the master controller breaks
        done
        %scale% %colossus% %colossus.level%
      end
      * End of script fragment
      %purge% %self%
    else
      eval next_panel_vnum 12552 + %random.5%
      if %next_panel_vnum% == %self.vnum%
        set next_panel_vnum 12558
      end
      %load% obj %next_panel_vnum%
      eval room %self.room%
      eval panel %room.contents%
      if %panel.vnum% == %next_panel_vnum%
        eval next_val %self.val0% + 1
        eval op %%panel.val0(%next_val%)%%
        nop %op%
        %echo% The open panel slides closed, and another panel opens nearby!
        %force% %actor% look panel
        %purge% %self%
      end
    end
  else
    %send% %actor% &rThe open panel blasts you with lightning!
    %echoaround% %actor% The open panel blasts %actor.name% with lightning!
    %damage% %actor% 300 magical
    %send% %actor% Your spasming hands lose purchase on the colossus!
    %echoaround% %actor% %actor.name% falls off the colossus, twitching and spasming!
    eval colossus %instance.mob(12500)%
    if %colossus%
      eval room %colossus.room%
      %teleport% %actor% %room%
      %echoaround% %actor% %actor.name% falls off the colossus!
      %force% %actor% look
    else
      %teleport% %actor% %instance.location%
      %force% %actor% look
      %echoaround% %actor% %actor.name% falls out of nowhere.
    end
  end
end
~
#12555
Colossus book of chants command~
1 c 2
chant~
if binding /= %arg%
  set chant_num 1
elseif rust /= %arg%
  set chant_num 2
elseif lightning /= %arg%
  set chant_num 3
elseif rats /= %arg%
  set chant_num 4
end
if !%chant_num% || %actor.position% != Standing
  return 0
  halt
end
switch %chant_num%
  case 1
    set message_1_self You chant, 'Binding roots, climb the giant, slither up his leg...'
    set message_1_other %actor.name% chants, 'Binding roots, climb the giant, slither up his leg...'
    set message_2_self You chant, 'Binding roots, grow more quickly, don't make goblins beg!'
    set message_2_other %actor.name% chants, 'Binding roots, grow more quickly, don't make goblins beg!'
    set message_3_self You chant, 'Binding roots, creepy crawly, cling to every peg...'
    set message_3_other %actor.name% chants, 'Binding roots, creepy crawly, cling to every peg...'
    set message_4_self You chant, 'Binding roots, weave together, drop him like an egg!'
    set message_4_other %actor.name% chants, 'Binding roots, weave together, drop him like an egg!'
    set message_5_self Long, slender vines slither up the leg of the colossus and root themselves in its gears.
    set message_5_other Long, slender vines slither up the leg of the colossus and root themselves in its gears.
  break
  case 2
    set message_1_self You chant, 'Mighty Orka, Sharky Godlin, lord of ocean sky...'
    set message_1_other %actor.name% chants, 'Mighty Orka, Sharky Godlin, lord of ocean sky...'
    set message_2_self You chant, 'Loose your clouds and shed your waters, rain them on this guy!'
    set message_2_other %actor.name% chants, 'Loose your clouds and shed your waters, rain them on this guy!'
    set message_3_self You chant, 'Strike with Kjelpnir, Spear of Water, roil seas you must...'
    set message_3_other %actor.name% chants, 'Strike with Kjelpnir, Spear of Water, roil seas you must...'
    set message_4_self You chant, 'Bring the waters down upon him, turn this guy to rust!'
    set message_4_other %actor.name% chants, 'Bring the waters down upon him, turn this guy to rust!'
    set message_5_self You hear the burbling sound of water flowing within the colossus, followed by the screeching of worn gears.
    set message_5_other You hear the burbling sound of water flowing within the colossus, followed by the screeching of worn gears.
  break
  case 3
    set message_1_self You chant, 'Clear sky, make it cloudy, goblins going to try...'
    set message_1_other %actor.name% chants, 'Clear sky, make it cloudy, goblins going to try...'
    set message_2_self You chant, 'White clouds, getting darker, blacking out the sky...'
    set message_2_other %actor.name% chants, 'White clouds, getting darker, blacking out the sky...'
    set message_3_self You chant, 'Gray clouds, light and sparkly, goblins make you fry...'
    set message_3_other %actor.name% chants, 'Gray clouds, light and sparkly, goblins make you fry...'
    set message_4_self You chant, 'Spears of lightning, striking strongly, forking up this guy!'
    set message_4_other %actor.name% chants, 'Spears of lightning, striking strongly, forking up this guy!'
    set message_5_self From the sky, a bolt of lightning strikes the colossus and rattles through its metal frame!
    set message_5_other From the sky, a bolt of lightning strikes the colossus and rattles through its metal frame!
  break
  case 4
    set message_1_self You chant, 'Rats from far and rats from wide, goblin calls you to his side!'
    set message_1_other %actor.name% chants, 'Rats from far and rats from wide, goblin calls you to his side!'
    set message_2_self You chant, 'Ratty friends, great and tiny, bring yourselves upon this shiny!'
    set message_2_other %actor.name% chants, 'Ratty friends, great and tiny, bring yourselves upon this shiny!'
    set message_3_self You chant, 'Climb the legs and bite the wires, even if it starts some fires!'
    set message_3_other %actor.name% chants, 'Climb the legs and bite the wires, even if it starts some fires!'
    set message_4_self You chant, 'Little rat friends, so compliant, help the goblins stop this giant!'
    set message_4_other %actor.name% chants, 'Little rat friends, so compliant, help the goblins stop this giant!'
    set message_5_self From out of nowhere, a horde of rats scurry up the colossus and into every open panel. You hear shrieks and sparks from inside!
    set message_5_other From out of nowhere, a horde of rats scurry up the colossus and into every open panel. You hear shrieks and sparks from inside!
  break
done
set room %actor.room%
set cycles 4
set cycles_left 4
while %cycles_left% >= 0
  if (%actor.room% != %room%) || %actor.fighting% || %actor.disabled% || (%actor.position% != Standing)
    * We've either moved or the room's no longer suitable for the chant
    if %cycles_left% < 4
      %echoaround% %actor% %actor.name%'s chant is interrupted.
      %send% %actor% Your chant is interrupted.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  eval message_num (%cycles% - %cycles_left%) + 1
  if %cycles_left% == 0
    set colossus %instance.mob(12500)%
    if %room.template% < 12500 || %room.template% > 12599 || !%colossus%
      %echo% Nothing seems to happen.
      halt
    end
  end
  eval cycles_left %cycles_left% - 1
  eval message_self %%message_%message_num%_self%%
  eval message_other %%message_%message_num%_other%%
  if %cycles_left% >= 0
    %send% %actor% %message_self%
    %echoaround% %actor% %message_other%
    wait 5 sec
  end
done
if %colossus.varexists(first_chant)%
  set first_chant %colossus.first_chant%
end
if %colossus.varexists(second_chant)%
  set second_chant %colossus.second_chant%
end
if %colossus.varexists(last_chant)%
  set last_chant %colossus.last_chant%
end
if !%first_chant%
  * colossus already damaged
  halt
end
* quest complete?
if %first_chant% == %last_chant% && %second_chant% == %chant_num%
  %send% %actor% %message_self%
  %echoaround% %actor% %message_other%
  rdelete first_chant %colossus.id%
  rdelete second_chant %colossus.id%
  rdelete last_chant %colossus.id%
  %buildingecho% %instance.start% An explosion rocks %colossus.name%!
  %at% %colossus.room% %echo% An explosion rocks %colossus.name%!
  * Start of script fragment: colossus damage updater
  eval colossus %instance.mob(12500)%
  if %colossus%
    dg_affect #12501 %colossus% off
    if !%colossus.varexists(parts_destroyed)%
      set parts_destroyed 1
    else
      eval parts_destroyed %colossus.parts_destroyed% + 1
    end
    remote parts_destroyed %colossus.id%
    switch %parts_destroyed%
      case 3
        nop %colossus.add_mob_flag(GROUP)%
        nop %colossus.remove_mob_flag(HARD)%
      break
      case 4
        nop %colossus.remove_mob_flag(GROUP)%
        nop %colossus.add_mob_flag(HARD)%
      break
    done
    %scale% %colossus% %colossus.level%
  end
  * End of script fragment
else
  if %chant_num% == %first_chant%
    %send% %actor% %message_self%
    %echoaround% %actor% %message_other%
  else
    %echo% Nothing seems to happen.
  end
  set last_chant %chant_num%
  remote last_chant %colossus.id%
end
~
#12556
Scrawled words -- colossus leg chant hint~
1 c 4
look~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set chant_1 the binding chant
set chant_2 the rust chant
set chant_3 the lightning chant
set chant_4 the chant of rats
set colossus %instance.mob(12500)%
if !%colossus%
  %send% %actor% On closer inspection, this scrawl is gibberish.
  halt
end
if %colossus.varexists(first_chant)%
  %send% %actor% The scrawled words read:
  eval chant %%chant_%colossus.first_chant%%%
  %send% %actor% 'First, perform %chant%.
  eval chant %%chant_%colossus.second_chant%%%
  %send% %actor% Then, perform %chant%.'
  %send% %actor% Underneath is scrawled, 'Chant book is hidden behind a gear'. (Try 'search'.)
else
  %send% %actor% The words have been rendered illegible by damage to the colossus.
end
~
#12557
Colossus: Search leg alcove for book~
2 c 0
search~
%load% obj 12560 %actor% inv
set obj %actor.inventory(12560)%
%send% %actor% Searching the room, you find %obj.shortdesc% stuffed behind a broken gear.
%echoaround% %actor% %actor.name% searches the room and finds %obj.shortdesc%.
detach 12557 %self.id%
~
#12558
Colossus chant book fake read~
1 c 2
read~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
%force% %actor% look chantbookcolossus
~
#12562
Clockwork Colossus debug tool~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if !%actor.is_immortal%
  halt
end
* Start of script fragment: colossus damage updater
eval colossus %instance.mob(12500)%
if %colossus%
  %send% %actor% You turn %self.shortdesc%...
  %echoaround% %actor% %actor.name% turns %self.shortdesc%...
  %buildingecho% %instance.start% A shudder runs through the colossus!
  %at% %colossus.room% %echo% A shudder runs through the colossus!
  dg_affect #12501 %colossus% off
  if !%colossus.varexists(parts_destroyed)%
    set parts_destroyed 1
    remote parts_destroyed %colossus.id%
  else
    eval parts_destroyed %colossus.parts_destroyed% + 1
    remote parts_destroyed %colossus.id%
  end
  switch %parts_destroyed%
    case 3
      nop %colossus.add_mob_flag(GROUP)%
      nop %colossus.remove_mob_flag(HARD)%
    break
    case 4
      nop %colossus.remove_mob_flag(GROUP)%
      nop %colossus.add_mob_flag(HARD)%
    break
  done
  %scale% %colossus% %colossus.level%
else
  %send% %actor% Colossus not found.
  halt
end
* End of script fragment
if %parts_destroyed% == 5 && %colossus%
  set room %colossus.room%
  %at% %room% %load% mob 12501
  set new_colossus %room.people%
  remote parts_destroyed %new_colossus.id%
  %purge% %colossus% $n abruptly shrinks!
end
~
$
