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
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
eval room_var %actor.room%
%send% %actor% You start unwrapping %self.shortdesc%...
%echoaround% %actor% %actor.name% starts unwrapping %self.shortdesc%...
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || %self.carried_by% != %actor% || %actor.disabled%
  halt
end
if !%actor.varexists(last_christmas_gift_item)%
  eval last_christmas_gift_item 10714
else
  eval last_christmas_gift_item %actor.last_christmas_gift_item%
end
eval next_gift 0
switch %last_christmas_gift_item%
  case 10708
    eval next_gift 10709
  break
  case 10709
    eval next_gift 10704
  break
  case 10704
    eval next_gift 10710
  break
  case 10710
    eval next_gift 10706
  break
  case 10706
    eval next_gift 10717
  break
  case 10717
    eval next_gift 10714
  break
  case 10714
    eval next_gift 10708
  break
done
%load% obj %next_gift% %actor% inv
eval item %actor.inventory()%
%send% %actor% You finish unwrapping the gift and find %item.shortdesc% inside!
%echoaround% %actor% %actor.name% finishes unwrapping the gift and finds %item.shortdesc%!
eval last_christmas_gift_item %next_gift%
remote last_christmas_gift_item %actor.id%
%purge% %self%
~
#10703
Father Christmas gift give~
0 g 100
~
if !%actor.is_pc% || %actor.nohassle%
  halt
end
if %self.varexists(gave_gifts)%
wait 4
%echo% %self.name% seems to be out of gifts!
  halt
end
wait 1 sec
%echo% %self.name% stands up to greet you.
wait 3 sec
say Ho ho ho! Merry Christmas!
wait 1 sec
eval room %self.room%
eval person %room.people%
* Gifts for everyone!
eval count 0
while %person%
  if %person.is_pc%
    %send% %person% %self.name% gives you a gift!
    %echoaround% %person% %self.name% gives %person.name% a gift!
    %load% obj 10702 %person%
    eval count %count%+1
  end
  eval person %person.next_in_room%
done
if %count%==0
  * Everyone left :(
  halt
end
* Gave at least one gift
eval gave_gifts 1
remote gave_gifts %self.id%
%adventurecomplete%
~
#10704
Completer~
0 f 100
~
%adventurecomplete%
~
#10710
Faster Hestian Trinket (snowglobe)~
1 c 2
use~
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
if (%actor.position% != Standing)
  %send% %actor% You can't do that right now.
  halt
end
if !%actor.can_teleport_room% || !%actor.canuseroom_guest%
  %send% %actor% You can't teleport out of here.
  halt
end
if !%actor.home%
  %send% %actor% You have no home to teleport back to with this trinket.
  halt
end
* once per 60 minutes
if %actor.varexists(last_hestian_time)%
  if (%timestamp% - %actor.last_hestian_time%) < 1800
    eval diff (%actor.last_hestian_time% - %timestamp%) + 1800
    eval diff2 %diff%/60
    eval diff %diff%//60
    if %diff%<10
      set diff 0%diff%
    end
    %send% %actor% You must wait %diff2%:%diff% to use %self.shortdesc%.
    halt
  end
end
eval room_var %actor.room%
%send% %actor% You shake %self.shortdesc% and it begins to swirl with light...
%echoaround% %actor% %actor.name% shakes %self.shortdesc% and it begins to swirl with light...
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor%
  halt
end
%send% %actor% %self.shortdesc% glows a wintery white and the light begins to envelop you!
%echoaround% %actor% %self.shortdesc% glows a wintery white and the light begins to envelop %actor.name%!
wait 5 sec
if %actor.room% != %room_var% || %actor.fighting% || !%actor.home% || %self.carried_by% != %actor%
  halt
end
%echoaround% %actor% %actor.name% vanishes in a flash of light!
%teleport% %actor% %actor.home%
%force% %actor% look
eval last_hestian_time %timestamp%
remote last_hestian_time %actor.id%
nop %actor.cancel_adventure_summon%
~
#10713
Reindeer spawner~
1 n 100
~
if %random.100% <= 14
  %load% mob 10700
else
  %load% mob 10705
end
%purge% %self%
~
#10730
Hey Diddle Diddle~
0 b 10
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
Cat/Fiddle~
0 j 100
~
if %object.vnum% != 10730
  %send% %actor% %self.name% does not want that.
  return 0
  halt
end
%load% obj 10731 %actor% inv 25
%send% %actor% %self.name% gives you a starry cloak in exchange for the fiddle!
%echoaround% %actor% %self.name% gives %actor.name% a starry cloak in exchange for a fiddle!
%purge% %object%
return 0
~
#10732
Jack~
0 b 10
~
switch %random.3%
  case 1
    say I'm supposed to fetch some water but I've lost my pail.
  break
  case 2
    say I don't like hills.
  break
  case 3
    %echo% %self.name% wears some sticks like a crown.
  break
done
~
#10733
Jack/Pail~
0 j 100
~
if %object.vnum% != 10732
  %send% %actor% %self.name% does not want that.
  return 0
  halt
end
%load% obj 10733 %actor% inv 25
%send% %actor% %self.name% gives you a pair of fetching trousers in exchange for the pail!
%echoaround% %actor% %self.name% gives %actor.name% a pair of fetching trousers in exchange for a pail!
%purge% %object%
return 0
~
#10734
Jack Be Nimble~
0 b 10
~
switch %random.3%
  case 1
    say Wanna see me jump something?
  break
  case 2
    say Me mum says jumping is evel.
  break
  case 3
    %echo% plants a regular stick in the ground and jumps over it, but it's just not the same.
  break
done
~
#10735
Jack/Candlestick~
0 j 100
~
if (%object.vnum% != 10734 && %object.vnum% != 10735)
  %send% %actor% %self.name% does not want that.
  return 0
  halt
end
%load% obj 10736 %actor% inv 25
%send% %actor% %self.name% gives you a pair of burnt shoes in exchange for the candlestick!
%echoaround% %actor% %self.name% gives %actor.name% a pair of burnt shoes in exchange for a candlestick!
%purge% %object%
return 0
~
#10736
Jill~
0 b 10
~
switch %random.3%
  case 1
    say I'm supposed to fetch some water but I've lost my pail.
  break
  case 2
    say Jack, that hill doesn't look safe.
  break
  case 3
    %echo% %self.name% tumbles around on the ground.
  break
done
~
#10737
Jack Sprat~
0 b 10
~
switch %random.3%
  case 1
    say The wife says we should feed the pigs more.
  break
  case 2
    say I think we overfeed the pigs.
  break
  case 3
    %echo% %self.name% could eat no fat. His wife could eat no lean.
  break
~
#10738
Jack/Ham~
0 j 100
~
if %object.vnum% != 10737
  %send% %actor% %self.name% does not want that.
  return 0
  halt
end
%load% obj 10738 %actor% inv 25
%send% %actor% %self.name% gives you a lean belt in exchange for the fatty ham!
%echoaround% %actor% %self.name% gives %actor.name% a lean belt in exchange for a fatty ham!
%purge% %object%
return 0
~
#10739
Jack Horner~
0 b 10
~
switch %random.3%
  case 1
    say Mum said I have to sit in this corner.
  break
  case 2
    say I can't believe we're out of pie.
  break
  case 3
    %echo% %self.name% licks his lips and looks around for his pie.
  break
done
~
#10740
Jack/Pie~
0 j 100
~
if %object.vnum% != 10739
  %send% %actor% %self.name% does not want that.
  return 0
  halt
end
%load% obj 10740 %actor% inv 25
%send% %actor% %self.name% gives you a pair of plum-stained gloves in exchange for the Christmas pie!
%echoaround% %actor% %self.name% gives %actor.name% a pair of plum-stained gloves in exchange for a Christmas pie!
%purge% %object%
return 0
~
#10741
Miss Muffet~
0 b 10
~
switch %random.3%
  case 1
    say Eek! A spider!
  break
  case 2
    say Has anyone seen my curds and whey?
  break
  case 3
    %echo% %self.name% dusts off her tuffet.
  break
done
~
#10742
Muffet/Curds~
0 j 100
~
if %object.vnum% != 10741
  %send% %actor% %self.name% does not want that.
  return 0
  halt
end
%load% obj 10742 %actor% inv 25
%send% %actor% %self.name% gives you a spider in exchange for the curds and whey!
%echoaround% %actor% %self.name% gives %actor.name% a spider in exchange for some curds and whey!
%purge% %object%
return 0
~
#10743
Mary~
0 b 10
~
switch %random.3%
  case 1
    say I had a little lamb, somewhere...
  break
  case 2
    say Oh where, oh where has my little lamb gone?
  break
  case 3
    %echo% %self.name% seems to have lost her bell.
  break
done
~
#10744
Mary/Lamb~
0 j 100
~
if %object.vnum% != 10744
  %send% %actor% %self.name% does not want that.
  return 0
  halt
end
%load% obj 10743 %actor% inv 25
%send% %actor% %self.name% gives you some warm fleece in exchange for the bell!
%echoaround% %actor% %self.name% gives %actor.name% some warm fleece in exchange for a bell and ribbon!
%purge% %object%
return 0
~
#10745
Jill/Pail~
0 j 100
~
if %object.vnum% != 10732
  %send% %actor% %self.name% does not want that.
  return 0
  halt
end
%load% obj 10745 %actor% inv 25
%send% %actor% %self.name% gives you a fetching skirt in exchange for the pail!
%echoaround% %actor% %self.name% gives %actor.name% a fetching skirt in exchange for a pail!
%purge% %object%
return 0
~
#10746
Old Woman who lived in a shoe~
0 b 10
~
switch %random.3%
  case 1
    say I'll whip those children if they don't behave.
  break
  case 2
    say I have so many children, I don't know what to do.
  break
  case 3
    %echo% %self.name% seems to have lost her broth.
  break
done
~
#10747
Woman/Broth~
0 j 100
~
if %object.vnum% != 10746
  %send% %actor% %self.name% does not want that.
  return 0
  halt
end
%load% obj 10747 %actor% inv 25
%send% %actor% %self.name% gives you a spare shoestring in exchange for the broth!
%echoaround% %actor% %self.name% gives %actor.name% a spare shoestring in exchange for a leathery broth!
%purge% %object%
return 0
~
#10748
Mother Goose spawn~
0 n 100
~
eval room %self.room%
if (!%instance.location% || %room.template% != 10730)
  halt
end
%echo% %self.name% vanishes!
mgoto %instance.location%
%echo% %self.name% exits from the giant shoe.
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
#10750
Sell spider parts to Miner Nynar~
1 c 2
sell~
eval test %%self.is_name(%arg%)%%
* Test keywords
if !%test%
  return 0
  halt
end
* find Miner Nynar
eval room %actor.room%
eval iter %room.people%
eval found 0
while %iter% && !%found%
  if %iter.vnum% == 10754
    eval found 1
  end
  eval iter %iter.next_in_room%
done
if !%found%
  return 0
  halt
end
%send% %actor% You sell %self.shortdesc% to Miner Nynar for 5 goblin coins.
%echoaround% %actor% %actor.name% sells %self.shortdesc% to Miner Nynar.
nop %actor.give_coins(5)%
%purge% %self%
~
#10751
Sell spider meat to Miner Meena~
1 c 2
sell~
eval test %%self.is_name(%arg%)%%
* Test keywords
if !%test%
  return 0
  halt
end
* find Miner Nynar
eval room %actor.room%
eval iter %room.people%
eval found 0
while %iter% && !%found%
  if %iter.vnum% == 10755
    eval found 1
  end
  eval iter %iter.next_in_room%
done
if !%found%
  return 0
  halt
end
%send% %actor% You sell %self.shortdesc% to Miner Meena for 5 goblin coins.
%echoaround% %actor% %actor.name% sells %self.shortdesc% to Miner Meena.
nop %actor.give_coins(5)%
%purge% %self%
~
#10752
Buy Pick/Meena~
0 c 0
buy~
eval vnum -1
set named a thing
if (!%arg%)
  %send% %actor% %self.name% tells you, 'Meena only sell pick.'
  %send% %actor% (Type 'buy pick' to spend 50 coins and buy a goblin pick.)
  halt
elseif pick ~= %arg%
  eval vnum 10768
  set named a goblin pick
else
  %send% %actor% %self.name% tells you, 'Meena don't sell %arg%.'
  halt
end
if !%actor.can_afford(50)%
  %send% %actor% %self.name% tells you, 'Big human needs 50 coin to buy that.'
  halt
end
nop %actor.charge_coins(50)%
%load% obj %vnum% %actor% inv 25
%send% %actor% You buy %named% for 50 coins.
%echoaround% %actor% %actor.name% buys %named%.
~
#10753
Buy Potion/Nynar~
0 c 0
buy~
eval vnum -1
set named a thing
if (!%arg%)
  %send% %actor% %self.name% tells you, 'Nynar only sell bug potion.'
  %send% %actor% (Type 'buy potion' to spend 30 coins and buy a bug potion.)
  halt
elseif bug potion ~= %arg%
  eval vnum 10754
  set named a bug potion
else
  %send% %actor% %self.name% tells you, 'Nynar don't sell %arg%.'
  halt
end
if !%actor.can_afford(30)%
  %send% %actor% %self.name% tells you, 'Big human needs 30 coin to buy that.'
  halt
end
nop %actor.charge_coins(30)%
%load% obj %vnum% %actor% inv 25
%send% %actor% You buy %named% for 30 coins.
%echoaround% %actor% %actor.name% buys %named%.
~
#10754
Nynar env~
0 b 10
~
switch %random.4%
  case 1
    say So much webs... So much webs...
    %echo% %self.name% shivers uncomfortably.
  break
  case 2
    say Nynar sell bug potion for 30 coin!
    %echo% (Type 'buy potion' to buy one.)
  break
  case 3
    %echo% %self.name% scratches %self.himher%self all over.
  break
  case 4
    say Never going back there.
  break
done
~
#10755
Meena env~
0 b 10
~
switch %random.4%
  case 1
    say All of the screaming!
    %echo% %self.name% shivers uncomfortably.
  break
  case 2
    say Meena sell goblin pick for 50 coin!
    %echo% (Type 'buy pick' to buy one.)
  break
  case 3
    %echo% %self.name% leaps up suddenly, as if something crawled up her leg.
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
eval room %self.room%
if (!%instance.location% || %room.template% != 10750)
  halt
end
%echo% %self.name% flees the mine!
mgoto %instance.location%
%echo% %self.name% comes screaming out of the mine!
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
%adventurecomplete%
return 0
~
$
