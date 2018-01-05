#12600
Elemental trap~
1 c 2
trap~
if !%arg%
  %send% %actor% Trap whom?
  halt
end
eval room %self.room%
eval target %%actor.char_target(%arg%)%%
if !%target%
  %send% %actor% They're not here.
  halt
end
if %target.is_pc%
  %send% %actor% That seems kind of mean, don't you think?
  halt
elseif %target.vnum% < 12601 || %target.vnum% > 12605
  %send% %actor% You can only use this trap on elementals from the rift.
  halt
elseif %target.fighting%
  %send% %actor% You can't trap someone who is fighting.
  halt
else
  %send% %actor% You capture %target.name% with %self.shortdesc%!
  %echoneither% %actor% %target% %actor.name% captures %target.name% with %self.shortdesc%!
  * Essences have the same vnum as the mob
  %load% obj %target.vnum% %actor% inv
  eval obj %actor.inventory%
  %send% %actor% You receive %obj.shortdesc%!
  %purge% %target%
  %purge% %self%
end
~
#12605
Elemental Rift spawn~
0 n 100
~
eval room %instance.location%
if !%room%
  halt
end
mgoto %room%
if %self.vnum% != 12600
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
end
if %self.vnum% == 12602
  %load% obj 12611 %self% inv
end
~
#12606
Elemental Death~
0 f 100
~
if %instance.start%
  %at% %instance.start% %load% obj 12610
end
eval obj %self.inventory%
while %obj%
  eval next_obj %obj.next_in_list%
  %purge% %obj%
  eval obj %next_obj%
done
~
#12609
Delayed completion on quest start~
0 uv 0
~
if %instance.start%
  %at% %instance.start% %load% obj 12610
end
~
#12610
Delayed Completer~
1 f 0
~
%adventurecomplete%
~
$
