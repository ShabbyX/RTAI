// Created by toolbox_creator
// cfiles and ofiles may be also be manually defined
// see SCI/contrib/toolbox_skeleton/src/c/builder_c.sce
//files=dir('*.c');
names=['rtsinus','rtsquare','rt_step']
files=[ 'rtai_sinus.c', 'rtai_square.c', 'rtai_step.c'];
src_c_path = get_absolute_file_path("builder_c.sce");

cflags    = '';
//source version
if isdir(SCI+"/modules/core/includes/") then
 cflags = cflags + " -I" + SCI + "/modules/scicos/includes/" + " -I" + SCI + "/modules/scicos_blocks/includes/";
end

// Binary version
if isdir(SCI+"/../../include/scilab/") then
 cflags = cflags + " -I" + SCI + "/../../include/scilab/scicos/" + " -I" + SCI + "/../../include/scilab/scicos_blocks/";
end

//ofiles=strsubst(files.name,'.c','.o');
tbx_build_src(names, files, 'c',src_c_path,"","",cflags);
//tbx_build_src(names, files, 'c', src_c_path ,libs,ldflags,cflags,fflags,cc,libname);
clear tbx_build_src, files, names, cflags, src_c_path;

