#505
Moon Rabbit Buffs~
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
Spirit Wolf Debuffs~
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
$
