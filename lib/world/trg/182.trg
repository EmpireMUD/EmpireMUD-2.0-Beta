#18200
Atlas turtle board~
0 c 0
board~
eval helper %%actor.char_target(%arg%)%%
if %helper% != %self%
  return 0
  halt
end
eval room i18201
if !%instance.start%
  %echo% %self.name% disappears! Or... was it ever there in the first place?
  %purge% %self%
  halt
end
%echoaround% %actor% %actor.name% boards %self.name%.
%teleport% %actor% %room%
%echoaround% %actor% %actor.name% boards %self.name%.
%send% %actor% You board %self.name%.
%force% %actor% look
~
#18201
City turtle greet~
0 hw 100
~
wait 5
if %actor.room% != %self.room%
  * target left the room during the delay
  halt
end
%send% %actor% %self.name% looms large over the scenery. It's so large that it carries
%send% %actor% &0an inn on its back. (Type 'board tortoise' to climb onto the tortoise.)
~
#18202
City tortoise entry~
0 i 100
~
eval room %self.room%
%regionecho% %room% -7 The footfalls of %self.name% shake the earth as %self.heshe% moves to %room.coords%.
wait 5
%echo% %self.name% looms large over the scenery. It's so large that it carries
%echo% &0an inn on its back. (Type 'board tortoise' to climb onto the tortoise.)
~
#18203
City turtle load~
0 n 100
~
context %instance.id%
if !%instance.location%
  %purge% %self%
end
eval room %self.room%
if %room.template%==18200
  if %already_loaded_tortoise%
    %purge% %self%
    halt
  else
    eval already_loaded_tortoise 1
    global already_loaded_tortoise
  end
  mgoto %instance.location%
  eval room %self.room%
end
~
#18204
City turtle disembark~
2 c 0
disembark~
if %cmd.mudcommand% != disembark
  return 0
  halt
end
* go to the turtle
eval turtle %instance.mob(18200)%
eval target %turtle.room%
if %turtle%
  eval target %turtle.room%
else
  eval target %startloc%
  %adventurecomplete%
end
%send% %actor% You disembark from the turtle.
%echoaround% %actor% %actor.name% disembarks from the turtle.
%teleport% %actor% %target%
%force% %actor% look
%echoaround% %actor% %actor.name% disembarks from the turtle.
~
#18205
City turtle look out~
2 c 0
look~
if %cmd.mudcommand% == look && out == %arg%
  %send% %actor% Looking over the side of the turtle, you see...
  eval turtle %instance.mob(18200)%
  if %turtle%
    eval target %turtle.room%
  else
    eval target %startloc%
    %adventurecomplete%
  end
  %teleport% %actor% %target%
  %force% %actor% look
  %teleport% %actor% %room%
  return 1
  halt
end
return 0
~
#18206
Tavern: Exit + Out + Leave~
1 c 4
exit out leave~
%force% %actor% enter door
~
#18207
Tavern Patron Spawner~
1 n 100
~
set message $n leaves.
eval room %self.room%
* Common patron
%purge% instance mob 18201 %message%
eval current_vnum 18236
* Weird patrons
while %current_vnum% <= 18240
  %purge% instance mob %current_vnum% %message%
  eval current_vnum %current_vnum% + 1
done
* Spawn new patrons
eval weird_patron_vnum 18236
eval weird_patron_vnum (%weird_patron_vnum% - 1) + %random.4%
%load% mob 18201
eval person %room.people%
%echo% %person.name% arrives.
%load% mob %weird_patron_vnum%
eval person %room.people%
%echo% %person.name% arrives.
%purge% %self%
~
#18210
Fight Club death walk~
0 f 100
~
if %random.5% == 5
  say Oww, shell shock.
end
%echo% %self.name% walks away from the mat.
return 0
~
#18211
Fight Club spawner~
2 bg 50
~
wait 5
eval ch %room.people%
eval found_turtles 0
eval found_mooks 0
eval last_mook 0
while %ch%
  if !%ch.is_pc%
    if (%ch.vnum% == 18204 || %ch.vnum% == 18205 || %ch.vnum% == 18206 || %ch.vnum% == 18207)
      eval found_turtles %found_turtles% + 1
    elseif (%ch.vnum% == 18203)
      eval found_mooks %found_mooks% + 1
      eval last_mook %ch%
    end
  end
  eval ch %ch.next_in_room%
done
if (%found_turtles% > 0)
  halt
end
switch %found_mooks%
  case 4
    %load% mob 18204
    %echo% Leon steps up to the mat.
  break
  case 3
    %load% mob 18205
    %echo% Raffee steps up to the mat.
  break
  case 2
    %load% mob 18206
    %echo% Donnet steps up to the mat.
  break
  case 1
    %load% mob 18207
    %echo% Meek Langolo steps up to the mat.
  break
done
if %last_mook%
  %purge% %last_mook%
end
~
#18212
Mob block higher template id - faction reputation Liked~
0 s 100
~
* One quick trick to get the target room
eval room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
eval test %%actor.has_reputation(%self.allegiance%,Liked)%%
if %test%
  %send% %actor% %self.name% lets you pass.
  return 1
  halt
end
%send% %actor% %self.name% won't let you past! You must be Liked by %self.faction_name%!
return 0
~
#18213
Adventurer guild cornucopia reset~
2 f 100
~
eval item_vnum 18204
* Find the existing item
eval object %room.contents%
while %object%
  eval next_obj %object.next_in_list%
  if %object.vnum% == %item_vnum%
    %purge% %object%
  end
  eval object %next_obj%
done
%load% obj 18204
eval object %room.contents%
%echo% %object.shortdesc% is refreshed!
~
#18214
Tortoise Trinket teleporter~
1 c 2
use~
eval test %%actor.obj_target(%arg%)%%
if %test% != %self%
  return 0
  halt
end
eval room_var %self.room%
* once per 60 minutes
if %actor.cooldown(18214)%
  %send% %actor% Your %cooldown.18214% is on cooldown.
  halt
end
eval cycle 0
while %cycle% >= 0
  * Repeats until break
  eval loc %instance.nearest_rmt(18201)%
  * Rather than setting error in 10 places, just assume there's an error and clear it if there isn't
  eval error 1
  if %actor.fighting%
    %send% %actor% You can't use %self.name% during combat.
  elseif %actor.position% != Standing
    %send% %actor% You need to be standing up to use %self.name%.
  elseif !%actor.can_teleport_room%
    %send% %actor% You can't teleport out of here.
  elseif !%loc%
    %send% %actor% There is no valid location to teleport to.
  elseif %actor.aff_flagged(DISTRACTED)%
    %send% %actor% You are too distracted to use %self.shortdesc%!
  else
    eval error 0
  end
  * Doing this AFTER checking loc exists
  eval limit_check %%actor.can_enter_instance(%loc%)%%
  if !%limit_check%
    %send% %actor% The destination is too busy.
    eval error 1
  end
  if %actor.room% != %room_var% || %self.carried_by% != %actor% || %error%
    if %cycle% > 0
      %send% %actor% %self.shortdesc% sparks and fizzles.
      %echoaround% %actor% %actor.name%'s trinket sparks and fizzles.
    end
    halt
  end
  switch %cycle%
    case 0
      %send% %actor% You touch %self.shortdesc% and it begins to rumble...
      %echoaround% %actor% %actor.name% touches %self.shortdesc% and it begins to rumble...
    break
    case 1
      %send% %actor% %self.shortdesc% glows a deep green and the light surrounds you like a shell!
      %echoaround% %actor% %self.shortdesc% glows a deep green and the light surrounds %actor.name% like a shell!
    break
    case 2
      %echoaround% %actor% %actor.name% vanishes in a flash of green light!
      %teleport% %actor% %loc%
      %force% %actor% look
      %echoaround% %actor% %actor.name% appears in a flash of green light!
      nop %actor.set_cooldown(18214, 3600)%
      nop %actor.cancel_adventure_summon%
      halt
    break
  done
  wait 5 sec
  eval cycle %cycle% + 1
done
%echo% Something is wrong.
~
#18216
Adventurer's Guildhall~
2 o 100
~
* Add basement
eval basement %%room.down(room)%%
if !%basement%
  %door% %room% down add 18218
end
* Add tower
eval tower %%room.up(room)%%
if !%tower%
  %door% %room% up add 18217
end
* Add office
eval office %%room.%room.enter_dir%(room)%%
if !%office%
  %door% %room% %room.enter_dir% add 18219
end
eval office %%room.%room.enter_dir%(room)%%
if !%office%
  * Failed to add
  halt
end
* Add vault
eval edir %room.bld_dir(east)%
eval vault %%office.%edir%(room)%%
if !%vault%
  %door% %office% %edir% add 18220
end
detach 18216 %room.id%
~
#18217
Give Adventurer Guild Charter~
2 u 100
~
%load% obj 18216 %actor%
~
#18219
Adventuring guild block vault~
0 s 100
~
* One quick trick to get the target room
eval room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
eval to_room %tricky%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky%)
  halt
end
* debug
if %room_var.vnum% != 18129 || %to_room.vnum% != 18130
  halt
end
* stealth prereq
if %actor.skill(Stealth)% > 50
  * alone prereq
  eval found 0
  eval person %room_var.people%
  while %person%
    if %person.is_pc% && (%person% != %actor%)
      eval found %person%
    end
    eval person %person.next_in_room%
  done
  if !%found%
    %send% %actor% %self.name% lets you pass.
    return 1
    halt
  else
    %send% %actor% %self.name% leans in and whispers, 'Come back later, without %found.name%.'
  end
end
%send% %actor% %self.name% tells you, 'Hey! You can't go down there!'
return 0
~
#18220
Egg Timeout~
1 f 0
~
%build% %self.room% demolish
~
#18221
Atlasian egg fake plant~
1 c 2
plant~
eval targ %%actor.obj_target(%arg%)%%
if %targ% != %self%
  return 0
  halt
end
eval room %actor.room%
if (%room.sector% != Plains && %room.sector% != Desert) || %room.building%
  %send% %actor% You can't plant %self.shortdesc% here.
  return 1
  halt
end
if !%actor.on_quest(18221)%
  %send% %actor% You must start the egg's quest before you can plant it.
  return 1
  halt
end
eval check %%actor.canuseroom_member(%room%)%%
if !%check%
  %send% %actor% You don't have permission to use %self.shortdesc% here.
  return 1
  halt
end
%send% %actor% You dig a hole in the ground large enough to fit %self.shortdesc%...
%send% %actor% You carefully nestle the egg into the hole, and brush the dirt off of it.
%echoaround% %actor% %actor.name% digs a hole in the ground and places %self.shortdesc% in it.
%load% obj 18222 room
%build% %room% 18221
%quest% %actor% finish 18221
%purge% %self%
~
#18222
Atlasian Egg growth stage 2~
2 v 100
~
if !(%room.building% ~= Egg)
  %echo% Something went wrong while growing the egg...
  return 0
  halt
end
return 1
%echo% The egg seems to grow as the burnt zephyr salve works its magic!
%build% %room% 18222
~
#18223
Atlasian Egg growth stage 3~
2 v 100
~
if !(%room.building% ~= Egg)
  %echo% Something went wrong while growing the egg...
  return 0
  halt
end
return 1
%echoaround% %actor% %actor.name% builds a magewood fire around the egg, and lights it!
eval obj %room.contents%
while %obj%
  eval next_obj %obj.next_in_list%
  if (%obj.vnum% == 18222)
    %purge% %obj%
  end
  eval obj %next_obj%
done
%load% obj 18223
%build% %room% 18223
~
#18224
Tortoise Vehicle Setup~
5 n 100
~
eval inter %self.interior%
if %inter%
  * add house
  if (!%inter.aft%)
    %door% %inter% aft add 18225
  end
  eval house %inter.aft(room)%
  if %house%
    * add bedroom
    if (!%house.starboard%)
      %door% %house% starboard add 18226
    end
    * add observatory
    if (!%house.up%)
      %door% %house% up add 18227
    end
  end
end
detach 18224 %self.id%
~
#18225
Atlasian Turtle Egg: Hatch~
1 f 0
~
eval room %self.room%
%load% veh 18224
eval tortoise %room.vehicles%
%echo% The huge atlasian egg cracks open, and %tortoise.shortdesc% pokes its head out!
%own% %tortoise% %room.empire%
%build% %room% demolish
return 0
%purge% %self%
~
#18238
Consider / Kill Death~
0 c 0
consider kill~
* Target check
eval target %%actor.char_target(%arg%)%%
if %target% != %self%
  return 0
  halt
end
if consider /= %cmd%
  %send% %actor% You consider your chances against %self.name%.
  %echoaround% %actor% %actor.name% considers %actor.hisher% chances against %self.name%.
  %send% %actor% %self.heshe% looks like %self.heshe%'d destroy you!
  return 1
  halt
else
  %send% %actor% You consider attacking %self.name%, then think better of it.
  return 1
  halt
end
~
#18240
Adventurer Guild mount requires Liked~
0 c 0
mount ride~
* Sanity check
* I don't know why we'd have a mount called 'swap' but you never know
if %arg.car% == list || %arg.car% == swap ||  %arg.car% == release
  return 0
  halt
end
* Target check
eval target %%actor.char_target(%arg%)%%
if %target% != %self%
  return 0
  halt
end
eval rep_check %actor.has_reputation(18200, Liked)%
if !%rep_check%
  %send% %actor% You must be at least Liked by the Adventurer's Guild to ride %self.name%.
  return 1
  halt
end
return 0
~
#18241
Adventurer Guild mount requires Venerated~
0 c 0
mount ride~
* Sanity check
* I don't know why we'd have a mount called 'swap' but you never know
if %arg.car% == list || %arg.car% == swap ||  %arg.car% == release
  return 0
  halt
end
* Target check
eval target %%actor.char_target(%arg%)%%
if %target% != %self%
  return 0
  halt
end
eval rep_check %actor.has_reputation(18200, Venerated)%
if !%rep_check%
  %send% %actor% You must be at least Venerated by the Adventurer's Guild to ride %self.name%.
  return 1
  halt
end
return 0
~
#18256
Catch wildling with a huge net~
1 c 3
net~
if !%arg%
  %send% %actor% What do you want to catch with %self.shortdesc%?
  return 1
  halt
end
eval target %%actor.char_target(%arg%)%%
if !%target%
  %send% %actor% They must have ran away when you started waving %self.shortdesc% around, because they're not here.
  return 1
  halt
end
if %target.is_npc% && %target.vnum% == 10000
  if %actor.inventory(18257)%
    %send% %actor% You don't need to catch any more wildlings.
    return 1
    halt
  end
  %send% %actor% You throw %self.shortdesc% over %target.name% and capture %target.himher%!
  %echoaround% %actor% %actor.name% throws %self.shortdesc% over %target.name% and captures %target.himher%!
  %purge% %target%
  %load% obj 18257 %actor% inv
  return 1
  %purge% %self%
  halt
elseif %target.is_npc% && %target.vnum% == 10005
  %send% %actor% That wildling is tame. Catching it in a net would be pointless and wasteful.
  return 1
  halt
elseif %target.is_pc%
  if %target% == %actor%
    %send% %actor% You briefly ponder catching yourself with %self.shortdesc%, then think better of it.
    return 1
    halt
  end
  %send% %actor% You advance menacingly on %target.name% with %self.shortdesc%...
  %send% %target% %actor.name% advances menacingly on you with a huge net...
  %echoneither% %actor% %target% %actor.name% advances menacingly on %target.name% with a huge net...
  return 1
  halt
else
  %send% %actor% You're supposed to catch a wildling with that, not %target.name%.
  return 1
  halt
end
~
#18257
Give wildling net on quest start~
2 u 100
~
%load% obj 18256 %actor% inv
~
#18258
Adventurer Guild, Monsoon Attunement: Speak to Druid~
0 c 0
speak~
if %actor.inventory(18258)%
  %send% %actor% You have already talked to %self.name%.
  return 1
  halt
end
if !(%actor.on_quest(18258)%
  %send% %actor% You don't need to talk to anyone here right now.
  return 1
  halt
end
eval room %actor.room%
eval cycles_left 5
while %cycles_left% >= 0
  if (%actor.room% != %room%) || %actor.fighting% || %actor.disabled%
    * We've either moved or the room's no longer suitable for the action
    if %cycles_left% < 5
      %echoaround% %actor% %actor.name%'s conversation is interrupted.
      %send% %actor% Your conversation is interrupted.
    else
      * combat, stun, sitting down, etc
      %send% %actor% You can't do that now.
    end
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echoaround% %actor% %actor.name% starts talking to %self.name%...
      %send% %actor% You greet the druid and ask %self.himher% about the rift...
    break
    case 4
      say We druids open these rifts wherever the desert is oppressed by the advance of the cities of mankind.
    break
    case 3
      say As we quench the desert with our magical rain, the plants themselves take up arms to defend their homes.
    break
    case 2
      say When we have roused enough of the cacti, nature itself will rise up against the nearby city and return it to the dust.
    break
    case 1
      say As you can see, the work we are doing here is important. Let me find you a pamphlet so I can get back to work.
    break
    case 0
      %echoaround% %actor% %self.name% fishes a pamphlet out of %self.hisher% robe and gives it to %actor.name%.
      %send% %actor% You finish your conversation with %self.name%, who gives you a pamphlet from %self.hisher% robe.
      * Quest complete
      %load% obj 18258 %actor% inv
      %send% %actor% You must still find a way to close the monsoon rift to finish your quest.
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
~
#18259
Monsoon quest finish: trigger adventurer guild quest~
2 v 100
~
* Have we already talked to the druid?
if %actor.on_quest(18258)% && %actor.inventory(18258)%
  %quest% %actor% trigger 18258
end
if %actor.on_quest(18268)%
  %quest% %actor% trigger 18268
end
~
#18260
Buy goblin gala ticket~
1 c 2
buy~
eval room %self.room%
eval person %room.people%
eval found 0
while %person%
  if %person.vnum% == 10451
    eval found 1
  end
  eval person %person.next_in_room%
done
if !%found%
  return 0
  halt
end
eval vnum -1
set named a thing
if (!%arg%)
  return 0
  halt
elseif ticket /= %arg%
  eval vnum 18261
  set named a goblin gala ticket
  if %actor.inventory(18261)%
    %send% %actor% You already have one!
    halt
  end
else
  return 0
  halt
end
if !%actor.can_afford(50)%
  %send% %actor% %self.name% tells you, 'Human needs 50 coin to buy that.'
  halt
end
nop %actor.charge_coins(50)%
%load% obj %vnum% %actor% inv
%send% %actor% You buy %named% for 50 coins.
%echoaround% %actor% %actor.name% buys %named%.
~
#18261
Give quest start items~
2 u 100
~
if %questvnum% == 18260
  %load% obj 18260 %actor% inv
elseif %questvnum% == 18272
  %load% obj 18272 %actor% inv
elseif %questvnum% == 18270
  set guild_siphoned_10551 0
  set guild_siphoned_10552 0
  remote guild_siphoned_10551 %actor.id%
  remote guild_siphoned_10552 %actor.id%
  %load% obj 18270 %actor% inv
end
~
#18270
Frost Siphon: use~
1 c 2
use~
eval test %%actor.obj_target(%arg.car%)%%
if %test% != %self%
  return 0
  halt
end
eval target %actor.char_target(%arg.cdr%)%%
if !%target%
  %send% %actor% You don't see a '%arg.cdr%' here.
  halt
end
if %target.vnum% != 10551 && %target.vnum% != 10552
  %send% %actor% That's not a valid target for %self.shortdesc%.
  return 1
  halt
end
set valid 1
eval check_var %%actor.varexists(guild_siphoned_%target.vnum%)%%
if %check_var%
  eval check %%actor.guild_siphoned_%target.vnum%%%
  if %check%
    %send% %actor% You've already siphoned energy from %target.name%.
    set valid 0
  end
end
if %valid%
  %send% %actor% You siphon energy from %target.name%...
  set guild_siphoned_%target.vnum% 1
  remote guild_siphoned_%target.vnum% %actor.id%
end
* Check quest completion
if %actor.guild_siphoned_10551% && %actor.guild_siphoned_10552%
  %send% %actor% You have siphoned both apprentices.
  %quest% %actor% trigger 18270
end
~
#18272
Fake pickpocket~
1 c 2
pickpocket~
eval target %%actor.char_target(%arg%)%%
if !%target%
  * Invalid target
  return 0
  halt
end
if (%target.vnum% != 228 && %target.vnum% != 237)
  * Not an empire sorcerer
  return 0
  halt
end
if %actor.inventory(18210)% || !%actor.on_quest(18272)%
  * Don't need the trinket
  return 0
  halt
end
* 100% chance
if 0
  %send% %actor% This must have been the wrong sorcerer.
  return 0
  halt
else
  %send% %actor% You pick %target.name%'s pocket...
  %load% obj 18210 %actor% inv
  eval item %actor.inventory()%
  %send% %actor% You find %item.shortdesc%!
  %actor.add_resources(18272, -1)%
  return 1
  halt
end
~
$
