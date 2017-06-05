#600
Seed enchantment~
1 c 2
enchant~
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
  return 0
  halt
end
set cost_item_name a prismatic crystal
set cost_item_vnum 602
set cost_item_count 1
set ability_required Enchant Tools
set mana_cost 100
set result_vnum 601
eval check %%actor.ability(%ability_required%)%%
if !%check%
  %send% %actor% You need %ability_required% to enchant %self.shortdesc%.
  return 1
  halt
end
if %actor.mana% < %mana_cost%
  %send% %actor% You need %mana_cost% mana to enchant %self.shortdesc%.
  return 1
  halt
end
eval check %%actor.has_resources(%cost_item_vnum%, %cost_item_count%)%%
if !%check%
  %send% %actor% You need %cost_item_name% (x%cost_item_count%) to enchant %self.shortdesc%.
  return 1
  halt
end
eval charge %%actor.add_resources(%cost_item_vnum%,-%cost_item_count%)%%
nop %charge%
eval charge %%actor.mana(-%mana_cost%)%%
%send% %actor% You enchant %self.shortdesc%!
%echoaround% %actor% %actor.name% enchants %self.shortdesc%!
nop %charge%
%load% obj %result_vnum% %actor% inv
eval obj %actor.inventory()%
%send% %actor% It becomes %obj.shortdesc%!
%echoaround% %actor% It becomes %obj.shortdesc%!
%purge% %self%
~
#601
Enchanted seed plant~
1 c 2
plant~
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
  return 0
  halt
end
eval room %self.room%
if !%actor.canuseroom_member()%
  %send% %actor% You don't have permission to plant anything here.
  return 1
  halt
end
if %room.sector% != Plains
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
#602
Enchanted forest unclaimed chop~
0 ab 100
~
eval room %self.room%
eval cycles_left 5
while %cycles_left% >= 0
  if (%self.room% != %room%) || %room.empire% || !(%room.sector% ~= Enchanted)
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
      %echo% %self.name% mutters an unspeakable word, killing the grass...
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
      %terraform% %room% 605
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
%echo% %self.name% looks confused...
~
#610
Talking Horse script~
0 bw 10
~
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
      eval has_duped %self.has_duped%
    else
      eval has_duped 0
    end
    if %has_duped% > 0
      * has already duplicated
      %echo% %self.name% curls up, becomes a ball of dust, and blows away in the wind.
      %purge% %self%
      halt
    else
      eval room %self.room%
      * only duplicate if not in a building
      if (!%room.building%)
        %echo% %self.name% does a little backflip and splits into two bunnies!
        %load% mob 612
        eval has_duped 1
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
    eval room %self.room%
    eval ch %room.people%
    while %ch%
      dg_affect %ch% FLY on 60
      eval ch %ch.next_in_room%
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
    eval room %self.room%
    eval ch %room.people%
    while %ch%
      dg_affect %ch% FLY on 60
      eval ch %ch.next_in_room%
    done
  break
  case 4
    say If we don't quash the fairy revolt, I'll be replaced by a Piczar!
  break
done
~
$
