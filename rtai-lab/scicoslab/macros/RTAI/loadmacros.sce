pathL=get_absolute_file_path('loadmacros.sce');

%scicos_menu($+1)=['RTAI','RTAI CodeGen','Set Target'];
scicos_pal($+1,:)=['RTAI-Lib','SCI/contrib/RTAI/macros/RTAI-Lib.cosf'];
%CmenuTypeOneVector($+1,:)=['RTAI CodeGen','Click on a Superblock (without activation output) to obtain a coded block!'];
%CmenuTypeOneVector($+1,:)=['Set Target','Click on a Superblock (without activation output) to set the Target!'];
%scicos_gif($+1)=[ pathL + 'gif_icons/' ];
