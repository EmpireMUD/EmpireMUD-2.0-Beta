#921
Caravan setup~
5 n 100
~
eval inter %self.interior%
if (!%inter%)
  halt
end
if (!%inter.aft%)
  nop %inter.aft(create,5518)%
end
~
#952
Caravel setup~
5 n 100
~
eval inter %self.interior%
if (!%inter% || %inter.aft%)
  halt
end
nop %inter.aft(create,5500)%
~
#953
Cog setup~
5 n 100
~
eval inter %self.interior%
if (!%inter% || %inter.down%)
  halt
end
nop %inter.down(create,5502)%
~
#954
Longship setup~
5 n 100
~
eval inter %self.interior%
if (!%inter% || %inter.aft%)
  halt
end
nop %inter.aft(create,5500)%
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
  nop %inter.aft(create,5512)%
end
if (!%inter.fore%)
  nop %inter.fore(create,5506)%
end
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
  nop %inter.aft(create,5513)%
end
if (!%inter.fore%)
  nop %inter.fore(create,5506)%
end
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
  nop %inter.aft(create,5500)%
end
if (!%inter.down%)
  nop %inter.down(create,5502)%
end
~
$
