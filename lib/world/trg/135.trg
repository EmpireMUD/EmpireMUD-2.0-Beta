#13500
Labyrinth: Difficulty selector / sealed passage~
1 c 4 36
L b 13505
L b 13510
L b 13520
L b 13525
L b 13530
L b 13535
L b 13536
L b 13538
L b 13539
L b 13541
L b 13542
L b 13544
L b 13545
L b 13547
L b 13548
L b 13550
L b 13551
L b 13553
L b 13556
L c 13501
L c 13503
L c 13504
L c 13507
L c 13511
L c 13515
L c 13516
L j 13501
L j 13505
L j 13510
L j 13515
L j 13520
L j 13570
L j 13575
L j 13580
L j 13590
L w 13525
difficulty~
* Process argument
if !%arg%
  %send% %actor% You must specify a level of difficulty (normal, hard, group, or boss).
  return 1
  halt
end
if normal /= %arg%
  %echo% Setting difficulty to Normal...
  set diff 1
elseif hard /= %arg%
  %echo% Setting difficulty to Hard...
  set diff 2
elseif group /= %arg%
  %echo% Setting difficulty to Group...
  set diff 3
elseif boss /= %arg%
  %echo% Setting difficulty to Boss...
  set diff 4
else
  %send% %actor% That is not a valid difficulty level for this adventure.
  halt
  return 1
end
* Messaging
%echo% The floor drops out from under you!
* exit
if !%self.room.down(room)%
  %door% %self.room% down room i13501
end
* Check for rope
set rope %self.room.contents(13511)%
if %rope%
  * rope attached, no drop
  %purge% %rope%
  %load% obj 13507 %self.room%
  %echo% ... luckily you catch yourself on the rope as it drops down the chute.
else
  * no rope: send everyone down
  set ch %self.room.people%
  while %ch%
    set next_ch %ch.next_in_room%
    if !%ch.is_flying%
      %echoaround% %ch% ~%ch% falls down the chute!
      %send% %ch% You fall down the chute! You can't seem to find your footing...
      %teleport% %ch% i13501
      * look will be handled by the greet script in
    end
    set ch %next_ch%
  done
end
* Set up the adventure
set route_list 13570 13575 13580
set pos %random.3%
while %pos% > 0
  if %pos% == 1
    set route %route_list.car%
    set route_list %route_list.cdr%
  else
    set route_list %route_list.cdr% %route_list.car%
  end
  eval pos %pos% - 1
done
if %random.2% == 2
  set mini_loc %route_list.cdr%
else
  set mini_loc %route_list.car%
end
switch %random.3%
  case 1
    set mini_vnum 13520
  break
  case 2
    set mini_vnum 13525
  break
  case 3
    set mini_vnum 13530
  break
done
* Store route to the campsite
makeuid campsite room i13505
remote route %campsite.id%
* Link boss room
makeuid linkroom room i%route%
makeuid bossroom room i13590
%at% %linkroom% %load% obj 13503
set in_port %linkroom.contents(13503)%
if %in_port%
  nop %in_port.val0(%bossroom.vnum%)%
end
set out_port %bossroom.contents(13504)%
if %out_port%
  nop %out_port.val0(%linkroom.vnum%)%
end
set jar %bossroom.contents(13515)%
if %jar%
  remote diff %jar.id%
end
* Boss
set boss %bossroom.people(13510)%
if %boss%
  nop %boss.remove_mob_flag(HARD)%
  nop %boss.remove_mob_flag(GROUP)%
  if %diff% == 2 || %diff% == 4
    nop %boss.add_mob_flag(HARD)%
  end
  if %diff% >= 3
    nop %boss.add_mob_flag(GROUP)%
  end
  remote diff %boss.id%
  nop %boss.unscale_and_reset%
end
* Put this body anywhere; it moves itself
%at% %bossroom% %load% obj 13516
* Miniboss
makeuid where room i%mini_loc%
if %where%
  %at% %where% %load% mob %mini_vnum%
  set miniboss %where.people(%mini_vnum%)%
  if %miniboss%
    nop %miniboss.link_instance%
    nop %miniboss.remove_mob_flag(HARD)%
    nop %miniboss.remove_mob_flag(GROUP)%
    if %diff% == 2 || %diff% == 4
      nop %miniboss.add_mob_flag(HARD)%
    end
    if %diff% >= 3
      nop %miniboss.add_mob_flag(GROUP)%
    end
    remote diff %miniboss.id%
    if %mini_vnum% == 13525
      dg_affect #13525 %miniboss% IMMUNE-WHERE on -1
    end
    nop %miniboss.unscale_and_reset%
  end
end
* Trash mobs
set trash_mob_list 13535 13538 13541 13544 13547 13550 13553 13556
set list_size 8
set trash_start_rooms 13510 13515 13520
while %trash_start_rooms%
  makeuid where room i%trash_start_rooms.car%
  set trash_start_rooms %trash_start_rooms.cdr%
  * choose
  eval pos %%random.%list_size%%%
  while %pos% > 1
    set trash_mob_list %trash_mob_list.cdr% %trash_mob_list.car%
    eval pos %pos% - 1
  done
  set mobv %trash_mob_list.car%
  eval elitev %mobv% + 1
  set trash_mob_list %trash_mob_list.cdr%
  eval list_size %list_size% - 1
  * load spawner
  %at% %where% %load% mob 13505
  set mob %where.people(13505)%
  if %mob%
    remote mobv %mob.id%
    remote elitev %mob.id%
    remote diff %mob.id%
  end
done
*
* And done
%load% obj 13501
%purge% %self%
~
#13501
Labyrinth: Welcome to the chute~
2 gA 100 2
L c 13502
L c 13508
~
if !%room.contents(13508)% && !%actor.is_flying% && !%actor.inventory(13502)%
  %load% obj 13502 %actor% inv
end
~
#13502
Labyrinth: Chute helper object~
1 bn 100 3
L j 13501
L j 13505
L w 13502
~
set cycle 0
while %cycle% < 5
  set actor %self.carried_by%
  if !%actor%
    * gone
    %purge% %self%
    halt
  elseif %actor.is_flying% || %actor.room.template% != 13501 || %actor.room.contents(13508)%
    * suddenly flying
    dg_affect #13502 %actor% off
    %purge% %self%
    halt
  end
  * ensure stun
  if !%actor.affect(13502)%
    dg_affect #13502 @%actor% %actor% HARD-STUNNED on 15
    dg_affect #13502 @%actor% %actor% BLIND on 15
  end
  * current cycle
  switch %cycle%
    case 0
      * no message during cycle 0, already received the fall message
      wait 3 sec
    break
    case 1
      %send% %actor% You're falling faster and faster down the chute!
      wait 3 sec
    break
    case 2
      %send% %actor% You see a flicker of light, the chute opens up, and then there's a THUD!
      wait 1 sec
    break
    case 3
      %send% %actor% Everything goes black.
      wait 5 sec
    break
    case 4
      %send% %actor% When you come to, there's no way to tell how much time has passed.
      dg_affect #13502 %actor% off
      if !%actor.is_npc%
        * check hunger
        if !%actor.nohunger%
          nop %actor.hunger(24)%
        end
        * check thirst
        if !%actor.nothirst%
          nop %actor.thirst(24)%
        end
      end
      * move actor
      %teleport% %actor% i13505
      %echoaround% %actor% ~%actor% falls in from above!
      %purge% %self%
      halt
    break
  done
  eval cycle %cycle% + 1
done
~
#13503
Labyrinth: Detect not-flying in the chute~
2 bw 100 1
L c 13502
~
set actor %room.people%
while %actor%
  set next_actor %actor.next_in_room%
  if !%actor.is_flying% && !%actor.inventory(13502)%
    %load% obj 13502 %actor% inv
  end
  set actor %next_actor%
done
~
#13504
Labyrinth: Track the silver threads~
2 c 0 20
L c 13503
L c 13506
L j 13505
L j 13510
L j 13515
L j 13520
L j 13525
L j 13530
L j 13535
L j 13540
L j 13545
L j 13550
L j 13555
L j 13560
L j 13565
L j 13570
L j 13575
L j 13580
L o 73
L o 80
track~
* room lists
set wing_13570 13510 13525 13540 13555 13570
set wing_13575 13515 13530 13545 13560 13575
set wing_13580 13520 13535 13550 13565 13580
* check abils and arg
if !%actor.ability(Track)% || !%actor.ability(Navigation)% || !%actor.can_see_in_room%
  * Fail through to ability message
  return 0
  halt
elseif %arg% && !(minotaur /= %arg%) && !(exit /= %arg%) && !(labyrinth /= %arg%) && !(maze /= %arg%) && !(thread /= %arg%)
  * unrecognized arg, fall through to regular track
  return 0
  halt
end
* already a thread here?
set thread %room.contents(13506)%
if %thread% && %thread.var(dir)%
  %send% %actor% You find a delicate silver thread leading to the %thread.var(dir)%.
  halt
elseif %room.var(tracked)%
  * silver thread gone; fail through to real track
  if !%arg%
    %send% %actor% You find no tracks of note.
  else
    return 0
  end
  halt
end
* special case for final room
if %room.contents(13503)%
  %send% %actor% You find a delicate silver thread leading to the passage!
  %load% obj 13506 %room%
  set thread %room.contents(13506)%
  if %thread%
    %mod% %thread% longdesc A delicate silver thread leads to the passage.
    set dir passage
    remote dir %thread.id%
  end
  set tracked passage
  * block further track
  remote tracked %room.id%
  return 1
  halt
end
* special case for basecamp; all other rooms just find a higher template
if %room.template% == 13505
  * find room 13510, 13515, or 13520 based on 13570, 13575, or 13580 route target
  eval find_template %room.var(route,13570)% - 60
else
  * not the camp room
  set find_template 0
  * also ensure we're in a valid location
  makeuid camp room i13505
  eval valid_rooms %%wing_%camp.var(route,13570)%%%
  if !(%valid_rooms% ~= %room.template%)
    * wrong wing
    if !%arg%
      %send% %actor% You find no tracks of note.
    else
      return 0
    end
    halt
  end
end
* find the direction
set dir_list north east south west northeast northwest southeast southwest
set found 0
set lowest 0
set lowest_dir 0
while %dir_list%
  set dir %dir_list.car%
  set dir_list %dir_list.cdr%
  eval to_room %%room.%dir%(room)%%
  if %to_room%
    if %to_room.template% == %find_template%
      set found %dir%
    elseif %to_room.template% > %room.template% && !%find_template%
      set found %dir%
    elseif !%find_template% && (!%lowest% || %to_room.template% < %lowest%)
      set lowest_dir %dir%
      set lowest %to_room.template%
    end
  end
done
if %found%
  %send% %actor% You find a delicate silver thread leading to the %found%!
  %load% obj 13506 %room%
  set thread %room.contents(13506)%
  if %thread%
    if %lowest_dir%
      %mod% %thread% longdesc A delicate silver thread leads from the %lowest_dir% to the %found%.
    else
      %mod% %thread% longdesc A delicate silver thread leads to the %found%.
    end
    set dir %found%
    remote dir %thread.id%
  end
  set tracked %found%
  * block further track
  remote tracked %room.id%
  return 1
else
  * did not find any valid room
  if !%arg%
    %send% %actor% You find no tracks of note.
  else
    return 0
  end
end
~
#13505
Labyrinth: Trash Spawner~
0 n 100 21
L b 13545
L b 13551
L b 13557
L j 13510
L j 13514
L j 13515
L j 13520
L j 13524
L j 13525
L j 13529
L j 13530
L j 13534
L j 13535
L j 13539
L j 13540
L j 13544
L j 13545
L j 13547
L j 13548
L j 13549
L j 13550
~
##
* This mob loads in the start room of a wing of the Long Lost Labyrinth and has
* no function anywhere else. It also requires a 'mobv' variable to tell it what
* it's spawning, 'elitev' for the elite mob vnum, and 'diff' to pass difficulty.
*
wait 1
nop %self.link_instance%
set mobv %self.var(mobv)%
set elitev %self.var(elitev)%
set diff %self.var(diff,1)%
if !%mobv%
  * oops
  %log% syslog script Trig 13505: mobv variable not set.
  %purge% %self%
  halt
end
*
* Configs: must set 'elite' to 1 room where the elite trash mob spawns,
* set 'list' to other rooms that may have trash mobs
switch %self.room.template%
  case 13510
    set elite 13540
    set list 13510 13525 13524 13539
    set size 4
  break
  case 13515
    set elite 13545
    set list 13515 13530 13514 13529 13544
    set size 5
  break
  case 13520
    set elite 13550
    set list 13520 13535 13534 13547 13548 13549
    set size 6
  break
  default
    * unknown location
    %purge% %self%
    halt
  break
done
*
* Travel the path, spawning as we go
eval room_count 1 + %diff%
while %room_count% > 0 && %size% > 0
  eval pos %%random.%size%%%
  while %pos% > 1
    set list %list.cdr% %list.car%
    eval pos %pos% - 1
  done
  set room_vnum %list.car%
  set list %list.cdr%
  eval size %size% - 1
  * move
  mgoto i%room_vnum%
  * load 1
  %load% mob %mobv%
  set mob %self.room.people%
  remote diff %mob.id%
  if %diff% == 4
    nop %mob.add_mob_flag(TANK)%
  end
  * load 2?
  if %diff% > 2
    %load% mob %mobv%
    set mob %self.room.people%
    remote diff %mob.id%
    if %diff% == 4
      nop %mob.add_mob_flag(TANK)%
    end
  end
  eval room_count %room_count% - 1
done
* and elite
if %elite% && %elitev%
  mgoto i%elite%
  %load% mob %elitev%
  set mob %self.room.people%
  remote diff %mob.id%
  switch %diff%
    case 2
      if %elitev% == 13551 || %elitev% == 13557
        nop %mob.add_mob_flag(DPS)%
      end
      if %elitev% != 13545 && %elitev% != 13551
        nop %mob.add_mob_flag(TANK)%
      end
    break
    case 3
      if %elitev% != 13545
        nop %mob.add_mob_flag(HARD)%
      end
    break
    case 4
      if %elitev% == 13551 || %elitev% == 13557
        nop %mob.add_mob_flag(DPS)%
      end
      if %elitev% != 13545
        nop %mob.add_mob_flag(HARD)%
      end
      nop %mob.add_mob_flag(TANK)%
    break
  done
end
%purge% %self%
~
#13506
Labyrinth: Get silver thread and reset it~
1 g 100 1
L f 13506
~
rdelete dir %self.id%
%mod% %self% longdesc -
detach 13506 %self.id%
~
#13507
Labyrinth: Up requires flying~
2 q 100 2
L c 13508
L c 13509
~
if %room.contents(13508)% || %room.contents(13509)%
  * rope: ok
  return 1
elseif !%actor.is_npc% && !%actor.is_flying% && %direction% == up
  %send% %actor% You can't reach the ceiling to go up!
  return 0
end
~
#13508
Labyrinth: Attach rope over chute~
2 c 0 9
L c 13502
L c 13507
L c 13508
L c 13509
L c 13511
L j 13501
L j 13505
L w 6880
L w 13502
use attach tie~
return 0
* qualify us to stay first: must target a rope item
set obj %actor.obj_target_inv(%arg.argument1%)%
if !%obj%
  halt
elseif !%obj.is_component(6880)%
  halt
end
* already one here?
if %room.contents(13507)% || %room.contents(13511)%
  %send% %actor% There's already a rope attached here.
  return 1
  halt
end
* otherwise looks like we're ok! top first:
if %room.down(room)%
  * chute open
  %load% obj 13507
  %send% %actor% You tie @%obj% securely and lower it down the chute.
  %echoaround% %actor% ~%actor% ties @%obj% securely and lowers it down the chute.
else
  * chute closed
  %load% obj 13511
  %send% %actor% You tie @%obj% securely and coil it on the ground near the seal.
  %echoaround% %actor% ~%actor% ties @%obj% securely and coils it on the ground near the seal.
end
* downstairs
makeuid camp room i13505
%at% %camp% %load% obj 13509
%at% %camp% %echo% A rope drops down from the chute above!
* and the complicated middle portion
makeuid chute room i13501
%at% %chute% %load% obj 13508
%at% %chute% %echo% A rope drops down the chute from above!
set ch %chute.people%
while %ch%
  set helper %ch.inventory(13502)%
  if %helper%
    dg_affect #13502 %actor% off
    %send% %actor% You grab the rope and manage to stop your fall!
    %purge% %helper%
  end
  set ch %ch.next_in_room%
done
* done
return 1
%purge% %obj%
~
#13509
Labyrinth: Door commands~
1 c 4 7
L c 13512
L c 13513
L j 13530
L j 13534
L o 96
L o 97
L q 4
open close kick bash unlock pick~
set will_open 0
set broke 0
*
if %cmd% == open
  if !(door /= %arg%)
    * fall thru to regular command
    return 0
  else
    %send% %actor% It's locked.
  end
elseif %cmd% == close
  if !(door /= %arg%)
    * fall thru to regular command
    return 0
  else
    %send% %actor% It's already closed.
  end
elseif (kick /= %cmd% || break /= %cmd%) && %actor.ability(96)%
  if !(door /= %arg%)
    * fall thru to regular command
    return 0
  else
    %send% %actor% You plant one foot firmly and then kick in the door!
    %echoaround% %actor% ~%actor% plants one foot firmly and then kicks in the door!
    set otherside There's a loud BANG as the door kicks open!
    set will_open 1
    set broke 1
  end
elseif (bash /= %cmd% || break /= %cmd%) && %actor.ability(97)%
  if !(door /= %arg%)
    * fall thru to regular command
    return 0
  else
    %send% %actor% You bash the door down!
    %echoaround% %actor% ~%actor% bashes the door down!
    set otherside There's a loud BANG as the door flies open!
    set will_open 1
    set broke 1
  end
elseif unlock /= %cmd%
  * find key for correct instance
  set key 0
  set bad_key 0
  set obj %actor.inventory(13512)%
  while %obj% && !%key%
    if %obj.vnum% == 13512
      if %obj.var(instance_id)% == %instance.id% && %obj.var(location)% == %instance.location%
        set key %obj%
      else
        set bad_key %obj%
      end
    end
    set obj %obj.next_in_list%
  done
  *
  if !%arg%
    %send% %actor% Unlock what?
  elseif !(door /= %arg%)
    %send% %actor% You can't unlock that.
  elseif !%key%
    if %bad_key%
      %send% %actor% The key doesn't quite fit in this door. It must be for a different long-lost labyrinth.
    else
      %send% %actor% You don't seem to have the right key.
    end
  else
    %send% %actor% You use @%key% to unlock the door, and open it.
    %send% %actor% The key sticks in the lock; you can't get it back out.
    %echoaround% %actor% ~%actor% unlocks the door and opens it.
    set otherside There's a click, and the door opens.
    set will_open 1
    %purge% %key%
  end
elseif pick /= %cmd%
  if !%arg%
    %send% %actor% Pick what?
  elseif !(door /= %arg%) && !(lock /= %arg%)
    %send% %actor% You can't pick that.
  elseif %actor.skill(Stealth)% < 50
    %send% %actor% You don't really know how to pick a lock.
  else
    %send% %actor% You peer around and then carefully pick the door lock, and open the door.
    %echoaround% %actor% ~%actor% peers around and then picks the door lock with a small tool, and opens the door.
    set otherside The door swings open.
    set will_open 1
  end
end
* are we opening?
if %will_open%
  set room %self.room%
  if %room.template% == 13530
    makeuid other room i13534
  else
    makeuid other room i13530
  end
  if !%other%
    %send% %actor% Door error.
    halt
  end
  * find a good direction
  set my_dir north east south west northeast northwest southeast southwest
  set other_dir south west north east southwest southeast northwest northeast
  set my_found 0
  while %my_dir% && !%my_found%
    set this %my_dir.car%
    set that %other_dir.car%
    set my_dir %my_dir.cdr%
    set other_dir %other_dir.cdr%
    *
    eval my_exists %%room.%this%(room)%%
    eval other_exists %%other.%that%(room)%%
    if !%my_exists% && !%other_exists%
      set my_found %this%
      set other_found %that%
    end
  done
  if !%my_found%
    %send% %actor% The labyrinth door seems to be broken. Report this as a bug.
    halt
  end
  * message
  if %otherside%
    %at% %other% %echo% %otherside%
  end
  * this side
  %door% %room% %my_found% room %other.vnum%
  * that side
  %door% %other% %other_found% room %room.vnum%
  * re-closable?
  if %broke%
    %mod% %room% append-description There's a doorway in the brick wall.
    %mod% %other% append-description There's a doorway in the brick wall.
  else
    %mod% %room% append-description A heavy door is set in the brick wall.
    %door% %room% %my_found% flags a
    %door% %room% %my_found% name door
    %mod% %other% append-description A heavy door is set in the brick wall.
    %door% %other% %other_found% flags a
    %door% %other% %other_found% name door
  end
  set obj %other.contents(13513)%
  if %obj%
    %purge% %obj%
  end
  * and I'm out
  %purge% %self%
end
~
#13511
Labyrinth: Shared death trigger~
0 fA 100 24
L b 13510
L b 13512
L b 13520
L b 13525
L b 13526
L b 13527
L b 13530
L b 13535
L b 13536
L b 13538
L b 13539
L b 13544
L b 13545
L b 13547
L b 13548
L b 13550
L b 13551
L b 13553
L b 13554
L c 13515
L c 13521
L c 13522
L j 13590
L j 13592
~
set miniboss 0
switch %self.vnum%
  case 13510
    * Minotaur
    * Death roar
    %subecho% %self.room% The minotaur ROARS out!
    %echo% He staggers and then collapses to the floor.
    %subecho% %self.room% The entire labyrinth shakes with a mighty THUMP!
    return 0
    *
    * Fuzzy goblin present?
    set goblin %instance.mob(13526)%
    if %goblin%
      %at% %goblin.room% %echo% ~%goblin% cheers and darts away down the corridor.
      %purge% %goblin%
      %at% i13592 %load% mob 13527
      set gobtwo %instance.mob(13527)%
      if %gobtwo%
        %at% %gobtwo.room% %echo% ~%gobtwo% comes scrambling up the ladder.
        if %self.mob_flagged(HARD)%
          nop %gobtwo.add_mob_flag(HARD)%
        end
        if %self.mob_flagged(GROUP)%
          nop %gobtwo.add_mob_flag(GROUP)%
        end
      end
    end
  break
  case 13512
    * Nightmare Queen
    %echo% ~%self% throws her head back and convulses...
    %subecho% %self.room% The terrifying death whinny of the Nightmare Queen echoes out through the labyrinth!
    %echo% &%self% explodes in a cloud of glimmering ash that falls like snow to cover everything in the chamber!
    %load% obj 13522 %self.room%
    return 0
  break
  case 13520
    * Wily adventurer
    set miniboss 1
    %echo% ~%self% falls on ^%self% makeshift torch, snuffing it.
    return 1
  break
  case 13525
    * gigantic goblin rat
    set miniboss 1
    * oh no
    %echo% The rat shrinks as it dies, turning into a fuzzy little goblin just as she exhales for the last time!
    return 0
  break
  case 13530
    * horned champion
    set miniboss 1
    return 1
  break
  case 13535
  case 13536
    * pale green skeleton, polished bronze skeleton
    %echo% ~%self% collapses in a pile.
    return 0
  break
  case 13538
  case 13539
    * shadow of the dead, wailing shadow
    %echo% The shadow fades away into the darkness.
    return 0
  break
  case 13544
    * bone slime
    %echo% The bone slime splashes to the ground with a clatter!
    return 0
  break
  case 13545
    * death cube
    %echo% The gelatinous cube splashes to the floor and soaks into the seams, leaving you gasping!
    return 0
  break
  case 13547
  case 13548
    * silver mirror spider, gold spider
    %echo% ~%self% lets out one final shriek as it falls onto its back and curls up its legs!
    return 0
  break
  case 13550
  case 13551
    * giant termite, monstrous centipede
    %echo% ~%self% rolls over onto its back and curls up its legs!
    return 0
  break
  case 13553
    * blind cave pixies
    %echo% The pixies fall to the ground, dead!
    return 0
  break
  case 13554
    * wokestone guardian
    %echo% ~%self% falls over backward, crushing the pixy with a THUD!
    return 0
  break
done
* update fiend seal if miniboss
if %miniboss%
  %load% obj 13521 %self%
  makeuid bossroom room i13590
  if %bossroom%
    set jar %bossroom.contents(13515)%
    if %jar%
      set fiend_done 1
      remote fiend_done %jar.id%
    end
  end
end
~
#13513
Labyrinth: Boss subzone threat spammer~
0 b 6 3
L b 13510
L b 13512
L j 13505
~
if %self.fighting% || %self.disabled% || %self.room.template% < 13505 || %self.room.template% >= 13599
  halt
elseif %self.vnum% == 13510
  * cyclopean minotaur
  set last %self.var(last,0)%
  set roll %random.6%
  while %roll% == %last%
    set roll %random.6%
  done
  set last %roll%
  remote last %self.id%
  *
  switch %roll%
    case 1
      %echo% ~%self% stomps ^%self% foot hard on the stone floor...
      %subecho% %self.room% Sand trickles from the ceiling as the entire labyrinth shakes!
    break
    case 2
      %echo% ~%self% throws ^%self% head back and roars out...
      %subecho% %self.room% A bestial roar echoes through the halls of the labyrinth!
    break
    case 3
      %echo% ~%self% slams ^%self% fists into the wall...
      %subecho% %self.room% A tremendous impact resonates through the labyrinth -- even the sand on the floor jumps!
    break
    case 4
      %echo% ~%self% pounds ^%self% chest with ^%self% fists and bellows!
      %subecho% %self.room% A deep and ominous sound rolls through the labyrinth like thunder!
    break
    case 5
      %echo% ~%self% picks up a clay jar and hurls it at you -- it shatters on the wall just above you, showering you in ash!
      %subecho% %self.room% A shattering noise pierces the air over and over again as it echoes through the halls!
    break
    case 6
      %echo% ~%self% grabs ^%self% ivory horns and lets out a bestial growl...
      %subecho% %self.room% A deep rumble rolls through the stone beneath your feet.
    break
  done
else
  * nightmare queen
  set last %self.var(last,0)%
  set roll %random.6%
  while %roll% == %last%
    set roll %random.6%
  done
  set last %roll%
  remote last %self.id%
  *
  switch %roll%
    case 1
      %echo% ~%self% casts back ^%self% head and whinnies so loudly you instinctively cover your ears!
      %subecho% %self.room% Your hair stands on end as a terrible death whinny echoes through the labyrinth!
    break
    case 2
      %echo% Plumes of flame erupt from |%self% nostrils!
      %subecho% %self.room% A low roar echoes through the halls.
    break
    case 3
      %subecho% %self.room% The thundering roll of hoofbeats echoes through the labyrinth.
    break
    case 4
      %echo% ~%self% whispers your name...
      %subecho% %self.room% An ominous whisper echoes through the labyrinth.
    break
    case 5
      %echo% Flame bellows from |%self% mouth as she raises her head toward the ceiling and laughs.
      %subecho% %self.room% The winnowing sound of woman's laughter echoes through the air!
    break
    case 6
      %echo% ~%self% snorts and shakes ^%head%...
      %subecho% %self.room% You feel warm breath on the back of your neck.
    break
    case 7
      %subecho% %self.room% You hear hoofbeats behind you, but when you turn, it's just the wall.
    break
    case 8
      %echo% ~%self% closes ^%self% eyes and beings to hum...
      %subecho% %self.room% An ancient lullaby echoes softly through the halls.
    break
  done
end
~
#13515
Labyrinth: Boss must-fight trigger~
0 q 100 2
L b 13512
L o 29
~
if %method% != move && %method% != portal
  * don't block other methods
  return 1
elseif %direction% == up && (!%actor.is_flying% || %self.vnum% == 13512)
  %send% %actor% You can't get past ~%self%!
  if !%self.fighting%
    %aggro% %actor%
  end
  return 0
elseif %method% == portal && !%actor.ability(29)% && !%actor.aff_flagged(SNEAK)%
  %send% %actor% You try to escape, but ~%self% stops you!
  if !%self.fighting%
    %aggro% %actor%
  end
  return 0
else
  return 1
end
~
#13516
Labyrinth: Hapless adventurer body setup~
1 n 100 16
L b 13510
L c 13593
L c 13594
L c 13595
L c 13596
L c 13597
L c 13598
L j 13523
L j 13548
L j 13553
L j 13559
L j 13564
L j 13570
L j 13575
L j 13580
L j 13590
~
set room_list 13523 13548 13553 13559 13564 13570 13575 13580 13590
set length 9
set spikes_list 13523
* random spot
eval pos %%random.%length%%
while %pos% > 0
  set roomv %room_list.car%
  set room_list %room_list.cdr%
  eval pos %pos% - 1
done
* move, if in an adventure (otherwise just updates itself in-place)
if %instance.id% && %roomv%
  if %random.2% == 2
    * only half of labyrinths get a body
    %purge% %self%
    halt
  end
  %teleport% %self% i%roomv%
end
* restring
switch %random.6%
  case 1
    * tank
    %mod% %self% keywords body corpse tombsward warden hapless adventurer
    set longdesc The body of a tombsward
    %mod% %self% lookdesc The late adventurer is draped in a heavy canvas cloak bearing a spiral symbol. His armor was made from black metal with bronze filigree accents; none of it is in good condition.
    %mod% %self% append-lookdesc He seems well-equipped for this delve, but it ended poorly.
    %load% obj 13593 %self%
  break
  case 2
    * healer
    %mod% %self% keywords body corpse silverseer seer hapless adventurer
    set longdesc The body of a silverseer
    %mod% %self% lookdesc She seems to have met a grisly end, with dark and dried bloodstains across her pale gray and white robes.
    %mod% %self% append-lookdesc The frilled margins of the robes bear many small bronze discs, each with the shape of an eye inside a spiral, and a spiral in the eye.
    %load% obj 13594 %self%
  break
  case 3
    * melee
    %mod% %self% keywords body corpse shadow vaulter hapless adventurer
    set longdesc The body of a shadow vaulter
    %mod% %self% lookdesc She has no supplies -- at least, none you can find in the many, many pockets of her black leather outfit. Her body is emaciated; she may have starved down here.
    %mod% %self% append-lookdesc The underside of her heavy cloak is stitched with a circular emblem depicting a shadowy figure standing in a stone archway flanked by large wings, beneath a downward-facing crescent moon.
    %load% obj 13595 %self%
  break
  case 4
    * caster
    %mod% %self% keywords body corpse barrow weaver hapless adventurer
    set longdesc The body of a barrow weaver
    %mod% %self% lookdesc The poor man met a dim end in the depths. He's decked out in layered green and violet robes and green cape, both stitched with intricate white and bronze vines and leaves.
    %mod% %self% append-lookdesc The cape has a large, round emblem of a skull between the roots and branches of a tree, flanked on the left by the crescent moon and star, and on the right by the sun.
    %load% obj 13596 %self%
  break
  case 5
    * pvp/solo
    %mod% %self% keywords body corpse dolmen freeblade blade hapless adventurer
    set longdesc The body of a dolmen freeblade
    %mod% %self% lookdesc She's easy to miss at first, in dark gray and black compound armor. Pockets and pouches adorn the outfit, but they're all empty. An emblem over her heart shows a sword and dagger crossed beneath a trilithon.
    %load% obj 13597 %self%
  break
  case 6
    * empire
    %mod% %self% keywords body corpse man well-dressed dressed hapless adventurer
    set longdesc The body of a well-dressed man
    %mod% %self% lookdesc He's dressed as a noble to be sure, with fur-lined boots and gloves, and a purple cloak stitched in gold and bronze. His belt and pack are missing. It looks like he starved to death in the dark.
    %load% obj 13598 %self%
  break
done
* flagging
set minotaur %instance.mob(13510)%
set clothing %self.contents%
if %minotaur%
  if %minotaur.mob_flagged(HARD)%
    nop %clothing.flag(HARD-DROP)%
  end
  if %minotaur.mob_flagged(GROUP)%
    nop %clothing.flag(GROUP-DROP)%
  end
end
%scale% %clothing% 1
* additions
if %spikes_list% ~= %roomv%
  %mod% %self% longdesc %longdesc% is impaled on the spikes.
  %mod% %self% append-lookdesc It's a rather grisly site the way it's impaled on the spikes.
else
  %mod% %self% longdesc %longdesc% is sprawled out on the floor.
end
~
#13517
Labyrinth: Clothes off my back~
1 c 6 1
L f 13517
get take~
* make corpse naked if empty
return 0
wait 1
if !%self.contents%
  %mod% %self% keywords body corpse naked adventurer hapless
  %mod% %self% shortdesc the naked body of a hapless adventurer
  %mod% %self% longdesc A hapless adventurer lies naked on the ground.
  %mod% %self% lookdesc Someone has stolen this poor adventurer's clothing.
  detach 13517 %self.id%
end
~
#13518
Labyrinth: Open the great jar~
1 c 4 2
L b 13510
L b 13512
open close~
if %actor.obj_target(%arg.argument1)% != %self%
  if %arg.argument1% == jars || smaller /= %arg.argument1%
    %send% %actor% You open some of the smaller jars, but they contain only ash.
  else
    * targeting something else
    return 0
  end
  halt
end
*
if %cmd% == open
  if %self.var(open)%
    %send% %actor% The great jar is already open.
  elseif %room.people(13510)%
    %send% %actor% You can't get close enough to the jar with the minotaur in the way!
  elseif !%self.var(water_done)% && !%self.var(blood_done)% && !%self.var(fiend_done)%
    %send% %actor% You can't get the great jar open.
  elseif !%self.var(water_done)% && !%self.var(blood_done)%
    %send% %actor% You can't get the great jar open... the water seal and blood seal are still intact.
  elseif !%self.var(blood_done)% && !%self.var(fiend_done)%
    %send% %actor% You can't get the great jar open... the blood seal and fiend seal are still intact.
  elseif !%self.var(water_done)% && !%self.var(fiend_done)%
    %send% %actor% You can't get the great jar open... the water seal and fiend seal are still intact.
  elseif !%self.var(water_done)%
    %send% %actor% You can't get the great jar open... the water seal is still intact.
  elseif !%self.var(blood_done)%
    %send% %actor% You can't get the great jar open... the blood seal is still intact.
  elseif !%self.var(fiend_done)%
    %send% %actor% You can't get the great jar open... the fiend seal is still intact.
  else
    * ok
    %send% %actor% You summon all your might and push the heavy lid off the great jar... it falls to the ground with a CRASH!
    %echoaround% %actor% ~%actor% plants ^%actor% feet and heaves ^%actor% weight against the lid of the great jar... which falls to the ground with a CRASH!
    wait 1 sec
    %echo% A terrible death whinny echoes out of the great jar as swirls of color fly out and streak up the chamber!
    wait 1 sec
    %load% mob 13512
    set mob %self.room.people(13512)%
    set diff %self.var(diff,1)%
    remote diff %mob.id%
    nop %mob.remove_mob_flag(HARD)%
    nop %mob.remove_mob_flag(GROUP)%
    if %diff% == 2 || %diff% == 4
      nop %mob.add_mob_flag(HARD)%
    end
    if %diff% >= 3
      nop %mob.add_mob_flag(GROUP)%
    end
    nop %mob.link_instance%
    nop %mob.unscale_and_reset%
    %echo% ~%mob% arises from the ashes!
    set open 1
    remote open %self.id%
  end
elseif %cmd% == close
  if %self.var(open)%
    %send% %actor% The lid is broken! There's no way to close it!
  else
    %send% %actor% The great jar is already closed.
  end
end
~
#13519
Labyrinth: Shared obj load script (key, spike pit, fiend seal)~
1 n 100 9
L c 13512
L c 13515
L c 13519
L c 13521
L c 13523
L j 13500
L j 13523
L j 13590
L w 13523
~
if %self.vnum% == 13512
  * tarnished key
  * store exact instance so the key only works in 1
  set instance_id %instance.id%
  remote instance_id %self.id%
  set location %instance.location%
  remote location %self.id%
elseif %self.vnum% == 13523
  * pit helper
  set actor %self.carried_by%
  if %actor%
    wait 1
    if %actor.is_flying%
      %send% %actor% &&rYou manage to stop yourself just as you reach the spikes! You only lose a droplet of blood.&&0
      %echoaround% %actor% ~%actor% manages to stop *%person%self just above the spikes!
    else
      %send% %actor% &&rYou feel a ripping pain as the spikes cut through you... that really hurt!&&0
      %echoaround% %actor% ~%actor% SCREAMS in pain as &%actor% falls onto the spikes!
      %subecho% %actor.room% A blood-curdling scream echoes through the labyrinth.
      dg_affect #13523 %actor% IMMOBILIZED on 20
      if !%actor.aff_flagged(IMMUNE-PHYSICAL-DEBUFFS)%
        %dot% %actor% 500 60 physical
      end
      %damage% %actor% 200 physical
    end
    * check blood seal
    makeuid bossroom room i13590
    set jar %bossroom.contents(13515)%
    if %jar% && !%jar.var(blood_done)%
      wait 1
      if !%jar.var(blood_done)%
        %subecho% %self.room% The haunting plucks of lute strings echo through the labyrinth.
        set blood_done 1
        remote blood_done %jar.id%
      end
    end
  end
  %purge% %self%
elseif %self.vnum% == 13521
  * fiend seal broken
  wait 1
  set check_inst %instance.start%
  if %check_inst%
    if %check_inst.template% == 13500
      * in correct zone
      %subecho% %self.room% The haunting notes of a panpipe echo through the labyrinth.
    end
  end
  %purge% %self%
elseif %self.vnum% == 13519
  * tripped in the dark
  wait 1
  set actor %self.carried_by%
  if !%actor%
    %purge% %self%
    halt
  end
  %echoaround% %actor% ~%actor% trips in the dark and screams as &%actor% falls down a hidden chute!
  %send% %actor% You trip in the dark and scream as you fall down a chute!
  wait 1
  %teleport% %actor% i13523
  wait 2
  if !%actor.dead%
    eval labyrinth_dark_falls %actor.var(labyrinth_dark_falls,0)% + 1
    if %labyrinth_dark_falls% < 3
      remote labyrinth_dark_falls %actor.id%
    else
      * dedz
      %send% %actor% You die from your wounds.
      %echoaround% %actor% ~%actor% dies from ^%actor% wounds.
      %slay% %actor% %actor.real_name% has died in the dark at %actor.room.coords%!
    end
  end
  %purge% %self%
end
~
#13520
Labyrinth: Wily madman combat script~
0 k 34 1
L w 13520
~
if %hit% && !%self.aff_flagged(DISARMED)% && !%actor.aff_flagged(IMMUNE-PHYSICAL-DEBUFFS)%
  %dot% #13520 %actor% 20 fire 15
end
~
#13521
Labyrinth: Drink the murky basin water~
1 c 6 4
L c 13514
L c 13515
L f 13521
L j 13590
drink sip fill pour~
return 0
set ok 0
if (%cmd% == sip || %cmd% == drink || %cmd% == pour) && %actor.obj_target(%arg.argument1%)% == %self%
  set val1 %self.val1%
  wait 1 sec
  if %self.val1% < %val1% || %cmd% == sip || %actor.nothirst%
    set ok 1
  end
elseif %cmd% == fill && %actor.obj_target(%arg.argument2%)% == %self%
  set val1 %self.val1%
  wait 1 sec
  if %self.val1% < %val1%
    set ok 1
  end
end
if %ok%
  %subecho% %self.room% The haunting beat of an unseen drum echoes through the labyrinth.
  makeuid bossroom room i13590
  set jar %bossroom.contents(13515)%
  if %jar%
    set water_done 1
    remote water_done %jar.id%
  end
  detach 13521 %self.id%
end
~
#13522
Labyrinth: Spike Pit Trap~
2 g 100 2
L c 13523
L j 13524
~
set ignore_methods enter exit login respawn summon system transport goto transfer
if %was_in% && %was_in.template% == 13524
  * climb down ladder: ok
  halt
elseif %ignore_methods% ~= %method%
  * method ok
  halt
else
  %load% obj 13523 %actor%
end
~
#13523
Labyrinth: Miniboss gets mad when attacked~
0 k 100 1
L f 13523
~
nop %self.add_mob_flag(AGGR)%
nop %self.add_mob_flag(SENTINEL)%
detach 13523 %self.id%
~
#13524
Labyrinth: Ephemeral teleport blocking for rooms~
2 n 100 2
L f 13524
L w 13524
~
* Uses a room effect to block teleport so it can be canceled by other scripts
dg_affect_room #13524 %room% !TELEPORT on -1
detach 13524 %room.id%
~
#13526
Labyrinth: Dispel/cleanse the giant goblin rat~
0 c 0 5
L b 13526
L c 13512
L o 22
L o 116
L o 180
dispel cleanse disenchant~
set ok 0
if dispel /= %cmd% && %actor.ability(22)%
  set ok 1
  %send% %actor% You shout 'KA!' and dispel |%self% afflictions.
  %echoaround% %actor% ~%actor% shouts 'KA!' and dispels |%self% afflictions.
elseif cleanse /= %cmd% && %actor.ability(116)%
  set ok 1
  %send% %actor% You shoot a bolt of white mana at ~%self%, cleansing *%self%.
  %echoaround% %actor% ~%actor% shoots a bolt of white mana at ~%self%, cleansing *%self%.
elseif disenchant /= %cmd% && %actor.ability(180)%
  set ok 1
  %send% %actor% You shout 'KA!' and disenchant |%self% afflictions.
  %echoaround% %actor% ~%actor% shouts 'KA!' and disenchant |%self% afflictions.
else
  return 0
  halt
end
if %ok%
  %echo% ~%self% rises into the air as the magic leaves her body. By the time she lands, she's a much smaller -- but equally fuzzy -- goblin!
  %load% obj 13512 %self.room%
  %load% mob 13526
  set mob %self.room.people(13526)%
  nop %mob.link_instance%
  if %self.mob_flagged(HARD)%
    nop %mob.add_mob_flag(HARD)%
  end
  if %self.mob_flagged(GROUP)%
    nop %mob.add_mob_flag(GROUP)%
  end
  %purge% %self%
end
~
#13527
Labyrinth: Fuzzy goblin load/intro~
0 n 100 6
L b 13510
L b 13527
L c 13515
L j 13590
L j 13592
L t 13526
~
* update fiend seal
makeuid bossroom room i13590
set jar %bossroom.contents(13515)%
if %jar%
  set fiend_done 1
  remote fiend_done %jar.id%
end
* my into sequence
set cycle 0
while %cycle% < 5
  wait 3 sec
  if %self.fighting% || %self.disabled%
    * end early
    halt
  end
  switch %cycle%
    case 0
      %echo% ~%self% lets out a timid squeak! And then covers her mouth.
      wait 1 sec
      %subecho% %self.room% The haunting notes of a panpipe echo through the labyrinth.
    break
    case 1
      say You... you freed me!
    break
    case 2
      say Thought I was seein' jade for sure.
    break
    case 3
      say I'll find you later...
    break
    case 4
      %echo% ~%self% peers around and lowers her voice.
    break
    case 5
      say If you can get past the minotaur.
    break
  done
  eval cycle %cycle% + 1
done
* start quest
set ch %self.room.people%
while %ch%
  if %ch.is_pc% && !%ch.on_quest(13526)%
    %quest% %ch% start 13526
  end
  set ch %ch.next_in_room%
done
* check for completion
if !%instance.mob(13510)%
  wait 5 sec
  say Oh? Don't hear him! I'll go check.
  wait 1 sec
  %echo% ~%self% leaves.
  %at% i13592 %load% mob 13527
  %at% i13592 %echo% A fuzzy goblin comes scrambling up the ladder.
  set other %instance.mob(13527)%
  if %self.mob_flagged(HARD)%
    nop %other.add_mob_flag(HARD)%
  end
  if %self.mob_flagged(GROUP)%
    nop %other.add_mob_flag(GROUP)%
  end
  %purge% %self%
end
~
#13528
Labyrinth: Goblin king quest completion~
0 v 0 2
L c 13592
L t 13526
~
if %questvnum% != 13526
  halt
end
* no more loot from killing me
nop %self.add_mob_flag(!LOOT)%
* load and give item
set level %actor.level%
if %level% > 350
  set level 350
end
%load% obj 13592 %self% inv %level%
* obj must go to self's inv first
set obj %self.inventory%
%send% %actor% &&Y&&Z~%self% gives you @%obj%.&&0
%teleport% %obj% %actor%
~
#13529
Labyrinth: Lost in the dark check~
2 g 100 1
L c 13519
~
if %actor.is_npc%
  halt
elseif %actor.can_see_in_room%
  rdelete labyrinth_dark_count %actor.id%
  rdelete labyrinth_dark_falls %actor.id%
else
  * whoops, moving while blind
  eval labyrinth_dark_count %actor.var(labyrinth_dark_count,0)% + 1
  if %labyrinth_dark_count% < 4
    remote labyrinth_dark_count %actor.id%
  else
    * whoops
    rdelete labyrinth_dark_count %actor.id%
    %load% obj 13519 %actor% inv
  end
end
~
#13531
Labyrinth: Meek adventurer setup~
0 n 100 1
L f 13531
~
wait 1
set name %self.pc_name%
set newname %name.car% the Meek
%mod% %self% shortdesc %newname%
detach 13531 %self.id%
~
#13533
Labyrinth: Scenery setup~
1 n 100 29
L b 13502
L b 13510
L c 851
L c 940
L c 2038
L c 13517
L c 13526
L c 13527
L c 13533
L c 13534
L c 13549
L c 13591
L f 13534
L j 13523
L j 13524
L j 13529
L j 13534
L j 13539
L j 13544
L j 13547
L j 13548
L j 13554
L j 13559
L j 13563
L j 13570
L j 13575
L j 13580
L r 13533
L w 13524
~
* configs
set room_list 13524 13529 13534 13539 13544 13547 13548 13554 13559 13563 13570 13575 13580
set room_size 13
* find minotaur for saving data
wait 1
set minotaur %instance.mob(13510)%
if !%minotaur%
  halt
end
* grab existing data
set done_rooms %minotaur.var(done_rooms)%
set done_mandatory %minotaur.var(done_mandatory)%
set done_random %minotaur.var(done_random)%
* pick at random
eval pos %%random.%room_size%%%
while %pos% > 1
  set room_list %room_list.cdr% %room_list.car%
  eval pos %pos% - 1
done
set use_room 0
while !%use_room% || %done_rooms% ~= %use_room%
  set use_room %room_list.car%
  set room_list %room_list.cdr%
done
* track
set done_rooms %done_rooms% %use_room%
remote done_rooms %minotaur.id%
* move
%teleport% %self% i%use_room%
* flavor items
if %done_mandatory% < 3
  eval done_mandatory %done_mandatory% + 1
  remote done_mandatory %minotaur.id%
  switch %done_mandatory%
    case 1
      %mod% %self% keywords bowl basalt bubbling bubbles
      %mod% %self% shortdesc the bubbling basalt bowl
      %mod% %self% longdesc Bubbles float delicately above a basalt bowl on the alcove.
      %mod% %self% lookdesc The bowl might be a mortar but it's missing its pestle. A bit of sky-blue paste remains in the bowl, perhaps enough for a taste.
      %mod% %self% append-lookdesc Above the bowl, a few bubbles hang in the air, sometimes moving slightly, but never rising or falling.
      set bubbles 1
      remote bubbles %self.id%
      attach 13534 %self.id%
    break
    case 2
      %mod% %self% keywords circle glyphs symbols arcane chalk
      %mod% %self% shortdesc the circle of arcane glyphs
      %mod% %self% longdesc Arcane glyphs drawn in chalk form a circle on the floor.
      %mod% %self% lookdesc There's no telling how long they've been here, but someone cleared off a section of the floor to draw them. The symbols resemble the ones on a summoning circle.
      dg_affect_room #13524 %self.room% off
      set summoning 1
      remote summoning %self.id%
      attach 13534 %self.id%
    break
    case 3
      %mod% %self% keywords bull golden plinth stone statuette
      %mod% %self% shortdesc the golden bull
      %mod% %self% longdesc A golden bull rests on a stone plinth.
      %mod% %self% lookdesc A worn stone plinth stands in front of the alcove, where it supports a golden statuette of a bull. A thick layer of dust covers both the plinth and the bull.
      set statue 1
      remote statue %self.id%
      attach 13534 %self.id%
    break
  done
else
  * basic rooms: pick random
  set pos 0
  while !%pos% || %done_random% ~= %pos%
    set pos %random.10%
  done
  set done_random %done_random% %pos%
  remote done_random %minotaur.id%
  * and setup
  switch %pos%
    case 1
      * library
      %load% obj 13526 %self.room%
      %purge% %self%
    break
    case 2
      * bone nest: just grisly, the abandoned nest of something that was collecting bones
      %mod% %self% keywords nest bones large small pile
      %mod% %self% shortdesc the nest of bones
      %mod% %self% longdesc Bones large and small are piled into a nest in the corner.
      %mod% %self% lookdesc The nest could comfortably seat a horse. Some of the bones are old and gnawed, well, to the bone. Others are newer. But all of them have been picked clean.
    break
    case 3
      * lost stash: contains a rope, candles, firestarter, fermented fruit
      %load% obj 13534 %self.room%
      set stash %self.room.contents(13534)%
      if %stash%
        %load% obj 851 %stash%
        %load% obj 851 %stash%
        %load% obj 940 %stash%
        %load% obj 2038 %stash%
        %load% obj 13591 %stash%
      end
      %purge% %self%
    break
    case 4
      * memorial/grave: makeshift grave mined into the floor
      %mod% %self% keywords grave pile rocks dirt oblong makeshift
      %mod% %self% shortdesc the makeshift grave
      %mod% %self% longdesc There's an oblong pile of rocks and dirt on the floor.
      %mod% %self% lookdesc It looks like someone is buried here. Buried might be too strong a word, though, as the floor is solid stone. More likely the hapless body is lying on the floor and merely covered in the pile of rocks and sand.
      set grave 1
      remote grave %self.id%
      attach 13534 %self.id%
    break
    case 5
      * chalk drawing of a horse woman
      %mod% %self% keywords drawing chalk woman horse
      %mod% %self% shortdesc the chalk drawing
      %mod% %self% longdesc A chalk drawing of a woman covers the wall.
      %mod% %self% lookdesc A large part of the wall is devoted to this drawing, which is rather finely detailed. Someone was stuck down here for a while. It depicts a woman with the head and face of a horse, dressed in jewels and fine cloth.
    break
    case 6
      * wooden chair facing the corner
      %load% veh 13533
      set chair %self.room.vehicles%
      nop %chair.link_instance%
      %purge% %self%
    break
    case 7
      * tiny edible garden with a magical light
      %load% obj 13527 %self.room%
      %purge% %self%
    break
    case 8
      * lost stash: contains cyclopean palace
      %load% obj 13534 %self.room%
      set stash %self.room.contents(13534)%
      if %stash%
        %load% obj 13549 %stash%
      end
      %purge% %self%
    break
    case 9
      * lost adventurer (for hire)
      %load% mob 13502
      set mob %self.room.people(13502)%
      nop %mob.link_instance%
      %purge% %self%
    break
    case 10
      * unmarked chute (portal to doom)
      makeuid spikes room i13523
      %load% obj 13517 %self.room%
      set chute %self.room.contents(13517)%
      if %chute%
        nop %chute.val0(%spikes.vnum%)%
      end
      %purge% %self%
    break
  done
end
~
#13534
Labyrinth: Scenery item commands~
1 c 4 21
L c 13550
L c 13551
L c 13552
L c 13553
L c 13554
L c 13555
L c 13556
L c 13557
L c 13558
L c 13559
L c 13560
L c 13561
L c 13562
L c 13563
L c 13564
L c 13565
L c 13566
L c 13567
L f 13534
L j 13523
L w 13533
taste drink sip use search get take dig push pull rub dust~
if !%actor.can_see(%self%)%
  return 0
elseif "taste drink sip use" ~= %cmd% && %self.var(bubbles)%
  if %actor.obj_target(%arg.argument1%)% == %self%
    return 1
    if %actor.aff_flagged(FLYING)%
      %send% %actor% You taste the sky-blue paste from the bowl, but nothing happens.
      %echoaround% %actor% ~%actor% sticks a finger into @%self% and then sticks it into ^%actor% mouth.
    else
      %send% %actor% You taste a fingerful of the sky-blue paste from the bowl...
      %echoaround% %actor% ~%actor% sticks a finger into @%self% and then sticks it into ^%actor% mouth...
      dg_affect #13533 %actor% FLYING on 30
    end
  else
    return 0
  end
  * end bubbles
elseif "get take use push pull rub dust" ~= %cmd% && %self.var(statue)%
  if %arg% == all || %actor.obj_target(%arg.argument1%)% == %self%
    %send% %actor% You reach out to grab the golden statuette...
    %echoaround% %actor% ~%actor% reaches out to grab the golden statuette...
    %echo% The stone floor in front of the plinth drops open for a moment, swallowing ~%actor%!
    %teleport% %actor% i13523
    %at% %actor.room% %echoaround% %actor% ~%actor% falls in from a chute high above!
    %echo% The floor snaps shut again.
  else
    return 0
  end
elseif %cmd% == use && %self.var(summoning)%
  %send% %actor% The chalk circle allows you to use the summon or teleport abilities here.
elseif "search dig" ~= %cmd% && %self.var(grave)%
  eval loot 13550 - 1 + %random.18%
  %load% obj %loot% %actor% inv
  set obj %actor.inventory%
  if %obj.vnum% == %loot%
    nop %obj.flag(HARD-DROP)%
    %send% %actor% You loot the makeshift grave and find @%obj%!
    %echoaround% %actor% ~%actor% loots the makeshift grave.
    * scale to level
    set level %actor.level%
    if %level% > 350
      set level 350
    end
    %scale% %obj% %level%
  end
  %mod% %self% append-lookdesc Someone has looted the grave.
  rdelete grave %self.id%
  detach 13534 %self.id%
else
  * nope
  return 0
end
~
#13535
Labyrinth: Secret stash empty~
1 c 4 1
L f 13535
get take~
return 0
wait 1
if !%self.contents%
  * empty
  %mod% %self% longdesc A small stone block has been pushed aside.
  detach 13535 %self.id%
end
~
#13537
Labyrinth: Trash mob exit blocking~
0 q 100 9
L b 13538
L b 13539
L b 13545
L b 13547
L b 13548
L b 13551
L c 9680
L j 13505
L j 13534
~
if %method% != move || %actor.nohassle%
  * ignore
  halt
elseif %self.room.template% < 13505 || %self.room.template% > 13599
  * out of bounds
  halt
end
if %self.vnum% == 13547 || %self.vnum% == 13548
  * spider!
  if !%self.fighting% && !%self.disabled%
    %send% %actor% Looks like you're stuck on part of the web!
    %aggro% %actor%
    return 0
  end
elseif %self.vnum% == 13545
  * death cube
  %send% %actor% You can't escape the gelatinous cube!
  return 0
elseif (%self.vnum% == 13538 || %self.vnum% == 13539) && %actor.can_see_in_room% && %direction% != up && %direction% != down
  * shadows (selectively block certain moves)
  %send% %actor% You walk for a ways but end up at the same spot.
  %load% obj 9680 %actor% inv
  return 0
elseif %actor.aff_flagged(SNEAK)%
  * the rest allow sneak
  halt
elseif %self.vnum% == 13551
  * monstrous centipede
  %send% %actor% The centipede encircles you, blocking your every attempt to leave!
  return 0
else
  * normal blocking by template
  set room %self.room%
  eval to_room %%room.%direction%(room)%%
  * Compare template ids to figure out if they're going forward or back (or it's the door)
  if (!%to_room% || %to_room.template% < %room.template% || %to_room.template% == 13534)
    halt
  end
  %send% %actor% ~%self% blocks your path! This doesn't look good.
  return 0
end
~
#13538
Labyrinth: Shadow of the dead room debuff~
0 bw 75 1
L w 13538
~
set ch %self.room.people%
while %ch%
  if !%ch.affect(13538)% && !%ch.aff_flagged(!ATTACK)% && !%ch.aff_flagged(IMMUNE-PHYSICAL-DEBUFFS)%
    if %ch.is_pc% || %ch.vnum% < 13500 || %ch.vnum% > 13599
      dg_affect #13538 %ch% SLOW on 300
      dg_affect #13537 %ch% RESIST-PHYSICAL -50 300
    end
  end
  set ch %ch.next_in_room%
done
~
#13540
Labyrinth: Trash mob load/setup~
0 n 100 16
L b 13541
L b 13542
L b 13544
L b 13547
L b 13548
L b 13550
L b 13553
L b 13556
L b 13557
L c 13537
L c 13538
L c 13540
L c 13542
L c 13546
L c 13548
L r 13556
~
* save this for later
set existing %self.room.contents%
* accessory items
switch %self.vnum%
  case 13541
  case 13542
    * mole rat tunnel
    if !%self.room.contents(13537)%
      %load% obj 13537
    end
  break
  case 13544
    * slime trail
    if !%self.room.contents(13538)%
      %load% obj 13538
    end
  break
  case 13547
  case 13548
    * webbing
    if !%self.room.contents(13540)%
      %load% obj 13540
    end
  break
  case 13550
    * termite hole
    if !%self.room.contents(13542)%
      %load% obj 13542
    end
  break
  case 13553
    * pixy hive
    if !%self.room.contents(13546)%
      %load% obj 13546
    end
  break
  case 13556
    * refugee
    %mod% %self% lookdesc &Z%self.heshe% wears stained and spotted clothing, frayed at the edges. &Z%self.heshe% is seated near the fire and wrapped in a blanket.
    if !%self.room.contents(13548)%
      %load% obj 13548
    end
    if !%self.room.vehicles%
      %load% veh 13556
      set bedroll %self.room.vehicles%
      nop %bedroll.link_instance%
    end
  break
  case 13557
    * refugee guard
    %mod% %self% lookdesc &Z%self.heshe% wears a sweat-stained white gambeson, stitched in some spots and patched in others. &Z%self.hisher% disheveled hair betrays too many nights sleeping on hard floors.
    %mod% %self% append-lookdesc A small glass lantern dangles from %self.hisher% belt, casting long shadows across the cold stone walls.
  break
done
* reorder items?
if %existing% && %existing% != %self.room.contents%
  %teleport% %existing% %self.room%
end
* second desc available for the other mob if I'm not the only one here
set change 0
set ch %self.room.people%
while %ch% && !%change%
  if %ch% != %self% && %ch.vnum% == %self.vnum%
    set change %ch%
  else
    * continue
    set ch %ch.next_in_room%
  end
done
if %change%
  if %change.custom(script1)%
    %mod% %change% longdesc %change.custom(script1)%
  end
end
~
#13542
Labyrinth: Bone slime combat~
0 k 34 1
L w 13544
~
if %hit% && !%self.aff_flagged(DISARMED)% && !%actor.aff_flagged(IMMUNE-PHYSICAL-DEBUFFS)%
  %dot% #13544 %actor% 20 30 physical 15
end
~
#13543
Labyrinth: Mob greetings~
0 h 100 14
L b 13535
L b 13536
L b 13538
L b 13539
L b 13541
L b 13542
L b 13544
L b 13545
L b 13547
L b 13548
L b 13550
L b 13551
L b 13553
L b 13554
~
wait 2 sec
set msg 1
if %self.fighting%
  set msg 0
end
* check for duplicate
set ch %self.room.people%
while %ch% && %msg%
  if %ch% == %self%
    * ok, exit loop
    set ch 0
  elseif %ch.vnum% == %self.vnum%
    * found match
    set msg 0
  else
    * keep on loopin
    set ch %ch.next_in_room%
  end
done
*
switch %self.vnum%
  case 13535
    * a pale green skeleton
    if !%self.var(standing)%
      if %msg%
        %echo% ~%self% stands up and stares at you.
        %mod% %self% longdesc A pale green skeleton watches you intently through hollow eyes.
      end
      set standing 1
      remote standing %self.id%
    end
  break
  case 13536
    * the polished bronze skeleton
    if !%self.var(standing)%
      if %msg%
        %echo% ~%self% stands and readies ^%self% spear.
        %mod% %self% longdesc A polished bronze skeleton stands in your path.
      end
      set standing 1
      remote standing %self.id%
    end
  break
  case 13538
  case 13539
    * the shadow of the dead, the wailing shadow
    if %msg%
      %echo% A shadow passes across the stones.
    end
  break
  case 13541
    * a mammoth mole rat
    * no current action
  break
  case 13542
    * the mole rat queen
    if %msg%
      %echo% ~%self% rears back and unleashes a high-pitched roar!
    end
  break
  case 13544
    * a bone slime
    * no current action
  break
  case 13545
    * the death cube
    if %msg%
      %echo% You can't breathe in here!
    end
    * reset timer
    rdelete timer_%actor.id% %self.id%
  break
  case 13547
    * a silver mirror spider
    if %msg%
      %echo% You think you see something move out of the corner of your eye, but then it's gone.
    end
  break
  case 13548
    * the gold spider
    * no current action
  break
  case 13550
    * a giant termite
    if %msg%
      %echo% ~%self% takes little notice of you as it works.
    end
  break
  case 13551
    * the monstrous centipede
    if %msg%
      %echo% ~%self% encircles you, blocking the path!
    end
  break
  case 13553
    * blind cave pixies
    if %msg%
      %echo% The pixies ignore you as they pick at motes of dust.
    end
  break
  case 13554
    * wokestone guardian
    if %msg%
      %echo% The pixy sees you and shrieks out! &&Z~%self% comes over to check you out.
    end
  break
done
~
#13544
Labyrinth: Bone slime multiplier~
0 b 50 2
L b 13544
L c 1000
~
* converts a basic corpse to a slime
set room %self.room%
set corpse %room.contents(1000)%
if %corpse%
  %echo% ~%self% sucks @%corpse% in!
  %purge% %corpse%
  wait 6 sec
  %echo% ~%self% splits in two!
  %load% mob 13544
end
~
#13545
Labyrinth: Death Cube~
0 bw 100 0
~
* players present?
set ch %self.room.people%
while %ch%
  set next_ch %ch.next_in_room%
  if %ch% != %self% && !%ch.dead% && !%ch.aff_flagged(!ATTACK)%
    * increment timer
    eval timer %self.var(timer_%ch.id%,0)% + 1
    set timer_%ch.id% %timer%
    remote timer_%ch.id% %self.id%
    * uh oh
    if %timer% >= 5
      * dedz
      rdelete timer_%ch.id% %self.id%
      %send% %ch% &&rThe world goes black as even the pain of your skin dissolving cannot keep you conscious... you're dead!&&0
      %echoaround% %ch% ~%ch% succumbs to the burning acid and lack of air... &%ch% is dead!
      %slay% %ch% %ch.real_name% has dissolved in a gelatinous cube at %self.room.coords%!
    elseif %timer% >= 3
      %send% %ch% &&rYou can't breathe!&&0
    elseif
      %send% %ch% You're stuck in the gelatinous cube and cannot breathe!
    end
  end
  set ch %next_ch%
done
~
#13546
Labyrinth: Reject pickpocket~
0 p 100 4
L b 13510
L b 13544
L b 13545
L o 142
~
if %ability% != 142
  return 1
  halt
end
switch %self.vnum%
  case 13510
    * Cyclopean Minotaur
    %send% %actor% You can't reach |%self% pocket... but you've reached ^%self% notice!
    %aggro% %actor%
    return 0
  break
  case 13544
    * bone slime
    %send% %actor% You're not exactly sure which part of it is the pocket.
    return 0
  break
  case 13545
    %send% %actor% You've got bigger things to worry about... YOU'RE in its pocket!
    return 0
  break
  default
    return 1
  break
done
~
#13547
Labyrinth: Hidden spider~
0 b 100 0
~
if !%self.fighting% && !%self.disabled% && %room.players_present% == 0
  hide
end
~
#13548
Labyrinth: Mirror spider venom~
0 k 75 1
L w 13548
~
if %hit% && !%actor.has_tech(!Poison)% && !%actor.aff_flagged(IMMUNE-POISON-DEBUFFS)%
  %dot% #13548 %actor% 35 30 poison 30
end
~
#13551
Labyrinth: Monstrous centipede venom~
0 k 100 1
L w 13551
~
if %hit% && !%actor.has_tech(!Poison)% && !%actor.aff_flagged(IMMUNE-POISON-DEBUFFS)%
  dg_affect #13551 %actor% SLOW on 30
  %dot% #13551 %actor% 10 30 poison 60
end
~
#13553
Labyrinth: Search for clues~
2 c 0 36
L c 13503
L c 13507
L c 13511
L c 13513
L c 13518
L c 13534
L j 13500
L j 13505
L j 13514
L j 13523
L j 13524
L j 13525
L j 13529
L j 13530
L j 13534
L j 13535
L j 13539
L j 13540
L j 13546
L j 13547
L j 13548
L j 13549
L j 13550
L j 13553
L j 13554
L j 13558
L j 13559
L j 13563
L j 13564
L j 13565
L j 13570
L j 13575
L j 13580
L o 18
L o 73
L o 300
search~
eval has_abil %actor.ability(18)% || %actor.ability(300)%
* always do regular search too
return 0
*
switch %room.template%
  case 13500
    if %has_abil%
      if %room.contents(13511)%
        %send% %actor% The brickwork here is quite old, but there's no way forward other than to open the sealed passage.
      elseif %room.contents(13500)%
        %send% %actor% Aside from the seal over the passage, you find a bit of frayed rope on the horns of a statue.
      elseif %room.contents(13507)%
        %send% %actor% It looks like everything interesting is down below.
      else
        %send% %actor% looks like everything interesting is down this dangerous old chute.
      end
    end
  break
  case 13505
    if %has_abil%
      if %actor.ability(73)%
        %send% %actor% You can't tell which is the correct path by looking; perhaps some tracking is in order.
      else
        %send% %actor% Three paths diverge here but you'll have to explore to find the right one.
      end
    end
  break
  case 13514
    if %has_abil%
      %send% %actor% You search around for the source of the water, but can't find it.
    end
  break
  case 13523
    if %has_abil%
      %send% %actor% You thoroughly search the pit but find only badly decayed remains. The ladder is the only way out.
    end
  break
  case 13524
    if %has_abil%
      %send% %actor% You search the dead end and find nothing of interest except that the ladder in the floor leads down to a spike pit.
    end
  break
  case 13525
  case 13539
  case 13540
    * fungus
    if %has_abil%
      %send% %actor% You search the stones, taking care around the fungus, but find nothing.
    end
  break
  case 13529
    if %has_abil%
      %send% %actor% You search along the walls and around the broken statue, but find nothing of any use.
    end
  break
  case 13530
  case 13534
    if %room.contents(13513)%
      %send% %actor% There's a door here, but it's locked.
    else
      %send% %actor% You search around the doorway and the walls but find nothing of interest.
    end
  break
  case 13535
  case 13549
  case 13550
  case 13564
  case 13565
    * shelves
    if %has_abil%
      %send% %actor% You search each of the shelves and every stone, but come up empty.
    end
  break
  case 13546
    if %has_abil%
      %send% %actor% You search around the stone table but it hasn't seen use in a long time and you find nothing of interest.
    end
  break
  case 13547
    if %has_abil%
      %send% %actor% You sift through the sand and potsherds, but find nothing of value.
    end
  break
  case 13548
    * Passage C-A
    set list east west north south northeast northwest southeast southwest
    set found 0
    set avail 0
    while %list% && !%found%
      set try %list.car%
      set list %list.cdr%
      eval exists %%room.%try%(room)%%
      if !%exists%
        if !%avail%
          set avail %try%
        end
      elseif %exists.template% == 13546
        set found %try%
      end
    done
    if %found%
      %send% %actor% There's a narrow passage in the %actor.dir(%try%)% wall, easy to miss.
    elseif !%has_abil%
      %send% %actor% You search the alcove and shelves but find nothing of use.
    else
      * add
      %door% %room% %avail% room i13546
      %door% i13546 %_map.reverse(%avail%)% room i13548
      %send% %actor% As you search the dead end, you find a narrow passage in the %actor.dir(%avail%)% wall that's almost impossible to see.
      %echoaround% %actor% ~%actor% searches around the dead end... and finds a passageway!
      * and back
      %door% i13554 up room i13546
    end
  break
  case 13553
    * Passage A-B
    if %room.contents(13518)%
      %send% %actor% The trail leads to a tiny passage through the bricks!
    elseif !%has_abil%
      %send% %actor% You search around and find little but sand.
    else
      %send% %actor% You search around the corner where the walls meet... and find loose bricks!
      %echoaround% %actor% ~%searches around in the corner and begins removing bricks...
      %echo% There's a small passage through the wall!
      %load% obj 13518 %room%
      set passage %room.contents(13518)%
      makeuid to_room room i13558
      nop %passage.val0(%to_room.vnum%)%
    end
  break
  case 13558
    if %has_abil%
      %send% %actor% This section has been walled off on both sides, but you can find nothing of interest. It just seems like a shortcut.
    end
  break
  case 13559
    if %has_abil%
      %send% %actor% You search through the rubble from the broken statues but find nothing of interest.
    end
  break
  case 13563
    if %has_abil%
      %send% %actor% You search around the area for a bit, digging through the pile of broken jars, but come up with nothing of any use.
    end
  break
  case 13570
  case 13575
  case 13580
    * end path
    if %room.contents(13503)%
      %send% %actor% This is it! A passageway through the bricks leads right to the center of the maze!
    elseif %has_abil%
      %send% %actor% Gaps in the bricks allow a little stale air through, but the wall is solid and you find no way through it.
    end
  break
  case 13590
    if %has_abil%
      if %room.people(13510)%
        %send% %actor% The minotaur is blocking the way to the ladder... if only you could fly over his head.
      elseif %room.people(13512)%
        %send% %actor% You can't get past the Nightmare Queen!
      else
        %send% %actor% The chamber has been sealed off for a long time, but it looks like the ladder leads to an exit.
      end
      *
      set jar %room.contents(13515)%
      if !%jar.var(water_done)% && !%jar.var(blood_done)% && !%jar.var(fiend_done)%
        %send% %actor% You try to inspect the great jar but it is sealed.
      elseif !%jar.var(water_done)% && !%jar.var(blood_done)%
        %send% %actor% You try to inspect the great jar... but the water seal and blood seal are still intact.
      elseif !%jar.var(blood_done)% && !%jar.var(fiend_done)%
        %send% %actor% You try to inspect the great jar... but the blood seal and fiend seal are still intact.
      elseif !%jar.var(water_done)% && !%jar.var(fiend_done)%
        %send% %actor% You try to inspect the great jar... but the water seal and fiend seal are still intact.
      elseif !%jar.var(water_done)%
        %send% %actor% You try to inspect the great jar... but the water seal is still intact.
      elseif !%jar.var(blood_done)%
        %send% %actor% You try to inspect the great jar... but the blood seal is still intact.
      elseif !%jar.var(fiend_done)%
        %send% %actor% You try to inspect the great jar... but the fiend seal is still intact.
      elseif !%jar.var(open)%
        %send% %actor% You notice the seals on the great jar have broken!
      end
    end
  break
  default
    if %has_abil%
      %send% %actor% You search the stones but find no secrets.
    end
  break
done
* other things to search for
set scenery %room.contents(13533)%
if %scenery%
  if %scenery.var(statue)%
    %send% %actor% ... but that golden bull seems suspicious. On closer inspection, it seems the stones of the floor in front of the plinth are loose.
  end
end
*
set stash %room.contents(13534)%
if %stash% && %stash.contents%
  %send% %actor% A loose stone has been pushed aside; something is hidden in the secret stash!
end
~
#13558
Labyrinth: Open A-wing secret passage, inside~
2 g 100 4
L c 13518
L f 13558
L j 13553
L j 13559
~
* Ensure both sides of the passage are open
makeuid out room i13553
if !%out.contents(13518)%
  %at% %out% %load% obj 13518
  set passage %out.contents(13518)%
  if %passage%
    nop %passage.val0(%room.vnum%)%
  end
end
*
makeuid down room i13559
if %down% && !%down.up(room)%
  %door% %down% up room %room.vnum%
  %at% %down% %echo% A rope ladder drops from the chute in the ceiling.
  %mod% %down% append-desc A rope ladder leads up into the chute above.
end
*
detach 13558 %room.id%
~
#13573
Labyrinth: loot/craft bop/boe twiddler and restringer~
1 n 100 6
L c 13555
L c 13561
L c 13567
L c 13573
L f 13573
L f 13575
~
* items default to BOP but are set BOE if they come from a craft
* if they remain BOP, this will also randomly restring them
* Configs:
set adjective_list acid-pitted banged-up battered bent corroded cracked crushed decayed decrepit dented dilapidated dinged-up gnawed gored mangled mildewed musty ruined scraped-up scratched shabby soiled tarnished tattered timeworn warped wrecked
set adjective_count 27
set add_empire_name_to 13555 13561 13567 13573
set fake_empire_name Nightmare Eternal Fell Lost Dread Unquiet Haunted Ghastly Sunken
set fake_empire_name_count 9
set fake_empire_type Imperium Kingdom Reign Realm Host Lands Rule
set fake_empire_type_count 7
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
end
if !%actor%
  halt
end
set rescale 0
if %actor.is_pc%
  * Item was crafted or bought
  if %self.is_flagged(BOP)%
    nop %self.flag(BOP)%
    set rescale 1
  end
  if !%self.is_flagged(BOE)%
    nop %self.flag(BOE)%
    set rescale 1
  end
  * Probably need to unbind when BOE
  nop %self.bind(nobody)%
  * restring some BOEs with empire name
  if %actor.empire% && %add_empire_name_to% ~= %self.vnum%
    %mod% %self% keywords %self.keywords% %actor.empire.name%
    %mod% %self% shortdesc %self.shortdesc% of %actor.empire.name%
    %mod% %self% append-lookdesc It bears the sigil of %actor.empire.name%.
  end
else
  * Item was probably dropped as loot
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
    %mod% %self% append-lookdesc It bears the sigil of the %fake_name% %fake_type%.
  end
  * restringing: add to the look desc
  %mod% %self% append-lookdesc It looks like the last owner's fateful underground encounter has left it a bit %adjective%.
  if %item.is_flagged(HARD-DROP)% || %item.is_flagged(GROUP-DROP)%
    set keywords %self.keywords%
    %mod% %self% append-lookdesc-noformat Type 'study %keywords.car%' to take it apart and learn to craft it.
    %mod% %self% append-lookdesc-noformat (Be sure to 'keep' any copies of it you don't want to lose.)
  end
  * add study script
  attach 13575 %self.id%
end
if %rescale%
  wait 0
  %scale% %self% %self.level%
end
detach 13573 %self.id%
~
#13574
Labyrinth: Block learn command on patterns from other empires~
1 c 2 0
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
#13575
Labyrinth: Study loot to create pattern~
1 c 2 1
L c 13575
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
elseif !%self.is_flagged(HARD-DROP)% && !%self.is_flagged(GROUP-DROP)%
  * Normal
  %send% %actor% @%self% is too damaged to learn anything of use.
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
%load% obj 13575 %actor% inv
set pattern %actor.inventory%
if %pattern.vnum% != 13575
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
#13578
Labyrinth: Loot quality fixer for minor items~
1 n 100 1
L f 13578
~
set actor %self.carried_by%
if !%actor%
  set actor %self.worn_by%
end
*
if %actor%
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
  if %rescale%
    wait 0
    %scale% %self% %self.level%
  end
end
detach 13578 %self.id%
~
#13579
Labyrinth: Free to a Good Home turn-in qualifier~
0 v 0 3
L j 13501
L j 13592
L t 13503
~
if %questvnum% == 13503 && %self.room.template% >= 13501 && %self.room.template% <= 13592
  %send% %actor% You need to get ~%self% out of the Labyrinth before you can finish %questname%.
  return 0
else
  return 1
end
~
#13591
Labyrinth: Consume fermented pomegranates~
1 s 100 0
~
if %quantity% < 1
  set quantity 1
end
eval quantity %quantity% * 2
nop %actor.drunk(%quantity%)%
~
#13592
Labyrinth: Boss loot controllers~
1 n 100 38
L c 13550
L c 13551
L c 13552
L c 13553
L c 13554
L c 13555
L c 13556
L c 13557
L c 13558
L c 13559
L c 13560
L c 13561
L c 13562
L c 13563
L c 13564
L c 13565
L c 13566
L c 13567
L c 13568
L c 13569
L c 13570
L c 13571
L c 13572
L c 13573
L c 13574
L c 13576
L c 13577
L c 13578
L c 13579
L c 13581
L c 13582
L c 13583
L c 13584
L c 13585
L c 13586
L c 13587
L c 13588
L c 13592
~
* determine where we are
set actor %self.carried_by%
if %self.level%
  set level %self.level%
else
  set level 100
end
* determine 1 item of loot
if %self.vnum% == 13592
  * BoE item list
  set roll %random.100%
  if %roll% <= 5
    set loot_list 13577
  elseif %roll% <= 15
    set loot_list 13576
  elseif %roll% <= 25
    set loot_list 13584 13585 13586 13587
  else
    set loot_list 13578 13579 13581 13582 13583 13588
  end
  * count list
  set temp %loot_list%
  set count 0
  while !%temp.empty%
    set temp %temp.cdr%
    eval count %count% + 1
  done
  * roll
  eval pos %%random.%count%%%
  while %pos% > 0 && !%loot_list.empty%
    set loot %loot_list.car%
    set loot_list %loot_list.cdr%
    eval pos %pos% - 1
  done
  *
elseif %self.vnum% == 13574
  * BoP item list 13550-13573
  eval loot 13550 - 1 + %random.24%
  *
end
* load loot
if %loot%
  %load% obj %loot% %actor% inv %level%
  set item %actor.inventory%
  if %item.vnum% == %loot%
    if %item.is_flagged(BOE)% || %item.is_flagged(BOP)%
      %item.bind(%self%)%
    end
  end
end
* any bonus items?
*   no
* done
wait 0
%purge% %self%
~
#13593
Labyrinth: Exit through the rafters~
2 n 100 0
~
set target %instance.location%
if %target% && !%room.down(room)%
  %door% %room% down room %target%
end
~
#13598
Labyrinth: Adventurer leaves after quest completion~
0 v 0 1
L t 13502
~
* one only -- purge me after completion
if %questvnum% == 13502
  %echoaround% %actor% ~%self% joins ~%actor%.
  wait 0
  %purge% %self%
end
~
#13599
Labyrinth: Admin controller~
1 c 2 52
L b 13510
L b 13512
L b 13520
L b 13525
L b 13526
L b 13527
L b 13530
L c 13500
L c 13503
L c 13507
L c 13511
L c 13515
L c 13518
L j 13500
L j 13501
L j 13505
L j 13510
L j 13514
L j 13515
L j 13520
L j 13523
L j 13524
L j 13525
L j 13529
L j 13530
L j 13534
L j 13535
L j 13539
L j 13540
L j 13544
L j 13545
L j 13546
L j 13547
L j 13548
L j 13549
L j 13550
L j 13553
L j 13554
L j 13555
L j 13558
L j 13559
L j 13560
L j 13563
L j 13564
L j 13565
L j 13570
L j 13575
L j 13580
L j 13590
L j 13591
L j 13592
L j 13593
labyrinth~
* modes: labyrinth goto <template>; labyrinth status; labyrinth unseal
if goto /= %arg.car%
  set vnum %arg.cdr%
  if %vnum% < 13500 || %vnum% > 13599
    %send% %actor% You can only use this controller to teleport to vnums 13500-13599.
    halt
  end
  set to_room %instance.nearest_rmt(%vnum%)%
  if !%to_room%
    %send% %actor% No labyrinth room %vnum% found.
    halt
  end
  %echoaround% %actor% ~%actor% vanishes.
  %teleport% %actor% %to_room%
  %echoaround% %actor% ~%actor% appears.
  %force% %actor% look
elseif status /= %arg.car%
  set entrance_area 13500 13501 13505
  set wing_a_main 13510 13525 13540 13555 13570
  set wing_b_main 13515 13530 13545 13560 13575
  set wing_c_main 13520 13535 13550 13565 13580
  set wing_a_side 13523 13524 13539 13553 13554 13558
  set wing_b_side 13514 13529 13544 13559
  set wing_c_side 13534 13546 13547 13548 13549 13563 13564
  set boss_area 13590
  set exit_area 13591 13592 13593
  *
  set room %actor.room%
  set find_dir 1
  set diff 0
  * Location
  if %entrance_area% ~= %room.template%
    %send% %actor% Labyrinth region: entrance
    set find_dir 0
  elseif %wing_a_main% ~= %room.template%
    %send% %actor% Labyrinth region: wing A, main path
  elseif %wing_b_main% ~= %room.template%
    %send% %actor% Labyrinth region: wing B, main path
  elseif %wing_c_main% ~= %room.template%
    %send% %actor% Labyrinth region: wing C, main path
  elseif %wing_a_side% ~= %room.template%
    %send% %actor% Labyrinth region: wing A, side path
  elseif %wing_b_side% ~= %room.template%
    %send% %actor% Labyrinth region: wing B, side path
  elseif %wing_c_side% ~= %room.template%
    %send% %actor% Labyrinth region: wing C, side path
  elseif %boss_area% ~= %room.template%
    %send% %actor% Labyrinth region: boss arena
    set find_dir 0
  elseif %exit_area% ~= %room.template%
    %send% %actor% Labyrinth region: exit area
    set find_dir 0
  elseif %room.template% >= 13500 && %room.template% <= 13599
    %send% %actor% Labyrinth region: unknown
  else
    %send% %actor% You are not in the Long-Lost Labyrinth.
    halt
  end
  * Entry/difficulty?
  set entry %instance.nearest_rmt(13500)%
  set diff_set 0
  if %entry%
    if %entry.contents(13500)%
      %send% %actor% Difficulty not set.
    else
      set diff_set 1
    end
  end
  * Path
  set a_end %instance.nearest_rmt(13570)%
  set b_end %instance.nearest_rmt(13575)%
  set c_end %instance.nearest_rmt(13580)%
  if %a_end% && %a_end.contents(13503)%
    %send% %actor% Path to the minotaur: wing A
  elseif %b_end% && %b_end.contents(13503)%
    %send% %actor% Path to the minotaur: wing B
  elseif %c_end% && %c_end.contents(13503)%
    %send% %actor% Path to the minotaur: wing C
  end
  makeuid passage room i13553
  if %passage%
    if %passage.contents(13518)%
      %send% %actor% Passage A-B open: yes
    else
      %send% %actor% Passage A-B open: no
    end
  end
  * Miniboss
  set mini_list 13520 13525 13530
  set any 0
  while %mini_list%
    set vnum %mini_list.car%
    set mob %instance.mob(%vnum%)%
    if %mob%
      %send% %actor% Miniboss: %mob.name% [at %mob.room.template% || %mob.room.vnum%]
      set any 1
    end
    set mini_list %mini_list.cdr%
  done
  if !%any%
    * also check fuzzy goblin
    set mob %instance.mob(13526)%
    if %mob%
      %send% %actor% Rescued miniboss: %mob.name% [at %mob.room.template% || %mob.room.vnum%]
    else
      set mob %instance.mob(13527)%
      if %mob%
        %send% %actor% Rescued miniboss: %mob.name% [at %mob.room.template% || %mob.room.vnum%]
      elseif !%diff_set%
        %send% %actor% Miniboss: not spawned
      else
        %send% %actor% Miniboss: defeated
      end
    end
  end
  * Minotaur
  set bossroom %instance.nearest_rmt(13590)%
  if %bossroom%
    if %instance.mob(13510)%
      %send% %actor% Minotaur defeated: no
    else
      %send% %actor% Minotaur defeated: yes
    end
    * Nightmare queen
    set jar %bossroom.contents(13515)%
    if %jar.var(open)%
      %send% %actor% Great jar: seals broken
      if %instance.mob(13512)%
        %send% %actor% Nightmare queen: released
      else
        %send% %actor% Nightmare queen: defeated
      end
    else
      if %jar.var(blood_done)% && %jar.var(water_done)% && %jar.var(fiend_done)%
        %send% %actor% Great jar: seals broken
      elseif %jar.var(blood_done)% && %jar.var(water_done)%
        %send% %actor% Great jar: fiend seal
      elseif %jar.var(blood_done)% && %jar.var(fiend_done)%
        %send% %actor% Great jar: water seal
      elseif %jar.var(fiend_done)% && %jar.var(water_done)%
        %send% %actor% Great jar: blood seal
      elseif %jar.var(blood_done)%
        %send% %actor% Great jar: water seal, fiend seal
      elseif %jar.var(water_done)%
        %send% %actor% Great jar: blood seal, fiend seal
      elseif %jar.var(fiend_done)%
        %send% %actor% Great jar: blood seal, water seal
      else
        %send% %actor% Great jar: blood seal, water seal, fiend seal
      end
    end
    set diff %jar.var(diff)%
  end
  if %entry%
    if %entry.contents(13507)% || %entry.contents(13511)%
      %send% %actor% Rope attached to entry chute: yes
    else
      %send% %actor% Rope attached to entry chute: no
    end
  end
  * difficulty?
  switch %diff%
    case 1
      %send% %actor% Difficulty: normal
    break
    case 2
      %send% %actor% Difficulty: &&Ghard&&0
    break
    case 3
      %send% %actor% Difficulty: &&Cgroup&&0
    break
    case 4
      %send% %actor% Difficulty: &&Mboss&&0
    break
  done
  * progress direction from here
  if %find_dir%
    set dir_list north east south west northeast northwest southeast southwest
    set found 0
    set lowest 0
    set lowest_dir 0
    while %dir_list%
      set dir %dir_list.car%
      set dir_list %dir_list.cdr%
      eval to_room %%room.%dir%(room)%%
      if %to_room%
        if %to_room.template% > %room.template%
          set found %dir%
        elseif !%find_template% && (!%lowest% || %to_room.template% < %lowest%)
          set lowest_dir %dir%
          set lowest %to_room.template%
        end
      end
    done
    if %found% && %lowest_dir%
      %send% %actor% Travel forward: %actor.dir(%found%)%, backward: %actor.dir(%lowest_dir%)%
    elseif %found%
      %send% %actor% Travel forward: %actor.dir(%found%)%
    elseif %lowest_dir%
      %send% %actor% Travel backward: %actor.dir(%lowest_dir%)%
    end
  end
elseif unseal /= %arg.car%
  set bossroom %instance.nearest_rmt(13590)%
  if %bossroom%
    set jar %bossroom.contents(13515)%
    if %jar%
      set water_done 1
      set blood_done 1
      set fiend_done 1
      remote water_done %jar.id%
      remote blood_done %jar.id%
      remote fiend_done %jar.id%
      %send% %actor% You use @%self% to unseal the great jar.
    else
      %send% %actor% Unable to find the great jar to unseal.
    end
  else
    %send% %actor% Unable to find a labyrinth with a great jar to unseal.
  end
else
  * no arg or unknown arg
  %send% %actor% &&0Usage: labyrinth goto <template>
  %send% %actor% &&0       labyrinth status
  %send% %actor% &&0       labyrinth unseal
end
~
$
