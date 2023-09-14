#18500
Sun King combat~
0 k 100
~
set heroic_mode %self.mob_flagged(GROUP)%
* Count combat script cycles until enrage
* 1 cycle should be 30 seconds
if %self.cooldown(18501)%
  halt
end
nop %self.set_cooldown(18501, 30)%
if %heroic_mode%
  * Start scaling up immediately, game over at 15 minutes
  set soft_enrage_cycles 1
  set hard_enrage_cycles 30
else
  * Start scaling up after 5 minutes, game over at 20 minutes
  set soft_enrage_cycles 10
  set hard_enrage_cycles 40
end
set enrage_counter 0
set enraged 0
if %self.varexists(enrage_counter)%
  set enrage_counter %self.enrage_counter%
end
eval enrage_counter %enrage_counter%+1
if %enrage_counter% >= %soft_enrage_cycles%
  * Start increasing damage
  set enraged 1
  if %enrage_counter% > %hard_enrage_cycles%
    set enraged 2
  end
  * Enrage messages
  if %enrage_counter% == %soft_enrage_cycles%
    say Tremble, for the time of my return is nigh!
    %echo% |%self% attacks are growing stronger and stronger!
    %echo% You'd better return *%self% to the grave, quickly!
  end
  if %enrage_counter% == %hard_enrage_cycles%
    %echo% ~%self% floats into the air!
    shout I'll burn you all with the fires of the sun!
  end
end
remote enrage_counter %self.id%
* Enrage effects:
if %enraged%
  if %enraged% == 2
    * Stops using normal script attacks, just spams this every hit
    %echo% ~%self% unleashes an all-consuming torrent of emerald flames!
    %aoe% 1000 direct
    halt
  end
  * Don't always show the message or it would be kinda spammy
  if %random.4% == 4
    %echo% As ~%self% fights, the pallor of death fades from ^%self% skin!
  end
  dg_affect #18502 %self% BONUS-MAGICAL 10 3600
end
* Start of regular combat script:
switch %random.4%
  * Searing burns on tank
  case 1
    set adj searing
    if %heroic_mode%
      set adj traumatic
    end
    %send% %actor% &&r~%self% unleashes a blast of green fire at you, causing %adj% burns!&&0
    %echoaround% %actor% ~%self% unleashes a blast of green fire at ~%actor%, causing %adj% burns!
    if %heroic_mode%
      %dot% #18503 %actor% 500 15 fire
      %damage% %actor% 150 fire
    else
      %dot% #18503 %actor% 200 15 fire
      %damage% %actor% 100 fire
    end
  break
  * Emerald flame wave AoE
  case 2
    %echo% ~%self% raises ^%self% hands to the sky, and the green sun flares!
    * Give the healer (if any) time to prepare for group heals
    wait 3 sec
    %echo% &&r~%self% unleashes a wave of emerald fire from above!&&0
    %echo% |%self% wounds close!
    if %heroic_mode%
      %aoe% 75 fire
      %damage% %self% -500
    else
      %aoe% 50 fire
      %damage% %self% -200
    end
  break
  * Mana drain + spirit bomb
  case 3
    set actor_id %actor.id%
    %echo% ~%self% starts drawing all the mana in the room to *%self%self...
    set cycles 4
    set amount 50
    set total_drained 0
    if %heroic_mode%
      set amount 125
    end
    while %cycles%>0
      wait 5 sec
      eval cycles %cycles% - 1
      %echo% ~%self% draws more mana...
      set person %self.room.people%
      while %person%
        if %person.is_pc%
          set actual_amount %amount%
          set mana_left %person.mana%
          if %mana_left% < %amount%
            set actual_amount %mana_left%
          end
          %send% %person% ~%self% drains %actual_amount% of your mana!
          eval total_drained %total_drained% + %actual_amount%
          nop %person.mana(-%actual_amount%)%
        end
        set person %person.next_in_room%
      done
    done
    %echo% ~%self% gathers the stolen mana together...
    wait 3 sec
    if !%actor% || %actor.id% != %actor_id%
      halt
    end
    if %heroic_mode%
      * This divisor is important; if this attack is too strong, increase it a bit
      if %damage_scale% < 100
        set damage_scale 100
      end
      eval damage_scale %total_drained% / 2
      %send% %actor% &&r~%self% hurls the stolen mana at you as a huge energy blast, which sends you flying!
      %echoaround% %actor% ~%self% hurls the stolen mana at ~%actor% in the form of a huge energy blast, sending *%actor% flying!
      dg_affect #18504 %actor% HARD-STUNNED on 10
      %damage% %actor% %damage_scale% magical
      if %damage_scale% >= 600
        %echo% &&rThe energy blast explodes!
        eval aoe_scale %damage_scale%/10
        %aoe% %aoe_scale% magical
      end
    else
      %send% %actor% &&r~%self% hurls the stolen mana at you in the form of an energy blast!
      %echoaround% %actor% ~%self% hurls the stolen mana at ~%actor% in the form of an energy blast!
      %damage% %actor% 100
    end
  break
  * Power word stun
  case 4
    set actor_id %actor.id%
    %echo% ~%self% raises ^%self% staff high and mutters an incantation...
    set interrupted 0
    wait 3 sec
    if %heroic_mode% && !%interrupted%
      * All enemies
      set person %self.room.people%
      set multi_target 1
    elseif %heroic_mode% || !%interrupted%
      * Random enemy
      set person %random.enemy%
      if !%person% && %actor.id% == %actor_id%
        set person %actor%
      end
      set multi_target 0
    else
      halt
    end
    %echo% ~%self% utters a word which causes ripples in the fabric of reality.
    set keep_going 1
    while %person% && %keep_going%
      if %self.is_enemy(%person%)%
        if !%person.trigger_counterspell%
          %send% %person% You fall to your knees as your body stops responding to your commands!
          %echoaround% %person% ~%person% falls to ^%person% knees, stunned.
          dg_affect #18506 %person% DODGE -50 10
          dg_affect #18505 %person% STUNNED on 5
        else
          %send% %person% Your counterspell protects you from |%self% word of power!
          %echoaround% %person% ~%person% doesn't seem to be affected!
        end
      end
      if !%multi_target%
        set keep_going 0
      else
        set person %person.next_in_room%
      end
    done
  break
done
~
#18501
Serpent Lord combat~
0 k 100
~
if %self.cooldown(18501)%
  halt
end
nop %self.set_cooldown(18501, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
set verify_target %actor.id%
switch %random.4%
  case 1
    * Blade Dance
    * Hard: 100% AoE damage, 25% at a time
    * Boss: 375% AoE damage, 75% at a time
    say I'll ssslash you all to ribbonsss!
    %echo% ~%self% starts swinging ^%self% blades in a graceful dance!
    if %heroic_mode%
      set cycles_left 5
    else
      set cycles_left 4
    end
    while %cycles_left% >= 1
      wait 3 sec
      %echo% &&rInvisible blades slash at you from every direction!
      if %heroic_mode%
        %aoe% 75 physical
      else
        %aoe% 25 physical
      end
      eval cycles_left %cycles_left% - 1
    done
    %echo% ~%self% finishes ^%self% blade dance.
  break
  case 2
    * Naga Venom
    * Hard: 100% damage, 100% DoT for 30 seconds, slow for 10 seconds
    * Boss: 150% damage, 300% DoT for 15 seconds, stun for 5 seconds
    say Hold ssstill, thisss won't hurt...!
    %send% %actor% ~%self% lunges forward and grabs you!
    %echoaround% %actor% ~%self% lunges forward and grabs onto ~%actor%!
    wait 3 sec
    if %verify_target% != %actor.id%
      halt
    end
    %send% %actor% ~%self% sinks ^%self% fangs deep into your neck!
    %echoaround% %actor% ~%self% sinks ^%self% fangs deep into |%actor% neck!
    if %heroic_mode%
      %damage% %actor% 150
      %send% %actor% You suddenly feel a bone-deep numbness...
      %dot% #18508 %actor% 300 15 poison
      dg_affect #18507 %actor% HARD-STUNNED on 5
    else
      %damage% %actor% 100
      %send% %actor% The pain of the bite numbs...
      %dot% #18508 %actor% 100 30 poison
      dg_affect #18507 %actor% SLOW on 10
    end
  break
  case 3
    * Powerful Blow
    * Hard: Stun tank 5 seconds, 75% damage
    * Boss: Stun tank 10 seconds, 150% damage
    * Boss: Also disarm for 30 seconds
    say I'll sssmash you to dussst!
    %echo% ~%self% raises all four of ^%self% blades overhead!
    wait 3 sec
    if %verify_target% != %actor.id%
      halt
    end
    %send% %actor% &&rAll four of |%self% blades strike you with deadly force!
    %echoaround% %actor% All four of |%self% blades strike ~%actor% with deadly force!
    %send% %actor% You are stunned by |%self% powerful blow!
    if %heroic_mode%
      %damage% %actor% 150 physical
      dg_affect #18509 %actor% HARD-STUNNED on 10
      %send% %actor% |%self% powerful blow sends your weapon flying!
      %echoaround% %actor% |%self% powerful blow sends |%actor% weapon flying!
      dg_affect #18510 %actor% DISARMED on 30
    else
      %damage% %actor% 75 physical
      dg_affect #18509 %actor% STUNNED on 5
    end
  break
  case 4
    * Mirage
    * Summons weak copies of the boss for 30 seconds
    * Hard: One copy
    * Hard: Two copies
    say Behold my massstery of magic!
    %echo% ~%self% starts casting a spell!
    wait 3 sec
    if %heroic_mode%
      %echo% ~%self% suddenly splits into three copies!
      set times 2
    else
      %echo% ~%self% suddenly splits in two!
      set times 1
    end
    while %times% > 0
      %load% mob 18507 ally %self.level%
      eval times %times% - 1
      set summon %self.room.people%
      if %summon.vnum% == 18507
        %force% %summon% %aggro% %actor%
      else
        %echo% Could not find summon.
      end
    done
  break
done
~
#18502
Quetzalcoatl combat~
0 k 100
~
if %self.cooldown(18501)%
  halt
end
nop %self.set_cooldown(18501, 30)%
set heroic_mode %self.mob_flagged(GROUP)%
switch %random.4%
  case 1
    * Blinding Flash
    * Hard: Blind tank for 10 seconds, 25% damage
    * Boss: Blind everyone for 10 seconds, 25% aoe damage
    %echo% ~%self% spreads ^%self% prismatic wings wide!
    set verify_target %actor.id%
    wait 2 sec
    if %heroic_mode%
      %echo% &&r|%self% jeweled feathers flash brightly, blinding everyone!
      %aoe% 25 magical
      set person %self.room.people%
      while %person%
        if %self.is_enemy(%person%)%
          dg_affect #18511 %person% BLIND on 10
        end
        set person %person.next_in_room%
      done
    else
      if %verify_target% != %actor.id%
        halt
      end
      %echo% |%self% jeweled feathers flash brightly, blinding ~%actor%!
      %damage% %actor% 25 magical
      dg_affect #18511 %actor% BLIND on 10
    end
  break
  case 2
    * Wind Dance
    * Buff dodge by level/5 (25-35) for 30 seconds
    * Boss: Also regen level/2 * 6 (372-522) health over 30 seconds (12-17 heal per second)
    * Amount:
    eval dodge_magnitude %self.level%/5
    eval heal_magnitude %self.level%/2
    %echo% ~%self% dances on the wind, becoming harder to hit!
    dg_affect #18512 %self% DODGE %dodge_magnitude% 30
    if %heroic_mode%
      %echo% As ~%self% dances, ^%self% wounds close!
      dg_affect #18512 %self% HEAL-OVER-TIME %heal_magnitude% 30
    end
  break
  case 3
    * West Wind
    * Hard: Mass disarm for 15 seconds
    * Boss: Also haste Quetzalcoatl for 30
    %echo% ~%self% flaps its wings hard, and a gale blows in from the west!
    %echo% Everyone's weapons are blown out of their hands!
    set person %self.room.people%
    while %person%
      if %self.is_enemy(%person%)%
        dg_affect #18513 %person% DISARMED on 15
      end
      set person %person.next_in_room%
    done
    if %heroic_mode%
      %echo% ~%self% tumbles through the air faster than before!
      dg_affect #18514 %self% HASTE on 30
    end
  break
  case 4
    * Summon Snakes
    * Summons a snake to attack each PC for 30 seconds
    * Hard: Quetzalcoatl is stunned for 10 seconds
    %echo% ~%self% produces a whistling hissing sound.
    %echo% Several snakes slither out of the undergrowth!
    set room %self.room%
    set person %room.people%
    while %person%
      if %self.is_enemy(%person%)%
        %load% mob 18506
        set summon %room.people%
        if %summon.vnum% == 18506
          %force% %summon% %aggro% %person%
        end
      end
      set person %person.next_in_room%
    done
    if !%heroic_mode%
      dg_affect %self% STUNNED on 10
    end
  break
done
~
#18503
Jungle Temple Trash fight~
0 k 10
~
switch %random.3%
  case 1
    * Heal Self
    %echo% A nimbus of green light swirls around ~%self%, and ^%self% wounds start to rapidly heal!
    %damage% %self% -300
    eval magnitude %self.level% / 10
    dg_affect %self% HEAL-OVER-TIME %magnitude% 15
  break
  case 2
    * Green Bolt
    if %self.vnum% == 18503
      %echo% Green light begins to gather around |%self% clenched fist...
    else
      %echo% Green light begins to gather within |%self% mouth...
    end
    wait 5
    set target %random.enemy%
    if !%target%
      * Blind?
      if %random.2% == 2
        set target %actor%
      else
        %echo% ~%self% shoots a bolt of crackling emerald light, but misses completely.
        halt
      end
    end
    if %target.trigger_counterspell%
      %send% %target% &&r~%self% shoots a bolt of crackling emerald light at you, but it hits your counterspell and explodes!
      %echoaround% %target% ~%self% shoots a bolt of crackling emerald light at ~%target%, but it explodes in mid flight!
    else
      %send% %target% &&r~%self% shoots a bolt of crackling emerald light at you!
      %echoaround% %target% ~%self% shoots a bolt of crackling emerald light at ~%target%!
      %damage% %target% 100 magical
    end
  break
  case 3
    * Gorilla: Mighty Blow
    * Jaguar: Pounce
    * Cobra: Poison Bite
    if %self.vnum% == 18503
      %echo% ~%self% draws back ^%self% mighty fist...
      wait 5
      %send% %actor% &&r~%self% delivers a devastating punch, sending you flying!
      %echoaround% %actor% ~%self% delivers a devastating punch, sending ~%actor% flying!
      %damage% %actor% 150 physical
      if %actor.aff_flagged(!STUN)%
        %send% %actor% You land on your feet and jump back into battle.
        %echoaround% %actor% ~%actor% lands on ^%actor% feet and charges back into battle.
      else
        %send% %actor% You crash to the floor, briefly stunned.
        %echoaround% %actor% ~%actor% crashes to the floor, looking stunned.
        dg_affect %actor% STUNNED on 5
      end
    elseif %self.vnum% == 18504
      %echo% ~%self% crouches, gathering energy to pounce...
      wait 5
      %send% %actor% &&r~%self% pounces on you, raking you with ^%self% claws!
      %echoaround% %actor% ~%self% pounces on ~%actor%, raking *%actor% with ^%self% claws!
      %damage% %actor% 150 physical
      %dot% %actor% 150 15 physical
    elseif %self.vnum% == 18505
      %send% %actor% &&r~%self% whips forward and sinks its fangs into you before you can react!
      %echoaround% %actor% ~%self% whips forward and sinks its fangs into ~%actor%!
      %damage% %actor% 50 physical
      %dot% %actor% 300 60 poison
    end
  break
done
~
#18504
Snake pit trap~
2 q 100
~
if %actor.is_npc%
  return 1
  halt
end
context %instance.id%
* One quick trick to get the target room
eval tricky %%self.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %self.template%)
  return 1
  halt
end
return 0
* Trap triggered
set trap_running 1
global trap_running
set last_trap_command 0
remote last_trap_command %actor.id%
%echoaround% %actor% ~%actor% starts walking %direction%...
%echo% You hear a quiet click...
wait 2 sec
%send% %actor% There is a grinding sound from the floor beneath your feet!
%echoaround% %actor% There is a grinding sound from the floor beneath |%actor% feet!
wait 2 sec
set trap_running 0
global trap_running
if %actor.last_trap_command% == jump && %actor.position% == Standing
  %send% %actor% You leap forward as the floor drops out from beneath your feet!
  %echoaround% %actor% ~%actor% leaps forward as a pit opens beneath ^%actor% feet!
  %send% %actor% You barely clear the pit, landing safely in the next room.
  %echoaround% %actor% ~%actor% barely clears the pit, landing safely in the next room.
  %teleport% %actor% %tricky%
  %echoaround% %actor% ~%actor% leaps through the doorway into the room!
  %force% %actor% look
else
  %send% %actor% The floor drops out from beneath your feet!
  %echoaround% %actor% The floor drops out from beneath |%actor% feet!
  %send% %actor% Hissing surrounds you as you tumble down through the darkness!
  %echoaround% %actor% ~%actor% disappears into the darkness below!
  %damage% %actor% 100
  %teleport% %actor% i18516
  %echoaround% %actor% ~%actor% falls into the room through a chute in the ceiling, along with several snakes!
  %at% %actor.room% %load% mob 18516 %actor.level%
  %force% %actor% look
end
* Send NPC followers after player
set person %self.people%
while %person%
  set next_person %person.next_in_room%
  if %person.is_npc% && %person.leader% == %actor%
    %echoaround% %person% ~%person% follows ~%actor%.
    %teleport% %person% %actor.room%
    %send% %actor% ~%person% follows you.
    %echoaround% %actor% ~%person% follows ~%actor%.
  end
  set person %next_person%
done
~
#18505
Swinging blade trap~
2 q 100
~
if %actor.is_npc%
  return 1
  halt
end
context %instance.id%
* One quick trick to get the target room
eval tricky %%self.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %self.template%)
  return 1
  halt
end
return 0
* Trap triggered
set trap_running 1
global trap_running
set last_trap_command 0
remote last_trap_command %actor.id%
%echoaround% %actor% ~%actor% starts walking %direction%...
%echo% You hear a quiet click...
wait 2 sec
%send% %actor% There is a metallic 'shing' from the ceiling!
%echoaround% %actor% There is a metallic 'shing' from the ceiling above ~%actor%!
wait 2 sec
set trap_running 0
global trap_running
if %actor.last_trap_command% == duck && %actor.position% == Standing
  %send% %actor% You hit the floor just as a blade slices through the space you were just in!
  %echoaround% %actor% A blade slices through the air, barely missing ~%actor%!
  %send% %actor% You scramble forward, reaching the next room safely.
  %echoaround% %actor% ~%actor% scrambles forward, reaching the next room.
  %teleport% %actor% %tricky%
  %echoaround% %actor% ~%actor% scrambles through the doorway from the previous room.
  %force% %actor% look
else
  %send% %actor% &&rA blade slashes out of the ceiling, dealing you a deadly blow!
  %echoaround% %actor% ~%actor% is suddenly slashed by a blade swinging from the ceiling!
  %damage% %actor% 1000 physical
  %dot% %actor% 100 10 physical
  if %actor.health% > 0 && %actor.position% == Standing
    %send% %actor% You quickly scramble forward to the next room.
    %echoaround% %actor% ~%actor% scrambles forward to the next room, badly wounded.
    %teleport% %actor% %tricky%
    %echoaround% %actor% ~%actor% scrambles through the doorway from the previous room.
    %force% %actor% look
  else
    halt
  end
end
* Send NPC followers after player
set person %self.people%
while %person%
  set next_person %person.next_in_room%
  if %person.is_npc% && %person.leader% == %actor%
    %echoaround% %person% ~%person% follows ~%actor%.
    %teleport% %person% %actor.room%
    %send% %actor% ~%person% follows you.
    %echoaround% %actor% ~%person% follows ~%actor%.
  end
  set person %next_person%
done
~
#18506
Poison dart trap~
2 q 100
~
if %actor.is_npc%
  return 1
  halt
end
context %instance.id%
* One quick trick to get the target room
eval tricky %%self.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %self.template%)
  return 1
  halt
end
return 0
* Trap triggered
set trap_running 1
global trap_running
set last_trap_command 0
remote last_trap_command %actor.id%
%echoaround% %actor% ~%actor% starts walking %direction%...
%echo% You hear a quiet click...
wait 2 sec
%send% %actor% There is a quiet swish behind you!
%echoaround% %actor% A dart flies past, barely missing ~%actor%!
wait 2 sec
set trap_running 0
global trap_running
if %actor.last_trap_command% == run && %actor.position% == Standing
  %send% %actor% You dash forward, barely avoiding a hail of darts from the wall!
  %echoaround% %actor% ~%actor% dashes forward, barely avoiding a hail of darts from the wall!
  %send% %actor% You dive through an archway into the next room.
  %echoaround% %actor% ~%actor% dives through an archway into the next room.
  %teleport% %actor% %tricky%
  %echoaround% %actor% ~%actor% dives through the doorway from the previous room.
  %force% %actor% look
else
  %send% %actor% &&rA dart flies out from a hole in the wall, piercing your side!
  %echoaround% %actor% A dart flies out from a hole in the wall, striking ~%actor%!
  %damage% %actor% 100 physical
  %dot% %actor% 1000 60 poison
  %send% %actor% You don't feel so good...
  if %actor.health% > 0 && %actor.position% == Standing
    %send% %actor% You quickly dive forward to the next room.
    %echoaround% %actor% ~%actor% dives forward to the next room, badly wounded.
    %teleport% %actor% %tricky%
    %echoaround% %actor% ~%actor% dives through the doorway from the previous room.
    %force% %actor% look
  end
end
* Send NPC followers after player
set person %self.people%
while %person%
  set next_person %person.next_in_room%
  if %person.is_npc% && %person.leader% == %actor%
    %echoaround% %person% ~%person% follows ~%actor%.
    %teleport% %person% %actor.room%
    %send% %actor% ~%person% follows you.
    %echoaround% %actor% ~%person% follows ~%actor%.
  end
  set person %next_person%
done
~
#18507
Trap action detection~
2 c 0
run jump duck~
context %instance.id%
if !%trap_running%
  %send% %actor% You don't need to do that right now.
  return 1
  halt
end
if run /= %cmd%
  set last_trap_command run
  %send% %actor% You lean forward and start sprinting!
  %echoaround% %actor% ~%actor% leans forward and starts sprinting!
elseif jump /= %cmd%
  set last_trap_command jump
  %send% %actor% You dash forward and prepare to jump...
  %echoaround% %actor% ~%actor% dashes forward and prepares to jump...
elseif duck /= %cmd%
  set last_trap_command duck
  %send% %actor% You dive for the floor!
  %echoaround% %actor% ~%actor% dives for the floor and presses *%actor%self against the stone!
end
remote last_trap_command %actor.id%
~
#18508
Trap flee fallback~
2 q 100
~
context %instance.id%
if %trap_running%
  %send% %actor% You can't leave right now!
  return 0
  halt
end
~
#18509
Idol-take open doors~
1 g 100
~
set room %self.room%
if %room.template% != 18510
  return 1
  halt
end
return 0
* Make sure the boss isn't still here, guarding the key
set person %room.people%
while %person%
  if %person.is_npc% && %person.vnum% >= 18500 && %person.vnum% <= 18502
    %send% %actor% ~%person% won't let you near @%self%!
    return 0
    halt
  end
  set person %person.next_in_room%
done
* Take idol, open doors, message
context %instance.id%
if %doors_open%
  return 1
  halt
end
set doors_open 1
global doors_open
%send% %actor% As you remove the idol from the pedestal, there is a loud, mechanical grinding sound!
%echoaround% %actor% As ~%actor% removes the idol from the pedestal, there is a loud, mechanical grinding sound!
%load% obj 18510
%echo% A secret door opens up nearby!
set door_room i18504
set new_room i18511
if %door_room% && %new_room%
  %door% %door_room% east room %new_room%
  %echo% A new door has opened somewhere else in the temple!
end
return 1
halt
~
#18510
Randomly assign jungle temple traps~
2 n 100
~
context %instance.id%
* Pick and attach a random trap
if %random.10% == 10
  set rand 0
else
  set rand %random.3%
end
switch %rand%
  case 1
    attach 18504 %self.id%
    set room_trap 18504
    global room_trap
  break
  case 2
    attach 18505 %self.id%
    set room_trap 18505
    global room_trap
  break
  case 3
    attach 18506 %self.id%
    set room_trap 18506
    global room_trap
  break
done
attach 18507 %self.id%
attach 18508 %self.id%
attach 18512 %self.id%
detach 18510 %self.id%
~
#18511
Jungle Temple trash spawner~
1 n 100
~
switch %random.3%
  case 1
    %load% mob 18503
  break
  case 2
    %load% mob 18504
  break
  case 3
    %load% mob 18505
  break
done
%purge% %self%
~
#18512
Search for Traps - Jungle Temple~
2 p 100
~
if %abilityname% != Search
  return 1
  halt
end
%send% %actor% You search for traps...
%echoaround% %actor% ~%actor% searches for traps...
context %instance.id%
switch %room_trap%
  case 18504
    %send% %actor% You find a pit trap. JUMP after triggering to avoid it.
  break
  case 18505
    %send% %actor% You find a swinging blade trap. DUCK after triggering to avoid it.
  break
  case 18506
    %send% %actor% You find a poison dart trap. RUN after triggering to avoid it.
  break
  default
    %send% %actor% You can't spot any traps in this room...
  break
done
return 0
halt
~
#18513
Mob block higher template id~
0 s 100
~
* One quick trick to get the target room
set room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
%send% %actor% You can't seem to get past ~%self%!
return 0
~
#18514
Jungle Temple difficulty select~
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
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  set difficulty 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  set difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
set vnum 18500
while %vnum% <= 18502
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
if %difficulty% >= 3
  %send% %actor% You strike @%self% hard, and it shatters!
  %echoaround% %actor% ~%actor% strikes @%self% hard, and it shatters!
else
  %send% %actor% You cautiously strike @%self%, and it shatters!
  %echoaround% %actor% ~%actor% strikes @%self%, and it shatters!
end
%echo% There is a brilliant burst of emerald-green light, which fades to an omnipresent eerie glow...
%echo% The door slowly grinds open, revealing a dimly lit gallery beyond.
set newroom i18502
%at% %newroom% %load% obj 18508
set exitroom i18500
if %exitroom%
  %door% %exitroom% north room %newroom%
end
set person %self.room.people%
while %person%
  set next_person %person.next_in_room%
  %teleport% %person% %newroom%
  set person %next_person%
done
~
#18515
Jungletemple delayed despawn~
1 f 0
~
%adventurecomplete%
~
#18516
Snake pit block exit~
0 s 100
~
if %actor.nohassle%
  halt
end
%send% %actor% You need to deal with ~%self% before you can climb back up!
return 0
~
#18517
LT Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(18500)%
end
~
#18519
Jungle Temple boss death - drop tokens~
0 f 100
~
set var_name jungletemple_tokens
* Tokens for everyone
set person %self.room.people%
while %person%
  if %person.is_pc%
    * You get a token, and you get a token, and YOU get a token!
    %send% %person% As ~%self% dies, a jungle temple token falls to the ground!
    %send% %person% You take the newly created token.
    nop %person.give_currency(18500, 1)%
    * Now purge summons
  elseif %person.vnum% == 18506
    %purge% %person% $n slithers away.
  elseif %person.vnum% == 18507
    %purge% %person% $n vanishes.
  end
  set person %person.next_in_room%
done
if %self.vnum% == 18500
  %adventurecomplete%
  %at% %instance.location% %load% obj 18502
end
~
#18520
Jungle Temple boss summon timer~
0 n 100
~
wait 30 sec
switch %self.vnum%
  case 18506
    %echo% ~%self% slithers away into the undergrowth.
  break
  default
    %echo% ~%self% vanishes.
  break
done
%purge% %self%
~
#18521
Jungle temple: cleared temple interior setup~
2 o 100
~
set room %self%
%door% %room% down add 18502
set room1 %room.down(room)%
%door% %room1% north add 18503
set room2 %room1.north(room)%
%door% %room2% north add 18504
set room3 %room2.north(room)%
%door% %room3% north add 18505
set room4 %room3.north(room)%
%door% %room4% east add 18506
set room5 %room4.east(room)%
%door% %room5% east add 18504
set room6 %room5.east(room)%
%door% %room6% east add 18507
set room7 %room6.east(room)%
%door% %room7% east add 18508
~
#18522
Jungle Temple adventure cleanup building replacer~
2 e 100
~
set start %instance.start%
if %start% && %start.var(18247_hidden,0)%
  * shortcut and do not leave a temple behind
  halt
end
set item %room.contents%
while %item%
  if %item.vnum% == 18502
    set instance_cleared 1
    %purge% %item%
  end
  set item %item.next_in_list%
done
set dir %room.exit_dir%
if %instance_cleared%
  if %dir%
    %build% %room% 18501 %dir%
  end
else
  if %dir%
    %build% %room% 18509 %dir%
  end
end
~
#18523
Enrage Buff/Counter Reset~
0 bw 25
~
if !%self.fighting% && %self.varexists(enrage_counter)%
  if %self.enrage_counter% == 0
    halt
  end
  dg_affect %self% BONUS-MAGICAL off 1
  set enrage_counter 0
  remote enrage_counter %self.id%
  %restore% %self%
  %echo% ~%self% settles down to rest.
end
~
#18544
Boss loot replacer~
1 n 100
~
set pet 18515
set land_mount 18512
set sea_mount 18513
set sky_mount 18514
set morpher 18516
set token_item 18542
set actor %self.carried_by%
if %actor%
  if %actor.mob_flagged(GROUP)%
    * Roll for mount
    set percent_roll %random.100%
    if %percent_roll% <= 2
      * Minipet
      set vnum %pet%
    else
      eval percent_roll %percent_roll% - 2
      if %percent_roll% <= 2
        * Land mount
        set vnum %land_mount%
      else
        eval percent_roll %percent_roll% - 2
        if %percent_roll% <= 2
          * Sea mount
          set vnum %sea_mount%
        else
          eval percent_roll %percent_roll% - 2
          if %percent_roll% <= 1
            * Flying mount
            set vnum %sky_mount%
          else
            eval percent_roll %percent_roll% - 1
            if %percent_roll% <= 1
              * Morpher
              set vnum %morpher%
            else
              * Token
              set vnum %token_item%
            end
          end
        end
      end
    end
    if %self.level%
      set level %self.level%
    else
      set level 100
    end
    %load% obj %vnum% %actor% inv %level%
    set item %actor.inventory(%vnum%)%
    if %item.is_flagged(BOP)%
      nop %item.bind(%self%)%
    end
    * %send% %actor% @%self% turns out to be %item.shortdesc%!
  end
end
%purge% %self%
~
$
