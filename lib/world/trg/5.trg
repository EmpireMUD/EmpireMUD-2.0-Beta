#505
Moon Rabbit Familiar Buffs~
0 kt 5
~
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    eval targ %self.master%
    if !%targ%
      %echo% %self.name% glows and purrs.
      halt
    end
    %send% %targ% %self.name% shines brightly at you, and you feel replenished!
    %echoaround% %targ% %self.name% shines brightly at %targ.name%, who looks replenished!
    eval amount %self.level% / 3
    eval adjust %%targ.mana(%amount%)%%
    nop %adjust%
  break
  case 2
    eval targ %self.master%
    if !%targ%
      %echo% %self.name% glows and purrs.
      halt
    end
    %send% %targ% %self.name% shines brightly at you, and you feel re-energized!
    %echoaround% %targ% %self.name% shines brightly at %targ.name%, who looks re-energized!
    eval amount %self.level% / 20
    dg_affect %targ% MANA-REGEN %amount% 30
  break
  case 3
    %echo% %self.name% shines as brightly as the moon, lighting up the whole party!
    eval amount %self.level% / 20
    eval room %self.room%
    eval ch %room.people%
    while %ch%
      eval test %%self.is_ally(%ch%)%%
      if %test%
        %send% %ch% You bask in %self.name%'s glow!
        dg_affect %ch% RESIST-PHYSICAL %amount% 30
        dg_affect %ch% RESIST-MAGICAL %amount% 30
      end
      eval ch %ch.next_in_room%
    done
  break
  case 4
    eval enemy %self.fighting%
    if !%enemy%
      %echo% %self.name% glows and purrs.
      halt
    end
    eval targ %enemy.fighting%
    eval test %%self.is_ally(%targ%)%%
    if (!%targ% || !%test%)
      %echo% %self.name% glows and purrs.
      halt
    end
    %send% %targ% %self.name% shines brightly at you, and you feel the moon's protection!
    %echoaround% %targ% %self.name% shines brightly at %targ.name%, protecting %targ.himher%!
    eval amount %self.level% / 10
    dg_affect %targ% RESIST-PHYSICAL %amount% 30
    dg_affect %targ% RESIST-MAGICAL %amount% 30
  break
done
~
#507
Spirit Wolf Familiar Debuffs~
0 kt 5
~
if %self.disabled%
  halt
end
eval targ %random.enemy%
if !%targ%
  %echo% You feel a chill as %self.name%'s howl echoes out through the air!
  halt
end
switch %random.4%
  case 1
    %send% %targ% %self.name% flashes brightly and shoots a bolt of lightning at you!
    %echoaround% %targ% %self.name% flashes brightly and shoots a bolt of lighting at %targ.name%!
    dg_affect %targ% DEXTERITY -2 30
  break
  case 2
    %send% %targ% %self.name% howls, followed by a clap of thunder, deafening your ears!
    %echoaround% %targ% %self.name% howls, followed by a clap of thunder, and %targ.name% looks deafened!
    dg_affect %targ% DODGE -20 30
  break
  case 3
    %send% %targ% %self.name% barks at you, and your vision begins to blur!
    %echoaround% %targ% %self.name% barks at %targ.name%, who squints as if %targ.heshe%'s having trouble seeing!
    dg_affect %targ% TO-HIT -20 30
  break
  case 4
    %send% %targ% %self.name% bites into you, your skin sizzling from the ghost energy!
    %echoaround% %targ% %self.name% bites into %targ.name%, %targ.hisher% skin sizzling from the ghost energy!
    %dot% %targ% 100 30 magical 1
  break
done
~
#509
Phoenix Familiar Buffs~
0 kt 5
~
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    eval targ %self.master%
    if !%targ%
      %echo% %self.name% flickers and burns.
      halt
    end
    %send% %targ% Fire from %self.name% spreads over you, and you blaze with power!
    %echoaround% %targ% Fire from %self.name% spreads over %targe.name%, who blazes with power!
    eval amount %self.level% / 20
    dg_affect %targ% BONUS-HEALING %amount% 30
  break
  case 2
    eval enemy %self.fighting%
    if !%enemy%
      %echo% %self.name% flickers and burns.
      halt
    end
    eval targ %enemy.fighting%
    eval test %%self.is_ally(%targ%)%%
    if (!%targ% || !%test%)
      %echo% %self.name% flickers and burns.
      halt
    end
    %send% %targ% %self.name% flies into your chest, and you feel a healing warmth!
    %echoaround% %targ% %self.name% flies into %targ.name%'s chest and glows outward with a healing warmth!
    %damage% %targ% -100
  break
  case 3
    %echo% %self.name% bursts into flames, sending a healing fire through the party!
    eval room %self.room%
    eval ch %room.people%
    while %ch%
      eval test %%self.is_ally(%ch%)%%
      if %test%
        %send% %ch% You feel warmed by %self.name%'s fire!
        %damage% %ch% -50
      end
      eval ch %ch.next_in_room%
    done
  break
  case 4
    %echo% %self.name% bursts into flames, inspiring burning passion in the party!
    eval amount %self.level% / 30
    eval room %self.room%
    eval ch %room.people%
    while %ch%
      eval test %%self.is_ally(%ch%)%%
      if %test%
        %send% %ch% You feel inspired by %self.name%'s fire!
        dg_affect %ch% BONUS-MAGICAL %amount% 30
        dg_affect %ch% BONUS-PHYSICAL %amount% 30
      end
      eval ch %ch.next_in_room%
    done
  break
done
~
#510
Scorpion Shadow Familiar Debuffs~
0 kt 5
~
if %self.disabled%
  halt
end
eval master %self.master%
if !%master%
  %echo% %self.name% shrinks into the shadows and vanishes.
  %purge% %self%
  halt
end
eval targ %random.enemy%
if !%targ%
  %echo% %master.name%'s scorpion shadow twists and coils from the darkness.
  halt
end
switch %random.4%
  case 1
    %send% %targ% %master.name%'s scorpion shadow stings you with a creeping venom!
    %send% %master% Your scorpion shadow stings %targ.name% with a creeping venom!
    %echoneither% %targ% %master% %master.name%'s scorpion shadow stings %targ.name% with a creeping venom!
    dg_affect %targ% SLOW ON 30
  break
  case 2
    %send% %targ% %master.name%'s scorpion shadow stings you with shadow venom!
    %send% %master% Your scorpion shadow stings %targ.name% with shadow venom!
    %echoneither% %targ% %master% %master.name%'s scorpion shadow stings %targ.name% with shadow venom!
    eval amount -1 * %self.level% / 20
    dg_affect %targ% BONUS-PHYSICAL %amount% 30
    dg_affect %targ% BONUS-MAGICAL %amount% 30
  break
  case 3
    %send% %targ% %master.name%'s scorpion shadow stings you with numbing venom!
    %send% %master% Your scorpion shadow stings %targ.name% with a numbing venom!
    %echoneither% %targ% %master% %master.name%'s scorpion shadow stings %targ.name% with numbing venom!
    dg_affect %targ% WITS -4 30
  break
  case 4
    %send% %targ% %master.name%'s scorpion shadow stings you with agonizing venom!
    %send% %master% Your scorpion shadow stings %targ.name% with agonizing venom!
    %echoneither% %targ% %master% %master.name%'s scorpion shadow stings %targ.name% with agonizing venom!
    %dot% %targ% 100 30 poison 1
  break
done
~
#511
Owl Shadow Familiar Buffs~
0 kt 5
~
if %self.disabled%
  halt
end
eval master %self.master%
if !%master%
  %echo% %self.name% shrinks into the shadows and vanishes.
  %purge% %self%
  halt
end
eval type %random.4%
* only one of these does not target party
if (%type% == 4)
  eval enemy %self.fighting%
  if !%enemy%
    %echoaround% %master% %master.name%'s shadow seems to flap its wings.
    halt
  end
  eval targ %enemy.fighting%
  eval test %%self.is_ally(%targ%)%%
  if (!%targ% || !%test%)
    %echoaround% %master% %master.name%'s shadow seems to flap its wings.
    halt
  end
  %send% %targ% %master.name%'s owl shadow wraps its dark wings around you, protecting you!
  %send% %master% Your owl shadow wraps its dark wings around %targ.name%, protecting %targ.himher%!
  %echoneither% %targ% %master% %master.name%'s owl shadow wraps its dark wings around %targ.name%, protecting %targ.himher%!
  dg_affect %targ% DODGE 20 30
  halt
end
* random results 1-3
%send% %master% Your owl shadow wraps itself around the party!
%echoaround% %master% %master.name%'s owl shadow wraps itself around the party!
eval room %self.room%
eval ch %room.people%
while %ch%
  eval test %%self.is_ally(%ch%)%%
  if %test%
    switch %type%
      case 1
        %send% %ch% You feel the healing darkness cure your injuries!
        eval amount %self.level% / 20
        dg_affect %ch% HEAL-OVER-TIME %amount% 30
      break
      case 2
        %send% %ch% You feel the invigorating darkness restore your stamina!
        eval amount %self.level% / 5
        eval adjust %%targ.move(%amount%)%%
        nop %adjust%
      break
      case 3
        %send% %ch% You feel the brilliant darkness boost your speed!
        dg_affect %ch% WITS 2 30
      break
    done
  end
  eval ch %ch.next_in_room%
done
~
#512
Basilisk Familiar Debuffs~
0 kt 5
~
if %self.disabled%
  halt
end
eval type %random.4%
* one type hits tank only
if (%type% == 4)
  eval enemy %self.fighting%
  if !%enemy%
    %echo% %self.name% flicks its tongue and whips its tail.
    halt
  end
  eval targ %enemy.fighting%
  eval test %%self.is_ally(%targ%)%%
  if (!%targ% || !%test%)
    %echo% %self.name% flicks its tongue and whips its tail.
    halt
  end
  %send% %targ% %self.name% unleases its marble gaze upon you, hardening your form against attacks!
  %echoaround% %targ% %self.name% unleashes its marble gaze upon %targ.name%, hardening %targ.hisher% form against attacks!
  dg_affect %targ% BLOCK 20 30
  halt
end
* other 3 types hit the enemy
eval targ %random.enemy%
if !%targ%
  %echo% %self.name% flicks its tongue and whips its tail.
  halt
end
switch %type%
  case 1
    %send% %targ% %self.name% unleashes its quartzite gaze upon you, turning you partially to stone!
    %echoaround% %targ% %self.name% unleashes its quartzite gaze upon %targ.name%, turning %targ.himher% partially to stone!
    eval amount -1 * %self.level% / 20
    dg_affect %targ% BONUS-PHYSICAL %amount% 30
    dg_affect %targ% BONUS-MAGICAL %amount% 30
  break
  case 2
    %send% %targ% %self.name% unleashes its basalt gaze upon you, turning you partially to stone!
    %echoaround% %targ% %self.name% unleashes its basalt gaze upon %targ.name%, turning %targ.himher% partially to stone!
    dg_affect %targ% WITS -4 30
  break
  case 3
    %send% %targ% %self.name% unleashes its granite gaze upon you, turning you partially to stone!
    %echoaround% %targ% %self.name% unleashes its granite gaze upon %targ.name%, turning %targ.himher% partially to stone!
    dg_affect %targ% TO-HIT -20 30
  break
done
~
#513
Salamander Familiar Buffs~
0 kt 5
~
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    eval targ %self.master%
    if !%targ%
      %echo% %self.name% sizzles and simmers.
      halt
    end
    %send% %targ% %self.name% coils around you, granting Alchemist's Fire to your healing spells!
    %echoaround% %targ% %self.name% coils around %targ.name%, granting %targ.himher% Alchemist's Fire!
    eval amount %self.level% / 4
    dg_affect %targ% BONUS-HEALING %amount% 10
  break
  case 2
    eval enemy %self.fighting%
    if !%enemy%
      %echo% %self.name% sizzles and simmers.
      halt
    end
    eval targ %enemy.fighting%
    eval test %%self.is_ally(%targ%)%%
    if (!%targ% || !%test%)
      %echo% %self.name% sizzles and simmers.
      halt
    end
    %send% %targ% %self.name% breathes its soothing flames upon you, and your wounds begin to heal!
    %echoaround% %targ% %self.name% breathes its soothing flames upon %targ.name%, whose wounds begin to heal!
    eval amount %self.level% / 4
    dg_affect %targ% HEAL-OVER-TIME %amount% 10
  break
  case 3
    eval enemy %self.fighting%
    if !%enemy%
      %echo% %self.name% sizzles and simmers.
      halt
    end
    eval targ %enemy.fighting%
    eval test %%self.is_ally(%targ%)%%
    if (!%targ% || !%test%)
      %echo% %self.name% sizzles and simmers.
      halt
    end
    %send% %targ% %self.name% breathes its guardian flames upon you, protecting you!
    %echoaround% %targ% %self.name% breathes its guardian flames upon %targ.name%, protecting %targ.himher%!
    dg_affect %targ% DODGE 20 30
  break
  case 4
    %echo% %self.name% sputters and throws embers out at the whole party!
    eval amount %self.level% / 30
    eval room %self.room%
    eval ch %room.people%
    while %ch%
      eval test %%self.is_ally(%ch%)%%
      if %test%
        %send% %ch% You feel yourself surge in the embers' glow!
        dg_affect %ch% BONUS-MAGICAL %amount% 30
        dg_affect %ch% BONUS-PHYSICAL %amount% 30
      end
      eval ch %ch.next_in_room%
    done
  break
done
~
#515
Banshee Familiar Debuffs~
0 kt 5
~
if %self.disabled%
  halt
end
eval type %random.4%
switch %type%
  case 1
    %echo% %self.name% lets out a soul-shattering wail!
    eval amount -1 * %self.level% / 20
  break
  case 2
    %echo% %self.name% lets out a terrifying wail!
  break
  case 3
    %echo% %self.name% lets out a heart-wrenching wail!
    eval amount -1 * %self.level% / 30
  break
  case 4
    %echo% %self.name% lets out a blood-curdling wail!
  break
done
eval room %self.room%
eval ch %room.people%
while %ch%
  eval test %%self.is_enemy(%ch%)%%
  if %test%
    %send% %ch% You feel the banshee's wail strike deep into your heart!
    switch %type%
      case 1
        dg_affect %ch% RESIST-MAGICAL %amount% 30
        dg_affect %ch% RESIST-PHYSICAL %amount% 30
      break
      case 2
        dg_affect %ch% TO-HIT -10 30
      break
      case 3
        dg_affect %ch% BONUS-MAGICAL %amount% 30
        dg_affect %ch% BONUS-PHYSICAL %amount% 30
      break
      case 4
        %dot% %ch% 50 30 magical 1
      break
    done
  end
  eval ch %ch.next_in_room%
done
~
$
