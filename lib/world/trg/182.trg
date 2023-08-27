#18200
Atlas turtle board~
0 c 0
board~
if %actor.char_target(%arg%)% != %self%
  return 0
  halt
end
set room i18201
if !%instance.start%
  %echo% ~%self% disappears! Or... was it ever there in the first place?
  %purge% %self%
  halt
end
%echoaround% %actor% ~%actor% boards ~%self%.
%teleport% %actor% %room%
%echoaround% %actor% ~%actor% boards ~%self%.
%send% %actor% You board ~%self%.
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
%send% %actor% ~%self% looms large over the scenery. It's so large that it carries
%send% %actor% &&0an inn on its back. (Type 'board tortoise' to climb onto the tortoise.)
~
#18202
Atlasian Tortoise location updater~
0 i 100
~
wait 1
set room %self.room%
nop %instance.set_location(%room%)%
%regionecho% %room% -7 The footfalls of ~%self% shake the earth as &%self% moves to %room.coords%.
wait 5
%echo% ~%self% looms large over the scenery. It's so large that it carries
%echo% &&0an inn on its back. (Type 'board tortoise' to climb onto the tortoise.)
~
#18203
City turtle load~
0 n 100
~
context %instance.id%
if !%instance.real_location%
  %purge% %self%
end
if %self.room.template%==18200
  if %already_loaded_tortoise%
    %purge% %self%
    halt
  else
    set already_loaded_tortoise 1
    global already_loaded_tortoise
  end
  mgoto %instance.real_location%
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
set turtle %instance.mob(18200)%
set target %turtle.room%
if %turtle%
  set target %turtle.room%
else
  set target %startloc%
  %adventurecomplete%
end
%send% %actor% You disembark from the turtle.
%echoaround% %actor% ~%actor% disembarks from the turtle.
%teleport% %actor% %target%
%force% %actor% look
%echoaround% %actor% ~%actor% disembarks from the turtle.
~
#18205
City turtle look out~
2 c 0
look~
if %cmd.mudcommand% == look && out == %arg%
  %send% %actor% Looking over the side of the turtle, you see...
  set turtle %instance.mob(18200)%
  if %turtle%
    set target %turtle.room%
  else
    set target %startloc%
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
set room %self.room%
* Common patron
%purge% instance mob 18201 %message%
set current_vnum 18236
* Weird patrons
while %current_vnum% <= 18240
  %purge% instance mob %current_vnum% %message%
  eval current_vnum %current_vnum% + 1
done
* Spawn new patrons
set weird_patron_vnum 18236
eval weird_patron_vnum (%weird_patron_vnum% - 1) + %random.4%
%load% mob 18201
set person %room.people%
%echo% ~%person% arrives.
%load% mob %weird_patron_vnum%
set person %room.people%
%echo% ~%person% arrives.
%purge% %self%
~
#18208
Seeker Stone: Atlasian Tortoise~
1 c 2
seek~
if !%arg%
  %send% %actor% Seek what?
  halt
end
set room %self.room%
if %room.rmt_flagged(!LOCATION)%
  %send% %actor% @%self% spins gently in a circle.
  halt
end
if (guild /= %arg% || tortoise /= %arg%)
  set adv %instance.nearest_rmt(18200)%
  if !%adv%
    %send% %actor% Could not find an instance.
    halt
  end
  set room %self.room%
  * Teleport to the instance, find the turtle, teleport back
  %teleport% %actor% %adv%
  set turtle %instance.mob(18200)%
  %teleport% %actor% %room%
  if !%turtle%
    %send% %actor% Something went wrong (turtle not found).
    halt
  end
  %send% %actor% You hold @%self% aloft...
  set real_dir %room.direction(%turtle.room%)%
  set distance %room.distance(%turtle.room%)%
  if %distance% == 0
    %send% %actor% There is an atlasian tortoise in the room with you.
  elseif %distance% == 1
    %send% %actor% There is an atlasian tortoise %distance% tile to the %actor.dir(%real_dir%)%.
  else
    %send% %actor% There is an atlasian tortoise %distance% tiles to the %actor.dir(%real_dir%)%.
  end
  %echoaround% %actor% ~%actor% holds @%self% aloft...
else
  return 0
  halt
end
~
#18209
Seek Adventure: Give Seeker Stone~
2 u 0
~
%load% obj 18208 %actor% inv
~
#18210
Fight Club death walk~
0 f 100
~
if %random.5% == 5
  say Oww, shell shock.
end
%echo% ~%self% walks away from the mat.
return 0
~
#18211
Fight Club spawner~
2 bg 50
~
wait 5
set ch %room.people%
set found_turtles 0
set found_mooks 0
set last_mook 0
while %ch%
  if !%ch.is_pc%
    if (%ch.vnum% == 18204 || %ch.vnum% == 18205 || %ch.vnum% == 18206 || %ch.vnum% == 18207)
      eval found_turtles %found_turtles% + 1
    elseif (%ch.vnum% == 18203)
      eval found_mooks %found_mooks% + 1
      set last_mook %ch%
    end
  end
  set ch %ch.next_in_room%
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
set room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky% || %tricky.template% < %room_var.template%)
  halt
end
if %actor.has_reputation(%self.allegiance%,Liked)%
  %send% %actor% ~%self% lets you pass.
  return 1
  halt
end
%send% %actor% ~%self% won't let you past! You must be Liked by %self.faction_name%!
return 0
~
#18213
Adventurer guild cornucopia reset~
2 f 100
~
set item_vnum 18204
* Find the existing item
set object %room.contents%
while %object%
  set next_obj %object.next_in_list%
  if %object.vnum% == %item_vnum%
    %purge% %object%
  end
  set object %next_obj%
done
%load% obj 18204
set object %room.contents%
%echo% @%object% is refreshed!
~
#18214
Tortoise Trinket teleporter~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room_var %self.room%
* once per 60 minutes
if %actor.cooldown(18214)%
  %send% %actor% Your %cooldown.18214% is on cooldown.
  halt
end
set cycle 0
while %cycle% >= 0
  * Repeats until break
  set loc %instance.nearest_rmt(18201)%
  * Rather than setting error in 10 places, just assume there's an error and clear it if there isn't
  set error 1
  if %actor.fighting%
    %send% %actor% You can't use ~%self% during combat.
  elseif %actor.position% != Standing
    %send% %actor% You need to be standing up to use ~%self%.
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
      %send% %actor% You touch @%self% and it begins to rumble...
      %echoaround% %actor% ~%actor% touches @%self% and it begins to rumble...
    break
    case 1
      %send% %actor% @%self% glows a deep green and the light surrounds you like a shell!
      %echoaround% %actor% @%self% glows a deep green and the light surrounds ~%actor% like a shell!
    break
    case 2
      %echoaround% %actor% ~%actor% vanishes in a flash of green light!
      %teleport% %actor% %loc%
      %force% %actor% look
      %echoaround% %actor% ~%actor% appears in a flash of green light!
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
#18215
GoA Start Progression~
2 g 100
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(18200)%
end
~
#18216
Adventurer's Guildhall Complete~
2 o 100
~
* Add basement
if !%room.down(room)%
  %door% %room% down add 18218
end
* Add tower
if !%room.up(room)%
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
set edir %room.bld_dir(east)%
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
#18218
Guildhall mob out of city despawn~
0 n 100
~
if !%self.room.in_city(true)%
  %purge% %self% $n leaves because this guildhall isn't in an active city.
else
  visible
end
~
#18219
Adventuring guild block vault~
0 s 100
~
* One quick trick to get the target room
set room_var %self.room%
eval tricky %%room_var.%direction%(room)%%
* Compare template ids to figure out if they're going forward or back
if (%actor.nohassle% || !%tricky%)
  halt
end
if %tricky.building% != Guildhall Vault
  halt
end
* stealth prereq
if %actor.skill(Stealth)% > 50
  * alone prereq
  set found 0
  set person %room_var.people%
  while %person%
    if %person.is_pc% && (%person% != %actor%)
      set found %person%
    end
    set person %person.next_in_room%
  done
  if !%found%
    %send% %actor% ~%self% lets you pass.
    return 1
    halt
  else
    %send% %actor% ~%self% leans in and whispers, 'Come back later, without ~%found%.'
  end
end
%send% %actor% ~%self% tells you, 'Hey! You can't go in there!'
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
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %actor.room%
if (%room.sector% != Plains && %room.sector% != Desert) || %room.building%
  %send% %actor% You can't plant @%self% here.
  return 1
  halt
end
if !%actor.on_quest(18221)%
  %send% %actor% You must start the egg's quest before you can plant it.
  return 1
  halt
end
if !%actor.canuseroom_member(%room%)%
  %send% %actor% You don't have permission to use @%self% here.
  return 1
  halt
end
%send% %actor% You dig a hole in the ground large enough to fit @%self%...
%send% %actor% You carefully nestle the egg into the hole, and brush the dirt off of it.
%echoaround% %actor% ~%actor% digs a hole in the ground and places @%self% in it.
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
%echoaround% %actor% ~%actor% builds a magewood fire around the egg, and lights it!
set obj %room.contents%
while %obj%
  set next_obj %obj.next_in_list%
  if (%obj.vnum% == 18222)
    %purge% %obj%
  end
  set obj %next_obj%
done
%load% obj 18223
%build% %room% 18223
~
#18224
Tortoise Vehicle Setup~
5 n 100
~
set inter %self.interior%
if %inter%
  * add house
  if (!%inter.aft%)
    %door% %inter% aft add 18225
  end
  set house %inter.aft(room)%
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
set room %self.room%
%load% veh 18224
set tortoise %room.vehicles%
%echo% The huge atlasian egg cracks open, and @%tortoise% pokes its head out!
%own% %tortoise% %room.empire%
%build% %room% demolish
return 0
%purge% %self%
~
#18226
random guild adventurer description~
0 n 100
~
switch %random.4%
  case 1
    %mod% %self% longdesc %self.name% is looking for a place to setup %self.hisher% lab tent.
    %mod% %self% lookdesc The shoes of this adventurer are completely covered in muck and other unspeakable things from the city's sewers where the local goblin outpost is located.
    %mod% %self% append-lookdesc Seems as though %self.heshe% had some luck however, as a goblin made dagger rests in a sheath on %self.hisher% hip and a bundled lab tent is over %self.hisher% shoulder.
  break
  case 2
    %mod% %self% longdesc Muck slowly drips from %self.name%'s winged armor.
    %mod% %self% lookdesc The shoes of this adventurer are completely covered in muck and other unspeakable things from the city's sewers where %self.heshe% has been hunting rats and other denizens of those smelly tunnels.
    %mod% %self% append-lookdesc It seems %self.heshe% might have found a young dragon down there as well, given the winged armor containing %self.hisher% torso.
  break
  case 3
    %mod% %self% longdesc %self.name% looks a little burnt but happy as %self.heshe% walks around.
    %mod% %self% lookdesc Even with this adventurer's obvious low to mid quality gear, you can see %self.heshe%'s been rather busy hunting the wandering dragons of the world.
    %mod% %self% append-lookdesc With slightly scorched armor, weapon, and hair %self.heshe% is still grinning as %self.heshe% sports a reasonable collection of dragon scales.
  break
  case 4
    %mod% %self% longdesc %self.name% wanders along in a scent cloud of burning bodies.
    %mod% %self% lookdesc The scent of burning bodies clings to this adventurer as %self.heshe% walks about.
    %mod% %self% append-lookdesc At first it isn't clear just where that offensive scent would have come from, but then %self.heshe% plucks a Necrofiend's spike from %self.hisher% backside with a wince.
  break
done
~
#18227
GoA: Use smoke bomb~
1 c 2
use~
* Flushes out a Goblin Challenge
return 1
* basic errors first
if !%arg% || %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
elseif %actor.fighting%
  %send% %actor% You're a bit busy right now!
  halt
elseif %actor.position% != Standing && %actor.position% != Resting && %actor.position% != Sitting
  %send% %actor% You need to get up first.
  halt
end
* try to do the thing
set ok 0
set room %actor.room%
* remove goblins
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.vnum% >= 10200 && %ch.vnum% <= 10205
    if !%ok%
      %send% %actor% You throw down the smoke bomb... there's a putrid smell as green gas fills the nest!
      %echoaround% %actor% ~%actor% throws down a smoke bomb... there's a putrid smell as green as fills the nest!
      set ok 1
    end
    if %ch.vnum% == 10200 || %ch.vnum% == 10204
      %regionecho% %room% 15 &&y&&Z~%ch% yells, 'We's going to get you next time!'&&0
    elseif %ch.vnum% == 10201 || %ch.vnum% == 10205
      %regionecho% %room% 15 &&y&&Z~%ch% yells, 'You hasn't seen the last of us!'&&0
    elseif %ch.vnum% == 10202 || %ch.vnum% == 10203
      %regionecho% %room% 15 &&y&&Z~%ch% yells, 'Aaaaaaaaaahhhh!!!'&&0
    end
    %echo% ~%ch% runs screaming from the nest!
    %purge% %ch%
  end
  set ch %next_ch%
done
* remove bell
set bell %room.contents(19060)%
if %bell%
  %purge% %bell%
  if !%ok%
    %send% %actor% You throw down the smoke bomb... there's a putrid smell as green gas fills the nest!
    %echoaround% %actor% ~%actor% throws down a smoke bomb... there's a putrid smell as green as fills the nest!
    %regionecho% %room% 15 &&yYou hear a goblin yell, 'Aaaaaaaaaahhhh!!!'&&0
    set ok 1
  end
end
* and?
if %ok%
  * trigger completion
  %adventurecomplete%
  set ch %room.people%
  while %ch%
    if %ch.on_quest(18241)%
      %quest% %ch% trigger 18241
      %send% %ch% You've successfully cleared a goblin nest with the smoke bomb!
    end
    set ch %ch.next_in_room%
  done
  %mod% %room% description The floor of the ruins is eaten away and you stumble down into a den to which the word filthy hardly does justice. 
  %mod% %room% append-description It looks like some goblins had been nesting here, but something has driven them off. There's a lingering stench and some green smoke hanging in the still air, but not sign of the goblins.
  %purge% %self%
else
  %send% %actor% It doesn't look like there are any goblins here to clear out. It's best you save your smoke bomb.
end
~
#18228
GoA: Use gilded net to catch bugs~
1 c 2
net~
* Catches a 'bug' in Mill Manor
set num_needed 2
set bug_list 11133 11137 11140
return 1
* basic errors first
set vict %actor.char_target(%arg.car%)%
if !%arg%
  %send% %actor% Net whom?
  halt
elseif %vict.vnum% < 11130 || !(%bug_list% ~= %vict.vnum%)
  %send% %actor% You can't net ~%vict% with this!
  halt
elseif %actor.fighting%
  %send% %actor% You're a bit busy right now!
  halt
elseif %actor.position% != Standing
  %send% %actor% You need to get up first.
  halt
end
* try to do the thing
%send% %actor% You swoop toward ~%vict% with the gilded net... and catch it!
%echoaround% %actor% ~%actor% swoops toward ~%vict% with @%self%... and catches it!
eval 18243_bug_count %actor.var(18243_bug_count,0)% + 1
remote 18243_bug_count %actor.id%
%purge% %vict%
if %18243_bug_count% >= %num_needed%
  %send% %actor% That's all the bugs you needed... and good thing, too. Your net just broke!
  if %actor.on_quest(18243)%
    %quest% %actor% trigger 18243
  end
  %purge% %self%
end
~
#18229
GoA: Summon Rodentmort~
1 c 2
release~
set needed 3
return 1
if !%arg%
  %send% %actor% Release what?
  halt
elseif !(Rodentmort /= %arg%) && !(rat /= %arg%) && !(Morty /= %arg%)
  %send% %actor% You don't seem to have that (try: release Rodentmort).
  halt
end
* validate
set found 0
set ch %actor.room.people%
while %ch%
  if %ch.vnum% == 18224 && %ch.leader% == %actor%
    set found 1
  end
  set ch %ch.next_in_room%
done
if %found%
  %send% %actor% Rodentmort is already out of his cage.
  halt
end
if %actor.var(18245_food_count,0)% >= %needed%
  %send% %actor% Rodentmort is sleeping in his cage; you already finished this quest.
  * in case
  %quest% %actor% trigger 18245
  halt
end
* ok, summon him
%load% mob 18224
set rat %actor.room.people%
if %rat.vnum% == 18224
  %force% %rat% mfollow %actor%
  %send% %actor% You open the water-logged cage and ~%rat% plops out!
  %echoaround% %actor% ~%actor% opens a water-logged cage and ~%rat% plops out!
else
  %send% %actor% You can't seem to get the cage open.
end
~
#18230
GoA: Rodentmort behavior~
0 bt 75
~
* seek food and eat
set needed 3
set leader %self.leader%
set room %self.room%
* check leader here
if !%leader% || %leader.room% != %room%
  %echo% ~%self% scampers off.
  %purge% %self%
  halt
end
* eat a corpse? try inventory then room
set obj %self.inventory(1000)%
set loop 0
while %loop% <= 1
  if %obj%
    %echo% ~%self% devours @%obj%! &&Z&%self% makes a huge mess.
    nop %obj.empty%
    %purge% %obj%
    eval 18245_food_count %leader.var(18245_food_count,0)% + 1
    remote 18245_food_count %leader.id%
    wait 1 s
    if %18245_food_count% >= %needed%
      %echo% ~%self% hops back into ^%self% cage and falls asleep.
      %quest% %leader% trigger 18245
      %purge% %self%
    end
    halt
  end
  set obj %room.contents(1000)%
  eval loop %loop% + 1
done
~
#18238
Consider / Kill Death~
0 c 0
consider kill~
* Target check
if %actor.char_target(%arg%)% != %self%
  return 0
  halt
end
if consider /= %cmd%
  %send% %actor% You consider your chances against ~%self%.
  %echoaround% %actor% ~%actor% considers ^%actor% chances against ~%self%.
  %send% %actor% &%self% looks like &%self% will destroy you in time!
  return 1
  halt
else
  %send% %actor% You consider attacking ~%self%, then think better of it.
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
if %actor.char_target(%arg%)% != %self%
  return 0
  halt
end
set rep_check %actor.has_reputation(18200, Liked)%
if !%rep_check%
  %send% %actor% You must be at least Liked by the Adventurer's Guild to ride ~%self%.
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
if %actor.char_target(%arg%)% != %self%
  return 0
  halt
end
set rep_check %actor.has_reputation(18200, Venerated)%
if !%rep_check%
  %send% %actor% You must be at least Venerated by the Adventurer's Guild to ride ~%self%.
  return 1
  halt
end
return 0
~
#18248
GoA: Pry gem off the wall~
1 c 2
pry~
set valid_rooms 18503 18504 18508 18509 18510 18511 18512 18513 18514
set needed 4
set room %actor.room%
return 1
if %arg% && %arg% != gem
  %send% %actor% You can only use @%self% to pry gems.
  halt
elseif !%room.template% || !(%valid_rooms% ~= %room.template%)
  %send% %actor% There are no gems you can pry here using @%self%.
  halt
elseif %room.var(18246_gem_pried,0)% == %actor.id%
  %send% %actor% You already pried it off.
  halt
elseif %room.var(18246_gem_pried,0)% > 0
  %send% %actor% There was a gem here, but someone has already pried it off.
  halt
end
* ok safe to pry
%load% obj 18249 %actor% inv
set 18246_gem_pried %actor.id%
remote 18246_gem_pried %room.id%
%send% %actor% You manage to pry a gem loose from the wall!
%echoaround% %actor% ~%actor% pries a gem loose from the wall and pockets it!
%mod% %room% append-description Someone has pried a gem from the wall.
* check limit
set found 0
set obj %actor.inventory%
while %obj%
  if %obj.vnum% == 18249
    eval found %found% + 1
  end
  set obj %obj.next_in_list%
done
if %found% >= %needed%
  %send% %actor% ... your enchanted bar of prying breaks. Luckily, you got enough gems.
  %echoaround% %actor% @%self% breaks in |%actor% hands.
  %purge% %self%
end
~
#18250
GoA: Use stone orb of hiding~
1 c 2
use~
* check targeting
if !%arg% || %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
return 1
set room %actor.room%
set start %instance.start%
set emerald %room.contents(18507)%
* check location
if %room.template% != 18501
  %send% %actor% You need to use this orb at the great gate of a lost temple, in front of its emerald-green orb.
  halt
elseif %start% && %start.var(18247_hidden,0)%
  %send% %actor% The emerald-green orb in front of the gate is drained of power. Someone has already hidden this temple.
  halt
elseif !%emerald%
  %send% %actor% It looks like someone already shattered the emerald-green orb. You won't be able to use @%self% here.
  halt
end
* ok go
%adventurecomplete%
if %start%
  set 18247_hidden %actor.id%
  remote 18247_hidden %start.id%
end
%send% %actor% You hold out @%self% and watch as a strange glow flows from the emerald-green orb in front of the gate, into the orb in your hands...
%echoaround% %actor% ~%actor% holds out @%self% and you watch as a strange glow flows from the emerald-green orb in front of the gate, into the orb in ^%actor% hands...
%regionecho% %room% 1 There's a strange rumbling that shakes the whole area!
%echo% The emerald-green orb goes dark as vines begin to overtake the temple.
%send% %actor% The stone orb in your hands turns to sand and falls to the ground.
%mod% %room% description You stand before a huge, thick stone gate, engraved with carvings of tribal warriors dressed as eagles and jaguars. The gate is closed;
%mod% %room% append-description there does not seem to be a way to open it. The orb that once powered the gate has gone dark.
* trigger quests
set ch %room.people%
while %ch%
  if %ch.on_quest(18247)%
    %quest% %ch% trigger 18247
  end
  set ch %ch.next_in_room%
done
* and purge
%purge% %emerald%
%purge% %self%
~
#18251
GoA: Bribe bandits~
1 c 2
bribe~
return 1
* check bandit here
set room %actor.room%
* find 1 bandit
set bandit %room.people(10105)%
if !%bandit%
  set bandit %room.people(10106)%
end
if !%bandit%
  set bandit %room.people(10107)%
end
if !%bandit%
  %send% %actor% There's nobody here you can give this bribe to.
  halt
end
* check bribe amount
set amount %arg.car%
set type %arg.cdr%
if !%arg%
  %force% %bandit% say Ha! You're wasting my time and yours. I want at least 300 coin on top of that.
  %send% %actor% Type 'bribe 300 coins' to make a better offer.
  halt
end
if %amount% < 300 || !(coins /= %type%)
  %send% %actor% Usage: bribe <number> coins
  %send% %actor% You cannot specify a type of coins for this.
  halt
end
if !%actor.can_afford(%amount%)%
  %send% %actor% You don't have that many coins.
  halt
end
* ok go
nop %actor.charge_coins(%amount%)%
%send% %actor% You slip ~%bandit% the guild's bribe plus %amount% coins of your own...
%echoaround% %actor% ~%actor% slips something to ~%bandit%.
wait 1
%force% %bandit% say Well, that's our mischief managed, then.
wait 1
* complete quests and purge bandits
set ch %room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch.on_quest(18251)%
    %quest% %ch% trigger 18251
  elseif %ch.vnum% >= 10105 && %ch.vnum% <= 10109
    %echo% ~%ch% leaves.
    %purge% %ch%
  end
  set ch %next_ch%
done
* and purge me
%purge% %self%
~
#18252
GoA: Pilfer pixy using a jar~
1 c 2
pilfer~
return 1
set room %actor.room%
if %arg% && !(pixy /= %arg%)
  %send% %actor% This jar can only be used for pilfering pixies.
  halt
elseif %self.val0%
  %quest% %actor% trigger 18252
  %send% %actor% You've already pilfered the pixy.
  halt
elseif %room.template% != 11918
  %send% %actor% You're not looking for any common pixy... You need to do this at the Pixy Races in the Tower Skycleave.
  halt
end
* ok:
%send% %actor% You covertly place the jar next to the pixy stalls and tap it three times...
%send% %actor% You see one of the pixies -- Mischantsy -- disappear in a little whirl of light, and then the jar shakes.
%send% %actor% You slip the jar back in your pocket and hope no one sees.
%echoaround% %actor% You notice ~%actor% doing something near the pixy stalls.
%quest% %actor% trigger 18252
nop %self.val0(1)%
%mod% %self% keywords jar pilfered pixy old clay Mischantsy's Mischantsys
%mod% %self% shortdesc Mischantsy's pixy jar
%mod% %self% longdesc A pilfered pixy in a jar is lying on the ground.
%mod% %self% lookdesc The jar is made from rough clay and looks quite old. The top is sealed with
%mod% %self% append-lookdesc red wax, and you can hear someone shouting inside when you shake it. Magic
%mod% %self% append-lookdesc words are written along the jar's margin, in an ancient language you can't read.
%mod% %self% append-lookdesc-noformat &0   You need to take the jar to the guild tinker in the Tipsy Tortoise.
~
#18253
GoA: Use skeleton key to steal scrolls~
1 c 2
use~
return 1
set room %actor.room%
if !%arg% || %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
elseif %self.val0%
  %send% %actor% You've already stolen the scrolls.
  %quest% %actor% trigger 18253
  halt
elseif %room.people(11847)%
  %send% %actor% You can't get to the right drawer with Kara Virduke here.
  halt
elseif %room.template% != 11835 && %room.template% != 11935
  %send% %actor% This isn't the right place to use the disarticulated skeleton key.
  halt
end
* ok:
%send% %actor% You sneak over through the archway and unlock a drawer with the disarticulated skeleton key...
%send% %actor% You find the sealed scrolls inside and quickly pocket them.
%echoaround% %actor% You notice ~%actor% doing something through the archway, but &%var%'s back before you can see what &%var%'s doing.
%quest% %actor% trigger 18253
nop %self.val0(1)%
%mod% %self% keywords scrolls sealed set
%mod% %self% shortdesc a set of sealed scrolls
%mod% %self% longdesc A set of sealed scrolls is lying on the ground.
%mod% %self% lookdesc tba
detach 18253 %self.id%
~
#18254
GoA: Reflect mob with smoky mirror~
1 c 2
reflect~
return 1
set room %actor.room%
set vict %actor.char_target(%arg.car%)%
if !%arg%
  %send% %actor% Reflect whom with the strange and smoky mirror?
  halt
elseif !%vict%
  %send% %actor% You don't see anybody called %arg.car% here.
  halt
elseif %vict.vnum% < 10401 || %vict.vnum% > 10415
  %send% %actor% You hold the strange and smoky mirror up to ~%vict% but can't see a reflection.
  %echoaround% %actor% ~%actor% holds a strange mirror up near ~%vict% but you can't tell what &%var%'s doing.
  halt
end
* ok to mirror them
%send% %actor% You hold the strange and smoky mirror up to ~%vict%...
%echoaround% %actor% ~%actor% holds a strange mirror up near ~%vict%...
if %vict.vnum% == 10404 && !%self.val0%
  nop %self.val0(1)%
  set extract 1
elseif %vict.vnum% == 10409 && !%self.val1%
  nop %self.val1(1)%
  set extract 1
elseif %vict.vnum% == 10414 && !%self.val2%
  nop %self.val2(1)%
  set extract 1
else
  * oops
  set extract 0
end
* check completion
if %self.val0% && %self.val1% && %self.val2%
  set ch %room.people%
  while %ch%
    if %ch.on_quest(18254)%
      %quest% %ch% trigger 18254
    end
    set ch %ch.next_in_room%
  done
end
* consequences!
if %extract%
  %echo% ~%vict% is sucked into the mirror!
  %purge% %vict%
else
  %force% %vict% maggro %actor%
end
~
#18256
Catch wildling with a huge net~
1 c 3
net~
if !%arg%
  %send% %actor% What do you want to catch with @%self%?
  return 1
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% They must have ran away when you started waving @%self% around, because they're not here.
  return 1
  halt
end
if %target.is_npc% && (%target.vnum% >= 12658 && %target.vnum% <= 12661)
  if %actor.inventory(18257)%
    %send% %actor% You don't need to catch any more wildlings.
    return 1
    halt
  end
  %send% %actor% You throw @%self% over ~%target% and capture *%target%!
  %echoaround% %actor% ~%actor% throws @%self% over ~%target% and captures *%target%!
  %purge% %target%
  %load% obj 18257 %actor% inv
  return 1
  %purge% %self%
  halt
elseif %target.is_npc% && ((%target.vnum% >= 12668 && %target.vnum% <= 12670) || %target.vnum% == 10005)
  %send% %actor% That wildling is tame. Catching it in a net would be pointless and wasteful.
  return 1
  halt
elseif %target.is_pc%
  if %target% == %actor%
    %send% %actor% You briefly ponder catching yourself with @%self%, then think better of it.
    return 1
    halt
  end
  %send% %actor% You advance menacingly on ~%target% with @%self%...
  %send% %target% ~%actor% advances menacingly on you with a huge net...
  %echoneither% %actor% %target% ~%actor% advances menacingly on ~%target% with a huge net...
  return 1
  halt
else
  %send% %actor% You're supposed to catch a wildling with that, not ~%target%.
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
  %send% %actor% You have already talked to ~%self%.
  return 1
  halt
end
if !(%actor.on_quest(18258)%
  %send% %actor% You don't need to talk to anyone here right now.
  return 1
  halt
end
set room %actor.room%
set cycles_left 5
while %cycles_left% >= 0
  if (%actor.room% != %room%) || %actor.fighting% || %actor.disabled%
    * We've either moved or the room's no longer suitable for the action
    if %cycles_left% < 5
      %echoaround% %actor% |%actor% conversation is interrupted.
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
      %echoaround% %actor% ~%actor% starts talking to ~%self%...
      %send% %actor% You greet the druid and ask *%self% about the rift...
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
      %echoaround% %actor% ~%self% fishes a pamphlet out of ^%self% robe and gives it to ~%actor%.
      %send% %actor% You finish your conversation with ~%self%, who gives you a pamphlet from ^%self% robe.
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
set person %self.room.people%
set found 0
while %person%
  if %person.vnum% == 10451
    set found 1
  end
  set person %person.next_in_room%
done
if !%found%
  return 0
  halt
end
set vnum -1
set named a thing
if (!%arg%)
  return 0
  halt
elseif ticket /= %arg%
  set vnum 18261
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
  %send% %actor% ~%self% tells you, 'Human needs 50 coin to buy that.'
  halt
end
nop %actor.charge_coins(50)%
%load% obj %vnum% %actor% inv
%send% %actor% You buy %named% for 50 coins.
%echoaround% %actor% ~%actor% buys %named%.
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
if %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
set target %actor.char_target(%arg.cdr%)%
if !%target%
  %send% %actor% You don't see a '%arg.cdr%' here.
  halt
end
if %target.vnum% != 10551 && %target.vnum% != 10552
  %send% %actor% That's not a valid target for @%self%.
  return 1
  halt
end
set valid 1
if %actor.varexists(guild_siphoned_%target.vnum%)%
  eval check %%actor.guild_siphoned_%target.vnum%%%
  if %check%
    %send% %actor% You've already siphoned energy from ~%target%.
    set valid 0
  end
end
if %valid%
  %send% %actor% You siphon energy from ~%target%...
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
set target %actor.char_target(%arg%)%
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
  %send% %actor% You pick |%target% pocket...
  %load% obj 18210 %actor% inv
  set item %actor.inventory()%
  %send% %actor% You find @%item%!
  %actor.add_resources(18272, -1)%
  return 1
  halt
end
~
#18277
Adventurer Guild Tier 2: Give items on start~
2 u 0
~
switch %questvnum%
  case 18241
    %load% obj 18218 %actor% inv
  break
  case 18243
    %load% obj 18220 %actor% inv
    set 18243_bug_count 0
    remote 18243_bug_count %actor.id%
  break
  case 18245
    %load% obj 18224 %actor% inv
    set 18245_food_count 0
    remote 18245_food_count %actor.id%
  break
  case 18246
    %load% obj 18248 %actor% inv
  break
  case 18247
    %load% obj 18250 %actor% inv
  break
  case 18251
    %load% obj 18251 %actor% inv
  break
  case 18252
    %load% obj 18252 %actor% inv
  break
  case 18253
    %load% obj 18253 %actor% inv
  break
  case 18254
    %load% obj 18254 %actor% inv
  break
  case 18279
    if %actor.completed_quest(18283)% && %actor.completed_quest(18287)% && %actor.completed_quest(18291)% && %actor.completed_quest(18295)%
      %load% obj 18294 %actor% inv
      set item %actor.inventory(18294)%
      nop %item.val0(18279)%
    end
  break
  case 18281
    %load% obj 18281 %actor% inv
  break
  case 18282
    if %actor.varexists(18282_dragon_imagined)%
      %load% obj 18294 %actor% inv
      set item %actor.inventory(18294)%
      nop %item.val0(18282)%
    else
      %load% obj 18282 %actor% inv
    end
  break
  case 18285
    %load% obj 18286 %actor% inv
  break
  case 18289
    %load% obj 18289 %actor% inv
  break
  case 18290
    %load% obj 18290 %actor% inv
  break
done
if %questvnum% >= 18280 && %questvnum% <= 18283 && !%actor.inventory(18280)%
  %load% obj 18280 %actor% inv
  set item %actor.inventory(18280)%
  * %send% %actor% You receive @%item%.
end
~
#18278
Support quest progress checker~
2 v 0
~
if %actor.completed_quest(18283)% && %actor.completed_quest(18287)% && %actor.completed_quest(18291)% && %actor.completed_quest(18295)%
  %quest% %actor% trigger 18279
end
~
#18280
Signal Malfernes~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if !(%actor.on_quest(18280)% || %actor.on_quest(18281)% || %actor.on_quest(18282)% || %actor.on_quest(18283)%)
  %send% %actor% You have no need to contact Malfernes right now.
  %send% %actor% @%self% crumbles to dust.
  %purge% %self%
  halt
end
set room %self.room%
if (%room.template% < 10250 || %room.template% > 10299)
  %send% %actor% You must use this item inside a Primeval Portal adventure instance.
  halt
end
set boss %instance.mob(10252)%
if !%boss%
  set boss %instance.mob(10253)%
end
if !%boss%
  set boss %instance.mob(10254)%
end
if !%boss%
  if %instance.mob(18280)%
    %send% %actor% Malfernes is already waiting for you.
    halt
  end
  %send% %actor% You cannot contact Malfernes from an adventure instance that has already been cleared. Find a new one.
  halt
end
set target_room %boss.room%
if %boss.fighting%
  %send% %actor% You can't do that, since someone is currently fighting this adventure's boss.
  halt
end
* Success
%send% %actor% You signal Archsorcerer Malfernes, letting him know you're coming.
%echoaround% %actor% ~%actor% waves @%self% in the air.
%at% %target_room% %echo% ~%boss% suddenly vanishes with a mighty bang, and is replaced by a relaxed-looking Archsorcerer Malfernes!
%purge% %boss%
%at% %target_room% %load% mob 18280
%adventurecomplete%
~
#18281
Use charm on chalice~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set chalice %actor.inventory(11131)%
if !%chalice%
  %send% %actor% You need to get the chalice first.
  halt
end
set room %self.room%
if %room.template% < 11130 || %room.template% > 11159
  %send% %actor% You need to do that inside Mill Manor.
  halt
end
%send% %actor% You quickly slap @%self% on @%chalice%, which sparks and crackles violently before settling down.
%echoaround% %actor% ~%actor% slaps @%self% on @%chalice%, which sparks and crackles violently!
%purge% %chalice%
%load% obj 18283 %actor% inv
if %instance.id%
  %adventurecomplete%
  * swap in i11151 for the old stone marker
  %door% i11130 east room i11151
  %echo% You find yourself back at the stone marker, where the adventure began!
  %teleport% adventure i11151
end
~
#18282
Imagine Dragons~
1 c 2
use~
if %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
if %arg.cdr%
  %send% %actor% Usage: use staff
  return 1
  halt
end
if %actor.varexists(18282_dragon_imagined)%
  %send% %actor% You have imagined enough dragons for one lifetime.
  halt
end
set room %self.room%
if %self.val0%
  * Captured a dragon
  if %roomm.building_vnum% == 11800 || %room.template% == 11800
    %send% %actor% Move further in past the entrance first.
    halt
  elseif (%room.template% < 11800 || %room.template% >= 11875) && (%room.template% < 11900 || %room.template% >= 11975)
    %send% %actor% You have already captured a dragon. Now go to Skycleave and use @%self% to imagine a copy of it.
    halt
  end
  %load% mob 18282
  set dragon %room.people%
  if %dragon.vnum% != 18282
    %send% %actor% Something went wrong while imagining a dragon. Please submit a bug report.
  else
    dg_affect %dragon% !ATTACK on 300
    %send% %actor% You imagine a mighty dragon!
    %echoaround% %actor% ~%actor% concentrates intensely, and ~%dragon% fades into view!
  end
  %quest% %actor% trigger 18282
  set 18282_dragon_imagined 1
  remote 18282_dragon_imagined %actor.id%
else
  * Trying to capture a dragon
  set person %room.people%
  while %person%
    if (%person.vnum% >= 10330 && %person.vnum% <= 10333) || %person.vnum% == 10300
      %send% %actor% You capture an image of ~%person%!
      nop %self.val0(1)%
      halt
    end
    set person %person.next_in_room%
  done
  %send% %actor% There aren't any flame dragons or wandering dragons to capture an image of here.
end
~
#18283
Imaginary dragon death~
0 f 100
~
%echo% As ~%self% dies, you realize it was just a figment of your imagination.
~
#18286
Net Rats for Germione~
1 c 2
net~
if !%arg%
  %send% %actor% What do you want to catch with @%self%?
  return 1
  halt
end
set target %actor.char_target(%arg%)%
if !%target%
  %send% %actor% They must have run away when you started waving @%self% around, because they're not here.
  return 1
  halt
end
if %target.is_npc%
  switch %target.vnum%
    case 9189
    break
    case 10011
    break
    case 10012
    break
    case 10013
      * Dire rat
      %send% %actor% That's a bit big to catch in your net...
      %send% %actor% ...but you give it a go anyway.
    break
    case 10450
    break
    case 10455
      %send% %actor% That's a bit big to catch in your net...
      halt
    break
    case 19001
      %send% %actor% Germione would prefer if you fetched rats that aren't hers.
      halt
    break
    case 19002
      %send% %actor% Germione would prefer if you fetched rats that aren't hers.
      halt
    break
    default
      %send% %actor% That doesn't appear to be a rat.
      halt
    break
  done
  if %actor.has_resources(18287, 10)%
    %send% %actor% You have enough rats.
    halt
  end
  %send% %actor% You throw @%self% over ~%target% and haul *%target% in...
  %echoaround% %actor% ~%actor% throws @%self% over ~%target% and hauls *%target% in.
  %purge% %target%
  %load% obj 18287 %actor% inv
  set item %actor.inventory(18287)%
  %send% %actor% You get @%item%.
else
  if %target% == %actor%
    %send% %actor% You briefly ponder catching yourself with @%self%, then think better of it.
    return 1
    halt
  end
  %send% %actor% You advance menacingly on ~%target% with @%self%...
  %send% %target% ~%actor% advances menacingly on you with a huge net...
  %echoneither% %actor% %target% ~%actor% advances menacingly on ~%target% with a huge net...
  return 1
  halt
end
~
#18287
Fake kill tree spirit~
0 c 0
kill~
if %actor.char_target(%arg%)% != %self%
  return 0
  halt
end
set room %self.room%
if !%actor.canuseroom_member(%room%)%
  %send% %actor% You don't have permission to do that!
  halt
end
%send% %actor% You strike ~%self% down...
%echoaround% %actor% ~%actor% strikes ~%self% down...
switch %room.sector_vnum%
  case 18294
    %echo% The dragon tree melts away, leaving behind an ordinary forest!
    %terraform% %room% 4
  break
  case 18293
    %echo% The field of imagination melts away, leaving behind an ordinary plain!
    %terraform% %room% 0
  break
  default
    %echo% ...but nothing seems to happen.
  break
done
%purge% %self%
~
#18288
Resurrect Scaldorran: DEPRECATED~
1 c 2
use~
* This is deprecated with the new version of Skycleave (Ashes of History), which does not require it.
return 0
halt
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if !(%actor.on_quest(18288) || %actor.on_quest(18289) || %actor.on_quest(18290) || %actor.on_quest(18391)%)
  %send% %actor% You don't have anything to talk to Scaldorran about right now.
  %send% %actor% @%self% vanishes in a puff of smoke.
  %purge% %self%
  halt
end
set room %self.room%
if %room.template% != 10055
  %send% %actor% @%self% can only be used in Skycleave's Lich Labs.
  halt
end
if %instance.mob(10048)%
  %send% %actor% You can't use @%self% when Scaldorran is already here.
  halt
end
%load% mob 10048
set scaldorran %room.people%
if %scaldorran.vnum% != 10048
  %send% %actor% Failed to load Scaldorran. Please submit a bug report containing this message.
end
%send% %actor% You use @%self% and the remains of ~%scaldorran% reform and reanimate!
%echoaround% %actor% ~%actor% uses @%self% and the remains of ~%scaldorran% reform and reanimate!
dg_affect %scaldorran% !ATTACK on -1
~
#18289
Bag roc egg for Scaldorran~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %self.room%
if %room.template% != 11000
  %send% %actor% Use this inside a roc nest.
  halt
end
set egg %room.contents%
set check 0
while %egg% && !%check%
  if %egg.vnum% == 11001
    set check 1
  else
    set egg %egg.next_in_list%
  end
done
if !%check%
  %send% %actor% There's no egg here to steal!
  halt
end
%send% %actor% You grab @%egg% and quickly slide it into @%self%.
%load% obj 18291 %actor% inv
%load% obj 11021 room
%load% mob 11002
set mob %room.people%
if %mob.vnum% != 11002
  %send% %actor% Error loading roc. Please submit a bug report with this message.
else
  %force% %actor% down
  %force% %mob% mhunt %actor%
  %regionecho% %instance.location% 10 An angry screech echoes across the land!
end
%purge% %self%
~
#18290
Bug the Grand High Sorcerer's Office~
1 c 2
plant~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %self.room%
return 1
set bad_office 11864 11964 11866 11966 11973
if %bad_office% ~= %room.template%
  %send% %actor% This doesn't quite seem to be the right office.
  halt
elseif %room.template% != 11868 && %room.template% != 11968
  if %room.template% >= 11800 && %room.template% <= 11999
    %send% %actor% You need to plant this bug in the Grand High Sorcerer's office.
  else
    %send% %actor% You need to plant this bug in the Grand High Sorcerer's office in the Tower Skycleave.
  end
  halt
end
* check shade
set shade %room.people(11869)%
if !%shade%
  set shade %room.people(11863)%
end
if %shade%
  %send% %actor% ~%shade% covers too much of the room for you to plant the bug.
  halt
end
* check GHS
set ghs %room.people(11968)%
if !%ghs%
  set ghs %room.people(11868)%
end
if !%ghs%
  set ghs %room.people(11870)%
end
if !%ghs%
  set ghs %room.people(11969)%
end
if %ghs%
  * still here...
  if %actor.skill(Stealth)% >= 75
    %send% %actor% You use your Stealth skill to plant @%self% while ~%ghs% isn't watching.
  elseif %ghs.aff_flagged(BLIND)%
    %send% %actor% You quickly plant @%self%, taking advantage of |%ghs% temporary blindness.
  elseif %ghs.aff_flagged(STUNNED)% && !%ghs.fighting%
    * Sap (presumably from an ally)
    %send% %actor% You quickly plant the bug while ~%ghs% is stunned.
  else
    %send% %actor% ~%ghs% would notice if you tried to plant the bug while &%ghs%'s watching...
    halt
  end
else
  %send% %actor% You surreptitiously plant @%self% in the palatial office.
end
%quest% %actor% trigger 18290
%purge% %self%
~
#18291
Plant dragon tree~
0 i 10
~
set room %self.room%
if %room.sector_vnum% == 4
  wait 1
  %echo% A great dragon tree suddenly springs from the ground nearby, eclipsing the surrounding forest!
  %terraform% %room% 18294
  %purge% %self%
end
~
#18292
Thirteen coded greeting~
0 g 100
~
if %actor.on_quest(18292)% && !%actor.quest_triggered(18292)%
  wait 5
  say What always runs but never walks, often murmurs, never talks, has a bed but never sleeps, has a mouth but never eats?
end
~
#18293
Seed of Imagination plant~
1 c 2
plant~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %actor.room%
if %room.sector_vnum% != 0
  %send% %actor% You can't plant that here.
  return 1
  halt
end
if !%actor.canuseroom_member(%room%)%
  %send% %actor% You don't have permission to use @%self% here.
  return 1
  halt
end
%send% %actor% You plant @%self%...
%echoaround% %actor% ~%actor% plants @%self%...
%terraform% %room% 18293
%echo% The plains around you shift slowly into a %room.sector%!
%purge% %self%
~
#18294
quest trigger on start~
1 n 100
~
wait 1
switch %questvnum%
  case 18279
    %send% %actor% You already have the support of everyone.
    %quest% %actor% trigger 18279
  break
  case 18282
    %send% %actor% You have already imagined enough dragons for one lifetime.
    %quest% %actor% trigger 18282
  break
done
%purge% %self%
~
#18295
Verdant Wand: Teleport / Terraform~
1 c 3
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set sectname %self.room.sector%
set terra 1
if %sectname% == Scorched Woods
  set vnum 2
elseif %sectname% == Scorched Grove
  set vnum 26
elseif %sectname% == Scorched Plains || %sectname% == Scorched Crop
  set vnum 0
elseif %sectname% == Scorched Desert || %sectname% == Scorched Desert Crop
  set vnum 20
else
  set terra 0
end
* limited charges
if (%terra% && !%self.val0%) || (!%terra% && !%self.val1%)
  %send% %actor% @%self% is out of charges for that ability.
  halt
end
set cycle 0
while %cycle% >= 0
  * Repeats until break
  set loc %instance.nearest_rmt(10300)%
  * Rather than setting error in 10 places, just assume there's an error and clear it if there isn't
  set error 1
  if %actor.fighting%
    %send% %actor% You can't use @%self% during combat.
  elseif %actor.position% != Standing
    %send% %actor% You need to be standing up to use @%self%.
  elseif !%actor.can_teleport_room% && !%terra%
    %send% %actor% You can't teleport out of here.
  elseif !%loc% && !%terra%
    %send% %actor% There is no valid location to teleport to.
  elseif %actor.aff_flagged(DISTRACTED)%
    %send% %actor% You are too distracted to use @%self%!
  else
    set error 0
  end
  * Doing this AFTER checking loc exists
  if !%actor.can_enter_instance(%loc%)% && !%terra%
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
      %send% %actor% The glyphs on @%self% glow a deep green and the light begins to envelop you!
      %echoaround% %actor% The glyphs on @%self% glow a deep green and the light begins to envelop ~%actor%!
    break
    case 2
      if %terra%
        %send% %actor% You raise @%self% high and the scorched landscape is restored!
        %echoaround% %actor% ~%actor% raises @%self% high and the scorched landscape is restored!
        %terraform% %room_var% %vnum%
        eval charges_left %self.val0%-1
        nop %self.val0(%charges_left%)%
        halt
      else
        %echoaround% %actor% ~%actor% vanishes in a flash of green light!
        %teleport% %actor% %loc%
        %force% %actor% look
        %echoaround% %actor% ~%actor% appears in a flash of green light!
        nop %actor.cancel_adventure_summon%
        eval charges_left %self.val1%-1
        nop %self.val1(%charges_left%)%
        halt
      end
    break
  done
  wait 5 sec
  eval cycle %cycle% + 1
done
~
#18296
Malfernes: Guild Quest Greeting~
0 g 100
~
if !%actor.on_quest(18280)% || %actor.quest_triggered(18280)% || %self.fighting% || %self.disabled%
  halt
end
nop %self.add_mob_flag(SILENT)%
wait 5
set room %self.room%
wait 1 sec
set cycles_left 5
while %cycles_left% >= 0
  if %self.fighting% || %self.disabled%
    * Combat interrupts the speech
    %echo% |%self% monologue is interrupted.
    nop %self.remove_mob_flag(SILENT)%
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      say Did the Academy send you? Are you here to take me back? I'm not going back there!
    break
    case 4
      say Oh! It was you who signalled me. Who gave you that signalling stone, the Guild? Ha!
    break
    case 3
      say If that old fool Nostrazak thinks I'm supporting another guildhall, he's got another think coming.
    break
    case 2
      %echo% A strange violet glow encircles ~%self%, as if some arcane magic is controlling him.
    break
    case 1
      say I... could support another guildhall. I just need... a favor from you.
    break
    case 0
      %echo% ~%self% blinks and seems to come out of a trance.
      wait 1 sec
      set person %room.people%
      while %person%
        if %person.is_pc% && %person.on_quest(18280)%
          %quest% %actor% trigger 18280
          %quest% %actor% finish 18280
        end
        set person %person.next_in_room%
      done
      nop %self.remove_mob_flag(SILENT)%
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
~
#18297
Germione: Guild Quest Codeword~
0 d 0
friend~
if !%actor.on_quest(18284)% || %actor.quest_triggered(18284)%
  %send% %actor% You don't need to give ~%self% the codeword now.
  halt
end
if %self.fighting% || %self.disabled%
  halt
end
nop %self.add_mob_flag(SILENT)%
set room %self.room%
wait 1 sec
set cycles_left 5
while %cycles_left% >= 0
  if %self.fighting% || %self.disabled%
    * Combat interrupts the speech
    %echo% |%self% monologue is interrupted.
    nop %self.remove_mob_flag(SILENT)%
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      say Oh, dearie, you must be from that Adventurers Guild.
    break
    case 4
      say I don't really have no time for adventuring these days. I'm running a holistic rat training agency out here in these parts.
    break
    case 3
      say I remember doing this silly gather-support quest, so I may as well make this one easy on you.
    break
    case 2
      say And maybe afterwards, I can talk you into adopting some trained swamp rats.
    break
    case 1
      say Rats really do make the best pets. I had my Measley but the poor scabber got eaten by a snake.
    break
    case 0
      say Best guard dog rat I ever did have...
      wait 1 sec
      set person %room.people%
      while %person%
        if %person.is_pc% && %person.on_quest(18284)%
          %quest% %actor% trigger 18284
          %quest% %actor% finish 18284
        end
        set person %person.next_in_room%
      done
      nop %self.remove_mob_flag(SILENT)%
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
~
#18298
Scaldorran: Guild Quest Codeword~
0 d 0
eternity~
if !%actor.on_quest(18288)% || %actor.quest_triggered(18288)%
  %send% %actor% You don't need to give ~%self% the codeword now.
  halt
end
if %self.vnum% == 10048
  %send% %actor% You seem to be in the wrong Skycleave.
  halt
end
if %self.fighting% || %self.disabled%
  halt
end
* quiet me
nop %self.add_mob_flag(SILENT)%
detach 11806 %self.id%
detach 11840 %self.id%
if %self.has_trigger(11839)%
  detach 11839 %self.id%
  set murder 1
else
  set murder 0
end
set room %self.room%
wait 1 sec
set cycles_left 5
while %cycles_left% >= 0
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echo% There is a howl from |%self% core as he intones, 'Oh, you're from the which guild? Adventurers? That's the boring one.'
    break
    case 4
      say The guild hasn't been so good to me. I had a lot more FACE left before I started working with them, if you know what I mean.
    break
    case 3
      %echo% |%self% linen wraps form a scowling face.
    break
    case 2
      say I SUPPOSE I could be convinced to work with the guild again, if you could run some ERRANDS for me.
    break
    case 1
      say I'll need a roc egg and I'll need some STEALTH WORK. Think you're up to it?
    break
    case 0
      say Well!? Why are you just standing there? GET TO IT!
      wait 1 sec
      set person %room.people%
      while %person%
        if %person.is_pc% && %person.on_quest(18288)%
          %quest% %actor% trigger 18288
          %quest% %actor% finish 18288
        end
        set person %person.next_in_room%
      done
      nop %self.remove_mob_flag(SILENT)%
      * check trigger
      if %self.vnum% == 11836
        set trig 11806
      else
        set trig 11840
      end
      if !%self.has_trigger(%trig%)%
        attach %trig% %self.id%
      end
      if %murder%
        attach 11839 %self.id
      end
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
~
#18299
Thirteen: Guild Quest Codeword~
0 d 0
river~
if !%actor.on_quest(18292)% || %actor.quest_triggered(18292)%
  %send% %actor% You don't need to give ~%self% the codeword now.
  halt
end
if %self.fighting% || %self.disabled%
  halt
end
nop %self.add_mob_flag(SILENT)%
set room %self.room%
wait 1 sec
set cycles_left 5
while %cycles_left% >= 0
  if %self.fighting% || %self.disabled%
    * Combat interrupts the speech
    %echo% |%self% monologue is interrupted.
    nop %self.remove_mob_flag(SILENT)%
    halt
  end
  * Fake ritual messages
  switch %cycles_left%
    case 5
      %echo% ~%self% rolls around clutching his belly, groaning with hunger. Then, slowly, he seems to realize ~%actor% just answered his riddle.
    break
    case 4
      %echo% ~%self% stands up, brushes himself off, and stops sucking in his gut. He doesn't actually look all that hungry.
    break
    case 3
      say Oh, you're from the guild! I should have known they'd send someone. How do you like my canyon vacation home?
    break
    case 2
      say Nevermind that. I'd be happy to support your guildhall. I just need you to fetch me a little nourishment...
    break
    case 1
      say And maybe take care of an old friend or two.
    break
    case 0
      say Are you up to it?
      wait 1 sec
      set person %room.people%
      while %person%
        if %person.is_pc% && %person.on_quest(18292)%
          %quest% %actor% trigger 18292
          %quest% %actor% finish 18292
        end
        set person %person.next_in_room%
      done
      nop %self.remove_mob_flag(SILENT)%
      halt
    break
  done
  wait 5 sec
  eval cycles_left %cycles_left% - 1
done
~
$
