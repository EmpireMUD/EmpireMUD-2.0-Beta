#10900
Red Dragon Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(10900)%
end
~
#10902
Colossal Dragon knight/thief random move~
0 n 100
~
set room %self.room%
if (!%instance.location% || %room.template% != 10900)
  halt
end
set room %instance.location%
* Teleport out of the cave
%echo% ~%self% vanishes!
set outside_dir %room.exit_dir%
eval outside %%room.%outside_dir%(room)%%
mgoto %outside%
%echo% ~%self% flees from the cave.
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
set start_room %instance.location%
if !%start_room%
  * No instance
  halt
end
set room %self.room%
if %room.distance(%start_room%)% > 30
  * Stop wandering
  %echo% ~%self% stops wandering around.
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
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
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
set soft_enrage_rounds 140
set hard_enrage_rounds 300
* Count attacks until enrage
set enrage_counter 0
set enraged 0
if %self.varexists(enrage_counter)%
  set enrage_counter %self.enrage_counter%
end
eval enrage_counter %enrage_counter%+1
if %enrage_counter% > %soft_enrage_rounds%
  * Start increasing damage
  set enraged 1
  if %enrage_counter% > %hard_enrage_rounds%
    set enraged 2
  end
  * Enrage messages
  if %enrage_counter% == %soft_enrage_rounds%
    %echo% |%self% eyes glow with fury! You'd better hurry up and kill *%self%!
  end
  if %enrage_counter% == %hard_enrage_rounds%
    %echo% ~%self% takes to the air, letting out a fearsome roar!
    %regionecho% %self.room% 5 |%self% roar shakes the ground!
  end
end
remote enrage_counter %self.id%
* Enrage effects:
if %enraged%
  if %enraged% == 2
    * This should wipe out even a pretty determined party in a couple of shots
    %echo% ~%self% unleashes a torrent of flame upon ^%self% foes from above!
    %aoe% 1000 direct
  end
  * Don't always show the message or it would be even spammier
  if %random.4% == 4
    %echo% |%self% eyes glow with white-hot rage!
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
    %send% %actor% &&r~%self% spits fire at you, causing searing burns!&&0
    %echoaround% %actor% ~%self% spits fire at ~%actor%, causing searing burns!
    %dot% %actor% 200 60 fire
    %damage% %actor% 150 fire
  break
  * Flame wave AoE
  case 2
    %echo% ~%self% begins puffing smoke and spinning in circles!
    * Give the healer (if any) time to prepare for group heals
    wait 3 sec
    %echo% &&r~%self% unleashes a flame wave!&&0
    %aoe% 75 fire
  break
  * Traumatic burns on tank
  case 3
    %send% %actor% &&r~%self% spits fire at you, causing traumatic burns!&&0
    %echoaround% %actor% ~%self% spits fire at ~%actor%, causing traumatic burns!
    %dot% %actor% 500 15 fire
    %damage% %actor% 50 fire
  break
  * Rock stun on random enemy
  case 4
    set target %random.enemy%
    if (%target%)
      %send% %target% &&r~%self% uses ^%self% tail to hurl a rock at you, stunning you momentarily!&&0
      %echoaround% %target% ~%self% hurls a rock at ~%target% with ^%self% tail, stunning *%target% momentarily!
      dg_affect %target% STUNNED on 10
      %damage% %target% 150 physical
    end
  break
done
~
#10906
Colossal crimson dragon death~
0 f 100
~
* Make the other NPC no longer killable
set mob %instance.mob(10901)%
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
    %echo% ~%self% coils around her hoard.
  break
  case 2
    %echo% Wisps of smoke trail from |%self% nostrils.
  break
  case 3
    %echo% ~%self% picks a scorched tower shield out of her teeth.
  break
  case 4
    %echo% The cave shakes as ~%self% grumbles something that sounds like 'vivor'.
  break
done
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
    %echo% ~%self% returns.
  end
  %load% mob %self.vnum%
  %echo% ~%self% settles down to rest.
  %purge% %self%
end
~
#10909
No Leave During Combat - must fight~
0 s 100
~
if %self.fighting%
  %send% %actor% You cannot flee during the combat with ~%actor%!
  return 0
  halt
end
return 1
~
#10910
Sir Vivor Combat + Enrage~
0 k 100
~
set soft_enrage_rounds 140
set hard_enrage_rounds 300
* Count attacks until enrage
set enrage_counter 0
set enraged 0
if %self.varexists(enrage_counter)%
  set enrage_counter %self.enrage_counter%
end
eval enrage_counter %enrage_counter%+1
if %enrage_counter% > %soft_enrage_rounds%
  * Start increasing damage
  set enraged 1
  if %enrage_counter% > %hard_enrage_rounds%
    set enraged 2
  end
  * Enrage messages
  if %enrage_counter% == %soft_enrage_rounds%
    %echo% |%self% screams and attacks with great ferocity! You'd better hurry up and kill *%self%!
  end
  if %enrage_counter% == %hard_enrage_rounds%
    %echo% ~%self%, seeing that nothing is working, looks for an escape route!
  end
end
remote enrage_counter %self.id%
* Enrage effects:
if %enraged%
  if %enraged% == 2
    * Pretend to flee
    if %self.aff_flagged(HARD-STUNNED)%
      %echo% ~%self% shakes his head and recovers from stunning!
    end
    if %self.aff_flagged(IMMOBILIZED)%
      %echo% ~%self% breaks free of the vines entangling him!
    end
    %echo% ~%self% tries to flee...
    %echo% ~%self% runs behind a large stalagmite and disappears!
    %restore% %self%
    dg_affect %self% !ATTACK on 300
    dg_affect %self% !SEE on -1
  end
  * Don't always show the message or it would be even spammier
  if %random.4% == 4
    %echo% ~%self% swings his sword with strength born of terror!
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
    set target %random.enemy%
    set person %room.people%
    while %person%
      if (%person.tohit% > %target.tohit%) && %self.is_enemy(%person%)% && !%person.aff_flagged(DISARMED)%
        set target %person%
      end
      set person %person.next_in_room%
    done
    if %target%
      if !%target.aff_flagged(DISARMED)%
        disarm %target%
      end
    end
  break
  * AoE
  case 2
    %echo% ~%self% starts spinning in circles!
    * Give the healer (if any) time to prepare for group heals
    wait 3 sec
    %echo% ~%self% swings his sword with wild abandon!
    %aoe% 75 physical
  break
  * Stun on tank
  case 3
    set target %actor%
    %send% %target% ~%self% trips you and bashes you with the pommel of ^%self% sword, stunning you!
    %echoaround% %target% ~%self% trips ~%target% and bashes *%target% with the pommel of ^%self% sword, stunning *%target% momentarily!
    dg_affect %target% HARD-STUNNED on 15
    %damage% %target% 75 physical
  break
  * Blind on healer (whoever has the most max mana at least)
  case 4
    set target %random.enemy%
    set person %room.people%
    while %person%
      if (%person.mana% > %target.mana%) && %self.is_enemy(%person%)% && !%person.aff_flagged(BLIND)%
        set target %person%
      end
      set person %person.next_in_room%
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
set mob %instance.mob(10900)%
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
    %echo% ~%self% mumbles, 'I just don't think I was cut out to do the fighting part.'
  break
  case 2
    %echo% ~%self% polishes some tokens with an oil cloth.
  break
  case 3
    %echo% ~%self% proudly shows you a list of dragons whose deaths he has been partially responsible for.
  break
  case 4
    say Mama said there'd be dragons like this...
  break
done
~
#10913
Bangles the thief environmental~
0 bw 10
~
switch %random.4%
  case 1
    %echo% You can see the back half of ~%self% sticking out of the shadows.
  break
  case 2
    %echo% ~%self% curses under his breath as he steps on a large, dry twig.
  break
  case 3
    say It's two for one on dragon's hoards, eh?
  break
  case 4
    say Friends call me Crash Bangles.
  break
done
~
#10914
Delayed spawn announcement~
0 n 100
~
wait 2
%echo% ~%self% arrives!
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
    %send% %actor% You're not high enough level to steal from ~%self%! It'd just eat you.
    return 0
    halt
  end
  %send% %actor% You pick |%self% pocket...
  %load% obj 10945 %actor% inv
  set item %actor.inventory()%
  %send% %actor% You find @%item%!
  return 0
  halt
end
~
#10919
Colossal Dragon must-fight~
0 q 100
~
if (%actor.is_npc% || %actor.nohassle% || %actor.on_quest(10900)% || %self.aff_flagged(!ATTACK)%)
  halt
end
if %actor.aff_flagged(SNEAK)%
  %send% %actor% You sneak away from ~%self%.
  halt
end
%send% %actor% You can't seem to get away from ~%self%!
return 0
~
#10920
Detach must-fight~
2 v 100
~
set dragon %instance.mob(10900)%
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
set person %room.people%
while %person%
  if %person.is_npc% && %person.vnum% == 10903
    halt
  end
  set person %person.next_in_room%
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
    %echo% Smoke billows from the statue!
  break
done
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
#10950
Muck Dragon delayed despawn~
1 f 0
~
%adventurecomplete%
~
#10951
Muck Dragon load~
0 n 100
~
if %instance.location%
  mgoto %instance.location%
end
%load% obj 10950
~
#10952
Muck Dragon updater + leash~
0 i 100
~
eval room %self.room%
if (%room.sector% != Swamp && %room.sector% != Marsh && %room.sector% != River && %room.sector% != Lake)
  return 0
  halt
end
nop %instance.set_location(%room%)%
~
#10953
Muck Dragon death~
0 f 100
~
nop %instance.set_location(%instance.real_location%)%
set char %self.room.people%
while %char%
  if %char.is_pc% && %char.empire%
    nop %char.empire.start_progress(10950)%
  end
  set char %char.next_in_room%
done
if %instance.real_location%
  set obj %instance.real_location.contents%
  while %obj%
    if %obj.vnum% == 10950
      %purge% %obj%
      %at% %instance.real_location% %load% obj 10959
      halt
    end
    set obj %obj.next_in_list%
  done
end
~
#10956
Muck Dragon: Muck Rake~
0 k 33
~
if %self.cooldown(10950)%
  halt
end
nop %self.set_cooldown(10950, 30)%
%echo% ~%self% shakes its body violently, spraying clumps of sticky mud flying in all directions!
set person %self.room.people%
while %person%
  if %person.is_enemy(%self%)%
    dg_affect #10956 %person% SLOW on 60
  end
  set person %person.next_in_room%
done
~
#10957
Muck Dragon: Burrow Charge~
0 k 50
~
if %self.cooldown(10950)%
  halt
end
nop %self.set_cooldown(10950, 30)%
set verify_target %actor.id%
wait 1 sec
dg_affect #10959 %self% HARD-STUNNED on 20
dg_affect #10959 %self% IMMUNE-DAMAGE on 20
%echo% ~%self% dives into the marshy water and vanishes from view!
wait 3 sec
if %verify_target% != %actor.id%
  %echo% ~%self% rises from the swampy ground with a splash!
  dg_affect #10959 %self% off
  halt
end
%send% %actor% The swampy ground shifts beneath your feet!
%send% %actor% ('leap' out of the way!)
%echoaround% %actor% The ground shifts beneath |%actor% feet!
set target %actor.id%
remote target %self.id%
wait 9 sec
if %verify_target% != %actor.id%
  %echo% ~%self% rises from the swampy ground with a splash!
  dg_affect #10959 %self% off
  halt
end
if %self.varexists(success)%
  set success %self.success%
end
rdelete target %self.id%
rdelete success %self.id%
if %success%
  %echo% ~%self% bursts from the swampy ground, narrowly missing ~%actor%!
else
  %echo% ~%self% bursts from the swampy ground beneath |%actor% feet, sending *%actor% flying!
  %damage% %actor% 200 physical
  dg_affect #10957 %actor% HARD-STUNNED on 10
end
dg_affect #10959 %self% off
~
#10958
Muck Dragon: Swamp Breath~
0 k 100
~
if %self.cooldown(10950)%
  halt
end
nop %self.set_cooldown(10950, 30)%
%echo% &&r~%self% opens ^%self% mouth wide and exhales a cloud of noxious vapor!
set person %self.room.people%
while %person%
  if %person.is_enemy(%self%)%
    %dot% #10958 %person% 100 30 poison
    %damage% %person% 25 poison
  end
  set person %person.next_in_room%
done
~
#10959
Muck dragon burrow charge leap command~
0 c 0
leap~
if %self.varexists(target)%
  set target %self.target%
end
if %actor.id% != %target%
  %send% %actor% You don't need to.
  halt
end
if %self.varexists(success)%
  %send% %actor% You already did.
  halt
end
set success 1
remote success %self.id%
%send% %actor% You desperately leap to one side!
%echoaround% %actor% ~%actor% desperately leaps to one side!
~
#10965
Hill Giant delayed despawn~
1 f 0
~
%adventurecomplete%
~
#10966
Hill Giant load~
0 n 100
~
if %instance.location%
  mgoto %instance.location%
end
%load% obj 10965
%load% mob %self.room.building_vnum%
%purge% %self%
~
#10967
Hill Giant updater~
0 i 100
~
nop %instance.set_location(%self.room%)%
~
#10968
Hill Giant death~
0 f 100
~
nop %instance.set_location(%instance.real_location%)%
set char %self.room.people%
while %char%
  if %char.is_pc% && %char.empire%
    nop %char.empire.start_progress(10965)%
  end
  set char %char.next_in_room%
done
if %instance.real_location%
  set obj %instance.real_location.contents%
  while %obj%
    if %obj.vnum% == 10965
      %purge% %obj%
      %at% %instance.real_location% %load% obj 10966
      halt
    end
    set obj %obj.next_in_list%
  done
end
~
#10970
Golden Harp plays self~
1 bw 12
~
if %self.carried_by%
  halt
end
switch %random.3%
  case 1
    %echo% Sweet music floats off of @%self% as it seemingly plays itself!
  break
  case 2
    %echo% The strings of @%self% vibrate almost imperceptibly as soft music fills the air!
  break
  case 3
    %echo% A disembodied voice sings softly behind the ethereal music of @%self%.
  break
done
~
#10971
Hill Giant fight: Throw Boulder (duck)~
0 k 33
~
if %self.cooldown(10966)%
  halt
end
nop %self.set_cooldown(10966, 30)%
%echo% ~%self% grabs a small boulder from the ground nearby and hefts it!
%echo% &&YYou'd better duck!&&0
set running 1
remote running %self.id%
wait 10 sec
set running 0
remote running %self.id%
%echo% ~%self% hurls the boulder forward and lets it fly!
set room %self.room%
set person %room.people%
while %person%
  if %person.is_pc%
    if %person.health% <= 0
      * Lying down is basically ducking right?
      set command 1
    else
      set command %self.varexists(command_%person.id%)%
      if %command%
        eval command %%self.command_%person.id%%%
        rdelete command_%person.id% %self.id%
      end
    end
    if %command% != duck
      %send% %person% &&rYou are knocked to the ground by the flying boulder!
      %echoaround% %person% ~%person% is knocked to the ground by the flying boulder!
      %damage% %person% 150 physical
      dg_affect #10967 %person% STUNNED on 5
    else
      %send% %person% You throw yourself to the ground, and the boulder flies over your head!
      %echoaround% %person% ~%person% throws *%person%self to the ground!
    end
  end
  set person %person.next_in_room%
done
~
#10972
Hill Giant fight: Club Smash (dive)~
0 k 50
~
if %self.cooldown(10966)%
  halt
end
nop %self.set_cooldown(10966, 30)%
%echo% ~%self% raises ^%self% club for an overhead smash!
%echo% &&YYou'd better dive out of the way!&&0
set running 1
remote running %self.id%
wait 10 sec
set running 0
remote running %self.id%
%echo% ~%self% slams ^%self% club down!
set room %self.room%
set person %room.people%
while %person%
  if %person.is_pc%
    if %person.health% <= 0
      * Can't dodge if incapacitated
    else
      set command %self.varexists(command_%person.id%)%
      if %command%
        eval command %%self.command_%person.id%%%
        rdelete command_%person.id% %self.id%
      end
    end
    if %command% != dive
      %send% %person% &&rYou are smashed to the ground by the force of the blow!
      %echoaround% %person% ~%person% is smashed to the ground by the force of the blow!
      %damage% %person% 150 physical
      dg_affect #10967 %person% STUNNED on 5
    else
      %send% %person% You dive to the side, and the club slams into the ground where you were standing!
      %echoaround% %person% ~%person% dives out of the way!
    end
  end
  set person %person.next_in_room%
done
~
#10973
Hill Giant fight: Quake Stomp (jump)~
0 k 100
~
if %self.cooldown(10966)%
  halt
end
nop %self.set_cooldown(10966, 30)%
%echo% ~%self% lifts ^%self% foot high, preparing for a powerful stomp...
%echo% &&YYou'd better jump!&&0
set running 1
remote running %self.id%
wait 10 sec
set running 0
remote running %self.id%
%echo% ~%self% stomps ^%self% foot down, sending a powerful tremor through the ground!
set room %self.room%
set person %room.people%
while %person%
  if %person.is_pc%
    if %person.health% <= 0
      * Can't dodge if incapacitated
    else
      set command %self.varexists(command_%person.id%)%
      if %command%
        eval command %%self.command_%person.id%%%
        rdelete command_%person.id% %self.id%
      end
    end
    if %command% != jump
      %send% %person% &&rYou are knocked off your feet by the ground tremor!&&0
      %echoaround% %person% ~%person% is knocked to the ground!
      dg_affect #10967 %person% HARD-STUNNED on 5
      dg_affect #10968 %person% DODGE -20 15
    else
      %send% %person% You leap into the air as the ground shakes below you, avoiding the tremor!
      %echoaround% %person% ~%person% leaps into the air, avoiding the tremor!
    end
  end
  set person %person.next_in_room%
done
~
#10974
Hill Giant fight commands~
0 c 0
duck dive jump~
if !%self.running%
  %send% %actor% You don't need to do that right now.
  return 1
  halt
end
if %self.varexists(command_%actor.id%)%
  eval current_command %%self.command_%actor.id%%%
  if %current_command% /= %cmd%
    %send% %actor% You already did.
    halt
  end
end
if duck /= %cmd%
  set command_%actor.id% duck
  %send% %actor% You stand your ground and prepare to duck...
  %echoaround% %actor% ~%actor% stands ^%actor% ground and prepares to duck...
elseif dive /= %cmd%
  %send% %actor% You tense and prepare to dive to one side...
  %echoaround% %actor% ~%actor% tenses, preparing to dive out of the way...
  set command_%actor.id% dive
elseif jump /= %cmd%
  %send% %actor% You crouch and prepare to jump into the air...
  %echoaroud% %actor% ~%actor% crouches and prepares to jump into the air...
  set command_%actor.id% jump
end
remote command_%actor.id% %self.id%
~
#10980
Renegade Caster spawner~
0 n 100
~
if %instance.location%
  mgoto %instance.location%
end
%load% mob %self.room.building_vnum%
if %self.room.building_vnum% == 10983
  %load% mob 10984
  %force% %self.room.people% mfollow %self.room.people.next_in_room%
end
%purge% %self%
~
#10981
Renegade Spellcaster movement~
0 i 100
~
nop %instance.set_location(%self.room%)%
~
#10982
Spellcaster delayed despawn~
1 f 0
~
%adventurecomplete%
~
#10983
Renegade Spellcaster death~
0 f 100
~
* Teen Witch stuff
if %self.vnum% == 10983 || %self.vnum% == 10984
  set other_witch %instance.mob(10983)%
  if !%other_witch%
    set other_witch %instance.mob(10984)%
  end
  if %other_witch%
    nop %other_witch.remove_mob_flag(!LOOT)%
  end
  set victim %self.room.people%
  while %victim%
    set spell_broken 0
    if %victim.morph% == 10992
      if !%spell_broken%
        %echo% As ~%self% dies, ^%self% spell is broken!
      end
      set spell_broken 1
      set prev_name %victim.name%
      morph %victim% normal
      %echoaround% %victim% %prev_name% suddenly turns into ~%victim%!
      %send% %victim% You suddenly return to your normal form!
      dg_affect #10992 %victim% off
    end
    set victim %victim.next_in_room%
  done
end
nop %instance.set_location(%instance.real_location%)%
%at% %instance.real_location% %load% obj 10980 room
set char %self.room.people%
while %char%
  if %char.is_pc% && %char.empire%
    nop %char.empire.start_progress(10980)%
  end
  set char %char.next_in_room%
done
~
#10984
Hostile Spellcaster Reaction~
0 e 1
you~
if %actor.is_npc%
  halt
end
wait 1
switch %self.vnum%
  case 10981
    say You have crossed the wrong wizard!
  break
  case 10982
    say It seems the spirits want you dead...
  break
  case 10983
    say As if!
  break
  case 10984
    say Whatever!
  break
done
wait 2 sec
if %actor.room% != %self.room%
  halt
end
nop %self.add_mob_flag(AGGR)%
detach 10984 %self.id%
set other_witch %instance.mob(10983)%
if !%other_witch%
  set other_witch %instance.mob(10984)%
end
if %other_witch%
  nop %other_witch.add_mob_flag(AGGR)%
end
~
#10985
Rogue Wizard: Portal Attack~
0 k 33
~
if %self.cooldown(10981)%
  halt
end
nop %self.set_cooldown(10981, 30)%
if %self.affect(3021)%
  dg_affect #3021 %self% off
else
  %echo% ~%self% flickers momentarily with a blue-white aura.
end
dg_affect #3021 %self% COUNTERSPELL on 35
set verify_target %actor.id%
wait 1 sec
if %verify_target% != %actor.id%
  halt
end
%echo% ~%self% opens a tiny portal and reaches inside...
if !%self.varexists(rare_summon_done)%
  if %random.10% == 10
    set rare_summon_done 1
    remote rare_summon_done %self.id%
    %send% %actor% ~%self% withdraws a silver ingot and hurls it at you!
    %send% %actor% The silver ingot falls to the ground at your feet.
    %echoaround% %actor% ~%self% withdraws a silver ingot and hurls it at ~%actor%!
    %echoaround% %actor% The silver ingot falls to the ground at ^%actor% feet.
    %echo% ~%self% mutters a series of increasingly profane curses.
    %load% obj 170 room
    halt
  end
end
switch %random.2%
  case 1
    %send% %actor% ~%self% withdraws a rock and hurls it at you!
    %send% %actor% &&rThe rock bounces off your head, stunning you!
    %echoaround% %actor% ~%self% withdraws a rock and hurls it at ~%actor%!
    %echoaround% %actor% The rock bounces off ^%actor% head, and &%actor% staggers!
    %damage% %actor% 50 physical
    dg_affect #10984 %actor% STUNNED on 5
  break
  case 2
    %send% %actor% ~%self% withdraws a throwing dagger and hurls it at you!
    %send% %actor% &&rThe dagger hits you in the shoulder, opening a bleeding wound!
    %echoaround% %actor% ~%self% withdraws a small dagger and hurls it at ~%actor%!
    %echoaround% %actor% The dagger hits *%actor% in the shoulder, drawing blood!
    %damage% %actor% 50 physical
    %dot% #10985 %actor% 100 15 physical
  break
done
~
#10986
Rogue Wizard: Energy Drain~
0 k 50
~
if %self.cooldown(10981)%
  halt
end
nop %self.set_cooldown(10981, 30)%
if %self.affect(3021)%
  dg_affect #3021 %self% off
else
  %echo% ~%self% flickers momentarily with a blue-white aura.
end
dg_affect #3021 %self% COUNTERSPELL on 35
set verify_target %actor.id%
wait 1 sec
if %verify_target% != %actor.id%
  halt
end
if %actor.trigger_counterspell(%self%)%
  %send% %actor% ~%self% shouts some kind of hex at you, but your counterspell dispels it!
  %echoaround% %actor% ~%self% shouts some kind of hex at ~%actor%, but nothing seems to happen!
else
  %echoaround% %actor% ~%self% shouts some kind of hex at ~%actor%, who seems weakened!
  %send% %actor% ~%self% shouts something at you, and you feel your energy drain away!
  %damage% %actor% 25 magical
  dg_affect #10986 %actor% SLOW on 15
  %echo% ~%self% suddenly fights with renewed strength!
  dg_affect #10986 %self% HASTE on 15
end
~
#10987
Rogue Wizard: Fire Ritual~
0 k 100
~
if %self.cooldown(10981)%
  halt
end
nop %self.set_cooldown(10981, 30)%
if %self.affect(3021)%
  dg_affect #3021 %self% off
else
  %echo% ~%self% flickers momentarily with a blue-white aura.
end
dg_affect #3021 %self% COUNTERSPELL on 35
set verify_target %actor.id%
wait 1 sec
%echo% ~%self% begins drawing mana to *%self%self...
%echo% (You should 'interrupt' ^%self% ritual.)
set ritual_active 1
remote ritual_active %self.id%
wait 5 sec
if !%self.ritual_active%
  halt
end
%echo% ~%self% concentrates ^%self% power into a whip of crackling flames!
wait 5 sec
if !%self.ritual_active%
  halt
end
if %verify_target% != %actor.id%
  %echo% |%self.% whip fades as ^%self% target is no longer viable.
  halt
end
%send% %actor% &&r~%self% cracks the blazing whip at you, blowing you off your feet!
%echoaround% %actor% ~%self% cracks the blazing whip at ~%actor%, blowing *%actor% off ^%actor% feet!
if %actor.trigger_counterspell(%self%)%
  %send% %actor% The fiery whip crashes through your counterspell unimpeded, breaking it!
end
%damage% %actor% 200 magical
dg_affect #10984 %actor% HARD-STUNNED on 5
~
#10988
Rogue Wizard: Interrupt Ritual~
0 c 0
interrupt~
if !%self.varexists(ritual_active)%
  %send% %actor% You don't need to do that right now.
end
if !%self.ritual_active%
  %send% %actor% You don't need to do that right now.
end
%send% %actor% You trip ~%self%, interrupting ^%self% ritual.
%echoaround% %actor% ~%actor% trips ~%self%, interrupting ^%self% ritual.
set ritual_active 0
remote ritual_active %self.id%
~
#10989
Rogue Druid: Morph Attack~
0 k 33
~
if %self.cooldown(10981)%
  halt
end
nop %self.set_cooldown(10981, 30)%
set verify_target %actor.id%
if %self.affect(3021)%
  dg_affect #3021 %self% off
else
  %echo% ~%self% flickers momentarily with a blue-white aura.
end
dg_affect #3021 %self% COUNTERSPELL on 35
wait 1 sec
if !%self.morph%
  set current %self.name%
  %morph% %self% 10988
  %echo% %current% rapidly morphs into ~%self%!
  wait 1 sec
end
%echo% &&r~%self% goes berserk, claws flailing in all directions!
if %verify_target% != %actor.id%
  %damage% %actor% 75 physical
end
%aoe% 25 physical
~
#10990
Rogue Druid: Dust Devil~
0 k 50
~
if %self.cooldown(10981)%
  halt
end
nop %self.set_cooldown(10981, 30)%
if %self.affect(3021)%
  dg_affect #3021 %self% off
else
  %echo% ~%self% flickers momentarily with a blue-white aura.
end
dg_affect #3021 %self% COUNTERSPELL on 35
set verify_target %actor.id%
wait 1 sec
if %self.morph%
  set current %self.name%
  %morph% %self% normal
  %echo% %current% rapidly morphs into ~%self%!
  wait 1 sec
end
if %verify_target% != %actor.id%
  halt
end
%echo% ~%self% makes a sweeping skyward gesture with both arms!
if %actor.trigger_counterspell(%self%)%
  %send% %actor% The dust around your feet swirls gently in a circle, then your counterspell stops it.
  %echoaround% %actor% The dust around |%actor% feet swirls gently in a circle, then stops.
else
  dg_affect #10982 %self% HARD-STUNNED on 10
  %echo% A swirling dust devil abruptly envelops ~%actor% and hurls *%actor% into the air!
  %damage% %actor% 50 magical
  dg_affect #10990 %actor% HARD-STUNNED on 10
  wait 5 sec
  if %verify_target% != %actor.id%
    dg_affect #10982 %self% off
    halt
  end
  if %actor.affect(10990)%
    %echo% ~%self% slams ^%self% fist into the earth!
    %send% %actor% &&rA gust of wind suddenly hurls you downward!
    %echoaround% %actor% ~%actor% is abruptly hurled downward!
    dg_affect #10990 %actor% off
    %damage% %actor% 100 physical
  end
  dg_affect #10982 %self% off
end
~
#10991
Rogue Druid: Dust Cloud~
0 k 100
~
if %self.cooldown(10981)%
  halt
end
nop %self.set_cooldown(10981, 30)%
if %self.affect(3021)%
  dg_affect #3021 %self% off
else
  %echo% ~%self% flickers momentarily with a blue-white aura.
end
dg_affect #3021 %self% COUNTERSPELL on 35
wait 1 sec
if %self.morph%
  set current %self.name%
  %morph% %self% normal
  %echo% %current% rapidly morphs into ~%self%!
  wait 1 sec
end
%echo% ~%self% slams ^%self% hands to the earth, fingers spread!
%echo% A thick cloud of dust explodes outward, filling the area and obscuring your vision!
set person %self.room.people%
while %person%
  if %person% != %self%
    dg_affect #10991 %person% BLIND on 20
  end
  set person %person.next_in_room%
done
wait 15 sec
%echo% The dust cloud settles.
set person %self.room.people%
while %person%
  if %person% != %self%
    dg_affect #10991 %person% off
  end
  set person %person.next_in_room%
done
~
#10992
Teen Witch: Baleful Polymorph~
0 k 33
~
if %self.cooldown(10981)%
  halt
end
nop %self.set_cooldown(10981, 30)%
if %self.affect(3021)%
  dg_affect #3021 %self% off
else
  %echo% ~%self% flickers momentarily with a blue-white aura.
end
dg_affect #3021 %self% COUNTERSPELL on 35
set id %actor.id%
wait 1 sec
say Don't think of this as ruining your chances for a date. Think of it as your only chance to kiss a princess!
wait 2 sec
%echo% ~%self% snaps ^%self% fingers!
set actor %self.fighting%
if !%actor% || %actor.id% != %id% || %actor.morph% == 10992
  %echo% ~%self% looks confused.
  halt
end
if %actor.trigger_counterspell(%self%)%
  %echo% Nothing seems to happen.
  halt
end
set prev_name %actor.name%
%morph% %actor% 10992
%send% %actor% You are abruptly transformed into %actor.name%!
%echoaround% %actor% %prev_name% is abruptly transformed into %actor.name%!
dg_affect #10992 %actor% STUNNED on 20
wait 15 sec
if !%actor% || %actor.id% != %id%
  halt
elseif %actor.morph% == 10992
  set prev_name %actor.name%
  %morph% %actor% normal
  %echoaround% %actor% %prev_name% slowly shifts back into ~%actor%.
  %send% %actor% Your form returns to normal.
  dg_affect #10992 %actor% off
end
~
#10993
Teen Witch: Ugly Stick~
0 k 50
~
if %self.cooldown(10981)%
  halt
end
nop %self.set_cooldown(10981, 30)%
if %self.affect(3021)%
  dg_affect #3021 %self% off
else
  %echo% ~%self% flickers momentarily with a blue-white aura.
end
dg_affect #3021 %self% COUNTERSPELL on 35
set id %actor.id%
wait 1 sec
if !%actor% || %actor.id% != %id% || !%self.fighting%
  halt
end
%send% %actor% &&r~%self% pulls out a gnarled wooden staff and smacks you over the head with it!
%echoaround% %actor% ~%self% pulls out a gnarled wooden staff and smacks ~%actor% over the head with it!
%damage% %actor% 25 physical
if %actor.trigger_counterspell(%self%)%
  %send% %actor% Your counterspell protects you from the staff's enchantment.
  halt
end
dg_affect #10993 %actor% STRENGTH -3 15
dg_affect #10994 %actor% INTELLIGENCE -3 15
dg_affect #10994 %actor% CHARISMA -3 15
~
#10994
Teen Witch: Expelliarmus~
0 k 100
~
if %self.cooldown(10981)%
  halt
end
nop %self.set_cooldown(10981, 30)%
if %self.affect(3021)%
  dg_affect #3021 %self% off
else
  %echo% ~%self% flickers momentarily with a blue-white aura.
end
dg_affect #3021 %self% COUNTERSPELL on 35
set id %actor.id%
wait 1 sec
if !%actor% || %actor.id% != %id%
  halt
end
if !%actor.is_pc%
  set person %self.room.people%
  while %person%
    if %person.is_pc% && %person.is_enemy(%self%)%
      set actor %person%
    end
    set person %person.next_in_room%
  done
end
%send% %actor% ~%self% pulls out a small wand and points it at you.
%echoaround% %actor% ~%self% pulls out a small wand and points it at ~%actor%.
say Expelliarmus!
if %actor.trigger_counterspell(%self%)%
  %send% %actor% Your weapon twitches in your hand, then stops.
else
  %send% %actor% Your weapon flies out of your hand!
  %echoaround% %actor% |%actor% weapon flies out of ^%actor% hand!
  dg_affect #10995 %actor% DISARMED on 20
end
~
#10995
Teen Witch: Kiss Frog~
0 e 0
kisses~
if %victim.morph% != 10992
  halt
end
wait 4
if %victim.morph% == 10992
  set prev_name %victim.name%
  morph %victim% normal
  %send% %actor% As you kiss %prev_name%, it suddenly turns into ~%victim%!
  %send% %victim% As ~%actor% kisses you, you suddenly return to your normal form!
  %echoneither% %actor% %victim% As ~%actor% kisses %prev_name%, it suddenly transforms into ~%victim%!
  dg_affect #10992 %victim% off
end
~
#10998
Mother's grimoire emotes~
0 btw 10
~
if !%self.leader%
  halt
end
switch %random.4%
  case 1
    %echo% ~%self% flips to a page marked 'Grooming' and begins to glow...
    %send% %self.leader% A blue light flies toward you, straightening your hair!
    %echoaround% %self.leader% A blue light flies toward %self.leader.name% and suddenly %self.leader.hisher% hair straightens itself!
  break
  case 2
    %echo% ~%self% flips to a page marked 'Hygiene' and begins to glow...
    %send% %self.leader% A white light flies into your mouth, polishing your teeth!
    %echoaround% %self.leader% A white light flies toward %self.leader.name%, into %self.leader.hisher% mouth, and polishing %self.leader.hisher% teeth!
  break
  case 3
    %echo% ~%self% flips to a page marked 'Food' and begins to glow...
    %send% %self.leader% A yellow light flies into your hand, revealing a chocolate frog!
    %echoaround% %self.leader% A yellow light flies into %self.leader.name%'s hand, revealing a chocolate frog!
    %load% obj 10998 %self.leader% inv
  break
  case 4
    %echo% ~%self% flips to a page marked 'Smudges' and begins to glow...
    %send% %self.leader% A green light flies toward you, removing a smudge of dirt from your face!
    %echoaround% %self.leader% A green light flies toward %self.leader.name%, removing a smudge of dirt from %self.leader.hisher% face!
  break
done
~
$
