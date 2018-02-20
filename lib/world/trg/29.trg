#2914
changeling change~
0 btw 10
~
if !%self.master%
  halt
end
set start_vnum 2910
set end_vnum 2914
eval total (%end_vnum% - %start_vnum%) + 1
eval rand %%random.%total%%%
eval select %start_vnum% - 1 + %rand%
set prev_name %self.name%
set chance %random.10%
if %chance% == 10
  set master %self.master%
  if !%master.fighting% && !%master.morph%
    eval select %select% - 10
    set prev_name %master.name%
    %morph% %master% %select%
    %echoaround% %master% %self.name% morphs %prev_name% into %master.name%!
    %send% %master% %self.name% morphs you into %master.name%!
    halt
  end
end
%morph% %self% %select%
%echo% %prev_name% morphs into %self.name%!
~
#2916
Heroic Caravan Setup~
5 n 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft(room)%)
  * add cooking
  %door% %inter% aft add 2915
  set next %inter.aft(room)%
  if (%next% && !%next.aft(room)%)
    * add bedroom
    %door% %next% aft add 2914
    set next %next.aft(room)%
    if (%next% && !%next.aft(room)%)
      * add smithy
      %door% %next% aft add 2913
    end
  end
end
detach 2916 %self.id%
~
#2917
Heroic Galleon Setup~
5 n 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.fore(room)%)
  * add forecastle
  %door% %inter% fore add 5504
end
if (!%inter.aft(room)%)
  * add cabin
  %door% %inter% aft add 5516
end
if (!%inter.down(room)%)
  * add below deck
  %door% %inter% down add 5503
  set hold %inter.down(room)%
  if (%hold% && !%hold.aft(room)%)
    * add mail
    %door% %hold% aft add 2911
  end
  if (%hold% && !%hold.fore(room)%)
    * add mail
    %door% %hold% fore add 2912
  end
end
detach 2917 %self.id%
~
#2926
Debuff Cleansing Potion~
1 s 100
~
%heal% %actor% debuffs
%heal% %actor% dots
~
$
