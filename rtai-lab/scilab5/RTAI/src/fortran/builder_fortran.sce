// Created by toolbox_creator
// ffiles and ofiles may be also be manually defined
// see SCI/contrib/toolbox_skeleton/src/fortran/builder_fortran.sce
files=dir('*.f');
ffiles=strsubst(files.name,'.f','');
ofiles=strsubst(files.name,'.f','.o');
tbx_build_src(ffiles, ofiles, 'f',get_absolute_file_path('builder_fortran.sce'));
clear tbx_build_src, files, ffiles, ofiles;

