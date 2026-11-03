#9229
Feral Dog Pack~
0 n 100 1
L b 9229
~
if %self.leader%
  halt
end
set num %random.3%
while %num% > 0
  eval num %num% - 1
  %load% mob 9229 ally
  set mob %self.room.people%
  if %mob.vnum% == 9229 && %mob% != %self%
    nop %mob.add_mob_flag(SENTINEL)%
  end
done
~
#9232
Box Jellyfish Combat~
0 k 100 1
L w 9232
~
switch %random.3%
  case 1
    set part leg
  break
  case 2
    set part arm
  break
  case 3
    set part neck
  break
done
%send% %actor% |%self% tentacles wrap around your %part%... that stings!
%echoaround% %actor% |%self% tentacles wrap around |%actor% %part%!
%dot% #9232 %actor% 200 20 poison 100
~
#9236
Lion and friends combat script~
0 k 33 1
L w 9236
~
* cooldown time
set use_cooldown 20
* check cooldown
if %self.cooldown(9236)% || %self.disabled%
  halt
end
* allow 3-8 summons
set max_summons %self.var(max_summons,0)%
if %max_summons% <= 0
  eval max_summons %random.6% + 2
  remote max_summons %self.id%
end
* set cooldown on same-faction mobs and check count
set ch %self.room.people%
while %ch%
  if %ch.is_npc% && %ch.allegiance% == %self.allegiance%
    nop %ch.set_cooldown(9236,%use_cooldown%)%
  end
  set ch %ch.next_in_room%
done
* update count on all mobs with same vnum
eval lion_assist_count %self.var(lion_assist_count,0)% + 1
set ch %self.room.people%
while %ch%
  if %ch.is_npc% && %ch.vnum% == %self.vnum%
    remote lion_assist_count %ch.id%
    remote max_summons %ch.id%
  end
  set ch %ch.next_in_room%
done
* cancel if over the summon cap
if %lion_assist_count% >= %max_summons%
  detach 9236 %self.id%
  halt
end
* summon otherwise
wait 1 s
%load% mob %self.vnum% ally %self.level%
set loaded %self.room.people%
if %loaded.vnum% == %self.vnum% && %loaded% != %self%
  * success
  %echo% Another lion leaps in from out of sight!
  remote lion_assist_count %loaded.id%
  remote max_summons %loaded.id%
  nop %loaded.set_cooldown(9236,%use_cooldown%)%
  %force% %loaded% maggro
end
~
#9242
Load pack animals (vnum + 1)~
0 n 100 0
~
eval vnum %self.vnum% + 1
set num %random.2%
while %num% > 0
  eval num %num% - 1
  %load% mob %vnum% ally
done
~
#9249
Summon non-following copies on-load (1-2)~
0 n 100 0
~
* Loads 1-2 copies if I'm the only one of me here
* verify I'm alone
set ch %self.room.people%
while %ch%
  if %ch% != %self% && %ch.vnum% == %self.vnum%
    * two's a crowd
    halt
  end
  set ch %ch.next_in_room%
done
* I think I'm alone now
set num %random.2%
while %num% > 0
  %load% mob %self.vnum%
  eval num %num% - 1
done
* Note: Copies are free-range, not followers
~
#9250
Load Mate (vnum + 1)~
0 n 100 0
~
eval vnum %self.vnum% + 1
%load% mob %vnum% ally
~
#9252
Summon duplicate followers on load (1-2)~
0 n 100 0
~
Loads 1-2 copies of me as followers if alone
* verify I'm alone
set ch %self.room.people%
while %ch%
  if %ch% != %self% && %ch.vnum% == %self.vnum%
    * two's a crowd
    halt
  end
  set ch %ch.next_in_room%
done
* I think I'm alone now
set num %random.2%
while %num% > 0
  %load% mob %self.vnum% ally
  eval num %num% - 1
done
* Note: Copies are free-range, not followers
~
#9255
Snake: Deadly Venom~
0 k 100 1
L w 9255
~
* High-damage-over-time bite. Combine with a NO-ATTACK flag.
%echo% ~%self% lunges forward and bites ~%actor%!
if %actor.has_tech(!Poison)%
  halt
end
%dot% #9255 %actor% 200 30 poison 100
~
#9259
Summon duplicate followers on-load (3-4)~
0 n 100 0
~
* Loads 3-4 copies if I'm the only one of me here
* verify I'm alone
set ch %self.room.people%
while %ch%
  if %ch% != %self% && %ch.vnum% == %self.vnum%
    * two's a crowd
    halt
  end
  set ch %ch.next_in_room%
done
* I think I'm alone now
eval num %random.2% + 2
while %num% > 0
  %load% mob %self.vnum% ally
  eval num %num% - 1
done
* Note: Copies are free-range, not followers
~
#9274
Chimp troop~
0 n 100 1
L b 9274
~
set num %random.3%
while %num% > 0
  eval num %num% - 1
  %load% mob 9274 ally
done
~
#9275
Bonobo troop~
0 n 100 2
L b 9276
L b 9277
~
%load% mob 9276 ally
set num %random.2%
while %num% > 0
  eval num %num% - 1
  %load% mob 9277 ally
done
~
#9284
Elephant herd~
0 n 100 2
L b 9285
L b 9286
~
eval num 1 + %random.2%
while %num% > 0
  %load% mob 9285 ally
  eval num %num% - 1
done
if %random.2% == 2
  %load% mob 9286 ally
end
~
#9296
Dire-tusked Mammoth War Platform Leaves Mammoth on Death~
5 f 100 2
L b 9296
L r 9296
~
%load% mob 9296
set mob %self.room.people%
if %mob.vnum% == 9296
  %slay% %mob%
end
%load% veh 9296
~
#9297
Dire-tusk mammoth to War Platform when barded~
0 n 100 1
L r 9297
~
wait 1
%load% veh 9297
set veh %self.room.vehicles%
if %veh.vnum% == 9297
  * auto-claim
  set owner_match 0
  set only_empire 0
  set ch %self.room.people%
  * detect claimant
  while %ch%
    if %ch.is_pc% && %ch.empire%
      if %ch.empire% == %self.room.empire%
        set owner_match 1
      elseif %ch.empire% && %only_empire% == 0
        set only_empire %ch.empire%
      elseif %ch.empire% && %ch.empire% != %only_empire%
        set only_empire -1
      end
    end
    set ch %ch.next_in_room%
  done
  * did we find a claimant
  if %owner_match%
    %own% %veh% %self.room.empire%
  elseif %only_empire% != 0 && %only_empire% != -1
    %own% %veh% %only_empire%
  end
end
%purge% %self%
~
$
