#18100
Lumberjack chop~
0 ab 100
~
if %self.fighting%
  halt
end
if !%self.varexists(logs)%
  eval logs 1
  remote logs %self.id%
end
eval logs %self.logs%
eval room %self.room%
eval gohome 0
if (%room.template% == 18100)
  eval gohome 1
end
if (%room.sector% ~= Forest || %room.sector% ~= Jungle || %room.sector% == Grove)
  * Stops the mob from wandering while it works on this tile
  nop %self.add_mob_flag(SENTINEL)%
  %echo% %self.name% swings %self.hisher% axe hard into a tree!
  if (%random.4% == 4)
    if (%room.sector% == Light Forest)
      %echo% The last tree falls with a mighty crash!
      %terraform% %room% 0
      eval gohome 1
      eval logs %logs% + 1
    elseif (%room.sector% == Forest)
      %echo% The tree falls with a mighty crash!
      %terraform% %room% 1
      eval logs %logs% + 1
    elseif (%room.sector% == Shady Forest)
      %echo% The tree falls with a mighty crash!
      %terraform% %room% 2
      eval logs %logs% + 1
    elseif (%room.sector% == Overgrown Forest)
      %echo% The tree falls with a mighty crash!
      %terraform% %room% 3
      eval logs %logs% + 1
    elseif (%room.sector% == Grove)
      %echo% Two trees fall into each other with a mighty crash!
      %terraform% %room% 20
      eval logs %logs% + 2
      eval gohome 1
      cackle
    elseif (%room.sector% == Jungle)
      %echo% The tree falls with a mighty crash!
      if (%random.3 == 3%)
        %terraform% %room% 27
      end
      eval logs %logs% + 1
    elseif (%room.sector% == Light Jungle)
      if (%random.2% == 2)
        %echo% The last tree falls with a mighty crash!
        %terraform% %room% 0
        eval gohome 1
      else
        %echo% The tree falls with a mighty crash!
      end
      eval logs %logs% + 1
    end
  end
else
* Tile is clear, can wander now
  nop %self.remove_mob_flag(SENTINEL)%
end
remote logs %self.id%
wait 1 sec
if (%gohome% && %instance.location%)
  %echo% %self.name% heads back to %self.hisher% camp!
  %teleport% %self% %instance.location%
  %echo% %self.name% returns to the camp!
end
~
#18101
Lumberjack drop logs~
0 f 100
~
eval loot 124
eval i 0
eval logs %self.logs%
if %logs% > 50
  eval logs 50
end
while %i% < %logs%
  eval i %i% + 1
  %load% obj %loot%
done
~
#18102
Goblin lumberjack combat~
0 k 10
~
switch %random.3%
  case 1
    kick
  break
  case 2
    blind
  break
  case 3
    outrage
  break
done
~
$
