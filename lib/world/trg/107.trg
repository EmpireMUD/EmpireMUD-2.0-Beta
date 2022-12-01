#10700
cookie turn in~
2 v 0
~
if %room.max_citizens% < 1
  return 0
  %send% %actor% You need to leave Christmas cookies in a house for the elves to collect.
  %send% %actor% (Go to any place where citizens live and try to finish this quest again.)
  halt
end
~
#10701
Carolers caroling a'merrily~
0 g 100
~
wait 5
switch %random.3%
  case 1
    %echo% The carolers sing, 'Dashing through the snow, on a one horse open sleigh!'
    wait 3 sec
    %echo% The carolers sing, 'O'er the fields we go, laughing all the way!'
    wait 3 sec
    %echo% The carolers sing, 'Bells on bobtail ring, making spirits bright!'
    wait 3 sec
    %echo% The carolers sing, 'What fun it is to ride and sing a sleighing song tonight!'
    wait 3 sec
    %echo% The carolers sing, 'Oh, jingle bells, jingle bells, jingle all the way!'
    wait 3 sec
    %echo% The carolers sing, 'Oh, what fun it is to ride in a one horse open sleigh!'
    wait 3 sec
    %echo% The carolers sing, 'Jingle bells, jingle bells, jingle all the way!
    wait 3 sec
    %echo% The carolers sing, 'Oh, what fun it is to ride in a one horse open sleigh!'
    wait 3 sec
  break
  case 2
    %echo% The carolers sing, 'Here we come a-wassailing among the leaves so green;'
    wait 3 sec
    %echo% The carolers sing, 'Here we come a-wand'ring so fair to be seen.'
    wait 3 sec
    %echo% The carolers sing, 'Love and joy come to you,'
    wait 3 sec
    %echo% The carolers sing, 'And to you your wassail too;'
    wait 3 sec
    %echo% The carolers sing, 'And god bless you and send you a Happy New Year'
    wait 3 sec
    %echo% The carolers sing, 'And god send you a Happy New Year.
  break
  case 3
    %echo% The carolers sing, 'How'd ja like to spend Christmas on Christmas Island?'
    wait 3 sec
    %echo% The carolers sing, 'How'd ja like to spend the Holiday away across the sea?'
    wait 3 sec
    %echo% The carolers sing, 'How'd ja like to spend Christmas on Christmas Island?'
    wait 3 sec
    %echo% The carolers sing, 'How'd ja like to hang your stockin' on a great big coconut tree?'
    wait 3 sec
    %echo% The carolers sing, 'How'd ja like to stay up late, like the islanders do:'
    wait 3 sec
    %echo% The carolers sing, 'Wait for Santa to sail in with your presents in a canoe.'
    wait 3 sec
    %echo% The carolers sing, 'If you ever spend Christmas on Christmas Island'
    wait 3 sec
    %echo% The carolers sing, 'You will never stray, for ev'ry day'
    wait 3 sec
    %echo% The carolers sing, 'Your Christmas dreams come true.'
    wait 3 sec
  break
done
~
#10702
Christmas Gift open~
1 c 2
open~
* gifts are given in order
set gift_list 10717 16666 10728 10719 10720 16657 16696 10704 10710 16656
set first_gift 10717
* targ check
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room_var %actor.room%
%send% %actor% You start unwrapping @%self%...
%echoaround% %actor% ~%actor% starts unwrapping @%self%...
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || %self.carried_by% != %actor% || %actor.disabled%
  halt
end
* find last gift
if !%actor.varexists(last_christmas_gift_item)%
  set last_christmas_gift_item 0
else
  set last_christmas_gift_item %actor.last_christmas_gift_item%
end
* determine next gift
set next_gift 0
while %gift_list%
  set entry %gift_list.car%
  if %entry% == %last_christmas_gift_item%
    set next_gift %gift_list.cdr%
    set next_gift %next_gift.car%
    set gift_list 0
  else
    set gift_list %gift_list.cdr%
  end
done
if !%next_gift%
  set next_gift %first_gift%
end
* ok go for it
%load% obj %next_gift% %actor% inv
set item %actor.inventory(%next_gift%)%
%send% %actor% You finish unwrapping the gift and find @%item% inside!
%echoaround% %actor% ~%actor% finishes unwrapping the gift and finds @%item%!
set last_christmas_gift_item %next_gift%
remote last_christmas_gift_item %actor.id%
%quest% %actor% trigger 16600
if %self.carried_by.quest_finished(16600)%
  %quest% %actor% finish 16600
end
%purge% %self%
~
#10703
Father Christmas gift give~
0 g 100
~
if !%actor.is_pc%
  halt
end
wait 1 sec
%echo% ~%self% stands up to greet you.
wait 3 sec
say Ho ho ho! Merry Christmas!
wait 1 sec
set person %self.room.people%
* Gifts for everyone! (if they haven't gotten one today)
while %person%
  if %person.is_pc%
    if %person.empire%
      nop %person.empire.start_progress(10700)%
    end
    if !%person.varexists(last_christmas_day)%
      set last_christmas_day 0
    else
      set last_christmas_day %person.last_christmas_day%
    end
    if %last_christmas_day% < %dailycycle%
      %send% %person% ~%self% gives you a gift!
      %echoaround% %person% ~%self% gives ~%person% a gift!
      %load% obj 10702 %person%
      set last_christmas_day %dailycycle%
      remote last_christmas_day %person.id%
    elseif %person% == %actor%
      %send% %person% You already received your gift today.
    end
  end
  set person %person.next_in_room%
done
* Adventure ends now
%adventurecomplete%
~
#10704
Completer~
0 f 100
~
* This formerly completed the adventure at the end of the frost elf fight.
%adventurecomplete%
~
#10706
Christmas present minipet: Open present~
0 ct 0
open~
if %actor.obj_target(%arg.car%)%
  return 0
  halt
end
if %actor.char_target(%arg.car%)% != %self%
  return 0
  halt
end
return 1
if %actor% != %self.leader%
  %send% %actor% You'd like to open the present but it's not for you, and you shouldn't.
  halt
end
* ok
%send% %actor% You open the little present and find... another present inside, exactly the same as the first!
%echoaround% %actor% ~%actor% has a look of childlike joy as &%actor% opens a little present and finds another identical present inside!
nop %actor.command_lag(ABILITY)%
~
#10710
Faster Hestian Trinket (snowglobe)~
1 c 2
use~
* This is basically a [252] HESTIAN TRINKET with custom strings
* checks
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if %actor.position% != Standing || %actor.action% || %actor.fighting%
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
set home %actor.home%
if !%home%
  %send% %actor% You have no home to teleport back to with this trinket.
  halt
end
set veh %home.in_vehicle%
if %veh%
  set outside_room %veh.room%
  if !%actor.canuseroom_guest(%outside_room%)%
    %send% %actor% You can't teleport home to a vehicle that's parked on foreign territory you don't have permission to use!
    halt
  elseif !%actor.can_teleport_room(%outside_room%)%
    %send% %actor% You can't teleport to your home's current location.
    halt
  end
end
* once per 30 minutes
if %actor.cooldown(256)%
  %send% %actor% @%self% is on cooldown.
  halt
end
set room_var %actor.room%
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
%send% %actor% You shake @%self% and it begins to swirl with light...
%echoaround% %actor% ~%actor% shakes @%self% and it begins to swirl with light...
set cycle 0
while %cycle% < 2
  wait 5 sec
  if %actor.stop_command% || %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)% || %actor.action%
    %force% %actor% stop cleardata
    %send% %actor% The swirling light from @%self% fades.
    halt
  end
  switch %cycle%
    case 0
      %send% %actor% @%self% glows a wintry white and the light begins to envelop you!
      %echoaround% %actor% @%self% glows a wintry white and the light begins to envelop ~%actor%!
    break
    case 1
      %echoaround% %actor% ~%actor% vanishes in a flurry of snow!
      %teleport% %actor% %actor.home%
      %force% %actor% look
      %echoaround% %actor% ~%actor% appears in a flurry of snow!
    break
  done
  eval cycle %cycle% + 1
done
* cleanup
nop %actor.set_cooldown(256, 1800)%
nop %actor.cancel_adventure_summon%
%force% %actor% stop cleardata
~
#10712
bottomless gift sack: spawn elf on claimed land~
1 bw 3
~
* Randomly spawns an elf while worn (on claimed land)
set ch %self.worn_by%
if !%ch%
  halt
end
set room %self.room%
if !ch.empire% || %room.empire% != %ch.empire%
  halt
end
* load the elf
%load% mob 10712
set mob %room.people%
if %mob.vnum% == 10712
  %echo% ~%mob% comes tumbling out of @%self%!
end
~
#10713
Reindeer spawner~
0 h 100
~
if !%actor.varexists(last_christmas_reindeer_day)%
  set last_christmas_reindeer_day 0
else
  set last_christmas_reindeer_day %actor.last_christmas_reindeer_day%
end
if %last_christmas_reindeer_day% >= %dailycycle%
  * already spawned this actor 1 today
  halt
end
* Safe to spawn: update their indicator
set last_christmas_reindeer_day %dailycycle%
remote last_christmas_reindeer_day %actor.id%
* load the reindeer
if %random.100% <= 14
  %load% mob 10700
else
  %load% mob 10705
end
wait 1
set mob %self.room.people%
if %mob.vnum% == 10705
  * success: send a message
  %echo% ~%mob% walks up from lower on the mountain.
elseif %mob.vnum% == 10700
  * success: send a message
  %echo% ~%mob% flies up from lower on the mountain.
end
%purge% %self%
~
#10714
Elf despawner~
0 b 10
~
if %self.fighting% || %self.disabled%
  halt
end
%echo% ~%self% runs off!
%purge% %self%
~
#10715
Reindeer mount response~
0 c 0
mount ride~
* This marks last-reindeer-spawn-day when mounted, IF in the adventure
return 0
if %actor.char_target(%arg.car%)% == %self% && %self.room.template% >= 10700 && %self.room.template% <= 10729
  set last_christmas_reindeer_day %dailycycle%
  remote last_christmas_reindeer_day %actor.id%
end
~
#10727
Winter Wonderland minipet whistle (random order) pre-2021~
1 c 2
use~
* NOTE: This is the pre-2021 version and has a shorter minipet list, for people who hoarded old whistles.
* List of vnums granted by this whistle (minipet mobs)
set list 10709 16657 16658 10723 10724 10725 10726 16653 16654 16655 16656
* length is used to shuffle the start point of the list
set length 11
* Check targeting
if %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
* All other results will return 1
return 1
* shuffle the start point
eval start %%random.%length%%% - 1
while %start% > 0
  eval start %start% - 1
  set list %list.cdr% %list.car%
done
* Pick a random pet the owner doesn't know
set found 0
while (%list% && !%found%)
  * get next vnum in list
  set vnum %list.car%
  set list %list.cdr%
  if !%actor.has_minipet(%vnum%)%
    set found %vnum%
  end
done
* Nothing to give
if !%found%
  %send% %actor% You already have all the minipets from the Winter Wonderland!
  %purge% %self%
  halt
end
* and load 1 for verification and messaging
%load% mob %vnum%
set pet %actor.room.people%
if %pet.vnum% == %vnum%
  nop %actor.add_minipet(%vnum%)%
  %send% %actor% You gain '~%pet%' as a minipet. Use the minipets command to summon it.
  %purge% %pet%
else
  %send% %actor% There seems to be a problem giving you minipet #%vnum%. Please report this.
end
%purge% %self%
~
#10728
Magical coal use~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
set room %actor.room%
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
set varname summon_%self.vnum%
* Cooldown
if %actor.cooldown(10728)%
  %send% %actor% @%self% is on cooldown.
  halt
end
set ch %room.people%
while %ch% && !%found%
  if %ch.is_npc%
    eval mobs %mobs% + 1
  end
  set ch %ch.next_in_room%
done
if %mobs% > 4
  %send% %actor% There are too many mobs here already.
  return 1
  halt
end
%send% %actor% You throw a handful of the magical coal lumps into the sky...
%echoaround% %actor% ~%actor% throws a handful of sparkling coal lumps into the sky...
wait 1 sec
if (%actor.room% != %room% || %actor.position% != Standing)
  %send% %actor% You stop building the snowman.
  halt
end
%echo% A sudden flurry of snow comes from the sky, creating a large pile nearby.
wait 1 sec
if (%actor.room% != %room% || %actor.position% != Standing)
  %send% %actor% You stop building the snowman.
  halt
end
%send% %actor% You start rolling the snow into a large ball...
%echoaround% %actor% ~%actor% starts rolling the snow into a large ball...
wait 3 sec
if (%actor.room% != %room% || %actor.position% != Standing)
  %send% %actor% You stop building the snowman.
  halt
end
%send% %actor% You finish the snowman's body and start rummaging through your belongings...
%echoaround% %actor% ~%actor% finishes the snowman's body and starts rummaging through ^%actor% belongings...
wait 1 sec
if (%actor.room% != %room% || %actor.position% != Standing)
  %send% %actor% You stop building the snowman.
  halt
end
%send% %actor% You find a carrot and some sticks, and add them to your snowman...
%echoaround% %actor% ~%actor% finds a carrot and some sticks, and adds them to ^%actor% snowman...
wait 1 sec
if (%actor.room% != %room% || %actor.position% != Standing)
  %send% %actor% You stop building the snowman.
  halt
end
%send% %actor% You finish your snowman and step back to admire your work.
%echoaround% %actor% ~%actor% finishes ^%actor% snowman and steps back with a satisfied nod.
nop %actor.set_cooldown(10728, 180)%
%load% m 10728
set mob %self.room.people%
if (%mob% && %mob.vnum% == %self.val0%)
  nop %mob.unlink_instance%
end
~
#10729
Winter Wonderland minipet whistle (random order) 2021-2022~
1 c 2
use~
* List of vnums granted by this whistle (minipet mobs)
set list 10709 16657 16658 10723 10724 10725 10726 16653 16654 16655 16656 16666 16667 16668 16669 10706
* length is used to shuffle the start point of the list
set length 15
* Check targeting
if %actor.obj_target(%arg.car%)% != %self%
  return 0
  halt
end
* All other results will return 1
return 1
* shuffle the start point
eval start %%random.%length%%% - 1
while %start% > 0
  eval start %start% - 1
  set list %list.cdr% %list.car%
done
* Pick a random pet the owner doesn't know
set found 0
while (%list% && !%found%)
  * get next vnum in list
  set vnum %list.car%
  set list %list.cdr%
  if !%actor.has_minipet(%vnum%)%
    set found %vnum%
  end
done
* Nothing to give
if !%found%
  %send% %actor% You already have all the minipets from the Winter Wonderland!
  %purge% %self%
  halt
end
* and load 1 for verification and messaging
%load% mob %vnum%
set pet %actor.room.people%
if %pet.vnum% == %vnum%
  nop %actor.add_minipet(%vnum%)%
  %send% %actor% You gain '~%pet%' as a minipet. Use the minipets command to summon it.
  %purge% %pet%
else
  %send% %actor% There seems to be a problem giving you minipet #%vnum%. Please report this.
end
%purge% %self%
~
#10730
Hey Diddle Diddle~
0 bw 10
~
switch %random.3%
  case 1
    say Hey diddle diddle...
  break
  case 2
    %echo% A cow jumps over the moon. Oh, that was just a reflection.
  break
  case 3
    say Have you seen my fiddle?
  break
done
~
#10731
Give rejection~
0 j 100
~
%send% %actor% ~%self% does not want that.
return 0
~
#10732
Jack~
0 bw 10
~
switch %random.3%
  case 1
    say I'm supposed to fetch some water but I've lost my pail.
  break
  case 2
    say I don't like hills.
  break
  case 3
    %echo% ~%self% wears some sticks like a crown.
  break
done
~
#10733
Jill find Jack on reboot~
0 x 0
~
set jack %instance.mob(10732)%
if !%jack%
  halt
end
mgoto %jack%
mfollow %jack%
~
#10734
Jack Be Nimble~
0 bw 10
~
switch %random.3%
  case 1
    say Wanna see me jump something?
  break
  case 2
    say Me mum says jumping is evel.
  break
  case 3
    %echo% ~%self% plants a regular stick in the ground and jumps over it, but it's just not the same.
  break
done
~
#10735
Drop Other Candle Quest~
2 u 100
~
if %actor%
  %quest% %actor% drop 10734
end
~
#10736
Jill~
0 bw 10
~
switch %random.3%
  case 1
    say I'm supposed to fetch some water but I've lost my pail.
  break
  case 2
    say Jack, that hill doesn't look safe.
  break
  case 3
    %echo% ~%self% tumbles around on the ground.
  break
done
~
#10737
Jack Sprat~
0 bw 10
~
switch %random.3%
  case 1
    say The wife says we should feed the pigs more.
  break
  case 2
    say I think we overfeed the pigs.
  break
  case 3
    %echo% ~%self% could eat no fat. His wife could eat no lean.
  break
done
~
#10738
Mother Goose Teleport~
1 c 2
use~
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if %actor.position% != Standing || %actor.action% || %actor.fighting%
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
* once per 30 minutes
if %actor.cooldown(10738)%
  %send% %actor% Your %cooldown.10738% is still on cooldown!
  halt
end
set room_var %actor.room%
* set up 'stop' command
set stop_command 0
set stop_message_char You stop using %self.shortdesc% to teleport.
set stop_message_room ~%actor% stops using %self.shortdesc% to teleport.
set needs_stop_command 1
remote stop_command %actor.id%
remote stop_message_char %actor.id%
remote stop_message_room %actor.id%
remote needs_stop_command %actor.id%
* begin
%send% %actor% You touch @%self% and it begins to swirl with light...
if !%actor.home%
  %send% %actor% WAIT! You haven't set a home! Type 'stop' if you want to do that before teleporting away, and see HELP HOME for more information. Completing this Mother Goose quest will get you a free teleport home using magic breadcrumbs.
end
%echoaround% %actor% ~%actor% touches @%self% and it begins to swirl with light...
set cycle 0
while %cycle% < 2
  wait 5 sec
  if %actor.stop_command% || %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)% || %actor.action%
    %force% %actor% stop cleardata
    %send% %actor% The swirling light from @%self% fades.
    halt
  end
  switch %cycle%
    case 0
      %send% %actor% Yellow light begins to whirl around you...
      %echoaround% %actor% Yellow light begins to whirl around ~%actor%...
    break
    case 1
      set destination %instance.nearest_rmt(10730)%
      if !%destination%
        %send% %actor% Your teleport fails!
        %echoaround% %actor% |%actor% teleport fails!
        %force% %actor% stop cleardata
        halt
      end
      %echoaround% %actor% ~%actor% vanishes in a flourish of yellow light!
      * 2-step teleport to get them to the outside
      %teleport% %actor% %destination%
      set destination %instance.location%
      %teleport% %actor% %destination%
      %force% %actor% look
      %echoaround% %actor% ~%actor% appears in a flourish of yellow light!
    break
  done
  eval cycle %cycle% + 1
done
* cleanup
nop %actor.set_cooldown(10738, 43200)%
nop %actor.cancel_adventure_summon%
%force% %actor% stop cleardata
~
#10739
Jack Horner~
0 bw 10
~
switch %random.3%
  case 1
    say Mum said I have to sit in this corner.
  break
  case 2
    say I can't believe we're out of pie.
  break
  case 3
    %echo% ~%self% licks his lips and looks around for his pie.
  break
done
~
#10740
Mother goose mutually exclusive quests~
2 v 0
~
switch %questvnum%
  case 10732
    %quest% %actor% drop 10733
  break
  case 10733
    %quest% %actor% drop 10732
  break
  case 10734
    %quest% %actor% drop 10735
  break
  case 10735
    %quest% %actor% drop 10734
  break
done
~
#10741
Miss Muffet~
0 bw 10
~
switch %random.3%
  case 1
    say Eek! A spider!
  break
  case 2
    say Has anyone seen my curds and whey?
  break
  case 3
    %echo% ~%self% dusts off her tuffet.
  break
done
~
#10743
Mary~
0 bw 10
~
switch %random.3%
  case 1
    say I had a little lamb, somewhere...
  break
  case 2
    say Oh where, oh where has my little lamb gone?
  break
  case 3
    %echo% ~%self% seems to have lost her bell.
  break
done
~
#10744
Minipet use - little lamb bell version~
1 c 3
ring~
if %actor.obj_target(%arg%)% != %self% && !(%self.is_name(%arg%)% && %self.worn_by%)
  return 0
  halt
end
if %actor.has_minipet(%self.val0%)%
  %send% %actor% You already have the lamb minipet.
else
  nop %actor.add_minipet(%self.val0%)%
  %send% %actor% You ring @%self% and gain a little lamb minipet! (see HELP MINIPET)
  %echoaround% %actor% ~%actor% rings @%self%.
end
~
#10746
Old Woman who lived in a shoe~
0 bw 10
~
switch %random.3%
  case 1
    say I'll whip those children if they don't behave.
  break
  case 2
    say I have so many children, I don't know what to do.
  break
  case 3
    %echo% ~%self% seems to have lost her broth.
  break
done
~
#10748
Mother Goose spawn~
0 n 100
~
if (!%instance.location% || %self.room.template% != 10730)
  halt
end
%echo% ~%self% vanishes!
mgoto %instance.location%
%echo% ~%self% exits from the giant shoe.
if (%self.vnum% == 10732)
  * Jack: load Jill
  %load% mob 10733 ally
end
if (%self.vnum% != 10746)
  * If not Old Woman, move about a bit
  mmove
  mmove
  mmove
  mmove
end
~
#10749
Mother Goose: Breadcrumbs teleport you home~
1 c 2
use~
* breadcrumb trinket: shorter copy of hestian trinket
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
if %actor.position% != Standing || %actor.action% || %actor.fighting%
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
set home %actor.home%
if !%home%
  %send% %actor% You have no home to teleport back to with these breadcrumbs (see HELP HOME).
  halt
end
set veh %home.in_vehicle%
if %veh%
  set outside_room %veh.room%
  if !%actor.canuseroom_guest(%outside_room%)%
    %send% %actor% You can't teleport home to a vehicle that's parked on foreign territory you don't have permission to use!
    halt
  elseif !%actor.can_teleport_room(%outside_room%)%
    %send% %actor% You can't teleport to your home's current location.
    halt
  end
end
* check tiemr
if %actor.cooldown(10749)%
  %send% %actor% Your %cooldown10749% is on cooldown!
  halt
end
set room_var %actor.room%
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
%send% %actor% You sprinkle some breadcrumbs and feel the wind whip them up into the air...
%echoaround% %actor% ~%actor% sprinkles some breadcrumbs, which catch the wind and whirl around *%actor%...
set cycle 0
while %cycle% < 2
  wait 5 sec
  if %actor.stop_command% || %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor% || %actor.aff_flagged(DISTRACTED)% || %actor.action%
    %force% %actor% stop cleardata
    %send% %actor% The breadcrums that were whirling around you fall to the ground and rot.
    halt
  end
  switch %cycle%
    case 0
      %send% %actor% You're enveloped by a swirl of breadcrumbs...
      %echoaround% %actor% ~%actor% is enveloped by the swirl of breadcrumbs...
    break
    case 1
      %echoaround% %actor% ~%actor% vanishes in a burst of breadcrumbs!
      %teleport% %actor% %actor.home%
      %force% %actor% look
      %echoaround% %actor% ~%actor% appears in a burst of breadcrumbs!
    break
  done
  eval cycle %cycle% + 1
done
* cleanup
nop %actor.set_cooldown(10749, 300)%
nop %actor.cancel_adventure_summon%
%force% %actor% stop cleardata
~
#10750
Sell spider parts to Miner Nynar~
1 c 2
sell~
* Test keywords
if !%self.is_name(%arg%)%
  return 0
  halt
end
* find Miner Nynar
set iter %actor.room.people%
set found 0
while %iter% && !%found%
  if %iter.vnum% == 10754
    set found 1
  end
  set iter %iter.next_in_room%
done
if !%found%
  return 0
  halt
end
%send% %actor% You sell @%self% to Miner Nynar for 5 goblin coins.
%echoaround% %actor% ~%actor% sells @%self% to Miner Nynar.
nop %actor.give_coins(5)%
%purge% %self%
~
#10751
Sell spider meat to Miner Meena~
1 c 2
sell~
* Test keywords
if !%self.is_name(%arg%)%
  return 0
  halt
end
* find Miner Nynar
set iter %actor.room.people%
set found 0
while %iter% && !%found%
  if %iter.vnum% == 10755
    set found 1
  end
  set iter %iter.next_in_room%
done
if !%found%
  return 0
  halt
end
%send% %actor% You sell @%self% to Miner Meena for 5 goblin coins.
%echoaround% %actor% ~%actor% sells @%self% to Miner Meena.
nop %actor.give_coins(5)%
%purge% %self%
~
#10752
Goblin Mine Shops~
0 c 0
buy~
set vnum -1
set named a thing
set nynar_is_here 0
set meena_is_here 0
set blacklung_is_here 0
set hanx_is_here 0
set person %self.room.people%
while %person%
  if %person.vnum% == 10754
    set nynar_is_here 1
  elseif %person.vnum% == 10755
    set meena_is_here 1
  elseif %person.vnum% == 10757
    set blacklung_is_here 1
  elseif %person.vnum% == 10758
    set hanx_is_here 1
  end
  set person %person.next_in_room%
done
if (!%arg%)
  %send% %actor% ~%self% tells you, 'What you want?'
  %send% %actor% (Type 'buy <item>' to spend 50 coins and buy something.)
  if %meena_is_here%
    %send% %actor% Meena sells: a goblin pick ('buy pick')
  end
  if %nynar_is_here%
    %send% %actor% Nynar sells: a bug potion ('buy bug potion')
  end
  if %blacklung_is_here%
    %send% %actor% Blacklung sells: a goblin coffin ('buy coffin')
  end
  if %hanx_is_here%
    %send% %actor% Hanx sells: a goblin raft ('buy raft')
  end
  halt
elseif pick ~= %arg%
  if !%meena_is_here%
    %send% %actor% ~%self% tells you, 'Meena sell that. She not here.'
    return 1
    halt
  else
    set vnum 10768
    set named a goblin pick
  end
elseif bug potion ~= %arg%
  if !%nynar_is_here%
    %send% %actor% ~%self% tells you, 'Nynar sell that. He not here.'
    return 1
    halt
  else
    set vnum 10754
    set named a bug potion
  end
elseif coffin ~= %arg%
  if !%blacklung_is_here%
    %send% %actor% ~%self% tells you, 'Blacklung sell that. He not here.'
    return 1
    halt
  else
    set vnum 10770
    set named a goblin coffin
  end
elseif raft ~= %arg%
  if !%hanx_is_here%
    %send% %actor% ~%self% tells you, 'Hanx sell that. He not here.'
    return 1
    halt
  else
    set vnum 10771
    set named a goblin raft
    set is_veh 1
  end
else
  %send% %actor% ~%self% tells you, 'Goblin don't sell %arg%.'
  halt
end
if !%actor.can_afford(50)%
  %send% %actor% ~%self% tells you, 'Big human needs 50 coin to buy that.'
  halt
end
nop %actor.charge_coins(50)%
if !%is_veh%
  %load% obj %vnum% %actor% inv 25
else
  %load% veh %vnum% 25
  set emp %actor.empire%
  set veh %self.room.vehicles%
  if %emp%
    %own% %veh% %emp%
  end
end
%send% %actor% You buy %named% for 50 coins.
%echoaround% %actor% ~%actor% buys %named%.
~
#10753
Buy Potion/Nynar~
0 c 0
buy~
set vnum -1
set named a thing
if (!%arg%)
  %send% %actor% ~%self% tells you, 'Nynar only sell bug potion.'
  %send% %actor% (Type 'buy potion' to spend 30 coins and buy a bug potion.)
  halt
elseif bug potion ~= %arg%
  set vnum 10754
  set named a bug potion
else
  %send% %actor% ~%self% tells you, 'Nynar don't sell %arg%.'
  halt
end
if !%actor.can_afford(30)%
  %send% %actor% ~%self% tells you, 'Big human needs 30 coin to buy that.'
  halt
end
nop %actor.charge_coins(30)%
%load% obj %vnum% %actor% inv 25
%send% %actor% You buy %named% for 30 coins.
%echoaround% %actor% ~%actor% buys %named%.
~
#10754
Nynar env~
0 bw 10
~
* NO LONGER USED -- replaced by mob custom msgs
switch %random.4%
  case 1
    say So much webs... So much webs...
    %echo% ~%self% shivers uncomfortably.
  break
  case 2
    say Nynar sell bug potion for 30 coin!
    %echo% (Type 'buy potion' to buy one.)
  break
  case 3
    %echo% ~%self% scratches *%self%self all over.
  break
  case 4
    say Never going back there.
  break
done
~
#10755
Meena env~
0 bw 10
~
* NO LONGER USED - replaced by mob custom messages
switch %random.4%
  case 1
    say All of the screaming!
    %echo% ~%self% shivers uncomfortably.
  break
  case 2
    say Meena sell goblin pick for 50 coin!
    %echo% (Type 'buy pick' to buy one.)
  break
  case 3
    %echo% ~%self% leaps up suddenly, as if something crawled up her leg.
  break
  case 4
    say Meena should have been a major.
  break
done
~
#10756
Goblin Miner Spawn~
0 n 100
~
if (!%instance.location% || %self.room.template% != 10750)
  halt
end
%echo% ~%self% flees the mine!
mgoto %instance.location%
%echo% ~%self% comes screaming out of the mine!
mmove
mmove
mmove
mmove
mmove
mmove
mmove
mmove
mmove
mmove
~
#10757
Widow Spider Complete~
0 f 100
~
%buildingecho% %self.room% You hear the terrifying skree of the widow spider dying!
%load% obj 10769
return 0
~
#10758
Buy Coffin/Blacklung~
0 c 0
buy~
set vnum -1
set named a thing
if (!%arg%)
  %send% %actor% ~%self% tells you, 'Blacklung only sell coffin.'
  %send% %actor% (Type 'buy coffin' to spend 50 coins and buy a goblin coffin.)
  halt
elseif coffin ~= %arg%
  set vnum 10770
  set named a goblin coffin
else
  %send% %actor% ~%self% tells you, 'Blacklung don't sell %arg%.'
  halt
end
if !%actor.can_afford(50)%
  %send% %actor% ~%self% tells you, 'Big human needs 50 coin to buy that.'
  halt
end
nop %actor.charge_coins(50)%
%load% obj %vnum% %actor% inv 25
%send% %actor% You buy %named% for 50 coins.
%echoaround% %actor% ~%actor% buys %named%.
~
#10759
Buy Raft/Hanx~
0 c 0
buy~
set vnum -1
set named a thing
if (!%arg%)
  %send% %actor% ~%self% tells you, 'Hanx only sell raft.'
  %send% %actor% (Type 'buy raft' to spend 50 coins and buy a goblin raft.)
  halt
elseif raft ~= %arg%
  set vnum 10771
  set named a goblin raft
else
  %send% %actor% ~%self% tells you, 'Hanx don't sell %arg%.'
  halt
end
if !%actor.can_afford(50)%
  %send% %actor% ~%self% tells you, 'Big human needs 50 coin to buy that.'
  halt
end
nop %actor.charge_coins(50)%
%load% veh %vnum% 25
set emp %actor.empire%
set veh %self.room.vehicles%
if %emp%
  %own% %veh% %emp%
end
%send% %actor% You buy %named% for 50 coins.
%echoaround% %actor% ~%actor% buys %named%.
~
#10760
Widow Spider: Bind~
0 k 100
~
if %self.cooldown(10761)%
  halt
end
set heroic_mode %self.mob_flagged(GROUP)%
if !%heroic_mode%
  halt
end
* Find a non-bound target
set target %actor%
set person %self.room.people%
set target_found 0
set no_targets 0
while %target.affect(10760)% && %person%
  if %person.is_pc% && %person.is_enemy(%self%)%
    set target %person%
  end
  set person %person.next_in_room%
done
if !%target%
  * Sanity check
  halt
end
if %target.affect(10760)%
  * No valid targets
  halt
end
nop %self.cooldown(10761, 15)%
* Valid target found, start attack
%send% %target% ~%self% twists around, pointing ^%self% spinneret at you...
%echoaround% %target% ~%self% twists around, pointing ^%self% spinneret at ~%target%...
wait 3 sec
%send% %target% A stream of sticky webs flies out, binding your limbs!
%echoaround% %target% A stream of sticky webs flies out, binding |%target% limbs!
%send% %target% Type 'struggle' to break free!
dg_affect #10760 %actor% STUNNED on 15
~
#10761
Widow Struggle Bind Struggle~
0 c 0
struggle~
set break_free_at 1
if !%actor.affect(10760)%
  return 0
  halt
end
if !%actor.varexists(struggle_counter)%
  set struggle_counter 0
  remote struggle_counter %actor.id%
else
  set struggle_counter %actor.struggle_counter%
end
eval struggle_counter %struggle_counter% + 1
if %struggle_counter% >= %break_free_at%
  %send% %actor% You break free of your bindings!
  %echoaround% %actor% ~%actor% breaks free of ^%actor% bindings!
  dg_affect #10760 %actor% off
  rdelete struggle_counter %actor.id%
  halt
else
  %send% %actor% You struggle against your bindings, but fail to break free.
  %echoaround% %actor% ~%actor% struggles against ^%actor% bindings!
  remote struggle_counter %actor.id%
  halt
end
~
#10769
Delayed Completer~
1 f 0
~
%adventurecomplete%
~
#10770
Instant tomb cooldown~
1 s 100
~
if %command% != build
  halt
end
set varname tomb%self.vnum%
* once per 6 hours
if %actor.cooldown(%self.vnum%)%
  %send% %actor% You must wait before using @%self% again.
  return 1
  halt
end
nop %actor.set_cooldown(%self.vnum%, 21600)%
return 1
~
#10771
Goblin gravesite decay timer~
1 f 0
~
if %self.room.building% == Goblin Gravesite
  %build% %self.room% demolish
end
~
#10772
Goblin gravesite setup timer~
2 o 100
~
set obj %room.contents%
while %obj%
  set next %obj.next_in_list%
  if %obj.vnum% == 10771
    %purge% %obj%
  end
  set obj %next%
done
%load% obj 10771
~
#10773
Goblin raft replacer~
1 n 100
~
%load% veh 10771
%purge% %self%
~
#10775
Fog Bank Environment~
2 bw 10
~
switch %random.4%
  case 1
    %echo% The fog rolls around you, making it hard to see.
  break
  case 2
    %echo% The fog around you is thick, almost suffocating.
  break
  case 3
    %echo% You wonder where this fog might have come from, as it isn't foggy anywhere else in the area.
  break
  case 4
    %echo% You nearly lose your way in the fog.
  break
done
~
#10776
Dust Storm Environment~
2 bw 10
~
switch %random.4%
  case 1
    %echo% The dust storm nearly knocks you off your feet!
  break
  case 2
    %echo% The dust storm makes it hard to see.
  break
  case 3
    %echo% The dust storm must have come out of nowhere. The rest of the desert is clear.
  break
  case 4
    %echo% This is the worst dust storm you've seen in a long time.
  break
done
~
#10777
Fruit of Knowledge consume~
1 s 100
~
%load% obj 10780 %actor% inv
~
#10778
Tree of Knowledge spawner~
2 e 100
~
if %room.building_vnum% == 10775
  %build% %room% 10778
elseif %room.building_vnum% == 10776
  %build% %room% 10779
end
~
#10779
Pick fruit of knowledge~
2 c 0
pick~
if !%actor.canuseroom_member()%
  %send% %actor% You don't have permission to pick anything here.
  return 1
  halt
end
%load% obj 10777 %actor% inv
%send% %actor% You pick the fruit of knowledge from the tree, which withers and dies!
%echoaround% %actor% ~%actor% picks the fruit of knowledge from the tree, which withers and dies!
if %room.building_vnum% == 10779
  %terraform% %room% 10776
else
  %terraform% %room% 10775
end
return 1
~
#10780
Tree of Knowledge skill gain~
1 n 100
~
* Script is called by a temporary item on a VERY short delay
wait 1
set actor %self.carried_by%
if !%actor%
  %purge% %self%
  %halt%
end
set try 10
set done 0
while %try% > 0 && !%done%
  * Determine which skill
  set vnum %random.7%
  if %vnum% == 1
    * Skip 1 (Empire) and give 0 (Battle) instead
    set vnum 0
  end
  * Attempt to gain the skill
  if !%actor.noskill(%vnum%)%
    if (%actor.skill(%vnum%)% > 0 && %actor.skill(%vnum%)% < 50) || (%actor.skill(%vnum%)% > 50 && %actor.skill(%vnum%)% < 75) || (%actor.skill(%vnum%)% > 75 && %actor.skill(%vnum%)% < 100)
      nop %actor.gain_skill(%vnum%, 1)%
      set done 1
    end
  end
  eval try %try% - 1
done
%purge% %self%
~
$
