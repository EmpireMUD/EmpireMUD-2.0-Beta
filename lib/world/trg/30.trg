#3054
Calabash drying timer~
1 f 0
~
if %self.carried_by%
  %send% %self.carried_by% %self.shortdesc% dries out.
  %load% obj 3054 %self.carried_by% inv
  if %random.3% == 3
    %load% obj 3055 %self.carried_by% inv
  end
else
  %echo% %self.shortdesc% dries out.
  %load% obj 3054
  if %random.3% == 3
    %load% obj 3055
  end
end
~
$
