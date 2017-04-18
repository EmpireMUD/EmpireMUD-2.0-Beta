#18500
Sun King combat~
0 k 100
~
eval heroic_mode %self.mob_flagged(GROUP)%
* Count combat script cycles until enrage
* 1 cycle should be 30 seconds
if %heroic_mode%
  * Start scaling up immediately, game over at 15 minutes
  eval soft_enrage_cycles 1
  eval hard_enrage_cycles 30
else
  * Start scaling up after 5 minutes, game over at 20 minutes
  eval soft_enrage_cycles 10
  eval hard_enrage_cycles 40
end
eval enrage_counter 0
eval enraged 0
if %self.varexists(enrage_counter)%
  eval enrage_counter %self.enrage_counter%
end
eval enrage_counter %enrage_counter%+1
if %enrage_counter% >= %soft_enrage_cycles%
  * Start increasing damage
  eval enraged 1
  if %enrage_counter% > %hard_enrage_cycles%
    eval enraged 2
  end
  * Enrage messages
  if %enrage_counter% == %soft_enrage_cycles%
    say Tremble, for the time of my return is nigh!
    %echo% %self.name%'s attacks are growing stronger and stronger!
    %echo% You'd better return %self.himher% to the grave, quickly!
  end
  if %enrage_counter% == %hard_enrage_cycles%
    %echo% %self.name% floats into the air!
    shout I'll burn you all with the fires of the sun!
  end
end
remote enrage_counter %self.id%
* Enrage effects:
if %enraged%
  if %enraged% == 2
    * Stops using normal script attacks, just spams this every hit
    %echo% %self.name% unleashes an all-consuming torrent of emerald flames!
    %aoe% 1000 direct
    halt
  end
  * Don't always show the message or it would be kinda spammy
  if %random.4% == 4
    %echo% As %self.name% fights, the pallor of death fades from %self.hisher% skin!
  end
  dg_affect %self% BONUS-MAGICAL 10 3600
end
* Start of regular combat script:
switch %random.4%
  * Searing burns on tank
  case 1
    set adj searing
    if %heroic_mode%
      set adj traumatic
    end
    %send% %actor% &r%self.name% unleashes a blast of green fire at you, causing %adj% burns!&0
    %echoaround% %actor% %self.name% unleashes a blast of green fire at %actor.name%, causing %adj% burns!
    if %heroic_mode%
      %dot% %actor% 500 15 fire
      %damage% %actor% 150 fire
    else
      %dot% %actor% 200 15 fire
      %damage% %actor% 100 fire
    end
    wait 30 sec
  break
  * Emerald flame wave AoE
  case 2
    %echo% %self.name% raises %self.hisher% hands to the sky, and the green sun flares!
    * Give the healer (if any) time to prepare for group heals
    wait 3 sec
    %echo% &r%self.name% unleashes a wave of emerald fire from above!&0
    %echo% %self.name%'s wounds close!
    if %heroic_mode%
      %aoe% 75 fire
      %damage% %self% -500
    else
      %aoe% 50 fire
      %damage% %self% -200
    end
    wait 27 sec
  break
  * Mana drain + spirit bomb
  case 3
    %echo% %self.name% starts drawing all the mana in the room to %self.himher%self...
    eval cycles 4
    eval amount 50
    eval total_drained 0
    if %heroic_mode%
      eval amount 125
    end
    while %cycles%>0
      wait 5 sec
      eval cycles %cycles% - 1
      %echo% %self.name% draws more mana...
      eval room %self.room%
      eval person %room.people%
      while %person%
        if %person.is_pc%
          eval actual_amount %amount%
          eval mana_left %person.mana%
          if %mana_left% < %amount%
            eval actual_amount %mana_left%
          end
          %send% %person% %self.name% drains %actual_amount% of your mana!
          eval total_drained %total_drained% + %actual_amount%
          eval do %%person.mana(-%actual_amount%)%%
          nop %do%
        end
        eval person %person.next_in_room%
      done
    done
    %echo% %self.name% gathers the stolen mana together...
    wait 3 sec
    if %heroic_mode%
      * This divisor is important; if this attack is too strong, increase it a bit
      if %damage_scale% < 100
        eval damage_scale 100
      end
      eval damage_scale %total_drained% / 2
      %send% %actor% &r%self.name% hurls the stolen mana at you as a huge energy blast, which sends you flying!
      %echoaround% %actor% %self.name% hurls the stolen mana at %actor.name% in the form of a huge energy blast, sending %actor.himher% flying!
      dg_affect %actor% STUNNED on 10
      %damage% %actor% %damage_scale% magical
      if %damage_scale% >= 600
        %echo% &rThe energy blast explodes!
        eval aoe_scale %damage_scale%/10
        %aoe% %aoe_scale% magical
      end
    else
      %send% %actor% &r%self.name% hurls the stolen mana at you in the form of an energy blast!
      %echoaround% %actor% %self.name% hurls the stolen mana at %actor.name% in the form of an energy blast!
      %damage% %actor% 100
    end
    wait 7 sec
  break
  * Power word stun
  case 4
    %echo% %self.name% raises %self.hisher% staff high and mutters an incantation...
    eval interrupted 0
    wait 3 sec
    if %heroic_mode% && !%interrupted%
      * All enemies
      eval room %self.room%
      eval person %room.people%
      eval multi_target 1
    elseif %heroic_mode% || !%interrupted%
      * Random enemy
      eval person %random.enemy%
      if !%person%
        eval person %actor%
      end
      eval multi_target 0
    else
      halt
    end
    %echo% %self.name% utters a word which causes ripples in the fabric of reality.
    eval keep_going 1
    while %person% && %keep_going%
      eval test %%self.is_enemy(%person%)%%
      if %test%
        if !%person.trigger_counterspell%
          %send% %person% You fall to your knees as your body stops responding to your commands!
          %echoaround% %person% %person.name% falls to %person.hisher% knees, stunned.
          dg_affect %person% DODGE -50 10
          dg_affect %person% STUNNED on 5
        else
          %send% %person% Your counterspell protects you from %self.name%'s word of power!
          %echoaround% %person% %person.name% doesn't seem to be affected!
        end
      end
      if !%multi_target%
        eval keep_going 0
      else
        eval person %person.next_in_room%
      end
    done
    wait 27 sec
  break
done
~
#18501
Serpent Lord combat~
0 k 100
~
eval heroic_mode %self.mob_flagged(GROUP)%
eval attack %random.4%
switch %attack%
  case 1
    * Blade Dance
    * Hard: 100% AoE damage, 25% at a time
    * Boss: 375% AoE damage, 75% at a time
    say I'll ssslash you all to ribbonsss!
    %echo% %self.name% starts swinging %self.hisher% blades in a graceful dance!
    if %heroic_mode%
      eval cycles_left 5
    else
      eval cycles_left 4
    end
    while %cycles_left% >= 1
      wait 3 sec
      %echo% &rInvisible blades slash at you from every direction!
      if %heroic_mode%
        %aoe% 75 physical
      else
        %aoe% 25 physical
      end
      eval cycles_left %cycles_left% - 1
    done
    %echo% %self.name% finishes %self.hisher% blade dance.
    wait 15 sec
  break
  case 2
    * Naga Venom
    * Hard: 100% damage, 100% DoT for 30 seconds, slow for 10 seconds
    * Boss: 150% damage, 300% DoT for 15 seconds, stun for 5 seconds
    say Hold ssstill, thisss won't hurt...!
    %send% %actor% %self.name% lunges forward and grabs you!
    %echoaround% %actor% %self.name% lunges forward and grabs onto %actor.name%!
    wait 3 sec
    %send% %actor% %self.name% sinks %self.hisher% fangs deep into your neck!
    %echoaround% %actor% %self.name% sinks %self.hisher% fangs deep into %actor.name%'s neck!
    if %heroic_mode%
      %damage% %actor% 150
      %send% %actor% You suddenly feel a bone-deep numbness...
      %dot% %actor% 300 15 poison
      dg_affect %actor% STUNNED on 5
    else
      %damage% %actor% 100
      %send% %actor% The pain of the bite numbs...
      %dot% %actor% 100 30 poison
      dg_affect %actor% SLOW on 10
    end
    wait 30 sec
  break
  case 3
    * Powerful Blow
    * Hard: Stun tank 5 seconds, 75% damage
    * Boss: Stun tank 10 seconds, 150% damage
    * Boss: Also disarm for 30 seconds
    say I'll sssmash you to dussst!
    %echo% %self.name% raises all four of %self.hisher% blades overhead!
    wait 3 sec
    %send% %actor% &rAll four of %self.name%'s blades strike you with deadly force!
    %echoaround% %actor% All four of %self.name%'s blades strike %actor.name% with deadly force!
    %send% %actor% You are stunned by %self.name%'s powerful blow!
    if %heroic_mode%
      %damage% %actor% 150 physical
      dg_affect %actor% STUNNED on 10
      %send% %actor% %self.name%'s powerful blow sends your weapon flying!
      %echoaround% %actor% %self.name%'s powerful blow sends %actor.name%'s weapon flying!
      dg_affect %actor% DISARM on 30
    else
      %damage% %actor% 75 physical
      dg_affect %actor% STUNNED on 5
    end
    wait 30 sec
  break
  case 4
    * Mirage
    * Summons weak copies of the boss for 30 seconds
    * Hard: One copy
    * Hard: Two copies
    say Behold my massstery of magic!
    %echo% %self.name% starts casting a spell!
    wait 3 sec
    if %heroic_mode%
      %echo% %self.name% suddenly splits into three copies!
      eval times 2
    else
      %echo% %self.name% suddenly splits in two!
      eval times 1
    end
    while %times% > 0
      %load% mob 18507 %self% %self.level%
      eval times %times% - 1
      eval room %self.room%
      eval summon %room.people%
      if %summon.vnum% == 18507
        %force% %summon% %aggro% %actor%
      else
        %echo% Could not find summon.
      end
    done
    wait 30 sec
  break
done
~
#18502
Quetzalcoatl combat~
0 k 100
~
eval heroic_mode %self.mob_flagged(GROUP)%
eval attack %random.4%
switch %attack%
  case 1
    * Blinding Flash
    * Hard: Blind tank for 10 seconds, 25% damage
    * Boss: Blind everyone for 10 seconds, 25% aoe damage
    %echo% %self.name% spreads %self.hisher% prismatic wings wide!
    wait 2 sec
    if %heroic_mode%
      %echo% &r%self.name%'s jeweled feathers flash brightly, blinding everyone!
      %aoe% 25 magical
      eval room %self.room%
      eval person %room.people%
      while %person%
        eval test %%self.is_enemy(%person%)%%
        if %test%
          dg_affect %person% BLIND on 10
        end
        eval person %person.next_in_room%
      done
    else
      %send% %actor% &r%self.name%'s jeweled feathers flash brightly, blinding you!
      %echoaround% %actor% %self.name%'s jeweled feathers flash brightly, blinding %actor.name%!
      %damage% %actor% 25 magical
      dg_affect %actor% BLIND on 10
    end
  break
  case 2
    * Wind Dance
    * Buff dodge by level/5 (25-35) for 30 seconds
    * Boss: Also regen level/2 * 6 (372-522) health over 30 seconds (12-17 heal per second)
    * Amount:
    eval dodge_magnitude %self.level%/5
    eval heal_magnitude %self.level%/2
    %echo% %self.name% dances on the wind, becoming harder to hit!
    dg_affect %self% DODGE %dodge_magnitude% 30
    if %heroic_mode%
      %echo% As %self.name% dances, %self.hisher% wounds close!
      dg_affect %self% HEAL-OVER-TIME %heal_magnitude% 30
    end
  break
  case 3
    * West Wind
    * Hard: Mass disarm for 15 seconds
    * Boss: Also haste Quetzalcoatl for 30
    %echo% %self.name% flaps its wings hard, and a gale blows in from the west!
    %echo% Everyone's weapons are blown out of their hands!
    eval room %self.room%
    eval person %room.people%
    while %person%
      eval test %%self.is_enemy(%person%)%%
      if %test%
        dg_affect %person% DISARM on 15
      end
      eval person %person.next_in_room%
    done
    if %heroic_mode%
      %echo% %self.name% tumbles through the air faster than before!
      dg_affect %self% HASTE on 30
    end
  break
  case 4
    * Summon Snakes
    * Summons a snake to attack each PC for 30 seconds
    * Hard: Quetzalcoatl is stunned for 10 seconds
    %echo% %self.name% produces a whistling hissing sound.
    %echo% Several snakes slither out of the undergrowth!
    eval room %self.room%
    eval person %room.people%
    while %person%
      eval test %%self.is_enemy(%person%)%%
      if %test%
        %load% mob 18506
        eval summon %room.people%
        if %summon.vnum% == 18506
          %force% %summon% %aggro% %person%
        end
      end
      eval person %person.next_in_room%
    done
    if !%heroic_mode%
      dg_affect %self% STUNNED on 10
    end
  break
done
wait 30 sec
~
#18503
Jungle Temple Trash fight~
0 k 10
~
switch %random.3%
  case 1
    * Heal Self
    %echo% A nimbus of green light swirls around %self.name%, and %self.hisher% wounds start to rapidly heal!
    %damage% %self% -300
    eval magnitude %self.level% / 10
    dg_affect %self% HEAL-OVER-TIME %magnitude% 15
  break
  case 2
    * Green Bolt
    if %self.vnum% == 18503
      %echo% Green light begins to gather around %self.name%'s clenched fist...
    else
      %echo% Green light begins to gather within %self.name%'s mouth...
    end
    wait 5
    eval target %random.enemy%
    if !%target%
      * Blind?
      if %random.2% == 2
        eval target %actor%
      else
        %echo% %self.name% shoots a bolt of crackling emerald light, but misses completely.
        halt
      end
    end
    if %target.trigger_counterspell%
      %send% %target% &r%self.name% shoots a bolt of crackling emerald light at you, but it hits your counterspell and explodes!
      %echoaround% %target% %self.name% shoots a bolt of crackling emerald light at %target.name%, but it explodes in mid flight!
    else
      %send% %target% &r%self.name% shoots a bolt of crackling emerald light at you!
      %echoaround% %target% %self.name% shoots a bolt of crackling emerald light at %target.name%!
      %damage% %target% 100 magical
    end
  break
  case 3
    * Gorilla: Mighty Blow
    * Jaguar: Pounce
    * Cobra: Poison Bite
    if %self.vnum% == 18503
      %echo% %self.name% draws back %self.hisher% mighty fist...
      wait 5
      %send% %actor% &r%self.name% delivers a devastating punch, sending you flying!
      %echoaround% %actor% %self.name% delivers a devastating punch, sending %actor.name% flying!
      %damage% %actor% 150 physical
      if %actor.aff_flagged(!STUN)%
        %send% %actor% You land on your feet and jump back into battle.
        %echoaround% %actor% %actor.name% lands on %actor.hisher% feet and charges back into battle.
      else
        %send% %actor% You crash to the floor, briefly stunned.
        %echoaround% %actor% %actor.name% crashes to the floor, looking stunned.
        dg_affect %actor% STUNNED on 5
      end
    elseif %self.vnum% == 18504
      %echo% %self.name% crouches, gathering energy to pounce...
      wait 5
      %send% %actor% &r%self.name% pounces on you, raking you with %self.hisher% claws!
      %echoaround% %actor% %self.name% pounces on %actor.name%, raking %actor.himher% with %self.hisher% claws!
      %damage% %actor% 150 physical
      %dot% %actor% 150 15 physical
    elseif %self.vnum% == 18505
      %send% %actor% &r%self.name% whips forward and sinks its fangs into you before you can react!
      %echoaround% %actor% %self.name% whips forward and sinks its fangs into %actor.name%!
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
eval room_var %self%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  return 1
  halt
end
return 0
* Trap triggered
eval trap_running 1
global trap_running
eval last_trap_command 0
remote last_trap_command %actor.id%
%echoaround% %actor% %actor.name% starts walking %direction%...
%echo% You hear a quiet click...
wait 2 sec
%send% %actor% There is a grinding sound from the floor beneath your feet!
%echoaround% %actor% There is a grinding sound from the floor beneath %actor.name%'s feet!
wait 2 sec
eval trap_running 0
global trap_running
if %actor.last_trap_command% == jump && %actor.position% == Standing
  %send% %actor% You leap forward as the floor drops out from beneath your feet!
  %echoaround% %actor% %actor.name% leaps forward as a pit opens beneath %actor.hisher% feet!
  %send% %actor% You barely clear the pit, landing safely in the next room.
  %echoaround% %actor% %actor.name% barely clears the pit, landing safely in the next room.
  %teleport% %actor% %to_room%
  %echoaround% %actor% %actor.name% leaps through the doorway into the room!
  %force% %actor% look
else
  %send% %actor% The floor drops out from beneath your feet!
  %echoaround% %actor% The floor drops out from beneath %actor.name%'s feet!
  %send% %actor% Hissing surrounds you as you tumble down through the darkness!
  %echoaround% %actor% %actor.name% disappears into the darkness below!
  %damage% %actor% 100
  %teleport% %actor% i18516
  %echoaround% %actor% %actor.name% falls into the room through a chute in the ceiling, along with several snakes!
  %at% %actor.room% %load% mob 18516 %actor.level%
  %force% %actor% look
end
* Send NPC followers after player
eval person %room_var.people%
while %person%
  eval next_person %person.next_in_room%
  if %person.is_npc% && %person.master% == %actor%
    %echoaround% %person% %person.name% follows %actor.name%.
    %teleport% %person% %actor.room%
    %send% %actor% %person.name% follows you.
    %echoaround% %actor% %person.name% follows %actor.name%.
  end
  eval person %next_person%
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
eval room_var %self%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  return 1
  halt
end
return 0
* Trap triggered
eval trap_running 1
global trap_running
eval last_trap_command 0
remote last_trap_command %actor.id%
%echoaround% %actor% %actor.name% starts walking %direction%...
%echo% You hear a quiet click...
wait 2 sec
%send% %actor% There is a metallic 'shing' from the ceiling!
%echoaround% %actor% There is a metallic 'shing' from the ceiling above %actor.name%!
wait 2 sec
eval trap_running 0
global trap_running
if %actor.last_trap_command% == duck && %actor.position% == Standing
  %send% %actor% You hit the floor just as a blade slices through the space you were just in!
  %echoaround% %actor% A blade slices through the air, barely missing %actor.name%!
  %send% %actor% You scramble forward, reaching the next room safely.
  %echoaround% %actor% %actor.name% scrambles forward, reaching the next room.
  %teleport% %actor% %to_room%
  %echoaround% %actor% %actor.name% scrambles through the doorway from the previous room.
  %force% %actor% look
else
  %send% %actor% &rA blade slashes out of the ceiling, dealing you a deadly blow!
  %echoaround% %actor% %actor.name% is suddenly slashed by a blade swinging from the ceiling!
  %damage% %actor% 1000 physical
  %dot% %actor% 100 10 physical
  if %actor.health% > 0 && %actor.position% == Standing
    %send% %actor% You quickly scramble forward to the next room.
    %echoaround% %actor% %actor.name% scrambles forward to the next room, badly wounded.
    %teleport% %actor% %to_room%
    %echoaround% %actor% %actor.name% scrambles through the doorway from the previous room.
    %force% %actor% look
  else
    halt
  end
end
* Send NPC followers after player
eval person %room_var.people%
while %person%
  eval next_person %person.next_in_room%
  if %person.is_npc% && %person.master% == %actor%
    %echoaround% %person% %person.name% follows %actor.name%.
    %teleport% %person% %actor.room%
    %send% %actor% %person.name% follows you.
    %echoaround% %actor% %person.name% follows %actor.name%.
  end
  eval person %next_person%
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
eval room_var %self%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  return 1
  halt
end
return 0
* Trap triggered
eval trap_running 1
global trap_running
eval last_trap_command 0
remote last_trap_command %actor.id%
%echoaround% %actor% %actor.name% starts walking %direction%...
%echo% You hear a quiet click...
wait 2 sec
%send% %actor% There is a quiet swish behind you!
%echoaround% %actor% A dart flies past, barely missing %actor.name%!
wait 2 sec
eval trap_running 0
global trap_running
if %actor.last_trap_command% == run && %actor.position% == Standing
  %send% %actor% You dash forward, barely avoiding a hail of darts from the wall!
  %echoaround% %actor% %actor.name% dashes forward, barely avoiding a hail of darts from the wall!
  %send% %actor% You dive through an archway into the next room.
  %echoaround% %actor% %actor.name% dives through an archway into the next room.
  %teleport% %actor% %to_room%
  %echoaround% %actor% %actor.name% dives through the doorway from the previous room.
  %force% %actor% look
else
  %send% %actor% &rA dart flies out from a hole in the wall, piercing your side!
  %echoaround% %actor% A dart flies out from a hole in the wall, striking %actor.name%!
  %damage% %actor% 100 physical
  %dot% %actor% 1000 60 poison
  %send% %actor% You don't feel so good...
  if %actor.health% > 0 && %actor.position% == Standing
    %send% %actor% You quickly dive forward to the next room.
    %echoaround% %actor% %actor.name% dives forward to the next room, badly wounded.
    %teleport% %actor% %to_room%
    %echoaround% %actor% %actor.name% dives through the doorway from the previous room.
    %force% %actor% look
  end
end
* Send NPC followers after player
eval person %room_var.people%
while %person%
  eval next_person %person.next_in_room%
  if %person.is_npc% && %person.master% == %actor%
    %echoaround% %person% %person.name% follows %actor.name%.
    %teleport% %person% %actor.room%
    %send% %actor% %person.name% follows you.
    %echoaround% %actor% %person.name% follows %actor.name%.
  end
  eval person %next_person%
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
  %echoaround% %actor% %actor.name% leans forward and starts sprinting!
elseif jump /= %cmd%
  set last_trap_command jump
  %send% %actor% You dash forward and prepare to jump...
  %echoaround% %actor% %actor.name% dashes forward and prepares to jump...
elseif duck /= %cmd%
  set last_trap_command duck
  %send% %actor% You dive for the floor!
  %echoaround% %actor% %actor.name% dives for the floor and presses %actor.himher%self against the stone!
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
eval room %self.room%
if %room.template% != 18510
  return 1
  halt
end
return 0
* Make sure the boss isn't still here, guarding the key
eval person %room.people%
while %person%
  if %person.is_npc% && %person.vnum% >= 18500 && %person.vnum% <= 18502
    %send% %actor% %person.name% won't let you near %self.shortdesc%!
    return 0
    halt
  end
  eval person %person.next_in_room%
done
* Take idol, open doors, message
context %instance.id%
if %doors_open%
  return 1
  halt
end
eval doors_open 1
global doors_open
%send% %actor% As you remove the idol from the pedestal, there is a loud, mechanical grinding sound!
%echoaround% %actor% As %actor.name% removes the idol from the pedestal, there is a loud, mechanical grinding sound!
%load% obj 18510
%echo% A secret door opens up nearby!
eval door_room i18504
eval new_room i18511
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
  eval rand 0
else
  eval rand %random.3%
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
%echoaround% %actor% %actor.name% searches for traps...
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
eval room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
%send% %actor% You can't seem to get past %self.name%!
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
  %send% %actor% You can't set this adventure to that difficulty...
  return 1
  halt
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  eval difficulty 2
elseif group /= %arg%
  %send% %actor% You can't set this adventure to that difficulty...
  return 1
  halt
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  eval difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Clear existing difficulty flags and set new ones.
eval vnum 18500
while %vnum% <= 18502
  eval mob %%instance.mob(%vnum%)%%
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
  %send% %actor% You strike %self.shortdesc% hard, and it shatters!
  %echoaround% %actor% %actor.name% strikes %self.shortdesc% hard, and it shatters!
else
  %send% %actor% You cautiously strike %self.shortdesc%, and it shatters!
  %echoaround% %actor% %actor.name% strikes %self.shortdesc%, and it shatters!
end
%echo% There is a brilliant burst of emerald-green light, which fades to an omnipresent eerie glow...
%echo% The door slowly grinds open, revealing a dimly lit gallery beyond.
eval newroom i18502
%at% %newroom% %load% obj 18508
eval exitroom i18500
if %exitroom%
  %door% %exitroom% north room %newroom%
end
eval room %self.room%
eval person %room.people%
while %person%
  eval next_person %person.next_in_room%
  %teleport% %person% %newroom%
  eval person %next_person%
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
%send% %actor% You need to deal with %self.name% before you can climb back up!
return 0
~
#18517
Jungle Temple shop list - archaeologist~
0 c 0
list~
* List of items
%send% %actor% %self.name% sells the following items for jungle temple tokens:
%send% %actor% - riding jaguar whistle (14 tokens, land mount)
%send% %actor% - sea serpent whistle (16 tokens, aquatic mount)
%send% %actor% - feathered serpent whistle (40 tokens, flying mount)
%send% %actor% - jaguar cub whistle (24 tokens, minipet)
%send% %actor% - jaguar charm (18 tokens, Animal Morphs disguise morph)
%send% %actor% - fearsome jaguar armor pattern (20 tokens, tank armor)
%send% %actor% - red coyote armor pattern (20 tokens, melee armor)
%send% %actor% - feathered serpent armor pattern (20 tokens, healer armor)
%send% %actor% - eagle warrior armor pattern (20 tokens, caster armor)
%send% %actor% - blue chameleon armor pattern (20 tokens, pvp armor)
~
#18518
Jungle Temple shop buy - archaeologist~
0 c 0
buy~
eval vnum -1
eval cost 0
set named a thing
eval currency jungletemple_tokens
eval test %%actor.varexists(%currency%)%%
if !%test%
  %send% %actor% You don't have any of this shop's currency!
  halt
end
if (!%arg%)
  %send% %actor% Type 'list' to see what's available.
  halt
  * Disambiguate
elseif jaguar /= %arg%
  %send% %actor% Jaguar charm or jaguar cub whistle?
  halt
elseif feathered serpent /= %arg%
  %send% %actor% Feathered serpent whistle or feathered serpent armor pattern?
  halt
  * Mounts and pets etc
elseif riding jaguar whistle /= %arg%
  eval vnum 18512
  eval cost 14
  set named a riding jaguar whistle
elseif sea serpent whistle /= %arg%
  eval vnum 18513
  eval cost 16
  set named a sea serpent whistle
elseif feathered serpent whistle /= %arg%
  eval vnum 18514
  eval cost 40
  set named a feathered serpent whistle
elseif jaguar cub whistle /= %arg%
  eval vnum 18515
  eval cost 24
  set named a jaguar cub whistle
elseif jaguar charm /= %arg%
  eval vnum 18516
  eval cost 18
  set named a jaguar charm
  * Crafts
elseif fearsome jaguar armor pattern /= %arg%
  eval vnum 18518
  eval cost 20
  set named the fearsome jaguar armor pattern
elseif red coyote armor pattern /= %arg%
  eval vnum 18520
  eval cost 20
  set named the red coyote armor pattern
elseif feathered serpent armor pattern /= %arg%
  eval vnum 18522
  eval cost 20
  set named feathered serpent armor pattern
elseif eagle warrior armor pattern /= %arg%
  eval vnum 18524
  eval cost 20
  set named the eagle warrior armor pattern
elseif blue chameleon armor pattern /= %arg%
  eval vnum 18526
  eval cost 20
  set named the blue chameleon armor pattern
else
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
eval var %%actor.%currency%%%
eval test %var% >= %cost%
eval correct_noun tokens
if %cost% == 1
  eval correct_noun token
end
if !%test%
  %send% %actor% %self.name% tells you, 'You'll need %cost% jungle temple %correct_noun% to buy that.'
  halt
end
eval %currency% %var%-%cost%
remote %currency% %actor.id%
%load% obj %vnum% %actor% inv %actor.level%
%send% %actor% You buy %named% for %cost% jungle temple %correct_noun%.
%echoaround% %actor% %actor.name% buys %named%.
~
#18519
Jungle Temple boss death - drop tokens~
0 f 100
~
eval var_name jungletemple_tokens
* Tokens for everyone
eval room %self.room%
eval person %room.people%
while %person%
  if %person.is_pc%
    * You get a token, and you get a token, and YOU get a token!
    %send% %person% As %self.name% dies, a jungle temple token falls to the ground!
    %send% %person% You take the newly created token.
    if !%person.inventory(18543)%
      %load% obj 18543 %person% inv
      %send% %person% You find a pouch to store your jungle temple tokens in.
    end
    eval test %%person.varexists(%var_name%)%%
    if %test%
      eval val %%person.%var_name%%%
      eval %var_name% %val%+1
      remote %var_name% %person.id%
    else
      eval %var_name% 1
      remote %var_name% %person.id%
    end
    * Now purge summons
  elseif %person.vnum% == 18506
    %purge% %person% $n slithers away.
  elseif %person.vnum% == 18507
    %purge% %person% $n vanishes.
  end
  eval person %person.next_in_room%
done
if %self.vnum% == 18500
  %adventurecomplete%
end
~
#18520
Jungle Temple boss summon timer~
0 n 100
~
wait 30 sec
switch %self.vnum%
  case 18506
    %echo% %self.name% slithers away into the undergrowth.
  break
  default
    %echo% %self.name% vanishes.
  break
done
%purge% %self%
~
#18521
Jungle temple: cleared temple interior setup~
2 o 100
~
eval room %self%
%door% %room% down add 18502
eval room1 %room.down(room)%
%door% %room1% north add 18503
eval room2 %room1.north(room)%
%door% %room2% north add 18504
eval room3 %room2.north(room)%
%door% %room3% north add 18505
eval room4 %room3.north(room)%
%door% %room4% east add 18506
eval room5 %room4.east(room)%
%door% %room5% east add 18504
eval room6 %room5.east(room)%
%door% %room6% east add 18507
eval room7 %room6.east(room)%
%door% %room7% east add 18508
~
#18522
Jungle Temple adventure cleanup building replacer~
2 e 100
~
eval dir %room.exit_dir%
if !%instance.mob(18500)%
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
  %echo% %self.name% settles down to rest.
end
~
#18542
Jungle token load/purge~
1 gn 100
~
eval var_name jungletemple_tokens
if !%actor%
  eval actor %self.carried_by%
else
  %send% %actor% You take %self.shortdesc%.
  %echoaround% %actor% %actor.name% takes %self.shortdesc%.
  return 0
end
if %actor%
  if %actor.is_npc%
    * Oops
    halt
  end
  if !%actor.inventory(18543)%
    %load% obj 18543 %actor% inv
    %send% %actor% You find a pouch to store your ancient temple tokens in.
  end
  eval test %%actor.varexists(%var_name%)%%
  if %test%
    eval val %%actor.%var_name%%%
    eval %var_name% %val%+1
    remote %var_name% %actor.id%
  else
    eval %var_name% 1
    remote %var_name% %actor.id%
  end
  %purge% %self%
end
~
#18543
Jungle temple tokens count~
1 c 2
count~
eval test %%actor.obj_target(%arg%)%%
if %self% != %test%
  return 0
  halt
end
eval var_name jungletemple_tokens
%send% %actor% You count %self.shortdesc%.
eval test %%actor.varexists(%var_name%)%%
%echoaround% %actor% %actor.name% counts %self.shortdesc%.
if %test%
  eval count %%actor.%var_name%%%
  if %count% == 1
    %send% %actor% You have one token.
  else
    %send% %actor% You have %count% tokens.
  end
else
  %send% %actor% You have no tokens.
end
~
#18544
Boss loot replacer~
1 n 100
~
eval pet 18515
eval land_mount 18512
eval sea_mount 18513
eval sky_mount 18514
eval morpher 18516
eval token_item 18542
eval actor %self.carried_by%
if %actor%
  if %actor.mob_flagged(GROUP)%
    * Roll for mount
    eval percent_roll %random.100%
    if %percent_roll% <= 2
      * Minipet
      eval vnum %pet%
    else
      eval percent_roll %percent_roll% - 2
      if %percent_roll% <= 2
        * Land mount
        eval vnum %land_mount%
      else
        eval percent_roll %percent_roll% - 2
        if %percent_roll% <= 2
          * Sea mount
          eval vnum %sea_mount%
        else
          eval percent_roll %percent_roll% - 2
          if %percent_roll% <= 1
            * Flying mount
            eval vnum %sky_mount%
          else
            eval percent_roll %percent_roll% - 1
            if %percent_roll% <= 1
              * Morpher
              eval vnum %morpher%
            else
              * Token
              eval vnum %token_item%
            end
          end
        end
      end
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
  end
end
%purge% %self%
~
$
