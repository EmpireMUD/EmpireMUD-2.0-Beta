#12300
Fur Dragon load, restring, and move~
0 n 100
~
set loc %instance.real_location%
if !%loc%
  halt
end
* move
mgoto %loc%
* determine dragon type
set type %loc.building_vnum%
remote type %self.id%
* restring mob
switch %type%
  case 12300
    * temperate plains
    %mod% %self% keywords dragon fur frisky enormous
    %mod% %self% shortdesc the frisky fur dragon
    %mod% %self% longdesc An enormous fur dragon leaps around, dangerously frisky for its size. (adventure)
  break
  case 12301
    * temperate forest
    %mod% %self% keywords dragon fur lithe enormous
    %mod% %self% shortdesc the lithe fur dragon
    %mod% %self% longdesc An enormous fur dragon stalks silently through the trees. (adventure)
  break
  case 12302
    * desert
    %mod% %self% keywords dragon fur wily enormous
    %mod% %self% shortdesc the wily fur dragon
    %mod% %self% longdesc An enormous fur dragon stalks low to the ground, nearly hidden in the dust. (adventure)
  break
  case 12303
    * jungle
    %mod% %self% keywords dragon fur savage enormous
    %mod% %self% shortdesc the savage fur dragon
    %mod% %self% longdesc An enormous fur dragon stalks through the jungle. (adventure)
  break
done
* attempt to move 5 times
set room %self.room%
set count 0
set moved 0
while %count% < 50 && %moved% < 5
  eval count %count% + 1
  mmove
  if %self.room% != %room%
    eval moved %moved% + 1
    set room %self.room%
  end
done
* turn on no-attack (until scaled)
dg_affect %self% !ATTACK on -1
~
#12301
Fur Dragon: leash and update loc~
0 i 100
~
* max distance from home (configurable)
set leash_distance 25
set room %self.room%
set origin %instance.real_location%
if %origin% && %room.distance(%origin%)% > %leash_distance%
  return 0
  halt
end
* update instance location
nop %instance.set_location(%room%)%
~
#12302
Fur Dragon: delayed despawn~
1 f 0
~
%adventurecomplete%
~
#12303
Fur Dragon: block enter portal when scaled~
0 c 0
enter~
return 0
set portal %actor.obj_target(%arg%)%
if !%portal%
  halt
elseif %portal.vnum% != 12301
  halt
elseif !%self.varexists(scaled)% || %actor.nohassle% || %actor.level% < 225
  halt
else
  %send% %actor% ~%self% won't let you leave!
  return 1
end
~
#12304
Fur Dragon: can't leave the burrow once scaled~
0 q 100
~
if !%self.varexists(scaled)% || %actor.nohassle% || %actor.level% < 225
  return 1
else
  %send% %actor% ~%self% won't let you leave!
  return 0
end
~
#12305
Fur Dragon: difficulty selection and retreat to burrow~
0 c 0
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty. (Hard, Group, or Boss)
  return 1
  halt
end
if %self.fighting%
  %send% %actor% You can't change |%self% difficulty while &%self% is in combat!
  return 1
  halt
end
if hard /= %arg%
  set difficulty 2
elseif group /= %arg%
  set difficulty 3
elseif boss /= %arg%
  set difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure. (Hard, Group, or Boss)
  return 1
  halt
end
* messaging
%send% %actor% You set the difficulty...
%echoaround% %actor% ~%actor% sets the difficulty...
* Clear existing difficulty flags and set new ones.
set mob %self%
nop %mob.remove_mob_flag(HARD)%
nop %mob.remove_mob_flag(GROUP)%
if %difficulty% == 1
  * Then we don't need to do anything
elseif %difficulty% == 2
  %echo% ~%self% has been set to Hard.
  nop %mob.add_mob_flag(HARD)%
elseif %difficulty% == 3
  %echo% ~%self% has been set to Group.
  nop %mob.add_mob_flag(GROUP)%
elseif %difficulty% == 4
  %echo% ~%self% has been set to Boss.
  nop %mob.add_mob_flag(HARD)%
  nop %mob.add_mob_flag(GROUP)%
end
%restore% %mob%
wait 1
* send mob home
set loc %instance.nearest_rmt(12300)%
if %loc% && %self.room% != %loc%
  %echo% ~%self% suddenly leaps high into the air, spreads leathery wings that were hidden under its fur, and flies off!
  mgoto %loc%
  nop %self.add_mob_flag(SENTINEL)%
  nop %instance.set_location(%loc%)%
  %regionecho% %loc% -30 You see a fur dragon fly home to its burrow at %loc.coords%!
  %echo% ~%self% enters the burrow through the hole and blocks the entrance!
  * update desc
  if %self.varexists(type)%
    switch %self.type%
      case 12300
        * temperate plains
        %mod% %self% longdesc The frisky fur dragon is blocking the entrance to the burrow!
      break
      case 12301
        * temperate forest
        %mod% %self% longdesc The lithe fur dragon has snuck behind you and is blocking the entrance!
      break
      case 12302
        * desert
        %mod% %self% keywords dragon fur wily enormous
        %mod% %self% shortdesc the wily fur dragon
        %mod% %self% longdesc The wily fur dragon has trapped you in here!
      break
      case 12303
        * jungle
        %mod% %self% longdesc The savage fur dragon has trapped you and is blocking the entrance!
      break
    done
  end
  * remove no-attack
  if %mob.aff_flagged(!ATTACK)%
    dg_affect %mob% !ATTACK off
  end
  * mark me as scaled
  set scaled 1
  remote scaled %self.id%
~
#12306
Fur Dragon: attack info before difficulty selection~
0 c 0
kill hit kick bash lightningbolt backstab skybrand entangle shoot sunshock enervate slow siphon disarm job blind sap prick~
if %self.aff_flagged(!ATTACK)%
  if %actor.char_target(%arg%)% != %self%
    return 0
    halt
  end
  %send% %actor% You need to choose a difficulty before you can attack ~%self%.
  %send% %actor% Usage: difficulty <hard \| group \| boss>
  %echoaround% %actor% ~%actor% considers attacking ~%self%.
  return 1
else
  * no need for this script anymore
  detach 12306 %self.id%
  return 0
end
~
#12307
Fur Dragon: death of the dragon~
0 f 100
~
if %self.room.template% == 12300
  %load% obj 12302 room
else
  %at% i12300 %load% obj 12302 room
end
set person %self.room.people%
while %person%
  if %person.vnum% == 12318
    %purge% %person%
  end
  set person %person.next_in_room%
done
~
#12308
Fur Dragon: adventure cleanup~
2 e 100
~
if %room.building_vnum% >= 12300 && %room.building_vnum% <= 12303
  %build% %room% 12308
end
~
#12309
Fur Dragon: furry drake familiar flute~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
* otherwise always return 1
return 1
* check if already knows it
if %actor.has_companion(12308)%
  %send% %actor% You already have that familiar in your companion list.
  halt
end
* check ability 122 Familiar
if !%actor.ability(122)%
  %send% %actor% You need to have the Familiar ability to use @%self%.
  halt
end
* grant familiar
nop %actor.add_companion(12308)%
%send% %actor% You blow the flute and gain a furry dragon companion! (Type 'companions' to see your list.)
%echoaround% %actor% %actor% uses @%self%.
%purge% %self%
~
#12310
Fur Dragon: furry drake load script~
0 nt 100
~
wait 1
set pc %self.leader%
* check for leader
if !%pc%
  %echo% ~%self% runs away.
  %purge% %self%
  halt
end
* check for familiar
if !%pc.ability(122)%
  %send% %pc% You must have the Familiar ability to summon ~%self%.
  %echo% ~%self% runs away.
  %purge% %self%
  halt
end
~
#12311
Fur Dragon: greet and slaughter and start progress~
0 h 100
~
set room %self.room%
* attempt to kill an npc if it qualifies
if %actor.is_npc% && !%actor.leader% && %self.can_see(%actor%)%
  %echo% ~%self% crouches low and tries to hide...
  * short wait (store id to prevent errors)
  set id %actor.id%
  wait 1
  * still here?
  if %actor.room% == %room% && %actor.id% == %id%
    %echoaround% %actor% ~%self% pounces on ~%actor%, biting hard into ^%actor% neck, killing *%person%!
    %send% %actor% ~%self% pounces out of nowhere and the last thing you feel is its jaws closing around your neck!
    %slay% %actor%
  end
end
* attempt to start the progression on everybody (still) here
wait 1
set ch %room.people%
while %ch%
  if %ch.is_pc% && %ch.empire%
    nop %ch.empire.start_progress(12300)%
  end
  set ch %ch.next_in_room%
done
~
#12314
Fur Dragon: block learn on patterns~
1 c 2
learn~
* ensure it targeted me
if %actor.obj_target_inv(%arg%)% != %self%
  return 0
  halt
end
* check vals present
if !%self.varexists(empire_id)% || !%self.val0%
  %send% %actor% You can't seem to learn anything from @%self%.
  return 1
  halt
end
if %actor.empire.id% != %self.empire_id% && %actor.id% != %self.player_id%
  %send% %actor% Only members of %self.empire_name% may learn this recipe.
  return 1
  halt
end
* if they got here, it's valid so just return 0
return 0
~
#12315
Fur Dragon: loot/craft bop/boe twiddler and restringer~
1 n 100
~
* items default to BOP but are set BOE if they come from a craft
* if they remain BOP, this will also randomly restring them
* Configs:
set adjective_list chewed-up mauled tattered battered frayed ripped torn shabby scraggy seedy shoddy burnt scorched damaged warped busted pulverized squashed ravaged dinged-up
set adjective_count 20
set add_empire_name_to 12336 12337 12338 12339 12340
set fake_empire_name Nightmare Eternal Fell Lost Dread Unquiet Haunted Ghastly
set fake_empire_name_count 8
set fake_empire_type Imperium Kingdom Reign Realm Host Lands Rule
set fake_empire_type_count 7
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
end
if !%actor%
  halt
end
if %actor% && %actor.is_pc%
  * Item was crafted or bought
  if %self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if !%self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
  * Probably need to unbind when BOE
  nop %self.bind(nobody)%
  * restring some BOEs with empire name
  if %actor.empire% && %add_empire_name_to% ~= %self.vnum%
    %mod% %self% keywords %self.keywords% %actor.empire.name%
    %mod% %self% shortdesc %self.shortdesc% of %actor.empire.name%
    %mod% %self% append-lookdesc It bears the crest of %actor.empire.name%.
  end
else
  * Item was probably dropped as loot
  if !%self.is_flagged(BOP)%
    nop %self.flag(BOP)%
  end
  if %self.is_flagged(BOE)%
    nop %self.flag(BOE)%
  end
  * restringing: choose strings
  eval random_pos %%random.%adjective_count%%%
  while %random_pos% > 0
    set adjective %adjective_list.car%
    set adjective_list %adjective_list.cdr%
    eval random_pos %random_pos% - 1
  done
  if !%adjective%
    * somehow?
    set adjective dilapidated
  end
  * restringing: determine start
  set short_car %self.shortdesc.car%
  if (%self.keywords% ~= pair && !(%self.shortdesc% ~= pair))
    * add anything that would make this need a "some"
    set prefix some
    set short %self.shortdesc%
    set is_are are
  elseif %short_car% == a || %short_car% == an || %short_car% == the
    * basic insert
    set prefix %adjective.ana%
    set short %self.shortdesc.cdr%
    set is_are is
  else
    * this is a guess? might also just set it to 0 to skip prefixes
    set prefix %adjective.ana%
    set short %self.shortdesc%
    set is_are is
  end
  * restringing: build strings
  %mod% %self% keywords %self.keywords% %adjective%
  if %prefix%
    %mod% %self% shortdesc %prefix% %adjective% %short%
    %mod% %self% longdesc %prefix.cap% %adjective% %short% %is_are% lying here.
  else
    %mod% %self% shortdesc %adjective% %short%
    %mod% %self% longdesc %adjective.cap% %short% %is_are% lying here.
  end
  * restringing: add fake empire name?
  if %add_empire_name_to% ~= %self.vnum%
    * make up a random fake empire
    eval random_pos %%random.%fake_empire_name_count%%%
    while %random_pos% > 0
      set fake_name %fake_empire_name.car%
      set fake_empire_name %fake_empire_name.cdr%
      eval random_pos %random_pos% - 1
    done
    if !%fake_name%
      set fake_name Forgotten
    end
    eval random_pos %%random.%fake_empire_type_count%%%
    while %random_pos% > 0
      set fake_type %fake_empire_type.car%
      set fake_empire_type %fake_empire_type.cdr%
      eval random_pos %random_pos% - 1
    done
    if !%fake_type%
      set fake_type Empire
    end
    %mod% %self% keywords %self.keywords% %fake_name% %fake_type%
    %mod% %self% shortdesc %self.shortdesc% of the %fake_name% %fake_type%
    %mod% %self% append-lookdesc It bears the crest of the %fake_name% %fake_type%.
  end
  * restringing: add to the look desc
  %mod% %self% append-lookdesc It looks like the last owner's fateful encounter with a fur dragon has left it a bit %adjective%.
  set keywords %self.keywords%
  %mod% %self% append-lookdesc-noformat Type 'study %keywords.car%' to take it apart and learn to craft it.
  %mod% %self% append-lookdesc-noformat (Be sure to 'keep' any copies of it you don't want to lose.)
  * add study script
  attach 12316 %self.id%
end
detach 12315 %self.id%
~
#12316
Fur Dragon: study loot to learn craft~
1 c 2
study~
* Note: requires 1x boss, 2x group, 3x hard, or 1x group + 1x hard (of same vnum)
return 1
set recipe_vnum %self.vnum%
if !%arg%
  * there's no default study command so give an error here
  %send% %actor% Study what?
  halt
elseif %actor.obj_target_inv(%arg%)% != %self%
  * possibly trying to study something else
  return 0
  halt
elseif !%actor.empire%
  %send% %actor% You need to be in an empire to do this. Only members of your empire will be able to use the notes.
  halt
end
* store empire vars
set empire_id %actor.empire.id%
set empire_name %actor.empire.name%
set empire_adjective %actor.empire.adjective%
* check amounts
set points 0
set kept 0
set take1 0
set take2 0
set take3 0
set take1_type 0
set take2_type 0
set take3_type 0
set item %actor.inventory%
while %item% && %points% < 3
  set next_item %item.next_in_list%
  set use 0
  set this_type 0
  if %item.vnum% == %self.vnum%
    * same item: see if it's available and count points
    if %item.is_flagged(*KEEP)%
      set kept 1
    elseif %item.is_flagged(HARD-DROP)% && %item.is_flagged(GROUP-DROP)%
      eval points %points% + 3
      set use 1
      set this_type 3
    elseif %item.is_flagged(GROUP-DROP)%
      eval points %points% + 2
      set use 1
      set this_type 2
    elseif %item.is_flagged(HARD-DROP)%
      eval points %points% + 1
      set use 1
      set this_type 1
    end
    * did we use it? save it for extraction
    if %use%
      if !%take1%
        set take1 %item%
        set take1_type %this_type%
      elseif !%take2%
        set take2 %item%
        set take2_type %this_type%
      else
        set take3 %item%
        set take3_type %this_type%
      end
    end
    * end same-vnum
  end
  set item %next_item%
done
* did we find enough? or any?
if %points% == 0 && %kept%
  %send% %actor% Studying @%self% will destroy it -- you can't do it if it's flagged (keep).
  halt
elseif %points% < 3
  %send% %actor% You'll need more of these to study them (you need 2 group-drop or 3 hard-drop copies).
  if %kept%
    %send% %actor% Check to see if some of them are flagged (keep).
  end
  halt
end
* ok create the pattern item...
%load% obj 12314 %actor% inv
set pattern %actor.inventory%
if %pattern.vnum% != 12314
  %send% %actor% Something went wrong trying to study it.
  halt
end
nop %pattern.val0(%recipe_vnum%)%
%mod% %pattern% keywords study %self.keywords% %empire_adjective%
%mod% %pattern% shortdesc %empire_adjective.ana% %empire_adjective% study of %self.shortdesc%
%mod% %pattern% longdesc %empire_adjective.ana.cap% %empire_adjective% study of %self.shortdesc% is lying here.
%mod% %pattern% lookdesc It looks like someone from %empire_name% has written notes on %self.shortdesc% that %actor.heshe% was studying.
%mod% %pattern% append-lookdesc-noformat Type 'learn study' to learn to make this pattern.
remote empire_name %pattern.id%
remote empire_id %pattern.id%
remote empire_adjective %pattern.id%
set player_id %actor.id%
remote player_id %pattern.id%
* messaging...
%send% %actor% You study @%self% and write out some notes on crafting it (in your inventory).
%echoaround% %actor% ~%actor% studies @%self% and writes out some notes.
* and take the items, in priority order:
if %take1% && %take1_type% == 3
  * only need take1
  %send% %actor% You had to take apart @%take1% (level %take1.level% boss-drop).
  %purge% %take1%
elseif %take2% && %take2_type% == 3
  * only need take2
  %send% %actor% You had to take apart @%take2% (level %take2.level% boss-drop).
  %purge% %take2%
elseif %take3% && %take3_type% == 3
  * only need take3
  %send% %actor% You had to take apart @%take3% (level %take3.level% boss-drop).
  %purge% %take3%
else
  * otherwise need to take multiple items and we'll go left-to-right
  set must_purge_self 0
  set points 0
  set counter 1
  while %counter% < 3 && %points% < 3
    eval item %%take%counter%%%
    if %item%
      if %item.is_flagged(GROUP-DROP)%
        eval points %points% + 2
        %send% %actor% You had to take apart @%item% (level %item.level% group-drop).
      else
        eval points %points% + 1
        %send% %actor% You had to take apart @%item% (level %item.level% hard-drop) to do it.
      end
      if %item% == %self%
        set must_purge_self 1
      else
        %purge% %item%
        unset take%counter%
      end
    end
    eval counter %counter% + 1
  done
  * ok done.. did we need to get rid of ourself?
  if %must_purge_self%
    %purge% %self%
  end
end
~
#12317
Fur Dragon Combat: Baby spawner~
0 k 30
~
if %self.cooldown(12317)% || %self.cooldown(12318)%
  halt
end
set difficulty 1
if %self.mob_flagged(hard)%
  eval defficulty %difficulty% + 1
end
if %self.mob_flagged(group)%
  eval difficulty %difficulty% + 2
end
switch %difficulty%
  case 1
    set roll 1
  break
  case 2
    eval roll %random.84%
  break
  case 3
    eval roll %random.90%
  break
  case 4
    eval roll %random.100%
  break
done
%echo% ~%self% shifts as you hear growling from somewhere.
wait 3
if %roll% > 95
  set SpawnStr a few fledgling fur dragons approach
  set FledglingCount 3
elseif %roll% > 80
  set SpawnStr a couple fledgling fur dragons approach
  set FledglingCount 2
else
  set SpawnStr a fledgling fur dragon approaches
  set FledglingCount 1
  set str s
end
%echo% Suddenly, %SpawnStr% from the corner and attack%str%!
while %FledglingCount%
  %load% mob 12318 ally
  eval FledglingCount %FledglingCount% - 1
done
eval timer 60 - %difficulty% * 10
nop %self.set_cooldown(12317, %timer%)%
~
#12318
Fur Dragon Combat: Purge baby dragons on enter~
0 h 100
~
set mother %self.leader%
if !%mother.fighting%
  %echo% ~%self% runs and hides!
  %purge% %self%
end
~
#12319
Fur Dragon Combat: Itchy mother~
0 b 15
~
if !%self.fighting%
  halt
end
%echo% ~%self% whines as &%self% rubs an ear against the nearest hard surface. Maybe you should scratch it?
set person %self.room.people%
while %person%
  if %person.varexists(FurScratching)%
    rdelete FurScratching %person.id%
  end
  set person %person.next_in_room%
done
nop %self.set_cooldown(12318, 11)%
wait 10 s
set person %self.room.people%
while %person%
  if %person.is_pc%
    eval PlayerCount %PlayerCount% + 1
    if %person.varexists(FurScratching)%
      rdelete FurScratching %person.id%
      eval CountScratched %CountScratched% + 1
    end
  end
  set person %person.next_in_room%
done
if %CountScratched% >= %PlayerCount%
  dg_affect #12319 %self% off
  %echo% ~%self% purs in satisfaction and seems to calm down a bit.
  halt
end
eval buff ( %PlayerCount% - %CountScratched% ) * 10
if %self.mob_flagged(group)%
  eval buff %buff% * 2
end
dg_affect #12319 %self% BONUS-PHYSICAL %buff% -1
%echo% ~%self% seems to become enraged and fights with a greater furiosity!
~
#12320
Fur Dragon Combat: Scratch the ear~
0 c 0
scratch~
if %arg% != ear
  set target %actor.char_target(%arg%)%
else
  set target ear
end
if !%target%
  %send% %actor% Just what or whom are you trying to scratch?
  halt
end
if %arg% != ear && %target% != %self%
  return 0
  halt
end
if !%self.fighting%
  %send% %actor% It doesn't look like right now is the time to be scratching ~%self%.
  halt
end
if !%self.cooldown(12318)%
  set difficulty 1
  if %self.mob_flagged(hard)%
    eval difficulty %difficulty% + 1
  end
  if %self.mob_flagged(group)%
    eval difficulty %difficulty% + 2
  end
  %send% %actor% ~%self% doesn't seem to be itchy and &%self% returns the favor!
  %damage% %actor% 80 physical
  eval difficulty %difficulty% * 15
  %dot% #12320 %actor% 40 %difficulty% direct 10
  halt
end
%send% %actor% You reach out and scratch ~%self% behind ^%self% ear.
%eachoaround %actor% ~%actor% reaches out and scratches ~%self% behind ^%self% ear.
set FurScratching 1
remote FurScratching %actor.id%
~
#12321
buff the baby fur dragon~
0 b 30
~
if !%self.fighting%
  %echo% ~%self% hisses and vanishes in a puff of fur!
  %purge% %self%
  halt
end
set person %self.room.people%
while %person%
  if %person.vnum% == 12318
    eval count %count% + 1
    if %person% == %self%
      set IAm %count%
    end
  end
  set person %person.next_in_room%
done
switch %IAm%
  case 1
    set IAm first
  break
  case 2
    set IAm second
  break
  case 3
    set IAm third
  break
  case 4
    set IAm fourth
  break
  case 5
    set IAm fifth
  break
  case 6
    set IAm sixth
  break
  case 7
    set IAm seventh
  break
  case 8
    set IAm eighth
  break
  case 9
    set IAm ninth
  break
  case 10
    set IAm tenth
  break
  case 11
    set IAm eleventh
  break
  case 12
    set IAm twelth
  break
  case 13
    set IAm thirteenth
  break
  case 14
    set IAm fourteenth
  break
  case 15
    set IAm fifteenth
  break
  case 16
    set IAm sixteenth
  break
  case 17
    set IAm seventeenth
  break
  case 18
    set IAm eighteenth
  break
  case 19
    set IAm nineteenth
  break
  case 20
    set IAm twentieth
  break
  default
    set IAm unknown
  break
done
if %IAm% == unknown
  %echo% One of the many fledglings growls as it becomes enraged!
else
  %echo% The %IAm% fledgling puffs up as it becomes enraged!
end
if %count% >=10 && !%self.affect(haste)%
  dg_affect %self% HASTE on -1
end
eval count %count% * 5
dg_affect #12319 %self% BONUS-PHYSICAL %count% -1
~
#12322
fur dragon pounce~
0 l 40
~
if %self.cooldown(12322)%
  halt
end
set person %self.room.people%
while %person%
  if %person.vnum% == 12318
    eval count %count% + 1
  end
  set person %person.next_in_room%
done
eval count %count% * 2
if %count% > 20
  set timer 10
else
  eval timer 30 - %count%
end
%echo% ~%self% begins to wiggle ^%self% butt as &%self% prepares to pounce!
wait 3 s
nop %self.set_cooldown(12322, %timer%)%
%echo% ~%self% lunges at ~%actor% and strikes with both front claws!
%damage% %actor% 65 physical
%damage% %actor% 80 physical
~
$
