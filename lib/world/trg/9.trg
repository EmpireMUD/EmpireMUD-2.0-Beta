#921
Caravan setup~
5 n 100
~
eval inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  %door% %inter% aft add 5518
end
detach 921 %self.id%
~
#952
Caravel setup~
5 n 100
~
eval inter %self.interior%
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
eval inter %self.interior%
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
eval inter %self.interior%
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
eval inter %self.interior%
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
eval inter %self.interior%
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
eval inter %self.interior%
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
