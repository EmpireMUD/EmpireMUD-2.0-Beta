#11900
Skycleave: Skymerc mercenary spawner on diff select~
0 c 0
skymerc~
* Usage: skymerc <difficulty 1-4>
if %actor% != %self%
  halt
end
set diff %arg.car%
if !(%diff%)
  set diff 1
end
set amount %diff%
if %amount% > 3
  * cap at 3 mobs
  set amount 3
end
set iter 0
* This loads a group of mobs in the vnum range 11841, 11842, 11843, 11844, 11845, 11846
while %iter% < %amount%
  eval vnum 11840 + %random.6%
  %load% mob %vnum%
  set mob %self.room.people%
  if %mob.vnum% == %vnum%
    remote diff %mob.id%
  end
  eval iter %iter% + 1
done
~
#11901
Skycleave: Immortal controller / skycleave command~
1 c 2
skycleave~
if !%actor.is_immortal%
  %send% %actor% You lack the power to use this.
  halt
end
set mode %arg.car%
set arg2 %arg.cdr%
if goto /= %mode%
  set to_room %instance.nearest_rmt(%arg2%)%
  if !%to_room%
    %send% %actor% Invalid target Skycleave room.
  elseif %to_room.template% < 11800 || %to_room.template% > 11999
    %send% %actor% You can only use this to goto a Skycleave room.
  else
    %echoaround% %actor% ~%actor% vanishes!
    %teleport% %actor% %to_room%
    %echoaround% %actor% ~%actor% appears in a flash!
    %force% %actor% look
  end
elseif passage /= %mode% || secret /= %mode%
  %send% %actor% Ok.
  %load% mob 11923
  %load% mob 11924
elseif phase /= %mode%
  if %self.room.template% < 11800 || %self.room.template% > 11999
    %send% %actor% You can only do this in the Tower Skycleave.
  elseif %arg2% == 1
    %send% %actor% Changing phase for floor 1...
    %load% mob 11895
  elseif %arg2% == 2
    %send% %actor% Changing phase for floor 2...
    %load% mob 11896
  elseif %arg2% == 3
    %send% %actor% Changing phase for floor 3...
    %load% mob 11897
  elseif %arg2% == 4
    %send% %actor% Changing phase for floor 4...
    %load% mob 11898
  else
    %send% %actor% Invalid floor to change phase.
  end
elseif information /= %mode% || status /= %mode%
  set room %actor.room%
  set temp %instance.nearest_rmt(11800)%
  if !%temp%
    %send% %actor% No Skycleave instance detected.
    halt
  end
  %teleport% %actor% %temp%
  set spirit %instance.mob(11900)%
  %teleport% %actor% %room
  if !%spirit%
    %send% %actor% Unable to get Skycleave information: Spirit of Skycleave not detected.
    halt
  end
  %send% %actor% Tower Skycleave instance info:
  %send% %actor% Location: %temp.coords%
  set floor 1
  while %floor% <= 4
    set phase %spirit.var(phase%floor%)%
    if %phase% == 0
      set phase A
    else
      set phase B
    end
    set diff %spirit.var(diff%floor%)%
    if %spirit.var(start%floor%)%
      set started , Started by %spirit.var(start%floor%)%
    else
      set started
    end
    if %spirit.var(finish%floor%)%
      set ended , Finished by %spirit.var(finish%floor%)%
    else
      set ended
    end
    %send% %actor% Floor %floor%: Phase %phase%, Difficulty %diff%%started%%ended%
    eval floor %floor% + 1
  done
  if %spirit.lab_open%
    set lab Open
  else
    set lab Closed
  end
  %send% %actor% Magichanical Labs: %lab%
  if %spirit.lich_released%
    set lich Yes
  else
    set lich No
  end
  %send% %actor% Scaldorran released: %lich%
  %send% %actor% Claw machine: %spirit.claw4%%spirit.claw3%%spirit.claw2%%spirit.claw1%
elseif test /= %mode%
  %send% %actor% No test configured.
else
  %send% %actor% Invalid command.
end
~
#11902
Skycleave: BoE loot quality flags~
1 n 100
~
* Inherit hard/group flags from an NPC and rescale itself, on NON-CRAFTED BOE
* first ensure there's a person
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
end
if !%actor%
  halt
end
set rescale 0
* next check pc/npc
if %actor.is_npc%
  if !%self.is_flagged(GENERIC-DROP)%
    if %actor.mob_flagged(HARD)% && !%self.is_flagged(HARD-DROP)%
      nop %self.flag(HARD-DROP)%
      set rescale 1
    end
    if %actor.mob_flagged(GROUP)% && !%self.is_flagged(GROUP-DROP)%
      nop %self.flag(GROUP-DROP)%
      set rescale 1
    end
  end
end
if %rescale% && %self.level%
  wait 0
  %scale% %self% %self.level%
end
~
#11903
Skycleave: Reset mobs when out of combat~
0 bw 100
~
* Resets "permanent" buffs when the mob is out of combat
if %self.fighting% || %self.disabled%
  halt
end
if %self.vnum% == 11850
  * just despawn
  %echo% ~%self% lies down in one of the coffins and dies.
  %purge% %self%
  halt
end
* otherwise:
dg_affect #11864 %self% off silent
dg_affect #11889 %self% off silent
dg_affect #11890 %self% off silent
dg_affect #11892 %self% off silent
%heal% %self% health 10000
~
#11904
Skycleave: Cleaning crew despawn~
0 b 33
~
if %self.loadtime% + 3600 > %timestamp%
  * more time left
  halt
end
* extra tasks for specific mobs
switch %self.vnum%
  case 11902
    * bucket: despawn helper first
    set sponge %instance.mob(11903)%
    if %sponge%
      %echo% ~%sponge% hops into ~%self%.
      %purge% %sponge%
    end
  break
  case 11961
    * bookshelf: load the skeleton in the lich labs
    %at% i11937 %load% mob 11937
    %at% i11937 %echo% A skeleton sits up on the work bench.
    * if Knezz and Niamh didn't die, also do the wrangler now
    if %instance.mob(11968)% && %instance.mob(11931)%
      set wright %instance.mob(11889)%
      if %wright%
        * oh no: Niamh survived and John caught no goblins
        %at% %wright.room% %echo% ~%wright% trudges out of the room.
        %purge% %wright%
        wait 20 s
        %at% i11914 %load% mob 11926
        %at% i11914 %echo% A sponge comes rolling in through the doorway.
      end
    end
  break
  case 11965
    * trolley: possible replacement for the missing high sorcerer
    set niamh %instance.mob(11931)%
    if %niamh%
      %at% %niamh.room% %echo% ~%niamh% heads upstairs.
      %load% mob 11970
      %load% obj 11966
      %echo% Niamh walks in from the north.
      if %niamh.mob_flagged(*PICKPOCKETED)%
        set mob %instance.mob(11970)%
        nop %mob.add_mob_flag(*PICKPOCKETED)%
      end
      %purge% %niamh%
      wait 3 s
      set wright %instance.mob(11889)%
      if %wright%
        * oh no: Niamh survived and John caught no goblins
        %at% %wright.room% %echo% ~%wright% trudges out of the room.
        %purge% %wright%
        wait 20 s
        %at% i11914 %load% mob 11926
        %at% i11914 %echo% A sponge comes rolling in through the doorway.
      end
    end
  break
done
* and leave
%echo% ~%self% scuttles away.
%purge% %self%
~
#11905
Skycleave: Shared load script for mobs~
0 nA 100
~
switch %self.vnum%
  case 11801
    * Dylane 1B
    dg_affect #11832 %self% !SEE on -1
    dg_affect #11832 %self% !TARGET on -1
    dg_affect #11832 %self% SNEAK on -1
    nop %self.add_mob_flag(SILENT)%
  break
  case 11900
    * spirit of skycleave: init vars
    set phase1 0
    set phase2 0
    set phase3 0
    set phase4 0
    set diff1 0
    set diff2 0
    set diff3 0
    set diff4 0
    set claw1 0
    set claw2 0
    set claw3 0
    set claw4 0
    set lab_open 0
    set lich_released 0
    remote lich_released %self.id%
    remote lab_open %self.id%
    remote claw4 %self.id%
    remote claw3 %self.id%
    remote claw2 %self.id%
    remote claw1 %self.id%
    remote diff4 %self.id%
    remote diff3 %self.id%
    remote diff2 %self.id%
    remote diff1 %self.id%
    remote phase4 %self.id%
    remote phase3 %self.id%
    remote phase2 %self.id%
    remote phase1 %self.id%
  break
  case 11901
    * gossippers in 1B
    south
    eval times 2 + %random.3%
    set iter 0
    while %iter% < %times%
      mmove
      eval iter %iter% + 1
    done
  break
  case 11902
    * bucket
    %load% mob 11903 ally
  break
  case 11837
    * swarm of rag
    if %self.room.template% == 11910
      %mod% %self% append-lookdesc-noformat &0   The black tower houses many of the most erudite sorcerers from across the
      %mod% %self% append-lookdesc-noformat world, though their admission standards are not as high as one might expect.
      %mod% %self% append-lookdesc-noformat Indeed, the tower is often willing to take on any eager apprentice. Some can
      %mod% %self% append-lookdesc-noformat be molded into powerful magic users. Others, unfortunately, wash out.
    elseif %self.room.template% == 11930
      %mod% %self% append-lookdesc-noformat &0   The Tower Skycleave is kept spotless at all times by the many apprentices
      %mod% %self% append-lookdesc-noformat who have come seeking power. The beauty and perfection of the tower are of such
      %mod% %self% append-lookdesc-noformat importance to the high sorcerers that they sometimes recruit more students than
      %mod% %self% append-lookdesc-noformat they need. The brightest ones rise to the top, while the rest are given their
      %mod% %self% append-lookdesc-noformat walking papers.
    end
  break
  case 11861
    * Barrosh post-fight
    wait 0
    %echo% &&mTime moves backwards for a second as High Sorcerer Barrosh composes himself and holds his staff to the sky, dropping some things in the process...&&0
    wait 9 sec
    say For the honor of Skycleave!
    wait 4 sec
    %echo% Glass flies everywhere as a bolt of lightning streaks through the window, momentarily blinding you as it strikes the gem on Barrosh's staff!
    wait 9 sec
    say By the power invested in me by the Tower Skycleave, I cast out the shadow!
    wait 9 sec
    %echo% Barrosh's eyes turn pitch black and he drops to his knees and screams as he throws his head back...
    wait 9 sec
    %echo% A thick, smoky shadow streams from Barrosh's eyes with a coarse, sputtering sound...
    wait 9 sec
    %echo% The shadow is caught by the light of Barrosh's staff and it evaporates with an agonizing hiss!
    wait 9 sec
    say There. It is done. My mind is free. I hate to think of what horrors I committed in that state.
    wait 9 sec
    if %instance.mob(11868)%
      say You have got to free the Grand High Sorcerer. He's the only one who will be able to stop all this.
    else
      say You have got to find a way to save the tower. I'm afraid I will not be any further use.
    end
  break
done
* mark time for despawn trig (if applicable)
set loadtime %timestamp%
remote loadtime %self.id%
* detach me
detach 11905 %self.id%
~
#11906
Skycleave: Secret Passage Detection~
0 hw 100
~
if %actor.is_pc%
  * Mark for claw game
  set spirit %instance.mob(11900)%
  set claw2 1
  remote claw2 %spirit.id%
  detach 11906 %self.id%
end
~
#11907
Skycleave: Claw Game (broken)~
1 c 4
play~
* play claw
if !%arg.car% || !(%self.name% ~= %arg.car%)
  return 0
  halt
end
if %self.val0%
  %send% %actor% %self.shortdesc% doesn't seem to be working.
  halt
end
%send% %actor% You try to play %self.shortdesc% but it goes dark and begins to smoke.
%echoaround% %actor% ~%actor% tries to play %self.shortdesc% but it goes dark and begins to smoke.
* Mark for claw game
set spirit %instance.mob(11900)%
set claw1 1
remote claw1 %spirit.id%
nop %self.val0(1)%
detach 11909 %self.id%
~
#11908
Skycleave: Claw Game (fixed)~
1 c 4
play~
* Usage: play claw
if %actor.is_npc% || !%arg.car% || !(%self.name% ~= %arg.car%)
  return 0
  halt
end
* Level check
if (%actor.is_npc% || %actor.level% < 150) && !%actor.is_immortal%
  %send% %actor% You must be at least level 150 to play the claw game.
  halt
end
* Check if they've already played in this instance
eval already %%already%actor.id%%%
if %already%
  %send% %actor% You have already played the claw game today.
  halt
end
* Record that they have
set already%actor.id% 1
global already%actor.id%
%send% %actor% You play the claw game...
%echoaround% %actor% ~%actor% plays the claw game...
set got_prize 0
* Determine reward: Each of the claw# tasks are treated as bits in a bitset
set spirit %instance.mob(11900)%
set id %spirit.claw1%
if %spirit.claw2%
  eval id %id% + 2
end
if %spirit.claw3%
  eval id %id% + 4
end
if %spirit.claw4%
  eval id %id% + 8
end
* Determine reward
set mini 0
set item 0
switch %id%
  case 0
    set mini 11999
  break
  case 1
    set mini 11990
  break
  case 2
    set mini 11993
  break
  case 3
    set mini 11992
  break
  case 4
    set item 11987
  break
  case 5
    set mini 11991
  break
  case 6
    set item 11991
  break
  case 7
    set mini 11998
  break
  case 8
    set mini 11997
  break
  case 9
    set mini 11995
  break
  case 10
    set item 11989
  break
  case 11
    set mini 11996
  break
  case 12
    set item 11988
  break
  case 13
    set mini 11989
  break
  case 14
    set item 11990
  break
  case 15
    set mini 11994
  break
done
if %mini%
  * Minipet
  %load% mob %mini%
  set mob %self.room.people%
  if %mob.vnum% != %mini%
    * Uh-oh.
    %echo% Something went horribly wrong while granting a minipet. Please bug-report this error.
    halt
  end
  set mob_string %mob.name%
  %purge% %mob%
  if !%actor.has_minipet(%mini%)%
    %send% %actor% You win '%mob_string%' as a minipet! Use the minipets command to summon it.
    %echoaround% %actor% ~%actor% has won '%mob_string%'!
    nop %actor.add_minipet(%mini%)%
  else
    %send% %actor% You win '%mob_string%' but you already have it as a minipet.
  end
end
if %item%
  %load% obj %item% %actor%
  set obj %actor.inventory%
  if %obj.vnum% != %item%
    * Uh-oh.
    %echo% Something went horribly wrong while granting a reward. Please bug-report this error.
    halt
  else
    %send% %actor% You win '%obj.shortdesc%'!
    %echoaround% %actor% ~%actor% has won '%obj.shortdesc%'!
  end
end
* and check quests:
if %actor.on_quest(11907)%
  %quest% %actor% trigger 11907
end
if %actor.on_quest(11908)%
  %quest% %actor% trigger 11908
  %quest% %actor% finish 11908
end
~
#11909
Skycleave: Skycleaver trinket 2.0 (and warpstone)~
1 c 2
use~
* will teleport the actor if used within this distance without a cooldown
set always_teleport_vnum 11910
set max_distance 100
set cooldown_time 1800
* basics
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
return 1
set loc %instance.nearest_rmt(11800)%
set room %actor.room%
set name %self.custom(script1)%
if !%loc% || %ch.plr_flagged(ADV-SUMMON)%
  %send% %actor% The %name% just spins and spins.
  %echoaround% %actor% ~%actor% holds up %name.ana% %name%, which spins in the air.
  halt
end
* if we're too far or the cooldown is too high, we just show direction
set distance %room.distance(%loc%)%
if %distance% == 0
  %send% %actor% The %name% just spins and spins.
  %echoaround% %actor% ~%actor% holds up %name.ana% %name%, which spins in the air.
  halt
end
* direction portion
if %self.vnum% != %always_teleport_vnum%
  set direction %room.direction(%loc%)%
  %send% %actor% You hold up @%self%, which spins and points to the %direction%, toward %loc.coords%!
  %echoaround% %actor% ~%actor% holds up a trinket, which spins and points to the %direction%!
  * trinket checks
  if %distance% > %max_distance% || %actor.cooldown(%self.vnum%)%
    halt
  end
else
  * always-teleport version
  if %actor.cooldown(%self.vnum%)%
    %send% %actor% Your warpstone is depleted. Try again in a little while.
    halt
  end
  * works?
  %send% %actor% You hold up @%self%...
  %echoaround% %actor% ~%actor% holds up @%self%...
end
* checks for both
if %actor.position% != Standing || %actor.action% || %actor.fighting% || !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  if %self.vnum% == %always_teleport_vnum%
    %send% %actor% ... but nothing happens.
  end
  halt
end
* teleport portion: if we got here, they qualify for a teleport
wait 1
* set up 'stop' command
set stop_command 0
set stop_message_char You stop using %self.shortdesc%.
set stop_message_room ~%actor% stops using %self.shortdesc%.
set needs_stop_command 1
remote stop_command %actor.id%
remote stop_message_char %actor.id%
remote stop_message_room %actor.id%
remote needs_stop_command %actor.id%
* start going
set cycle 0
while %cycle% < 3
  set loc %instance.nearest_rmt(11800)%
  if !%loc% || %actor.stop_command% || %actor.room% != %room% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)% || %actor.action%
    %force% %actor% stop cleardata
    %send% %actor% @%self% stops glowing.
    halt
  end
  switch %cycle%
    case 0
      %send% %actor% @%self% begins to glow bright violet and shake violently...
      wait 5 sec
    break
    case 1
      %send% %actor% The violet glow from @%self% envelops you!
      %echoaround% %actor% |%actor% skycleaver %name% glows a stunning violet and the light envelops *%actor%!
      wait 5 sec
    break
    case 2
      %echoaround% %actor% ~%actor% vanishes in a flash of violet light!
      %teleport% %actor% %loc%
      %force% %actor% look
      %echoaround% %actor% ~%actor% appears in a flash of violet light!
    break
  done
  eval cycle %cycle% + 1
done
* cleanup
nop %actor.set_cooldown(%self.vnum%,%cooldown_time%)%
nop %actor.cancel_adventure_summon%
%force% %actor% stop cleardata
~
#11910
Skycleave: Auto-fill fountains~
1 bw 20
~
* ensures the fountains are never empty
set max %self.val0%
nop %self.val1(%max%)%
switch %self.vnum%
  case 11882
    nop %self.val2(11885)%
  break
  case 11885
    nop %self.val2(11885)%
  break
  case 11906
    nop %self.val2(0)%
  break
  case 11921
    nop %self.val2(11921)%
  break
  case 11970
    nop %self.val2(0)%
  break
  case 11981
    nop %self.val2(0)%
  break
done
~
#11911
Pixy Races: Racecall (command)~
0 c 0
racecall~
* announces the race in current positions
if %actor% != %self%
  return 0
  halt
end
* configs
set area_name0 the first turn
set area_name1 the Forest Dome
set area_name2 the first tunnel
set area_name3 the Lava Dome
set area_name4 the second tunnel
set area_name5 the Thunder Dome
set area_name6 the home stretch
* first, pull stats for easy echoing, this puts them into name1, dist1,
* prev1 based on their current positions in the race and does NOT correspond
* to their places in stored data
set pos 1
while %pos% <= 3
  set place %self.var(place%pos%)%
  * moves them based on current place in TEMPORARY data
  set pos%place% %pos%
  set name%place% %self.var(name%pos%)%
  set dist%place% %self.var(dist%pos%)%
  set area%place% %self.var(area%pos%)%
  set prev_area%place% %self.var(prev_area%pos%)%
  set gap%place% %self.var(gap%pos%)%
  set guile%place% %self.var(guile%pos%)%
  set luck%place% %self.var(luck%pos%)%
  set winner%place% %self.var(winner%pos%)%
  set prev_place%place% %self.var(prev_place%pos%)%
  set prev_gap%place% %self.var(prev_gap%pos%)%
  eval pos %pos% + 1
done
* see who's being watched
set need1 0
set need2 0
set need3 0
set ch %self.room.people%
while %ch%
  if %ch.is_pc%
    set varname watching_%ch.id%
    set watching %self.var(%varname%,0)%
    if %watching% > 0
      set watching %self.var(place%watching%)%
      set need%watching% 1
    end
  end
  set ch %ch.next_in_room%
done
set str1
set str2
set str3
set says
set tie 0
* 1. start strings: position w/changes
if %gap1% == 0 && %gap2% == 0
  set say %say% It's a three-way tie with all three pixies wing to wing!
  set str1 All three pixies are tied
  set str2 %str1%
  set str3 %str1%
  set tie 3
end
if %gap1% == 0 && %str1.empty%
  set say %say% %name1.cap% and %name2% are tied for the lead!
  set str1 %name1.cap% is wing-and-wing with %name2%
  set str2 %name2.cap% is wing-and-wing with %name1%
  set tie 2
end
if %gap2% == 0 && %str2.empty%
  set say %say% %name2.cap% and %name3% are tied for second!
  set str2 %name2.cap% is wing-and-wing with %name3%
  set str3 %name3.cap% is wing-and-wing with %name2%
  set tie 2
end
if %need1% && %str1.empty%
  if %prev_place1% == 1
    set str1 %name1.cap% is out in front
  else
    set str1 %name1.cap% takes the lead
    set say %say% %name1.cap% has taken the lead!
  end
  if %gap1% < 8
    set str1 %str1% with %name2% right behind
    if %prev_gap1% < %gap1%
      set str1 %str1% and gaining fast
    end
  elseif %gap1% > 20
    set str1 %str1% and has a strong lead
  end
end
if %need2% && %str2.empty%
  if %prev_place2% == 2
    if %gap1% < 10
      set hf a close second
    elseif %gap1% > 25
      set hf a distant second
    else
      set hf second
    end
    set str2 %name2.cap% is in %hf% place
  if %gap2% < 7
    set str2 %str2% with %name3% right behind
  end
  elseif %prev_place2% == 3
    set str2 %name2.cap% has overtaken %name3% for second place
    set say %say% %name2.cap% has overtaken second!
  elseif %prev_place2% == 1
    set str2 %name2.cap% has fallen back to second place
  end
end
if %need3% && %str3.empty%
  if %prev_place3% != 3
    set str3 %name3.cap% has fallen to last place
  elseif %gap2% < 10
    set str3 %name3.cap% is holding on in last place
  elseif %gap2% > 25
    set str3 %name3.cap% is far behind in last place
  else
    set str3 %name3.cap% remains in last place
  end
end
* 2. "as they enter AREA"?
* idea: if say length is long enough, add "it's an exciting race" or similar
  * else: gaps widened/shrunk
    * else: just current positions
* part-of-the-track messages (track to have several obstacles around certain spots
  * may have to track progress through obstacles on each pix to avoid echoing about it twice
* location parts
set forestdome 0
set lavadome 0
set thunderdome 0
set place 1
while %place% <= 3
  eval need %%need%place%%%
  set strname str%place%
  eval area %%area%place%%%
  eval prev_area %%prev_area%place%%%
  eval luck %%luck%place%%%
  eval rand_luck %%random.%luck%%%
  eval area_name %%area_name%area%%%
  eval pix_name %%name%place%%%
  eval temp_str %%%strname%%%
  if %need% && %area% > %prev_area%
    set %strname% %temp_str% entering %area_name%
  else
    set %strname% %temp_str% in %area_name%
  end
  * hazards
  eval penalty_var penalty%%pos%place%%%
  if %self.var(%penalty_var%,0)% == 0 && %area% == %prev_area% && !%tie%
    set penalty 0
    if %area% == 1 && !%forestdome% && %rand_luck% < 2
      set penalty 2
      set forestdome 1
      if %need%
        eval temp %%%strname%%%
        set %strname% %temp% but is grabbed by a dryad
        * in %area_name%
      end
    elseif %area% == 3 && !%lavadome% && %rand_luck% < 3
      set penalty 4
      set lavadome 1
      if %need%
        eval temp %%%strname%%%
        set %strname% %temp% but takes a lava ball to the face from a fire sprite
        * in %area_name%
      end
    elseif %area% == 5 && !%thunderdome% && %rand_luck% < 4
      set penalty 6
      set thunderdome 1
      if %need%
        eval temp %%%strname%%%
        set %strname% %temp% but is blasted by a thunderling
        * in %area_name%
      end
    end
    if %penalty%
      set %penalty_var% %penalty%
      remote %penalty_var% %self.id%
    end
  end
  eval place %place% + 1
done
* 2. send messages?
if !%say.empty%
  say %say%
end
set ch %self.room.people%
while %ch%
  if %ch.is_pc%
    set varname watching_%ch.id%
    set watching %self.var(%varname%,0)%
    if %watching% > 0
      set watching %self.var(place%watching%)%
      eval send_str %%str%watching%%%
      if !%send_str.empty%
        %send% %ch% %send_str%.
      end
    elseif %random.3% == 3
      %send% %ch% The pixies are racing! Type 'race' to see who's running and 'watch' to follow one.
    end
  end
  set ch %ch.next_in_room%
done
raceman tricks
~
#11912
Pixy Races: Greet triggers upcoming race~
0 hw 100
~
* starts a race countdown if a player shows up and no race is waiting
if %actor.is_npc%
  halt
end
if !%self.varexists(next_race_time)%
  racework init
end
* also check watching: clear watching if own pixy isn't in the race
set watch_var watching_%actor.id%
if %self.var(%watch_var%,0)% > 0 && %self.owner1% != %actor.id% && %self.owner2% != %actor.id% && %self.owner3% != %actor.id%
  set %watch_var% 0
  remote %watch_var% %self.id%
end
* ensure running soon
set max_time 300
if %self.next_race_time% > %timestamp && (%self.next_race_time% <= %timestamp% + %max_time%)
  * already running
  halt
end
* let's start it now; it will announce itself
racework countdown %max_time%
~
#11913
Pixy Races: Catch pixy in jar command~
1 c 2
catch~
set ok_list 615 616 10042 11520 11521 11522 11523 11524 11525 11526 11820 16624 16625 11963
set clever_list 11819 11982
set error_list 11873 11874 11875 11876 11877 11878 11879 11880 11881 11882 11883 11884 11885 11886 11887
set jar_vnum 11914
return 1
if %actor.fighting%
  %send% %actor% You can't do that while fighting!
  halt
elseif !%arg%
  %send% %actor% What do you want to catch with @%self%?
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% They must have flown away when they saw you coming, because they're not here.
  halt
elseif !%target.is_npc%
  %send% %actor% You can really only catch pixies with those jars.
  halt
elseif %target.fighting%
  %send% %actor% You can't catch someone who's in combat!
  halt
end
* basic vnum validations
if %clever_list% ~= %target.vnum%
  %send% %actor% You try to catch ~%target% but &%target%'s too clever for you!
  halt
elseif %error_list% ~= %target.vnum%
  %send% %actor% &%target% is way too big to fit in the jar!
  halt
elseif !(%ok_list% ~= %target.vnum%)
  %send% %actor% You can really only catch pixies in this jar.
  halt
end
* switch will validate the target vnum and set up stats
set speed 1
set guile 1
set luck 1
set pixy a pixy
switch %target.vnum%
  case 11963
    * renegade pixy
    set pixy a renegade pixy
    eval speed 1 + %random.3%
    eval guile 1 + %random.3%
    eval luck 1 + %random.3%
  break;
  case 615
    * feral pixy
    set pixy a feral pixy
    eval speed 1 + %random.2%
    eval guile 1 + %random.2%
    eval luck 1 + %random.2%
  break
  case 616
    * pixy queen / enchanted forest (easier because rare)
    set pixy a pixy queen
    eval speed 1 + %random.3%
    eval guile 1 + %random.3%
    eval luck 1 + %random.3%
  break
  case 11919
    * escaped pixy / skycleave
    set pixy an escaped pixy
    set speed %random.2%
    set guile 1
    set luck 1
  break
  case 11520
    * royal pixy (event)
    set pixy a royal pixy
    eval speed 1 + %random.3%
    eval guile 1 + %random.3%
    eval luck 1 + %random.3%
  break
  default
    * all other vnums in the pixy_list
    set speed %random.2%
    set guile %random.2%
    set luck %random.2%
    set pixy %target.name%
  break
done
%send% %actor% You swoop toward ~%target% with an enchanted jar...
%echoaround% %actor% ~%actor% swoops toward ~%target% with an enchanted jar...
* short wait, then re-validate
wait 2 sec
if !%target%
  %send% %actor% The pixy got away!
  halt
end
if !%target.is_npc% || %target.fighting% || %actor.fighting% || %actor.room% != %target.room% || %actor.disabled%
  * fail with no message (reason should be obvious)
  halt
end
* Success!
%load% obj %jar_vnum% %actor% inv
set jar %actor.inventory%
if %jar.vnum% != %jar_vnum%
  %send% %actor% Something went wrong.
  halt
end
* store vars to jar
remote pixy %jar.id%
remote speed %jar.id%
remote guile %jar.id%
remote luck %jar.id%
set wins 0
remote wins %jar.id%
set losses 0
remote losses %jar.id%
set last_race 0
remote last_race %jar.id%
* messaging
%send% %actor% You catch ~%target% in a jar!
%echoaround% %actor% ~%actor% catches ~%target% in a jar!
%purge% %target%
%purge% %self%
~
#11914
Pixy Races: Check, name, or release pixy jar (command)~
1 c 2
check name release~
* vars
set race_time 180
return 1
* args
set arg1 %arg.car%
set arg2 %arg.cdr%
if !%arg1%
  if check /= %cmd%
    %send% %actor% Usage: check <jar>
  elseif release /= %cmd%
    %send% %actor% Usage: release <jar>
  else
    %send% %actor% Usage: name <jar> <name>
  end
  halt
end
* targeting
set obj %actor.obj_target(%arg1%)%
if !%obj%
  %send% %actor% You don't see that here.
  halt
elseif %obj.vnum% == 11836 && release /= %cmd%
  * hit another release script; ignore
  return 0
  halt
elseif %obj% != %self%
  %send% %actor% You can't do that with @%obj%.
  halt
end
* check vnum
if %self.vnum% == 11913
  %send% %actor% There's no pixy in the jar. Try catching one.
  halt
end
* which command?
if check /= %cmd%
  * check racing
  %send% %actor% You check the pixy jar. It contains: %self.pixy%
  %send% %actor% Wins: %self.wins%, Losses: %self.losses%
  * show stats
  set stat_list speed guile luck
  while %stat_list%
    set stat %stat_list.car%
    set stat_list %stat_list.cdr%
    set value %self.var(%stat%,1)%
    if %stat% == luck
      set stat %stat% \&0
    end
    if %value% >= 5
      %send% %actor% %stat%: \*\*\*\*\* 5
    elseif %value% == 4
      %send% %actor% %stat%:  \*\*\*\* 4
    elseif %value% == 3
      %send% %actor% %stat%:   \*\*\* 3
    elseif %value% == 2
      %send% %actor% %stat%:    \*\* 2
    else
      %send% %actor% %stat%:     \* 1
    end
  done
elseif release /= %cmd%
  if %obj.last_race% + %race_time% > %timestamp%
    %send% %actor% You can't do that while your pixy is racing.
  elseif !%arg2% || %arg2% != CONFIRM
    %send% %actor% Are you sure? You must type 'release jar CONFIRM' to do this.
  else
    %send% %actor% You release %self.pixy%, who flies away!
    %echoaround% %actor% ~%actor% releases %self.pixy% from a jar and it flies away!
    %purge% %self%
  end
elseif name /= %cmd%
  * name
  if %self.varexists(named)% && %self.pixy% != %arg2%
    %send% %actor% It's already named %self.pixy%.
  elseif %obj.last_race% + %race_time% > %timestamp%
    %send% %actor% You can't do that while your pixy is racing.
  elseif !%arg2%
    %send% %actor% Name it what?
  elseif %arg2.strlen% > 18
    %send% %actor% That name is too long.
  else
    set first %arg2.car%
    if %first% == a || %first% == the
      * do not auto-cap
      set pixy %arg2%
    else
      * auto-cap
      set pixy %arg2.cap%
    end
    remote pixy %self.id%
    set named 1
    remote named %self.id%
    %send% %actor% You name your pixy %pixy%.
    %mod% %self% keywords pixy jar %pixy%'s
    %mod% %self% shortdesc %pixy%'s pixy jar
  end
else
  return 0
end
~
#11915
Skithe Ler-Wyn combat: Gash of Cronus, Forsaken Fate, Cut Short, Skithe Variations~
0 k 50
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_r %self.var(moves_r)%
set num_r %self.var(num_r,0)%
if !%moves_r% || !%num_r%
  set moves_r 1 2 3 4
  set num_r 4
end
* pick
eval which %%random.%num_r%%%
set old %moves_r%
set moves_r
set move 0
while %which% > 0
  set move %old.car%
  if %which% != 1
    set moves_r %moves_r% %move%
  end
  set old %old.cdr%
  eval which %which% - 1
done
set moves_r %moves_r% %old%
* store
eval num_r %num_r% - 1
remote moves_r %self.id%
remote num_r %self.id%
* perform move
skyfight lockout 30 35
if %move% == 1
  * Gash of Cronus / rosy red light
  skyfight clear dodge
  %echo% &&m~%self% cuts a brutal gash across the sky as rosy red mana streaming off of the heartwood of time whips around the vortex...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 1 s
  %echo% &&m**** The ribbon of rosy red light streams toward you... ****&&0 (dodge)
  wait 8 s
  set ch %room.people%
  eval hit %diff% * 20
  set any 0
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)%
      if %ch.var(did_sfdodge)%
        if %diff% == 1
          dg_affect #11856 %ch% TO-HIT 25 20
        end
      else
        set any 1
        %echo% &&mThe rosy red light strikes ~%ch% in the chest and cuts right through *%ch%!&&0
        if %diff% == 4 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
          dg_affect #11851 %ch% STUNNED on 10
        end
        if %diff% >= 3
          dg_affect #11859 %ch% SLOW on 20
        end
        if %diff% >= 2
          dg_affect #11859 %ch% TO-HIT -%hit% 20
        end
        %damage% %ch% 80 magical
      end
    end
    set ch %next_ch%
  done
  if !%any%
    %echo% &&mThe rosy red light of the gash of Cronus swirls back and cuts deeply into the vortex!&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 10
    else
      eval amount 100 - (%diff% * 10)
      dg_affect #11854 %self% DODGE -%amount% 10
    end
  end
  skyfight clear dodge
elseif %move% == 2
  * Forsaken Fate
  skyfight clear struggle
  %echo% &&m~%self% slashes the strands of fate with her claws...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 sec
  %echo% &&m**** You are bound by the strands of your own forsaken fate! ****&&0 (struggle)
  skyfight setup struggle all 20
  set ch %room.people%
  while %ch%
    set bug %ch.inventory(11890)%
    if %bug%
      set strug_char You struggle against your forsaken fate...
      set strug_room ~%%actor%% struggles against ^%%actor%% forsaken fate...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You struggle against your forsaken fate!
      set free_room ~%%actor%% manages to free *%%actor%%self!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    set ch %ch.next_in_room%
  done
  * damage
  if %diff% > 1
    set cycle 0
    while %cycle% < 5
      wait 4 s
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %ch.affect(11822)%
          %send% %ch% &&m**** You feel your very being as it's ripped apart by your forsaken fate! ****&&0 (struggle)
          %damage% %ch% 100 magical
        end
        set ch %next_ch%
      eval cycle %cycle% + 1
    done
  else
    wait 10 s
    nop %self.remove_mob_flag(NO-ATTACK)%
    wait 10 s
  end
elseif %move% == 3
  * Cut Short
  skyfight clear dodge
  %echo% &&m~%self% grows in size a hundredfold as she prepares to strike...&&0
  wait 3 s
  %echo% &&m**** &&Z~%self% reaches across the vortex and swipes with her enormous paw! ****&&0 (dodge)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 8 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  set hit 0
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)%
      if !%ch.var(did_sfdodge)%
        set hit 1
        %echo% &&mThere's a scream from ~%ch% as ^%ch% years are cut short by ~%self%!&&0
        %damage% %ch% 200 magical
      elseif %ch.is_pc%
        %send% %ch% &&mYou narrowly avoid the lion's massive paw!&&0
        if %diff% == 1
          dg_affect #11856 %ch% TO-HIT 25 20
        end
      end
    end
    set ch %next_ch%
  done
  skyfight clear dodge
  if !%hit%
    if %diff% == 1
      %echo% &&m~%self% slowly shrinks back to her original size.&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
elseif %move% == 4
  * Skithe Variations
  %echo% &&m**** &&Z~%self% begins to recite the deep magic... this won't be good! ****&&0 (interrupt and dodge)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  skyfight clear interrupt
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  while !%broke% && %cycle% <= 4
    skyfight setup dodge all
    wait 4 s
    if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou somehow manage to interrupt the Lion of Time... and not a moment too soon!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&m~%self% becomes distracted and starts garbling the words as the spell fades.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
        wait 10 s
      end
    else
      %echo% &&m~%self% roars ancient words not heard since the dawn of time...&&0
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if %ch.var(did_sfdodge)%
            %send% %ch% &&mYou move just in time as Skithe Ler-Wyn appears behind you, slashing at the air where you just stood!&&0
            if %diff% == 1
              dg_affect #11856 %ch% off
              dg_affect #11856 %ch% TO-HIT 25 20
            end
          else
            %send% %ch% &&mA lion appears behind you, cutting a deep slash with her claws!&&0
            %echoaround% %ch% &&m~%ch% shouts out in pain as a time lion appears behind *%ch% and cuts deep!&&0
            if %diff% > 1
              %dot% #11842 %ch% 125 20 physical 5
            end
            %damage% %ch% 125 physical
          end
        end
        set ch %next_ch%
      done
    end
    if !%broke% && %cycle% < 4
      %echo% &&m**** The lion is still casting... ****&&0 (interrupt and dodge)
    end
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11916
Pixy Races: Race command for players~
0 c 0
race watch~
* Check status or enter a pixy, or choose who to watch
return 1
if race /= %cmd% && %actor.is_pc%
  * usage 1: race [jar]
  * 1. no-arg: just show status
  if !%arg%
    %send% %actor% == Pixy Races ==
    if %self.state% == 1
      * not started
      set pos 1
      while %pos% <= 3
        if %pos% <= %self.count%
          set name %self.var(name%pos%)%
          set owner %self.var(owner%pos%,0)%
          if %owner% > 0
            makeuid owner %owner%
          end
          if %owner%
            set owner_name (%owner.name%)
          else
            set owner_name
          end
          %send% %actor% %pos%. %name% %owner_name%
        else
          %send% %actor% %pos%. <open>
        end
        eval pos %pos% + 1
      done
      if %self.count% < 3 && %actor.inventory(11914)% && %actor.id% != %self.owner1% && %actor.id% != %self.owner2%
        %send% %actor% Type 'race <jar>' to enter your pixy!
      end
    elseif %state% == 2
      * race running
      set pos 1
      while %pos% <= 3
        set name %self.var(name%pos%)%
        set owner %self.var(owner%pos%,0)%
        set place %self.var(place%pos%)%
        if %owner% > 0
          makeuid owner %owner%
        end
        if %owner%
          set owner_name (%owner.name%) -
        else
          set owner_name -
        end
        if %place% == 1
          set place 1st place
        elseif %place% == 2
          set place 2nd place
        else
          set place 3rd place
        end
        %send% %actor% %pos%. %name% %owner_name% %place%
        eval pos %pos% + 1
      done
    else
      %send% %actor% The track is being cleaned.
    end
    halt
    * end of no-arg version
  end
  * 2. basic targeting w/arg
  set jar %actor.obj_target_inv(%arg%)%
  if %self.state% != 1
    %send% %actor% You can't enter any pixies right now.
    halt
  elseif !%jar%
    %send% %actor% You don't seem to have %arg.ana% %arg%.
    halt
  elseif %jar.vnum% != 11914
    %send% %actor% You can't race @%jar%.
    halt
  elseif %self.count% >= 3
    %send% %actor% Three pixies are already racing. You'll have to wait for next time.
    halt
  elseif %self.owner1% == %actor.id% || %self.owner2% == %actor.id% || %self.owner3% == %actor.id%
    %send% %actor% You already have a pixy in this race.
    halt
  end
  * should be ok!
  %send% %actor% You enter %jar.pixy% in the race!
  %echoaround% %actor% ~%actor% enters %jar.pixy% in the race!
  eval count %self.count% + 1
  remote count %self.id%
  * store pixy stats
  set name%count% %jar.pixy%
  set speed%count% %jar.speed%
  set guile%count% %jar.guile%
  set luck%count% %jar.luck%
  set owner%count% %actor.id%
  set jar%count% %jar.id%
  remote name%count% %self.id%
  remote speed%count% %self.id%
  remote guile%count% %self.id%
  remote luck%count% %self.id%
  remote owner%count% %self.id%
  remote jar%count% %self.id%
  * store watching
  set watching_%actor.id% %count%
  remote watching_%actor.id% %self.id%
  * update pixy race time
  set last_race %timestamp%
  remote last_race %jar.id%
  * start race early if we have max pixies
  if %count% >= 3
    wait 1
    set next_race_time %timestamp%
    remote next_race_time %self.id%
    say The race is getting ready to start...
  elseif %self.next_race_time% > (%timestamp% + 60)
    * if not, set a max wait time of 1m
    eval next_race_time %timestamp% + 60
    remote next_race_time %self.id%
    say Last call for more racers.
  end
elseif watch /= %cmd% && %actor.is_pc%
  * usage 2: watch [pixy name]
  if !%arg%
    %send% %actor% Watch which pixy?
    halt
  end
  if %self.count% >= 1 && %self.name1% /= %arg%
    set found 1
  elseif %self.count% >= 2 && %self.name2% /= %arg%
    set found 2
  elseif %self.count% >= 3 && %self.name3% /= %arg%
    set found 3
  else
    %send% %actor% No pixy called '%arg%' is racing right now.
    halt
  end
  set watching_%actor.id% %found%
  remote watching_%actor.id% %self.id%
  %send% %actor% You will now watch %self.var(name%found%)% in the race.
else
  return 0
end
~
#11917
Skycleave: Time traveler's corpse~
2 gwA 100
~
* loads a corpse only if a person visits after having been here on a previous day
set corpse_vnum 11927
set page_vnum 11917
if %room.contents(%corpse_vnum%)% || %actor.is_npc%
  * corpse already present or NPC triggered this
  halt
end
if %actor.varexists(skycleave_tt_corpse)%
  * fetch var
  set skycleave_tt_corpse %actor.skycleave_tt_corpse%
else
  * no var: just set it to today and exit out
  set skycleave_tt_corpse %dailycycle%
  remote skycleave_tt_corpse %actor.id%
  halt
end
if %skycleave_tt_corpse% == 0 || %skycleave_tt_corpse% == %dailycycle%
  * actor doesn't qualify
  halt
end
* if we got here, load the corpse
%load% obj %corpse_vnum%
* put page inside
set corpse %room.contents(%corpse_vnum%)%
if %corpse% && %corpse.vnum% == %corpse_vnum%
  %load% obj %page_vnum% %corpse%
end
* don't announce it... anyone who didn't see it the first time just "didn't notice"
~
#11918
Pixy Races: Racework (command)~
0 c 0
racework~
* configure:
set win_distance 150
set area_marks 50 65 75 90 100 115 150
set no_pass_areas 2 4
if %actor% != %self%
  return 0
  halt
end
set mode %arg.car%
set arg %arg.cdr%
if %mode% == init
  * racework init: basic vars
  set state 1
  set count 0
  set first 0
  eval next_race_time %timestamp% + 86400
  remote state %self.id%
  remote count %self.id%
  remote first %self.id%
  remote next_race_time %self.id%
  * entrant vars
  set pos 1
  while %pos% <= 3
    set name%pos% pixy %pos%
    set speed%pos% 1
    set guile%pos% 1
    set luck%pos% 1
    set owner%pos% 0
    set jar%pos% 0
    set dist%pos% 0
    set area%pos% 0
    set prev_area%pos% 0
    set place%pos% 1
    set prev_place%pos% 1
    set gap%pos% 0
    set prev_gap%pos% 0
    set penalty%pos% 0
    set winner%pos% 0
    remote name%pos% %self.id%
    remote speed%pos% %self.id%
    remote guile%pos% %self.id%
    remote luck%pos% %self.id%
    remote owner%pos% %self.id%
    remote jar%pos% %self.id%
    remote dist%pos% %self.id%
    remote area%pos% %self.id%
    remote prev_area%pos% %self.id%
    remote place%pos% %self.id%
    remote prev_place%pos% %self.id%
    remote gap%pos% %self.id%
    remote prev_gap%pos% %self.id%
    remote penalty%pos% %self.id%
    remote winner%pos% %self.id%
    eval pos %pos% + 1
  done
elseif %mode% == countdown
  * racework countdown [timer in seconds]
  * set a timer to start the race, defaults to 5 minutes
  if %arg% && %arg% > 0
    set seconds %arg%
  else
    set seconds 300
  end
  eval next_race_time %timestamp% + %seconds%
  remote next_race_time %self.id%
elseif %mode% == announce_countdown
  * racework announce_countdown: give time remaining; fails if no race set
  set when %self.next_race_time%
  if %when% > %timestamp%
    eval minutes (%when% - %timestamp%) / 60
    eval seconds (%when% - %timestamp%) // 60
    if %minutes% > 1
      set long_when %minutes% minutes
    elseif %minutes% == 1
      set long_when 1 minute
    else
      set long_when
    end
    if %minutes% > 0 && %seconds% > 0
      set long_when %long_when% and
    end
    if %seconds% > 1
      set long_when %long_when% %seconds% seconds
    elseif %seconds% == 1
      set long_when %long_when% 1 second
    end
    %echo% The race will begin in %long_when%.
  end
elseif %mode% == fillin
  * ideally 8 each; stats are based on list pos
  switch %arg%
    case 1
      set name_list Discord Strife Cruelwing Jaws Kysleve Fearon Speedy Fasty
      set list_size 8
    break
    case 2
      set name_list Spite Sprite Spitfire Doom-Wing Doomer Faithbreaker Vyla Blaze
      set list_size 8
    break
    case 3
      set name_list Malice Scorn Teeny Smols Bitey Marrowgnaw Needleknife Caterkiller
      set list_size 8
    break
    default
      * only works for 1/2/3
      halt
    break
  done
  eval pos %%random.%list_size%%%
  eval stats 1 + (%pos% / 2)
  while %pos% > 0 && %name_list%
    set name %name_list.car%
    set name_list %name_list.cdr%
    eval pos %pos% - 1
  done
  * set up stats
  set count %arg.car%
  set name%count% %name%
  set speed%count% %stats%
  set guile%count% %stats%
  set luck%count% %stats%
  set owner%count% 0
  set jar%count% 0
  remote name%count% %self.id%
  remote speed%count% %self.id%
  remote guile%count% %self.id%
  remote luck%count% %self.id%
  remote owner%count% %self.id%
  remote jar%count% %self.id%
elseif %mode% == places
  * determines what place everyone is in and updates their prev_place/gap and place/gap
  * this can also detect winners
  set best_dist 0
  set best_pos 0
  set sec_dist 0
  set sec_pos 0
  set third_dist 0
  set third_pos 0
  * loop first to determine places and store prev_place/gap
  set pos 1
  while %pos% <= 3
    set prev_place%pos% %self.var(place%pos%)%
    remote prev_place%pos% %self.id%
    set prev_gap%pos% %self.var(gap%pos%)%
    remote prev_gap%pos% %self.id%
    set dist %self.var(dist%pos%)%
    if %dist% > %best_dist% || !%best_pos%
      set third_pos %sec_pos%
      set third_dist %sec_dist%
      set sec_pos %best_pos%
      set sec_dist %best_dist%
      set best_pos %pos%
      set best_dist %dist%
    elseif %dist% > %sec_dist% || !%sec_pos%
      set third_pos %sec_pos%
      set third_dist %sec_dist%
      set sec_pos %pos%
      set sec_dist %dist%
    else
      set third_pos %pos%
      set third_dist %dist%
    end
    eval pos %pos% + 1
  done
  * store places
  set place%best_pos% 1
  set place%sec_pos% 2
  set place%third_pos% 3
  remote place1 %self.id%
  remote place2 %self.id%
  remote place3 %self.id%
  * store gaps
  eval gap%best_pos% %best_dist% - %sec_dist%
  eval gap%sec_pos% %sec_dist% - %third_dist%
  set gap%third_pos% 0
  remote gap1 %self.id%
  remote gap2 %self.id%
  remote gap3 %self.id%
  * store winners?
  if %best_dist% >= %win_distance%
    set winner%best_pos% 1
    remote winner%best_pos% %self.id%
    if %sec_dist% == %best_dist%
      set winner%sec_pos% 1
      remote winner%sec_pos% %self.id%
    end
    if %third_dist% == %best_dist%
      set winner%third_pos% 1
      remote winner%third_pos% %self.id%
    end
  end
elseif %mode% == round
  * one round: update distances and penalties
  set pos 1
  while %pos% <= 3
    * update distance
    eval this_dist %self.var(dist%pos%)% + %self.var(speed%pos%)% + %random.5% - %self.var(penalty%pos%)%
    set dist%pos% %this_dist%
    remote dist%pos% %self.id%
    * update penalty
    eval penalty %self.var(penalty%pos%)% - %self.var(luck%pos%)%
    if %penalty% > 0
      set penalty%pos% %penalty%
    else
      set penalty%pos% 0
    end
    remote penalty%pos% %self.id%
    * also extract former place here for a place-to-pos map
    set place %self.var(place%pos%)%
    set place%place% %pos%
    eval pos %pos% + 1
  done
  * check area to see if anyone is in a no-passing zone
  if %no_pass_areas% ~= %self.var(area%place1%)% && %self.var(dist%place2%)% > %self.var(dist%place1%)%
    eval dist%place2% %self.var(dist%place1%)% - 1
    remote dist%place2% %self.id%
  end
  if %no_pass_areas% ~= %self.var(area%place2%)% && %self.var(dist%place3%)% > %self.var(dist%place2%)%
    eval dist%place3% %self.var(dist%place2%)% - 1
    remote dist%place3% %self.id%
  end
  * check areas
  set area 0
  set area1 0
  set area2 0
  set area3 0
  while %area_marks%
    eval area %area% + 1
    set dist %area_marks.car%
    set area_marks %area_marks.cdr%
    if %self.dist1% >= %dist%
      set area1 %area%
    end
    if %self.dist2% >= %dist%
      set area2 %area%
    end
    if %self.dist3% >= %dist%
      set area3 %area%
    end
  done
  * store areas
  set pos 1
  while %pos% <= 3
    set prev_area%pos% %self.var(area%pos%)%
    remote prev_area%pos% %self.id%
    remote area%pos% %self.id%
    eval pos %pos% + 1
  done
end
~
#11919
Pixy Races: Raceman (command)~
0 c 0
raceman~
* manages the 'start', 'win', and 'tricks' messages/etc argument gives different messages
if %actor% != %self%
  return 0
  halt
end
* first, pull stats for easy echoing, this puts them into name1, dist1,
* prev1 based on their current positions in the race and does NOT correspond
* to their places in stored data
set pos 1
while %pos% <= 3
  set place %self.var(place%pos%)%
  * moves them based on current place in TEMPORARY data
  set pos%place% %pos%
  set name%place% %self.var(name%pos%)%
  set dist%place% %self.var(dist%pos%)%
  set gap%place% %self.var(gap%pos%)%
  set guile%place% %self.var(guile%pos%)%
  set luck%place% %self.var(luck%pos%)%
  set winner%place% %self.var(winner%pos%)%
  set prev_place%place% %self.var(prev_place%pos%)%
  set prev_gap%place% %self.var(prev_gap%pos%)%
  eval pos %pos% + 1
done
if %arg% == start
  * raceman start: call it as the start of the race
  if %dist1% == %dist2% && %dist1% == %dist3%
    say It's a three-way tie out of the gate!
  elseif %dist1% == %dist2%
    say It's wing-and-wing right out the gate with %name1% and %name2% in the lead!
  elseif %dist2% == %dist3%
    say %name1.cap% blasts out of the gate and takes an early lead with %name2% and %name3% wing to wing right behind!
  else
    say %name1.cap% blasts out of the gate and takes an early lead, followed by %name2%, with %name3% trailing!
  end
  * end of start-race call
elseif %arg% == tricks
  eval guile_roll %%random.%guile2%%%
  eval guile_def %%random.%guile1%%%
  if %guile_roll% > %guile_def%
    wait 1
    if %gap1% < 20
      * small gap
      switch %guile_roll%
        case 1
          say %name2.cap% slams into %name1%! There's no penalty for that in this sport!
          eval penalty%pos1% %penalty1% + 2
          remote penalty%pos1% %self.id%
        break
        case 2
          say %name2.cap% sprinkles %name1% with pixy dust! Dirty tricks!
          eval penalty%pos1% %penalty1% + 3
          remote penalty%pos1% %self.id%
        break
        case 3
          say %name2.cap% blasts %name1% with pixy dust! That scoundrel!
          eval penalty%pos1% %penalty1% + 5
          remote penalty%pos1% %self.id%
        break
        case 4
          say %name2.cap% hurls an acorn at %name1%'s head! That had to hurt!
          eval penalty%pos1% %penalty1% + 7
          remote penalty%pos1% %self.id%
        break
        case 5
          say %name2.cap% hurls a fireball at %name1%! That's going to leave a scorchmark!
          eval penalty%pos1% %penalty1% + 8
          remote penalty%pos1% %self.id%
        break
      done
    else
      * large gap
      switch %random.3%
        case 1
          say %name2.cap% hurls a spiked blue shell at %name1%! That has to sting!
          eval penalty%pos1% %penalty1% + 10
          remote penalty%pos1% %self.id%
        break
        case 2
          say %name2.cap% calls a lightningbolt down through the window! It hits %name1% with a loud sizzle!
          eval penalty%pos1% %penalty1% + 14
          remote penalty%pos1% %self.id%
        break
        case 3
          say %name2.cap% teleports! That has to be cheating, right? No? Everything is legal in Pixy Racing!
          eval dist%pos2% %self.dist2% + 20
          remote dist%pos2% %self.id%
        break
      done
    end
  end
elseif %arg% == win
  * raceman win: announce victory
  if %self.winner1% && %self.winner2% && %self.winner3%
    say It's a three-way tie! Everybody wins!
  elseif %self.winner1% && %self.winner2%
    say %self.name1.cap% and %self.name2% cross the finish line at exactly the same time! We have a tie!
  elseif %self.winner1% && %self.winner3%
    say %self.name1.cap% and %self.name3% cross the finish line at exactly the same time! We have a tie!
  elseif %self.winner3% && %self.winner2%
    say %self.name3.cap% and %self.name2% cross the finish line at exactly the same time! We have a tie!
  elseif %self.winner1%
    say %self.name1.cap% wins!
  elseif %self.winner2%
    say %self.name2.cap% wins!
  elseif %self.winner3%
    say %self.name3.cap% wins!
  end
  * reward victory
  set pos 1
  while %pos% <= 3
    * message owner
    set owner_id %self.var(owner%pos%)%
    set name %self.var(name%pos%)%
    set winner %self.var(winner%pos%)%
    if %owner_id% > 0
      makeuid owner %owner_id%
      if %owner% && %owner.is_pc% && %owner.id% == %owner_id%
        if %winner%
          %send% %owner% Your pixy, %name%, won the race!
          if %owner.on_quest(11914)%
            %quest% %owner% trigger 11914
          end
        else
          %send% %owner% Your pixy, %name%, lost the race.
        end
        if %owner.on_quest(11913)%
          %quest% %owner% trigger 11913
        end
      else
        * did not find
        unset owner
      end
    end
    * try finding the jar
    set jar %self.var(jar%pos%,0)%
    if %jar% > 0
      makeuid jar %jar%
      if %jar% && %jar.vnum% == 11914
        * found a jar! clear race timer
        set last_race 0
        remote last_race %jar.id%
        * update stats
        if %winner%
          eval wins %jar.wins% + 1
          remote wins %jar.id%
        else
          eval losses %jar.losses% + 1
          remote losses %jar.id%
        end
        * check skillup
        eval skillup (%winner% && %random.4% == 4) || (!%winner% && %random.2% == 2)
        set message 0
        if %skillup%
          set which %random.3%
          if %which% == 1 && %jar.speed% < 5
            eval speed %jar.speed% + 1
            remote speed %jar.id%
            set message 1 speed
          elseif %which% == 2 && %jar.guile% < 5
            eval guile %jar.guile% + 1
            remote guile %jar.id%
            set message 1 guile
          elseif %which% == 3 && %jar.luck% < 5
            eval luck %jar.luck% + 1
            remote luck %jar.id%
            set message 1 luck
          end
        end
        if %message% && %owner%
          %send% %owner% %name% has leveled up and gained %message%!
        end
      end
    end
    eval pos %pos% + 1
  done
  * clear watching here
  set ch %self.room.people%
  while %ch%
    if %ch.is_pc%
      set watching_%ch.id% 0
      remote watching_%ch.id% %self.id%
    end
    set ch %ch.next_in_room%
  done
end
~
#11920
Pixy Races: Main race controller~
0 ab 100
~
if !%self.varexists(state)%
  * uninitialized? (first run)
  racework init
elseif %self.state% == 0
  racework init
elseif %self.state% == 1
  * preparing to run
  if %self.next_race_time% <= %timestamp%
    * start time! set up data and ensure 3 racers
    set pos 1
    while %pos% <= 3
      * ensure we have 3 racers
      if %pos% > %self.count%
        racework fillin %pos%
      end
      * determine initial distance and position
      set luck %self.var(luck%pos%)%
      eval dist%pos% %self.var(speed%pos%)% + %%random.%luck%%%
      remote dist%pos% %self.id%
      eval pos %pos% + 1
    done
    set count 3
    remote count %self.id%
    * announce the start
    say The race is ready to start! In lane 1, we have %self.name1%! In lane 2, it's %self.name2%! And in lane 3, give it up for %self.name3%!
    wait 1 sec
    * determine and announce initial places
    racework places
    %echo% There's a loud BANG! sound, and the tiny gate opens!
    set ch %self.room.people%
    while %ch%
      if %ch.is_pc%
        set varname watching_%ch.id%
        if %self.var(%varname%,0)% == 0
          %send% %ch% Type 'watch <pixy name>' if you want to follow one through the race.
        end
      end
      set ch %ch.next_in_room%
    done
    wait 1
    raceman start
    * update the state to start the race
    set state 2
    remote state %self.id%
  elseif %self.varexists(skip_announce)% && (%self.next_race_time% - %timestamp%) > 45
    * announce start every other time, unless timer is short
    rdelete skip_announce %self.id%
  else
    racework announce_countdown
    set skip_announce 1
    remote skip_announce %self.id%
  end
elseif %self.state% == 2
  * RACE RUNNING: run twice, with a gap
  set iter 0
  set won 0
  while %iter% < 2 && !%won%
    * move them
    racework round
    * update positions/gaps and check for winners
    racework places
    if %self.winner1% || %self.winner2% || %self.winner3%
      set won 1
      raceman win
      set state 3
      remote state %self.id%
    else
      racecall
      wait 6 sec
    end
    eval iter %iter% + 1
  done
elseif %self.state% == 3
  * RESET RACE
  * reset for next time (short reset only if players are present)
  racework init
  set any 0
  set ch %self.room.people%
  while %ch% && !%any%
    if %ch.is_pc%
      set any 1
    end
    set ch %ch.next_in_room%
  done
  if %any%
    eval next_race_time %timestamp% + 300
  else
    eval next_race_time %timestamp% + 86400
  end
  remote next_race_time %self.id%
end
~
#11921
Skycleave: Search ability for hints and secret passages~
2 p 100
~
return 1
if %ability% != 18 && %abilityname% != Search
  * wrong ability
  halt
end
set spirit %instance.mob(11900)%
switch %room.template%
  case 11815
  case 11915
    * goblin cages (both phases)
    if %room.north(room)%
      * Already open
      halt
    end
    * Not already open
    %send% %actor% You search around and find a hidden switch on the side of the bin...
    %echoaround% %actor% ~%actor% seems to have found something while searching around...
    %load% mob 11923
    return 0
  break
  case 11817
  case 11917
    * Back of the passage
    if !%room.northwest(room)%
      %send% %actor% You search around and notice a note by the exit to the passage.
      %send% %actor% It reads: Secret Pass Phrase 'O ala lilo'
      %echoaround% %actor% ~%actor% searches around and finds a note.
      return 0
    end
  break
  case 11822
  case 11922
    * Outside the passage, walkway side
    if !%room.southeast(room)%
      %send% %actor% It looks like there's a secret passage in the wall. Perhaps one of the portraits has a hint on how to open it.
      %echoaround% %actor% ~%actor% searches around.
      return 0
    end
  break
  case 11835
    * Eruditorium
    if %room.people(11847)%
      %send% %actor% It doesn't look like it would be a good idea to get in Kara Virduke's way as she searches the room.
    else
      %send% %actor% You search the room, but can't figure out what Kara Virduke was looking for in here.
      %echoaround% %actor% ~%actor% searches around the room.
    end
    return 0
  break
  case 11836
    * Lich labs
    if !%spirit.lich_released%
      %send% %actor% You search around but keep noticing the lich's repository rattling, and the instructions on the blackboard.
      %echoaround% %actor% ~%actor% searches around the room.
      return 0
    end
  break
  case 11839
  case 11939
    * Enchanting lab
    if !%room.east(room)%
      %send% %actor% You search around the illusory wall, trying to figure out how to open it, but can't find a way.
      %send% %actor% Perhaps there's a clue in the painting's name.
      %echoaround% %actor% ~%actor% searches around the wall.
      return 0
    end
  break
  case 11841
    * Attunement lab 3A
    %send% %actor% You're not sure how anybody finds anything in this place... it's a mess.
    %echoaround% %actor% ~%actor% searches around fruitlessly.
    return 0
  break
done
~
#11922
Skycleave: Secret passage levers~
1 c 4
pull~
if !%arg.car% || %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
if %self.val0%
  %send% %actor% Someone has already pulled the lever.
  halt
end
%send% %actor% You pull the lever!
%echoaround% %actor% ~%actor% pulls the lever!
if %self.room.template% == 11816 || %self.room.template% == 11916
  %load% mob 11923
elseif %self.room.template% == 11817 || %self.room.template% == 11917
  %load% mob 11924
end
~
#11923
Skycleave: Open secret passage from goblin side~
0 n 100
~
if %self.room.template% < 11800 || %self.room.template% > 11999
  * Only works in Skycleave
  %purge% %self%
  halt
end
* Ensure part of the instance
nop %self.link_instance%
wait 0
* link outside, phase A
mgoto i11815
levtog
%door% %self.room% n room i11816
%echo% There's a low rumble as the wall slides open!
* link outside, phase B
mgoto i11915
levtog
%door% %self.room% n room i11916
%echo% There's a low rumble as the wall slides open!
* link inside, phase A
mgoto i11816
levtog
%door% %self.room% s room i11815
%echo% The exit to the secret passage slides open!
* link inside, phase B
mgoto i11916
levtog
%door% %self.room% s room i11915
%echo% The exit to the secret passage slides open!
* done
%purge% %self%
~
#11924
Skycleave: Open secret passage from hall side~
0 n 100
~
if %self.room.template% < 11800 || %self.room.template% > 11999
  * Only works in Skycleave
  %purge% %self%
  halt
end
* Ensure part of the instance
nop %self.link_instance%
wait 0
* link outside, phase A
mgoto i11822
levtog
%door% %self.room% se room i11817
%echo% There's a low rumble and a scraping noise as a passage in the southeast wall slides open!
* link outside, phase B
mgoto i11922
levtog
%door% %self.room% se room i11917
%echo% There's a low rumble and a scraping noise as a passage in the southeast wall slides open!
* link inside, phase A
mgoto i11817
levtog
%door% %self.room% nw room i11822
%echo% The exit to the secret passage slides open!
* link inside, phase B
mgoto i11917
levtog
%door% %self.room% nw room i11922
%echo% The exit to the secret passage slides open!
* done
%purge% %self%
~
#11925
Skycleave: levtog to disable levers~
0 c 0
levtog~
if %actor% != %self%
  return 0
  halt
end
* This script disables the levers
set lever %self.room.contents(11922)%
if %lever%
  nop %lever.val0(1)%
end
~
#11926
Skycleave: Open Magichanical Lab (Behold, Everything Bagel)~
2 d 0
behold~
* Check phrase
if !(%speech% ~= behold && %speech% ~= everything && %speech% ~= bagel)
  halt
end
* Check spirit
set spirit %instance.mob(11900)%
if !%spirit%
  halt
end
wait 1
if %spirit.lab_open%
  halt
end
* Add exits
%door% i11839 e room i11840
%at% i11839 %echo% The east wall flickers and fades, revealing the Magichanical Lab!
%door% i11939 e room i11940
%at% i11939 %echo% The east wall flickers and fades, revealing the Magichanical Lab!
* Mark as open
set lab_open 1
remote lab_open %spirit.id%
* Cause havoc if the rogue mercenary boss is still alive
set rogue %instance.mob(11848)%
if %rogue% && !%rogue.fighting% && !%rogue.disabled%
  %force% %rogue% skyrogueslay
end
~
#11927
Skycleave: Drink Teacup~
1 s 100
~
dg_affect #11927 %actor% off silent
dg_affect #11927 %actor% MANA-REGEN -1 60
* check eligibility
if (%actor.completed_quest(11918)% || %actor.completed_quest(11919)% || %actor.completed_quest(11920)% || %actor.completed_quest(11864)%)
  * any of these quests are required for the real dream
  set dreams_only 0
else
  set dreams_only 1
end
set teleported 0
* begin loop to wait for sleep
set count 0
while %count% < 12
  wait 5 sec
  set room %actor.room%
  set to_room 0
  * see where we're at
  if %actor.position% != Sleeping
    if %teleported%
      * already moved and woke up
      dg_affect #11927 %actor% off silent
      halt
    end
  elseif !%room.function(BEDROOM)% || !%actor.can_teleport_room% || !%actor.canuseroom_guest%
    * no bedroom? nothing to do
  elseif %dreams_only%
    switch %random.7%
      case 1
        %send% %actor% You dream of fishing by a peaceful little river, alone with your thoughts.
      break
      case 2
        %send% %actor% You dream you're stuck in a cold, dark cave with square eyes watching you on all sides.
      break
      case 3
        %send% %actor% You dream of standing shoulder-to-shoulder with the smallest of heroes.
      break
      case 4
        %send% %actor% You dream of an unexpected visit to the headmistress's office.
      break
      case 5
        %send% %actor% You dream of being trapped in a cold, round, windowless cell.
      break
      case 6
        %send% %actor% You dream of perfect serenity.
      break
      case 7
        %send% %actor% You dream you're fighting a tremendous giant.
      break
    done
  else
    * Sleeping AND in a bedroom: set a target
    set to_room %instance.nearest_rmt(11973)%
  end
  * did we find a teleport target?
  if %to_room%
    * determine where to send them back to
    if (%room.template% >= 11800 && %room.template% <= 11874) || (%room.template% >= 11900 && %room.template% <= 11974)
      set skycleave_wake_room %room.template%
    elseif %actor.varexists(skycleave_wake_room)% && (%room.template% >= 11800 && %room.template% <= 11999)
      set skycleave_wake_room %actor.skycleave_wake_room%
    else
      set skycleave_wake_room 0
    end
    if !%actor.plr_flagged(ADV-SUMMON)% && !%room.template%
      nop %actor.mark_adventure_summoned_from%
    end
    * ensure boss is spawned
    set boss %to_room.people(11920)%
    if !%boss%
      %at% %to_room% %load% mob 11920
      %at% %to_room% %echo% You suddenly notice the Grand High Sorceress sitting on her desk.
    end
    * move player
    %echoaround% %actor% ~%actor% lets out a raucous snore and then vanishes into ^%actor% dream.
    %teleport% %actor% %to_room%
    nop %actor.link_adventure_summon%
    %echoaround% %actor% ~%actor% appears out of nowhere, asleep on the floor!
    %send% %actor% You dream you're flying -- and it feels so real!
    remote skycleave_wake_room %actor.id%
    set skycleave_wake 0
    remote skycleave_wake %actor.id%
    * teleport fellows
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %ch.leader% == %actor% && !%ch.fighting%
        if %ch.is_pc% && !%ch.plr_flagged(ADV-SUMMON)% && !%room.template%
          nop %ch.mark_adventure_summoned_from%
        end
        %echoaround% %ch% ~%ch% vanishes into thin air!
        %teleport% %ch% %to_room%
        if %ch.is_pc%
          nop %ch.link_adventure_summon%
          remote skycleave_wake_room %ch.id%
          remote skycleave_wake %ch.id%
        end
        %echoaround% %ch% ~%ch% appears out of nowhere!
        if %ch.position% != Sleeping
          %force% %ch% look
        end
      end
      set ch %next_ch%
    done
    set teleported 1
    dg_affect #11927 %actor% off silent
    halt
  end
  * next while loop
  eval count %count% + 1
done
dg_affect #11927 %actor% off silent
~
#11928
Skycleave: skyrogueslay command for floor 3~
0 c 0
skyrogueslay~
if %actor% != %self%
  return 0
  halt
end
if !%self.room.east(room)%
  halt
end
wait 1
if %self.fighting% || %self.disabled%
  halt
end
say Eureeker! The wall is open!
east
if %self.room.template% != 11840
  halt
end
wait 1
set ch %self.room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if (%ch.vnum% == 11840 || %ch.vnum% == 11831)
    %echo% ~%self% vanishes into the shadows and murders ~%ch% with a dagger to the back!
    %slay% %ch%
  end
  set ch %next_ch%
done
wait 1
%echo% ~%self% grabs something from the table and then vanishes into the shadows!
%purge% %self%
~
#11929
Skycleave: Leave breadcrumbs in the pixy maze~
2 qA 100
~
return 1
* basic checks
if %actor.is_npc% || !%direction% || %direction% == portal
  halt
end
* needs crummy bread
if !%actor.inventory(11929)%
  halt
end
* check for existing tracks in that dir, and bump the new ones up in the list
set iter %room.contents(11930)%
while %iter%
  set next %iter.next_in_list%
  if %iter.vnum% == 11930
    if %iter.direction% == %direction%
      * found match: purge it to put a fresh one at the top
      %purge% %iter%
      * formerly: %teleport% %iter% %room%
    end
  end
  set iter %next%
done
* Success: has breadcrumbs item, is a player, and there are no crumbs in the room
%load% obj 11930
set obj %room.contents%
if %obj.vnum% != 11930
  halt
end
* Set data on the item
%mod% %obj% longdesc A trail of breadcrumbs leads %direction%!
%mod% %obj% append-lookdesc The trail seems to lead %direction%!
remote direction %obj.id%
~
#11930
Elemental Plane of Water: Spawn boss~
2 bw 100
~
if %room.people(11928)%
  halt
end
%load% mob 11928
set mob %room.people%
if %mob.vnum% == 11928
  if %room.varexists(spawned)%
    eval spawned %room.spawned% + 1
  else
    set spawned 1
  end
  remote spawned %room.id%
  if %spawned% > 1
    wait 15 sec
    %echo% The water stirs and comes back to life... you're caught in the embodiment of the First Water!
  else
    wait 5 sec
    %echo% The water around you comes to life... you're caught in the embodiment of the First Water!
  end
end
~
#11931
Skycleave: Despawn boss and empty room when alone~
0 ab 50
~
if %self.fighting%
  halt
end
set room %self.room%
set ch %room.people%
while %ch%
  if %ch.is_pc%
    halt
  end
  set ch %ch.next_in_room%
done
* nobody here: reset the whole room
if %room.template% == 11972
  * Elemental Plane of Water: empty room
  makeuid exit room i11908
  * move any items
  set obj %room.contents%
  while %obj%
    set next_obj %obj.next_in_list%
    if %obj.can_wear(TAKE)%
      %teleport% %obj% %exit%
      %at% %exit% %echo% # @%obj% rises from the fountain.
    end
    set obj %next_obj%
  done
  * move mobs out
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch% != %self%
      %teleport% %ch% %exit%
      %at% %exit% %echoaround% %ch% ~%ch% rises from the fountain.
    end
    set ch %next_ch%
  done
end
* always: remove me
%purge% %self%
~
#11932
Elemental Plane of Water: Boss death and loot check~
0 f 100
~
set min_level 150
set room %self.room%
set ch %room.people%
set any_ok 0
while %ch%
  if %ch.is_pc%
    set varname enter_%ch.id%
    if %room.varexists(%varname%)%
      rdelete %varname% %room.id%
    end
  end
  set ch %ch.next_in_room%
done
* ensure a player has loot permission
if %actor.is_pc% && %actor.level% >= %min_level% && %self.is_tagged_by(%actor%)%
  set varname pc%actor.id%
  if %room.var(%varname%,0)% < 1
    * actor qualifies
    set %varname% 1
    remote %varname% %room.id%
    nop %self.remove_mob_flag(!LOOT)%
    set any_ok 1
  end
end
* actor didn't qualify -- find anyone present who does
set ch %room.people%
while %ch% && !%any_ok%
  if %ch.is_pc% && %ch.level% >= %min_level% && %self.is_tagged_by(%ch%)%
    set varname pc%ch.id%
    if %room.var(%varname%,0)% < 1
      * ch qualifies
      set %varname% 1
      remote %varname% %room.id%
      nop %self.remove_mob_flag(!LOOT)%
      set any_ok 1
    end
  end
  set ch %ch.next_in_room%
done
* and some messaging
%echo% &&AYou manage to defeat the First Water and everything seems to calm around you...&&0
%echo% You grab a breath while you can, but you can't shake the unsettling feeling that the First Water can never be defeated.
return 0
~
#11933
Elemental Plane of Water: Breath check~
0 bw 75
~
if !%self.varexists(scaled)%
  halt
end
* determine difficulty and limit
set diff %self.var(diff,1)%
switch %diff%
  case 2
    set limit 240
  break
  case 3
    set limit 360
  break
  case 4
    set limit 480
  break
  default
    * no drowning on normal
    halt
  break
done
set room %self.room%
set ch %room.people%
set aggro 0
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_pc% && !%ch.nohassle% && %ch.position% != Dead
    set varname enter_%ch.id%
    if %room.varexists(%varname%)%
      set time %room.var(%varname%)%
    else
      set time %timestamp%
      set %varname% %time%
      remote %varname% %room.id%
      * warn
      %send% %ch% You quickly realize it's impossible to breathe in here. Whatever you're here to do, do it quickly.
    end
    * how long is left
    eval left %time% + %limit% - %timestamp%
    if %left% <= 0
      * oh noes
      %send% %ch% You watch the world go dark as the water presses its way into your lungs and drowns you.
      %echoaround% %ch% ~%ch% has drowned!
      if %ch.is_immortal%
        makeuid exit room i11908
        %teleport% %ch% %exit%
        %load% obj 11805 %ch%
      else
        %slay% %ch% %ch.name% has drowned at %room.coords%!
      end
    elseif %left% < 50
      %send% %ch% &&rYour nose and throat HURT as water presses its way in...&&0
      set aggro 1
    elseif %left% < 90
      %send% %ch% You feel yourself begin to drown...
      set aggro 1
    end
  end
  set ch %next_ch%
done
if %aggro%
  wait 1
  if !%self.fighting%
    %aggro%
  end
end
~
#11934
Skycleave: Janitor cleanup service~
0 bi 20
~
set purge_list 1000 11864 11865 11867 11875 11876 11929 11930
wait 2 sec
set done 0
set obj %self.room.contents%
while %obj% && !%done%
  set next_obj %obj.next_in_list%
  if %purge_list% ~= %obj.vnum%
    %echo% ~%self% cleans up @%obj%.
    nop %obj.empty%
    %purge% %obj%
    set done 1
  end
  set obj %next_obj%
done
if !%done% && %self.vnum% == 11837
  * didn't find any?
  %echo% The swarm of rags scatters, with each rag tucking itself away out of sight.
  %purge% %self%
end
~
#11935
Skycleave: Detect look interaction~
0 c 0
look~
return 0
* detects looking at character
if %actor.char_target(%arg.car%)% == %self%
  * looking at me
  switch %self.vnum%
    case 11933
      * the mop / Dylane Jr
      if %actor.on_quest(11801)%
        %quest% %actor% trigger 11801
      end
    break
  done
elseif %self.vnum% == 11888 && %arg% == knezz
  * Iskip of Rot and Ruin easter egg
  %send% %actor% You can't get a good look at him from down here.
  return 1
elseif %self.vnum% == 11920 && (%arg% == mageina || %arg% == barista || %arg% == face)
  * Grand High Sorceress easter egg
  %send% %actor% She keeps her head tilted so her hat blocks her face.
  return 1
end
~
#11936
Skycleave: Room commands (Pixy Races, Lich Labs, Goblin Cages, Gate, Ossuary)~
2 c 0
touch open disturb wake awaken search attune look bet wager~
set lich_cmds touch open disturb wake awaken search
return 0
if attune /= %cmd% && %room.template% == 11841
  * attunement lab: wrong phase
  %send% %actor% You seem to be in the right place, but the wrong time. There's nobody here to attune skystones for you.
  return 1
elseif (bet /= %cmd% || wager /= %cmd%) && %room.template% == 11918
  * pixy races
  %send% %actor% It looks like they don't let guests bet on the races.
  return 1
elseif open /= %cmd% && %room.template% == 11989
  * gobbrabakh of orka gate: can't open it
  if (gates /= %arg% || doors /= %arg%)
    %send% %actor% You go to open the gate, but the guards stop you.
    %echoaround% %actor% ~%actor% moves toward the gate but is blocked by the guards.
    return 1
  end
elseif open /= %cmd% && (%room.template% == 11839 || %room.template% == 11939)
  * bagel helper
  if (%arg% == bagel || %arg% == painting || %arg% == east || %arg% == secret)
    if %room.east(room)%
      %send% %actor% The secret door is already open.
    else
      %send% %actor% No, that's not it...
    return 1
  end
elseif search /= %cmd% && (%room.template% == 11815 || %room.template% == 11915)
  * goblin cages: searching without skill?
  if !%actor.ability(Search)%
    %send% %actor% You search around but don't see anything special.
    %echoaround% %actor% ~%actor% searches around the room.
    return 1
  end
elseif (%lich_cmds% ~= %cmd%) && (%room.template% == 11836 || %room.template% == 11936)
  * lich desk
  if !%arg%
    halt
  elseif !(desk /= %arg% || antique /= %arg%)
    halt
  else
    return 1
    %send% %actor% You make the mistake of touching the antique desk...
    %echoaround% %actor% ~%actor% tries to open the antique desk...
    %echo% An ethereal spirit flies out of the desk and swirls around the room, howling as it goes!
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if !%ch.aff_flagged(!ATTACK)%
        %dot% #11936 %ch% 1000 30 magical
        %damage% %ch% 100 magical
      end
      set ch %next_ch%
    done
    %echo% The spirit returns to the desk, which slams shut with a thud!
  end
elseif look /= %cmd% && %room.template% == 11981
  return 0
  wait 0
  if skulls /= %arg% || shelves /= %arg%
    %force% %actor% scriptwake skulls
  elseif walls /= %arg% || alcoves /= %arg%
    %force% %actor% scriptwake alcoves
  elseif bones /= %arg% || ribs /= %arg% || femurs /= %arg% || piles /= %arg%
    %force% %actor% scriptwake bones
  end
end
~
#11937
Skycleave: Gossipping pages~
0 bw 50
~
set spirit %instance.mob(11900)%
* page sheila: chance to jump the no-mob barrier
if %self.vnum% == 11959 && %self.var(barrier_jump,0)% + 120 < %timestamp%
  if %self.room.template% == 11961
    northeast
    set barrier_jump %timestamp%
    remote barrier_jump %self.id%
    halt
  elseif %self.room.template% == 11963
    southeast
    set barrier_jump %timestamp%
    remote barrier_jump %self.id%
    halt
  end
end
set count 0
set target 0
while %count% < 10 && !%target%
  set person %random.char%
  if %person.is_pc% || %person.mob_flagged(HUMAN)%
    set target %person%
  end
  eval count %count% + 1
done
if !%target%
  halt
end
* per-person time limit (mobs and players both)
set varname limit_%target.id%
if %self.var(%varname%,0)% + 29 > %timestamp%
  * too soon
  halt
end
set %varname% %timestamp%
remote %varname% %self.id%
* determine random rumor list
if !%self.var(rumor_list)%
  set rumor_list 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
  set rumor_count 19
else
  * attempt to pull lists
  set rumor_list %self.var(rumor_list,1)%
  set rumor_count %self.var(rumor_count,1)%
end
* pick 2 rumors
set rumor1 0
set rumor2 0
set rumor1_text
set rumor2_text
set count 1
while %count% <= 2 && %rumor_count% >= 1
  * This should be the number of cases in the switch below
  eval pos %%random.%rumor_count%%%
  set temp_list %rumor_list%
  set rumor_list
  while %temp_list%
    set this %temp_list.car%
    set temp_list %temp_list.cdr%
    eval pos %pos% - 1
    if %pos% == 0
      set rumor %this%
    else
      set rumor_list %rumor% %rumor_list%
    end
  done
  eval rumor_count %rumor_count% - 1
  if %count% == 1 || %rumor% != %rumor1%
    * found valid rumor
    set rumor%count% %rumor%
    switch %rumor%
      case 1
        set rumor%count%_text I heard Marina and Djon are an item now.
      break
      case 2
        set rumor%count%_text I heard Ravinder followed Weyonomon right into a wall.
      break
      case 3
        set rumor%count%_text I heard the statue in the foyer used to be a sorcerer here.
      break
      case 4
        set rumor%count%_text I heard oreonics can separate themselves from their physical forms.
      break
      case 5
        set rumor%count%_text I heard the goblins were trying to break in, not out.
      break
      case 6
        set rumor%count%_text I heard someone left bread all over the second floor.
      break
      case 7
        set rumor%count%_text I heard the entire second floor was turned into a maze.
      break
      case 8
        set rumor%count%_text I heard the mercenaries got stuck outside the Magichanical lab when they couldn't read the sign to get in.
      break
      case 9
        if %instance.mob(11969)%
          set rumor%count%_text I heard a shadow got ahold of the GHS's wand and made itself real.
        else
          set rumor%count%_text I heard there was a time lion. A real one.
        end
      break
      case 10
        if %instance.mob(11969)%
          set rumor%count%_text I heard Mezvienne killed Professor Knezz.
        else
          set rumor%count%_text I heard Mezvienne was drawing power directly from Professor Knezz and the tower.
        end
      break
      case 11
        if %spirit.lich_released%
          set rumor%count%_text I heard Scaldorran shredded some of those mercenaries.
        else
          set rumor%count%_text I heard Scaldorran spent the whole invasion hidden in his lamp.
        end
      break
      case 12
        if %instance.mob(11934)%
          set rumor%count%_text I heard Sanjiv was stuck in the Eruditorium when the mercenaries took over.
        else
          set rumor%count%_text I heard someone let the otherworlder out.
        end
      break
      case 13
        if %instance.mob(11926)%
          set rumor%count%_text I heard old Wright couldn't recapture any of the goblins.
        else
          set rumor%count%_text I heard old Wright got some of the goblins into cages.
        end
      break
      case 14
        if %instance.mob(11929)%
          set rumor%count%_text I heard Mezvienne mind-controlled Professor Barrosh.
        else
          set rumor%count%_text I heard Barrosh got Paige and Grace killed when he was mind-controlled.
        end
      break
      case 15
        if %instance.mob(11940)%
          set rumor%count%_text I heard Goef was down on floor 1 when the mercenaries had the Enchanting lab. How does that work?
        else
          set rumor%count%_text I heard mercenaries killed Waltur and Niamh.
        end
      break
      case 16
        if %spirit.claw2%
          set rumor%count%_text I heard there's a secret passage on floor 2.
        else
          set rumor%count%_text I heard one of the pages got trapped in the fountain.
        end
      break
      case 17
        if %instance.mob(11965)%
          set rumor%count%_text I heard Niamh is next in line for High Sorcerer.
        else
          set rumor%count%_text I heard Professor Knezz invited the enchantress here himself.
        end
      break
      case 18
        * SEASON (not rumor) - always set rumor1_text
        set room %self.room%
        if %room.rmt_flagged(LOOK-OUT)% && %room.season% != summer
          switch %room.season%
            case winter
              set rumor1_text Oh my, it's snowing out. I could swear it was just summer.
            break
            case spring
              set rumor1_text It's a terribly nice day out for midsummer. Look at all the flowers out there.
            break
            case autumn
              set rumor1_text Oh dear, the trees are changing out there. Midsummer seems awfully early for that.
            break
        end
      break
      case 19
        * use a random SAY on me (not rumor) - always set rumor1_text
        set msg %self.custom(say)%
        if !%msg.empty%
          set rumor1_text %msg%
        end
      break
    done
    eval count %count% + 1
  else
    * did not find a valid rumor: repeat
  end
done
* store back
if %rumor_list%
  remote rumor_list %self.id%
  remote rumor_count %self.id%
else
  rdelete rumor_list %self.id%
  rdelete rumor_count %self.id%
end
* say the rumors
if !%rumor1_text.empty%
  say %rumor1_text%
  wait 3 sec
  if %target.room% == %self.room% && %target.is_npc% && %response_mobs% ~= %target.vnum% && !%rumor2_text.empty%
    %force% %target% say Oh? %rumor2_text%
  end
end
~
#11938
Walking Sorcery Tower: interior setup~
5 n 100
~
set inter %self.interior%
if !%inter%
  detach 11938 %self.id%
  halt
end
if !%inter.up(room)%
  %door% %inter% up add 11940
end
set upper %inter.up(room)%
if %upper%
  if !%upper.east(room)%
    %door% %upper% east add 11941
  end
  if !%upper.west(room)%
    %door% %upper% west add 11942
  end
end
detach 11938 %self.id%
~
#11939
Skycleave: Attune skystone at Goef the Oreonic~
0 c 0
attune~
* attunes skystones for the user
set allow_list 11900 11899
set fake_list 10036 10037
* targeting
set obj %actor.obj_target_inv(%arg.car%)%
if !%arg%
  %send% %actor% Which stone you like to attune?
  halt
elseif %arg% == all || %arg% ~= all.
  %send% %actor% You have to attune them one at a time.
  halt
elseif !%obj%
  %send% %actor% You don't seem to have %arg.ana% %arg.car%.
  halt
elseif %obj.vnum% == 11898
  %send% %actor% That skystone is already depleted. Try an unattuned stone.
  halt
elseif %fake_list% ~= %obj.vnum%
  %echo% ~%self% takes @%obj% from ~%actor% and examines it, but hands it back.
  say Alas, little human, I can't attune that one. Is it from a different timeline?
  halt
elseif !(%allow_list% ~= %obj.vnum%)
  %send% %actor% You can't attune @%obj% here. Only skystones.
  halt
end
* pull variables
if %actor.varexists(skystone_progress)%
  set skystone_progress %actor.skystone_progress%
else
  set skystone_progress 0
end
if %actor.varexists(skystone_finished)%
  set skystone_finished %actor.skystone_finished%
else
  set skystone_finished 0
end
* attempt attunement
%send% %actor% You give @%obj% to ~%self%... Goef cradles the stone, which glows with a bright blue light!
%echoaround% %actor% ~%actor% gives @%obj% to ~%self%... Goef cradles the stone, which glows a bright blue light!
eval skystone_progress %skystone_progress% + 1
if %skystone_progress% > %skystone_finished%
  * success!
  %send% %actor% For a moment you feel like you might faint, like you've lost something, but you can't quite figure out what.
  %echoaround% %actor% ~%actor% looks weak for a moment, like &%actor% might fall, but &%actor% catches ^%actor%self.
  %send% %actor% You gain 1 %currency.11900(1)%. Type 'coins' to see it.
  nop %actor.give_currency(11900,1)%
  eval skystone_progress 0
  eval skystone_finished %skystone_finished% + 1
  if %actor.on_quest(11942)%
    %quest% %actor% trigger 11942
  end
else
  * 'failure' -- it will take more
  %send% %actor% You feel stronger as the glow from the stone passes into you, but the skystone goes dark.
  %echoaround% %actor% The glow from the skystone bursts toward ~%actor% and passes into ^%actor% chest, but the stone goes dark.
  %load% obj 11898 %actor% inv
end
* remove old stone
%purge% %obj%
* store vars
remote skystone_progress %actor.id%
remote skystone_finished %actor.id%
~
#11940
Skycleave: Craft-or-Drop: Set BoE/BoP and loot quality flags~
1 n 100
~
* This script makes loot BOP when dropped by a mob but BOE when crafted.
* It will also inherit hard/group flags from an NPC and rescale itself.
* first ensure there's a person
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
end
if !%actor%
  halt
end
set rescale 0
* next check pc/npc
if %actor.is_npc%
  * BOP mode (loot)
  if !%self.is_flagged(BOP)%
    nop %self.flag(BOP)%
    set rescale 1
  end
  if %self.is_flagged(BOE)%
    nop %self.flag(BOE)%
    set rescale 1
  end
  if !%self.is_flagged(GENERIC-DROP)%
    if %actor.mob_flagged(HARD)% && !%self.is_flagged(HARD-DROP)%
      nop %self.flag(HARD-DROP)%
      set rescale 1
    end
    if %actor.mob_flagged(GROUP)% && !%self.is_flagged(GROUP-DROP)%
      nop %self.flag(GROUP-DROP)%
      set rescale 1
    end
  end
else
  * BOE mode (crafted)
  if %self.is_flagged(BOP)%
    nop %self.flag(BOP)%
    set rescale 1
  end
  if !%self.is_flagged(BOE)%
    nop %self.flag(BOE)%
    set rescale 1
  end
  * in case
  nop %self.bind(nobody)%
end
if %rescale% && %self.level%
  wait 0
  %scale% %self% %self.level%
end
detach 11902 %self.id%
~
#11941
Skycleave: Only drops loot for unique fighters~
0 f 100
~
* mob only loses !LOOT flag if a unique person over min_level has tagged it
* can also work in reverse, adding it
set room %self.room%
set min_level 150
set done 0
if %actor.is_pc% && %actor.level% >= %min_level% && %self.is_tagged_by(%actor%)%
  set varname pc%actor.id%
  if !%room.var(%varname%,0)%
    * actor qualifies
    set %varname% %dailycycle%
    remote %varname% %room.id%
    nop %self.remove_mob_flag(!LOOT)%
    set done 1
  end
end
* actor didn't qualify -- find anyone present who does
set ch %room.people%
while %ch% && !%done%
  if %ch.is_pc% && %ch.level% >= %min_level% && %self.is_tagged_by(%ch%)%
    set varname pc%ch.id%
    if !%room.var(%varname%,0)%
      * ch qualifies
      set %varname% %dailycycle%
      remote %varname% %room.id%
      nop %self.remove_mob_flag(!LOOT)%
      set done 1
    end
  end
  set ch %ch.next_in_room%
done
* ensure !LOOT otherwise
if !%done%
  nop %self.add_mob_flag(!LOOT)%
end
* and message
switch %self.vnum%
  case 11888
    if %self.mob_flagged(!LOOT)%
      %echo% &&mThe giant, Iskip, turns to ash as he falls and blows away on the breeze.&&0
    else
      %echo% &&mThe giant, Iskip, turns to ash as he falls, dropping some items on the stump as he blows away on the breeze.&&0
    end
  break
  case 11920
    if %self.mob_flagged(!LOOT)%
      %echo% &&mThe Grand High Sorceress gives you a coy smile as she disappears in a burst of magenta glitter!&&0
    else
      %echo% &&mThe Grand High Sorceress gives you a coy smile as she disappears in a burst of magenta glitter, dropping something as she vanishes!&&0
      * chance of mount:
      if !%room.people(11852)% && %random.100% <= 4
        %load% mob 11852
      end
    end
  break
done
return 0
~
#11942
Rot and Ruin: Re-spawn boss when new player arrives~
2 gA 100
~
* Iskip of Rot and Ruin (11888) respawns if any player arrives
if %actor.is_npc%
  halt
end
set mob %instance.mob(11888)%
if !%mob%
  %load% mob 11888
  %echoaround% %actor% The giant emerges once again from the still water.
end
~
#11943
Skycleave: Dreams of Smol Nes-Pik~
2 bw 100
~
* Gives sleeping players dreams -- non-teleporting version
set ch %room.people%
while %ch%
  if %ch.position% == Sleeping
    switch %random.10%
      case 1
        %send% %ch% You dream of an incredible tree with a rainbow staircase spiraling around it.
      break
      case 2
        %send% %ch% You dream of a glowing green seashell with people inside it.
      break
      case 3
        %send% %ch% You dream of a tree rotting from the core.
      break
      case 4
        %send% %ch% You dream of a time long ago.
      break
      case 5
        %send% %ch% You dream of people who tend the living mana of nature like a crop.
      break
      case 6
        %send% %ch% You dream of a robed giant splitting lumber by hand.
      break
      case 7
        %send% %ch% You dream of people fleeing in panic as a great tree falls.
      break
      case 8
        %send% %ch% You dream of people cutting every color of block from gemstones.
      break
      case 9
        %send% %ch% You dream of a still night, lit by the full moon.
      break
      case 10
        %send% %ch% You dream of impossible things.
      break
    done
  end
  set ch %ch.next_in_room%
done
* random echo section
if !%room.people%
  halt
elseif %room.people.fighting%
  halt
end
if %room.varexists(delay)%
  eval delay %room.delay% + 1
  if %delay% > 6
    set delay 1
  end
else
  set delay 1
end
remote delay %room.id%
set sleepy 0
if %delay% == 1
  switch %random.5%
    case 1
      %echo% You nearly jump out of your skin as a vine tries to wrap itself around your leg!
    break
    case 2
      %echo% Pixies taunt you from a distance, but as you try to approach, they seem to vanish!
    break
    case 3
      * sapling growth: check variables
      if %room.varexists(sapling_time)%
        set sapling_time %room.sapling_time%
      else
        set sapling_time 0
      end
      if %room.depletion(chop)% > 0 && %sapling_time% + 1800 > %timestamp%
        * too soon
        %echo% A pixy smacks you in the face with a plush sheep full of sleeping dust. Try not to snore!
        set sleepy 1
      else
        %echo% You leap out of the way as a sapling grows so fast underneath you that it threatens to impale you.
        if %room.depletion(chop)% > 0
          eval amount %room.depletion(chop)% - 1
          nop %room.depletion(chop,%amount%)%
        end
        * rate limit
        set sapling_time %timestamp%
        remote sapling_time %room.id%
      end
    break
    case 4
      %echo% You feel a tap on your shoulder but when you turn, it's just a dangling vine.
    break
    case 5
      %echo% You feel like it's getting harder to breathe in here.
    break
  done
end
if %sleepy%
  set ch %room.people%
  while %ch%
    if %ch.is_pc%
      dg_affect #11943 %ch% IMMOBILIZED on 10
    end
    set ch %ch.next_in_room%
  done
end
~
#11944
Smol Nes-Pik: Main entrance dream teleporter~
2 bw 100
~
* Sleeping players and their NPC followers teleport to Smol Nes-Pik
set to_room %instance.nearest_rmt(11875)%
if !%to_room%
  * Error?
  halt
end
set skycleave_wake_room %room.template%
set skycleave_wake 0
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_pc% && %ch.position% == Sleeping%
    * Teleport player first
    %echoaround% %ch% ~%ch% lets out a raucous snore and then vanishes into ^%ch% dream.
    %teleport% %ch% %to_room%
    %echoaround% %ch% ~%ch% appears out of nowhere, asleep on the branch!
    %send% %ch% You dream you're falling -- the kind where you really feel it!
    remote skycleave_wake_room %ch.id%
    remote skycleave_wake %ch.id%
    * bring followers
    set fol %room.people%
    while %fol%
      set next_fol %fol.next_in_room%
      if %fol.is_npc% && %fol.leader% == %ch% && !%fol.fighting%
        if %fol% == %next_ch%
          set next_ch %next_ch.next_in_room%
        end
        %echoaround% %fol% ~%fol% vanishes into thin air!
        %teleport% %fol% %to_room%
        %echoaround% %fol% ~%fol% appears out of nowhere!
        if %fol.position% != Sleeping
          %force% %fol% look
        end
      end
      set fol %next_fol%
    done
  end
  set ch %next_ch%
done
* random echo section
if !%room.people%
  halt
elseif %room.people.fighting%
  halt
end
if %room.varexists(delay)%
  eval delay %room.delay% + 1
  if %delay% > 6
    set delay 1
  end
else
  set delay 1
end
remote delay %room.id%
set sleepy 0
if %delay% == 1
  switch %random.5%
    case 1
      %echo% You nearly jump out of your skin as a vine tries to wrap itself around your leg!
    break
    case 2
      %echo% Pixies taunt you from a distance, but as you try to approach, they seem to vanish!
    break
    case 3
      * sapling growth: check variables
      if %room.varexists(sapling_time)%
        set sapling_time %room.sapling_time%
      else
        set sapling_time 0
      end
      if %room.depletion(chop)% > 0 && %sapling_time% + 1800 > %timestamp%
        * too soon
        %echo% A pixy smacks you in the face with a plush sheep full of sleeping dust. Try not to snore!
        set sleepy 1
      else
        %echo% You leap out of the way as a sapling grows so fast underneath you that it threatens to impale you.
        if %room.depletion(chop)% > 0
          eval amount %room.depletion(chop)% - 1
          nop %room.depletion(chop,%amount%)%
        end
        * rate limit
        set sapling_time %timestamp%
        remote sapling_time %room.id%
      end
    break
    case 4
      %echo% You feel a tap on your shoulder but when you turn, it's just a dangling vine.
    break
    case 5
      %echo% A pixy blows a shimmering yellow dust in your face! You begin to feel drowsy.
      set sleepy 1
    break
  done
end
if %sleepy%
  set ch %room.people%
  while %ch%
    if %ch.is_pc%
      dg_affect #11943 %ch% IMMOBILIZED on 10
    end
    set ch %ch.next_in_room%
  done
end
~
#11945
Skycleave Dreams: Triple Wake or Pinch Self to Exit~
2 c 0
wake pinch scriptwake run jump trip fall~
* Teleports the player home if they type 'wake' 3 times while already awake
* also accepts 'pinch <me/self/name>' or 'scriptwake MODE'
if !%actor.is_pc%
  return 0
  halt
end
set room %actor.room%
set fall_kws run jump trip fall
set fall_rooms 11983 11984 11985 11986 11987 11988 11989 11991 11992
set non_fall_rooms 11975 11976 11977 11978 11979 11980 11981 11982
if %actor.position% == Standing || %actor.position% == Resting || %actor.position% == Sitting || %cmd% == scriptwake
  * separate behavior for pinch vs wake:
  if %cmd.mudcommand% == wake
    * 'wake': first two times fail
    if %actor.varexists(skycleave_wake)%
      eval skycleave_wake %actor.skycleave_wake% + 1
    else
      set skycleave_wake 1
    end
    remote skycleave_wake %actor.id%
    if %skycleave_wake% == 1
      %send% %actor% You can't seem to wake up...
      halt
    elseif %skycleave_wake% == 2
      %send% %actor% Just five more minutes.
      halt
    else
      %send% %actor% You wake up with the hazy feeling you were just somewhere else.
      %echoaround% %actor% ~%actor% lets out a snore and then fades away until &%actor% vanishes!
    end
  elseif pinch /= %cmd%
    * 'pinch': must target self/me/name
    if !%arg% || (%arg% != me && %arg% != self && %actor.char_target(%arg%)% != %actor%)
      * if they aren't pinching themselves, let the pinch social handle it
      return 0
      halt
    else
      %send% %actor% You pinch yourself -- OUCH -- and suddenly wake up!
      %echoaround% %actor% ~%actor% pinches *%actor%self, lets out a short gasp, and vanishes!
    end
  elseif (%fall_kws% ~= %cmd%) && (%fall_rooms% ~= %room.template%)
    * fall ok
    if jump /= %cmd% || fall /= %cmd%
      %send% %actor% You feel dizzy as you look down over the edge but cannot bring yourself to jump.
      return 1
      halt
    else
      %send% %actor% You run along the top of the city but trip and fall off the edge!
      %send% %actor% As you plummet toward the rocks, you pinch yourself and wake up sweating.
      %echoaround% %actor% ~%actor% runs along the top of the city but trips and goes over the edge!
    end
  elseif (%fall_kws% ~= %cmd%) && (%non_fall_rooms% ~= %room.template%)
    * non-fall room
    if jump /= %cmd% || run /= %cmd%
      %send% %actor% There's not really room to do that in here.
    elseif trip /= %cmd%
      %send% %actor% You trip over your own two feet.
      %echoaround% %actor% ~%actor% trips over ^%actor% own two feet.
    elseif fall /= %cmd%
      %send% %actor% You can't really do that here.
    end
    return 1
    halt
  elseif %cmd% == scriptwake
  set scare_kws skulls shelves walls alcoves bones ribs femurs piles
    switch %arg%
      case skulls
        %echoaround% %actor% ~%actor% jumps backwards, bumps into the wall, and vanishes!
      break
      case bones
        %echoaround% %actor% ~%actor% falls into a pile of bones and vanishes!
      break
      case alcoves
        %echoaround% %actor% ~%actor% jumps suddenly and vanishes!
      break
      default
        * unknown
        return 0
        halt
      break
    done
    %send% %actor% You wake up in a cold sweat!
  else
    * not a wake condition
    return 0
    halt
  end
  * If we made it this far, we teleport them:
  * check stored room first
  set wake_room 0
  if %actor.varexists(skycleave_wake_room)%
    if %actor.skycleave_wake_room% > 0
      set spirit %instance.mob(11900)%
      set wake_room %actor.skycleave_wake_room%
      * account for phase change
      if %wake_room% < 11830 && %spirit.phase2%
        set wake_room 11925
      elseif %wake_room% < 11875 && %spirit.phase4%
        eval wake_room %wake_room% + 100
      end
    end
  end
  * otherwise find a room
  set leaving_adv 1
  if %wake_room%
    set target %instance.nearest_rmt(%wake_room%)%
    set leaving_adv 0
  elseif %actor.plr_flagged(ADV-SUMMON)% && %actor.adventure_summoned_from%
    set target %actor.adventure_summoned_from%
  elseif %actor.home%
    set target %actor.home%
  else
    set target %startloc%
  end
  %teleport% %actor% %target%
  if %leaving_adv%
    nop %actor.cancel_adventure_summon%
  end
  %echoaround% %actor% ~%actor% wakes up with a start! You didn't even see *%actor% there.
  * move fellows
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch.is_npc% && %ch.leader% == %actor% && !%ch.fighting% && !%ch.disabled%
      %echoaround% %ch% ~%ch% fades away and vanishes!
      %teleport% %ch% %target%
      if %leaving_adv%
        nop %ch.cancel_adventure_summon%
      end
      %send% %ch% You wake up with the hazy feeling you were just somewhere else.
      %at% %target% %echoneither% %actor% %ch% ~%ch% wakes up.
    end
    set ch %next_ch%
  done
  return 1
elseif %actor.position% == Fighting
  %send% %actor% You're a little busy right now.
  return 1
else
  * wrong position -- just fall through
  return 0
end
~
#11946
Skycleave Dreams: Reset wake on poof-in~
2 gwA 100
~
* When a player enters by any means OTHER than normal walking, reset their
* 'wake' count. Typing 'wake' 3 times exits the area using trigger 11945.
* This also clears their wake-room, which will be set by another script.
set dir_list north east south west northwest northeast southwest southeast up down
set ok_methods script login
if %actor.is_pc% && !(%ok_methods% ~= %method%) && (%direction% == none || !(%dir_list% ~= %direction%))
  set skycleave_wake 0
  remote skycleave_wake %actor.id%
  set skycleave_wake_room 0
  remote skycleave_wake_room %actor.id%
end
~
#11947
Smol Nes-Pik: Queen flirts on entry~
0 gw 100
~
* Queen flirtatiously greets the first player who enters and gives them a
* blue iris (1206). On repeat visits, she just winks.
if %actor.is_npc% || !%self.can_see(%actor%)% || %direction% != south
  halt
end
* wait a tic
wait 1
* still here?
if %actor.room% != %self.room%
  halt
end
* check var
if %actor.varexists(skycleave_queen)%
  set skycleave_queen %actor.skycleave_queen%
else
  set skycleave_queen 0
end
* do I know you?
if %skycleave_queen% > 0
  %send% %actor% ~%self% winks at you as you enter.
else
  %send% %actor% ~%self% bids you approach...
  %echoaround% %actor% ~%self% bids ~%actor% approach...
  switch %actor.sex%
    case male
      set to_actor &%self% hands you an iridescent blue iris and says, 'A token of my love for so handsome a champion.'
      set to_room &%self% hands ~%actor% an iridescent blue iris and says, 'A token of my love for so handsome a champion.'
    break
    case female
      set to_actor &%self% hands you an iridescent blue iris and says, 'A token of my love for so beautiful a hero.'
      set to_room &%self% hands ~%actor% an iridescent blue iris and says, 'A token of my love for so beautiful a hero.'
    break
    default
      set to_actor &%self% hands you an iridescent blue iris and says, 'A token of my love for such a stunning figure.'
      set to_room &%self% hands ~%actor% an iridescent blue iris and says, 'A token of my love for such a stunning figure.'
    break
  done
  %load% obj 1206 %actor% inv
  %send% %actor% %to_actor%
  %echoaround% %actor% %to_room%
  %send% %actor% &%self% whispers in your ear, 'Perhaps some day...'
  set skycleave_queen 1
  remote skycleave_queen %actor.id%
end
~
#11948
Skycleave: Pixy Queen's greeting~
0 g 100
~
* Queen gives a hidden wink if a player arrives who she met in the Dream (mob 11884)
wait 1
set ch %self.room.people%
set fail 0
while %ch%
  if %ch.varexists(skycleave_queen)%
    %send% %actor% You catch ~%self% cast a stray wink in your direction.
  elseif %ch.is_pc%
    set fail 1
  end
  set ch %ch.next_in_room%
done
if %fail%
  wait 1
  say On the oath we swore -- on the honor of the moon -- you shall proceed no further!
end
~
#11949
Walking mausoleum death knell~
2 g 100
~
if %method% == respawn
  %regionecho% %room% 50 A booming death knell echoes from %room.coords%.
  wait 1
end
~
#11950
Skycleave: Consume handler (long potions and other consumables)~
1 s 100
~
* handles long-duration skycleave potions plus other consumables
switch %self.vnum%
  case 11912
    dg_affect #%self.vnum% %actor% off silent
    dg_affect #%self.vnum% %actor% WATERWALK on 86400
    %send% %actor% # Your feet start to tingle!
  break
  case 11924
    dg_affect #%self.vnum% %actor% off silent
    dg_affect #%self.vnum% %actor% INFRA on 86400
    %send% %actor% # Everything seems so much brighter!
  break
  case 11925
    dg_affect #%self.vnum% %actor% off silent
    dg_affect #%self.vnum% %actor% INVENTORY 35 86400
    %send% %actor% # That's a weight off your shoulders.
  break
  *
  * non-potions:
  case 11884
    * poison mushroom
    if %command% == taste
      %send% %actor% You taste the little white mushroom. It tastes earthy.
      %echoaround% %actor% ~%actor% tastes a little white mushroom.
      return 0
    else
      %send% %actor% You eat the little white mushroom but you don't feel so good...
      %send% %actor% Oh no... the world goes black and the last thing you feel is your head hitting something hard.
      %echoaround% %actor% ~%actor% eats a little white mushroom...
      %slay% %actor% %actor.name% has accidentally died at %actor.room.coords%!
      return 0
      %purge% %self%
    end
  break
done
~
#11951
Queen's Nightmare: Block abilities in the jar~
2 p 100
~
%send% %actor% None of your abilities have any effect in this jar. What a nightmare!
%echoaround% %actor% ~%actor% struggles in futility to find a way out of the jar.
return 0
~
#11952
Rot and Ruin: Inside the sap: catch look and skip~
2 c 0
look skip~
if %cmd.mudcommand% == look && !%arg%
  %send% %actor% You can't see much of anything through the thick sap.
  return 1
elseif skip /= %cmd%
  %send% %actor% You skip the cutscene.
  if %actor.var(last_iskip_intro,0)% == %dailycycle%
    %send% %actor% (It may take a few seconds to finish the cutscene anyway.)
  end
  set last_iskip_intro %dailycycle%
  remote last_iskip_intro %actor.id%
else
  return 0
end
~
#11953
Rot and Ruin: Sap teleport manager (room)~
2 bgwA 100
~
* this runs both at random and on enter
if %actor%
  if %actor.is_npc%
    halt
  end
  * actor provided: see if they already have a sap
  set obj %room.contents%
  set sap 0
  while %obj% && !%sap%
    if %obj.vnum% == 11891 && %obj.val0% == %actor.id%
      set sap %obj%
    end
    set obj %obj.next_in_list%
  done
  if %sap%
    halt
  end
else
  * if no actor was provided, find a player who has no sap in the room
  set ch %room.people%
  while %ch% && !%actor%
    if %ch.is_pc%
      * find sap
      set obj %room.contents%
      set sap 0
      while %obj% && !%sap%
        if %obj.vnum% == 11891 && %obj.val0% == %ch.id%
          set sap %obj%
        end
        set obj %obj.next_in_list%
      done
    end
    if !%sap%
      set actor %ch%
    end
    set ch %ch.next_in_room%
  done
  if !%actor%
    halt
  end
end
* we now definitely have an actor with no sap
%load% obj 11891 room
set sap %room.contents%
if %sap.vnum% == 11891
  nop %sap.val0(%actor.id%)%
end
~
#11954
Rot and Ruin: Sap intro/teleport~
1 b 100
~
* fetch cycle
if %self.varexists(cycle)%
  eval cycle %self.cycle% + 1
else
  set cycle 1
end
* ensure we still have the character here
makeuid person %self.val0%
if !%person% || %person.id% != %self.val0% || %person.room% != %self.room%
  %purge% %self%
  halt
end
* check if they've seen the intro today
if %person.varexists(last_iskip_intro)%
  if %person.last_iskip_intro% == %dailycycle%
    set cycle end
  end
end
* main messages: must be sequential starting with 1; player is teleported when it hits a missing number
switch %cycle%
  case 1
    %send% %person% The sap crystallizes around you, freezing you in time as the world passes around you...
    wait 9 sec
    %send% %person% You watch in horror as rot takes the Great Tree. Bud by bud, leaf by leaf, branch by branch, it falls into ruin.
    wait 9 sec
    %send% %person% The primordial mana that once surged through the veins of the tree now flows like blood down its rotten trunk.
  break
  case 2
    %send% %person% Slowly from the haze, a great giant arises, clad in robes like the night...
    wait 9 sec
    %send% %person% The giant waves his enormous hand and speaks a few words in a language you've never heard. A gleaming, radiant axe forms in midair from the haze itself, and the giant grabs it in both hands.
    wait 9 sec
    %send% %person% With a single mighty swing of his axe, the giant cleaves the Great Tree in two, straight through you. But what you feel isn't pain, it's an unfathomable anguish. It's the shattering of thousands of years of history.
    wait 1
    %send% %person% As the tree comes crashing down, so too falls a fatal stillness. For a moment, nothing moves. The giant stands mid-swing. Shards of rotten wood hang like a cloud around the trunk.
  break
  default
    * done: teleport the character
    set target %instance.nearest_rmt(11888)%
    %teleport% %person% %target%
    dg_affect #11891 %person% off
    %force% %person% look
    %send% %person% &&0
    %send% %person% Slowly, inch by inch, time resumes again, and for the first time in ages, you almost feel like you can breathe again.
    %echoaround% %person% ~%person% emerges from the putrid sap.
    * Teleport followers
    set ch %self.room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %ch.is_npc% && %ch.leader% == %person% && !%ch.fighting% && !%ch.disabled%
        %teleport% %ch% %target%
        dg_affect #11891 %ch% off
        %force% %ch% look
        %echoaround% %ch% ~%ch% emerges from the putrid sap.
      end
      set ch %next_ch%
    done
    * store last time they player saw the intro
    set last_iskip_intro %dailycycle%
    remote last_iskip_intro %person.id%
    * and clear
    %purge% %self%
    halt
  break
done
* store current cycle
remote cycle %self.id%
~
#11955
Smol Nes-Pik: Queen Vehl Cutscene Controller~
0 bw 100
~
* configs
set num_stories 4
* fetch current story
if %self.varexists(story)%
  set story %self.story%
else
  set story 1
end
if %story% > %num_stories%
  set story 1
  remote story %self.id%
end
* ensure story is running
switch %story%
  case 1
    set trig 11956
  break
  case 2
    set trig 11957
  break
  case 3
    set trig 11958
  break
  case 4
    set trig 11959
  break
done
if !%trig%
  halt
end
if !%self.has_trigger(%trig%)%
  * attach trigger and reset
  attach %trig% %self.id%
  set line 0
  remote line %self.id%
end
~
#11956
Smol Nes-Pik: Queen Vehl Cutscene: Repairs Continue Apace~
0 ab 100
~
* runs until 'line' hits the default cause in the switch
* Queen Vehl, Story 1: Repairs Continue Apace
set mob_vnum 11876
set my_trig_vnum 11956
set no_mob_after 4
* detect line
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
remote line %self.id%
* detect mob and spawn if needed
if %line% <= %no_mob_after%
  set mob %instance.mob(%mob_vnum%)%
  if !%mob%
    %load% mob %mob_vnum%
    set mob %self.room.people%
    %echo% ~%mob% walks in from the south and ascends the steps to approach the throne.
    wait 1 sec
  elseif %mob.room% != %self.room%
    %at% %mob.room% %echoaround% %mob% ~%mob% leaves.
    %teleport% %mob% %self.room%
    %echo% %mob% ~%mob% walks in from the south and ascends the steps to approach the throne.
    wait 1 sec
  end
end
* text: this goes through sequential numbers from 1+ then ends when it hits default
switch %line%
  case 1
    %echo% ~%mob% kneels.
  break
  case 2
    say Rise, Pelet.
    wait 3 sec
    %echo% ~%mob% rises and stands on the step beneath the throne.
    wait 7 sec
    %force% %mob% say My queen, long be your custody of Tagra Nes.
    wait 8 sec
    say Thank you, young Pelet. What news? How go repairs?
  break
  case 3
    %force% %mob% say Repairs continue apace but damage to the city was extensive, as my queen knows.
    wait 9 sec
    say When can we expect completion on the lower stair?
    wait 9 sec
    %force% %mob% say Work is hindered by short labor. If my queen desires, we can shift focus to the crown column near the palace.
  break
  case 4
    say No, my urborist, our vanity in the crown does not supercede the needs of our people below. Continue your work on the lower stair.
    wait 9 sec
    %force% %mob% say The great queen is wise.
    wait 9 sec
    %echo% ~%mob% descends the stairs and leaves.
    %at% i11883 %echo% ~%mob% exits the palace and leaves in a hurry.
    %purge% %mob%
  break
  default
    wait 110 sec
    * script complete
    if %self.varexists(story)%
      eval story %self.story% + 1
    else
      set story 2
    end
    remote story %self.id%
    detach %my_trig_vnum% %self.id%
    halt
  break
done
~
#11957
Smol Nes-Pik: Queen Vehl Cutscene: The Second Front~
0 ab 100
~
* runs until 'line' hits the default cause in the switch
* Queen Vehl, Story 2: The Second Front
set mob_vnum 11875
set my_trig_vnum 11957
set no_mob_after 9
* detect line
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
remote line %self.id%
* detect mob and spawn if needed
if %line% <= %no_mob_after%
  set mob %instance.mob(%mob_vnum%)%
  if !%mob%
    %load% mob %mob_vnum%
    set mob %self.room.people%
    %echo% A messenger runs through the door before stopping, brushing *%mob%self off, and approaching the queen.
    wait 1 sec
  elseif %mob.room% != %self.room%
    %at% %mob.room% %echoaround% %mob% ~%mob% leaves.
    %teleport% %mob% %self.room%
    %echo% A messenger runs through the door before stopping, brushing *%mob%self off, and approaching the queen.
    wait 1 sec
  end
end
* text: this goes through sequential numbers from 1+ then ends when it hits default
switch %line%
  case 1
    %echo% ~%mob% salutes and stands at attention.
  break
  case 2
    say What is your name, soldier?
    wait 9 sec
    %force% %mob% say Nayyul, ma'am.
    wait 9 sec
    say What news from the front, Nayyul?
  break
  case 3
    %force% %mob% say I am sent in the name of Velory, Knight General of the Crown Wing, to give report and request aid.
    wait 9 sec
    %force% %mob% say Lady Velory has seized Glim Daro and Glim Cari. Glim Dun is burnt to ash and glowstone.
    wait 9 sec
    %force% %mob% say Lady Velory sends one hundred pallets of unshaped glimstone, to arrive on the next moon.
  break
  case 4
    %force% %mob% say The Crown Wing will hold these cities in your name but requests resupply in food and drink.
    wait 9 sec
    say You shall have it. When the glimstone arrives I shall return the pallets with supplies for the front.
    wait 9 sec
    say Does the Knight General intend to overwinter in the quarries?
  break
  case 5
    %force% %mob% say The Knight General requests permission to attack Smol Ze-Pik itself.
    wait 9 sec
    say That is quite impossible.
    wait 9 sec
    %force% %mob% say If you would be willing to listen to Lady Velory's plea, ma'am...
  break
  case 6
    say We swore the Oath.
    wait 9 sec
    %echo% ~%mob% breaks ^%mob% stiff composure as ^%mob% eyes widen.
    wait 5 sec
    %force% %mob% say They attacked us here, first, in our homes. They broke the Glimmering Stair. They killed...
  break
  case 7
    say Enough! We are not renegades. We will not attack a great tree and I will hold no further discussion on it.
    wait 9 sec
    %echo% ~%mob% stiffens up.
    wait 5 sec
    %force% %mob% say Yes, ma'am.
  break
  case 8
    say We will provide Lady Velory with all the resources she requires to pursue her campaign in the quarries.
    wait 9 sec
    %force% %mob% say Yes, ma'am.
    wait 9 sec
    say And you will remind Lady Velory of the Oath.
  break
  case 9
    %force% %mob% say Yes, ma'am.
    wait 9 sec
    say You are dismissed, Nayyul.
    wait 9 sec
    %echo% ~%mob% turns, marches down the steps, and leaves.
    %at% i11883 %echo% ~%mob% marches out of the palace and leaps off the edge of the stair!
    %purge% %mob%
  break
  default
    wait 110 sec
    * script complete
    if %self.varexists(story)%
      eval story %self.story% + 1
    else
      set story 2
    end
    remote story %self.id%
    detach %my_trig_vnum% %self.id%
    halt
  break
done
~
#11958
Smol Nes-Pik: Queen Vehl Cutscene: Queen's Remorse~
0 ab 100
~
* runs until 'line' hits the default cause in the switch
* Queen Vehl, Story 3: Queen's Remorse
set mob_vnum 11874
set my_trig_vnum 11958
set no_mob_after 13
* detect line
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
remote line %self.id%
* detect mob and spawn if needed
if %line% <= %no_mob_after%
  set mob %instance.mob(%mob_vnum%)%
  if !%mob%
    %load% mob %mob_vnum%
    set mob %self.room.people%
    %echo% An envoy walks in from the south and ascends the steps to approach the throne.
    wait 1 sec
  elseif %mob.room% != %self.room%
    %at% %mob.room% %echoaround% %mob% ~%mob% leaves.
    %teleport% %mob% %self.room%
    %echo% %mob% An envoy walks in from the south and ascends the steps to approach the throne.
    wait 1 sec
  end
end
* text: this goes through sequential numbers from 1+ then ends when it hits default
switch %line%
  case 1
    %echo% ~%mob% kneels.
  break
  case 2
    %echo% ~%self% stands and holds ^%self% hand out to ~%mob%, who touches ^%self% forehead to it.
    wait 9 sec
    %echo% ~%self% sits delicately on the throne.
    wait 9 sec
    %echo% ~%mob% stands and brushes ^%self%self off.
  break
  case 3
    say Good envoy, welcome to Smol Nes-Pik. I trust your journey was not unduly arduous.
    wait 9 sec
    %force% %mob% say Apologies, great Queen, but if we could dispense with the pleasantries, I have urgent business...
    wait 9 sec
    %echo% ~%self% turns ^%self% hand palm-up and beckons the envoy to speak.
  break
  case 4
    %force% %mob% say E garra Sparo Wale, our city is besieged by giants from across the sea.
    wait 9 sec
    %force% %mob% say Ours is the second city, and surely the next to fall if the giants are not repelled.
    wait 9 sec
    %force% %mob% say She asks the great queen to commit, at her pleasure, as much food and as many brave defenders as she can spare.
  break
  case 5
    say I have rot and ruin at my own doorstep and your queen, noble though she may be, asks me to abandon my own charge to tend hers?
    wait 9 sec
    %echo% ~%self% turns away.
    wait 6 sec
    %echo% ~%mob% turns to leave, but turns back.
    wait 3 sec
    %force% %mob% say Great Queen, we meant no offense, only, without your benevolent assistance, we cannot hope to hold the city.
  break
  case 6
    say We require every brave defender, as you call them, here.
    wait 9 sec
    %force% %mob% say Great Queen...
    wait 1 sec
    %echo% ~%mob% hangs ^%mob% head and lets out an audible breath.
    wait 9 sec
  break
  case 7
    say You may as well speak your mind, envoy. You have traveled a frightfully long way.
    wait 9 sec
    %force% %mob% say Great Queen, the giants assault our shores from great vessels carved from the death of trees.
    wait 9 sec
    %force% %mob% say They covet our lands, they covet our trees, and they covet that which was left to us and us alone by the Ebru.
  break
  case 8
    %force% %mob% say The giants will not stop at Tagra Sul. Even now, they cut at the feet of the mountains and build their own cities from what remains.
    wait 9 sec
    %force% %mob% say In the words of Queen Wale: This is the moment in which we must come together, stand together, to protect our way of life.
    wait 9 sec
    %force% %mob% say Our queen is willing to offer your city the Ges Garkole, that you may serve as its guardian in her stead, only please, please, great Queen, help us defend our homes.
  break
  case 9
    %echo% |%self% mouth hangs open, only for a second, and then &%self% composes ^%self%self.
    wait 9 sec
    say Nevertheless, we have no soldiers to spare and few resources to commit.
    wait 9 sec
    %force% %mob% say Please, great Queen, anything.
  break
  case 10
    %echo% ~%self% turns to guard, who bends down and turns his ear to her.
    wait 9 sec
    %echo% The royal guard and ~%self% exchange hushed whispers for a few moments.
    wait 9 sec
  break
  case 11
    say We hold the deepest compassion for your situation and will provide what we can.
    wait 9 sec
    %force% %mob% say Thank you, great Queen. We will be in your debt.
    wait 9 sec
    say Don't thank me too quickly. Smol Nes-Pik will be able to furnish you with one hundred pallets of glimstone, ten pallets of bleakstone, and one thousand weights of foodstuffs from our larders.
  break
  case 12
    %echo% ~%mob% pauses for a moment as ^%mob% face looks drawn, almost overcome.
    wait 9 sec
    %force% %mob% emote 'Thank you, great Queen,' is all the envoy manages to say.
    wait 9 sec
    say And I shall despatch my personal psychopomp. May she see as many giants across the great divide as there are knots in the tree.
  break
  case 13
    %force% %mob% say Thank you, great Queen.
    wait 9 sec
    %echo% ~%mob% collects ^%mob%self and leaves.
    %at% i11883 %echo% ~%mob% exits the palace, leaps into the air, and flies away on silvery wings.
    %purge% %mob%
    wait 9 sec
  break
  case 14
    %echo% ~%self% cries quietly on her throne.
    wait 20 sec
  break
  case 15
    %echo% The royal guard stands valiantly as ~%self% sobs.
  break
  default
    wait 180 sec
    * script complete
    if %self.varexists(story)%
      eval story %self.story% + 1
    else
      set story 2
    end
    remote story %self.id%
    detach %my_trig_vnum% %self.id%
    halt
  break
done
~
#11959
Smol Nes-Pik: Queen Vehl Cutscene: Under One Last Moon~
0 ab 100
~
* runs until 'line' hits the default cause in the switch
* Queen Vehl, Story 4: Under One Last Moon
set mob_vnum 11873
set my_trig_vnum 11959
set no_mob_after 14
set no_players_start 9
set no_players_end 14
* detect line
if %self.varexists(line)%
  eval line %self.line% + 1
else
  set line 1
end
remote line %self.id%
* detect mob and spawn if needed
set guard %instance.mob(11879)%
if %line% <= %no_mob_after%
  set mob %instance.mob(%mob_vnum%)%
  if !%mob%
    %load% mob %mob_vnum%
    set mob %self.room.people%
    %echo% The queen's astrologer trudges in from the south and slowly approaches the throne.
    wait 1 sec
  elseif %mob.room% != %self.room%
    %at% %mob.room% %echoaround% %mob% ~%mob% leaves.
    %teleport% %mob% %self.room%
    %echo% %mob% The queen's astrologer trudges in from the south and slowly approaches the throne.
    wait 1 sec
  end
end
* during part of this, the queen's guard shoos players out
if %line% >= %no_players_start% && %line% <= %no_players_end%
  set ch %self.room.people%
  while %ch%
    if %ch.is_pc% && %guard.can_see(%ch%)% && !%ch.aff_flagged(HIDE)%
      * oops -- kick them out and block this cycle
      %force% %guard% say The queen requires privacy in the chamber. You'll have to leave.
      eval line %line% - 1
      remote line %self.id%
      halt
    end
    set ch %ch.next_in_room%
  done
end
* text: this goes through sequential numbers from 1+ then ends when it hits default
switch %line%
  case 1
    %echo% ~%mob% kneels and bends forward to touch ^%mob% head to the obsidian step in front of *%mob%.
    wait 9 sec
    say Well, out with it. What news, Moneryl?
    wait 8 sec
    %echo% ~%mob% takes a slow, deliberate breath and sits up on ^%mob% heels.
  break
  case 2
    %force% %mob% say I fear...
    wait 9 sec
    %force% %mob% say Great Queen, do you recall the story of the final moon?
    wait 9 sec
    say What is it you fear, Moneryl?
  break
  case 3
    %force% %mob% say In the story, Erlash, astrologer to Ebsparo Viv, flies to the top of the world to consult the moon about the Snowfell Festival...
    wait 9 sec
    say I am familiar with the story, of course.
    wait 9 sec
    %force% %mob% say But instead of the festival...
  break
  case 4
    say He learns it is the final moon of Tagra Ain. Yes, yes, get to the point.
    wait 9 sec
    %echo% ~%mob% lowers ^%mob% head.
    wait 9 sec
    %force% %mob% say Great Queen, I flew to the top of the tree to consult the moon about the giants who now camp on the morning green.
  break
  case 5
    say Did you see how we might drive them off?
  break
  case 6
    say Moneryl, your queen requires an answer.
    wait 9 sec
    %force% %mob% say No answers came, Great Queen. The future, once profound and boundless...
    wait 4 sec
    %echo% |%mob% voice catches in ^%mob% throat.
    wait 5 sec
    %force% %mob% say ... holds no full moon for Tagra Nes.
  break
  case 7
    %echo% Cold silence pours down the steps of the palace as hot tears streak |%mob% face.
  break
  case 8
    say This tree has basked in the light of a hundred thousand full moons. Surely it will stand one more.
    wait 9 sec
    %force% %mob% say My queen...
    wait 9 sec
    say Clear the room.
    * cases 9-14 do not allow players
  break
  case 9
    say Have your... consultations... revealed anything that will actually be of any use?
  break
  case 10
    %force% %mob% say In the moonlight, I witnessed the shadows of giants rending the great tree with unsated jaws of black metal.
    wait 9 sec
    %force% %mob% say In the moonlight, I witnessed the shadows of giants hauling the heartwood of our ancestors on beasts the size of the sky itself.
    wait 9 sec
    %force% %mob% say In the moonlight, I witnessed the shadows of giants erecting a tree of stone as a monument to their victory.
  break
  case 11
    %force% %mob% say In the moonlight, I witnessed the flickering flame of the burning of a hundred thousand moons.
    wait 9 sec
    say Every man and woman in Smol Nes-Pik would give their life to defend this tree.
    wait 9 sec
    %force% %mob% say Without hesitation.
  break
  case 12
    say Is that to be our fate?
    wait 9 sec
    say Moneryl? Is that to be our fate?
    wait 9 sec
    %force% %mob% say My queen, I witnessed things in the moonlight that are better left unsaid.
  break
  case 13
    say Is there no council you can give? No wisdom that could save us?
    wait 9 sec
    %force% %mob% say The path that would save us is the path we cannot take, unless you mean to break the Oath.
    wait 9 sec
    say No. I am no renegade. We will do as we promised, whatever the cost. Would you undertake the journey to Smol Ara-Pik to raise the alarm?
  break
  case 14
    %force% %mob% say I would do anything for my queen, but please, do not ask this of me. I have lived each and every one of my days in the shadow of this tree.
    wait 9 sec
    say Very well, loyal Moneryl. You are dismissed. Make whatever arrangements you see fit.
    wait 9 sec
    %echo% Moneryl touches ^%mob% head to the step in front of the queen, then rises and leaves.
    %at% i11883 %echo% ~%mob% exits the palace and trudges away.
    %at% i11885 %echo% A slow, solitary pair of footsteps can be heard trudging out of the palace.
    %purge% %mob%
    * this ends the privacy phase
  break
  case 15
    say Veloryan, you will recall Malvaleda from Smol Sul-Pik. We will require her talents.
    wait 9 sec
    %force% %guard% say With apologies, perhaps my queen hasn't heard? Malvaleda has been felled by the giants.
    wait 9 sec
    say Did she die well?
  break
  case 16
    %force% %guard% say I wish I knew, my queen. Her body was not recovered.
    wait 9 sec
    say Was she able to provide Smol Sul-Pik any assistance?
    wait 9 sec
    %force% %guard% say I'm told she felled the leader of the giants, but that they were undeterred.
  break
  case 17
    say May she rest in the roots.
    wait 3 sec
    %force% %guard% say May she rest in the roots.
    wait 9 sec
    say No time remains to rally our defenses, but I will not stand by and let Smol Nes-Pik fall. We will cast what remains of our magic into the tree in hopes of buying more time. Instruct the tresydions to prepare to bind the heartwood.
    wait 6 sec
    %force% %guard% say My queen?
  break
  case 18
    say Time flows in one direction: up. It is pulled into the great tree at the roots, through the heartwood, and out the leaves and blooms. Once bound, it will be trapped in the heartwood and no time will flow.
    wait 9 sec
    %force% %guard% say The great queen is wise. But such a thing ever been attempted?
    wait 9 sec
    say In the days of Ebsparo Neffer, her tresydions bound the heartwood of Tagra Mun, which now slumbers beyond the veil of time.
  break
  case 19
    say I will don the thorny crown of the Eternal Queen and foresake death itself to buy us the time we need to fulfill our Oath and save the tree.
    wait 9 sec
    %force% %guard% say And if it should fail?
    wait 9 sec
    say Then I will bear the cost of that failure for all time. But the tree must not fall.
  break
  default
    wait 300 sec
    * script complete
    if %self.varexists(story)%
      eval story %self.story% + 1
    else
      set story 2
    end
    remote story %self.id%
    detach %my_trig_vnum% %self.id%
    halt
  break
done
~
#11960
Smol Nes-Pik: Palace echo~
2 d 0
*~
* Dew pools
%at% i11885 %echo% ~%actor% echoes down from above, '%speech%'
* Queen's chamber
if %random.3% == 3
  %at% i11889 %echo% You hear muffled talking from the audience chamber below, but can't make it out.
end
* Outside
if %random.2% == 2
  %at% i11883 %echo% You hear lively discussion echoing from inside the palace, but can't make out the words.
end
~
#11961
Smol Nes-Pik: Drink dew of Tagra Nes~
1 c 2
drink use~
* Causes a teleport if the players sleeps in their home after drinking this
if !%arg% || %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
* need to return 1 now as we'll be doing some waits
return 1
%send% %actor% You drink a little of the dew and start to feel drowsy...
%echoaround% %actor% ~%actor% drinks from a small vial.
dg_affect #11961 %actor% SLOW on 60
set teleported 0
* begin loop to wait for sleep
set count 0
while %count% < 12
  wait 5 sec
  set room %actor.room%
  set to_room 0
  * see where we're at
  if %actor.position% != Sleeping
    * awake?
    if %teleported%
      * teleported and woke up
      dg_affect #11961 %actor% off silent
      halt
    elseif (%count% // 4) == 1
      * hasn't slept yet
      %send% %actor% You feel tired.
    end
  elseif !%room.function(BEDROOM)% || !%actor.can_teleport_room% || !%actor.canuseroom_guest%
    * Sleeping but not in a bedroom they can use: just reveal a dream.
    switch %random.10%
      case 1
        %send% %actor% You dream of an incredible tree with a tiny rainbow staircase spiraling around it.
      break
      case 2
        %send% %actor% You dream of a glowing green seashell with little people inside it.
      break
      case 3
        %send% %actor% You dream of a tree rotting from the core.
      break
      case 4
        %send% %actor% You dream of a time long ago.
      break
      case 5
        %send% %actor% You dream of little people who tend the living mana of nature like a crop.
      break
      case 6
        %send% %actor% You dream of a robed man splitting lumber by hand.
      break
      case 7
        %send% %actor% You dream of tiny people fleeing in panic as a great tree falls.
      break
      case 8
        %send% %actor% You dream of tiny people cutting still tinier blocks from gemstones.
      break
      case 9
        %send% %actor% You dream of a still night, lit by the full moon.
      break
      case 10
        %send% %actor% You dream of impossible things.
      break
    done
  elseif %room.template% == 11889
    * sleeping in the Queen's Bedroom: special outcome / set a target
    set to_room %instance.nearest_rmt(11890)%
  else
    * Sleeping AND in a bedroom: set a target
    set to_room %instance.nearest_rmt(11880)%
  end
  * did we find a teleport target?
  if %to_room%
    * determine where to send them back to
    if (%room.template% >= 11800 && %room.template% <= 11874) || (%room.template% >= 11900 && %room.template% <= 11974)
      set skycleave_wake_room %room.template%
    elseif %actor.varexists(skycleave_wake_room)% && (%room.template% >= 11800 && %room.template% <= 11999)
      set skycleave_wake_room %actor.skycleave_wake_room%
    else
      set skycleave_wake_room 0
    end
    if !%actor.plr_flagged(ADV-SUMMON)% && !%room.template%
      nop %actor.mark_adventure_summoned_from%
    end
    %echoaround% %actor% ~%actor% lets out a raucous snore and then vanishes into ^%actor% dream.
    %teleport% %actor% %to_room%
    nop %actor.link_adventure_summon%
    %echoaround% %actor% ~%actor% appears out of nowhere, asleep on the floor!
    %send% %actor% You dream you're falling -- the kind where you really feel it!
    remote skycleave_wake_room %actor.id%
    set skycleave_wake 0
    remote skycleave_wake %actor.id%
    * teleport fellows
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %ch.leader% == %actor% && !%ch.fighting%
        if %ch.is_pc% && !%ch.plr_flagged(ADV-SUMMON)% && !%room.template%
          nop %ch.mark_adventure_summoned_from%
        end
        %echoaround% %ch% ~%ch% vanishes into thin air!
        %teleport% %ch% %to_room%
        if %ch.is_pc%
          nop %ch.link_adventure_summon%
          remote skycleave_wake_room %ch.id%
          remote skycleave_wake %ch.id%
        end
        %echoaround% %ch% ~%ch% appears out of nowhere!
        if %ch.position% != Sleeping
          %force% %ch% look
        end
      end
      set ch %next_ch%
    done
    set teleported 1
    * it's actually ok to continue the loop and show the dreams
  end
  * next while loop
  eval count %count% + 1
done
dg_affect #11961 %actor% off silent
~
#11962
Skycleave: Knezz's broken mirror portal~
1 c 4
enter look examine~
if (%actor.obj_target(%arg%)% != %self%)
  return 0
  halt
end
set destination_floor %random.4%
switch %destination_floor%
  case 1
    eval destination_room 11800 + %random.9% - 1
  break
  case 2
    eval destination_room 11910 + %random.17% - 1
  break
  case 3
    eval destination_room 11930 + %random.12% - 1
    * note that this can go to the magichanical lab even if closed
  break
  case 4
    eval destination_room 11860 + %random.12% - 1
  break
done
set destination %instance.nearest_rmt(%destination_room%)%
set destination_idnum %destination.vnum%
nop %self.val0(%destination_idnum%)%
return 0
~
#11963
Skycleave Dreams: Help repeat commands on dew/tear~
1 c 2
drink use~
* This trigger only fires if 11961/11965 is already running
return 0
if !%arg% || %actor.obj_target(%arg%)% != %self%
  halt
end
switch %self.vnum%
  case 11961
    %send% %actor% You can't drink the dew again so soon; it's still in your system.
    return 1
  break
  case 11965
    if %cmd.mudcommand% == use
      %send% %actor% You already used it. That's why you're so tired.
      return 1
    end
  break
done
~
#11964
Smol Nes-Pik: Adventure, time, and weather commands~
2 c 0
adventure time weather~
set indoor_list 11882 11884 11885 11889
set no_vis_list 11890 11891
set cloudless_night 11888
if %cmd.mudcommand% == adventure
  if skycleave /= %arg% || summon /= %arg.car%
    * back out to the normal adventure command
    return 0
    halt
  end
  * send fake adventure info
  %send% %actor% A Hundred Thousand Moons (inside the Tower Skycleave)
  %send% %actor% \&zby Paul Clarke
  %send% %actor% \&0    Discover the lost city of Smol Nes-Pik, meet the people who live there, and
  %send% %actor% \&zunravel their fate in this rich hidden environment. For players who enjoy an
  %send% %actor% \&zimmersive reading experience, each room is filled with extra descriptions and
  %send% %actor% \&zmany of the characters are animated with vignettes and cutscenes. And remember,
  %send% %actor% \&zyou can wake up from this dreamy memory any time.
  %send% %actor% (type 'adventure skycleave' to see the description for the main adventure)
elseif %cmd.mudcommand% == time
  if %indoor_list% ~= %room.template%
    %send% %actor% It seems to be late in the day, but it's difficult to tell the time from here.
  elseif %no_vis_list% ~= %room.template%
    %send% %actor% You can't tell what time it is from here.
  elseif %cloudless_night% ~= %room.template%
    %send% %actor% Based on the position of the full moon, it's not yet midnight.
  else
    %send% %actor% It seems to be late in the day, but you can hardly see the sun through the haze.
  end
  return 1
elseif %cmd.mudcommand% == weather
  if %no_vis_list% ~= %room.template%
    %send% %actor% There's no weather in here.
  elseif %cloudless_night% ~= %room.template%
    %send% %actor% The sky is cloudless and dark. The moon hangs above the horizon.
    %send% %actor% It is spring and the air is cool.
    %send% %actor% It's late evening.
  else
    %send% %actor% The air is hazy and the sun barely shines through.
    %send% %actor% It is spring and the air is cool.
    %send% %actor% It's late afternoon.
  end
  return 1
else
  * unknown command somehow?
  return 0
end
~
#11965
Goblin's Dream: Jade tear sleep teleporter~
1 c 2
use~
* Causes a teleport if the players sleeps in their bedroom after using this
if !%arg% || %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
* need to return 1 now as we'll be doing some waits
return 1
%send% %actor% You hold the jade tear up to your head and suddenly feel drowsy...
%echoaround% %actor% ~%actor% holds a little jade stone up to ^%actor% head for a moment.
dg_affect #11965 %actor% SLOW on 60
set teleported 0
* begin loop to wait for sleep
set count 0
while %count% < 12
  wait 5 sec
  set room %actor.room%
  set to_room 0
  * see where we're at
  if %actor.position% != Sleeping
    * awake?
    if %teleported%
      * already moved and woke
      dg_affect #11965 %actor% off silent
      halt
    elseif (%count% // 4) == 1
      %send% %actor% You feel tired.
    end
  elseif !%room.function(BEDROOM)% || %room.template% == 11975 || !%actor.can_teleport_room% || !%actor.canuseroom_guest%
    * Sleeping but not in a bedroom: just reveal a dream.
    switch %random.10%
      case 1
        %send% %actor% You dream of a gleaming white city perched upon a mountain.
      break
      case 2
        %send% %actor% You dream of sitting under an enormous standing stone.
      break
      case 3
        %send% %actor% You dream of a fighting a trio of goblins, one at a time.
      break
      case 4
        %send% %actor% You dream of a time long ago.
      break
      case 5
        %send% %actor% You dream of a dark cave with skeletons grinning at you from every shadow.
      break
      case 6
        %send% %actor% You dream of making a pilgrimage to a sacred city.
      break
      case 7
        %send% %actor% You dream of being trapped in a cage in a great tower.
      break
      case 8
        %send% %actor% You dream of stealing back a precious treasure from a wizard.
      break
      case 9
        %send% %actor% You dream of a bright, sunny summer day.
      break
      case 10
        %send% %actor% You dream of impossible things.
      break
    done
  elseif %room.template% == 11976 || %room.template% == 11978
    * commoner/guard's dream
    set to_room %instance.nearest_rmt(11993)%
    * ensure fishing net
    if %to_room%
      if !%to_room.contents(11979)%
        %at% %to_room% %load% obj 11979 room
      end
    end
  elseif %room.template% == 11977
    * priestess's dream
    set to_room %instance.nearest_rmt(11994)%
  else
    * Sleeping AND in a bedroom: intro to the Goblin's Dream
    set to_room %instance.nearest_rmt(11975)%
  end
  * did we find a teleport target?
  if %to_room%
    * determine where to send them back to
    if (%room.template% >= 11800 && %room.template% <= 11874) || (%room.template% >= 11900 && %room.template% <= 11974)
      set skycleave_wake_room %room.template%
    elseif %actor.varexists(skycleave_wake_room)% && (%room.template% >= 11800 && %room.template% <= 11999)
      set skycleave_wake_room %actor.skycleave_wake_room%
    else
      set skycleave_wake_room 0
    end
    if !%actor.plr_flagged(ADV-SUMMON)% && !%room.template%
      nop %actor.mark_adventure_summoned_from%
    end
    %echoaround% %actor% ~%actor% lets out a whistling snore and then vanishes into ^%actor% dream.
    %teleport% %actor% %to_room%
    nop %actor.link_adventure_summon%
    %echoaround% %actor% ~%actor% appears out of nowhere, asleep on the bed!
    %send% %actor% You dream you're falling -- the kind where you really feel it!
    remote skycleave_wake_room %actor.id%
    set skycleave_wake 0
    remote skycleave_wake %actor.id%
    * teleport fellows
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %ch.leader% == %actor% && !%ch.fighting%
        if %ch.is_pc% && !%ch.plr_flagged(ADV-SUMMON)% && !%room.template%
          nop %ch.mark_adventure_summoned_from%
        end
        %echoaround% %ch% ~%ch% vanishes into thin air!
        %teleport% %ch% %to_room%
        if %ch.is_pc%
          nop %ch.link_adventure_summon%
          remote skycleave_wake_room %ch.id%
          remote skycleave_wake %ch.id%
        end
        %echoaround% %ch% ~%ch% appears out of nowhere!
        if %ch.position% != Sleeping
          %force% %ch% look
        end
      end
      set ch %next_ch%
    done
    set teleported 1
    * it's actually ok to continue the loop and show the dreams
  end
  * next while loop
  eval count %count% + 1
done
dg_affect #11961 %actor% off silent
~
#11966
Skycleave: Shared get trigger (diary replacement, struggle)~
1 g 100
~
if %self.vnum% == 11890
  * the struggle-- just purge
  %send% %actor% # You can't get that.
  return 0
  %purge% %self%
  halt
end
* otherwise: Time-traveler's diary:
* gives the pages in order if not owned, or at random if owned
set diary_list 11918 11920 11919
set list_size 3
return 1
wait 0
set ch %self.carried_by%
* check owned list
set list %diary_list%
while %list%
  set vnum %list.car%
  set list %list.cdr%
  if !%actor.inventory(%vnum%)% && !%actor.on_quest(%vnum%)% && !%actor.completed_quest(%vnum%)%
    if %ch%
      %load% obj %vnum% %ch% inv
      set obj %ch.inventory(%vnum%)%
      if %obj% && %obj.vnum% == %vnum%
        nop %obj.bind(%self%)%
      end
      %purge% %self%
      halt
    end
  end
done
* just give a random one
eval pos %%random.%list_size%%%
while %pos% > 0
  set vnum %diary_list.car%
  set diary_list %diary_list.cdr%
  eval pos %pos% - 1
done
if %vnum% && %ch%
  %load% obj %vnum% %ch% inv
  set obj %ch.inventory(%vnum%)%
  if %obj% && %obj.vnum% == %vnum%
    nop %obj.bind(%self%)%
  end
  %purge% %self%
  halt
end
~
#11967
Hanging gardens visitor restring~
0 n 100
~
set vnum 11943
set female_list Maria Ana Mary Anna Elena Marie Fatima Olga Sandra Rita Xin Sri Yu Lei Hui Ying Yan Nushi
set female_size 18
set male_list Muhammed Jose Ahmed Ali John David Li Abdul Juan Jean Robert Daniel Luis Carlos James Antonio Joseph Francisco Hong Ibrahim
set male_size 20
* pick sex
if %random.2% == 1
  set list %female_list%
  set size %female_size%
  set sex female
else
  set list %male_list%
  set size %male_size%
  set sex male
end
* pick name
eval pos %%random.%size%%%
while %list% && %pos% > 0
  set name %list.car%
  set list %list.cdr%
  eval pos %pos% - 1
done
* basic strings
%mod% %self% sex %sex%
%mod% %self% keywords %name% tourist
%mod% %self% shortdesc %name%
%mod% %self% lookdesc %name% is enjoying the hanging gardens.
* random look
set heshe %self.heshe%
switch %random.8%
  case 1
    %mod% %self% append-lookdesc %heshe.cap% isn't dressed like the locals but %heshe% seems to be getting along just fine
  break
  case 2
    %mod% %self% append-lookdesc %heshe.cap% seems to be looking around at the city in wonder
  break
  case 3
    %mod% %self% append-lookdesc %heshe.cap% strikes up a conversation with everyone who passes by
  break
  case 4
    %mod% %self% append-lookdesc %heshe.cap% fans %self.himher%self
  break
  case 5
    %mod% %self% append-lookdesc %heshe.cap% eats a handful of berries
  break
  case 6
    %mod% %self% append-lookdesc %heshe.cap% sips wine from a glass
  break
  case 7
    %mod% %self% append-lookdesc %heshe.cap% smokes from a long pipe
  break
  case 8
    %mod% %self% append-lookdesc %heshe.cap% seems to count the bricks
  break
done
* random placement
switch %random.8%
  case 1
    %mod% %self% append-lookdesc as %heshe% sits under a tree.
    %mod% %self% longdesc %name% is sitting under a tree.
  break
  case 2
    %mod% %self% append-lookdesc while %heshe% sits by some flowers.
    %mod% %self% longdesc %name% is sitting by some flowers.
  break
  case 3
    %mod% %self% append-lookdesc while dangling %self.hisher% feet from a terrace.
    %mod% %self% longdesc %name% sits on the edge of one of the terraces.
  break
  case 4
    %mod% %self% append-lookdesc as %heshe% sits on a large stone.
    %mod% %self% longdesc %name% is sitting on a large stone.
  break
  case 5
    %mod% %self% append-lookdesc as %heshe% relaxes by the pond.
    %mod% %self% longdesc %name% is relaxing by the pond.
  break
  case 6
    %mod% %self% append-lookdesc as %heshe% sits in the shade.
    %mod% %self% longdesc %name% is sitting in the shade.
  break
  case 7
    %mod% %self% append-lookdesc as %heshe% strolls through the gardens.
    %mod% %self% longdesc %name% is strolling through the gardens.
  break
  case 8
    %mod% %self% append-lookdesc as %heshe% reflects on the pond.
    %mod% %self% longdesc %name% is reflecting on the pond.
  break
done
detach 11967 %self.id%
~
#11968
Gnarled old wand: By the Power of Skycleave~
1 c 1
say ' shout whisper~
return 0
set room %actor.room%
set phrase1 by the power
set phrase2 skycleave
* basic things that could prevent them speaking
set valid_pos Standing Fighting Sitting Resting
if !(%valid_pos% ~= %actor.position%)
  halt
end
if !(%arg% ~= %phrase1%) || !(%arg% ~= %phrase2%)
  halt
end
wait 1
* look for mm
set mm %room.people(11905)%
if !%mm%
  set mm %room.people(11805)%
end
* see if ok
if %room.template% >= 11875 && %room.template% <= 11899
  set safe 1
elseif %room.template% >= 11975 && %room.template% <= 11994
  set safe 1
elseif %room.template% == 11973
  set safe 1
else
  set safe 0
end
* messaging/action
if %safe%
  dg_affect #11883 %actor% off silent
  %send% %actor% You hold up the gnarled old wand as a bolt of lightning from out of nowhere strikes it with a loud CRACK!
  %echoaround% %actor% ~%actor% holds up ^%actor% gnarled old wand... a bolt of lightning out of nowhere strikes it with a loud CRACK!
  eval amount %actor.level% / 15
  dg_affect #11883 %actor% MANA-REGEN %amount% 60
elseif %actor.is_immortal%
  %echo% A bolt of lightning comes out of nowhere and strikes |%actor% wand!
elseif %mm%
  %echo% A bolt of lightning streaks out of nowhere...
  %echo% ... ~%mm% whips a gnarled old wand out of her sleeve and catches the lightning!
  wait 1
  %force% %mm% say No!
  wait 1
  if %actor.room% != %mm.room%
    halt
  end
  wait 1
   %force% %mm% say Expeliarmus!
  %send% %actor% Before you can even blink, the wand flips out of your hand!
  %echoaround% %actor% |%actor% wand flips out of ^%actor% hand!
  wait 1
  %echo% ~%mm% catches the wand and puts it away.
  %purge% %self%
else
  %echo% A bolt of lightning from nowhere strikes ~%actor% right in the chest!
  %slay% %actor% %actor.name% has died of hubris at %actor.room.coords%!
end
~
#11969
Elemental Plane of Water: Hendecagon fountain summoned NPC run~
0 ab 100
~
wait 3 sec
* Runs down the tower and into the Elemental Plane of Water
set down_rooms 11971 11960 11930 11910
set ne_rooms 11970 11934 11922 11904
set se_rooms 11963 11933 11913 11903
set sw_rooms 11962 11932 11912 11902
set nw_rooms 11961 11931 11911
set template %self.room.template%
if %template% == 11908
  * final location
  switch %self.vnum%
    case 11854
      %echo% ~%self% runs toward the fountain, trips, and rolls in!
    break
    case 11855
      %echo% ~%self% flows its long liquid body into the fountain!
    break
    case 11856
      %echo% ~%self% swoops upward and then dives into the fountain!
    break
    case 11857
      %echo% ~%self% bounds straight into the fountain!
    break
    case 11858
      %echo% ~%self% leaps into the air and lands in the fountain with a splash!
    break
  done
  %echo% You watch as it joins with the fountain's water and vanishes.
  * process followers
  makeuid water room i11972
  set ch %self.room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %ch.leader%
      if %ch.leader% == %self% || %ch.leader.leader% == %self%
        %send% %ch% You follow ~%self% into the fountain!
        %echoaround% %ch% ~%ch% follows ~%self% into the fountain!
        %teleport% %ch% %water%
        %load% obj 11805 %ch%
      end
    end
    set ch %next_ch%
  done
  %purge% %self%
elseif %template% == 11901
  north
elseif %down_rooms% ~= %template%
  down
elseif %ne_rooms% ~= %template%
  northeast
elseif %se_rooms% ~= %template%
  southeast
elseif %sw_rooms% ~= %template%
  southwest
elseif %nw_rooms% ~= %template%
  northwest
else
  * unknown room?
  %echo% ~%self% splashes to the ground and soaks in.
  %purge% %self%
end
~
#11970
Goblin's Dream: Arena challenge spawner~
2 bw 90
~
set mob_list 11956 11957 11958
set first_mob 11956
* skip it if delay obj is present
if %room.contents(11973)%
  halt
end
while %mob_list%
  set vnum %mob_list.car%
  set mob_list %mob_list.cdr%
  set mob %instance.mob(%vnum%)%
  if %mob%
    * make sure it's here...
    if %mob.room% != %room%
      %at% %mob.room% %echo% ~%mob% heads to the cliffside.
      %teleport% %mob% %room%
      %echo% ~%mob% dusts *%mob%self off and enters the arena.
    end
    * found any of them... just exit
    halt
  end
done
* if we got this far, must start the challenge
%load% mob %first_mob%
set mob %room.people%
if %mob.vnum% != %first_mob%
  %echo% Script error loading mob %first_mob%.
  halt
end
%echo% ~%mob% dusts *%mob%self off and enters the arena.
~
#11971
Skycleave: Burn the heartwood of time (rescue the tower)~
1 c 4
burn light~
* some mobs block burning
set mob_list 11871 11866 11872
if !%arg.car% || %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
* check mobs that prevent burning
while %mob_list%
  set vnum %mob_list.car%
  set mob_list %mob_list.cdr%
  set mob %instance.mob(%vnum%)%
  if %mob%
    %send% %actor% Every time you approach the heartwood, you find yourself back where you were a few moments ago. You can't get to it while ~%mob% is channeling it!
    halt
  end
done
* ok go
%send% %actor% You burn %self.shortdesc%!
%echoaround% %actor% ~%actor% burns %self.shortdesc%!
* mark who did this
set spirit %instance.mob(11900)%
set finish4 %actor.name%
remote finish4 %spirit.id%
* and phase transition
%load% mob 11898
%load% mob 11895
%purge% %self%
~
#11972
Goblin's Dream: Arena must-fight timer~
1 f 0
~
* when this item expires, it resets the fight if nobody is fighting
set mob_list 11956 11957 11958
set room %self.room%
while %mob_list% && !%mob%
  set vnum %mob_list.car%
  set mob_list %mob_list.cdr%
  set mob %instance.mob(%vnum%)%
done
if %mob%
  set ok 0
  if %mob.fighting%
    set ok 1
  else
    set ch %room.people%
    while %ch% && !%ok%
      if %ch.fighting% == %mob%
        set ok 1
      end
      set ch %ch.next_in_room%
    done
  end
  if %ok%
    * someone is fighting the mob: reload self and reset the timer
    %load% obj %self.vnum%
  else
    * we have a mob but nobody is fighting it
    %echo% ~%mob% steps out of the arena and goes to mingle in the crowd.
    %purge% %mob%
  end
end
return 0
%purge% %self%
~
#11973
Goblin's Dream: Arena challenge death~
0 f 100
~
* This both spawns the next mob and manages the !LOOT flag based on unique fighters
set room %self.room%
set min_level 150
set must_fight_timer_obj 11972
set delay_obj 11973
* we will return 0 no matter what
return 0
* strings/setup by vnum
switch %self.vnum%
  case 11956
    * Elver
    %echo% &&j~%self% trudges away to tend to ^%self% wounds.&&0
    set next_vnum 11957
    set pos 1
  break
  case 11957
    * Nailbokh
    %echo% &&j~%self% congratulates you and steps out of the arena.&&0
    set next_vnum 11958
    set pos 2
  break
  case 11958
    * Biksi
    %echo% &&j~%self% cheers for you and exits the arena.&&0
    set next_vnum 0
    set pos 3
  break
done
* remove any delay obj here
set delay %room.contents(%must_fight_timer_obj%)%
if %delay%
  %purge% %delay%
end
* load next mob or delay obj
if %next_vnum%
  %load% mob %next_vnum%
  set mob %self.room.people%
  if %mob.vnum% == %next_vnum%
    set diff %self.var(diff,1)%
    remote diff %mob.id%
    * setup flags
    if %self.mob_flagged(HARD)%
      nop %mob.add_mob_flag(HARD)%
    end
    if %self.mob_flagged(GROUP)%
      nop %mob.add_mob_flag(GROUP)%
    end
    %scale% %mob% %self.level%
    * message
    %echo% &&j~%mob% steps into the arena!&&0
  end
  * and load a timer obj to ensure promptness
  %load% obj %must_fight_timer_obj% room
else
  * no next mob
  %load% obj %delay_obj% room
end
* ensure a player has loot permission
if %actor.is_pc% && %actor.level% >= %min_level% && %self.is_tagged_by(%actor%)%
  set varname pc%actor.id%
  if %room.var(%varname%,0)% < %pos%
    * actor qualifies
    set %varname% %pos%
    remote %varname% %room.id%
    nop %self.remove_mob_flag(!LOOT)%
    %echo% The crowd cheers in unison and some of the goblins throw items into the arena!
    halt
  end
end
* actor didn't qualify -- find anyone present who does
set ch %room.people%
while %ch%
  if %ch.is_pc% && %ch.level% >= %min_level% && %self.is_tagged_by(%ch%)%
    set varname pc%ch.id%
    if %room.var(%varname%,0)% < %pos%
      * ch qualifies
      set %varname% %pos%
      remote %varname% %room.id%
      nop %self.remove_mob_flag(!LOOT)%
      %echo% The crowd cheers in unison and some of the goblins throw items into the arena!
      halt
    end
  end
  set ch %ch.next_in_room%
done
* If we got here, nobody qualified for loot
%echo% The crowd cheers in unison!
~
#11974
Skycleave: Hendecagon fountain animation~
1 b 15
~
set room %self.room%
if %room.template% == 11971 && %random.4% == 4
  * phase 4B summon version: mobs 11854, 11855, 11856, 11857, 11858
  eval vnum 11853 + %random.5%
  %load% mob %vnum%
  set mob %room.people%
  %echo% ~%mob% springs from the fountain!
elseif %room.people%
  * echo-only version
  if %room.people.fighting%
    halt
  end
  switch %random.6%
    case 1
      %echo% A horse made of crystal clear water leaps up from the hendecagon fountain and then splashes back down and vanishes into the water.
    break
    case 2
      %echo% A pair of lions made from pure water leap up from the hendecagon fountain, chase each other, then vanish back into the water.
    break
    case 3
      %echo% A flock of birds made of water fly up out of the hendecagon fountain, circle the tower, then dive back into the water.
    break
    case 4
      %echo% The hendecagon fountain releases a cloud of bubbles into the air. You watch as they float away on the wind.
    break
    case 5
      %echo% You watch little waves chase each other playfully in the hendecagon fountain.
    break
    case 6
      %echo% The water dances and twirls across the surface of the hendecagon fountain.
    break
  done
end
~
#11975
Goblin's Dream: Main entrance dream telepoter~
2 bw 100
~
* Sleeping players and their NPC followers teleport to Gobbrabakh of Orka
set to_room %instance.nearest_rmt(11975)%
if !%to_room%
  * Error?
  halt
end
set skycleave_wake_room %room.template%
set skycleave_wake 0
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.is_pc% && %ch.position% == Sleeping%
    * Teleport player first
    %echoaround% %ch% ~%ch% lets out a whistling snore and then vanishes into ^%ch% dream.
    %teleport% %ch% %to_room%
    %echoaround% %ch% ~%ch% appears out of nowhere, asleep on the bed!
    %send% %ch% You dream you're falling -- the kind where you really feel it!
    remote skycleave_wake_room %ch.id%
    remote skycleave_wake %ch.id%
    * bring followers
    set fol %room.people%
    while %fol%
      set next_fol %fol.next_in_room%
      if %fol.is_npc% && %fol.leader% == %ch% && !%fol.fighting%
        if %fol% == %next_ch%
          set next_ch %next_ch.next_in_room%
        end
        %echoaround% %fol% ~%fol% vanishes into thin air!
        %teleport% %fol% %to_room%
        %echoaround% %fol% ~%fol% appears out of nowhere!
        if %fol.position% != Sleeping
          %force% %fol% look
        end
      end
      set fol %next_fol%
    done
  end
  set ch %next_ch%
done
~
#11976
Goblin's Dream: Adventure, time, and weather commands~
2 c 0
adventure time weather~
set indoor_list 11975 11976 11977 11978 11979 11980 11981
if %cmd.mudcommand% == adventure
  if skycleave /= %arg% || summon /= %arg.car%
    * back out to the normal adventure command
    return 0
    halt
  end
  * send fake adventure info
  %send% %actor% Zenith of the Gobbrabakhs (inside the Tower Skycleave)
  %send% %actor% \&zby Paul Clarke
  %send% %actor% \&0   Venture back into the Goblin's Dream to experience a culture lost to history
  %send% %actor% \&zin this lively hidden area. For players who enjoy an immersive reading
  %send% %actor% \&zexperience, each room is filled with extra descriptions and many of the
  %send% %actor% \&zcharacters are animated with vignettes and cutscenes. Whether it's a short
  %send% %actor% \&zdream or a long one, you can wake up any time you want.
  %send% %actor% (type 'adventure skycleave' to see the description for the main adventure)
elseif %cmd.mudcommand% == time
  if %room.template% == 11981
    %send% %actor% It's hard to tell what time it is with so little light making it down here.
  elseif %indoor_list% ~= %room.template%
    %send% %actor% It's roughly noon, with the sun streaming in through the roof.
  else
    %send% %actor% It's roughly noon, with the sun high in the sky.
  end
  return 1
elseif %cmd.mudcommand% == weather
  if %room.template% == 11981
    %send% %actor% It's hard to tell the weather from in here.
  else if %indoor_list% ~= %room.template%
    %send% %actor% It's hard to tell the weather from in here, but a lot of sun is coming from above.
  else
    %send% %actor% The sky is cloudless and sunny.
    %send% %actor% It's summer time and you're quite warm.
    %send% %actor% It's midday.
  end
  return 1
else
  * unknown command somehow?
  return 0
end
~
#11977
Elemental Plane of Water: Enter resets breath timer~
2 gA 100
~
if !%actor.is_pc%
  halt
end
set mob %room.people(11928)%
if %mob%
  if %mob.varexists(scaled)%
    * already scaled: set entry time
    set enter_%actor.id% %timestamp%
    remote enter_%actor.id% %room.id%
    halt
  end
end
* if we made it hear, clear them
set varname enter_%actor.id%
if %room.varexists(%varname%)%
  rdelete %varname% %room.id%
end
~
#11978
Goblin's Dream: Altar pilgrim behavior~
0 ab 100
~
* This runs 100% of the time on mob 11979 (pilgrim) who is loaded by trig 11979
* fetch sequence number: runs every 13sec until it hits a 'default' below
if %self.varexists(seq)%
  eval seq %self.seq% + 1
else
  set seq 1
end
remote seq %self.id%
switch %seq%
  case 1
    wait 4 sec
    if %random.2% == 1
      %echo% ~%self% kneels before the altar.
    else
      %echo% ~%self% kneels in front of the altar.
    end
  break
  case 2
    if %random.2% == 1
      %echo% ~%self% leans forward and presses ^%self% head to the altar.
    else
      %echo% ~%self% bows low to the ground.
    end
  break
  case 3
    if %self.mob_flagged(*PICKPOCKETED)%
      %echo% ~%self% reaches into ^%self% pocket and makes a perplexed look.
      set lost_pearl 1
      remote lost_pearl %self.id%
    else
      %echo% ~%self% sits up and pulls a pearl from ^%self% pocket.
    end
    * disable pickpocket now if not already
    nop %self.add_mob_flag(*PICKPOCKETED)%
  break
  case 4
    if %self.varexists(lost_pearl)%
      %echo% ~%self% pats ^%self% pocket trying to find something.
    elseif %random.2% == 1
      %echo% ~%self% sets the pearl on the altar.
    else
      %echo% ~%self% gently places the pearl on the altar.
    end
  break
  case 5
    * skip; long delay here if the player stole the pearl
    if %self.varexists(lost_pearl)%
      wait 360 s
    end
  break
  case 6
    %echo% ~%self% stands up and brushes *%self%self off.
  break
  default
    * done
    %echo% ~%self% climbs up the ladder and leaves.
    %at% i11991 %echo% %self.name% climbs up from below the altar and walks west.
    %purge% %self%
  break
done
~
#11979
Goblin's Dream: Altar below pilgrim loader~
2 bw 33
~
* This script randomly loads a pilgrim into the room, if there isn't one yet.
* The pilgrim will take care of its own actions and cleanup.
* Name list should alternate male / female, starting with male.
set vnum 11979
set name_list Gron Frelliks Kazelen Nailba Rikh Maina Chon Kyulia Gliam Olyva Ohan Gremma Jaden Gomelia Blacas Iva Khenry Ima Khlevi Galeanor Gurkhan Gora Giyatt Azel Khon Nilbag Kaz Filkas Flinkh Quarrelyne
set name_count 30
* check existing mob
set mob %instance.mob(%vnum%)%
if %mob%
  halt
end
* load mob
%load% mob %vnum%
set mob %self.people%
if %mob.vnum% != %vnum%
  * error?
  halt
end
* fetch sequence number
if %self.varexists(seq)%
  eval seq %self.seq% + 1
else
  set seq 1
end
* check bounds
if %seq% > %name_count%
  set seq 1
end
remote seq %self.id%
* determine strings
while %name_list% && %seq% > 0
  set name %name_list.car%
  set name_list %name_list.cdr%
  eval seq %seq% - 1
done
* set strings
%mod% %mob% keywords %name% goblin
%mod% %mob% shortdesc %name%
%mod% %mob% longdesc %name% is kneeling in front of the altar.
eval sex %self.seq% // 2
switch %sex%
  case 0
    %mod% %mob% sex female
    %mod% %mob% lookdesc %name% seems thoughtful as she kneels and wipes her green forehead with the matching sleeve of her long gown.
  break
  case 1
    %mod% %mob% sex male
    %mod% %mob% lookdesc %name% has a solemn look as he kneels in front of the altar with his long green robe, nearly the same shade as his skin, splayed out around him on the rocky floor.
  break
done
* announce
%at% i11991 %echo% %name% walks in from the west and descends the ladder.
%echo% %name% climbs down from above and approaches the altar.
~
#11980
Goblin's Dream: Zenith passage every 30 minutes~
2 b 30
~
* once per 30 minutes
set start_v 11982
set end_v 11992
set vnum %start_v%
while %vnum% <= %end_v%
  %at% i%vnum% %echo% You watch as the sun passes directly overhead -- today is the zenith passage!
  eval vnum %vnum% + 1
done
%echo% The vertical tube in the ceiling shows a perfect circle on the altar below as the sun passes directly overhead.
wait 1787 sec
~
#11981
Skycleave: Handy mob restring command~
0 c 0
restring~
* uses a self-only command trig
if %actor% != %self%
  return 0
  halt
else
  return 1
end
set old_name %self.name%
switch %arg.car%
  case comedian
    %mod% %self% sex female
    %mod% %self% keywords Shoan goblin
    %mod% %self% shortdesc Shoan
    %mod% %self% longdesc Shoan is standing in front of the crowd.
    %mod% %self% lookdesc Shoan's pale jade skin shimmers with sweat as she stands before the crowd in the central plaza. She's dressed in an airy purple dress and her dary gray hair
    %mod% %self% append-lookdesc is cut very short. She doesn't smile as she speaks in a dry, steady voice. But the crowd seems to enjoy it.
  break
  case poet
    %mod% %self% sex male
    %mod% %self% keywords Choona goblin
    %mod% %self% shortdesc Choona
    %mod% %self% longdesc Choona stands alone in front of the crowd.
    %mod% %self% lookdesc Though he's not tall, Choona's posture is rigidly upright and he stands over many of the other goblins. He's clad in a clean white tunic and violet robe,
    %mod% %self% append-lookdesc with a kelp wreath in his brown hair. His skin is the shade of spring grass, with rosy red powder on his cheeks.
  break
  case priest
    %mod% %self% sex female
    %mod% %self% keywords Celles goblin
    %mod% %self% shortdesc Celles
    %mod% %self% longdesc Celles, Voice of Orka, stands in front of the crowd.
    %mod% %self% lookdesc With long, straight, translucent white hair that flows over her deep purple robe, Celles looks to be the oldest goblin in the city. Her skin, perhaps
    %mod% %self% append-lookdesc chartreuse once, has faded almost to gray. Her eyes are milky white to match her hair, and she stares off toward the horizon as she speaks.
  break
  case janitor
    %mod% %self% sex male
    %mod% %self% keywords Rask goblin
    %mod% %self% shortdesc Rask
    %mod% %self% longdesc Rask is sweeping the plaza.
    %mod% %self% lookdesc Rask is clad in short purple trousers and a sleeveless white shirt that show off his ashy green arms and legs. He sweeps the plaza with an old broom that badly needs new bristles.
  break
done
* announce change
if %old_name% != %self.name%
  switch %self.room.template%
    case 11982
      %echo% %old_name% disappears into the crowd and ~%self% steps down into the center of the plaza.
    break
    default
      %echo% %old_name% leaves.
      %echo ~%self% arrives.
    break
  done
break
~
#11982
Elver the Worthy combat: Hammer Dance, Ring Your Bell, Prayer of Thunder, Hammer Throw~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
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
skyfight lockout 30 35
if %move% == 1
  * Hammer Dance
  skyfight clear dodge
  %subecho% %room% &&y~%self% shouts, 'Time for the hammer dance!'&&0
  %echo% &&j~%self% starts singing and dancing around wildly with his hammers out...&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 3 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %echo% &&j**** ~%self% sings as he dances toward you with his hammers swinging! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          set hit 1
          %echo% &&j|%self% hammers hit ~%ch% as he dances wildly around the arena!&&0
          if %diff% >= 2
            %send% %ch% That really hurt! Your leg is immobilized.
            dg_affect #11956 %ch% off silent
            dg_affect #11956 %ch% IMMOBILIZED on 10
          end
          %damage% %ch% 100 physical
        elseif %ch.is_pc%
          %send% %ch% &&jYou narrowly avoid the hammer dance!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&j**** He's whirling back around again and bellowing in a deep voice... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  if !%hit%
    if %diff% < 3
      %echo% &&j~%self% spins himself out doing the hammer dance.&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
elseif %move% == 2 && !%self.aff_flagged(BLIND)%
  * Ring Your Bell
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  eval dodge %diff% * 20
  dg_affect #11955 %self% DODGE %dodge% 10
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %subecho% %room% &&y~%self% shouts, 'I'm going to ring your bell!'&&0
  %send% %targ% &&j**** &&Z~%self% leaps high into the air! ****&&0 (dodge)
  %echoaround% %targ% &&j~%self% leaps high into the air!&&0
  skyfight setup dodge %targ%
  wait 8 s
  dg_affect #11955 %self% off
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
    %echo% &&j~%self% lands with a little roll.&&0
  elseif %targ.var(did_sfdodge)%
    * miss
    %echo% &&j~%self% goes crashing into the arena sand as he misses ~%targ%!&&0
    if %diff% < 3
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
    if %diff% == 1
      %damage% %self% 10 physical
      dg_affect #11856 %targ% TO-HIT 25 20
    end
  else
    * hit
    %echo% &&j~%self% lands on |%targ% chest and clangs both hammers together on ^%targ% head, shouting the whole time!&&0
    if %diff% >= 3 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
      %send% %targ% &&jYou're seeing stars!&&0
      dg_affect #11851 %targ% STUNNED on 15
    end
    %damage% %targ% 150 physical
  end
  skyfight clear dodge
elseif %move% == 3
  * Prayer of Thunder
  skyfight clear interrupt
  %echo% &&j**** &&Z~%self% takes a brief respite to pray... ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup interrupt all
  if %self.diff% < 3 || %room.players_present% < 2
    set requires 1
  else
    set requires 2
  end
  wait 4 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if %self.var(sfinterrupt_count,0)% < %requires%
    %echo% &&j**** &&Z~%self% prays toward the standing stone... ****&&0 (interrupt)
  end
  wait 4 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if %self.var(sfinterrupt_count,0)% >= %requires%
    %echo% &&j~%self% is interrupted during the prayer... he looks furious!&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
    wait 30 s
  else
    %echo% &&j~%self% finishes his prayer and holds his hammers to the sky... there is a tremendous crack of thunder and the hammer starts to glow!&&0
    eval amount %diff% * 15
    dg_affect #11954 %self% BONUS-PHYSICAL %amount% 300
  end
  skyfight clear interrupt
elseif %move% == 4 && !%self.aff_flagged(BLIND)%
  * Hammer Throw
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %send% %targ% &&j**** &&Z~%self% goes to throw his hammers... and he's aiming at you! ****&&0 (dodge)
  %echoaround% %targ% &&j~%self% goes to throw his hammers...&&0
  skyfight setup dodge %targ%
  wait 8 s
  if %self.disabled% || %self.aff_flagged(DISARMED)%
    halt
  end
  dg_affect #11953 %self% DISARMED on 30
  %subecho% %room% &&y~%self% shouts, 'Hammer throw!'&&0
  if !%targ% || %targ.id% != %id%
    * gone
    dg_affect #11953 %self% off silent
  elseif %targ.var(did_sfdodge)%
    * miss
    %echo% &&j|%self% hammers plunk into the sand as he misses ~%targ%!&&0
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
  else
    * hit
    %send% %targ% &&j|%self% hammers soar through the air and hit |%targ% head one after the other!&&0
    if %diff% >= 3 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
      %send% %targ% &&jYou're seeing stars!&&0
      dg_affect #11851 %targ% STUNNED on 15
    elseif %diff% >= 2
      %send% %targ% &&jYou're seeing stars!&&0
      dg_affect #11952 %targ% IMMOBILIZED on 30
    end
    %damage% %targ% 150 physical
    wait 10 2 s
    dg_affect #11953 %self% off
  end
  skyfight clear dodge
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11983
Iskip combat: Jar of Captivity, Lightning Torrent, Buff Blitz, Radiant Axe~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
dg_affect #3021 %self% SOULMASK on 15
set m_l %self.var(m_l)%
set n_m %self.var(n_m,0)%
if !%m_l% || !%n_m%
  set m_l 1 2 3 4
  set n_m 4
end
eval which %%random.%n_m%%%
set old %m_l%
set m_l
set move 0
while %which% > 0
  set move %old.car%
  if %which% != 1
    set m_l %m_l% %move%
  end
  set old %old.cdr%
  eval which %which% - 1
done
set m_l %m_l% %old%
eval n_m %n_m% - 1
remote m_l %self.id%
remote n_m %self.id%
skyfight lockout 30 35
if %move% == 1 && !%self.aff_flagged(BLIND)%
  * Jar of Captivity
  skyfight clear free
  skyfight clear struggle
  %echo% &&mThe Iskip pulls out an enormous clay jar and swoops down toward you...&&0
  wait 3 s
  if %self.disabled%
    halt
  end
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  if %self.fighting% == %targ% && %diff% < 4
    dg_affect #11852 %self% HARD-STUNNED on 20
  end
  if %diff% <= 2 || (%self.level% + 100) <= %targ.level% || %room.players_present% == 1
    %send% %targ% &&m**** The jar comes down on your head! You have to break free! ****&&0 (struggle)
    %echoaround% %targ% &&mThe jar comes down on ~%targ%, trapping *%targ%!&&0
    skyfight setup struggle %targ% 20
    set bug %targ.inventory(11890)%
    if %bug%
      set strug_char You try to break out of the jar...
      set strug_room You hear ~%%actor%% trying to break out of the jar...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You break out of the jar!
      set free_room ~%%actor%% manages to break out of the jar!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    wait 20 s
    skyfight clear struggle
  else
    %send% %targ% &&mThe jar comes down on your head! There's nothing you can do!&&0
    %echoaround% %targ% &&m**** The jar comes down on ~%targ%, trapping *%targ%! ****&&0 (free %targ.pc_name.car%)
    skyfight setup free %targ%
    eval time %diff% * 15
    dg_affect #11888 %targ% HARD-STUNNED on %time%
    * wait and clear
    set done 0
    while !%done% && %time% > 0
      wait 5 s
      eval time %time% - 5
      if %targ_id% != %targ.id%
        set done 1
      elseif !%targ.affect(11888)%
        set done 1
      end
    done
    skyfight clear free
  end
  dg_affect #11852 %self% off
elseif %move% == 2
  * Lightning Torrent
  %echo% &&mThe Iskip holds his hands out to the side as sparks crackle around his fingers...&&0
  %echo% &&m**** He seems to be drawing lighting up from the water! ****&&0 (interrupt and dodge)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  skyfight clear interrupt
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  while !%broke% && %cycle% <= 4
    skyfight setup dodge all
    wait 4 s
    if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou somehow manage to interrupt the Iskip before another lightning torrent!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&mThe Iskip seems distracted, if only for a moment.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
        wait 10 s
      elseif %diff% < 4
        dg_affect #11852 %self% HARD-STUNNED on 5
        wait 5 s
      end
    else
      %echo% &&mThe Iskip's fingers crackle as a torrent of lightning bolts stream up from the water...&&0
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if %ch.var(did_sfdodge)%
            %send% %ch% &&mYou manage to narrowly dodge the lightning bolts!&&0
            if %diff% == 1
              dg_affect #11856 %ch% off
              dg_affect #11856 %ch% TO-HIT 25 20
            end
          else
            if %ch.trigger_counterspell%
              %send% %ch% Your counterspell does nothing against the Iskip!
            end
            * hit
            %send% %ch% &&mA lightning bolt strikes you right in the chest!&&0
            %echoaround% %ch% &&m~%ch% screams as a lightning bolt strikes *%ch%!&&0
            %damage% %ch% 120 physical
            if %cycle% == 4 && %diff% == 4 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
              dg_affect #11851 %ch% STUNNED on 10
            end
          end
        end
        set ch %next_ch%
      done
    end
    if !%broke% && %cycle% < 4
      %echo% &&m**** Here comes another lightning torrent... ****&&0 (interrupt and dodge)
    end
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 3
  * Buff Blitz
  %echo% &&mThe Iskip raises his hands to the sky and begins chanting something you don't understand...&&0
  %echo% &&m**** He seems to be casting spells... on himself! ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  set cycle 1
  set needed %room.players_present%
  if %needed% > 4
    set needed 4
  end
  set which 0
  while %cycle% <= 4
    skyfight clear interrupt
    skyfight setup interrupt all
    wait 5 s
    if %self.sfinterrupt_count% >= %needed%
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou somehow manage to interrupt the Iskip's spells!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&mThe Iskip seems distracted, if only for a moment.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
      elseif %diff% < 4
        dg_affect #11852 %self% HARD-STUNNED on 5
      end
    else
      * 4 buffs in order:
      eval which %which% + 1
      if %which% == 1
        %echo% &&mRays of moonlight bathe the Iskip from above... he seems stronger!&&0
        eval amt %diff% * 5
        dg_affect #11889 %self% BONUS-MAGICAL %amt% -1
      elseif %which% == 2
        %echo% &&mA rush of water swirls up around the Iskip from the sea... he's gone to be hard to hit!&&0
        eval amt %diff% * 10
        dg_affect #11890 %self% DODGE %amt% -1
      elseif %which% == 3
        %echo% &&mA lightning bolt strikes the Iskip from out of the blue... he seems faster!&&0
        dg_affect #11892 %self% HASTE on 30
      elseif %which% == 4
        %echo% &&mMana streams out of the enormous stump and into the Iskip, healing him!&&0
        %heal% %self% health
      end
    end
    if %cycle% < 4
      %echo% &&m**** He's casting another one... ****&&0 (interrupt)
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 4
  * Radiant Axe
  skyfight clear dodge
  %echo% &&mThe Iskip speaks a few words in a language you don't understand...&&0
  wait 3 s
  %echo% &&m**** A gleaming, radiant axe forms from the haze itself and the giant grabs it! ****&&0 (dodge)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 8 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  set hit 0
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)%
      if !%ch.var(did_sfdodge)%
        set hit 1
        %echo% &&mThe giant's radiant axe slices through ~%ch%!&&0
        %damage% %ch% 200 physical
      elseif %ch.is_pc%
        %send% %ch% &&mYou narrowly avoid the giant's radiant axe!&&0
        if %diff% == 1
          dg_affect #11856 %ch% TO-HIT 25 20
        end
      end
    end
    set ch %next_ch%
  done
  skyfight clear dodge
  if !%hit%
    if %diff% < 3
      %echo% &&mThe Iskip stumbles as his radiant axe misses!&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11984
First Water combat: Frozen Solid, Cavitation Cascade, Lightning Wave, Under Pressure~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
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
skyfight lockout 30 35
if %move% == 1
  * Frozen Solid
  skyfight clear struggle
  %echo% &&AThe water around you is suddenly bitter cold... and getting colder by the second!&&0
  if %diff% <= 3
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  wait 3 s
  %echo% &&A**** You are frozen solid as the water around you turns to ice! ****&&0 (struggle)
  skyfight setup struggle all 20
  set ch %room.people%
  while %ch%
    set bug %ch.inventory(11890)%
    if %bug%
      set strug_char You struggle to break free of the ice...
      set strug_room ~%%actor%% struggles to break free...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You manage to get out of the ice!
      set free_room ~%%actor%% manages to get out of the ice!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    set ch %ch.next_in_room%
  done
  * messages
  set cycle 0
  while %cycle% < 4
    wait 5 s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %ch.affect(11822)%
        if %cycle% == 3
          * final
          %send% %ch% &&AYou're unfrozen but desperately need air...&&0
          if %diff% >= 3
            eval enter_%ch.id% %room.var(enter_%ch.id%,%timestamp%)% - (%diff% * 5)
            remote enter_%ch.id% %room.id%
          end
          dg_affect #11822 %ch% off silent
        else
          %send% %ch% &&A**** You are still frozen solid! ****&&0 (struggle)
        end
      end
      set ch %next_ch%
    done
    if %cycle% >= (4 - %diff%)
      nop %self.remove_mob_flag(NO-ATTACK)%
    end
    eval cycle %cycle% + 1
  done
  nop %self.remove_mob_flag(NO-ATTACK)%
  skyfight clear struggle
elseif %move% == 2
  * Cavitation Cascade
  skyfight clear dodge
  %echo% &&AThe water calms for the briefest moment before large bubbles start to appear around you...&&0
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 3 s
  %echo% &&A**** Suddenly the bubbles around you begin to implode! ****&&0 (dodge)
  set cycle 1
  eval wait 12 - %diff%
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          %echo% &&AThere's a blinding flash as a bubble implodes right next to ~%ch%!&&0
          if %cycle% == %diff% && %diff% >= 3 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
            dg_affect #11851 %ch% STUNNED on 5
          end
          %damage% %ch% 130 physical
        elseif %ch.is_pc%
          %send% %ch% &&AYou cover your eyes as you swim out of the way of an imploding bubble!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&A**** Here comes another one... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  wait 8 s
elseif %move% == 3
  * Lightning Wave
  %echo% &&AA terrifying clap of thunder shakes you to the core, even down here...&&0
  %echo% &&A**** The First Water seems to be drawing down the lightning! ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear interrupt
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  set needed %room.players_present%
  while !%broke% && %cycle% <= 4
    wait 4 s
    if %self.sfinterrupt_count% >= 1 && (%self.sfinterrupt_count% >= %needed% || %self.sfinterrupt_count% == 4)
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&AYou manage to interrupt the First Water before another bolt can come down!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&AThe water is still for a moment...&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
        wait 10 s
      elseif %diff% < 4
        dg_affect #11852 %self% HARD-STUNNED on 5
        wait 5 s
      end
    else
      if %cycle% < 4
        %echo% &&A**** A bolt strikes the water above you, triggering a lightning wave! ****&&0 (interrupt)
      else
        %echo% &&AA bolt strikes the water above you, triggering a lightning wave!&&0
      end
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          * hit and no counterspell
          %send% %ch% &&AYou gurgle in pain as the wave passes through you!&&0
          %echoaround% %ch% &&A~%ch% gurgles in pain as the wave passes through *%ch%!&&0
          %damage% %ch% 100 physical
          if %cycle% == 4 && %diff% == 4 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
            dg_affect #11851 %ch% STUNNED on 10
          end
        end
        set ch %next_ch%
      done
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 4
  * Under Pressure
  skyfight clear struggle
  %echo% &&AThe water goes still for a moment -- too still...&&0
  nop %self.add_mob_flag(NO-ATTACK)%
  wait 3 sec
  %echo% &&A**** You are trapped in a pressure wave! ****&&0 (struggle)
  skyfight setup struggle all 20
  set ch %room.people%
  while %ch%
    set bug %ch.inventory(11890)%
    if %bug%
      set strug_char You struggle to escape the pressure wave...
      set strug_room ~%%actor%% struggles against the pressure wave...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You manage to get out of the pressure wave!
      set free_room ~%%actor%% manages to get out of the pressure wave!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    set ch %ch.next_in_room%
  done
  * messages
  set cycle 0
  eval time_pain (%diff% - 1) * 7.5
  eval dodge_pain %diff% * 10
  while %cycle% < 4
    wait 5 s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %ch.affect(11822)%
        if %diff% >= 2
          eval enter_%ch.id% %room.var(enter_%ch.id%,%timestamp%)% - %time_pain%
          remote enter_%ch.id% %room.id%
        end
        if %cycle% == 3
          * final
          %send% %ch% &&AYou're no longer under pressure but your lungs ache badly!&&0
          dg_affect #11822 %ch% off silent
          * penalty
          dg_affect #11972 %ch% DODGE -%dodge_pain% 25
        else
          %send% %ch% &&A**** You are still trapped in the pressure wave! ****&&0 (struggle)
        end
      end
      set ch %next_ch%
    done
    if %cycle% >= (5 - %diff%)
      nop %self.remove_mob_flag(NO-ATTACK)%
    end
    eval cycle %cycle% + 1
  done
  nop %self.remove_mob_flag(NO-ATTACK)%
  skyfight clear struggle
end
* in case
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11985
Gray fox pet behavior~
0 bt 25
~
set list 11893 11946 11947 11977
set room %self.room%
set ch %room.people%
set any 0
while %ch% && !%any%
  if %ch% != %self% && %ch.is_npc% && %list% ~= %ch.vnum%
    set any 1
    switch %random.9%
      case 1
        %echo% One of the foxes yips and plays with the other one.
      break
      case 2
        %echo% One of the foxes sniffs at the other.
      break
      case 3
        %echo% One of the foxes chases the other one in a little circle.
      break
      case 4
        %echo% The two foxes roll around on the ground.
      break
      case 5
        %echo% One fox bites the other one on the tail.
      break
      case 6
        %echo% The foxes tumble around, playing.
      break
      case 7
        %echo% One of the foxes sneaks up on the other one, who leaps into the air!
      break
      case 8
        %echo% A fox nips at the other one.
      break
      case 9
        %echo% The two foxes chase each other back and forth.
      break
    done
  end
  set ch %ch.next_in_room%
done
if %any%
  nop %self.add_mob_flag(SILENT)%
else
  nop %self.remove_mob_flag(SILENT)%
end
* short delay
wait 30 s
~
#11986
Grand High Sorceress combat: Creeping Vines, Cavitation Cascade, Pocket Glitter, Scalding Air, Summon Frens~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
* order
set moves_left %self.var(moves_left)%
set num_left %self.var(num_left,0)%
if !%moves_left% || !%num_left%
  set moves_left 1 2 3 4 5
  set num_left 5
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
skyfight lockout 30 35
if %move% == 1
  * Creeping Vines
  skyfight clear struggle
  %echo% &&y~%self% shouts, 'By the power of Skycleave!'&&0
  wait 3 sec
  %echo% &&m**** Creeping vines come out of the woodwork, ensnaring your arms and legs! ****&&0 (struggle)
  skyfight setup struggle all 20
  set ch %room.people%
  while %ch%
    set bug %ch.inventory(11890)%
    if %bug%
      set strug_char You struggle against the vines...
      set strug_room ~%%actor%% struggles against the vines...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You get a hand loose and are able to escape the vines!
      set free_room ~%%actor%% manages to free *%%actor%%self!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    set ch %ch.next_in_room%
  done
  * damage
  if %diff% > 1
    set cycle 0
    while %cycle% < 5
      wait 4 s
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %ch.affect(11822)%
          %send% %ch% &&m**** You bleed from your arms and legs as the thorny vines cut into you! ****&&0 (struggle)
          %damage% %ch% 100 magical
        end
        set ch %next_ch%
      eval cycle %cycle% + 1
    done
  else
    wait 20 s
  end
elseif %move% == 2
  * Cavitation Cascade
  skyfight clear dodge
  %echo% &&y~%self% shouts, 'By the power of Skycleave!'&&0
  wait 3 sec
  %echo% &&m**** Suddenly the air around you begins to explode with violent blasts! ****&&0 (dodge)
  set cycle 1
  eval wait 12 - %diff%
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          %echo% &&mThere's a blinding flash as the air explodes right next to ~%ch%!&&0
          if %cycle% == %diff% && %diff% >= 3 && (%self.level% + 100) > %ch.level% && !%ch.aff_flagged(!STUN)%
            dg_affect #11851 %ch% STUNNED on 5
          end
          %damage% %ch% 150 physical
        elseif %ch.is_pc%
          %send% %ch% &&mYou duck behind the furniture as the air explodes!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&m**** Here comes another one... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  wait 8 s
elseif %move% == 3
  * Pocket Glitter
  skyfight clear dodge
  say Pocket Glitter!
  wait 1
  %echo% &&m**** &&Z~%self% pulls a handful of magenta glitter from her pocket and throws it across the room! ****&&0 (dodge)
  skyfight setup dodge all
  set any 0
  set cycle 0
  eval max (%diff% + 2) / 2
  while %cycle% < %max%
    wait 5 s
    if %self.disabled%
      halt
    end
    set this 0
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if %ch.var(did_sfdodge)%
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        else
          set any 1
          set this 1
          %echo% &&mThe glitter gets into |%ch% eyes and nose!&&0
          switch %random.5%
            case 1
              dg_affect #11920 %ch% BLIND on 30
              %send% %ch% You're blind!
            break
            case 2
              dg_affect #11920 %ch% DODGE -50 30
              %send% %ch% You're distracted by sparkles in your eyes!
            break
            case 3
              dg_affect #11920 %ch% IMMOBILIZED on 30
              %send% %ch% Your legs don't seem to work right!
            break
            case 4
              dg_affect #11920 %ch% TO-HIT -50 30
              %send% %ch% Your arms don't seem to work right!
            break
            case 5
              dg_affect #11920 %ch% SLOW on 30
              %send% %ch% You're having trouble moving!
            break
          done
          %damage% %ch% 50 physical
        end
      end
      set ch %next_ch%
    done
    if !%this%
      %echo% &&mThe magenta glitter falls harmlessly to the floor.&&0
    end
    eval cycle %cycle% + 1
  done
  if !%any%
    * full miss
    %echo% &&m~%self% gives a shocked look as glitter blows back into her face!&&0
    dg_affect #11852 %self% HARD-STUNNED on 5
  end
  skyfight clear dodge
elseif %move% == 4
  * Scalding Air
  skyfight clear interrupt
  %echo% &&y~%self% shouts, 'By the power of Skycleave!'&&0
  wait 3 sec
  set targ %self.fighting%
  set id %targ.id%
  %echo% &&m**** &&Z~%self% holds her gnarled wand high and the air starts to heat up FAST... ****&&0 (interrupt)
  skyfight setup interrupt all
  set cycle 0
  set broke 0
  while !%broke% && %cycle% < 5
    wait 4 s
    if %self.disabled%
      halt
    end
    if %self.sfinterrupt_count% >= 1 && %self.sfinterrupt_count% >= (%diff% + 1) / 2
      set broke 1
      set ch %room.people%
      while %ch%
        if %ch.var(did_sfinterrupt,0)%
          %send% %ch% &&mYou manage to distract the sorceress by throwing knickknacks at her!&&0
        end
        set ch %ch.next_in_room%
      done
      %echo% &&m~%self% is hit in the head with a knickknack; the air starts to cool back down as her spell breaks.&&0
      if %diff% == 1
        dg_affect #11852 %self% HARD-STUNNED on 10
      end
    else
      set ch %room.people%
      while %ch%
        set next_ch %ch.next_in_room%
        if %self.is_enemy(%ch%)%
          if %ch.trigger_counterspell%
            %send% %ch% &&m... your counterspell does nothing against the scalding air!&&0
          end
          * hit!
          %send% %ch% &&mThe scalding air burns your skin, eyes, and lungs!&&0
          %echoaround% %ch% &&m~%ch% screams as the air scalds *%ch%!&&0
          %damage% %ch% 100 magical
        end
        set ch %next_ch%
      done
    end
    eval cycle %cycle% + 1
  done
  skyfight clear interrupt
elseif %move% == 5
  * Summon Frens
  if %diff% > 2
    if %random.2% == 1
      %echo% &&y~%self% shouts, 'Gaaaaableeeeeeeeeeens!'&&0
      set vnum 11817
    else
      %echo% &&y~%self% shouts, 'To me, my little friend!'&&0
      set vnum 11820
    end
    wait 4 s
    %load% m %vnum% ally %self.level%
    set mob %room.people%
    if %mob.vnum% == %vnum%
      nop %mob.add_mob_flag(!LOOT)%
      set diff %diff%
      remote diff %mob.id%
      if %mob.vnum% == 11820
        %echo% &&mThere's a flash and a BANG! And ~%mob% comes flying through the fog door!&&0
      else
        %echo% &&m~%mob% comes running in from the fog door!&&0
      end
      %force% %mob% mkill %actor%
    end
  end
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11987
Nailbokh the Axe combat: Axe-nado, Sand Slash, Whirling Storm, Rain of Hatchets~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
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
skyfight lockout 30 35
if %move% == 1
  * Axe-nado
  skyfight clear dodge
  %subecho% %room% &&y~%self% shouts, 'Axe tornado!'&&0
  %echo% &&j~%self% winds up and starts to spin with her axe out...&&0
  eval dodge %diff% * 40
  dg_affect #11958 %self% DODGE %dodge% 20
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 3 s
  if %self.disabled%
    dg_affect #11958 %self% off
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %subecho% %room% &&y~%self% shouts, 'Aaaaaaaaaaaahhhh!'&&0
  %echo% &&j**** ~%self% whirls right toward you with her axe out! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          set hit 1
          %echo% &&j~%self% slices ~%ch% as she whirls around the arena!&&0
          %dot% #11957 %ch% 100 15 physical 4
        elseif %ch.is_pc%
          %send% %ch% &&jYou narrowly avoid the axe-nado!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&j**** She's still spinning... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  dg_affect #11958 %self% off
  if !%hit%
    if %diff% < 3
      %echo% &&j~%self% spins herself out and bobbles to a stop.&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
elseif %move% == 2
  * Sand Slash
  if %diff% <= 2
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight clear dodge
  set targ %self.fighting%
  set id %targ.id%
  %subecho% %room% &&y~%self% shouts, 'Aha!'&&0
  %send% %targ% &&j**** &&Z~%self% shouts triumphantly as she slashes at the sand in front of you! ****&&0 (dodge)
  %echoaround% %targ% &&j~%self% shouts triumphantly as she slashes at the sand!&&0
  skyfight setup dodge %targ%
  wait 8 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if !%targ% || %targ.id% != %id%
    * gone
    %echo% &&jThe slash of sand flies off the side of the arena.&&0
  elseif %targ.var(did_sfdodge)%
    * miss
    %echo% &&jThe sand from |%self% sand slash misses ~%targ%!&&0
    if %diff% == 1
      dg_affect #11856 %targ% TO-HIT 25 20
    end
  else
    * hit
    %echo% &&jThe sand flies into |%targ% eyes!&&0
    if %diff% >= 3 && (%self.level% + 100) > %targ.level% && !%targ.aff_flagged(!STUN)%
      %send% %targ% &&jThat really hurt! You can't do anything but try to get the sand out of your eyes.&&0
      dg_affect #11851 %targ% STUNNED on 15
    else
      %send% %targ% &&jYou're blind!&&0
      dg_affect #11959 %targ% BLIND on 10
    end
    if %diff% >= 3
      %damage% %targ% 100 physical
    end
  end
  skyfight clear dodge
elseif %move% == 3
  * Whirling Storm
  skyfight clear interrupt
  %echo% &&j**** &&Z~%self% leans her huge axe into the arena sand and starts to whirl in a circle! ****&&0 (interrupt)
  eval dodge %diff% * 40
  dg_affect #11958 %self% DODGE %dodge% 20
  skyfight setup interrupt all
  set requires %room.players_present%
  if %requires% > 4
    set requires 4
  end
  wait 4 s
  if %self.disabled%
    dg_affect #11958 %self% off
    halt
  end
  %subecho% %room% &&y~%self% shouts, 'Yaaaaaaaaaaaaaaaaaa!'&&0
  if %self.var(sfinterrupt_count,0)% < %requires%
    %echo% &&j**** Sand is flying everywhere as ~%self% whirls around with her axe, screaming... ****&&0 (interrupt)
  end
  wait 4 s
  if %self.disabled%
    dg_affect #11958 %self% off
    halt
  end
  if %self.var(sfinterrupt_count,0)% >= %requires%
    %echo% &&jYou manage to interrupt ~%self% before the whirling storm gets too big!&&0
    if %diff% <= 2
      %echo% &&j~%self% is spun out!&&0
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
    wait 30 s
  else
    %echo% &&j~%self% whirls up a sandstorm and it blows right into your face!&&0
    dg_affect #11958 %self% off
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        dg_affect #11959 %ch% BLIND on 20
        %dot% #11960 %ch% 75 10 physical
      end
      set ch %next_ch%
    done
  end
  skyfight clear interrupt
elseif %move% == 4
  * Rain of Hatchets
  skyfight clear dodge
  %subecho% %room% &&y~%self% shouts, 'Alalalalalalalalala!'&&0
  %echo% &&j~%self% lets out a piercing war cry as she throws hatchet after hatchet into the air... this won't be good.&&0
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 3 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %echo% &&j**** Oh no... it's raining hatchets! Watch out! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          set hit 1
          %echo% &&jA hatchet comes down on |%ch% head!&&0
          %damage% %ch% 100 physical
        elseif %ch.is_pc%
          %send% %ch% &&jA hatchet just misses you as it falls!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&j**** There are still more hatchets coming down! ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  if !%hit%
    if %diff% < 3
      %echo% &&jA stray hatchet hits ~%self% on the head, handle-first!&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11988
Biksi, Champion of Orka combat: Thornlash, Thornbound, Triplash, Crown of Thorns~
0 k 100
~
if %self.cooldown(11800)% || %self.disabled%
  halt
end
set room %self.room%
set diff %self.diff%
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
skyfight lockout 30 35
if %move% == 1
  * Thornlash
  skyfight clear dodge
  %subecho% %room% &&y~%self% shouts, 'Time to dance!'&&0
  %echo% &&j~%self% screams as she furiously lashes both her thorny whips around the arena!&&0
  eval dodge %diff% * 40
  dg_affect #11950 %self% DODGE %dodge% 20
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 3 s
  if %self.disabled%
    dg_affect #11950 %self% off
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  %echo% &&j**** If you're hoping to avoid |%self% lashing whips, now is the time! ****&&0 (dodge)
  set cycle 1
  set hit 0
  eval wait 10 - %diff%
  while %cycle% <= %diff%
    skyfight setup dodge all
    wait %wait% s
    set ch %room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        if !%ch.var(did_sfdodge)%
          set hit 1
          %echo% &&j~%self% lashes ~%ch% with her thorny whips!&&0
          %dot% #11951 %ch% 100 40 physical 25
        elseif %ch.is_pc%
          %send% %ch% &&jThe lashing whip hits the sand near your feet!&&0
          if %diff% == 1
            dg_affect #11856 %ch% TO-HIT 25 20
          end
        end
        if %cycle% < %diff%
          %send% %ch% &&j**** Here come the whips again... ****&&0 (dodge)
        end
      end
      set ch %next_ch%
    done
    skyfight clear dodge
    eval cycle %cycle% + 1
  done
  dg_affect #11950 %self% off
  if !%hit%
    if %diff% < 3
      %echo% &&j~%self% seems to exhaust herself from the thornlash.&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
elseif %move% == 2 && !%self.aff_flagged(BLIND)%
  * Thornbound
  skyfight clear free
  skyfight clear struggle
  %echo% &&j~%self% pulls one of her whips back and takes aim...&&0
  wait 3 s
  if %self.disabled%
    halt
  end
  set targ %random.enemy%
  if !%targ%
    halt
  end
  set targ_id %targ.id%
  if %self.fighting% == %targ% && %diff% < 4
    dg_affect #11852 %self% HARD-STUNNED on 20
  end
  if %diff% <= 2 || (%self.level% + 100) <= %targ.level%
    %send% %targ% &&j**** ~%self% lashes her thorny whip around you! You can't move! ****&&0 (struggle)
    %echoaround% %targ% &&j~%self% lashes her thorny whip around ~%targ%, trapping *%targ%!&&0
    skyfight setup struggle %targ% 20
    set bug %targ.inventory(11890)%
    if %bug%
      set strug_char You try to wiggle out of the thorny whip...
      set strug_room You hear ~%%actor%% tries to wiggle free of the thorny whip...
      remote strug_char %bug.id%
      remote strug_room %bug.id%
      set free_char You wiggle out of the thorny whip!
      set free_room ~%%actor%% manages to wiggle free of the thorny whip!
      remote free_char %bug.id%
      remote free_room %bug.id%
    end
    set cycle 0
    set done 0
    while %cycle% < 5 && !%done%
      wait 4 s
      if %targ.id% == %targ_id% && %targ.affect(11822)% && %diff% > 1
        %send% %targ% &&jThe thorns press into you! That really hurts!&&0
        %dot% #11951 %targ% 100 40 physical 25
      else
        set done 1
      end
      eval cycle %cycle% + 1
    done
    skyfight clear struggle
  else
    %send% %targ% &&j~%self% lashes her thorny whip around you! You can't move!&&0
    %echoaround% %targ% &&j**** ~%self% lashes her thorny whip around ~%targ%, trapping *%targ%! ****&&0 (free %targ.pc_name.car%)
    skyfight setup free %targ%
    eval time %diff% * 15
    dg_affect #11949 %targ% HARD-STUNNED on %time%
    * wait and clear
    set done 0
    while !%done% && %time% > 0
      wait 5 s
      eval time %time% - 5
      if %targ_id% != %targ.id%
        set done 1
      elseif !%targ.affect(11888)%
        set done 1
      else
        %send% %targ% &&jThe thorns press into you! That really hurts!&&0
        %dot% #11951 %ch% 100 40 physical 25
      end
    done
    skyfight clear free
  end
  dg_affect #11852 %self% off
elseif %move% == 3
  * Triplash
  skyfight clear dodge
  %echo% &&j**** ~%self% lets out a wicked war cry as she lashes her thorny whips wildly around the arena! ****&&0 (dodge)
  eval dodge %diff% * 40
  dg_affect #11950 %self% DODGE %dodge% 10
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup dodge all
  wait 8 s
  set ch %room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if %self.is_enemy(%ch%)%
      if !%ch.var(did_sfdodge)%
        set hit 1
        %echo% &&j~%self% trips ~%ch% with her thorny whip!&&0
        if %diff% >= 3 && (%self.level% + 100) <= %ch.level% && !%ch.aff_flagged(!STUN)%
          dg_affect #11814 %ch% STUNNED on 10
        else
          dg_affect #11814 %ch% DISARMED on 10
          dg_affect #11814 %ch% IMMOBILIZED on 10
        end
        if %diff% > 1
          %dot% #11951 %ch% 100 40 physical 25
        end
      elseif %ch.is_pc%
        %send% %ch% &&jThe whip sends sand spraying as it narrowly misses your feet!&&0
        if %diff% == 1
          dg_affect #11856 %ch% TO-HIT 25 20
        end
      end
    end
    set ch %next_ch%
  done
  skyfight clear dodge
  dg_affect #11950 %self% off
  if !%hit%
    if %diff% < 3
      %echo% &&j~%self% seems to exhaust herself from the triplash.&&0
      dg_affect #11852 %self% HARD-STUNNED on 10
    end
  end
  wait 8 s
elseif %move% == 4
  * Crown of Thorns
  skyfight clear interrupt
  %echo% &&j**** &&Z~%self% seems to be wrapping her whip around her own head! ****&&0 (interrupt)
  if %diff% == 1
    nop %self.add_mob_flag(NO-ATTACK)%
  end
  skyfight setup interrupt all
  if %self.diff% < 3 || %room.players_present% < 2
    set requires 1
  else
    set requires 2
  end
  wait 4 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if %self.var(sfinterrupt_count,0)% < %requires%
    %echo% &&j**** &&Z~%self% is screaming something you can't understand as blood trickles down her face... ****&&0 (interrupt)
  end
  wait 4 s
  if %self.disabled%
    nop %self.remove_mob_flag(NO-ATTACK)%
    halt
  end
  if %self.var(sfinterrupt_count,0)% >= %requires%
    %echo% &&j~%self% is interrupted during whatever she was doing with those thorns... thankfully!&&0
    if %diff% == 1
      dg_affect #11852 %self% HARD-STUNNED on 5
    end
    wait 30 s
  else
    %echo% &&jBlood trickles down |%self% as she lets out a fearsome war cry!&&0
    eval amount %diff% * 20
    dg_affect #11948 %self% BONUS-PHYSICAL %amount% 300
  end
  skyfight clear interrupt
end
nop %self.remove_mob_flag(NO-ATTACK)%
~
#11989
Goblin's Dream: Guard patrol~
0 b 50
~
* The captain of the guard doesn't wander; he patrols
* This roughly folllows the expected room order:
switch %self.room.template%
  case 11989
    up
  break
  case 11988
    north
  break
  case 11985
    down
    wait 13 sec
    up
    wait 13 sec
    northwest
  break
  case 11986
    northeast
  break
  case 11983
    north
  break
  case 11990
    south
    wait 1 sec
    southeast
  break
  case 11984
    east
  break
  case 11991
    west
    wait 1 sec
    southwest
    wait 13 sec
    south
    wait 13 sec
    down
  break
  case 11978
    * just in case
    up
  break
  default
    * where the heck am I?
    %echo% ~%self% leaves.
    mgoto i11989
    %echo% ~%self% arrives.
  break
done
~
#11990
Liberated wand casts spells on leader~
0 bt 5
~
set verb_list flicks swishes taps twirls waves
set verb_count 5
set ch %random.char%
set leader %self.leader%
if !%ch% || !%leader% || %ch.is_enemy(%leader%)% || %self.room% != %leader.room%
  halt
end
* remove first
set aff_list 11990
while %aff_list%
  set vnum %aff_list.car%
  set aff_list %aff_list.cdr%
  dg_affect #%vnum% %ch% off silent
done
switch %random.12%
  case 1
    set type MOVE-REGEN
    set amount 1
    set vnum 11990
    set glow amber
  break
  case 2
    set type MANA-REGEN
    set amount 1
    set vnum 11991
    set glow cerulean
  break
  case 3
    set type AGE
    set amount 1
    set vnum 11992
    set glow ghastly
  break
  case 4
    set type AGE
    set amount -1
    set vnum 11993
    set glow powdery
  break
  case 5
    set type MAX-MOVE
    set amount 10
    set vnum 11994
    set glow flaxen
  break
  case 6
    set type MAX-MANA
    set amount 10
    set vnum 11995
    set glow fluvial
  break
  case 7
    set type MAX-HEALTH
    set amount 10
    set vnum 11996
    set glow ruby
  break
  case 8
    set type MAX-BLOOD
    set amount 10
    set vnum 11997
    set glow sanguine
  break
  case 9
    set type RESIST-PHYSICAL
    set amount 5
    set vnum 11998
    set glow mahogany
  break
  case 10
    set type RESIST-MAGICAL
    set amount 5
    set vnum 11999
    set glow maple
  break
  case 11
    set type INVENTORY
    set amount 5
    set vnum 11989
    set glow light
  break
  default
    set type DODGE
    set amount 5
    set vnum 11988
    set glow blurry
  break
done
* prepare
eval pos %%random.%verb_count%%%
while %pos% > 0
  set verb %verb_list.car%
  set verb_list %verb_list.cdr%
  eval pos %pos% - 1
done
* message
dg_affect #%vnum% %ch% %type% %amount% 300
%send% %ch% ~%self% %verb% itself toward you... you take on a %glow% glow.
%echoaround% %ch% ~%self% %verb% itself toward ~%ch%... &%ch% takes on a %glow% glow.
~
#11991
Skycleave: Barrosh storytime using script1-3~
0 bw 100
~
* variant of 11840/Storytime:
* script1: Grace died
* script2: Knezz died but Grace lived
* script3: Both lived
set line_gap 9 s
set story_gap 360 s
* random wait to offset competing scripts slightly
wait %random.30%
* determine story number
set knezz %instance.mob(11968)%
set grace %instance.mob(11966)%
if %knezz% && %grace%
  set story 3
elseif %grace%
  set story 2
else
  set story 1
end
* story detected: prepare (storing as variables prevents reboot issues)
if !%self.mob_flagged(SENTINEL)%
  set no_sentinel 1
  remote no_sentinel %self.id%
  nop %self.add_mob_flag(SENTINEL)%
end
if !%self.mob_flagged(SILENT)%
  set no_silent 1
  remote no_silent %self.id%
  nop %self.add_mob_flag(SILENT)%
end
* tell story
set pos 0
set done 0
while !%done%
  set msg %self.custom(script%story%,%pos%)%
  if %msg%
    set mode %msg.car%
    set msg %msg.cdr%
    if %mode% == say
      say %msg%
      wait %line_gap%
    elseif %mode% == do
      %msg.process%
      * no wait
    elseif %mode% == echo
      %echo% %msg.process%
      wait %line_gap%
    elseif %mode% == vforce
      set vnum %msg.car%
      set msg %msg.cdr%
      set targ %self.room.people(%vnum%)%
      if %targ%
        %force% %targ% %msg.process%
      end
    elseif %mode% == emote
      emote %msg%
      wait %line_gap%
    elseif %mode% == set
      set subtype %msg.car%
      set msg %msg.cdr%
      if %subtype% == line_gap
        set line_gap %msg%
      elseif %subtype% == story_gap
        set story_gap %msg%
      else
        %echo% ~%self%: Invalid set type '%subtype%' in storytime script.
      end
    elseif %mode% == skip
      * nothing this round
      wait %line_gap%
    else
      %echo% %self.name%: Invalid script message type '%mode%'.
    end
  else
    set done 1
  end
  eval pos %pos% + 1
done
* cancel sentinel/silent
if %self.varexists(no_sentinel)%
  nop %self.remove_mob_flag(SENTINEL)%
end
if %self.varexists(no_silent)%
  nop %self.remove_mob_flag(SILENT)%
end
* wait between stories
wait %story_gap%
~
#11992
Smash striped stone seedling to create calamander forest~
1 c 2
smash~
* smash <self>
set valid_sects 27 28 55 62 64 65
set room %actor.room%
set targ %actor.obj_target(%arg%)%
if %targ% != %self%
  return 0
  halt
end
return 1
if !(%valid_sects% ~= %room.sector_vnum%)
  %send% %actor% You don't want to do that here. Try a jungle.
  halt
elseif !%room.empire_id%
  %send% %actor% You need to claim the area to do this.
  halt
elseif !%actor.canuseroom_member(%room%)%
  %send% %actor% You don't have permission to use @%self% here.
  halt
end
%send% %actor% You smash @%self% on the ground...
%echoaround% %actor% ~%actor% smashes @%self% on the ground...
%echo% Streaks of brown light burst from the broken statuette! The jungle transforms before your very eyes as the light strikes it!
%terraform% %room% 11990
%purge% %self%
~
#11993
Clingy cloak wears itself on uncloaked people~
0 btw 20
~
set pers %random.char%
if %pers.is_npc% || %pers.eq(about)%
  halt
end
* put cloak on them
%send% %pers% ~%self% flies toward you...
%send% %pers% It flings itself over your shoulders and neatly ties its strings in a bow!
%echoaround% %pers% ~%self% flies toward ~%pers%...
%echoaround% %pers% It flings itself over ^%pers% shoulders and ties its strings in a bow!
%load% obj 11993 %pers% about
%purge% %self%
~
#11994
Skycleave: Fake movement in hidden areas~
2 q 100
~
* for rooms with fake exits: you can't actually leave
* but first ensure it's a walking direction
set valid_dirs north south east west northeast northwest southeast southwest up down port starboard fore aft
if !(%valid_dirs% ~= %direction%)
  halt
end
* test if it's a real exit that doesn't go here or to 11974
eval real_test %%room.%direction%(room)%%
if %real_test% != %room% && %real_test.template% != 11974
  halt
end
* prevent leaving
return 0
switch %self.template%
  case 11993
    %send% %actor% You walk along the river.
    %echoaround% %actor% ~%actor% walks along the river.
    %force% %actor% look
  break
  case 11994
    %send% %actor% You go %direction%.
    %force% %actor% look
  break
  case 11972
    %send% %actor% You swim %direction%.
    %echoaround% %actor% ~%actor% swims %direction%.
    %force% %actor% look
  break
  case 11973
    %send% %actor% You walk through the cold fog, but find yourself in the same room again on the other side.
    %echoaround% %actor% ~%actor% walks through the %direction% fog door, but comes back out the other one.
  break
done
~
#11995
Skycleave: Reset comment count on enter (room version)~
2 gA 100
~
* pairs with triggers like 11996 to reset comments when a player arrives
set comment 0
remote comment %self.id%
~
#11996
Priest's Dream: Presence of the god~
2 bw 100
~
* dream cutscene: player meets the god of Orka
* The comment sequence will reset whenever a player enters
* First, ensure a player is awake here (don't start while sleeping)
set ch %room.people%
set any 0
while %ch% && !%any%
  if %ch.is_pc% && %ch.position% != Sleeping
    set any 1
  end
  set ch %ch.next_in_room%
done
if !%any%
  halt
end
* load comment number
if %self.varexists(comment)%
  eval comment %self.comment% + 1
else
  set comment 1
end
remote comment %self.id%
switch %comment%
  case 1
    * skip first one
  break
  case 2
    %echo% A voice booms, 'Hello.'
  break
  case 3
    %echo% A voice booms, 'I've been expecting you.'
  break
  case 4
    %echo% A voice booms, 'You're safe here.'
  break
  case 5
    %echo% A voice booms, 'You can wake up any time you want.'
  break
  case 6
    %echo% A voice booms, 'Or you can just rest here for a bit.'
  break
  case 7
    %echo% A voice booms, 'Whatever you decide, I love you.'
  break
done
~
#11997
Adoring fan idle animations~
0 bt 8
~
* ensure leader
set leader %self.leader%
if !%leader%
  halt
end
* check for non-leader humans present (some scripts use this)
set guests 0
set guest_who 0
set ch %self.room.people%
while %ch%
  if %ch% != %leader% && %self.can_see(%ch%)% && (%ch.is_pc% || %ch.mob_flagged(HUMAN)%)
    eval guests %guests% + 1
    if !%guest_who%
      set guest_who %ch%
    end
  end
  set ch %ch.next_in_room%
done
* messaging
switch %random.10%
  case 1
    %echo% ~%self% flutters around to cool ~%leader%.
  break
  case 2
    %echo% ~%self% blushes and fans.
  break
  case 3
    %echo% ~%self% does an amazing fan dance.
    wait 4 sec
    %echo% ~%self% dances around in the air.
    wait 4 sec
    %echo% ~%self% flutters and twirls.
  break
  case 4
    say I'm just a huge fan.
  break
  case 5
    if %guests% > 0
      say Hey, have you heard about %leader.name%? Greatest hero in the land!
    else
      say You're just the greatest hero in the land!
    end
  break
  case 6
    if %guests% > 0
      say I guess you look pretty great, but for my money, nobody beats %leader.name%.
    else
      say There's never been a hero like you!
    end
  break
  case 7
    if %guests% > 0
      say Oh wow, that's %leader.name% over there. Let's get %leader.hisher% autograph!
    else
      say You must have a lot of fans.
    end
  break
  case 8
    if %guest_who%
      say Sure, %guest_who.name% is okay, I guess, but have you heard of %leader.name%?
    else
      say I'm so glad you brought me, even though you're too cool for me.
    end
  break
  case 9
    %echo% ~%self% waves at ~%leader%.
  break
  case 10
    if %guests% > 0
      say I'm a lifelong fan of %leader.name%.
    else
      say I'm a lifelong fan.
    end
  break
done
~
#11998
Gemstone flute: Everybody dance now~
1 ab 100
~
set actor %self.worn_by%
if !%actor%
  detach 11998 %self.id%
  halt
elseif %actor.action% != playing
  detach 11998 %self.id%
  halt
end
* lists
set list1 11873 11874 11875 11876 11877 11878 11879 11880 11881 11882 11883 11885 11886 11887
set list2 615 616 10042 11520 11521 11522 11523 11524 11525 11526 11819 11820 11963 11982 11624 11625
* loop
set ch %actor.room.people%
while %ch%
  if %ch.is_npc% && !%ch.disabled% && %ch.position% == Standing
    if %list1% ~= %ch.vnum% || %list2% ~= %ch.vnum%
      wait 1
      switch %ch.vnum%
        case 11873
          %echo% ~%ch% moves ^%ch% feet to the music.
        break
        case 11874
          %echo% ~%ch% taps ^%ch% leg with ^%ch^ hand.
        break
        case 11875
          %echo% The chamber echoes as ~%ch% snaps ^%ch% fingers to the music.
        break
        case 11876
          %echo% ~%ch% hums along as the music plays.
        break
        case 11878
          %echo% ~%ch% dances a little as &%ch% walks.
        break
        case 11879
          %echo% ~%ch% taps ^%ch% foot to the music.
        break
        case 11882
          %echo% ~%ch% moves with the music as &%ch% works.
        break
        case 11883
          %echo% ~%ch% bobs ^%ch% head to the music.
        break
        case 11885
          %echo% ~%ch% hums along to the music.
        break
        case 11886
          %echo% ~%ch% sways to the music.
        break
        case 11887
          %echo% ~%ch% sways along.
        break
        default
          set rng %random.3%
          if %rng% == 1
            %echo% ~%ch% dances around.
          elseif %rng% == 2
            %echo% ~%ch% dances to the music.
          else
            %echo% ~%ch% dances along.
          end
        break
      done
    end
  end
  set ch %ch.next_in_room%
done
~
#11999
Crystal ball visions~
0 ct 0
look examine~
* looking at me?
if !%arg% || %actor.char_target(%arg%)% != %self%
  return 0
  halt
end
%send% %actor% You look into the crystal ball...
%echoaround% %actor% ~%actor% looks into the crystal ball.
* random response
switch %random.10%
  case 1
    %send% %actor% You see money -- tons of it -- huge, huge piles. Unfortunately, that's not you counting the money.
  break
  case 2
    %send% %actor% You see yourself getting falling asleep and getting stuck in a dreamcatcher.
  break
  case 3
    %send% %actor% You see goblins. So many goblins... Some of them are dancing. What could it mean?
  break
  case 4
    %send% %actor% It looks like you're playing a claw game machine quite a lot.
  break
  case 5
    %send% %actor% You see yourself smelting iron ore. What could that mean?
  break
  case 6
    %send% %actor% You see yourself looking into a crystal ball!
  break
  case 7
    %send% %actor% You see a tree... then another tree. It looks like you might be in a forest some time soon.
  break
  case 8
    %send% %actor% It just says 'Outcome uncertain.'
  break
  case 9
    %send% %actor% All you see is your own distorted reflection.
  break
  case 10
    %send% %actor% All you see is death.
  break
done
~
$
