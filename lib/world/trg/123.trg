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
  %echo% ~%self% has been set to Normal.
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
  %echo% &&t&&Z~%self% suddenly leaps high into the air, spreads leathery wings that were hidden under its fur, and flies off!&&0
  mgoto %loc%
  nop %self.add_mob_flag(SENTINEL)%
  nop %instance.set_location(%loc%)%
  %regionecho% %loc% -30 You see a fur dragon fly home to its burrow at %loc.coords%!
  %echo% &&t&&Z~%self% enters the burrow through the hole and blocks the entrance!&&0
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
if %actor.is_npc% && !%actor.leader% && %self.can_see(%actor%)% && !%actor.mob_flagged(HARD)% && !%actor.mob_flagged(GROUP)% && %actor.level% < 100
  %echo% ~%self% crouches low and tries to hide...
  * short wait (store id to prevent errors)
  set id %actor.id%
  wait 1
  * still here?
  if %actor.room% == %room% && %actor.id% == %id%
    %echoaround% %actor% ~%self% pounces on ~%actor%, biting hard into ^%actor% neck, killing *%person%!
    %send% %actor% ~%self% pounces out of nowhere and the last thing you feel is its jaws closing around your neck!
    nop %actor.add_mob_flag(!LOOT)%
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
%echo% &&t&&Z~%self% shifts as you hear growling from somewhere.&&0
wait 3
if %roll% > 95
  set SpawnStr a few fluffy fledglings approach
  set FledglingCount 3
elseif %roll% > 80
  set SpawnStr a couple fluffy fledglings approach
  set FledglingCount 2
else
  set SpawnStr a fluffy fledgling approaches
  set FledglingCount 1
  set str s
end
%echo% &&tSuddenly, %SpawnStr% from the corner and attack%str%!&&0
while %FledglingCount%
  %load% mob 12318 ally
  set CoolOff %self.room.people%
  nop %CoolOff.set_cooldown(12321, 90)%
  eval FledglingCount %FledglingCount% - 1
done
eval timer 80 - %difficulty% * 10
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
%echo% &&t**** &&Z~%self% whines as &%self% rubs an ear against the nearest hard surface. Maybe you should scratch it? ****&&0 (scratch)
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
  %echo% &&t&&Z~%self% purs in satisfaction and seems to calm down a bit.&&0
  halt
end
eval buff ( %PlayerCount% - %CountScratched% ) * 10
if %self.mob_flagged(group)%
  eval buff %buff% * 2
end
dg_affect #12319 %self% BONUS-PHYSICAL %buff% -1
%echo% &&t&&Z~%self% seems to become enraged and fights with a greater furiosity!&&0
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
  %echoaround% %actor% ~%actor% tries to scratch ~%self%, who quickly returns the favor!
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
Fur Dragon Combat: Buff the baby fur dragon~
0 b 30
~
if %self.cooldown(12321)%
  halt
end
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
  %echo% &&tOne of the many fledglings growls as it becomes enraged!&&0
else
  %echo% &&tThe %IAm% fledgling puffs up as it becomes enraged!&&0
end
if %count% >=10 && !%self.aff_flagged(haste)%
  dg_affect %self% HASTE on -1
end
eval count %count% * 5
dg_affect #12319 %self% BONUS-PHYSICAL %count% -1
nop %self.set_cooldown(12321, 90)%
~
#12322
Fur Dragon Combat: pounce~
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
* store id to prevent errors after wait
set id %actor.id%
wait 3 s
if %actor.id% == %id%
  nop %self.set_cooldown(12322, %timer%)%
  %echo% ~%self% lunges at ~%actor% and strikes with both front claws!
  %damage% %actor% 65 physical
  %damage% %actor% 80 physical
end
~
#12350
Hoarfrost Serragon: Leave pit on load~
0 n 100
~
dg_affect #12350 %self% !ATTACK on -1
if (!%instance.location% || %self.room.template% != 12350)
  halt
end
mgoto %instance.location%
%echo% ~%self% bursts forth from the pit!
~
#12351
Hoarfrost Serragon: Delayed completion~
1 f 0
~
%adventurecomplete%
~
#12352
Hoarfrost Serragon: Death trigger~
0 f 100
~
set inside %instance.start%
if %inside%
  %at% %inside% %load% obj 12350
end
~
#12353
Hoarfrost Serragon: Terraformer~
0 i 100
~
* freezes the tile as the creature walks in, or leashes it
* configs:
set avoid_sects 15 16 27 28 29 34 35 55 61 62 63 64 65
set ignore_sects 6 8 9
set frozen_plains 0 7 13 36 40 41 46 50 54 56 59
set frozen_forest 1 2 3 4 37 38 39 42 43 44 45 47 60 90
set frozen_desert 20 23
set frozen_grove 12 14 26 24 25
set frozen_oasis 21 80 81 83 84
set irrigated_field 70 73 75 77 78
set irrigated_forest 71 74 79
set irrigated_jungle 72 76
set dry_oasis 82 91
set irrigated_oasis 88 89
set frozen_lake 32 33
* note: sects 5, 19, 51, 53, 57, 58, 85, and 87 are checked individually below as well
* basic checks
if %self.fighting% || %self.disabled%
  halt
end
wait 1
set room %self.room%
* check leash
set dist %room.distance(%instance.location%)%
if %instance.location% && ((%dist% > 6 && %random.2% == 2) || %avoid_sects% ~= %room.sector_vnum%)
  if !%self.aff_flagged(!SEE)%
    %echo% ~%self% burrows down and vanishes from sight!
  end
  mgoto %instance.location%
  if !%self.aff_flagged(!SEE)%
    %echo% The ground shakes as ~%self% bursts forth from the pit!
  end
  halt
end
* check if we can terraform
if %room.sector_vnum% >= 100 || %ignore_sects% ~= %room.sector_vnum%
  * everything outside of this block is forbidden
  * ignore_sects is meant to avoid single-digit vnums matching partial strings
  halt
elseif %frozen_plains% ~= %room.sector_vnum%
  %echo% Hoarfrost grows over the grass as a bitter cold descends upon the plains.
  %terraform% %room% 12366
elseif %frozen_forest% ~= %room.sector_vnum%
  %echo% The trees wither and die as hoarfrost overtakes the leaves and ice pours down the trunks!
  %terraform% %room% 12365
elseif %frozen_desert% ~= %room.sector_vnum%
  %echo% Hoarfrost grows on each plant and every grain of sand, until the desert is covered in ice!
  %terraform% %room% 12350
elseif %frozen_grove% ~= %room.sector_vnum%
  %echo% The grove is glazed with ice as hoarfrost grows on every branch and leaf!
  %terraform% %room% 12351
elseif %frozen_oasis% ~= %room.sector_vnum%
  %echo% The oases glazes over with ice as hoarfrost forms on the plants around it!
  %terraform% %room% 12352
elseif %room.sector_vnum% == 51
  %echo% Hoarfrost spreads across the sandy beach like an arctic wave!
  %terraform% %room% 12353
elseif %room.sector_vnum% == 57
  %echo% Icebergs form in the water around the frozen shore!
  %terraform% %room% 12354
elseif %irrigated_field% ~= %room.sector_vnum%
  %echo% Hoarfrost covers every plant as the whole field ices over!
  %terraform% %room% 12355
elseif %irrigated_forest% ~= %room.sector_vnum%
  %echo% Hoarfrost grows on the trees as the entire forest freezes over!
  %terraform% %room% 12356
elseif %irrigated_jungle% ~= %room.sector_vnum%
  %echo% Ice and hoarfrost overtake the jungle!
  %terraform% %room% 12357
elseif %dry_oasis% ~= %room.sector_vnum%
  %echo% Hoarfrost forms on every plant and rock as ice overtakes the whole area!
  %terraform% %room% 12358
elseif %room.sector_vnum% == 85
  %echo% The canal freezes over with thick ice!
  %terraform% %room% 12359
elseif %room.sector_vnum% == 87
  %echo% The canal freezes over with thick ice!
  %terraform% %room% 12360
elseif %room.sector_vnum% == 19
  %echo% The canal freezes over with thick ice!
  %terraform% %room% 12361
elseif %irrigated_oasis% ~= %room.sector_vnum%
  %echo% The oases glazes over with ice as hoarfrost forms on the plants around it!
  %terraform% %room% 12362
elseif %room.sector_vnum% == 5
  %echo% The river grinds to a halt as it rapidly turns to ice!
  %terraform% %room% 12363
elseif %room.sector_vnum% == 53
  %echo% The river grinds to a halt as the entire estuary turns to ice!
  %terraform% %room% 12364
elseif %room.sector_vnum% == 58
  %echo% Hoarfrost grows on the grass as any icy cold overtakes the foothills!
  %terraform% %room% 12367
elseif %frozen_lake% ~= %room.sector_vnum%
  %echo% Frost grows on the surface of the lake, freezing it in the blink of an eye!
  %terraform% %room% 12368
end
~
#12354
Hoarfrost Serragon: Start progress goal (pit)~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12350)%
end
~
#12355
Hoarfrost Serragon: Start progress goal (mob)~
0 h 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(12350)%
end
~
#12356
Hoarfrost Serragon: Difficulty selector with retreat~
0 c 0
difficulty~
if !%arg%
  %send% %actor% You must specify a level of difficulty. (Normal, Hard, Group, or Boss)
  return 1
  halt
end
if %self.fighting%
  %send% %actor% You can't change |%self% difficulty while &%self% is in combat!
  return 1
  halt
end
if normal /= %arg%
  set difficulty 1
elseif hard /= %arg%
  set difficulty 2
elseif group /= %arg%
  set difficulty 3
elseif boss /= %arg%
  set difficulty 4
else
  %send% %actor% That is not a valid difficulty level for this adventure. (Normal, Hard, Group, or Boss)
  return 1
  halt
end
* messaging
%send% %actor% You set the difficulty...
%echoaround% %actor% ~%actor% sets the difficulty...
* Clear existing difficulty flags and set new ones.
nop %self.remove_mob_flag(HARD)%
nop %self.remove_mob_flag(GROUP)%
if %difficulty% == 1
  * Then we don't need to do anything
  %echo% ~%self% has been set to Normal.
elseif %difficulty% == 2
  %echo% ~%self% has been set to Hard.
  nop %self.add_mob_flag(HARD)%
elseif %difficulty% == 3
  %echo% ~%self% has been set to Group.
  nop %self.add_mob_flag(GROUP)%
elseif %difficulty% == 4
  %echo% ~%self% has been set to Boss.
  nop %self.add_mob_flag(HARD)%
  nop %self.add_mob_flag(GROUP)%
end
remote difficulty %self.id%
%restore% %self%
nop %self.unscale_and_reset%
wait 1
* send mob home
set loc %instance.location%
if %loc% && %self.room% != %loc%
  %echo% &&C~%self% clacks its massive jaws and then burrows down and vanishes from sight!&&0
  %echo% &&C...it can't have gone far!&&0
  mgoto %loc%
  nop %self.add_mob_flag(SENTINEL)%
  dg_affect #12351 %self% BONUS-PHYSICAL 1 60
  * no_sentinel is used by the storytime script to remove sentinel
  set no_sentinel 0
  remote no_sentinel %self.id%
  wait 1
  %regionecho% %loc% -12 The ground shakes as something enormous moves beneath it!
  %echo% &&C~%self% bursts forth from the pit... and it looks angry!&&0
end
* remove no-attack
if %self.affect(12350)%
  dg_affect #12350 %self% off
end
* mark me as scaled
set scaled 1
remote scaled %self.id%
* and allow me to wander again later
attach 12358 %self.id%
~
#12357
Hoarfrost Serragon: Instruction to diff-sel~
0 B 0
~
if %self.affect(12350)% && %actor.char_target(%arg%)% == %self%
  %send% %actor% You need to choose a difficulty before you can attack ~%self%.
  %send% %actor% Usage: difficulty <normal \| hard \| group \| boss>
  %echoaround% %actor% ~%actor% considers attacking ~%self%.
  return 0
else
  return 1
  halt
end
~
#12358
Hoarfrost Serragon: Resume wandering if nobody fights.~
0 b 10
~
* cancel sentinel and resume movement if nobody is around and fighting me
if !%self.affect(12351)% && !%self.fighting% && %room.players_present% == 0
  nop %self.remove_mob_flag(SENTINEL)%
  dg_affect #12350 %self% !ATTACK on -1
  * reset no-sentinel for storytime
  set no_sentinel 1
  remote no_sentinel %self.id%
  * and detach me
  detach 12358 %self.id%
end
~
#12359
Hoarfrost Serragon combat: Avalanche Slam, Coil, Snapping Jaws, Frost Pores / Heat Drain~
0 k 100
~
if %self.cooldown(12352)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.var(difficulty,1)%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4
  set num_left 4
end
* pick
eval which %%random.%num_left%%%
set old %moves_left%
set moves_left
set move 0
while %which% > 0
  set move %old.car%
  if %which% != 1
    set moves_left %moves_left% %move%
  end
  set old %old.cdr%
  eval which %which% - 1
done
set moves_left %moves_left% %old%
* store
eval num_left %num_left% - 1
remote moves_left %self.id%
remote num_left %self.id%
* perform move
scfight lockout 12352 30 35
if %move% == 1
  * Avalanche Slam
  scfight clear dodge
  %echo% &&C~%self% raises most of its long body up high into the air...&&0
  eval dodge %diff% * 40
  dg_affect #12357 %self% DODGE %dodge% 20
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  scfight setup dodge all
  wait 5 s
  if %self.disabled%
    dg_affect #12357 %self% off
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %echo% &&C**** &&Z~%self% slams into the ground! An avalanche is headed toward you! ****&&0 (dodge)
  if %diff% > 1
    set ouch 100
  else
    set ouch 50
  end
  set cycle 1
  set hit 0
  eval wait 9 - %diff%
  while %cycle% <= %diff%
    scfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_scfdodge)%
          set hit 1
          %echo% &&CThe avalanche knocks ~%ch% over and partially buries *%ch%!&&0
          %send% %ch% That really hurt!
          dg_affect #12359 %ch% SLOW on 10
          dg_affect #12359 %ch% COOLING 20 10
          %damage% %ch% %ouch% physical
        elseif %ch.is_pc%
          %send% %ch% &&CYou find high ground just in time as the avalanche roars past you!&&0
          if %diff% == 1
            dg_affect #12358 %ch% TO-HIT 25 10
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&C**** The shockwave is still going... here comes another avalanche! ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    scfight clear dodge
    eval cycle %cycle% + 1
  done
  dg_affect #12357 %self% off
  if !%hit% && %diff% == 1
    %echo% &&C~%self% seems to exhaust *%self%self from all that spinning.&&0
    dg_affect #12353 %self% HARD-STUNNED on 10
  end
  wait 8 s
elseif %move% == 2
  * Coil
  scfight clear struggle
  if %room.players_present% > 1
    %echo% &&CThere's a screeching sound as ~%self% begins to coil around the whole party!&&0 (struggle)
  else
    %echo% &&CThere's a screeching sound as ~%self% begins to coil around you!&&0 (struggle)
  end
  if %diff% == 1
    dg_affect #12353 %self% HARD-STUNNED on 20
  end
  scfight setup struggle all 20
  * messages
  set cycle 0
  set ongoing 1
  while %cycle% < 5 && %ongoing%
    wait 4 s
    set ongoing 0
    set person %room.people%
    while %person%
      if %person.affect(9602)%
        set ongoing 1
        if %diff% > 1
          %send% %person% &&CYou scream in pain as the serrated scales cut you!&&0 (struggle)
          %dot% #12355 %person% 100 30 physical 5
        else
          %send% %person% &&CYou strain to breath as the serrated scales coil around you!&&0 (struggle)
        end
      end
      set person %person.next_in_room%
    done
    eval cycle %cycle% + 1
  done
  scfight clear struggle
  dg_affect #12353 %self% off
elseif %move% == 3
  * Snapping Jaws
  scfight clear dodge
  %echo% &&C~%self% rears back and clacks its jaws open and shut...&&0
  wait 3 s
  if %self.disabled% || %self.aff_flagged(BLIND)%
    halt
  end
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  if %diff% == 1
    dg_affect #12353 %self% HARD-STUNNED on 20
  end
  %send% %targ% &&C**** &&Z~%self% leans in and snaps its jaws... at you! ****&&0 (dodge)
  %echoaround% %targ% &&C~%self% leans in and snaps its jaws... at ~%targ%!&&0
  scfight setup dodge %targ%
  if %diff% > 1
    set ouch 100
  else
    set ouch 50
  end
  set cycle 0
  eval times %diff% * 2
  eval when 9 - %diff%
  set done 0
  while %cycle% < %times% && !%done%
    wait %when% s
    if %targ.id% != %targ_id%
      set done 1
    elseif !%targ.var(did_scfdodge)%
      %echo% &&C~%self% bites down on ~%targ% hard!&&0
      %send% %targ% That really hurts!
      %damage% %targ% %ouch% physical
    end
    eval cycle %cycle% + 1
    if %cycle% < %times% && !%done%
      wait 1
      %send% %targ% &&C**** &&Z~%self% is still snapping its jaws... looks like its coming for you again... ****&&0 (dodge)
      %echoaround% %targ% &&C~%self% rears back to snap at ~%targ% again...&&0
      scfight clear dodge
      scfight setup dodge %targ%
    else if %done% && %targ.id% == %targ_id% && %diff% == 1
      dg_affect #12358 %targ% TO-HIT 25 20
    end
  done
  scfight clear dodge
  dg_affect #12353 %self% off
elseif %move% == 4
  * Frost Pores / Heat Drain
  scfight clear struggle
  %echo% &&CA cold mist seeps from |%self% pores and flows over you...&&0
  %echo% &&CYou begin to freeze in place!&&0 (struggle)
  if %diff% == 1
    dg_affect #12353 %self% HARD-STUNNED on 20
  end
  scfight setup struggle all 20
  * messages
  eval punish -2 * %diff%
  set cycle 0
  set ongoing 1
  while %cycle% < 5 && %ongoing%
    wait 4 s
    set ongoing 0
    set person %room.people%
    while %person%
      if %person.affect(9602)%
        set ongoing 1
        if %diff% > 1
          %send% %person% &&CYou feel the heat draining out of your body!&&0 (struggle)
          dg_affect #12356 %person% BONUS-PHYSICAL %punish% 20
          dg_affect #12356 %person% BONUS-MAGICAL %punish% 20
          dg_affect #12356 %person% COOLING 20 20
        else
          %send% %person% &&CYou're trapped in the frost!&&0 (struggle)
        end
      end
      set person %person.next_in_room%
    done
    eval cycle %cycle% + 1
  done
  scfight clear struggle
  dg_affect #12353 %self% off
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#12360
Hoarfrost Serragon: Moving adventure command~
0 c 0
adventure~
if !(summon /= %arg.car%)
  %teleport% %actor% %instance.location%
  %force% %actor% adventure
  %teleport% %actor% %self.room%
  return 1
else
  return 0
end
~
#12361
Hoarfrost Serragon: Leash for ice creatures~
0 i 100
~
set room %self.room%
if %room.sector_vnum% < 12350 || %room.sector_vnum% > 12399
  return 0
end
~
$
