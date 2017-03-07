#18450
Rising water terraformer~
0 i 100
~
eval room %self.room%
if !%instance.location%
  %purge% %self%
  halt
end
eval dist %%room.distance(%instance.location%)%%
if (%dist% > 5)
  mgoto %instance.location%
elseif %room.aff_flagged(*HAS-INSTANCE)%
  halt
elseif (%room.sector% == Plains || %room.sector% == Crop)
  %terraform% %room% 18451
  %echo% The rising water from the nearby river floods the plains!
elseif (%room.sector% == Light Forest || %room.sector% == Forest || %room.sector% == Shady Forest || %room.sector% == Overgrown Forest)
  %terraform% %room% 18452
  %echo% The rising water from the nearby river floods the forest!
elseif (%room.sector% == Light Jungle || %room.sector% == Jungle || %room.sector% == Jungle Crop)
  %terraform% %room% 29
  %echo% The rising water from the nearby river floods the jungle!
elseif (%room.sector% == Desert || %room.sector% == Grove || %room.sector% == Desert Crop)
  %terraform% %room% 18453
  %echo% The rising water from the nearby river floods the desert!
end
~
#18451
Dire beaver spawn~
0 n 100
~
eval room %self.room%
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
%terraform% %room% 18450
~
#18453
Beaver dam construction~
0 i 100
~
eval room %self.room%
if !%instance.location%
  %purge% %self%
  halt
end
eval dist %%room.distance(%instance.location%)%%
if (%dist% > 2)
  %echo% %self.name% scrambles into a hole in the dam and returns to %self.hisher% lodge.
  mgoto %instance.location%
  %echo% %self.name% appears from %self.hisher% lodge.
elseif %room.aff_flagged(*HAS-INSTANCE)%
  halt
elseif (%room.sector% == River && %dist% <= 2)
  %build% %room% 18451
  %echo% %self.name% expands %self.hisher% dam here.
end
~
#18454
Dire beaver death: stop flooding~
0 f 100
~
if !%instance%
  halt
end
eval mob %instance.mob(18451)%
if %mob%
  %echo% The rising water stops.
  %purge% %mob%
end
~
$
