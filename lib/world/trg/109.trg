#10900
Dragon tokens count~
1 c 2
count~
eval test %%actor.obj_target(%arg%)%%
if %self% != %test%
  return 0
  halt
end
eval var_name dragtoken_128
%send% %actor% You count %self.shortdesc%.
eval test %%actor.varexists(%var_name%)%%
%echoaround% %actor% %actor.name% counts %self.shortdesc%.
if %test%
  eval count %%actor.%var_name%%%
  if %count% == 1
    %send% %actor% You have one token.
  else
    %send% %actor% You have %count% tokens.
  end
else
  %send% %actor% You have no tokens.
end
~
#10901
Dragon token load/purge~
1 n 100
~
eval var_name dragtoken_128
eval actor %self.carried_by%
if %actor%
  if !%actor.inventory(10900)%
    %load% obj 10900 %actor% inv
    %send% %actor% You find a pouch to store your dragon tokens in.
  end
  eval test %%actor.varexists(%var_name%)%%
  if %test%
    eval val %%actor.%var_name%%%
    eval %var_name% %val%+1
    remote %var_name% %actor.id%
  else
    eval %var_name% 1
    remote %var_name% %actor.id%
  end
end
%purge% %self%
~
#10902
Colossal Dragon knight/thief random move~
0 n 100
~
eval room %self.room%
if (!%instance.location% || %room.template% != 10900)
  halt
end
eval room %instance.location%
* Teleport out of the cave
%echo% %self.name% vanishes!
eval outside_dir %room.exit_dir%
eval outside %%room.%outside_dir%(room)%%
mgoto %outside%
%echo% %self.name% flees from the cave.
* Move around a bit
mmove
mmove
mmove
mmove
mmove
mmove
~
#10903
Colossal Dragon knight/thief limit wander~
0 i 100
~
eval start_room %instance.location%
if !%start_room%
  * No instance
  halt
end
eval room %self.room%
eval dist %%room.distance(%start_room%)%%
if %dist%>30
  * Stop wandering
  %echo% %self.name% stops wandering around.
  nop %self.add_mob_flag(SENTINEL)%
  * If we're in a building, exit it
  if %room.exit_dir%
    * Check we're in the entrance room
    mgoto %room.coords%
    * Then leave
    %room.exit_dir%
  end
  detach 10903 %self.id%
end
~
#10904
Dragon loot load boe/bop~
1 n 100
~
eval actor %self.carried_by%
if !%actor%
  eval actor %self.worn_by%
end
if !%actor%
  halt
end
if %actor% && %actor.is_pc%
  * Item was crafted
  if %self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if !%self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
  * Default flag is BOP so need to unbind when setting BOE
  nop %self.bind(nobody)%
else
  * Item was probably dropped
  if !%self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if %self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
end
~
#10905
Colossal red dragon combat + enrage~
0 k 100
~
eval soft_enrage_rounds 140
eval hard_enrage_rounds 300
* Count attacks until enrage
eval enrage_counter 0
eval enraged 0
if %self.varexists(enrage_counter)%
  eval enrage_counter %self.enrage_counter%
end
eval enrage_counter %enrage_counter%+1
if %enrage_counter% > %soft_enrage_rounds%
  * Start increasing damage
  eval enraged 1
  if %enrage_counter% > %hard_enrage_rounds%
    eval enraged 2
  end
  * Enrage messages
  if %enrage_counter% == %soft_enrage_rounds%
    %echo% %self.name%'s eyes glow with fury! You'd better hurry up and kill %self.himher%!
  end
  if %enrage_counter% == %hard_enrage_rounds%
    %echo% %self.name% takes to the air, letting out a fearsome roar!
    %regionecho% %self.room% 5 %self.name%'s roar shakes the ground!
  end
end
remote enrage_counter %self.id%
* Enrage effects:
if %enraged%
  if %enraged% == 2
    * This should wipe out even a pretty determined party in a couple of shots
    %echo% %self.name% unleashes a torrent of flame upon %self.hisher% foes from above!
    %aoe% 1000 direct
  end
  * Don't always show the message or it would be even spammier
  if %random.4% == 4
    %echo% %self.name%'s eyes glow with white-hot rage!
  end
  dg_affect %self% BONUS-PHYSICAL 5 3600
end
* Start of regular combat script:
* Check chance (this was a percent on the script but the enrage counter needs to always work)
if %random.10%!=10
  halt
  * Pick an attack:
end
switch %random.4%
  * Searing burns on tank
  case 1
    %send% %actor% &r%self.name% spits fire at you, causing searing burns!&0
    %echoaround% %actor% %self.name% spits fire at %actor.name%, causing searing burns!
    %dot% %actor% 200 60 fire
    %damage% %actor% 150 fire
  break
  * Flame wave AoE
  case 2
    %echo% %self.name% begins puffing smoke and spinning in circles!
    * Give the healer (if any) time to prepare for group heals
    wait 3 sec
    %echo% &r%self.name% unleashes a flame wave!&0
    %aoe% 75 fire
  break
  * Traumatic burns on tank
  case 3
    %send% %actor% &r%self.name% spits fire at you, causing traumatic burns!&0
    %echoaround% %actor% %self.name% spits fire at %actor.name%, causing traumatic burns!
    %dot% %actor% 500 15 fire
    %damage% %actor% 50 fire
  break
  * Rock stun on random enemy
  case 4
    eval target %random.enemy%
    if (%target%)
      %send% %target% &r%self.name% uses %self.hisher% tail to hurl a rock at you, stunning you momentarily!&0
      %echoaround% %target% %self.name% hurls a rock at %target.name% with %self.hisher% tail, stunning %target.himher% momentarily!
      dg_affect %target% STUNNED on 10
      %damage% %target% 150 physical
    end
  break
end
~
#10906
Colossal crimson dragon death~
0 f 100
~
* Make the other NPC no longer killable
eval mob %instance.mob(10901)%
if %mob%
  dg_affect %mob% !ATTACK on -1
end
* Complete the adventure
if %instance.start%
  * Attempt delayed despawn
  %at% %instance.start% %load% o 10930
else
  %adventurecomplete%
end
~
#10907
Colossal Red Dragon environmental~
0 bw 10
~
* This script is no longer used. It was replaced by custom strings.
if %self.fighting%
  halt
end
switch %random.4%
  case 1
    %echo% %self.name% coils around her hoard.
  break
  case 2
    %echo% Wisps of smoke trail from %self.name%'s nostrils.
  break
  case 3
    %echo% %self.name% picks a scorched tower shield out of her teeth.
  break
  case 4
    %echo% The cave shakes as %self.name% grumbles something that sounds like 'vivor'.
  break
end
~
#10908
Enrage Buff/Counter Reset~
0 b 25
~
if !%self.fighting% && %self.varexists(enrage_counter)%
  if %self.enrage_counter% == 0
    halt
  end
  if %self.aff_flagged(!ATTACK)%
    halt
  end
  if %self.aff_flagged(!SEE)%
    %echo% %self.name% returns.
  end
  %load% mob %self.vnum%
  %echo% %self.name% settles down to rest.
  %purge% %self%
end
~
#10909
No Leave During Combat - must fight~
0 s 100
~
if %self.fighting%
  %send% %actor% You cannot flee during the combat with %actor.name%!
  return 0
  halt
end
return 1
~
#10910
Sir Vivor Combat + Enrage~
0 k 100
~
eval soft_enrage_rounds 140
eval hard_enrage_rounds 300
* Count attacks until enrage
eval enrage_counter 0
eval enraged 0
if %self.varexists(enrage_counter)%
  eval enrage_counter %self.enrage_counter%
end
eval enrage_counter %enrage_counter%+1
if %enrage_counter% > %soft_enrage_rounds%
  * Start increasing damage
  eval enraged 1
  if %enrage_counter% > %hard_enrage_rounds%
    eval enraged 2
  end
  * Enrage messages
  if %enrage_counter% == %soft_enrage_rounds%
    %echo% %self.name%'s screams and attacks with great ferocity! You'd better hurry up and kill %self.himher%!
  end
  if %enrage_counter% == %hard_enrage_rounds%
    %echo% %self.name%, seeing that nothing is working, looks for an escape route!
  end
end
remote enrage_counter %self.id%
* Enrage effects:
if %enraged%
  if %enraged% == 2
    * Pretend to flee
    if %self.aff_flagged(STUNNED)%
      %echo% %self.name% shakes his head and recovers from stunning!
    end
    if %self.aff_flagged(ENTANGLED)%
      %echo% %self.name% breaks free of the vines entangling him!
    end
    %echo% %self.name% tries to flee...
    %echo% %self.name% runs behind a large stalagmite and disappears!
    %restore% %self%
    dg_affect %self% !ATTACK on 300
    dg_affect %self% !SEE on -1
  end
  * Don't always show the message or it would be even spammier
  if %random.4% == 4
    %echo% %self.name% swings his sword with strength born of terror!
  end
  dg_affect %self% BONUS-PHYSICAL 5 3600
end
* Start of regular combat script:
* Check chance (this was a percent on the script but the enrage counter needs to always work)
if %random.10%!=10
  halt
end
* Pick an attack:
switch %random.4%
  * Disarm dps (whoever has most tohit)
  case 1
    eval target %random.enemy%
    eval person %room.people%
    while %person%
      eval check %%self.is_enemy(%person%)%%
      if (%person.tohit% > %target.tohit%) && %check% && !%person.aff_flagged(DISARM)%
        eval target %person%
      end
      eval person %person.next_in_room%
    done
    if %target%
      if !%target.aff_flagged(DISARM)%
        disarm %target%
      end
    end
  break
  * AoE
  case 2
    %echo% %self.name% starts spinning in circles!
    * Give the healer (if any) time to prepare for group heals
    wait 3 sec
    %echo% %self.name% swings his sword with wild abandon!
    %aoe% 75 physical
  break
  * Stun on tank
  case 3
    eval target %actor%
    %send% %target% %self.name% trips you and bashes you with the pommel of %self.hisher% sword, stunning you!
    %echoaround% %target% %self.name% trips %target.name% and bashes %target.himher% with the pommel of %self.hisher% sword, stunning %target.himher% momentarily!
    if !%target.aff_flagged(STUNNED) && !%target.aff_flagged(!STUN)%
      dg_affect %target% STUNNED on 15
    end
    %damage% %target% 75 physical
  break
  * Blind on healer (whoever has the most max mana at least)
  case 4
    eval target %random.enemy%
    eval person %room.people%
    while %person%
      eval check %%self.is_enemy(%person%)%%
      if (%person.mana% > %target.mana%) && %check% && !%person.aff_flagged(BLIND)%
        eval target %person%
      end
      eval person %person.next_in_room%
    done
    if (%target%)
      if !%target.aff_flagged(BLIND)%
        blind %target%
      end
    end
  break
done
~
#10911
Sir Vivor death~
0 f 100
~
* Make the other NPC no longer killable
eval mob %instance.mob(10900)%
if %mob%
  dg_affect %mob% !ATTACK on -1
end
* Complete the adventure
if %instance.start%
  * Attempt delayed despawn
  %at% %instance.start% %load% o 10930
else
  %adventurecomplete%
end
~
#10912
Sir Vivor environmental~
0 bw 10
~
* This script is no longer used. It was replaced by custom strings.
if %self.fighting%
  halt
end
switch %random.4%
  case 1
    %echo% %self.name% mumbles, 'I just don't think I was cut out to do the fighting part.'
  break
  case 2
    %echo% %self.name% polishes some tokens with an oil cloth.
  break
  case 3
    %echo% %self.name% proudly shows you a list of dragons whose deaths he has been partially responsible for.
  break
  case 4
    say Mama said there'd be dragons like this...
  break
end
~
#10913
Bangles the thief environmental~
0 bw 10
~
switch %random.4%
  case 1
    %echo% You can see the back half of %self.name% sticking out of the shadows.
  break
  case 2
    %echo% %self.name% curses under his breath as he steps on a large, dry twig.
  break
  case 3
    say It's two for one on dragon's hoards, eh?
  break
  case 4
    say Friends call me Crash Bangles.
  break
end
~
#10914
Delayed spawn announcement~
0 n 100
~
wait 2
%echo% %self.name% arrives!
~
#10915
Colossal crimson dragon fake pickpocket~
0 p 100
~
if !(%abilityname%==pickpocket)
  halt
end
if %actor.inventory(10945)%
  %send% %actor% You have already stolen from the dragon.
  return 0
  halt
end
if %actor.on_quest(10902)%
  if %self.level%-%actor.level% > 50
    %send% %actor% You're not high enough level to steal from %self.name%! It'd just eat you.
    return 0
    halt
  end
  %send% %actor% You pick %self.name%'s pocket...
  %load% obj 10945 %actor% inv
  eval item %actor.inventory()%
  %send% %actor% You find %item.shortdesc%!
  return 0
  halt
end
~
#10916
Dragon token shop list~
0 c 0
list~
if !%arg%
  %send% %actor% %self.name% sells crafting materials, building materials, and crafting patterns.
  %send% %actor% Use '&clist materials&0', '&clist buildings&0' and '&clist crafting&0' to browse.
  %send% %actor% For more information on these items, see HELP COLOSSAL RED DRAGON.
elseif materials /= %arg%
  %send% %actor% For more information on these crafting materials, see HELP COLOSSAL RED DRAGON.
  %send% %actor% %self.name% sells the following materials for 2 dragon tokens each:
  %send% %actor% - dragon scale  ('buy scale')
  %send% %actor% - colossal dragon bone  ('buy bone')
  %send% %actor% - red dragon hide  ('buy hide')
elseif buildings /= %arg%
  %send% %actor% For more information on these buildings, see HELP COLOSSAL RED DRAGON.
  %send% %actor% %self.name% sells the following buildings for 6 dragon tokens each:
  %send% %actor% For a butcher tent:    dragonsflame butcher block ('buy butcher')
  %send% %actor% For a dragon cave:  dragonstone hearth ('buy hearth')
  %send% %actor% For a dragon statue:  dragonseye ruby ('buy ruby')
  %send% %actor% For a dragon library:  dragonquill pen ('buy pen')
elseif crafting /= %arg%
  %send% %actor% For more information on the items these patterns are for, see HELP COLOSSAL RED DRAGON.
  %send% %actor% %self.name% sells the following crafting patterns for 12 dragon tokens each:
  %send% %actor% - dragonsteel warhammer schematic  ('buy warhammer')
  %send% %actor% - dragonsteel claymore schematic  ('buy claymore')
  %send% %actor% - dragonbone mace schematic  ('buy mace')
  %send% %actor% - draconic wand schematic  ('buy wand')
  %send% %actor% - dragonbone staff schematic  ('buy staff')
  %send% %actor% - dragonbone wristblade design  ('buy wristblade')
  %send% %actor% - dragonbone tower shield design  ('buy shield')
  %send% %actor% - red dragon charm design  ('buy charm')
  %send% %actor% - dragonkind tome design  ('buy tome')
  %send% %actor% - crimson dragon girdle pattern  ('buy girdle')
  %send% %actor% - burning dragonsteel waistband pattern  ('buy waistband')
  %send% %actor% - flaming draconic cincture pattern  ('buy cincture')
  %send% %actor% - burning dragoncloth sash pattern  ('buy sash')
  %send% %actor% - magnificent dragon bracelet schematic  ('buy bracelet')
else
  %send% %actor% Valid categories are 'materials', 'buildings', and 'crafting'.
end
~
#10917
Dragon token shop buy~
0 c 0
buy~
eval vnum -1
eval cost 0
eval named a thing
eval currency dragtoken_128
if (!%arg%)
  %send% %actor% Type 'list' to see what's available.
  halt
  * Start of resources
elseif scale /= %arg%
  eval vnum 10006
  eval cost 2
  eval named a dragon scale
elseif bone /= %arg%
  eval vnum 10903
  eval cost 2
  eval named a dragon bone
elseif hide /= %arg%
  eval vnum 10902
  eval cost 2
  eval named a dragon hide
  * Start of building patterns
elseif butcher /= %arg%
  eval vnum 10923
  eval cost 6
  eval named a dragonsflame butcher block
elseif hearth /= %arg%
  eval vnum 10924
  eval cost 6
  eval named a dragonstone hearth
elseif ruby /= %arg%
  eval vnum 10925
  eval cost 6
  eval named a dragonseye ruby
elseif pen /= %arg%
  eval vnum 10926
  eval cost 6
  eval named a dragonquill pen
  * Start of crafting patterns
elseif warhammer /= %arg%
  eval vnum 10931
  eval cost 12
  eval named a dragonsteel warhammer schematic
elseif claymore /= %arg%
  eval vnum 10932
  eval cost 12
  eval named a dragonsteel claymore schematic
elseif mace /= %arg%
  eval vnum 10933
  eval cost 12
  eval named a dragonbone mace schematic
elseif wand /= %arg%
  eval vnum 10934
  eval cost 12
  eval named a draconic wand schematic
elseif staff /= %arg%
  eval vnum 10935
  eval cost 12
  eval named a dragonbone staff schematic
elseif wristblade /= %arg%
  eval vnum 10936
  eval cost 12
  eval named a dragonbone wristblade design
elseif shield /= %arg%
  eval vnum 10937
  eval cost 12
  eval named a dragonbone tower shield design
elseif charm /= %arg%
  eval vnum 10938
  eval cost 12
  eval named a red dragon charm design
elseif tome /= %arg%
  eval vnum 10939
  eval cost 12
  eval named a dragonkind tome design
elseif girdle /= %arg%
  eval vnum 10940
  eval cost 12
  eval named a crimson dragon girdle pattern
elseif waistband /= %arg%
  eval vnum 10941
  eval cost 12
  eval named a burning dragonsteel waistband pattern
elseif cincture /= %arg%
  eval vnum 10942
  eval cost 12
  eval named a flaming draconic cincture pattern
elseif sash /= %arg%
  eval vnum 10943
  eval cost 12
  eval named a burning dragoncloth sash pattern
elseif bracelet /= %arg%
  eval vnum 10947
  eval cost 12
  eval named a magnificent dragon bracelet schematic
  * End of crafting patterns
else
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
eval var %%actor.%currency%%%
eval test %var% >= %cost%
eval correct_noun tokens
if %cost% == 1
  eval correct_noun token
end
if !%test%
  %send% %actor% %self.name% tells you, 'You'll need %cost% dragon %correct_noun% to buy that.'
  halt
end
eval %currency% %var%-%cost%
remote %currency% %actor.id%
%load% obj %vnum% %actor% inv %actor.level%
%send% %actor% You buy %named% for %cost% dragon %correct_noun%.
%echoaround% %actor% %actor.name% buys %named%.
~
#10919
Colossal Dragon must-fight~
0 q 100
~
if (%actor.is_npc% || %actor.nohassle% || %actor.on_quest(10900)% || %self.aff_flagged(!ATTACK)%)
  halt
end
if %actor.aff_flagged(SNEAK)%
  %send% %actor% You sneak away from %self.name%.
  halt
end
%send% %actor% You can't seem to get away from %self.name%!
return 0
~
#10920
Detach must-fight~
2 v 100
~
eval dragon %instance.mob(10900)%
if %dragon%
  detach 10919 %dragon.id%
end
~
#10921
Dragon quest spawn shopkeeper~
2 v 100
~
if %questvnum% < 10900 || %questvnum% > 10902
  halt
end
eval person %room.people%
while %person%
  if %person.is_npc% && %person.vnum% == 10903
    halt
  end
  eval person %person.next_in_room%
done
* Spawn the shopkeeper
%load% m 10903
~
#10925
Dragonslayer Statue~
2 bw 15
~
switch %random.4%
  case 1
    %echo% Scorching flames belch forth from the statue!
  break
  case 2
    %echo% Wisps of smoke trail from the face of the statue!
  break
  case 3
    %echo% You barely dodge a burst of flames from the statue!
  break
  case 4
    %echo% Smoke bellows from the statue!
  break
end
~
#10926
Add Laboratory on Build~
2 o 100
~
eval lab %%room.%room.enter_dir%(room)%%
* Add laboratry
if !%lab%
  %door% %room% %room.enter_dir% add 5618
end
detach 10926 %room.id%
~
#10930
Crimson dragon despawn timer~
1 f 0
~
%adventurecomplete%
~
$
