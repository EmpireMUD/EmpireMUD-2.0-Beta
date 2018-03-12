#18100
Lumberjack chop~
0 ab 10
~
if %self.fighting%
  halt
end
if !%self.varexists(logs)%
  set logs 1
  remote logs %self.id%
end
set logs %self.logs%
set room %self.room%
set gohome 0
set cap 30
if (%room.template% == 18100)
  set gohome 1
end
if (%random.10% == 10 && !%self.mob_flagged(SENTINEL)%
  set gohome 1
end
if %room.sector_vnum% >= 1 && %room.sector_vnum% <= 4
  nop %self.add_mob_flag(SENTINEL)%
  eval new_sector %room.sector_vnum% - 1
  if %new_sector% == 0
    set new_sector 18100
  end
  %terraform% %room% %new_sector%
  eval logs %logs% + 1
  if %room.sector_vnum% == 1
    %echo% %self.name% fells the last tree with a mighty crash!
  else
    %echo% %self.name% fells a tree with a mighty crash!
  end
elseif %room.sector_vnum% == 26
  nop %self.add_mob_flag(SENTINEL)%
  %echo% %self.name% fells two trees!
  %terraform% %room% 20
  eval logs %logs% + 2
  wait 1 sec
  %echo% The trees fall into each other with a single mighty crash!
  cackle
elseif %room.sector_vnum% >= 42 && %room.sector_vnum% <= 45
  nop %self.add_mob_flag(SENTINEL)%
  eval new_sector %room.sector_vnum% - 1
  if %room.sector_vnum% == 44
    set new_sector 40
  end
  %terraform% %room% %new_sector%
  eval logs %logs% + 1
  if %room.sector_vnum% == 42 || %room.sector_vnum% == 44
    %echo% %self.name% fells the last tree with a mighty crash!
  else
    %echo% %self.name% fells a tree with a mighty crash!
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
  set logs 1
  remote logs %self.id%
end
set loot 124
set i 0
set logs %self.logs%
if %logs% > 50
  set logs 50
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
