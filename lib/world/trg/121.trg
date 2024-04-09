#12102
Master Baker's Trials~
0 e 0
finishes cooking~
if %actor.on_quest(12102)% & %object%
  set want_vnums 3357 3358 3359 3360 3361
  if %want_vnums% ~= %object.vnum%
    * mark that we got this one
    if !%actor.var(12102_made_%object.vnum%)%
      set 12102_made_%object.vnum% 1
      remote 12102_made_%object.vnum% %actor.id%
say Ah, a perfect example of %object.shortdesc%! Nicely done.
    end
    * check if we got 'em all
    set missing 0
    set list %want_vnums%
    while %list%
      set vnum %list.car%
      set list %list.cdr%
      if !%actor.var(12102_made_%vnum%)%
        set missing 1
      end
    done
    * did we finish the quest?
    if !%missing%
      %quest% %actor% trigger 12102
      %send% %actor% You've finished all the feasts for the quest!
      * and delete the vars
      set list %want_vnums%
      while %list%
        set vnum %list.car%
        set list %list.cdr%
        rdelete 12102_made_%vnum% %actor.id%
      done
    end
  end
end
~
#12141
Learn Imperial Chef recipes from book~
1 c 2
learn~
* Usage: learn <self>
* check targeting
if %actor.obj_target(%arg%)% != %self%
  return 0
  halt
end
*
* configs
set vnum_list 12100 12101 12103 12105 12106 12108 12140
set abil_list 293 270 271 271 271 271 293
set name_list fried porkchop, mom's pecan pie, venison stew pot, chili pot, chicken pot pie, cookoff-winning gumbo, and royal feast
*
* check if the player needs any of them
set any 0
set error 0
set vnum_copy %vnum_list%
while %vnum_copy%
  * pluck first numbers out
  set vnum %vnum_copy.car%
  set vnum_copy %vnum_copy.cdr%
  set abil %abil_list.car%
  set abil_list %abil_list.cdr%
  if !%actor.learned(%vnum%)%
    if %actor.ability(%abil%)%
      set any 1
    else
      set error 1
    end
  end
done
* how'd we do?
if %error% && !%any%
  %send% %actor% You don't have the right ability to learn anything new from @%self%.
  halt
elseif !%any%
  %send% %actor% There aren't any recipes you can learn from @%self%.
  halt
end
* ok go ahead and teach them
while %vnum_list%
  * pluck first numbers out
  set vnum %vnum_list.car%
  set vnum_list %vnum_list.cdr%
  if !%actor.learned(%vnum%)%
    nop %actor.add_learned(%vnum%)%
  end
done
* and report...
%send% %actor% You study @%self% and learn: %name_list%
%echoaround% %actor% ~%actor% studies @%self%.
%purge% %self%
~
$
