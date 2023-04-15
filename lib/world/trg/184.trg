#18450
Rising water terraformer~
0 i 100
~
set room %self.room%
if !%instance.location%
  %purge% %self%
  halt
end
* sector configs
set plains_sects 0 7 13 36 46
set forest_sects 1 2 3 4 90 39 44 45 37 38 39 47
set jungle_sects 15 16 27 28 61 62 65
set desert_sects 20 12 14 23 24 25 26
* work
if (%room.distance(%instance.location%)% > 5)
  mgoto %instance.location%
elseif %room.aff_flagged(*HAS-INSTANCE)%
  halt
elseif %plains_sects% ~= %room.sector_vnum%
  %terraform% %room% 18451
  %echo% The rising water from the nearby river floods the plains!
elseif %forest_sects% ~= %room.sector_vnum%
  %terraform% %room% 18452
  %echo% The rising water from the nearby river floods the forest!
elseif %jungle_sects% ~= %room.sector_vnum%
  %terraform% %room% 29
  %echo% The rising water from the nearby river floods the jungle!
elseif %desert_sects% ~= %room.sector_vnum%
  %terraform% %room% 18453
  %echo% The rising water from the nearby river floods the desert!
end
~
#18451
Dire beaver spawn~
0 n 100
~
set room %self.room%
if (!%instance.location% || %room.template% != 18450)
  halt
end
mgoto %instance.location%
makeuid portal obj beaverportal
if %portal%
  %purge% %portal%
end
~
#18452
Beaver dam cleanup~
2 e 100
~
* this used to terraform but it results in canals becoming a river
%build% %room% 18451
* %terraform% %room% 18450
~
#18453
Beaver dam construction~
0 i 100
~
set room %self.room%
if !%instance.location%
  %purge% %self%
  halt
end
set dist %room.distance(%instance.location%)%
if (%dist% > 2)
  wait 1
  %echo% ~%self% scrambles into a hole in the dam and returns to ^%self% lodge.
  mgoto %instance.location%
  %echo% ~%self% appears from ^%self% lodge.
elseif %room.aff_flagged(*HAS-INSTANCE)%
  halt
elseif (%room.sector_vnum% == 5 && %dist% <= 2)
  %build% %room% 18451
  %echo% ~%self% expands ^%self% dam here.
end
~
#18454
Dire beaver death: stop flooding~
0 f 100
~
if !%instance%
  halt
end
set mob %instance.mob(18451)%
if %mob%
  %echo% The rising water stops.
  %purge% %mob%
end
~
#18460
Unstable Portal setup~
2 n 100
~
* Pick a random room for the inside of the portal
eval room_vnum 18461 + %random.12%
makeuid new_room room i%room_vnum%
%door% %room% down room %new_room%
* Move the inside portal object to the other room
set item %room.contents%
while %item%
  set next_item %item.next_in_list%
  if %item.vnum% == 18461
    %purge% %item%
  end
  set item %next_item%
done
%at% %new_room% %load% obj 18461
set targ %instance.location%
set num %targ.vnum%
set item %new_room.contents%
nop %item.val0(%num%)%
* Retarget the outside portal to the new room
set loc %instance.location%
set item %loc.contents%
while %item%
  if %item.vnum% == 18460
    set targ %new_room%
    set num %targ.vnum%
    nop %item.val0(%num%)%
  end
  set item %item.next_in_list%
done
~
#18461
Unstable Portal - Block farther entry~
2 q 100
~
if %actor.nohassle% || %direction% == none || %direction% == portal
  return 1
  halt
end
%send% %actor% Each time you try to walk away from the portal, you end up right back next to it.
return 0
~
#18462
Give gift on entry~
2 g 100
~
context %instance.id%
* Don't message until after the room description shows
wait 1
set person %room.people%
while %person%
  if %person.is_pc%
    * Only once per instance
    eval check %%given_item_%person.id%%%
    if !%check%
      set given_item_%person.id% 1
      global given_item_%person.id%
      * Give everyone a pet
      * Get vnum of item to give
      set vnum %room.template%
      * Do they already have it?
      set item %person.inventory(%vnum%)%
      if !%item% && !%actor.has_minipet(%vnum%)%
        * Give them the item
        %load% obj %vnum% %person% inv
        * Message
        set item %person.inventory(%vnum%)%
        if %item%
          %send% %person% # You discover @%item%!
        end
      end
      * Chance of a rare mount
      * Roll for mount
      set vnum 0
      set percent_roll %random.100%
      if %percent_roll% <= 2
        * Land mount
        set vnum 18497
      else
        eval percent_roll %percent_roll% - 2
        if %percent_roll% <= 2
          * Sea mount
          set vnum 18498
        else
          eval percent_roll %percent_roll% - 2
          if %percent_roll% <= 1
            * Flying mount
            set vnum 18499
          end
        end
      end
      if %vnum% != 0
        %load% obj %vnum% %person% inv
        set item %person.inventory()%
        %send% %person% # You are lucky! You have discovered a rare item: @%item%!
      end
    end
    if %person.is_immortal%
      set vnum 18495
      set item %person.inventory(%vnum%)%
      if !%item%
        * Give them the item
        %load% obj %vnum% %person% inv
        * Message
        set item %person.inventory(%vnum%)%
        if %item%
          %send% %person% # You create @%item% for yourself.
        end
      end
    end
  end
  set person %person.next_in_room%
done
~
#18463
Cupboard environment~
2 bw 20
~
switch %random.3%
  case 1
    %echo% The thudding sound of someone jumping above the cupboard causes sawdust to fall from the ceiling.
  break
  case 2
    %echo% You hear someone outside the cupboard yelling about petunias.
  break
  case 3
    %echo% You hear the hooting sound of owls outside the cupboard.
  break
done
~
#18464
Magic words~
2 c 0
xyzzy plugh abracadabra~
if abracadabra /= %cmd%
  %send% %actor% Good try, but that is an old worn-out magic word.
else
  %send% %actor% Nothing happens.
end
~
#18465
Twisty passages env~
2 bw 20
~
switch %random.3%
  case 1
    %echo% A bearded pirate strolls past, carrying a pile of treasure and chortling to himself.
  break
  case 2
    %echo% You hear the distant scream of a spelunker falling into a pit.
  break
  case 3
    set obj %room.contents%
    while %obj%
      if %obj.vnum% == 18486
        halt
      end
      set obj %obj.next_in_list%
    done
    set person %room.people%
    while %person%
      set obj %person.inventory(18486)%
      if %obj%
        halt
      end
      set person %person.next_in_room%
    done
    %echo% A little dwarf just walked around a corner, saw you, threw a little axe at you which missed, cursed, and ran away.
    %load% obj 18486 25
  break
done
~
#18466
Unstable env~
2 bw 10
~
set num %random.5%
if %num% == 1
  switch %random.5%
    case 1
      set material living wood
    break
    case 2
      set material smooth stone
    break
    case 3
      set material packed straw
    break
    case 4
      set material a strange fleshy material
    break
    case 5
      set material red-brown brick
    break
  done
  if !%current_material%
    set current_material wooden planks
  end
  %echo% The walls suddenly change from %current_material% to %material%.
  set current_material %material%
  global current_material
elseif %num% == 2
  set animal_num 1
  while %animal_num% <= 2
    switch %random.5%
      case 1
        set animal_%animal_num% a horse
      break
      case 2
        set animal_%animal_num% a sheep
      break
      case 3
        set animal_%animal_num% a pig
      break
      case 4
        set animal_%animal_num% a cow
      break
      case 5
        set animal_%animal_num% a chicken
      break
    done
    eval animal_num %animal_num% + 1
  done
  if %animal_1% == %animal_2%
    %echo% %animal_1% wanders by, looking strangely incongruous in the surrounding chaotic unreality.
  else
    %echo% %animal_1% with the head of %animal_2% gazes at you curiously, and then explodes and vanishes.
  end
elseif %num% == 3
  %echo% The straw on the floor ripples and shifts like grass in the wind.
elseif %num% == 4
  %echo% The stable briefly cracks open, and you glimpse infinity.
  set person %room.people%
  while %person%
    if %person.is_pc%
      if !%person.aff_flagged(!STUN)%
        %send% %person% You are stunned by its enormity...
        dg_affect %person% STUNNED on 5
      end
    end
    set person %person.next_in_room%
  done
elseif %num% == 5
  %echo% The unstable fabric of reality ripples, distorting your surroundings like a heat haze...
end
~
#18467
Hobbit hole env~
2 bw 10
~
switch %random.6%
  case 1
    %echo% You hit your head on the low ceiling. Ow!
    %scale% instance 10
    %aoe% 1
  break
  case 2
    %echo% The fire crackles quietly in the fireplace.
  break
  case 3
    %echo% You wonder where you might be swept off to if you went out the door.
  break
  case 4
    %echo% You hear dwarves shuffling around outside.
  break
  case 5
    %echo% It sounds like someone is having a birthday party outside.
  break
  case 6
    %echo% This seems like as good a place as any to start an adventure.
  break
done
~
#18470
Spirit Steed: Only leader may mount~
0 ct 0
mount harness~
if %actor.char_target(%arg.car%)% != %self%
  return 0
  halt
end
if !%self.leader% || %actor% != %self.leader%
  if %cmd.mudcommand% == mount
    %send% %actor% You try to get onto the spirit steed, but it double-jumps out of the way!
    %echoaround% %actor% ~%actor% tries to climb up onto ~%self% but it double-jumps out of the way at the last second!
  else
    %send% %actor% You try to get harness the spirit steed, but it double-jumps out of the way!
    %echoaround% %actor% ~%actor% tries harness ~%self% but it double-jumps out of the way at the last second!
  end
  return 1
  halt
end
* made it?
nop %self.add_mob_flag(MOUNTABLE)%
return 0
* check and remove mountable if mount failed
wait 1
nop %self.remove_mob_flag(MOUNTABLE)%
~
#18471
Sparkle sparkle~
2 bw 10
~
* I am shuddering internally as I type this
if %room.time(hour)% < 7 || %room.time(hour)% > 19
  halt
end
set num_people 0
set person %room.people%
while %person%
  if %person.is_pc% && %person.vampire%
    eval num_people %num_people% + 1
    set person_%num_people% %person%
  end
  set person %person.next_in_room%
done
if %num_people% == 0
  * Oops, no vampires.
else
  eval person_num %%random.%num_people%%%
  eval person %%person_%person_num%%%
  %send% %person% Your skin sparkles slightly in the sunlight.
  %echoaround% %person% ~%person% sparkles slightly in the sunlight.
done
~
#18472
Oregon Trail env + dysentery~
2 bw 10
~
* Choose a random living player...
set num_people 0
set person %room.people%
while %person%
  if %person.is_pc% && %person.health% > 0
    eval num_people %num_people% + 1
    set person_%num_people% %person%
  end
  set person %person.next_in_room%
done
if %num_people% == 0
  * Oops. There aren't any.
  %echo% All of the people in your party have died.
else
  eval person_num %%random.%num_people%%%
  eval person %%person_%person_num%%%
  %send% %person% [ You have dysentery. ]
  %echoaround% %person% [ ~%person% has dysentery. ]
  set level %person.level%
  if %level% < 10
    set level 10
  end
  %scale% instance %level%
  %dot% %person% 2000 20 poison 1
end
~
#18473
Unstable Portal: Precipice: Spawn scion~
2 bw 50
~
wait 15 sec
if %room.people(18495)%
  * already present
  halt
end
set any 0
set ch %room.people%
* check for people not killed here
while %ch% && !%any%
  if %ch.is_pc% && !%ch.is_immortal% && !%room.varexists(killed_%ch.id%)%
    set any 1
  end
  set ch %ch.next_in_room%
done
* load if needed
if %any%
  %echo% A shadow appears over the circle at the center of the precipice...
  wait 3 sec
  %load% mob 18495
  %echo% A many-armed scion drops from above!
end
~
#18482
"Leave" random direction~
0 ab 25
~
set direction_num %random.4%
switch %direction_num%
  case 1
    set direction north
  break
  case 2
    set direction east
  break
  case 3
    set direction south
  break
  case 4
    set direction west
  break
done
%echo% ~%self% %self.movetype% %direction%.
%purge% %self%
~
#18483
"Arrive from" random direction~
2 bw 33
~
set direction_num %random.4%
switch %direction_num%
  case 1
    set direction north
  break
  case 2
    set direction east
  break
  case 3
    set direction south
  break
  case 4
    set direction west
  break
done
eval vnum 18481+%random.3%
set person %room.people%
while %person%
  if %person.vnum% == %vnum%
    halt
  end
  set person %person.next_in_room%
done
%load% mob %vnum%
set mob %room.people%
%echo% ~%mob% %mob.movetype% up from the %direction%.
~
#18487
Modern portal fake list/buy~
2 c 0
list buy~
if list /= %cmd%
  * List
  %send% %actor% The stores here sell the following items (type 'buy <name>'):
  %send% %actor%  - water ($4)
  %send% %actor%  - organic kale ($16)
  %send% %actor%  - selfie stick ($20)
  %send% %actor%  - sushi ($22)
  %send% %actor%  - George Foreman grill ($40)
  %send% %actor%  - Beats headphones ($250)
  %send% %actor%  - Nintendo ($400)
  %send% %actor%  - smartphone ($800)
  %send% %actor%  - teacup poodle ($1250, not returnable)
elseif buy /= %cmd%
  * Buy
  switch %random.4%
    case 1
      %send% %actor% The shopkeeper says, 'Get out of here with your Monopoly money!'
    break
    case 2
      %send% %actor% The shopkeeper says, 'Come back when you have some real cash!'
    break
    case 3
      %send% %actor% The shopkeeper says, 'Sorry, pally, all we take here is cold, hard cash.'
    break
    case 4
      %send% %actor% The shopkeeper says, 'We don't take D&&D money here, buddy.'
    break
  done
else
  * This should never happen
  return 0
end
~
#18488
Load vampire on enter~
2 bw 100
~
return 1
wait 5
set players 0
set already_present 0
set person %room.people%
while %person%
  if %person.vnum% == 18489
    set already_present 1
  elseif %person.is_pc%
    eval players %players% + 1
  end
  set person %person.next_in_room%
done
if %players% == 0 || %already_present%
  halt
end
* One time only
%load% mob 18489
detach 18488 %self.id%
~
#18489
EmpireMUD 1.0 Vampire Attack~
0 n 100
~
set room %self.room%
set dawn 7
set dusk 19
visible
%echo% ~%self% rises from the earth!
wait 2 sec
if %room.time(hour)% >= %dawn% && %room.time(hour)% <= %dusk%
  * Sun's out
  %echo% ~%self% lunges toward you, just as the sun comes out from behind the clouds!
  wait 3 sec
  %echo% A ray of sunlight strikes ~%self%, lighting *%self% aflame!
  wait 3 sec
  %echo% ~%self% crumbles to ash!
  %load% obj 18489
  %purge% %self%
  halt
else
  * Choose a random non-vampire...
  set num_people 0
  set person %room.people%
  while %person%
    if %person.is_pc% && !%person.vampire()%
      eval num_people %num_people% + 1
      set person_%num_people% %person%
    end
    set person %person.next_in_room%
  done
  if %num_people% == 0
    * Oops. There aren't any.
    %echo% ~%self% seems surprised to see you here!
    wait 3 sec
    %echo% ~%self% transforms into a bat and flies away!
    %purge% %self%
    halt
  else
    eval person_num %%random.%num_people%%%
    eval person %%person_%person_num%%%
    %send% %person% ~%self% lunges toward you, sinking ^%self% teeth into your neck!
    %echoaround% %person% ~%self% lunges toward ~%person%, sinking ^%self% teeth into ^%person% neck!
    dg_affect %person% HARD-STUNNED on 10
    wait 5 sec
    if %person.room% != %self.room%
      %echo% ~%self% transforms into a bat and flies away, looking confused.
      %purge% %self%
      halt
    end
    %send% %person% You shudder with ecstasy at the feeling of your precious blood leaving your body...
    %echoaround% %person% ~%person% shudders with ecstasy as ~%self% feeds from *%person%!
    wait 5 sec
    if %person.room% != %self.room%
      %echo% ~%self% transforms into a bat and flies away, looking confused.
      %purge% %self%
      halt
    end
    %echoaround% %person% ~%self% tears open ^%self% wrist with ^%self% teeth and drips blood into |%person% mouth!
    %send% %person% As the world turns black, you feel the taste of warm blood in your mouth...
    wait 1 sec
    %echoaround% %person% ~%person% sits up suddenly.
    * Attempt to sire.
    nop %person.vampire(1)%
    if !%person.vampire()%
      * Sire failed...
      %send% %person% You sit up suddenly, wondering why you're still alive.
      wait 1 sec
      %echo% ~%self% transforms into a bat and flies away, looking annoyed.
      %purge% %self%
      halt
    end
    * Sire succeeded.
    %send% %person% You sit up suddenly, craving blood!
    wait 1 sec
    %self.name% grins a bloody grin, transforms into a bat, and flies away!
    %purge% %self%
    halt
  end
end
~
#18490
Hobbit poetry~
0 btw 5
~
emote clears %self.hisher% throat.
wait 5 sec
switch %random.5%
  case 1
    say Upon the hearth the fire is red,
    wait 5 sec
    say Beneath the roof there is a bed;
    wait 5 sec
    say But not yet weary are our feet,
    wait 5 sec
    say Still round the corner we may meet
    wait 5 sec
    say A sudden tree or standing stone
    wait 5 sec
    say That none have seen but we alone.
    wait 5 sec
    say Tree and flower and leaf and grass,
    wait 5 sec
    say Let them pass! Let them pass!
    wait 5 sec
    say Hill and water under sky,
    wait 5 sec
    say Pass them by! Pass them by!
  break
  case 2
    say Still around the corner there may wait
    wait 5 sec
    say A new road or a secret gate,
    wait 5 sec
    say And though we pass them by today,
    wait 5 sec
    say Tomorrow we may come this way
    wait 5 sec
    say And take the hidden paths that run
    wait 5 sec
    say Towards the Moon or to the Sun.
    wait 5 sec
    say Apple, thorn, and nut and sloe
    wait 5 sec
    say Let them go! Let them go!
    wait 5 sec
    say Sand and stone and pool and dell,
    wait 5 sec
    say Fare you well! Fare you well!
  break
  case 3
    say Home is behind, the world ahead,
    wait 5 sec
    say And there are many paths to tread
    wait 5 sec
    say Through shadows to the edge of night,
    wait 5 sec
    say Until the stars are all alight.
    wait 5 sec
    say Then world behind and home ahead,
    wait 5 sec
    say We'll wander back to home and bed.
    wait 5 sec
    say Mist and twilight, cloud and shade,
    wait 5 sec
    say Away shall fade! Away shall fade!
    wait 5 sec
    say Fire and lamp, and meat and bread,
    wait 5 sec
    say And then to bed! And then to bed!
  break
  case 4
    say Roads go ever ever on,
    wait 5 sec
    say Over rock and under tree,
    wait 5 sec
    say By caves where never sun has shone,
    wait 5 sec
    say By streams that never find the sea;
    wait 5 sec
    say Over snow by winter sown,
    wait 5 sec
    say And through the merry flowers of June,
    wait 5 sec
    say Over grass and over stone,
    wait 5 sec
    say And under mountains of the moon.
  break
  case 5
    say Roads go ever ever on
    wait 5 sec
    say Under cloud and under star,
    wait 5 sec
    say Yet feet that wandering have gone
    wait 5 sec
    say Turn at last to home afar.
    wait 5 sec
    say Eyes that fire and sword have seen
    wait 5 sec
    say And horror in the halls of stone
    wait 5 sec
    say Look at last on meadows green
    wait 5 sec
    say And trees and hills they long have known.
  break
done
wait 5 sec
emote falls silent.
~
#18491
DeLorean outta nowhere~
2 bw 1
~
%echo% Suddenly, there is a loud crash and a flash of blue-white light, and a strange silver chariot appears out of nowhere!
%load% veh 18491
wait 3 sec
%echo% A strangely-dressed old man steps out of the strange silver chariot.
%load% mob 18491
set mob %self.people%
if %mob%
  nop %mob.add_mob_flag(SILENT)%
end
wait 5
if !%mob%
  * ???
else
  * Room-specific comment
  switch %self.template%
    case 18464
      %force% %mob% say Great Scott! All this popcorn and me without my threedee glasses!
    break
    case 18468
      %force% %mob% say Great Scott! I've traveled back to one-point-oh!
    break
    case 18471
      %force% %mob% say Great Scott! I've gone back to the future!
    break
    case 18472
      %force% %mob% say Great Scott! This is the Oregon Trail!
    break
    default
      * Default goes here
    break
  done
end
wait 2 sec
set person %self.people%
while %person%
  if %person.is_pc% && !%person.inventory(18490)%
    %load% obj 18490 %person% inv
    set item %person.inventory(18490)%
    if %item%
      %send% %person% ~%mob% says, 'Oh! These are yours.'
      %send% %person% ~%mob% gives you @%item%.
    end
  elseif %person.vnum% == 18492
    set doctor %person%
    nop %doctor.add_mob_flag(SILENT)%
  end
  set person %person.next_in_room%
done
if %doctor%
  wait 2 sec
  %force% %mob% say Nice TARDIS.
  wait 2 sec
  %force% %doctor% say Nice car.
  nop %doctor.remove_mob_flag(SILENT)%
end
nop %mob.remove_mob_flag(SILENT)%
detach 18491 %self.id%
~
#18492
Tardis appears~
2 bw 1
~
%echo% You hear a strange, grinding roar...
wait 1 sec
%echo% A tall blue box slowly fades into view.
%load% veh 18492
wait 3 sec
%echo% A strange man steps out of the box.
%load% mob 18492
set mob %self.people%
if %mob%
  nop %mob.add_mob_flag(SILENT)%
end
wait 5
if !%mob%
  * ???
else
  * Room-specific comment
  switch %self.template%
    case 18463
      %force% %mob% say Don't worry about stepping on anything. This seems to be a fixed point in time.
    break
    case 18465
      %force% %mob% say Oy! What sort of game is this?
    break
    case 18466
      %force% %mob% say These are the sort of animals that would make even an angel weep.
    break
    case 18470
      %force% %mob% say I live in a vault now. Vaults are cool.
    break
    case 18471
      %force% %mob% say Ah, the 21st century. Although we're a little early for the Anti-Grav Olympics.
    break
    default
      * Default goes here
    break
  done
end
wait 2 sec
set person %self.people%
while %person%
  if %person.is_pc% && !%person.inventory(18493)%
    %load% obj 18493 %person% inv
    set item %person.inventory(18493)%
    if %item%
      %send% %person% ~%mob% gives you @%item%.
    end
  elseif %person.vnum% == 18491
    set doc_brown %person%
    nop %doc_brown.add_mob_flag(SILENT)%
  end
  set person %person.next_in_room%
done
if %doc_brown%
  wait 2 sec
  %force% %doc_brown% say Nice TARDIS.
  wait 2 sec
  %force% %mob% say Nice car.
  nop %doc_brown.remove_mob_flag(SILENT)%
end
nop %mob.remove_mob_flag(SILENT)%
detach 18492 %self.id%
~
#18494
Doctor Who?~
0 d 0
Doctor Who~
wait 2
say It's just The Doctor.
~
#18495
Change portal destination~
1 c 2
destination~
set cupboard 18462
set midgaard 18463
set popcorn 18464
set cave 18465
set unstable 18466
set hobbit 18467
set empire 18468
set closet 18469
set vault 18470
set modern 18471
set trail 18472
set precipice 18473
eval test %%%arg%%%
if %test%
  eval arg %test%
end
if !%actor.is_immortal%
  %send% %actor% You can't use @%self%. You need to be an immortal.
  halt
end
if !%instance%
  %send% %actor% You need to be in an Unstable Portal instance to use @%self%.
  return 1
  halt
end
set start_room %instance.start%
if %start_room.template% != 18460
  %send% %actor% You need to be in an Unstable Portal instance to use @%self%.
  return 1
  halt
end
* Get a room for the inside of the portal
set room_vnum %arg%
if %room_vnum% < 18462 || %room_vnum% > 18499
  %send% %actor% Invalid room vnum.
  return 1
  halt
end
makeuid new_room room i%room_vnum%
if !%new_room%
  %send% %actor% That doesn't seem to be a valid room vnum.
  halt
end
%door% %start_room% down room %new_room%
* Make sure we don't add a duplicate portal
set item %new_room.contents%
while %item%
  if %item.vnum% == 18461
    %purge% %item%
  end
  set item %item.next_in_list%
done
%at% %new_room% %load% obj 18461
set targ %instance.location%
set num %targ.vnum%
set item %new_room.contents%
nop %item.val0(%num%)%
* Retarget the outside portal to the new room
set loc %instance.location%
set item %loc.contents%
while %item%
  if %item.vnum% == 18460
    set targ %new_room%
    set num %targ.vnum%
    nop %item.val0(%num%)%
  end
  set item %item.next_in_list%
done
%send% %actor% You link the unstable portal to %new_room.name%.
%echoaround% %actor% ~%actor% links the unstable portal to ~%new_room.name%.
~
#18496
Unstable portal block where~
2 c 0
where~
%send% %actor% You don't even know where YOU are!
return 1
~
#18497
Unstable Portal: Precipice: Grafted scion kill~
0 z 100
~
* mark player killed
if %actor.is_pc% && %self.room.template% == 18473
  set killed_%actor.id% 1
  remote killed_%actor.id% %self.room.id%
end
* check for more players
set valid_pos Standing Fighting Sitting Resting Sleeping
set ch %self.room.people%
set found 0
while %ch% && !%found%
  if %ch.is_pc% && %valid_pos% ~= %ch.position%
    set found 1
  end
  set ch %ch.next_in_room%
done
if !%found%
  wait 5 sec
  %echo% ~%self% skitters out of sight.
  %purge% %self%
end
~
$
