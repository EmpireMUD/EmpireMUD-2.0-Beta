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
$
