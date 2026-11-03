#18100
Lumberjack chop~
0 ab 10 30
L h 0
L h 1
L h 2
L h 3
L h 4
L h 23
L h 26
L h 36
L h 42
L h 43
L h 44
L h 45
L h 46
L h 54
L h 59
L h 90
L h 210
L h 211
L h 220
L h 221
L h 222
L h 224
L h 232
L h 233
L h 10562
L h 10563
L h 10564
L h 10565
L h 10566
L j 18100
~
if %self.fighting%
  halt
end
set logs %self.var(logs,0)%
set rough %self.var(rough,0)%
set trop %self.var(trop,0)%
set evergreen %self.var(evergreen,0)%
set room %self.room%
set gohome 0
if (%room.template% == 18100)
  set gohome 1
end
if %self.room.distance(%instance.location%)% > 8 && !%self.mob_flagged(SENTINEL)%
  set gohome 1
end
if %room.sector_vnum% >= 1 && %room.sector_vnum% <= 4
  * temperate
  nop %self.add_mob_flag(SENTINEL)%
  eval new_sector %room.sector_vnum% - 1
  if %new_sector% == 0
    set new_sector 36
  end
  %terraform% %room% %new_sector%
  eval logs %logs% + 1
  if %room.sector_vnum% == 1
    %echo% ~%self% fells the last tree with a mighty crash!
  else
    %echo% ~%self% fells a tree with a mighty crash!
  end
elseif %room.sector_vnum% == 90
  * old-growth -> overgrown
  nop %self.add_mob_flag(SENTINEL)%
  %terraform% %room% 4
  eval logs %logs% + 2
  %echo% ~%self% fells a tree with a mighty crash!
elseif %room.sector_vnum% == 26
  * grove
  nop %self.add_mob_flag(SENTINEL)%
  %echo% ~%self% fells two trees!
  %terraform% %room% 23
  eval rough %rough% + 2
  wait 1 sec
  %echo% The trees fall into each other with a single mighty crash!
  cackle
elseif %room.sector_vnum% >= 42 && %room.sector_vnum% <= 45
  * riverside
  nop %self.add_mob_flag(SENTINEL)%
  eval new_sector %room.sector_vnum% - 1
  if %room.sector_vnum% == 44
    set new_sector 46
  end
  %terraform% %room% %new_sector%
  eval logs %logs% + 1
  if %room.sector_vnum% == 42 || %room.sector_vnum% == 44
    %echo% ~%self% fells the last tree with a mighty crash!
  else
    %echo% ~%self% fells a tree with a mighty crash!
  end
elseif %room.sector_vnum% == 54
  * shoreside
  nop %self.add_mob_flag(SENTINEL)%
  set new_sector 59
  %terraform% %room% %new_sector%
  eval logs %logs% + 1
  %echo% ~%self% fells the tree with a mighty crash!
elseif %room.sector_vnum% == 210
  * savanna
  nop %self.add_mob_flag(SENTINEL)%
  set new_sector 211
  %terraform% %room% %new_sector%
  eval rough %rough% + 1
  %echo% ~%self% fells the tree with a mighty crash!
elseif %room.sector_vnum% == 220 || %room.sector_vnum% == 221 || %room.sector_vnum% == 224
  * jungle
  nop %self.add_mob_flag(SENTINEL)%
  if %random.2% == 2
    if %room.sector_vnum% == 220
      set new_sector 221
    else
      set new_sector 222
    end
    %terraform% %room% %new_sector%
  end
  eval trop %trop% + 1
  if %room.sector_vnum% == 222
    %echo% ~%self% fells the last tree with a mighty crash!
  else
    %echo% ~%self% fells a tree with a mighty crash!
  end
elseif %room.sector_vnum% == 232
  * savanna
  nop %self.add_mob_flag(SENTINEL)%
  set new_sector 233
  %terraform% %room% %new_sector%
  eval trop %trop% + 1
  %echo% ~%self% fells the last tree with a mighty crash!
elseif %room.sector_vnum% >= 10562 && %room.sector_vnum% <= 10565
  * evergreen
  nop %self.add_mob_flag(SENTINEL)%
  eval new_sector %room.sector_vnum% - 1
  if %new_sector% == 10561
    set new_sector 10566
  end
  %terraform% %room% %new_sector%
  eval evergreen %evergreen% + 1
  if %room.sector_vnum% == 1
    %echo% ~%self% fells the last tree with a mighty crash!
  else
    %echo% ~%self% fells a tree with a mighty crash!
  end
else
  * Tile is clear, can wander now
  nop %self.remove_mob_flag(SENTINEL)%
end
wait 1 sec
if (%gohome% && %instance.location%)
  %echo% ~%self% heads back to ^%self% camp!
  %teleport% %self% %instance.location%
  %echo% ~%self% returns to the camp!
end
remote logs %self.id%
remote rough %self.id%
remote trop %self.id%
remote evergreen %self.id%
~
#18101
Lumberjack drop logs~
0 f 100 5
L c 124
L c 126
L c 129
L c 10558
L c 18106
~
* logs
set logs %self.var(logs,0)%
if %logs% > 75
  set logs 75
end
while %logs% >= 5
  %load% obj 124
  %load% obj 124
  %load% obj 124
  %load% obj 124
  %load% obj 124
  eval logs %logs% - 5
done
while %logs% > 0
  %load% obj 124
  eval logs %logs% - 1
done
*
* rough wood
set rough %self.var(rough,0)%
if %rough% > 75
  set rough 75
end
while %rough% >= 5
  %load% obj 126
  %load% obj 126
  %load% obj 126
  %load% obj 126
  %load% obj 126
  eval rough %rough% - 5
done
while %rough% > 0
  %load% obj 126
  eval rough %rough% - 1
done
*
* tropical
set trop %self.var(trop,0)%
if %trop% > 75
  set trop 75
end
while %trop% >= 5
  %load% obj 129
  %load% obj 129
  %load% obj 129
  %load% obj 129
  %load% obj 129
  eval trop %trop% - 5
done
while %trop% > 0
  %load% obj 129
  eval trop %trop% - 1
done
*
* evergreens
set evergreen %self.var(evergreen,0)%
if %evergreen% > 75
  set evergreen 75
end
while %evergreen% >= 5
  %load% obj 10558
  %load% obj 10558
  %load% obj 10558
  %load% obj 10558
  %load% obj 10558
  eval evergreen %evergreen% - 5
done
while %evergreen% > 0
  %load% obj 10558
  eval evergreen %evergreen% - 1
done
*
if !%instance.start%
  halt
end
* Load the goblin retreat timer item
%at% %instance.start% %load% obj 18106
~
#18102
Goblin lumberjack combat~
0 k 10 0
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
0 bw 50 0
~
* This mob is only flagged SENTINEL when it's chopping
if (%self.mob_flagged(SENTINEL)% && !%self.fighting%)
  switch %random.3%
    case 1
      %echo% ~%self% swings ^%self% axe hard into a tree!
    break
    case 2
      %echo% ~%self% hacks wildly at a tree with ^%self% axe!
    break
    case 3
      %echo% ~%self% flails furiously at a tree, screaming loudly!
    break
  done
end
return 0
~
#18104
Goblin camp cleanup~
2 e 100 40
L h 0
L h 1
L h 2
L h 3
L h 4
L h 36
L h 37
L h 38
L h 39
L h 40
L h 41
L h 42
L h 43
L h 44
L h 45
L h 46
L h 47
L h 54
L h 59
L h 60
L h 70
L h 71
L h 72
L h 73
L h 74
L h 75
L h 76
L h 77
L h 78
L h 79
L h 210
L h 211
L h 212
L h 220
L h 221
L h 222
L h 223
L h 224
L h 232
L h 233
~
* Replace the camp with stumps
set vnum %room.base_sector_vnum%
if %vnum% <= 4 || (%vnum% >= 36 && %vnum% <= 39)
  * temperate
  %terraform% %room% 36
elseif (%vnum% >= 40 && %vnum% <= 47)
  * temperate riverbank
  %terraform% %room% 46
elseif %vnum% == 54 || %vnum% == 59 || %vnum% == 60
  * shoreside tree
  %terraform% %room% 59
elseif (%vnum% >= 70 && %vnum% <= 79)
  * irrigation
  %terraform% %room% 70
elseif %vnum% == 210 || %vnum% == 212
  * savanna
  %terraform% %room% 211
elseif %vnum% >= 220 && %vnum% <= 224
  * jungle
  %terraform% %room% 222
elseif %vnum% == 232
  * mangrove
  %terraform% %room% 233
else
  * don't terraform anything else
end
~
#18106
Goblin camp despawn timer~
1 f 0 0
~
%adventurecomplete%
~
$
