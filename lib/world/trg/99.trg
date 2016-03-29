#9900
Mini-pet Use~
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
eval varname minipet%self.val0%
eval test %%actor.varexists(%varname%)%%
* once per 30 minutes
if %test%
  eval tt %%actor.%varname%%%
  if (%timestamp% - %tt%) < 1800
    eval diff (%tt% - %timestamp%) + 1800
    eval diff2 %diff%/60
    eval diff %diff%//60
    if %diff%<10
      set diff 0%diff%
    end
    %send% %actor% You must wait %diff2%:%diff% to use %self.shortdesc% again.
    halt
  end
end
* check too many mobs
eval mobs 0
eval found 0
eval found_pet 0
eval room_var %self.room%
eval ch %room_var.people%
while %ch% && !%found%
  if (%ch.is_npc% && %ch.vnum% == %self.val0% && %ch.master% && %ch.master% == %actor%)
    eval found 1
  elseif (%ch.is_npc% && %ch.master% && %ch.master% == %actor% && !%ch.mob_flagged(FAMILIAR)%)
    eval found_pet 1
  elseif %ch.is_npc%
    eval mobs %mobs% + 1
  end
  eval ch %ch.next_in_room%
done
if %found%
  %send% %actor% You already have this mini-pet.
elseif %found_pet% then
  %send% %actor% You already have another non-familiar follower.
elseif %mobs% > 4
  %send% %actor% There are too many mobs here already.
else
  %send% %actor% You use %self.shortdesc%...
  %echoaround% %actor% %actor.name% uses %self.shortdesc%...
  %load% m %self.val0%
  eval pet %room_var.people%
  if (%pet% && %pet.vnum% == %self.val0%)
    %force% %pet% mfollow %actor%
    %echo% %pet.name% appears!
    eval %varname% %timestamp%
    remote %varname% %actor.id%
    dg_affect %pet% *CHARM on -1
  end
end
~
#9901
Dismissable~
0 ct 0
dismiss~
eval test %%self.is_name(%arg%)%%
if !%test%
  return 0
  halt
end
if (%self.master% && %self.master% == %actor%)
  %send% %actor% You dismiss %self.name%.
  %echoaround %actor% %actor.name% dismisses %self.name%.
  %purge% %self%
else
  * ordinary dismiss error
  return 0
end
~
#9902
Lonely Despawn~
0 abt 5
~
eval count 0
eval room_var %self.room%
eval target_char %room_var.people%
while %target_char%
  if (%target_char.is_pc%)
    eval count %count% + 1
  end
  eval target_char %target_char.next_in_room%
done
* Despawn if no players present
if %count% < 1
  %purge% %self%
end
~
$
