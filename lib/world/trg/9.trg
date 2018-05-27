#921
Caravan setup~
5 n 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 5518
end
detach 921 %self.id%
~
#929
bloodletter arrows~
1 s 25
~
set scale 100
%send% %actor% %target.name% begins to bleed from the wound left by your bloodletter arrow!
%send% %target% You begin to bleed from the wound left by %actor.name%'s bloodletter arrow!
%echoneither% %target% %actor% %target.name% begins to bleed from the wound left by %actor.name%'s bloodletter arrow!
%dot% %target% %scale% 15 physical
~
#952
Caravel setup~
5 n 100
~
set inter %self.interior%
if (!%inter% || %inter.aft%)
  halt
end
%door% %inter% aft add 5500
detach 952 %self.id%
~
#953
Cog setup~
5 n 100
~
set inter %self.interior%
if (!%inter% || %inter.down%)
  halt
end
%door% %inter% down add 5502
detach 953 %self.id%
~
#954
Longship setup~
5 n 100
~
set inter %self.interior%
if (!%inter% || %inter.aft%)
  halt
end
%door% %inter% aft add 5500
detach 954 %self.id%
~
#955
Brigantine setup~
5 n 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 5512
end
if (!%inter.fore%)
  %door% %inter% fore add 5506
end
detach 955 %self.id%
~
#956
Carrack setup~
5 n 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 5513
end
if (!%inter.fore%)
  %door% %inter% fore add 5506
end
detach 956 %self.id%
~
#957
Hulk setup~
5 n 100
~
set inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 5500
end
if (!%inter.down%)
  %door% %inter% down add 5502
end
detach 957 %self.id%
~
$
