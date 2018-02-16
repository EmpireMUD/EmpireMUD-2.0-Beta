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
$
