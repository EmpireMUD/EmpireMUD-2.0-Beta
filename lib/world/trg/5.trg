#505
Moon Rabbit Familiar Buffs~
0 kt 100
~
if %self.disabled%
  halt
end
if %self.cooldown(500)%
  halt
end
if %self.varexists(selected_ability)%
  set selected_ability %self.selected_ability%
end
if !%selected_ability%
  set selected_ability %random.4%
end
nop %self.set_cooldown(500, 25)
switch %selected_ability%
  case 1
    * Restore mana on master
    set targ %self.master%
    if !%targ%
      %echo% %self.name% glows and purrs.
      halt
    end
    if %targ.mana% == %targ.maxmana%
      %send% %targ% %self.name% tries to restore your mana, but it is full!
      %echoaround% %targ% %self.name% glows and purrs.
      nop %self.set_cooldown(500, 5)
      halt
    end
    %send% %targ% %self.name% shines brightly at you, and you feel replenished!
    %echoaround% %targ% %self.name% shines brightly at %targ.name%, who looks replenished!
    %heal% %actor% mana 100
  break
  case 2
    * Mana regen buff on master
    set targ %self.master%
    if !%targ%
      %echo% %self.name% glows and purrs.
      halt
    end
    %send% %targ% %self.name% shines brightly at you, and you feel re-energized!
    %echoaround% %targ% %self.name% shines brightly at %targ.name%, who looks re-energized!
    eval amount %self.level% / 10
    dg_affect #505 %targ% MANA-REGEN %amount% 30
  break
  case 3
    * Resist buff on party
    %echo% %self.name% shines as brightly as the moon, lighting up the whole party!
    eval amount %self.level% / 20
    set ch %self.room.people%
    while %ch%
      if %self.is_ally(%ch%)%
        %send% %ch% You bask in %self.name%'s glow!
        dg_affect #505 %ch% RESIST-PHYSICAL %amount% 30
        dg_affect #505 %ch% RESIST-MAGICAL %amount% 30
      end
      set ch %ch.next_in_room%
    done
  break
  case 4
    * Resist buff on tank
    set enemy %self.fighting%
    if !%enemy%
      %echo% %self.name% glows and purrs.
      halt
    end
    set targ %enemy.fighting%
    if (!%targ% || !%self.is_ally(%targ%)%)
      %echo% %self.name% glows and purrs.
      halt
    end
    %send% %targ% %self.name% shines brightly at you, and you feel the moon's protection!
    %echoaround% %targ% %self.name% shines brightly at %targ.name%, protecting %targ.himher%!
    eval amount %self.level% / 5
    dg_affect #505 %targ% RESIST-PHYSICAL %amount% 30
    dg_affect #505 %targ% RESIST-MAGICAL %amount% 30
  break
done
~
#506
Moon Rabbit commands~
0 ct 0
order~
set arg1 %arg.car%
set arg %arg.cdr%
set arg2 %arg.car%
* discard the rest
if %actor.char_target(%arg1%)% != %self%
  return 0
  halt
end
if %actor% != %self.master%
  return 0
  halt
end
set ability_0 random
set ability_1 replenish
set ability_2 energize
set ability_3 brightness
set ability_4 protection
if %ability_1% /= %arg2%
  set selected_ability 1
elseif %ability_2% /= %arg2%
  set selected_ability 2
elseif %ability_3% /= %arg2%
  set selected_ability 3
elseif %ability_4% /= %arg2%
  set selected_ability 4
elseif random /= %arg2%
  set selected_ability 0
  remote selected_ability %self.id%
  %send% %actor% You order %self.name% to use abilities at random.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  halt
elseif status /= %arg2%
  set current_ability 0
  if %self.varexists(selected_ability)%
    set current_ability %self.selected_ability%
  end
  eval ability_name %%ability_%current_ability%%%
  %send% %actor% %self.name% is currently using: %ability_name%.
  %send% %actor% %self.name% has the following abilities available:
  %send% %actor% replenish: Replenish (restore mana on master)
  %send% %actor% energize: Re-energize (+mana-regen on master)
  %send% %actor% brightness: Bright as the moon (+resistance on party)
  %send% %actor% protection: Moon's protection (+resistance on tank)
  halt
else
  * other command
  return 0
  halt
end
if %selected_ability%
  remote selected_ability %self.id%
  eval ability_name %%ability_%selected_ability%%%
  %send% %actor% You order %self.name% to use: %ability_name%.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  return 1
  halt
end
~
#507
Spirit Wolf Familiar Debuffs~
0 kt 100
~
if %self.disabled%
  halt
end
if %self.cooldown(500)%
  halt
end
set targ %random.enemy%
if !%targ%
  %echo% You feel a chill as %self.name%'s howl echoes out through the air!
  halt
end
nop %self.set_cooldown(500, 25)%
if %self.varexists(selected_ability)%
  set selected_ability %self.selected_ability%
end
if !%selected_ability%
  set selected_ability %random.4%
end
switch %selected_ability%
  case 1
    * Dexterity debuff on enemy
    %send% %targ% %self.name% flashes brightly and shoots a bolt of lightning at you!
    %echoaround% %targ% %self.name% flashes brightly and shoots a bolt of lightning at %targ.name%!
    eval amount %self.level% / 100
    dg_affect #507 %targ% DEXTERITY -%amount% 30
  break
  case 2
    * Dodge debuff on enemy
    %send% %targ% %self.name% howls, followed by a clap of thunder, deafening your ears!
    %echoaround% %targ% %self.name% howls, followed by a clap of thunder, and %targ.name% looks deafened!
    eval amount 15 + %self.level% / 25
    dg_affect #507 %targ% DODGE -%amount% 30
  break
  case 3
    * Tohit debuff on enemy
    %send% %targ% %self.name% barks at you, and your vision begins to blur!
    %echoaround% %targ% %self.name% barks at %targ.name%, who squints as if %targ.heshe%'s having trouble seeing!
    eval amount 15 + %self.level% / 25
    dg_affect #507 %targ% TO-HIT -%amount% 30
  break
  case 4
    * Magical DoT on enemy
    %send% %targ% %self.name% bites into you, your skin sizzling from the ghost energy!
    %echoaround% %targ% %self.name% bites into %targ.name%, %targ.hisher% skin sizzling from the ghost energy!
    * Damage is scaled by the script engine, no need to mess with it
    set amount 100
    %dot% #508 %targ% %amount% 30 magical 1
  break
done
~
#508
Spirit Wolf commands~
0 ct 0
order~
set arg1 %arg.car%
set arg %arg.cdr%
set arg2 %arg.car%
* discard the rest
if %actor.char_target(%arg1%)% != %self%
  return 0
  halt
end
if %actor% != %self.master%
  return 0
  halt
end
set ability_0 random
set ability_1 lightning
set ability_2 thunder
set ability_3 bark
set ability_4 bite
if %ability_1% /= %arg2%
  set selected_ability 1
elseif %ability_2% /= %arg2%
  set selected_ability 2
elseif %ability_3% /= %arg2%
  set selected_ability 3
elseif %ability_4% /= %arg2%
  set selected_ability 4
elseif random /= %arg2%
  set selected_ability 0
  remote selected_ability %self.id%
  %send% %actor% You order %self.name% to use abilities at random.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  halt
elseif status /= %arg2%
  set current_ability 0
  if %self.varexists(selected_ability)%
    set current_ability %self.selected_ability%
  end
  eval ability_name %%ability_%current_ability%%%
  %send% %actor% %self.name% is currently using: %ability_name%.
  %send% %actor% %self.name% has the following abilities available:
  %send% %actor% lightning: Bolt of Lightning (-dex on random enemy)
  %send% %actor% thunder: Clap of Thunder (-dodge on random enemy)
  %send% %actor% bark: Blurred Vision (-tohit on random enemy)
  %send% %actor% bite: Ghost Energy (magic DoT on random enemy)
  halt
else
  * other command
  return 0
  halt
end
if %selected_ability%
  remote selected_ability %self.id%
  eval ability_name %%ability_%selected_ability%%%
  %send% %actor% You order %self.name% to use: %ability_name%.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  return 1
  halt
end
~
#509
Phoenix Familiar Buffs~
0 kt 100
~
if %self.disabled%
  halt
end
if %self.cooldown(500)%
  halt
end
if %self.varexists(selected_ability)%
  set selected_ability %self.selected_ability%
end
if !%selected_ability%
  set selected_ability %random.4%
end
nop %self.set_cooldown(500, 25)
switch %selected_ability%
  case 1
    * Bonus-healing buff on master
    set targ %self.master%
    if !%targ%
      %echo% %self.name% flickers and burns.
      halt
    end
    %send% %targ% Fire from %self.name% spreads over you, and you blaze with power!
    %echoaround% %targ% Fire from %self.name% spreads over %targ.name%, who blazes with power!
    eval amount %self.level% / 20
    dg_affect #509 %targ% BONUS-HEALING %amount% 30
  break
  case 2
    * Restore health on tank
    set enemy %self.fighting%
    if !%enemy%
      %echo% %self.name% flickers and burns.
      halt
    end
    set targ %enemy.fighting%
    if (!%targ% || !%self.is_ally(%targ%)%)
      %echo% %self.name% flickers and burns.
      halt
    end
    if %targ.health% == %targ.maxhealth%
      %send% %self.master% %self.name% tries to heal %targ.name%, but %targ.heshe% doesn't need healing.
      %echoneither% %self.master% %targ% %self.name% flies in circles around %targ.name%.
      %send% %targ% %self.name% flies in circles around you.
      nop %self.set_cooldown(500, 5)%
      halt
    end
    %send% %targ% %self.name% flies into your chest, and you feel a healing warmth!
    %echoaround% %targ% %self.name% flies into %targ.name%'s chest and glows outward with a healing warmth!
    %heal% %targ% health 100
  break
  case 3
    * Restore health on party
    %echo% %self.name% bursts into flames, sending a healing fire through the party!
    set healing_done 0
    set ch %self.room.people%
    while %ch%
      if %self.is_ally(%ch%)%
        if %ch.health% < %ch.maxhealth%
          %send% %ch% You feel warmed by %self.name%'s fire!
          %heal% %ch% health 50
          set healing_done 1
        end
      end
      set ch %ch.next_in_room%
    done
    if !%healing_done%
      nop %self.set_cooldown(500, 5)%
    end
  break
  case 4
    * Bonus damage buff on party
    %echo% %self.name% bursts into flames, inspiring burning passion in the party!
    eval amount %self.level% / 30
    set ch %self.room.people%
    while %ch%
      if %self.is_ally(%ch%)%
        %send% %ch% You feel inspired by %self.name%'s fire!
        dg_affect #509 %ch% BONUS-MAGICAL %amount% 30
        dg_affect #509 %ch% BONUS-PHYSICAL %amount% 30
      end
      set ch %ch.next_in_room%
    done
  break
done
~
#510
Scorpion Shadow Familiar Debuffs~
0 kt 100
~
if %self.disabled%
  halt
end
set master %self.master%
if !%master%
  %echo% %self.name% shrinks into the shadows and vanishes.
  %purge% %self%
  halt
end
if %self.cooldown(500)%
  halt
end
if %self.varexists(selected_ability)%
  set selected_ability %self.selected_ability%
end
set targ %random.enemy%
if !%targ%
  %echo% %master.name%'s scorpion shadow twists and coils from the darkness.
  halt
end
nop %self.set_cooldown(500, 25)%
if !%selected_ability%
  set selected_ability %random.4%
end
switch %selected_ability%
  case 1
    * Slow on enemy
    %send% %targ% %master.name%'s scorpion shadow stings you with a creeping venom!
    %send% %master% Your scorpion shadow stings %targ.name% with a creeping venom!
    %echoneither% %targ% %master% %master.name%'s scorpion shadow stings %targ.name% with a creeping venom!
    dg_affect #510 %targ% SLOW ON 30
  break
  case 2
    * Damage debuff on enemy
    %send% %targ% %master.name%'s scorpion shadow stings you with shadow venom!
    %send% %master% Your scorpion shadow stings %targ.name% with shadow venom!
    %echoneither% %targ% %master% %master.name%'s scorpion shadow stings %targ.name% with shadow venom!
    eval amount %self.level% / 20
    dg_affect #510 %targ% BONUS-PHYSICAL -%amount% 30
    dg_affect #510 %targ% BONUS-MAGICAL -%amount% 30
  break
  case 3
    * Wits debuff on enemy
    %send% %targ% %master.name%'s scorpion shadow stings you with numbing venom!
    %send% %master% Your scorpion shadow stings %targ.name% with a numbing venom!
    %echoneither% %targ% %master% %master.name%'s scorpion shadow stings %targ.name% with numbing venom!
    eval amount 2 + %self.level% / 100
    dg_affect #510 %targ% WITS -%amount% 30
  break
  case 4
    * Poison DoT on enemy
    %send% %targ% %master.name%'s scorpion shadow stings you with agonizing venom!
    %send% %master% Your scorpion shadow stings %targ.name% with agonizing venom!
    %echoneither% %targ% %master% %master.name%'s scorpion shadow stings %targ.name% with agonizing venom!
    %dot% #510 %targ% 100 30 poison 1
  break
done
~
#511
Owl Shadow Familiar Buffs~
0 kt 100
~
if %self.disabled%
  halt
end
if %self.cooldown(500)%
  halt
end
set master %self.master%
if !%master%
  %echo% %self.name% shrinks into the shadows and vanishes.
  %purge% %self%
  halt
end
if %self.varexists(selected_ability)%
  set selected_ability %self.selected_ability%
end
if !%selected_ability%
  set selected_ability %random.4%
end
* only one of these does not target party
if (%selected_ability% == 4)
  * Dodge buff on one ally
  set enemy %self.fighting%
  if !%enemy%
    %echoaround% %master% %master.name%'s shadow seems to flap its wings.
    halt
  end
  set targ %enemy.fighting%
  if (!%targ% || !%self.is_ally(%targ%)%)
    %echoaround% %master% %master.name%'s shadow seems to flap its wings.
    halt
  end
  if %targ% != %master%
    %send% %targ% %master.name%'s owl shadow wraps its dark wings around you, protecting you!
    %send% %master% Your owl shadow wraps its dark wings around %targ.name%, protecting %targ.himher%!
  else
    %send% %master% Your owl shadow wraps its dark wings around you, protecting you!
  end
  %echoneither% %targ% %master% %master.name%'s owl shadow wraps its dark wings around %targ.name%, protecting %targ.himher%!
  eval amount 15 + %self.level% / 25
  dg_affect #511 %targ% DODGE %amount% 30
  nop %self.set_cooldown(500, 25)%
  halt
end
* random results 1-3
%send% %master% Your owl shadow wraps itself around the party!
%echoaround% %master% %master.name%'s owl shadow wraps itself around the party!
set ch %self.room.people%
set had_effect 0
while %ch%
  if %self.is_ally(%ch%)%
    switch %selected_ability%
      case 1
        * Rejuvenate all allies
        %send% %ch% You feel the healing darkness cure your injuries!
        eval amount %self.level% / 20
        dg_affect #511 %ch% HEAL-OVER-TIME %amount% 30
      break
      case 2
        * Restore move on all allies
        if %ch.move()% < %ch.maxmove%
          set had_effect 1
        end
        %send% %ch% You feel the invigorating darkness restore your stamina!
        eval amount %self.level% / 5
        nop %targ.move(%amount%)%
      break
      case 3
        * Wits buff on all allies
        %send% %ch% You feel the brilliant darkness boost your speed!
        eval amount 1 + %self.level% / 100
        dg_affect #511 %ch% WITS %amount% 30
      break
    done
  end
  set ch %ch.next_in_room%
done
if %selected_ability% == 2 && %had_effect%
  nop %self.set_cooldown(500, 25)%
elseif %selected_ability% != 2
  nop %self.set_cooldown(500, 25)%
end
~
#512
Basilisk Familiar Debuffs~
0 kt 100
~
if %self.disabled%
  halt
end
if %self.cooldown(500)%
  halt
end
if %self.varexists(selected_ability)%
  set selected_ability %self.selected_ability%
end
if !%selected_ability%
  set selected_ability %random.4%
end
* one ability hits tank only
if (%selected_ability% == 4)
  * Block buff on tank
  set enemy %self.fighting%
  if !%enemy%
    %echo% %self.name% flicks its tongue and whips its tail.
    halt
  end
  set targ %enemy.fighting%
  if (!%targ% || !%self.is_ally(%targ%)%)
    %echo% %self.name% flicks its tongue and whips its tail.
    halt
  end
  %send% %targ% %self.name% unleases its marble gaze upon you, hardening your form against attacks!
  %echoaround% %targ% %self.name% unleashes its marble gaze upon %targ.name%, hardening %targ.hisher% form against attacks!
  eval amount 15 + %self.level% / 50
  dg_affect #512 %targ% BLOCK %amount% 30
  nop %self.set_cooldown(500, 25)%
  halt
end
* other 3 abilities hit the enemy
set targ %random.enemy%
if !%targ%
  %echo% %self.name% flicks its tongue and whips its tail.
  halt
end
switch %selected_ability%
  case 1
    * Damage debuff on random enemy
    %send% %targ% %self.name% unleashes its quartzite gaze upon you, turning you partially to stone!
    %echoaround% %targ% %self.name% unleashes its quartzite gaze upon %targ.name%, turning %targ.himher% partially to stone!
    eval amount -1 * %self.level% / 20
    dg_affect #512 %targ% BONUS-PHYSICAL %amount% 30
    dg_affect #512 %targ% BONUS-MAGICAL %amount% 30
  break
  case 2
    * Wits debuff on random enemy
    %send% %targ% %self.name% unleashes its basalt gaze upon you, turning you partially to stone!
    %echoaround% %targ% %self.name% unleashes its basalt gaze upon %targ.name%, turning %targ.himher% partially to stone!
    eval amount 2 + %self.level% / 100
    dg_affect #512 %targ% WITS -%amount% 30
  break
  case 3
    * Tohit buff on random enemy
    %send% %targ% %self.name% unleashes its granite gaze upon you, turning you partially to stone!
    %echoaround% %targ% %self.name% unleashes its granite gaze upon %targ.name%, turning %targ.himher% partially to stone!
    eval amount 15 + %self.level% / 25
    dg_affect #512 %targ% TO-HIT -%amount% 30
  break
done
nop %self.set_cooldown(500, 25)%
~
#513
Salamander Familiar Buffs~
0 kt 100
~
if %self.disabled%
  halt
end
if %self.cooldown(500)%
  halt
end
if %self.varexists(selected_ability)%
  set selected_ability %self.selected_ability%
end
if !%selected_ability%
  set selected_ability %random.4%
end
switch %selected_ability%
  case 1
    * Short, LARGE bonus-healing buff on master
    set targ %self.master%
    if !%targ%
      %echo% %self.name% sizzles and simmers.
      halt
    end
    %send% %targ% %self.name% coils around you, granting Alchemist's Fire to your healing spells!
    %echoaround% %targ% %self.name% coils around %targ.name%, granting %targ.himher% Alchemist's Fire!
    eval amount %self.level% / 4
    dg_affect #513 %targ% BONUS-HEALING %amount% 10
  break
  case 2
    * Short, large heal-over-time on tank
    set enemy %self.fighting%
    if !%enemy%
      %echo% %self.name% sizzles and simmers.
      halt
    end
    set targ %enemy.fighting%
    if (!%targ% || !%self.is_ally(%targ%)%)
      %echo% %self.name% sizzles and simmers.
      halt
    end
    %send% %targ% %self.name% breathes its soothing flames upon you, and your wounds begin to heal!
    %echoaround% %targ% %self.name% breathes its soothing flames upon %targ.name%, whose wounds begin to heal!
    eval amount %self.level% / 2
    dg_affect #513 %targ% HEAL-OVER-TIME %amount% 10
  break
  case 3
    * Dodge buff on tank
    set enemy %self.fighting%
    if !%enemy%
      %echo% %self.name% sizzles and simmers.
      halt
    end
    set targ %enemy.fighting%
    if (!%targ% || !%self.is_ally(%targ%)%)
      %echo% %self.name% sizzles and simmers.
      halt
    end
    %send% %targ% %self.name% breathes its guardian flames upon you, protecting you!
    %echoaround% %targ% %self.name% breathes its guardian flames upon %targ.name%, protecting %targ.himher%!
    eval amount 15 + %self.level% / 25
    dg_affect #513 %targ% DODGE %amount% 30
  break
  case 4
    * Damage buff on party
    %echo% %self.name% sputters and throws embers out at the whole party!
    eval amount %self.level% / 30
    set ch %self.room.people%
    while %ch%
      if %self.is_ally(%ch%)%
        %send% %ch% You feel yourself surge in the embers' glow!
        dg_affect #513 %ch% BONUS-MAGICAL %amount% 30
        dg_affect #513 %ch% BONUS-PHYSICAL %amount% 30
      end
      set ch %ch.next_in_room%
    done
  break
done
nop %self.set_cooldown(500, 25)%
~
#514
Shadow Wolf Familiars: Hide with Master~
0 ct 0
hide~
* never block command
return 0
* master only
set master %self.master%
if !%master%
  halt
end
if %actor% != %master%
  halt
end
if %self.fighting% || %master.fighting%
  halt
end
* master is hiding; hide
if !%master.ability(Hide)%
  halt
end
dg_affect %self% HIDE on -1
~
#515
Banshee Familiar Debuffs~
0 kt 100
~
if %self.disabled%
  halt
end
if %self.cooldown(500)%
  halt
end
if %self.varexists(selected_ability)%
  set selected_ability %self.selected_ability%
end
if !%selected_ability%
  set selected_ability %random.4%
end
switch %selected_ability%
  case 1
    * Resist debuff
    %echo% %self.name% lets out a soul-shattering wail!
    eval amount -1 * %self.level% / 20
  break
  case 2
    * To-hit debuff
    %echo% %self.name% lets out a terrifying wail!
    eval amount 5 + %self.level% / 50
  break
  case 3
    * Damage debuff
    %echo% %self.name% lets out a heart-wrenching wail!
    eval amount -1 * %self.level% / 30
  break
  case 4
    * DoT effect
    %echo% %self.name% lets out a blood-curdling wail!
  break
done
set ch %self.room.people%
while %ch%
  if %self.is_enemy(%ch%)%
    %send% %ch% You feel the banshee's wail strike deep into your heart!
    switch %selected_ability^
      case 1
        dg_affect #515 %ch% RESIST-MAGICAL %amount% 30
        dg_affect #515 %ch% RESIST-PHYSICAL %amount% 30
      break
      case 2
        dg_affect #515 %ch% TO-HIT -%amount% 30
      break
      case 3
        dg_affect #515 %ch% BONUS-MAGICAL %amount% 30
        dg_affect #515 %ch% BONUS-PHYSICAL %amount% 30
      break
      case 4
        %dot% #515 %ch% 50 30 magical 1
      break
    done
  end
  set ch %ch.next_in_room%
done
nop %self.set_cooldown(500, 25)%
~
#516
Phoenix commands~
0 ct 0
order~
set arg1 %arg.car%
set arg %arg.cdr%
set arg2 %arg.car%
* discard the rest
if %actor.char_target(%arg1%)% != %self%
  return 0
  halt
end
if %actor% != %self.master%
  return 0
  halt
end
set ability_0 random
set ability_1 blaze
set ability_2 warmth
set ability_3 fire
set ability_4 passion
if %ability_1% /= %arg2%
  set selected_ability 1
elseif %ability_2% /= %arg2%
  set selected_ability 2
elseif %ability_3% /= %arg2%
  set selected_ability 3
elseif %ability_4% /= %arg2%
  set selected_ability 4
elseif random /= %arg2%
  set selected_ability 0
  remote selected_ability %self.id%
  %send% %actor% You order %self.name% to use abilities at random.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  halt
elseif status /= %arg2%
  set current_ability 0
  if %self.varexists(selected_ability)%
    set current_ability %self.selected_ability%
  end
  eval ability_name %%ability_%current_ability%%%
  %send% %actor% %self.name% is currently using: %ability_name%.
  %send% %actor% %self.name% has the following abilities available:
  %send% %actor% blaze: Blaze with Power (+bonus healing on master)
  %send% %actor% warmth: Healing Warmth (heals the tank)
  %send% %actor% fire: Healing Fire (heals the whole party)
  %send% %actor% passion: Burning Passion (+bonus damage on party)
  halt
else
  * other command
  return 0
  halt
end
if %selected_ability%
  remote selected_ability %self.id%
  eval ability_name %%ability_%selected_ability%%%
  %send% %actor% You order %self.name% to use: %ability_name%.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  return 1
  halt
end
~
#517
Scorpion Shadow commands~
0 ct 0
order~
set arg1 %arg.car%
set arg %arg.cdr%
set arg2 %arg.car%
* discard the rest
if %actor.char_target(%arg1%)% != %self%
  return 0
  halt
end
if %actor% != %self.master%
  return 0
  halt
end
set ability_0 random
set ability_1 creeping
set ability_2 shadow
set ability_3 numbing
set ability_4 agonizing
if %ability_1% /= %arg2%
  set selected_ability 1
elseif %ability_2% /= %arg2%
  set selected_ability 2
elseif %ability_3% /= %arg2%
  set selected_ability 3
elseif %ability_4% /= %arg2%
  set selected_ability 4
elseif random /= %arg2%
  set selected_ability 0
  remote selected_ability %self.id%
  %send% %actor% You order %self.name% to use abilities at random.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  halt
elseif status /= %arg2%
  set current_ability 0
  if %self.varexists(selected_ability)%
    set current_ability %self.selected_ability%
  end
  eval ability_name %%ability_%current_ability%%%
  %send% %actor% %self.name% is currently using: %ability_name%.
  %send% %actor% %self.name% has the following abilities available:
  %send% %actor% creeping: Creeping Venom (slow on random enemy)
  %send% %actor% shadow: Shadow Venom (-damage on random enemy)
  %send% %actor% numbing: Numbing Venom (-wits on random enemy)
  %send% %actor% agonizing: Agonizing Venom (poison DoT on random enemy)
  halt
else
  * other command
  return 0
  halt
end
if %selected_ability%
  remote selected_ability %self.id%
  eval ability_name %%ability_%selected_ability%%%
  %send% %actor% You order %self.name% to use: %ability_name%.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  return 1
  halt
end
~
#518
Owl Shadow commmands~
0 ct 0
order~
set arg1 %arg.car%
set arg %arg.cdr%
set arg2 %arg.car%
* discard the rest
if %actor.char_target(%arg1%)% != %self%
  return 0
  halt
end
if %actor% != %self.master%
  return 0
  halt
end
set ability_0 random
set ability_1 wings
set ability_2 healing
set ability_3 invigorating
set ability_4 brilliant
if %ability_1% /= %arg2%
  set selected_ability 1
elseif %ability_2% /= %arg2%
  set selected_ability 2
elseif %ability_3% /= %arg2%
  set selected_ability 3
elseif %ability_4% /= %arg2%
  set selected_ability 4
elseif random /= %arg2%
  set selected_ability 0
  remote selected_ability %self.id%
  %send% %actor% You order %self.name% to use abilities at random.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  halt
elseif status /= %arg2%
  set current_ability 0
  if %self.varexists(selected_ability)%
    set current_ability %self.selected_ability%
  end
  eval ability_name %%ability_%current_ability%%%
  %send% %actor% %self.name% is currently using: %ability_name%.
  %send% %actor% %self.name% has the following abilities available:
  %send% %actor% wings: Dark Wings (+dodge on tank)
  %send% %actor% healing: Healing Darkness (heal party over time)
  %send% %actor% invigorating: Invigorating Darkness (restore moves to party)
  %send% %actor% brilliant: Brilliant Darkness (+wits on party)
  halt
else
  * other command
  return 0
  halt
end
if %selected_ability%
  remote selected_ability %self.id%
  eval ability_name %%ability_%selected_ability%%%
  %send% %actor% You order %self.name% to use: %ability_name%.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  return 1
  halt
end
~
#519
Basilisk commands~
0 ct 0
order~
set arg1 %arg.car%
set arg %arg.cdr%
set arg2 %arg.car%
* discard the rest
if %actor.char_target(%arg1%)% != %self%
  return 0
  halt
end
if %actor% != %self.master%
  return 0
  halt
end
set ability_0 random
set ability_1 quartzite
set ability_2 basalt
set ability_3 granite
set ability_4 marble
if %ability_1% /= %arg2%
  set selected_ability 1
elseif %ability_2% /= %arg2%
  set selected_ability 2
elseif %ability_3% /= %arg2%
  set selected_ability 3
elseif %ability_4% /= %arg2%
  set selected_ability 4
elseif random /= %arg2%
  set selected_ability 0
  remote selected_ability %self.id%
  %send% %actor% You order %self.name% to use abilities at random.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  halt
elseif status /= %arg2%
  set current_ability 0
  if %self.varexists(selected_ability)%
    set current_ability %self.selected_ability%
  end
  eval ability_name %%ability_%current_ability%%%
  %send% %actor% %self.name% is currently using: %ability_name%.
  %send% %actor% %self.name% has the following abilities available:
  %send% %actor% marble: Marble gaze (+block on tank)
  %send% %actor% quartzite: Quartzite gaze (-damage on random enemy)
  %send% %actor% basalt: Basalt gaze (-wits on random enemy)
  %send% %actor% granite: Granite gaze (-tohit on random enemy)
  halt
else
  * other command
  return 0
  halt
end
if %selected_ability%
  remote selected_ability %self.id%
  eval ability_name %%ability_%selected_ability%%%
  %send% %actor% You order %self.name% to use: %ability_name%.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  return 1
  halt
end
~
#520
Salamander commands~
0 ct 0
order~
set arg1 %arg.car%
set arg %arg.cdr%
set arg2 %arg.car%
* discard the rest
if %actor.char_target(%arg1%)% != %self%
  return 0
  halt
end
if %actor% != %self.master%
  return 0
  halt
end
set ability_0 random
set ability_1 alchemy
set ability_2 soothing
set ability_3 guardian
set ability_4 surge
if %ability_1% /= %arg2%
  set selected_ability 1
elseif %ability_2% /= %arg2%
  set selected_ability 2
elseif %ability_3% /= %arg2%
  set selected_ability 3
elseif %ability_4% /= %arg2%
  set selected_ability 4
elseif random /= %arg2%
  set selected_ability 0
  remote selected_ability %self.id%
  %send% %actor% You order %self.name% to use abilities at random.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  halt
elseif status /= %arg2%
  set current_ability 0
  if %self.varexists(selected_ability)%
    set current_ability %self.selected_ability%
  end
  eval ability_name %%ability_%current_ability%%%
  %send% %actor% %self.name% is currently using: %ability_name%.
  %send% %actor% %self.name% has the following abilities available:
  %send% %actor% alchemy: Alchemist's Fire (short +bonus-healing on master)
  %send% %actor% soothing: Soothing flames (short heal-over-time on tank)
  %send% %actor% guardian: Guardian flames (short +dodge on tank)
  %send% %actor% surge: Ember surge (+damage on party)
  halt
else
  * other command
  return 0
  halt
end
if %selected_ability%
  remote selected_ability %self.id%
  eval ability_name %%ability_%selected_ability%%%
  %send% %actor% You order %self.name% to use: %ability_name%.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  return 1
  halt
end
~
#521
Banshee commands~
0 ct 0
order~
set arg1 %arg.car%
set arg %arg.cdr%
set arg2 %arg.car%
* discard the rest
if %actor.char_target(%arg1%)% != %self%
  return 0
  halt
end
if %actor% != %self.master%
  return 0
  halt
end
set ability_0 random
set ability_1 soul
set ability_2 terrify
set ability_3 heart
set ability_4 blood
if %ability_1% /= %arg2%
  set selected_ability 1
elseif %ability_2% /= %arg2%
  set selected_ability 2
elseif %ability_3% /= %arg2%
  set selected_ability 3
elseif %ability_4% /= %arg2%
  set selected_ability 4
elseif random /= %arg2%
  set selected_ability 0
  remote selected_ability %self.id%
  %send% %actor% You order %self.name% to use abilities at random.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  halt
elseif status /= %arg2%
  set current_ability 0
  if %self.varexists(selected_ability)%
    set current_ability %self.selected_ability%
  end
  eval ability_name %%ability_%current_ability%%%
  %send% %actor% %self.name% is currently using: %ability_name%.
  %send% %actor% %self.name% has the following abilities available:
  %send% %actor% soul: Soul-shattering wail (-resist on all enemies)
  %send% %actor% terrify: Terrifying wail (-tohit on all enemies)
  %send% %actor% heart: Heart-wrenching wail (-damage on all enemies)
  %send% %actor% blood: Blood-curdling wail (magical DoT on all enemies)
  halt
else
  * other command
  return 0
  halt
end
if %selected_ability%
  remote selected_ability %self.id%
  eval ability_name %%ability_%selected_ability%%%
  %send% %actor% You order %self.name% to use: %ability_name%.
  %echoaround% %actor% %actor.name% gives %self.name% an order.
  return 1
  halt
end
~
$
