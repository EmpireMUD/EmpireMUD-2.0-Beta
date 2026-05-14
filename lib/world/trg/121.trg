#12102
Master Baker's Trials~
0 e 0 6
L c 3357
L c 3358
L c 3359
L c 3360
L c 3361
L t 12102
finishes cooking~
if !%actor.on_quest(12102)% || !%object%
  halt
end
set want_vnums 3357 3358 3359 3360 3361
set num_wanted 5
if %want_vnums% ~= %object.vnum%
  * mark that we got this one
  if !%actor.var(12102_made_%object.vnum%)%
    set 12102_made_%object.vnum% 1
    remote 12102_made_%object.vnum% %actor.id%
    say Ah, a perfect example of %object.shortdesc%! Nicely done.
    * triggers once per feast
    %quest% %actor% trigger 12102
  end
  * check if we got 'em all
  set missing 0
  set list %want_vnums%
  while %list%
    set vnum %list.car%
    set list %list.cdr%
    if !%actor.var(12102_made_%vnum%)%
      eval missing %missing% + 1
    end
  done
  * double-check trigger count
  while %actor.quest_triggered(12102)% < (%num_wanted% - %missing%)
    %quest% %actor% trigger 12102
  done
  while %actor.quest_triggered(12102)% > (%num_wanted% - %missing%)
    %quest% %actor% untrigger 12102
  done
  * did we finish the quest?
  if %actor.quest_triggered(12102)% >= %num_wanted%
    %send% %actor% You've finished all the feasts for the quest!
    * and delete the vars
    set list %want_vnums%
    while %list%
      set vnum %list.car%
      set list %list.cdr%
      rdelete 12102_made_%vnum% %actor.id%
    done
    wait 1
    %quest% %actor% finish 12102
  end
end
~
#12103
Divinely Tasty: Require milk then teach molds~
2 v 0 1
L a 12142
~
* script to check for milk
set milk_vnum 5
set milk_needed 6
*
set obj %actor.inventory%
set found 0
set low 0
while %obj% && !%found%
  if %obj.type% == DRINKCON && %obj.val2% == %milk_vnum%
    if %obj.val1% >= %milk_needed%
      set found %obj%
    else
      set low 1
    end
  end
  set obj %obj.next_in_list%
done
* ok?
if !%found%
  if %low%
    %send% %actor% You don't have enough milk to complete Divinely Tasty.
  else
    %send% %actor% You need some milk to complete the Divinely Tasty quest.
  end
  * block
  return 0
  halt
end
* otherwise, we are ok
eval amt %found.val1% - %milk_needed%
nop %found.val1(%amt%)%
%send% %actor% You hand over some milk...
nop %actor.add_learned(12142)%
return 1
~
#12141
Learn Imperial Chef recipes from book~
1 c 2 10
L a 12100
L a 12101
L a 12103
L a 12105
L a 12106
L a 12108
L a 12140
L o 270
L o 271
L o 293
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
#12142
DEPRECATED set of wooden molds~
1 c 6 2
L c 12129
L c 12131
mold~
* this script is deprecated and replaced by 12144
*
halt
*
set target %arg.car%
set shape %arg.cdr%
set item %actor.obj_target(%target%)%
if !%item%
  %send% %actor% You don't seem to have a '%target%'.
  halt
end
if %item.vnum% != 12129 && %item.vnum% != 12131
  %send% %actor% You can't mold @%item%.
  halt
end
makeuid empire empire %shape%
if %empire%
  set empire_name %empire.name%
else
end
if %shape% == dove
  %mod% %item% shortdesc a chocolate dove
  %mod% %item% keywords chocolate dove
  %mod% %item% longdesc A chocolate dove has been dropped here.
  %mod% %item% lookdesc A solid piece of chocolate has been shaped in the likeness of a dove.
elseif %shape% == bunny
  %mod% %item% keywords chocolate bunny
  %mod% %item% shortdesc a chocolate bunny
  %mod% %item% longdesc An entire chocolate bunny has been dropped here.
  %mod% %item% lookdesc A solid piece of chocolate has been molded into the likeness of a bunny.
elseif %shape% == fish
  %mod% %item% keywords chocolate fish
  %mod% %item% shortdesc a chocolate fish
  %mod% %item% longdesc A chocolate fish lies here.
  %mod% %item% lookdesc Solid chocolate has been molded into the shape of a fish.
elseif %shape% == wolf
  %mod% %item% keywords chocolate wolf
  %mod% %item% shortdesc a chocolate wolf
  %mod% %item% longdesc A chocolate wolf is lying here, tongue lolling and tail curved as if in mid wag.
  %mod% %item% lookdesc This chocolate wolf has been molded from a solid block of chocolate to look like it's been playing and is happily exhausted.
elseif %shape% == bear
  %mod% %item% keywords chocolate bear
  %mod% %item% shortdesc a chocolate bear
  %mod% %item% longdesc A chocolate bear has been left here.
  %mod% %item% lookdesc A solid chocolate bear is formed to look as if it's snarling and daring you to eat it.
elseif %shape% == deer
  %mod% %item% keywords chocolate deer
  %mod% %item% shortdesc a chocolate deer
  %mod% %item% longdesc A chocolate deer has been left here.
  %mod% %item% lookdesc A chocolate deer has been molded to look like it's grazing on some leaves.
elseif %shape% == sheep
  %mod% %item% keywords chocolate sheep
  %mod% %item% shortdesc a chocolate sheep
  %mod% %item% longdesc A chocolate sheep is lying here.
  %mod% %item% lookdesc A solid piece of chocolate has been molded to look like a sheep.
elseif %shape% == goat
  %mod% %item% keywords chocolate goat
  %mod% %item% shortdesc a chocolate goat
  %mod% %item% longdesc A chocolate goat has been dropped here.
  %mod% %item% lookdesc A solid piece of chocolate has been molded to resemble a goat.
elseif %shape% == fox
  %mod% %item% shortdesc a chocolate fox
  %mod% %item% keywords chocolate fox
  %mod% %item% longdesc A chocolate fox has been dropped here.
  %mod% %item% lookdesc A block of chocolate has been molded to look like a fox.
elseif %shape% == bull
  %mod% %item% keywords chocolate bull
  %mod% %item% shortdesc a chocolate bull
  %mod% %item% longdesc A chocolate bull looks ready to charge.
  %mod% %item% lookdesc This bull has been molded from a solid piece of chocolate, its long, curling horn thrust out inviting a delicious chomp.
elseif %shape% == jaguar
  %mod% %item% shortdesc a chocolate jaguar
  %mod% %item% keywords chocolate jaguar
  %mod% %item% longdesc A chocolate jaguar looks poised to pounce.
  %mod% %item% lookdesc This block of chocolate has been molded to resemble a jaguar in mid pounce, claws out and ready to shred flesh.
elseif %shape% == badger
  %mod% %item% shortdesc a chocolate badger
  %mod% %item% keywords chocolate badger
  %mod% %item% longdesc A chocolate badger looks to be industriously digging.
  %mod% %item% lookdesc A block of chocolate has been molded to resemble a digging badger.
elseif %shape% == sorcerer
  %mod% %item% shortdesc a chocolate sorcerer
  %mod% %item% keywords chocolate sorcerer
  %mod% %item% longdesc A chocolate sorcerer stands here leaning on his staff.
  %mod% %item% lookdesc A block of chocolate has been molded to resemble a robed sorcerer bearing a contemplative expression and leaning on a stout staff.
elseif %shape% == longship
  %mod% %item% shortdesc a chocolate longship
  %mod% %item% keywords chocolate longship
  %mod% %item% longdesc A chocolate longship is here.
  %mod% %item% lookdesc This block of chocolate is molded to look like a ship of war at full sail, its cannon raised to fire.
elseif %shape% == hulk
  %mod% %item% shortdesc a chocolate hulk
  %mod% %item% keywords chocolate hulk
  %mod% %item% longdesc A chocolate hulk is here.
  %mod% %item% lookdesc This block of chocolate resembles a hulk at full sail, with a hint of secured parcels and crates visible in its hold if you look through the top.
elseif %shape% == anaconda
  %mod% %item% shortdesc a chocolate anaconda
  %mod% %item% keywords chocolate anaconda
  %mod% %item% longdesc A chocolate anaconda is curled here.
  %mod% %item% lookdesc Molded to appear to be constricting around something, this block of chocolate now resembles an anaconda with enough texture for scales.
elseif %empire%
  %mod% %item% shortdesc a chocolate map of %empire_name%
  %mod% %item% keywords map chocolate %empire_name%
  %mod% %item% longdesc A chocolate map of %empire_name% lies here.
  %mod% %item% lookdesc An intricate chocolate map of %empire_name% has been molded from this solid block of chocolate, with key locations and terrain features marked.
  set shape map of %empire_name%
else
  %send% %actor% You don't have a mold for that.
  halt
end
if !%item.is_flagged(NO-BASIC-STORAGE)%
  nop %item.flag(NO-BASIC-STORAGE)%
end
%send% %actor% You pour the chocolate into the mold and retrieve %shape.ana% %shape% when it cools.
%echoaround% %actor% ~%actor% molds %shape.ana% %shape%.
~
#12143
DEPRECATED set of halloween-themed wooden molds~
1 c 6 2
L c 12129
L c 12131
mold~
* this script is deprecated and replaced by 12144
*
halt
*
set target %arg.car%
set shape %arg.cdr%
set item %actor.obj_target(%target%)%
if !%item%
  %send% %actor% You don't seem to have a '%target%'.
  halt
end
if %item.vnum% != 12129 && %item.vnum% != 12131
  %send% %actor% You can't mold @%item%.
  halt
end
makeuid empire empire %shape%
if %empire%
  set empire_name %empire.name%
end
if %shape% == witch's hat || %shape% == witchs hat || %shape% == hat
  set shape witch's hat
  %mod% %item% shortdesc a chocolate witch's hat
  %mod% %item% keywords chocolate hat witchs witch's
  %mod% %item% longdesc A chocolate witch's hat stands here.
  %mod% %item% lookdesc A block of chocolate has been shaped into a witch's hat, held open as if it contained someone's head.
elseif %shape% == ghost
  %mod% %item% keywords chocolate ghost hollow
  %mod% %item% shortdesc a chocolate ghost
  %mod% %item% longdesc A chocolate ghost is here.
  %mod% %item% lookdesc A chocolate block has been hollowed out into the shape of a ghost.
elseif %shape% == banshee
  %mod% %item% keywords chocolate banshee
  %mod% %item% shortdesc a chocolate banshee
  %mod% %item% longdesc A chocolate banshee lies here.
  %mod% %item% lookdesc A block of chocolate has been hollowed out into the shape of a banshee with mouth open as if screaming.
elseif %shape% == werewolf
  %mod% %item% keywords chocolate werewolf
  %mod% %item% shortdesc a chocolate werewolf
  %mod% %item% longdesc A chocolate wolf is half transformed here, neither fully human nor wolf.
  %mod% %item% lookdesc This solid block of chocolate is molded into a muscular werewolf, caught between human and wolf form.
elseif %shape% == pumpkin
  %mod% %item% keywords chocolate pumpkin
  %mod% %item% shortdesc a chocolate pumpkin
  %mod% %item% longdesc A chocolate pumpkin has been left here.
  %mod% %item% lookdesc A hollow chocolate pumpkin has been molded as if someone is just starting to carve it, with only the barest suggestion of a distorted face on one side.
elseif %shape% == skull
  %mod% %item% keywords chocolate skull
  %mod% %item% shortdesc a chocolate skull
  %mod% %item% longdesc A chocolate skull is lying here.
  %mod% %item% lookdesc A block of chocolate has been hollowed out and molded into the shape of a skull.
elseif %shape% == cat
  %mod% %item% keywords chocolate cat
  %mod% %item% shortdesc a chocolate cat
  %mod% %item% longdesc A chocolate cat has been dropped here.
  %mod% %item% lookdesc A block of chocolate has been hollowed out and molded into the shape of a cat.
elseif %shape% == spider
  %mod% %item% keywords chocolate spider
  %mod% %item% shortdesc a chocolate spider
  %mod% %item% longdesc A chocolate spider has been dropped here, its delicate web spread around it.
  %mod% %item% lookdesc A block of chocolate has been hollowed out into the shape of a spider with delicate webbing around it.
elseif %shape% == broom
  %mod% %item% keywords chocolate broom
  %mod% %item% shortdesc a chocolate broom
  %mod% %item% longdesc A chocolate broom is balanced on its bristles.
  %mod% %item% lookdesc This block of chocolate is molded to resemble a broom, its delicate bristles fluttering slightly as if it's flying.
elseif %shape% == demon
  %mod% %item% keywords chocolate demon
  %mod% %item% shortdesc a chocolate demon
  %mod% %item% longdesc A chocolate demon has its claws stretched to full length.
  %mod% %item% lookdesc This block of chocolate has been molded to resemble a horned demon stalking toward the viewer with claws out and ready to shred flesh.
elseif %shape% == scarecrow
  %mod% %item% keywords chocolate scarecrow
  %mod% %item% shortdesc a chocolate scarecrow
  %mod% %item% longdesc A chocolate scarecrow stands here on a chocolate stick.
  %mod% %item% lookdesc A block of chocolate has been hollowed out and molded into the shape of a scarecrow.
elseif %shape% == skeleton
  %mod% %item% keywords chocolate skeleton
  %mod% %item% shortdesc a chocolate skeleton
  %mod% %item% longdesc A chocolate skeleton stands here loosely.
  %mod% %item% lookdesc A block of chocolate has been hollowed into the form of a skeleton, the joints looking loose and easy to snap.
elseif %shape% == necromancer
  %mod% %item% keywords chocolate necromancer
  %mod% %item% shortdesc a chocolate necromancer
  %mod% %item% longdesc A chocolate necromancer stands here drawing runes.
  %mod% %item% lookdesc A block of chocolate has been hollowed to resemble a thin, nearly skeletal necromancer with tiny runes at his feet showing the beginnings of a summoning.
elseif %empire%
  %mod% %item% keywords memorial chocolate %empire_name%
  %mod% %item% shortdesc a chocolate memorial to %empire_name%
  %mod% %item% longdesc A chocolate memorial to %empire_name% lies here.
  %mod% %item% lookdesc An intricate map of %empire_name% has been molded from this solid block of chocolate, with locations of key cities marked by tombstones indicating mass death.
  set shape memorial to %empire_name%
else
  %send% %actor% You don't have a mold for that.
  halt
end
if !%item.is_flagged(NO-BASIC-STORAGE)%
  nop %item.flag(NO-BASIC-STORAGE)%
end
%send% %actor% You pour the chocolate into the mold and hear an eerie cackle in the air before %shape.ana% %shape% emerges, seemingly of its own accord.
%echoaround% %actor% ~%actor% pours some chocolate and %shape.ana% %shape% seems to pop briskly out of the mold all by itself.
~
#12144
Chocolate Molds reusable script~
1 c 6 2
L c 12129
L c 12131
mold~
* Handles molding various chocolates.
* This item should may have unlimited sets of 5 custom messages in script1. For
* each set of 5 messages, they should be in this order: shape name, short
* description, keywords, long description, look description.
*
* Additionally, if this mold supports an empire shape, script2 should have
* exactly 5 custom messages in this order: shape name, short description,
* keywords, long description, look description. The string %empire_name% will
* also be replaced with the actual name of the empire.
*
* You can also specify the messages shown when the player molds chocolate. For
* this, script3 may have exactly 2 entries. First, the message shown to the
* player; and second, the message shown to the room. You can use normal script
* variables like %actor% and %shape% in these messages.
*
* Config vnums of chocolate that can be molded
set chocolate_vnums 12129 12131
* Basic targeting
set target %arg.car%
set shape %arg.cdr%
set item %actor.obj_target(%target%)%
if !%arg%
  %send% %actor% Mold what into what?
  halt
elseif !%item%
  %send% %actor% You don't seem to have a '%target%'.
  halt
elseif !(%chocolate_vnums% ~= %item.vnum%)
  %send% %actor% You can't mold @%item%.
  halt
elseif !%shape
  %send% %actor% Mold it into what?
  halt
end
*
* Check for empire
makeuid empire empire %shape%
if %empire%
  set empire_name %empire.name%
end
*
* Check shapes on mold object
set found -1
set pos 0
set match 1
while %found% < 0 && %match%
  set match %self.custom(script1,%pos%)%
  if %match% && %shape% == %match%
    set found %pos%
  end
  eval pos %pos% + 5
done
* verify and error
if %found% < 0 && !%empire%
  * only send error if we are the LAST mold in the list
  set any 0
  set obj %self.next_in_list%
  while %obj% && !%any%
    if %obj.vnum% != %self.vnum% && %obj.has_trigger(12144)%
      set any 1
    else
      set obj %obj.next_in_list%
    end
  done
  if !%any%
    %send% %actor% You don't have a mold for that.
    return 1
  else
    return 0
  end
  halt
end
* prepare strings
if %found% < 0
  * empire
  set shape %self.custom(script2,0).process%
  set shortstr %self.custom(script2,1).process%
  set keystr %self.custom(script2,2).process%
  set longstr %self.custom(script2,3).process%
  set lookstr %self.custom(script2,4).process%
else
  * shape
  eval shortpos %found% + 1
  eval keypos %found% + 2
  eval longpos %found% + 3
  eval lookpos %found% + 4
  set shortstr %self.custom(script1,%shortpos%)%
  set keystr %self.custom(script1,%keypos%)%
  set longstr %self.custom(script1,%longpos%)%
  set lookstr %self.custom(script1,%lookpos%)%
end
* one last check
if !%shortstr% || !%keystr% || !%longstr% || !%lookstr%
  %send% %actor% You don't have a working mold for that.
  halt
end
* and apply
%mod% %item% shortdesc %shortstr%
%mod% %item% keywords %keystr%
%mod% %item% longdesc %longstr%
%mod% %item% lookdesc %lookstr%
if !%item.is_flagged(NO-BASIC-STORAGE)%
  nop %item.flag(NO-BASIC-STORAGE)%
end
* messaging
set to_char %self.custom(script3,0)%
set to_room %self.custom(script3,1)%
if %to_char%
  %send% %actor% %to_char.process%
else
  %send% %actor% You pour the chocolate into the mold and retrieve %shape.ana% %shape% when it cools.
end
if %to_room%
  %echoaround% %actor% %to_room.process%
else
  %echoaround% %actor% ~%actor% molds %shape.ana% %shape%.
end
~
$
