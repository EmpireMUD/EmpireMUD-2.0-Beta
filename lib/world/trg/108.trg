#10800
Pageboy spawn~
0 n 100
~
if (!%instance.location%)
  halt
end
mgoto %instance.location%
set room %self.room%
if %room.exit_dir%
  %room.exit_dir%
end
~
#10801
Give rejection~
0 j 100
~
%send% %actor% You can't give items to %self.name%. Try "quest finish <name>" instead.
return 0
~
#10802
Pageboy shout~
0 ab 1
~
if %random.3% == 3
  %regionecho% %self.room% 20 %self.name% shouts, 'Learn the Empire skill at the (^^) Royal Planning Office!'
end
~
#10803
imperial ring: setup on load~
1 n 100
~
set pers %self.carried_by%
if %pers%
  set emp %pers.empire%
  if %emp%
    set empire %emp.id%
    %mod% %self% shortdesc the imperial ring of %emp.name%
    %mod% %self% longdesc The %emp.adjective% imperial ring is lying here.
    %mod% %self% keywords ring imperial %emp.name% %emp.adjective%
    %mod% %self% append-lookdesc It can only be worn by members of %emp.name%.
  else
    set empire -1
  end
  global empire
end
~
#10804
imperial ring: bind-to-empire~
1 j 0
~
if !%empire%
  %send% %actor% %self.shortdesc% can't be used anymore.
  return 0
  halt
end
makeuid emp %empire%
if !%emp%
  %send% %actor% %self.shortdesc% is from a long-lost empire and can no longer be used.
  return 0
  halt
end
if %actor.empire% != %emp%
  %send% %actor% Only members of %emp.name% can use %self.shortdesc%.
  return 0
  halt
end
* success
return 1
~
#10825
Crier spawn~
0 n 100
~
if (!%instance.location%)
  halt
end
mgoto %instance.location%
set room %self.room%
if %room.exit_dir%
  %room.exit_dir%
end
~
#10826
Give rejection~
0 j 100
~
%send% %actor% You can't give items to %self.name%. Try "quest finish <name>" instead.
return 0
~
#10827
Crier Shout~
0 ab 1
~
if %random.3% == 3
  %regionecho% %self.room% 20 %self.name% shouts, 'Learn the Trade skill at the &y/()\&0 Museum of Early Man!'
end
~
#10828
Curator environment~
0 bw 5
~
switch %random.4%
  case 1
    say Maybe I should hire some skeletons to stand around and chat with visitors.
  break
  case 2
    %echo% %self.name% sprinkles some authentic-looking dust on the shelves.
  break
  case 3
    %echo% %self.name% rubs a piece of leather in the dirt, then heaps it on a shelf.
  break
  case 4
    %echo% %self.name% jingles a jar labeled 'Admissions', but it sounds empty.
  break
done
~
#10829
Cave Phase 2 Linker~
2 n 100
~
if !%instance.id%
  halt
end
set entr %instance.location%
set dir %entr.exit_dir%
eval outside %%entr.%dir%(vnum)%%
%door% %room% %dir% room %outside%
detach 10829 %room.id%
~
#10830
Cave Phase 2 Teleporter~
2 g 100
~
if (%actor.is_pc% && %actor.completed_quest(10835)%)
  %teleport% %actor% i10826
  halt
end
set boss %actor.master%
if (%actor.is_npc% && %boss%)
  %teleport% %actor% %boss.room%
end
~
#10837
Start Grandmaster's Journey~
2 u 0
~
%load% obj 10837 %actor%
~
#10850
No-see on spawn~
0 n 100
~
* Mob will appear via its greet trig later
nop %self.add_mob_flag(SILENT)%
dg_affect %self% !SEE on -1
~
#10851
Detect Ritual of Burdens~
2 p 100
~
if (%ability% != 163 || !%actor.on_quest(10851)%)
  halt
end
* check every second for 12 seconds to detect the affect
set tries 12
while %tries% > 0
  wait 1 sec
  eval tries %tries% - 1
  if !%actor%
    halt
  end
  if %actor.affect(3052)%
    %quest% %actor% trigger 10851
    %send% %actor% You have completed the ritual for your quest.
    halt
  end
done
~
#10852
Give Scythe~
2 u 100
~
%load% obj 10856 %actor%
~
#10853
Give Skinning Knife~
2 u 100
~
%load% obj 10853 %actor%
~
#10854
Detect Heal~
0 c 0
heal~
if ((!%self.is_name(%arg%)% && %actor.char_target(%arg%)% != %self%) || !%actor.ability(Heal)% || !%actor.on_quest(10854)%)
  return 0
  halt
end
%send% %actor% Your mana pulses and waves over %self.name%, healing %self.hisher% spirit.
%echoaround% %actor% %actor.name%'s mana pulses and waves over %self.name%, healing %self.hisher% spirit.
%quest% %actor% trigger 10854
return 1
~
#10855
Detect Sneak on Leave~
0 s 100
~
if %actor.on_quest(10855)% && !%actor.quest_triggered(10855)%
  if %actor.aff_flagged(SNEAK)%
    %quest% %actor% trigger 10855
    wait 1
    %send% %actor% You have successfully sneaked! The quest is complete.
  else
    wait 1
    %send% %actor% You weren't sneaky enough.
  end
elseif %direction% == north && !%actor.aff_flagged(SNEAK)%
  %send% %actor% You can't get there unless you're sneaking.
  return 0
end
~
#10856
Detect Harvest~
1 c 3
harvest~
if %actor.room.crop%
  %quest% %actor% trigger 10856
end
return 0
~
#10857
Start Blood Tutorial~
2 u 100
~
if (%actor.blood% >= (%actor.maxblood% - 5))
  * Ensure not at full blood by taking a little
  nop %actor.blood(-5)%
end
set bloodamt %actor.blood%
remote bloodamt %actor.id%
~
#10858
Detect Full Blood + Soulstream Entry~
2 g 100
~
* Detect full blood
if (%actor.on_quest(10857)% && %actor.varexists(bloodamt)%)
  if %actor.blood% > %actor.bloodamt%
    %quest% %actor% trigger 10857
    rdelete bloodamt %actor.id%
  end
end
* Detect soulstream entry
if %room.template% == 10850
  if (%direction% != none || %actor.nohassle%)
    * Not a portal entry - or an immortal with hassle off
    return 1
    halt
  end
  if (!%actor.on_quest(10850)% && !%actor.completed_quest(10850)%)
    %send% %actor% You must start the quest 'Enter the Soulstream' before entering.
    %send% %actor% Use 'quest start Enter' to begin the quest.
    %send% %actor% (If you can't see the quest, try 'toggle tutorials'.)
    %echoaround% %actor% %actor.name% is pushed back through the portal.
    return 0
  end
end
~
#10859
Convert Spirit Token~
1 gh 100
~
%send% %actor% Your spirit token vanishes and can now be found on the 'coins' command.
%actor.give_currency(10852,1)%
%purge% %self%
return 0
~
#10860
Soulstream Mob Show/Hide~
0 h 100
~
if %actor.is_npc%
  halt
end
* determine skill
switch %self.vnum%
  case 10851
    set skl High Sorcery
  break
  case 10852
    set skl Survival
  break
  case 10853
    set skl Battle
  break
  case 10854
    set skl Natural Magic
  break
  case 10855
    set skl Stealth
  break
  case 10857
    set skl Vampire
  break
  case 10858
    set skl Trade
  break
  default
    * No valid skill to check
    halt
  break
done
* Wait for all arrivals...
wait 1
* Count and see if we should then appear/disappear (sometimes followers enter late)
set count 0
set person %self.room.people%
while %person%
  if %person.is_pc% && %person.skill(%skl%)% > 0
    eval count %count% + 1
  end
  set person %person.next_in_room%
done
if %count% > 0 && %self.aff_flagged(!SEE)%
  nop %self.remove_mob_flag(SILENT)%
  dg_affect %self% !SEE off
  %echo% %self.name% appears from deep in the soulstream!
elseif %count% == 0 && !%self.aff_flagged(!SEE)%
  %echo% %self.name% vanishes into the soulstream.
  nop %self.add_mob_flag(SILENT)%
  dg_affect %self% !SEE on -1
end
~
#10861
Detect Sneak on Greet~
0 h 100
~
if %actor.on_quest(10855)% && !%actor.quest_triggered(10855)%
  if %actor.aff_flagged(SNEAK)%
    %quest% %actor% trigger 10855
    wait 1
    %send% %actor% You have successfully sneaked! The quest is complete.
  else
    wait 1
    %send% %actor% You weren't sneaky enough.
  end
end
~
$
