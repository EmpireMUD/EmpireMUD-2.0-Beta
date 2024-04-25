#1502
Silent Ascension: Detect pickpocket~
1 c 2
pickpocket~
* basic checks first
return 0
set target %actor.char_target(%arg%)%
if !%target%
  halt
elseif !%actor.on_quest(1502)% || !%actor.ability(Pickpocket)%
  halt
elseif %actor.empire% && %target.empire% == %actor.empire%
  * must be different empire
  halt
elseif %target.mob_flagged(*PICKPOCKETED)%
  halt
end
* ok to proceed
return 1
set person %self.room.people%
while %person%
  if %person.mob_flagged(CITYGUARD)%
    %force% %person% say Stop, %actor.name%! You aren't going to pickpocket anything on my watch!
    halt
  end
  set person %person.next_in_room%
done
* going to attempt to detect the true result of pickpocket
set inv_start %actor.inventory%
set coins %actor.coins%
* let them try it...
return 0
wait 0
* did we actually pickpocket something?
if %actor.coins% > %coins% || %actor.inventory% != %inv_start%
  * got one!
  %quest% %actor% trigger 1502
end
~
#1503
Legacy in Crimson: Peep and Minionize minion~
1 c 2
peep minionize~
* citizen mobs we can minionize: must have #n as first keyword
set cit_list 202 203 206 207 208 209 224 226 227 229 256 257 258 265 267 268 269 272 274 276 277 279 283 284 285
*
* shared
set counter %self.var(counter,0)%
if %counter% == 0
  eval counter 8 + %random.11%
end
set target %actor.char_target(%arg.argument1%)%
if %actor.disabled% || %actor.position% == Sleeping
  %send% %actor% You can't do that right now.
  halt
elseif !%target%
  %send% %actor% Who were you trying to use @%self% on?
  halt
elseif !%target.is_npc%
  %send% %actor% It doesn't seem to work on ~%target%.
  halt
elseif %target.empire% != %actor.empire%
  %send% %actor% You need to use it on members of your own empire.
  halt
end
*
if peep /= %cmd%
  * PEEP command
  if %actor.cooldown(1503)% > 0
    %send% %actor% @%self% isn't ready to use again yet.
  elseif !(%cit_list% ~= %target.vnum%)
    %send% %actor% You look through @%self% but don't see anything special about ~%target%.
  elseif %target.var(minion_%actor.id%)%
    %send% %actor% You look through @%self% and see a powerful aura around ~%target%... &%target% would make the perfect minion!
  elseif %target.var(peeped_%actor.id%)%
    %send% %actor% You look through @%self% but quickly realize you already checked ~%target%.
  elseif %counter% > 1
    * would be valid but we're on countdown
    %send% %actor% You look through @%self% but ~%target% doesn't have a very strong aura through it.
    set peeped_%actor.id% 1
    remote peeped_%actor.id% %target.id%
    eval counter %counter% - 1
    remote counter %self.id%
    nop %actor.set_cooldown(1503,10)%
  else
    * success
    %send% %actor% You look through @%self% and see a powerful aura around ~%target%... &%target% would make the perfect minion!
    set minion_%actor.id% 1
    remote minion_%actor.id% %target.id%
    eval counter %counter% - 1
    remote counter %self.id%
    nop %actor.set_cooldown(1503,10)%
  end
elseif minionize /= %cmd%
  * MINIONIZE command
  if %actor.has_companion(550)%
    %send% %actor% You already have a dark minion. Use 'companions' to summon it.
    %quest% %actor% trigger 1503
  elseif !(%cit_list% ~= %target.vnum%)
    %send% %actor% ~%target% would't make much of a minion.
  elseif !%target.var(minion_%actor.id%)% && !%target.var(peeped_%actor.id%)%
    %send% %actor% You haven't peeped ~%target% through the stone yet.
  elseif !%target.var(minion_%actor.id%)%
    %send% %actor% ~%target% probably wouldn't make a very good minion.
  elseif %actor.fighting% || %target.fighting%
    %send% %actor% This isn't the time!
  else
    * SUCCESS
    %send% %actor% You use the blood-red stone to transform ~%target% into your own personal minion!
    %echoaround% %actor% ~%actor% uses a blood-red stone to transform ~%target%!
    nop %actor.add_companion(550)%
    %mod% %actor% companion 550
    set minion %actor.companion%
    if %minion%
      * set up companion
      set name %target.alias.car%
      %mod% %minion% sex %target.sex%
      %mod% %minion% keywords %name% dark minion
      %mod% %minion% shortdesc Dark %name%
      %mod% %minion% longdesc %name% stands here, tapping %target.hisher% fingers.
      %mod% %minion% lookdesc %name% has a strange pallor and an odd habit about %target.himher%.
      * messaging
      %send% %actor% (Use the 'companions' command to re-summon *%target% at any time.)
      %quest% %actor% trigger 1503
      * remove citizen?
      set targ_id %target.id%
      dg_affect %target% !SEE on 5
      %own% %target% none
      * still here?
      if %target% && %target.id% == %targ_id%
        %purge% %target%
      end
    else
      * huh? minion failed
      nop %actor.remove_companion(550)%
      %send% %actor% Unknown failure in the minionize command. Please report this.
    end
    * stone is used up
    %purge% %self%
  end
end
~
#1506
Archetype unlock quests: Give quest items~
2 u 0
~
switch %questvnum%
  case 1506
    %load% obj 1001 %actor% inv
  break
  case 1502
    %load% obj 1011 %actor% inv
  break
  case 1503
    %load% obj 1012 %actor% inv
  break
done
~
$
