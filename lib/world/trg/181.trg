#18100
Lumberjack chop~
0 ab 10
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
eval cap 30
if (%room.template% == 18100)
  eval gohome 1
end
if (%random.10% == 10 && !%self.mob_flagged(SENTINEL)%
  eval gohome 1
end
if (%room.sector% ~= Forest || %room.sector% == Grove) && (%logs% < %cap%) && (!%room.empire_id%)
  * Stops the mob from wandering while it works on this tile
  nop %self.add_mob_flag(SENTINEL)%
  if (%room.sector% == Light Forest)
    %echo% %self.name% fells the last tree with a mighty crash!
    %terraform% %room% 0
    eval logs %logs% + 1
  elseif (%room.sector% == Forest)
    %echo% %self.name% fells a tree with a mighty crash!
    %terraform% %room% 1
    eval logs %logs% + 1
  elseif (%room.sector% == Shady Forest)
    %echo% %self.name% fells a tree with a mighty crash!
    %terraform% %room% 2
    eval logs %logs% + 1
  elseif (%room.sector% == Overgrown Forest)
    %echo% %self.name% fells a tree with a mighty crash!
    %terraform% %room% 3
    eval logs %logs% + 1
  elseif (%room.sector% == Grove)
    %echo% %self.name% fells two trees!
    %terraform% %room% 20
    eval logs %logs% + 2
    wait 1 sec
    %echo% The trees fall into each other with a single mighty crash!
    cackle
  end
else
  * Tile is clear, can wander now
  nop %self.remove_mob_flag(SENTINEL)%
end
wait 1 sec
if (%gohome% && %instance.location%)
  %echo% %self.name% heads back to %self.hisher% camp!
  %teleport% %self% %instance.location%
  %echo% %self.name% returns to the camp!
  if (%logs% > 15 && %random.5%==5)
    %echo% %self.name% drops off a log at the camp.
    eval logs %logs%-1
  end
end
remote logs %self.id%
~
#18101
Lumberjack drop logs~
0 f 100
~
if !%self.varexists(logs)%
  eval logs 1
  remote logs %self.id%
end
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
if !%instance.start%
  halt
end
* Load the goblin retreat timer item
%at% %instance.start% %load% obj 18106
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
#18103
Goblin lumberjack environmental~
0 bw 50
~
* This mob is only flagged SENTINEL when it's chopping
if (%self.mob_flagged(SENTINEL)% && !%self.fighting%)
  switch %random.3%
    case 1
      %echo% %self.name% swings %self.hisher% axe hard into a tree!
    break
    case 2
      %echo% %self.name% hacks wildly at a tree with %self.hisher% axe!
    break
    case 3
      %echo% %self.name% flails furiously at a tree, screaming loudly!
    break
  done
end
return 0
~
#18104
Goblin camp cleanup~
2 e 100
~
* Replace the camp with plains
%terraform% %room% 0
~
#18106
Goblin camp despawn timer~
1 f 0
~
%adventurecomplete%
~
$
