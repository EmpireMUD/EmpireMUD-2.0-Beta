#1100
Shimmering anvil: Lasts 1 hour~
5 ab 10
~
* requires 9681 to set load_time
if %timestamp% - %self.var(load_time)% >= 3600
  %echo% %self.shortdesc% dissipates into a cloud of dust.
  %purge% %self%
end
~
#1128
Lost Book: Replace on load~
1 n 100
~
wait 1
if %self.carried_by%
  %load% book lost %self.carried_by%
elseif %self.in_obj%
  %load% book lost %self.in_obj%
else
  %load% book lost
end
%purge% %self%
~
$
