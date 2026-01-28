#15900
Shipwrecked Goblins: Shared load trigger~
0 nA 100 5
L b 15900
L b 15901
L b 15902
L b 15903
L r 15900
~
set important_vnums 15900 15901 15902 15903
set traps 3
wait 0
set loc %instance.location%
if %loc%
  set start %instance.start%
  if %start% && %important_vnums% ~= %self.vnum%
    eval goblins %start.var(goblins,0)% + 1
    remote goblins %start.id%
  end
else
  * not instanced?
  nop %self.add_mob_flag(SPAWNED)%
  nop %self.remove_mob_flag(IMPORTANT)%
end
* mob-based stuff
switch %self.vnum%
  case 15900
    * scrapboss
    remote traps %self.id%
    if %loc%
      mgoto %loc%
      * load ship
      %load% veh 15900
      set num %random.20%
      if %num% == 1
        set name Brine Basher
      elseif %num% == 2
        set name Splintered Glory
      elseif %num% == 3
        set name Soggy Goblin
      elseif %num% == 4
        set name Boomtide
      elseif %num% == 5
        set name Leakin' Goblin
      elseif %num% == 6
        set name Keelcrack
      elseif %num% == 7
        set name Tidewrecker
      elseif %num% == 8
        set name Splodefish
      elseif %num% == 9
        set name Brineclaw
      elseif %num% == 10
        set name Goblin O' War
      elseif %num% == 11
        set name Boomchop
      elseif %num% == 12
        set name Grand Scuttler
      elseif %num% == 13
        set name Hullsplitter
      elseif %num% == 14
        set name Krunchwave
      elseif %num% == 15
        set name Salty Goblin
      elseif %num% == 16
        set name Stormsnout
      elseif %num% == 17
        set name Gobbernaut
      elseif %num% == 18
        set name Golden Driftwood
      elseif %num% == 19
        set name Foam Reaver
      else
        set name Brine Reaper
      end
      * rename
      set veh %loc.vehicles%
      if %veh.vnum% == 15900
        %mod% %veh% keywords shipwreck wreck salvage %name%
        %mod% %veh% shortdesc the wreck of the %name%
        %mod% %veh% longdesc The wreck of the %name% rises from the shore, half salvaged.
      end
    end
  break
  case 15901
  case 15902
  case 15903
    if %loc%
      mgoto %loc%
      mmove
      mmove
      mmove
    end
  break
done
~
#15901
Shipwrecked Goblins: Shared death trigger~
0 f 100 1
L c 15900
~
* This only belongs on goblins listed as %important_vnums% in trig 15900
set start %instance.start%
if %start%
  eval goblins %start.var(goblins,1)% - 1
  remote goblins %start.id%
  if %goblins% <= 0
    %load% obj 15900 %instance.location%
  end
end
~
#15902
Shipwrecked Goblins: Leash script~
0 i 100 15
L h 5
L h 6
L h 8
L h 30
L h 32
L h 33
L h 52
L h 57
L h 251
L h 253
L h 10190
L h 10191
L h 10192
L h 12368
L h 18450
~
*
set max_dist 6
set avoid_sects 5 6 8 30 32 33 52 251 253 10190 10191 10192 12368 18450
*
set start %instance.location%
set room %self.room%
return 1
if %method% != move || !%start%
  halt
end
* distance check
if %room.distance(%start%)% >= %max_dist%
  return 0
  halt
end
* sector check
if %avoid_sects% ~= %room.sector_vnum%
  return 0
  halt
end
~
#15903
Shipwrecked Goblins: Scrapboss build and repair~
0 ab 10 42
L e 15901
L e 15905
L h 0
L h 20
L h 21
L h 40
L h 50
L h 51
L h 56
L h 57
L h 58
L h 70
L h 71
L h 72
L h 73
L h 74
L h 75
L h 76
L h 79
L h 82
L h 200
L h 204
L h 230
L h 231
L h 234
L h 240
L h 241
L h 243
L h 244
L h 250
L h 252
L h 10301
L h 10302
L h 10303
L h 10305
L h 10306
L h 10308
L h 10311
L r 15900
L r 15901
L r 15902
L r 15903
~
* This script terraforms the area and builds defenses
set trap_sects 0 20 21 40 50 51 56 58 70 71 72 73 74 75 76 79 82 200 204 230 231 234 240 241 243 244 250 252 10301 10302 10303 10305 10306 10308 10311
set start %instance.location%
if !%start%
  halt
end
set room %self.room%
set dist %room.distance(%start%)%
if %room.empire% || %self.fighting% || %self.disabled%
  * skip
  halt
elseif %room.building_vnum% == 15901 && %room.health% < %room.maxhealth% && %room.complete%
  %restore% %room%
  %echo% ~%self% repairs the barricades.
elseif %room.sector_vnum% == 57
  %echo% ~%self% uses some of the wreckage to construct a barricade.
  %build% %room% 15901
  nop %room.add_built_with(15905,10,1)%
  nop %room.add_built_with(15903,12,1)%
  nop %room.add_built_with(15904,12,1)%
elseif %trap_sects% ~= %room.sector_vnum% && %self.var(traps,0)% > 0 && %dist% >= 4 && !%room.vehicles%
  %echo% ~%self% arranges some debris into piles.
  %build% %room% 15905
  eval traps %self.var(traps,1)% - 1
  remote traps %self.id%
else
  * look for something to fix
  set veh %room.vehicles%
  while %veh%
    if %veh.vnum% >= 15900 && %veh.vnum% <= 15910
      if %veh.health% < %veh.maxhealth% && %veh.complete%
        %echo% ~%self% repairs @%veh%.
        %restore% %veh%
      end
    end
    set veh %veh.next_in_room%
  done
  * stuck check
  if %self.var(last_room,0)% != %room.vnum%
    set last_room %room.vnum%
    set last_check %timestamp%
    remote last_room %self.id%
    remote last_check %self.id%
  elseif (%timestamp% - %self.var(last_check,0)%) > 1800
    * stuck half an hour
    set loc %instance.location%
    if %loc% && %loc% != %room%
      %echo% ~%self% lights a fuse... There's a moment of sparking and then &%self% blasts off!
      mgoto %loc%
      %echo% ~%self% comes flying in with a trail of smoke!
    end
  end
  halt
end
~
#15904
Shipwrecked Goblins: Tent setup and leash~
0 i 100 78
L f 15904
L f 15909
L h 0
L h 5
L h 6
L h 7
L h 8
L h 9
L h 12
L h 13
L h 14
L h 20
L h 21
L h 30
L h 32
L h 33
L h 50
L h 51
L h 52
L h 53
L h 56
L h 58
L h 70
L h 71
L h 73
L h 74
L h 77
L h 78
L h 83
L h 84
L h 91
L h 200
L h 202
L h 203
L h 204
L h 230
L h 231
L h 236
L h 237
L h 238
L h 239
L h 240
L h 241
L h 243
L h 244
L h 246
L h 247
L h 248
L h 249
L h 250
L h 251
L h 253
L h 601
L h 602
L h 603
L h 604
L h 606
L h 607
L h 608
L h 611
L h 612
L h 613
L h 614
L h 615
L h 616
L h 617
L h 618
L h 621
L h 622
L h 623
L h 10190
L h 10191
L h 10192
L h 12368
L h 18450
L r 15901
L r 15902
L r 15903
~
* this builds tents for each mob in a specific range, mob vnums 15901 15902 15903
set tent_sects 0 7 9 12 13 14 20 21 50 51 53 56 58 70 71 73 74 77 78 83 84 91 200 202 203 204 230 231 236 237 238 239 240 241 243 244 246 247 248 249 250 601 602 603 604 606 607 608 611 612 613 614 615 616 617 618 621 622 623
set avoid_sects 5 6 8 30 32 33 52 251 253 10190 10191 10192 12368 18450
eval range %self.vnum% - 15901 + 2
return 1
*
set room %self.room%
set loc %instance.location%
if !%loc%
  return 0
  halt
end
set dist %room.distance(%loc%)%
if %avoid_sects% ~= %room.sector_vnum%
  return 0
  halt
elseif %dist% >= %range% && %dist% < %range% + 1
  if %tent_sects% ~= %room.sector_vnum% && !%room.empire%
    * tent here!
    nop %self.add_mob_flag(SENTINEL)%
    wait 2 s
    %echo% ~%self% sets up ^%self% tent.
    %load% veh %self.vnum%
    wait 1 s
    enter tent
    detach 15904 %self.id%
    detach 15909 %self.id%
  end
elseif %dist% > %range%
  return 0
  halt
end
~
#15905
Shipwrecked Goblins: Random Namer~
0 nA 100 13
L b 15900
L b 15901
L b 15902
L b 15903
L b 15904
L b 15905
L b 15906
L b 15907
L b 15908
L b 15909
L b 15910
L b 15911
L j 15900
~
* Namelists: Both 20 names in length, update size if you change this
set male_names Zalkaba Greenchops Gobtholomew Gidd Gobstede Goblarossa Balben Rek Rekker Brokka Elvo Goona Seascar Stabman Shivnow Bitesal Fyunk Diyed Axer Bilj Rotbelleh Grimspit Bonegryn Bonepocket Charmspit Grubsnuff Murcant Glummire Pokeskull
set female_names Merkilda Nilbagga Thilxa Bongy Reeda Gobshih Killigobba Myain Pimba Crunchbones Rattlebones Steala Drooner Vence Bitta Bythe Onner Nelbog Meener Warze Rotlik Moldthumb Wormbitar Stitchgut Sewrot Eetchfang Peeleye Moonspork Hakbone
set size 30
*
* Exit early on resurrect
if %self.room.template% != 15900
  halt
end
*
* Pick name
set start %instance.start%
if %start%
  set found 0
  set used_names %start.var(used_names,0)%
  while !%found%
    eval pos %%random.%size%%%
    if !(%used_names% ~= %pos%)
      set found 1
      set used_names %used_names% %pos%
      remote used_names %start.id%
    end
  done
else
  * no start
  set found 1
  eval pos %%random.%size%%%
end
if %self.sex% == female
  set list %female_names%
else
  set list %male_names%
end
while %pos% > 0 && %found%
  set name %list.car%
  set list %list.cdr%
  eval pos %pos% - 1
done
if !%name%
  * failure to find a name means it's ok to use the one they came with
  halt
end
* mob type
switch %self.vnum%
  case 15900
    %mod% %self% keywords %name% goblin scrapboss boss
    %mod% %self% shortdesc Scrapboss %name%
    * The goblin scrapboss is dragging a bag of nails.
  break
  case 15901
    %mod% %self% keywords %name% goblin wavewhisperer whisperer wave-whisperer
    %mod% %self% shortdesc Wavewhisperer %name%
    %mod% %self% longdesc Wavewhisperer %name% stares serenely off into the distance.
  break
  case 15902
    %mod% %self% keywords %name% goblin crabherd crab-herd herd
    %mod% %self% shortdesc %name% the crabherd
    %mod% %self% longdesc %name% is looking for one of the crabs.
  break
  case 15903
    %mod% %self% keywords %name% goblin raidscribe raid-scribe scribe
    %mod% %self% shortdesc Raidscribe %name%
    %mod% %self% longdesc Raidscribe %name% is carving notches into a long pole.
  break
  case 15904
    %mod% %self% keywords %name% goblin ropesnarl rope-snarl snarl
    %mod% %self% shortdesc Ropesnarl %name%
    * A goblin ropesnarl is twirling a net.
  break
  case 15905
    %mod% %self% keywords %name% goblin bonecounter bone-counter counter
    %mod% %self% shortdesc Bonecounter %name%
    * A goblin bonecounter is eyeing you up and down.
  break
  case 15906
    %mod% %self% keywords %name% goblin shellpicker shell-picker picker
    %mod% %self% shortdesc Shellpicker %name%
    * A goblin shellpicker is gleaming in the light.
  break
  case 15907
    %mod% %self% keywords %name% goblin lootcaller loot-caller caller
    %mod% %self% shortdesc Lootcaller %name%
    * A goblin lootcaller is barking out orders.
  break
  case 15908
    %mod% %self% keywords %name% goblin banner-lugger bannerlugger lugger
    %mod% %self% shortdesc Banner-lugger %name%
    * A goblin banner-lugger is standing here.
  break
  case 15909
    %mod% %self% keywords %name% goblin flame-skulker flameskulker skulker
    %mod% %self% shortdesc Flame-skulker %name%
    * A goblin flame-skulker is lurking nearby.
  break
  case 15910
    %mod% %self% keywords %name% goblin tarflinger tar-flinger flinger
    %mod% %self% shortdesc Tarflinger %name%
    * A goblin tarflinger is holding a bucket.
  break
  case 15911
    %mod% %self% keywords %name% goblin deadwright dead-wright wright
    %mod% %self% shortdesc Deadwright %name%
    * A goblin deadwright seems to be chanting something.
  break
done
~
#15906
Shipwrecked Goblins: Another trap~
1 n 100 1
L b 15900
~
* Restores a trap to the scrapboss, if possible
set mob %instance.mob(15900)%
if %mob%
  eval traps %mob.var(traps,0)% + 1
  remote traps %mob.id%
end
%purge% %self%
~
#15907
Shipwrecked Goblins: Trap go boom~
2 g 100 2
L c 15901
L c 15902
~
if %actor.is_npc%
  halt
elseif %actor.aff_flagged(FLYING)%
  halt
end
wait 0
%send% %actor% You trip over some of the debris... there's a strange clicking noise.
%echoaround% %actor% ~%actor% trips over some debris... there's a strange clicking noise.
wait 1 s
%load% obj 15902
set trap %room.contents%
if %trap.vnum% == 15902
  %scale% %trap% %actor.level%
end
* and reset
set start %instance.nearest_rmt(15900)%
if %room.distance(%start%)% < 10
  %load% obj 15901 %start%
end
~
#15908
Shipwrecked Goblins: Trap damage~
1 n 100 0
~
wait 0
%echo% &&r**** The debris piles EXPLODE! ****&&0
%aoe% 500 physical
%build% %self.room% demolish
%purge% %self%
~
#15909
Shipwrecked Goblins: Choppin' trees~
0 ab 10 46
L h 0
L h 1
L h 2
L h 3
L h 4
L h 9
L h 20
L h 21
L h 23
L h 24
L h 26
L h 36
L h 37
L h 38
L h 39
L h 40
L h 44
L h 45
L h 46
L h 47
L h 50
L h 54
L h 59
L h 80
L h 81
L h 88
L h 89
L h 90
L h 200
L h 204
L h 210
L h 211
L h 212
L h 220
L h 221
L h 222
L h 223
L h 224
L h 231
L h 232
L h 233
L h 10562
L h 10563
L h 10564
L h 10565
L h 10566
~
Commands:
* This script terraforms forests to flat land
* forests that terraform down 1 vnum:
set easy_forest 24 37 38 45 47 81 89 212 10563 10564 10565
*
set room %self.room%
set start %instance.location%
if !%start% || %room.empire% || %self.fighting% || %self.disabled%
  halt
end
set dist %room.distance(%start%)%
set sect %room.sector_vnum%
* some forests chop down 1 vnum
if %easy_forest% ~= %sect%
  %echo% ~%self% chops down a tree.
  eval vnum %sect% - 1
  %terraform% %room% %vnum%
  halt
end
* otherwise chop/burn by vnum:
switch %sect%
  case 1
  case 39
  case 10562
    %echo% ~%self% chops down the last tree.
    %terraform% %room% 0
  break
  case 2
    %echo% ~%self% chops the last trees.
    %terraform% %room% 0
  break
  case 3
    %echo% ~%self% chops trees.
    %terraform% %room% 2
  break
  case 4
    %echo% ~%self% chops some last trees.
    %terraform% %room% 2
  break
  case 9
    %echo% ~%self% pulls up the road.
    %terraform% %room% %room.base_sector_vnum%
  break
  case 23
  case 211
  case 222
    %echo% ~%self% burns the stumps.
    %terraform% %room% 20
  break
  case 26
    %echo% ~%self% chops down the last tree.
    %terraform% %room% 20
  break
  case 36
  case 10566
    %echo% ~%self% burns the stumps.
    %terraform% %room% 0
  break
  case 44
    %echo% ~%self% chops down the last tree.
    %terraform% %room% 40
  break
  case 46
    %echo% ~%self% burns the stumps.
    %terraform% %room% 40
  break
  case 54
    %echo% ~%self% chops down the last tree.
    %terraform% %room% 50
  break
  case 59
    %echo% ~%self% burns the stumps.
    %terraform% %room% 50
  break
  case 80
    %echo% ~%self% chops down the last tree.
    %terraform% %room% 21
  break
  case 90
    %echo% ~%self% chops down a massive tree.
    %terraform% %room% 4
    %load% obj 137 %room%
  break
  case 210
    %echo% ~%self% chops down a tree.
    %terraform% %room% 200
  break
  case 220
    %echo% ~%self% chops down a tree.
    %terraform% %room% 221
  break
  case 221
  case 223
  case 224
    %echo% ~%self% chops down a tree.
    %terraform% %room% 204
  break
  case 232
  case 233
    %echo% ~%self% chops down a tree.
    %terraform% %room% 231
  break
end
~
#15910
Shipwrecked Goblins: Start progress goal~
0 h 100 1
L y 15900
~
if %actor.is_pc% && %actor.empire%
  nop %actor.empire.start_progress(15900)%
end
~
#15911
Shipwrecked Goblins: Deadwright resurrection~
0 b 33 8
L b 15904
L b 15905
L b 15906
L b 15907
L b 15908
L b 15909
L b 15910
L b 15911
~
* find a dead goblin
if %self.disabled%
  halt
end
set obj %self.room.contents%
set found 0
while %obj% && !%found%
  if %obj.type% == CORPSE && %obj.val0% >= 15904 && %obj.val0% <= 15911
    set found %obj%
  end
  set obj %obj.next_in_list%
done
* did we find one?
if !%found%
  halt
end
* start
%echo% A purple glow surrounds ~%self% as *%self% begins a throaty chant...
nop %self.add_mob_flag(SENTINEL)%
set id %found.id%
set cycle 0
while %cycle% <= 4
  wait 3 s
  switch %cycle%
    case 0
      say By the Godlins strong and mighty...
    break
    case 1
      say Neath the earth we work and render...
    break
    case 2
      say Fill this green one with your lighty...
    break
    case 3
      say Rise again in goblin splendor!
    break
    case 4
      if %found% && %found.id% == %id%
        %load% mob %found.val0%
        set mob %self.room.people%
        if %mob.vnum% == %found.val0%
          nop %mob.add_mob_flag(!LOOT)%
          %echo% A rich a purple glow emanates from ~%mob% as &%mob% is resurrected!
          %mod% %mob% append-lookdesc There's a dull purple glow in %mob.hisher% eyes and a bit of drool on the chin.
        else
          say Oops... didn't work.
        end
        %purge% %found%
      else
        say Oops... didn't work.
      end
    break
  done
  eval cycle %cycle% + 1
done
nop %self.remove_mob_flag(SENTINEL)%
~
#15912
Shipwrecked Goblins: Learn a random hat~
1 c 2 6
L a 15908
L a 15909
L a 15910
L a 15911
L a 15919
L o 54
learn~
set recipes 15908 15909 15910 15911 15919
set count 5
*
if %actor.obj_target(%arg.argument1%)% != %self%
  return 0
  halt
elseif !%actor.ability(54)%
  %send% %actor% You require the Jewelry ability to learn these patterns.
  halt
end
* pick at random until we have one the player doesn't know
set list %recipes%
set tot %count%
set found 0
while !%found% && %tot% > 0 && %list%
  eval pos %%random.%tot%%%
  * find pos in list
  set new_list
  set cur 0
  while %pos% > 0
    set cur %list.car%
    set list %list.cdr%
    if %pos% > 1
      set new_list %new_list% %cur%
    else
      set new_list %new_list% %list%
    end
    eval pos %pos% - 1
  done
  if %cur% && !%actor.learned(%cur%)%
    * found 1 to learn
    set found %cur%
  else
    * just keep trying
    eval tot %tot% - 1
    set list %new_list%
  end
done
* did we find one to learn?
if %found%
  nop %actor.add_learned(%found%)%
  %send% %actor% You study @%self% and learn to make %_obj.name(%found%)%!
  %echoaround% %actor% ~%actor% studies @%self% and learns a new craft!
  %purge% %self%
else
  %send% %actor% There's nothing left for you to learn from @%self%.
end
~
#15915
Shipwrecked Goblins: Pay to leave~
2 v 0 3
L i 15900
L t 15915
L y 15901
~
if %questvnum% == 15915
  %adventurecomplete%
  * also shut off aggro flags
  set vnum 15904
  while %vnum% <= 15910
    set mob %instance.mob(%vnum%)%
    if %mob%
      nop %mob.remove_mob_flag(AGGR)%
    end
    eval vnum %vnum% + 1
  done
  * and pay-off progress goal if possible
  if %actor%
    set emp %actor.empire%
    if %emp%
      nop %emp.add_progress(15901)%
    end
  end
end
~
#15916
Shipwrecked Goblins: Mob quest finish speech~
0 v 0 5
L b 15901
L b 15902
L b 15903
L t 15916
L t 15917
~
if %questvnum% == 15916
  * goblin trinkets
  wait 1
  switch %self.vnum%
    case 15901
      * wavewhisperer
      say This a big old treasure.
      wait 2 s
      say Where you get this? Grand greatmother had one like this.
      wait 2 s
      say Not be thinking any of these were left.
      wait 2 s
      say You human seem all right.
    break
    case 15902
      * crabherd
      say Not even believing you have these!
      wait 2 s
      say Elders going to be real surprised.
      wait 2 s
      say Good surprise this time, not like when we landed.
    break
    case 15903
      * raidscribe
      say I had some like this as a little boy.
      wait 2 s
      say I'm sure you only had these trinkets because you looted them from one of our ruins.
      wait 2 s
      say Nevertheless, that you'd return them to us. You have my thanks.
    break
  done
elseif %questvnum% == 15917
  * jade tear
  wait 1
  switch %self.vnum%
    case 15901
      * wavewhisperer
      say Never before seen one like this.
      wait 2 s
      say Where you get this? Nobody has this!
      wait 2 s
      say You must be some kind of seeker to have this.
    break
    case 15902
      * crabherd
      say Jade!
      wait 2 s
      say Not expecting to find any jade.
      wait 2 s
      say Everything's coming up crabherd!
    break
    case 15903
      * raidscribe
      say Oh, my.
      wait 2 s
      say Oh, my, indeed. This is extraordinary.
      wait 2 s
      say You can almost taste the ancestors in this rough work. Oh, my.
      wait 2 s
      say You've made a rare friend here, human.
    break
  done
end
~
#15922
Shipwrecked Goblins: Buy haberdasher services~
1 n 100 2
L b 15922
L f 15923
~
set clothes_list 15923 15924 15925 15926 15927 15928 15929 15930 15931 15932 15933 15934
wait 1
set actor %self.carried_by%
if !%actor%
  %purge% %self%
  halt
end
* check if we already have one?
if %actor.has_companion(15922)%
  %send% %actor% You already have a goblin haberdasher companion; buying another won't help.
  %send% %actor% The wavewhisper shouts, 'No refunds!'
  %purge% %self%
  halt
end
* add haberdasher
nop %actor.add_companion(15922)%
%mod% %actor% companion 15922
%send% %actor% Your haberdasher arrives. He is now your companion and may be re-summoned later with the companion command.
%echoaround% %actor% |%actor% haberdasher arrives.
set mob %self.room.people%
if %mob.vnum% == 15922 && %mob.leader% == %actor%
  remote clothes_list %mob.id%
  %force% %mob% !haberdash
end
%purge% %self%
~
#15923
Shipwrecked Goblins: Haberdash command~
0 ct 0 13
L c 15923
L c 15924
L c 15925
L c 15926
L c 15927
L c 15928
L c 15929
L c 15930
L c 15931
L c 15932
L c 15933
L c 15934
L c 15935
!haberdash~
set ch %actor.companion%
set clothes_list %self.var(clothes_list)%
if %actor% != %self%
  * not me
  return 0
  halt
elseif !%ch% || %ch.is_npc% || %ch.room% != %self.room%
  * gone
  halt
elseif %ch.carrying% >= %ch.maxcarrying%
  * full inventory
  halt
elseif !%clothes_list%
  * done!
  say Done with this job. But one last outfit for my best customer...
  %load% obj 15935 %ch% inv
  set obj %ch.inventory%
  if %obj.vnum% == 15935
    %echo% ~%actor% gives ~%ch% @%obj%.
  end
  nop %ch.remove_companion(%self.vnum%)%
  %purge% %self%
  halt
elseif %timestamp% - %self.var(last_time,0)% < 43200
  * too soon
  halt
end
* count remaining
set temp %clothes_list%
set count 0
while %temp%
  set temp %temp.cdr%
  eval count %count% + 1
done
* random item
set temp %clothes_list%
eval pos %%random.%count%%%
set clothes_list
set found 0
while %pos% > 0
  set this %temp.car%
  set temp %temp.cdr%
  if %pos% == 1
    set found %this%
  else
    set clothes_list %clothes_list% %this%
  end
  eval pos %pos% - 1
done
set clothes_list %clothes_list% %temp%
remote clothes_list %self.id%
* ok?
if !%found%
  say Having trouble making new clothes.
  halt
end
* ok!
%load% obj %found% %ch% inv
set obj %ch.inventory%
if %obj.vnum% == %found%
  * success!
  %send% %ch% ~%self% finishes making @%obj% and gives them to you!
  %echoaround% %ch% ~%self% finishes making @%obj% and gives them to ~%ch%.
  set last_time %timestamp%
  remote last_time %self.id%
end
~
#15924
Shipwrecked Goblins: Haberdasher random check~
0 bt 5 1
L f 15923
~
* Periodically tries to haberdash
!haberdash
~
#15925
Shipwrecked Goblins: Haberdasher fight and flight~
0 kt 100 0
~
wait 1
%echo% ~%self% flees for ^%self% life!
%purge% %self%
~
$
