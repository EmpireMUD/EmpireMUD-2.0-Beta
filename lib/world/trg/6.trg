#600
Seed enchantment~
1 c 2
enchant~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set cost_item_name a prismatic crystal
set cost_item_vnum 602
set cost_item_count 1
set ability_required Enchant Tools
set mana_cost 100
set result_vnum 601
if !%actor.ability(%ability_required%)%
  %send% %actor% You need %ability_required% to enchant %self.shortdesc%.
  return 1
  halt
end
if %actor.mana% < %mana_cost%
  %send% %actor% You need %mana_cost% mana to enchant %self.shortdesc%.
  return 1
  halt
end
if !%actor.has_resources(%cost_item_vnum%, %cost_item_count%)%
  %send% %actor% You need %cost_item_name% (x%cost_item_count%) to enchant %self.shortdesc%.
  return 1
  halt
end
nop %actor.add_resources(%cost_item_vnum%,-%cost_item_count%)%
%send% %actor% You enchant %self.shortdesc%!
%echoaround% %actor% %actor.name% enchants %self.shortdesc%!
nop %actor.mana(-%mana_cost%)%
%load% obj %result_vnum% %actor% inv
set obj %actor.inventory()%
%send% %actor% It becomes %obj.shortdesc%!
%echoaround% %actor% It becomes %obj.shortdesc%!
%purge% %self%
~
#601
Enchanted seed plant - desert (default)~
1 c 2
plant~
* default script for obj 601: can only make enchanted forests in the desert
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %self.room%
if !%actor.canuseroom_member()%
  %send% %actor% You don't have permission to plant anything here.
  return 1
  halt
end
if %room.sector_vnum% != 26
  %send% %actor% You can only plant %self.shortdesc% in a desert grove.
  return 1
  halt
end
%send% %actor% You dig a hole and plant %self.shortdesc% in it.
%echoaround% %actor% %actor.name% digs a hole and plant %self.shortdesc% in it.
%echo% The trees around you twist and turn and take on a strange violet hue.
%terraform% %room% 610
%purge% %self%
~
#602
Enchanted forest unclaimed chop~
0 ab 100
~
set room %self.room%
set cycles_left 5
while %cycles_left% >= 0
  if (%self.room% != %room%) || %room.empire% || !(%room.sector% ~= Enchanted || %room.sector% ~= Weirdwood)
    * We've either moved or the room's no longer suitable for deforesting - despawn the mob
    %echo% %self.name%, seeing no opportunity for destruction here, starts wandering away.
    nop %self.add_mob_flag(SPAWNED)%
    nop %self.remove_mob_flag(SENTINEL)%
    detach 602 %self.id%
    halt
  end
  if %self.fighting% || %self.disabled%
    * Combat interrupts the ritual
    %echo% %self.name%'s ritual is interrupted.
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echo% %self.name% plants %self.hisher% skull staff into the ground...
    break
    case 4
      %echo% %self.name% mutters an unspeakable word, killing the undergrowth...
    break
    case 3
      %echo% %self.name% sends out a shockwave of death magic, withering the leaves on the trees...
    break
    case 2
      %echo% %self.name% draws a death sigil on the ground, driving away the animals...
    break
    case 1
      %echo% %self.name% chants a dark incantation, rotting the bark of the trees...
    break
    case 0
      %echo% %self.name% completes %self.hisher% ritual, killing the forest!
      * terraform based on sector
      if %room.sector_vnum% >= 600 && %room.sector_vnum% <= 604
        %terraform% %room% 605
      elseif %room.sector_vnum% >= 610 && %room.sector_vnum% <= 614
        %terraform% %room% 615
      else
        * not a sector we can finish
        nop %self.add_mob_flag(SPAWNED)%
      end
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
%echo% %self.name% looks confused...
~
#603
Enchanted seed plant - desert AND plains~
1 c 2
plant~
* If you prefer both desert AND temperate enchanted forests, put this script on obj vnum 601 instead of its normal one
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %self.room%
if !%actor.canuseroom_member()%
  %send% %actor% You don't have permission to plant anything here.
  return 1
  halt
end
if (%room.sector_vnum% != 0 && %room.sector_vnum% != 26)
  %send% %actor% You can only plant %self.shortdesc% on open plains or in a desert grove.
  return 1
  halt
end
%send% %actor% You dig a hole and plant %self.shortdesc% in it.
%echoaround% %actor% %actor.name% digs a hole and plant %self.shortdesc% in it.
%echo% Dozens of strange saplings spring up!
if %room.sector_vnum% == 26
  %terraform% %room% 610
else
  %terraform% %room% 600
end
%purge% %self%
~
#604
Enchanted seed plant - Temperate Only~
1 c 2
plant~
* If your MUD has too few deserts, put this script on obj vnum 601 instead of its normal one
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %self.room%
if !%actor.canuseroom_member()%
  %send% %actor% You don't have permission to plant anything here.
  return 1
  halt
end
if %room.sector_vnum% != 0
  %send% %actor% You can only plant %self.shortdesc% on open plains.
  return 1
  halt
end
%send% %actor% You dig a hole and plant %self.shortdesc% in it.
%echoaround% %actor% %actor.name% digs a hole and plant %self.shortdesc% in it.
%echo% Dozens of strange saplings spring up!
%terraform% %room% 600
%purge% %self%
~
#610
Talking Horse script~
0 bw 10
~
* This script is no longer in use, and has been replaced with custom messages.
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    say I've been trying to buy a bamboo harvester, but nobody wants to talk to a horse.
  break
  case 2
    say Why the short face?
  break
  case 3
    say Of course, of course.
  break
  case 4
    say Have you seen Wilbur?
  break
done
~
#611
Talking Warhorse script~
0 bw 10
~
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    %echo% %self.name% coughs loudly.
    wait 5
    say It's okay, I'm just a little hoarse.
  break
  case 2
    say This armor makes me look like a real knight mare.
  break
  case 3
    say Do you know 'The Whip and the Neigh Neigh'?
  break
  case 4
    say I escaped from Morpurgo for this?
  break
done
~
#612
Talking Bunny script~
0 bw 10
~
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    say What's up, doc?
  break
  case 2
    %echo% %self.name% thumps %self.hisher% foot on the ground.
  break
  case 3
    if %self.varexists(has_duped)%
      set has_duped %self.has_duped%
    else
      set has_duped 0
    end
    if %has_duped% > 0
      * has already duplicated
      %echo% %self.name% curls up, becomes a ball of dust, and blows away in the wind.
      %purge% %self%
      halt
    else
      set room %self.room%
      * only duplicate if not in a building
      if (!%room.building%)
        %echo% %self.name% does a little backflip and splits into two bunnies!
        %load% mob 612
        set has_duped 1
        remote has_duped %self.id%
      else
        say I'm late! I'm late!
      end
    end
  break
  case 4
    say If you can't say something nice... don't say nothing at all.
  break
done
~
#613
Singing Bear script~
0 bw 10
~
* This is deprecated, replaced by mob custom strings.
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    %echo% %self.name% sings, 'Look for the bear necessities, the simple bear necessities. Forget about your worries and your strife...'
  break
  case 2
    %echo% %self.name% dances around, humming to %self.himher%self.
  break
  case 3
    %echo% %self.name% sings, 'Now when you pick a pawpaw, or a prickly pear, and you prick a raw paw, well, next time beware!'
  break
  case 4
    say I don't know what all the hullabaloo is about a singing bear.
  break
done
~
#614
Walking Tree script~
0 bw 10
~
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    say My family tree...
    wait 1 sec
    say ... is quite large.
  break
  case 2
    say My grandmother...
    wait 1 sec
    say ... was a willow.
  break
  case 3
    say We can leave...
    wait 1 sec
    say ... as soon as...
    wait 1 sec
    say ... I spruce myself up.
  break
  case 4
    say I am...
    wait 1 sec
    say ... not Groot.
  break
done
~
#615
Feral Pixy script~
0 bw 10
~
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    %echo% %self.name% whittles away at a toothpick.
  break
  case 2
    %echo% %self.name% hurls acorns at you!
  break
  case 3
    %echo% %self.name% sneezes pixy dust all over you!
    set ch %self.room.people%
    while %ch%
      dg_affect %ch% FLY on 60
      set ch %ch.next_in_room%
    done
  break
  case 4
    say They used to make us race for food.
    wait 1
    %echo% %self.name% shivers uncomfortably.
  break
done
~
#616
Pixy Queen script~
0 bw 10
~
if %self.disabled%
  halt
end
switch %random.4%
  case 1
    say We are not amused.
  break
  case 2
    say I'm the queen bee around here.
  break
  case 3
    %echo% %self.name% sprinkles pixy dust all over you!
    set ch %self.room.people%
    while %ch%
      dg_affect %ch% FLY on 60
      set ch %ch.next_in_room%
    done
  break
  case 4
    say If we don't quash the fairy revolt, I'll be replaced by a Piczar!
  break
done
~
#617
Free genie load~
0 n 100
~
wait 1
* only allows 1 copy of the genie here
set ch %self.next_in_room%
while %ch%
  if %ch.vnum% == %self.vnum%
    say I shall go enjoy my freedom.
    %echo% %self.name% vanishes in a puff of smoke!
    %purge% %self%
    halt
  end
  set ch %ch.next_in_room%
done
* still here?
dg_affect %self% !ATTACK on -1
detach 617 %self.id%
~
#618
Genie trigger phrase~
0 d 1
wish~
if %actor.is_npc% || %actor.nohassle%
  halt
end
wait 1
say Wish? You wish? I've had enough with wishes!
wait 1
if %actor.room% != %self.room%
  halt
end
dg_affect %self% !ATTACK off
%aggro% %actor%
~
#619
Genie death~
0 f 100
~
say No! Noooo! I won't go back in the bottle!
%echo% %self.name% screams as he's sucked into a brass bottle, expelling all his power trying in vain to get free.
return 0
~
$
