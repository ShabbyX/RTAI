% *******
% setup.m
% *******
%
% Don't forget to copy ptinfo.tlc to [matlabroot]\rtw\c\tlc\mw if you're
% using Matlab 7.0 or newer.


if ispc, devpath='\rtw\c\rtai\devices';
else devpath='/rtw/c/rtai/devices';
end

devices = [matlabroot, devpath];
addpath(devices);
savepath;

cdir=cd;
cd(devices);

sfuns = dir('*.c')
for cnt = 1:length(sfuns)
    eval(['mex ' sfuns(cnt).name])
end

cd(cdir);

% ****************
