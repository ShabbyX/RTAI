// Created by toolbox_creator
// cfiles and ofiles may be also be manually defined
// see SCI/contrib/toolbox_skeleton/src/c/builder_c.sce
files=dir('*.c');
cfiles=['rtsinus';'rtsquare';'rt_step']
ofiles=strsubst(files.name,'.c','.o');
tbx_build_src(cfiles, ofiles, 'c',get_absolute_file_path('builder_c.sce'),"","","-I"+get_absolute_file_path('builder_c.sce'));
clear tbx_build_src, files, cfiles, ofiles;

