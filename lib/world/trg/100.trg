#10000
Mob Kill Adventure Completion~
0 f 100
~
%adventurecomplete%
~
#10001
Mob Must Fight~
0 s 100
~
%send% %actor% You can't leave because of ~%self%.
return 0
~
#10002
Wildling combat: Nasty Bite~
0 k 10
~
* Nasty bite: low damage over time
%send% %actor% ~%self% snaps ^%self% teeth and takes off a piece of your skin!
%echoaround% %actor% ~%self% snaps ^%self% teeth at ~%actor% and takes off a piece of ^%actor% skin!
%dot% %actor% %random.100% 60 physical
~
#10003
Druid combat~
0 k 10
~
set chance %random.3%
if (%chance% < 3)
  lightningbolt
else
  * buuurn!
  %send% %actor% ~%self% reaches out and touches you, leaving a terrible burn!
  %echoaround% %actor% ~%self% reaches out and touches ~%actor%, leaving a terrible burn!
  %dot% %actor% 100 60 fire
end
~
#10004
Archdruid combat: frost~
0 k 10
~
set chance %random.3%
if %chance% < 3
  * Arctic chill on tank
  %send% %actor% &&r~%self% draws moisture from the air and hits you with an arctic chill!&&0
  %echoaround% %actor% ~%self% draws moisture from the air and hits ~%actor% with an arctic chill!
  %dot% %actor% 100 60 magical
  %damage% %actor% 90 magical
else
  * Frost storm AoE
  %echo% ~%self% summons a frost storm...
  * Give the healer (if any) time to prepare for group heals
  wait 3 sec
  %echo% &&r~%self% unleashes a frost storm!&&0
  %aoe% 50 magical
end
~
#10005
Magiterranean Grove environment~
2 bw 10
~
switch %random.4%
  case 1
    %echo% You hear the howl of nearby animals.
  break
  case 2
    %echo% A treebranch brushes your back.
  break
  case 3
    %echo% You hear strange chanting through the trees.
  break
  case 4
    %echo% Your footprints seem to vanish behind you.
  break
done
~
#10006
Spawn Red Dragon Mount~
0 f 15
~
* Load mob 10007: the rare red dragon mount
%echo% As the dragon dies, you notice a smaller red dragon cowering in the nest.
%load% mob 10007
~
#10007
Red Dragon combat~
0 k 10
~
switch %random.4%
  * Searing burns on tank
  case 1
    %send% %actor% &&r~%self% spits fire at you, causing searing burns!&&0
    %echoaround% %actor% ~%self% spits fire at ~%actor%, causing searing burns!
    %dot% %actor% 100 60 fire
    %damage% %actor% 20 fire
  break
  * Flame wave AoE
  case 2
    %echo% ~%self% begins puffing smoke and spinning in circles!
    * Give the healer (if any) time to prepare for group heals
    wait 3 sec
    %echo% &&r~%self% unleashes a flame wave!&&0
    %aoe% 25 fire
  break
  * Traumatic burns on tank
  case 3
    %send% %actor% &&r~%self% spits fire at you, causing traumatic burns!&&0
    %echoaround% %actor% ~%self% spits fire at ~%actor%, causing traumatic burns!
    %dot% %actor% 300 10 fire
    %damage% %actor% 20 fire
  break
  * Rock stun on random enemy
  case 4
    set target %random.enemy%
    if (%target%)
      %send% %target% &&r~%self% uses ^%self% tail to hurl a rock at you, stunning you momentarily!&&0
      %echoaround% %target% ~%self% hurls a rock at ~%target% with ^%self% tail, stunning *%target% momentarily!
      dg_affect %target% STUNNED on 10
      %damage% %target% 50 physical
    end
  break
done
~
#10011
Sewer Environment~
2 bw 5
~
switch %random.4%
  case 1
    %echo% The sound of dripping echoes in the distance.
  break
  case 2
    %echo% The stench of raw sewage nearly makes you gag.
  break
  case 3
    %echo% Something furry brushes past your ankle!
  break
  default
    %echo% The rancid smell of waste is thick in the air.
  break
done
~
#10012
Ratskins's request~
0 bw 15
~
say I could use some more rat skins, if you have any.
%echo% (Type 'trade' to exchange 15 rat skins.)
~
#10013
Ratskins's reward~
0 c 100
trade~
if %self.varexists(gave%actor.id%)%
  %send% %actor% You have already completed this quest in this adventure.
  halt
end
if (%actor.has_resources(10013,15)%)
  nop %actor.add_resources(10013,-15)%
  say Excellent, %actor.name%, this is exactly what I was looking for!
  %load% o 10015 %actor% inv
  %send% %actor% ~%self% gives you the ratskin totem.
  %echoaround% %actor% ~%self% gives ~%actor% the ratskin totem.
  set gave%actor.id% 1
  remote gave%actor.id% %self.id%
else
  say What I'm looking for is 15 rat skins.
end
~
#10014
Rat combat~
0 k 10
~
* Slow bite
%send% %actor% ~%self% bites into your flesh, and you don't feel so good.
%echoaround% %actor% ~%self% bites into |%actor% flesh, and &%actor% doesn't look so good.
dg_affect %actor% SLOW on 60
~
#10015
Dire rat combat~
0 k 10
~
switch %random.3%
  * Slow bite
  case 1
    %send% %actor% ~%self% bites into your flesh, and you don't feel so good.
    %echoaround% %actor% ~%self% bites into |%actor% flesh, and &%actor% doesn't look so good.
    dg_affect %actor% SLOW on 120
  break
  * Dash scratch AOE
  case 2
    %echo% &&r~%self% dashes around the room, striking everyone!&&0
    %aoe% 50 physical
  break
  * Pain bite
  case 3
    %send% %actor% ~%self% takes a huge bite out of your arm!
    %echoaround% %actor% ~%self% takes a huge bite out of |%actor% arm!
    %dot% %actor% 100 60 physical
    %damage% %actor% 20 physical
  break
done
~
#10016
Urchin combat~
0 k 10
~
set chance %random.5%
if (%chance% < 5)
  * blind tank
  blind %actor.name%
else
  * blind all
  set %count% 0
  set target_char %self.room.people%
  while %target_char%
    * save next char now
    set next_target %target_char.next_in_room%
    if %self.is_enemy(%target_char%)%
      blind %target_char%.name
      eval count %count% + 1
    end
    set target_char %next_target%
  done
  if %count% > 1
    detach 10016 %self.id%
  end
end
~
#10017
Rare thief death~
0 f 100
~
set ch %self.room.people%
while %ch%
  * Combat is ended by the thief's death, so is_enemy doesn't actually work
  set ch_stealth %ch.skill(Stealth)%
  eval can_gain (%ch_stealth% != 0) && (%ch_stealth% != 50) && (%ch_stealth% != 75) && (%ch_stealth% != 100)
  if (%ch.is_pc% && %ch.is_ally(%actor%)% && %can_gain% && !%ch.noskill(Stealth)%)
    %send% %actor% You learn a bit about Stealth from watching ~%self% fight.
    nop %ch.gain_skill(Stealth, 1)%
  end
  set ch %ch.next_in_room%
done
~
#10018
Rare thief despawn~
2 f 100
~
%purge% instance mob 10017 $n vanishes into the shadows!
~
#10019
Thief recruiter passive~
0 bw 5
~
set target_char %self.room.people%
while %target_char%
  if (%target_char.is_pc% && %target_char.skill(Stealth)% == 0)
    %echoaround% %target_char% ~%self% whispers something to ~%target_char%.
    %send% %target_char% ~%self% whispers, 'I could let you into the Guild for 100 coins.'
  end
  set target_char %target_char.next_in_room%
done
~
#10021
City Official says~
0 bw 15
~
switch %random.4%
  case 1
    say My, oh my, it's filthy down here!
  break
  case 2
    say I do wish someone would do something about this rat problem.
  break
  case 3
    say I do wish someone would do something about this urchin problem.
  break
  default
    say I simply do not get paid enough for this.
  break
done
~
#10022
City Official rewards~
0 bw 30
~
* Rewards some gold when a player has ratskins in their inventory.
set target %random.char%
if (%target.is_pc% && %target.has_resources(10013,10)%)
  say Ah, %target.name%, you've done my job for me. Here, have some money.
  nop %target.give_coins(100)%
  %send% %target% ~%actor% gives you 100 coins.
  detach 10022 %self.id%
end
~
#10023
Spider combat~
0 k 10
~
switch %random.2%
  * Webby on random enemy
  case 1
    set target %random.enemy%
    if (%target%)
      %send% %target% ~%self% shoots a web at you, anchoring you to the ground!
      %echoaround% %target% ~%self% shoots a web at ~%target%, anchoring *%target% to the ground!
      dg_affect %actor% SLOW on 120
      dg_affect %actor% IMMOBILIZED on 120
    end
  break
  * Searing Pain poison (DoT) on tank
  case 2
    %send% %actor% ~%self% sinks ^%self% fangs into your flesh, causing searing pain!
    %echoaround% %actor% ~%self% sinks ^%self% fangs into |%actor% flesh, causing searing pain!
    %dot% %actor% 100 60 poison
    %damage% %actor% 20 poison
  break
done
~
#10024
Baby dragon combat~
0 k 10
~
set chance %random.3%
if %chance% < 3
  * Fire spurt on tank
  %echo% ~%self% spurts fire at ~%actor%, causing searing burns!
  %dot% %actor% 100 60 fire
  %damage% %actor% 20 fire
else
  * Flame wave AoE
  %echo% ~%self% begins puffing smoke and spinning in circles!
  * Give the healer (if any) time to prepare for group heals
  wait 3 sec
  %echo% &&r~%self% unleashes a flame wave!&&0
  %aoe% 50 fire
end
~
#10025
Rat hunter/rare thief combat~
0 k 10
~
switch %random.5%
  * Kick
  case 1
    kick
  break
  * Jab
  case 2
    jab
  break
  * Disarm
  case 3
    disarm
  break
  * Healing Potion
  case 4
    %echo% ~%self% quaffs a potion!
    %damage% %self% -100
  break
  * Buff Potion
  case 5
    %echo% ~%self% quaffs a vial of sewer water!
    dg_affect %self% DEXTERITY 1 60
  break
done
~
#10026
Cheese Drop Rat Summon~
1 h 50
~
wait 2 sec
%echo% A rat appears and gobbles up the cheese!
%load% m 10011
%purge% %self%
~
#10027
Nest miniboss spawn/despawn~
2 f 100
~
* Get rid of the old miniboss
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if (%ch.vnum% == 10018 || %ch.vnum% == 10019 || %ch.vnum% == 10020)
    if !(%ch.fighting%)
      %echo% ~%ch% leaves to hunt somewhere else.
      %purge% %ch%
    end
  end
  set ch %next_ch%
done
* Spawn a new one
eval vnum 10017+%random.3%
%load% mob %vnum%
set person %self.people%
%echo% ~%person% arrives.
~
#10030
Gossipers~
0 bw 10
~
* This script is no longer used. It was replaced by custom strings.
switch %random.4%
  case 1
    say I visited the Tower Skycleave and all I got was this skystone!
  break
  case 2
    say Have you heard about this traveling sorcery tower?
  break
  case 3
    say I swear that Skycleave barista re-heated my coffee with a wand.
  break
  case 4
    say I can't believe the Tower Skycleave is finally here!
  break
done
~
#10031
Bustling Page~
0 bw 10
~
* This script is no longer used. It was replaced by custom strings.
switch %random.4%
  case 1
    say Gosh, let's see, I found the skystones for Celiya and that old book for Barrosh. Now where did I put the breakers for the Grand High Sorcerer?
  break
  case 2
    %echo% ~%self% looks like &%self%'s in a hurry.
  break
  case 3
    say Oh, muggle forks! I've ripped another shoe. Oh, well. Cobbellus!
    %echo% |%self% shoe stitches itself back together.
  break
  case 4
    %echo% ~%self% vanishes briefly and then reappears with an armful of scrolls.
  break
done
~
#10032
Barista passive~
0 bw 5
~
switch %random.4%
  case 1
    say Try the wine here. It's actually pressed by pixies!
    %echo% (type 'buy wine')
  break
  case 2
    say We import only the finest coffee!
    %echo% (type 'buy coffee')
    wait 1 sec
    %echo% ~%self% opens a tiny portal to summon materials...
    %echo% ~%self% retrieves a sack of coffee beans.
  break
  case 3
    say Is that getting cold, hon?
    %echo% ~%self% waves a gnarled wand and the coffee cups start steaming.
  break
  case 4
    say One cream and two pixy dust?
  break
done
~
#10033
Barista purchase~
0 c 0
buy~
set vnum -1
set cost 0
set named drink
if (!%arg%)
  %send% %actor% You can buy coffee for 5 coins or wine for 15 coins.
  halt
elseif coffee /= %arg%
  set vnum 10033
  set cost 5
  set named mug of coffee
elseif wine /= %arg%
  set vnum 10032
  set cost 15
  set named glass of wine
else
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
if !%actor.can_afford(%cost%)%
  %send% %actor% ~%self% tells you, 'You'll need %cost% coins to buy that.'
  halt
end
nop %actor.charge_coins(%cost%)%
%load% obj %vnum% %actor% inv
%send% %actor% You buy a %named% for %cost% coins.
%echoaround% %actor% ~%actor% buys a %named%.
~
#10034
Teacher passive~
0 bw 5
~
context %self.room.vnum%
if (%lesson_running%)
  halt
end
switch %random.4%
  case 1
    %echo% ~%self% draws diagrams of wand motions on the board.
  break
  case 2
    %echo% ~%self% splits into two people!
    wait 1 sec
    %echo% ~%self% vanishes in a puff of smoke!
    wait 1 sec
    %echo% The other ~%self% looks baffled.
  break
  case 3
    %echo% Type 'study' to purchase a lesson.
  break
  case 4
    say Flish and swick!
  break
done
~
#10035
Teacher study~
0 c 0
study~
if (%actor.skill(High Sorcery)% >= 50)
  %send% %actor% ~%self% tells you, 'There's nothing more I can teach you.'
  halt
end
if !(%actor.has_resources(10037,1)%)
  %send% %actor% You need a greater skystone to purchase a lesson.
  halt
end
set room_var %self.room%
context %room_var.vnum%
set lesson_running 1
global lesson_running
nop %actor.add_resources(10037,-1)%
%send% %actor% You trade a greater skystone for a lesson.
%echoaround% %actor% ~%actor% begins to study.
wait 1 sec
if (%actor.room% != %room_var%)
  halt
end
%send% %actor% ~%self% begins to teach you a lesson.
wait 3 sec
if (%actor.room% != %room_var%)
  halt
end
%send% %actor% ~%self% shows you some simple magical movements.
wait 3 sec
if (%actor.room% != %room_var%)
  halt
end
%send% %actor% ~%self% gives you a lesson on magical lore.
wait 3 sec
if (%actor.room% != %room_var%)
  halt
end
%send% %actor% ~%self% grab you by the head and streams magical thoughts into your mind!
wait 3 sec
if (%actor.room% != %room_var%)
  halt
end
%send% %actor% The lesson ends.
%echoaround% %actor% |%actor% lesson ends.
if (%actor.skill(High Sorcery)% < 50)
  nop %actor.gain_skill(High Sorcery,1)%
end
set lesson_running 0
global lesson_running
~
#10036
Skycleave Cashier list~
0 c 0
list~
%send% %actor% ~%self% sells:
%send% %actor%  - a skycleaver trinket ('buy trinket', 10 greater skystones)
%send% %actor%  - a skycleaver belt ('buy belt', 8 skystones)
%send% %actor%  - skycleaver gloves ('buy gloves', 5 skystones)
%send% %actor%  - skycleaver armor ('buy armor', 14 skystones)
%send% %actor%  - a skycleaver rucksack ('buy rucksack', 10 skystones)
%send% %actor%  - an iridescent blue iris ('buy iris', 2 skystones)
%send% %actor%  - a yellow lightning stone ('buy lightningstone', 2 skystones)
%send% %actor%  - a red bloodstone ('buy bloodstone', 2 skystones)
%send% %actor%  - a glowing green seashell ('buy seashell', 2 skystones)
~
#10037
Skycleave Cashier purchase~
0 c 0
buy~
set vnum -1
set cost 0
set currency skystones
set currency_vnum 10036
set named a thing
set keyw skycleaver
if (!%arg%)
  %send% %actor% Type 'list' to see what's available.
  halt
elseif belt /= %arg%
  set vnum 10038
  set cost 8
  set named a skycleaver belt
elseif gloves /= %arg%
  set vnum 10039
  set cost 5
  set named skycleaver gloves
elseif armor /= %arg%
  set vnum 10040
  set cost 14
  set named skycleaver armor
elseif rucksack /= %arg%
  set vnum 10041
  set cost 10
  set named a skycleaver rucksack
elseif iris /= %arg%
  set vnum 1206
  set cost 2
  set named an iridescent blue iris
  set keyw iris
elseif lightningstone /= %arg%
  set vnum 103
  set cost 2
  set named a yellow lightning stone
  set keyw lightning
elseif bloodstone /= %arg%
  set vnum 104
  set cost 2
  set named a red bloodstone
  set keyw bloodstone
elseif seashell /= %arg%
  set vnum 1300
  set cost 2
  set named a glowing green seashell
  set keyw seashell
elseif trinket /= %arg%
  set vnum 10079
  set cost 10
  set named a skycleaver trinket
  set currency greater skystones
  set currency_vnum 10037
else
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
if !%actor.has_resources(%currency_vnum%,%cost%)%
  %send% %actor% ~%self% tells you, 'You'll need %cost% %currency% to buy that.'
  halt
end
nop %actor.add_resources(%currency_vnum%,-%cost%)%
%load% obj %vnum% %actor% inv %actor.level%
%send% %actor% You buy %named% for %cost% %currency%.
%echoaround% %actor% ~%actor% buys %named%.
~
#10038
Goblin Wrangler passive~
0 bw 5
~
* This script is no longer used. It was replaced by custom strings.
switch %random.4%
  case 1
    %echo% ~%self% adjusts ^%self% wrangler pants.
  break
  case 2
    say We've got a real wild crop of goblins this time.
  break
  case 3
    %echo% ~%self% wraps ^%self% whip around the light fixture and swings back and forth across the room.
  break
  case 4
    say Sometimes I worry we'll have too many goblins to control.
  break
done
~
#10039
Goblin Wrangler combat~
0 k 10
~
switch %random.4%
  case 1
    slow %actor.name%
  break
  case 2
    sunshock %actor.name%
  break
  case 3
    %send% %actor% ~%self% shoots a bolt of lightning from the end of ^%self% whip!
    %echoaround% %actor% ~%self% shoots a bolt of lightning at ~%actor% from the end of ^%self% whip!
    %damage% %actor% 120 magical
  break
  case 4
    %load% mob 10039 ally
    makeuid goblin mob goblin
    if %goblin%
      %echo% ~%self% opens a cage and lets ~%goblin% out!
      %force% %goblin% %aggro% %actor%
    end
  break
done
~
#10040
Goblin combat~
0 k 10
~
if %random.3% < 3
  %send% %actor% ~%self% stabs you in the leg with a goblin shortsword!
  %echoaround% %actor% ~%self% stabs ~%actor% in the leg with a goblin shortsword!
  %damage% %actor% 20 physical
  %dot% %actor% 100 10 physical
else
  blind %actor.name%
end
~
#10041
Pixy race passive~
2 bw 100
~
context %instance.id%
if %pixy_race_running%
  * Check race running time for errors and kill it if it seems to be over
  if %last_race_time% && %last_race_time% + 600 < %timestamp%
    unset pixy_race_running
    unset pixy_race_wager
    unset last_race_time
  else
    * race already running
    return 0
    halt
  end
end
if %pixy_race_wager% && !%race_stage%
  if !%pixy_race_running% && %last_race_time% + 30 < %timestamp%
    %echo% Wagering time is over. The race is about to begin!
    unset pixy_race_wager
    set pixy_race_running 1
    global pixy_race_running
    set race_stage 0
    global race_stage
  elseif %random.3% == 3
    %echo% There's still time to wager! Type 'wager <1, 2, or 3> <amount>' to place a bet.
  end
  halt
end
if !%last_race_time% || %last_race_time% + 300 < %timestamp%
  %echo% The pixies are lining up to race! You can see #1 Caterkiller, #2 Needleknife,
  %echo% and #3 Marrowgnaw flutter up to the starting line. Place your wagers now
  %echo% by typing 'wager <1, 2, or 3> <amount>'.
  set pixy_race_wager 1
  global pixy_race_wager
  set last_race_time %timestamp%
  global last_race_time
end
~
#10042
Pixy wager~
2 c 0
wager~
context %instance.id%
if %pixy_race_running%
  %send% %actor% It's too late to wager! The pixy race is already running.
  halt
elseif !%pixy_race_wager%
  %send% %actor% The race isn't running right now.
  halt
elseif %actor.varexists(pixy_wager)% && %actor.varexists(pixy_choice)%
  %send% %actor% You have already wagered %actor.pixy_wager% on #%actor.pixy_choice%.
  halt
end
set pixy_choice %arg.car%
eval pixy_wager %arg.cdr% + 0
if !%arg% || !%pixy_choice% || !%pixy_wager% || (%pixy_choice% != 1 && %pixy_choice% != 2 && %pixy_choice% != 3)
  %send% %actor% Usage: wager <1, 2, or 3> <amount>
  halt
end
if !%actor.can_afford(%pixy_wager%)%
  %send% %actor% You can't seem to afford that wager.
  halt
end
remote pixy_wager %actor.id%
remote pixy_choice %actor.id%
nop %actor.charge_coins(%pixy_wager%)%
%send% %actor% You place a wager on #%pixy_choice%, for %pixy_wager% coins.
%echoaround% %actor% ~%actor% places a wager.
~
#10043
Pixy race~
2 bw 100
~
context %instance.id%
if !%pixy_race_running% || (%race_stage% && %race_stage% > 0)
  return 0
  halt
end
set race_stage 1
global race_stage
wait 1 sec
%echo% And they're off!
wait 3 sec
switch %random.3%
  case 1
    %echo% #1 Caterkiller takes an early lead as #2 Needleknife and #3 Marrowgnaw run into each other out of the gate!
    wait 4 sec
    %echo% Coming out of the first turn, #2 Needleknife is only a wing behind #1 Caterkiller, with #3 Marrowgnaw trailing by five lengths.
    wait 5 sec
    %echo% #2 Needleknife has passed #1 Caterkiller on the backstretch!
    wait 4 sec
    %echo% The pixies are entering the final turn. #3 Marrowgnaw is gaining on the leaders...
    wait 3 sec
    %echo% It's wing-and-wing as they come into the homestretch...
    wait 3 sec
    %echo% #1 Caterkiller has retaken the lead, but only by an inch...
  break
  case 2
    %echo% #2 Needleknife kicks #1 Caterkiller and takes an early lead, as #3 Marrowgnaw has some trouble out of the gate!
    wait 4 sec
    %echo% It's the first turn, and #3 Marrowgnaw has caught up with the other two.
    wait 5 sec
    %echo% #3 Marrowgnaw passes #1 Caterkiller, and, WOAH! Did you see that? #3 Marrowgnaw is in the lead!
    wait 4 sec
    %echo% #3 Marrowgnaw holds the lead through the final turn.
    wait 3 sec
    %echo% It's wing-and-wing as the pixies begin the homestretch...
    wait 3 sec
    %echo% #2 Needleknife looks to have taken the lead...
  break
  case 3
    %echo% #3 Marrowgnaw comes screaming out of the gate, as #1 Caterkiller and #2 Needleknife struggle to keep up.
    wait 4 sec
    %echo% It's the first turn, and #2 Needleknife looks like he's stabbed something into #3 Marrowgnaw's wing.
    wait 5 sec
    %echo% #2 Needleknife has passed #3 Marrowgnaw on the backstretch!
    wait 4 sec
    %echo% The pixies enter the final turn. #3 Marrowgnaw is back in the lead, barely...
    wait 3 sec
    %echo% It looks like #3 Marrowgnaw blew the turn...
    wait 3 sec
    %echo% #2 Needleknife and #1 Caterkiller are wing-and-wing, neck-and-neck, heading into the homestretch...
  break
done
wait 3 sec
%echo% #3 Marrowgnaw swoops in from behind. It's going to be a close race, folks.
wait 2 sec
%echo% It's going to be #2 Needleknife...
wait 10
%echo% No, it's #1 Caterkiller...
wait 10
%echo% It's anybody's race...
wait 10
set winner %random.3%
switch %winner%
  case 1
    %echo% It's #1 Caterkiller by less than an inch!
  break
  case 2
    %echo% It's #2 Needleknife! We have a winner!
  break
  case 3
    %echo% #3 Marrowgnaw has won the race!
  break
done
wait 1 sec
set ch %room.people%
while %ch%
  if %ch.varexists(pixy_choice)%
    if %ch.pixy_choice% == %winner%
      eval amt %ch.pixy_wager% * 3
      %send% %ch% Your pixy won! You earn %amt% coins!
      %echoaround% %ch% ~%ch% has won ^%ch% bet!
      nop %ch.give_coins(%amt%)%
    elseif %ch.varexists(pixy_choice)%
      %send% %ch% Your pixy lost the race.
    end
    rdelete pixy_choice %ch.id%
    rdelete pixy_wager %ch.id%
  end
  set ch %ch.next_in_room%
done
unset race_stage
unset pixy_race_running
~
#10044
2F Watcher combat~
0 k 5
~
%echo% ~%self% draws the Eye Sigil in the air!
%aoe% 75 magical
~
#10045
Apprentice passive~
0 bw 5
~
switch %random.4%
  case 1
    %echo% ~%self% casts a spell at a broomstick, which promptly splits into two broomsticks!
  break
  case 2
    %echo% ~%self% opens a tiny portal to summon materials...
    %echo% ~%self% retrieves a venomous skink.
    wait 1 sec
    %echo% The venomous skink scampers away.
  break
  case 3
    %echo% ~%self% casts a spell at an escaped pixy, which promptly splits into two pixies!
  break
  case 4
    %echo% ~%self% vanishes briefly and then reappears with an armful of venomous skinks.
    wait 1 sec
    %echo% The venomous skinks scamper away.
  break
done
wait 1 sec
%echo% ~%self% looks quite embarrassed.
~
#10046
Sorcerer combat~
0 k 5
~
if !%self.affect(foresight)%
  foresight
elseif !%actor.affect(colorburst)%
  colorburst
elseif !%actor.affect(slow)%
  slow
else
  sunshock
end
~
#10047
3F High Master combat~
0 k 5
~
if !%actor.affect(enervate)%
  enervate
else
  %send% %actor% ~%self% marks you with a piece of chalk. It burns!
  %echoaround% %actor% ~%self% marks ~%actor% with a piece of chalk. It looks like it burns!
  %dot% %actor% 50 20 magical
  %damage% %actor% 50 magical
end
~
#10048
Otherworlder guard passive~
0 bw 5
~
if %self.varexists(msg_pos)%
  eval msg_pos %self.msg_pos% + 1
else
  set msg_pos 1
end
switch %msg_pos%
  case 1
    say We found the otherworlders in a damaged ovaloid of some kind.
  break
  case 2
    say Only one of the otherworlders survived.
  break
  case 3
    say I would certainly like to find more otherworlders.
  break
  case 4
    say We brought the otherworlder here to study, but he got out of hand.
  break
done
if %msg_pos% >= 4
  set msg_pos 0
end
remote msg_pos %self.id%
~
#10049
Otherworlder prisoner passive~
0 bw 5
~
* This script is no longer used. It was replaced by custom strings.
%echo% ~%self% pulls at its chains and lets out a shout.
~
#10050
Otherworlder prisoner combat~
0 k 10
~
if !%actor.affect(disarm)%
  disarm
else
  bash
end
~
#10051
Lich passive~
0 bw 5
~
if %self.varexists(msg_pos)%
  eval msg_pos %self.msg_pos% + 1
else
  set msg_pos 1
end
switch %msg_pos%
  case 1
    %echo% ~%self% makes a skeleton dance like a puppet.
  break
  case 2
    %echo% ~%self% pours a jar of ash into a bone-shaped mold.
  break
  case 3
    %echo% ~%self% spews flies from ^%self% mouth. Luckily, they escape out the window.
  break
  case 4
    %echo% ~%self% lets out an unearthly groan.
  break
done
if %msg_pos% >= 4
  set msg_pos 0
end
remote msg_pos %self.id%
~
#10052
Lich combat~
0 k 10
~
if !%self.affect(3009)%
  foresight
elseif !%actor.affect(3035)%
  slow
else
  switch %random.4%
    case 1
      %send% %actor% ~%self% grabs you by the neck and you're both enveloped in a bright purple glow!
      %echoaround% %actor% ~%self% grabs ~%actor% by the neck and they're both enveloped in a bright purple glow!
      %heal% %self% health 50
      %damage% %actor% 75 magical
    break
    case 2
      %send% %actor% ~%self% stares into your eyes. You begin to wither!
      %echoaround% %actor% ~%self% stares into |%actor% eyes. ~%actor% seems to wither!
      dg_affect #10052 %actor% strength -2 10
    break
    case 3
      %echo% ~%self% raises ^%self% arms and the floor begins to rumble...
      wait 2 sec
      %echo% Bone hands rise from the floor and claw at you!
      %aoe% 50 physical
    break
    case 4
      %echo% ~%self% raises ^%self% arms and the floor begins to rumble...
      wait 2 sec
      %load% mob 10049 ally
      makeuid skelly mob skeleton
      if %skelly%
        %echo% ~%skelly% rises from the floor!
        %force% %skelly% %aggro% %actor%
      end
    break
  done
end
~
#10053
Skeleton combat~
0 k 10
~
bash
~
#10054
Shackled Ghost combat~
0 k 10
~
%send% %actor% ~%self% envelops you. You hear a terrible, soul-piercing scream!
%echoaround% %actor% ~%self% envelops ~%actor%, who lets out a terrible, soul-piercing scream!
dg_affect %actor% SLOW on 20
dg_affect %actor% IMMOBILIZED on 20
~
#10055
Celiya passive~
0 bw 5
~
if %self.varexists(msg_pos)%
  eval msg_pos %self.msg_pos% + 1
else
  set msg_pos 1
end
switch %msg_pos%
  case 1
    %echo% ~%self% sits in her chair.
  break
  case 2
    %echo% ~%self% stands up, puts a book in a glass case, and locks it.
  break
  case 3
    say I have a lithe, form-fitting backpack for sale -- just 5 greater skystones (type 'buy').
  break
  case 4
    %echo% ~%self% whistles a catchy tune.
  break
done
if %msg_pos% >= 4
  set msg_pos 0
end
remote msg_pos %self.id%
~
#10056
Celiya combat~
0 k 10
~
switch %random.3%
  case 1
    %echo% ~%self% begins to channel a spell. The silk of ^%self% robe begins to grow and stretch out toward you!
    wait 1 sec
    %echo% Pieces of silk fly at you, slicing at your skin, leaving you in intense pain!
    %aoe% 75 magical
  break
  case 2
    %echo% ~%self% begins to glow white...
    wait 2 sec
    heal self
  break
  case 3
    %send% %actor% ~%self% grabs you by the wrist, and you feel a deep pain in your bones!
    %echoaround% %actor% ~%self% grabs ~%actor% by the wrist, and ~%actor% writhes in pain!
    %damage% %actor% 20 magical
    %dot% %actor% 100 20 magical
  break
done
~
#10057
Celiya buy~
0 c 0
buy~
command: buy
set vnum 10066
set cost 5
set named a form-fitting backpack
set keyw backpack
if (!%arg%)
  %send% %actor% ~%self% sells %named% for %cost% greater skystones.
  %send% %actor% Type 'buy backpack' to purchase one.
  halt
elseif !(backpack /= %arg% || formfitting /= %arg%)
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
if !%actor.has_resources(10037,%cost%)%
  %send% %actor% ~%self% tells you, 'You'll need %cost% greater skystones to buy that.'
  halt
end
nop %actor.add_resources(10037,-%cost%)%
%load% obj %vnum% %actor% inv %actor.level%
%send% %actor% You buy %named% for %cost% greater skystones.
%echoaround% %actor% ~%actor% buys %named%.
~
#10058
Barrosh passive~
0 bw 5
~
if %self.varexists(msg_pos)%
  eval msg_pos %self.msg_pos% + 1
else
  set msg_pos 1
end
switch %msg_pos%
  case 1
    %echo% ~%self% stops concentrating for a moment, and several hovering objects fall.
  break
  case 2
    %echo% ~%self% stares out the window at the surrounding countryside.
  break
  case 3
    say I have an armored backpack for sale -- just 5 greater skystones (type 'buy').
  break
  case 4
    %echo% ~%self% shouts, 'Whoever keeps whistling, cut it out.'
  break
done
if %msg_pos% >= 4
  set msg_pos 0
end
remote msg_pos %self.id%
~
#10059
Barrosh combat~
0 k 10
~
switch %random.4%
  case 1
    %echo% ~%self% speaks an arcane phrase...
    set verify_target %actor.id%
    wait 1 sec
    if %verify_target% != %actor.id%
      halt
    end
    dg_affect %actor% STRENGTH -1 20
    %send% %actor% You feel weaker.
  break
  case 2
    %echo% ~%self% speaks an arcane phrase...
    set verify_target %actor.id%
    wait 1 sec
    if %verify_target% != %actor.id%
      halt
    end
    dg_affect %actor% INTELLIGENCE -1 20
    %send% %actor% You feel dumber.
  break
  case 3
    %echo% ~%self% begins to glow white...
    wait 2 sec
    heal self
  break
  case 4
    %echo% ~%self% mutters some arcane words...
    wait 1 sec
    %echo% There is a flash of intense light!
    set ch %self.room.people%
    while %ch%
      if %self.is_enemy(%ch%)%
        %send% %ch% You are blinded!
        dg_affect %ch% BLIND on 10
      end
      set ch %ch.next_in_room%
    done
  break
done
~
#10060
Barrosh buy~
0 c 0
buy~
set vnum 10065
set cost 5
set named an armored backpack
set keyw backpack
if (!%arg%)
  %send% %actor% ~%self% sells %named% for %cost% greater skystones.
  %send% %actor% Type 'buy backpack' to purchase one.
  halt
elseif !(backpack /= %arg% || armored /= %arg%)
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
if !%actor.has_resources(10037,%cost%)%
  %send% %actor% ~%self% tells you, 'You'll need %cost% greater skystones to buy that.'
  halt
end
nop %actor.add_resources(10037,-%cost%)%
%load% obj %vnum% %actor% inv %actor.level%
%send% %actor% You buy %named% for %cost% greater skystones.
%echoaround% %actor% ~%actor% buys %named%.
~
#10061
Knezz passive~
0 bw 5
~
if %self.varexists(msg_pos)%
  eval msg_pos %self.msg_pos% + 1
else
  set msg_pos 0
end
switch %msg_pos%
  case 1
    %echo% ~%self% reads some correspondence at ^%self% desk.
  break
  case 2
    %echo% ~%self% vanishes momentarily, and then reappears in new clothes.
  break
  case 3
    say I have a gigantic little bag for sale -- just 5 greater skystones (type 'buy').
  break
  case 4
    %echo% ~%self% pulls some bones out of a pile of ashes and begins tinkering with them.
  break
done
if %msg_pos% >= 4
  set msg_pos 0
end
remote msg_pos %self.id%
~
#10062
Knezz combat~
0 k 10
~
switch %random.4%
  case 1
    %echo% ~%self% seems to spark with electricity...
    wait 1 sec
    set actor %self.fighting%
    if !%actor%
      halt
    end
    %send% %actor% ~%self% unleashes a powerful torrent of lightning bolts at you!
    %echoaround% %actor% ~%self% unleashes a torrent of lightning bolts at ~%actor%!
    %damage% %actor% 150 magical
  break
  case 2
    %echo% ~%self% chants a poem you don't understand...
    set ch %self.room.people%
    while %ch%
      set next_ch %ch.next_in_room%
      if %self.is_enemy(%ch%)%
        %send% %ch% Your insides begin to burn!
        %dot% %ch% 50 10 magical
      end
      set ch %next_ch%
    done
  break
  case 3
    %echo% ~%self% begins to glow white...
    wait 2 sec
    heal self
  break
  case 4
    %echo% ~%self% chants a poem you don't understand...
    set ch %self.room.people%
    while %ch%
      if %self.is_enemy(%ch%)%
        %send% %ch% You feel like you're moving through mud!
        dg_affect %ch% SLOW on 20
      end
      set ch %ch.next_in_room%
    done
  break
done
~
#10063
Knezz buy~
0 c 0
buy~
set vnum 10067
set cost 5
set named a portable hole
set keyw portable
if (!%arg%)
  %send% %actor% ~%self% sells %named% for %cost% greater skystones.
  %send% %actor% Type 'buy hole' to purchase one.
  halt
elseif !(hole /= %arg% || portable /= %arg%)
  %send% %actor% They don't seem to sell '%arg%' here.
  halt
end
if !%actor.has_resources(10037,%cost%)%
  %send% %actor% ~%self% tells you, 'You'll need %cost% greater skystones to buy that.'
  halt
end
nop %actor.add_resources(10037,-%cost%)%
%load% obj %vnum% %actor% inv %actor.level%
%send% %actor% You buy %named% for %cost% greater skystones.
%echoaround% %actor% ~%actor% buys %named%.
~
#10064
Escaped experiment passive~
0 bw 5
~
* This script is no longer used. It was replaced by custom strings.
switch %random.3%
  case 1
    %echo% ~%self% flickers, as if &%self% isn't real!
  break
  case 2
    %echo% ~%self% seems to be growing...
  break
  case 3
    %echo% ~%self% seems to be growing spikes.
  break
done
~
#10065
Escaped experiment combat~
0 k 10
~
* Deliberately sends no messages -- mob grows stronger over time
switch %random.3%
  case 1
    dg_affect %self% STRENGTH 1 60
  break
  case 2
    dg_affect %self% TO-HIT 1 60
  break
  case 3
    dg_affect %self% RESIST-PHYSICAL 1 60
  break
done
~
#10066
Skystone combine~
1 c 2
combine~
if !%self.is_name(%arg%)%
  return 0
  halt
end
if !(%actor.has_resources(10036,5)%)
  %send% %actor% You need 5 skystones to combine.
  halt
end
%send% %actor% You combine 5 skystones into a greater skystone!
%echoaround% %actor% ~%actor% combines 5 skystones into a greater skystone!
%load% obj 10037 %actor% inv
nop %actor.add_resources(10036,-5)%
~
#10067
Greater skystone split~
1 c 2
split~
if !%self.is_name(%arg%)%
  return 0
  halt
end
%send% %actor% You split a greater skystone into 5 small skystones!
%echoaround% %actor% ~%actor% splits a greater skystone into 5 smaller skystones!
%load% obj 10036 %actor% inv
%load% obj 10036 %actor% inv
%load% obj 10036 %actor% inv
%load% obj 10036 %actor% inv
%load% obj 10036 %actor% inv
%purge% %self%
~
#10068
Tower Skycleave announcement~
2 ab 1
~
if %random.3% == 3
  %regionecho% %room% 100 The Tower Skycleave has appeared in the region %room.coords%.
end
~
#10079
Skycleaver Trinket teleport~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room_var %self.room%
* once per 60 minutes
if %actor.cooldown(10079)%
  %send% %actor% Your %cooldown.10079% is on cooldown.
  halt
end
set cycle 0
while %cycle% >= 0
  * Repeats until break
  set loc %instance.nearest_rmt(10030)%
  * Rather than setting error in 10 places, just assume there's an error and clear it if there isn't
  set error 1
  if %actor.fighting%
    %send% %actor% You can't use @%self% during combat.
  elseif %actor.position% != Standing
    %send% %actor% You need to be standing up to use @%self%.
  elseif !%actor.can_teleport_room%
    %send% %actor% You can't teleport out of here.
  elseif !%loc%
    %send% %actor% There is no valid location to teleport to.
  elseif %actor.aff_flagged(DISTRACTED)%
    %send% %actor% You are too distracted to use @%self%!
  else
    set error 0
  end
  * Doing this AFTER checking loc exists
  if !%actor.can_enter_instance(%loc%)%
    %send% %actor% The destination is too busy.
    set error 1
  end
  if %actor.room% != %room_var% || %self.carried_by% != %actor% || %error%
    if %cycle% > 0
      %send% %actor% @%self% sparks and fizzles.
      %echoaround% %actor% |%actor% trinket sparks and fizzles.
    end
    halt
  end
  switch %cycle%
    case 0
      %send% %actor% You touch @%self% and the glyphs carved into it light up...
      %echoaround% %actor% ~%actor% touches @%self% and the glyphs carved into it light up...
    break
    case 1
      %send% %actor% The glyphs on @%self% glow a deep blue and the light begins to envelop you!
      %echoaround% %actor% The glyphs on @%self% glow a deep blue and the light begins to envelop ~%actor%!
    break
    case 2
      %echoaround% %actor% ~%actor% vanishes in a flash of blue light!
      %teleport% %actor% %loc%
      %force% %actor% look
      %echoaround% %actor% ~%actor% appears in a flash of blue light!
      nop %actor.set_cooldown(10079, 3600)%
      nop %actor.cancel_adventure_summon%
      nop %self.bind(%actor%)%
      halt
    break
  done
  wait 5 sec
  eval cycle %cycle% + 1
done
%echo% Something went wrong.
~
$
