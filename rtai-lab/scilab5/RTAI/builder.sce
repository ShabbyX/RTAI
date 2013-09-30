mode(-1);
lines(0);
// ====================================================================
// Created using toolbox_creator()
// ====================================================================
try
 getversion('scilab');
catch
 error(gettext('Scilab 5.0 or more is required.'));  
end;
// ====================================================================
if ~with_module('development_tools') then
  error(msprintf(gettext('%s module not installed."),'development_tools'));
end
// ====================================================================
TOOLBOX_NAME = 'RTAI';
TOOLBOX_TITLE = 'RTAI';
// ====================================================================
toolbox_dir = get_absolute_file_path('builder.sce');

tbx_builder_macros(toolbox_dir);
tbx_builder_src(toolbox_dir);
tbx_build_loader(TOOLBOX_NAME, toolbox_dir);

loadScicosLibs();

blklib = lib(toolbox_dir + 'macros/');
xpal = xcosPal("RTAI");
blks = list("C_RTAI_BLK", "rtai4_comedi_datain", "rtai4_comedi_dataout", "rtai4_comedi_dioin", "rtai4_comedi_dioout","rtai4_comedi_encoder", "rtai4_extdata", "rtai4_led", "rtai4_log", "rtai4_mbx_ovrwr_send", "rtai4_mbx_rcv", "rtai4_mbx_rcv_if", "rtai4_mbx_send_if", "rtai4_meter", "rtai4_scope", "rtai4_sem_signal", "rtai4_sem_wait", "rtai4_sinus", "rtai4_square", "rtai4_step", "rtai_ext_clock","rtai_fifoin", "rtai_fifoout", "rtai_generic_inp", "rtai_generic_out");
// Add blocks to the palette
for blkname = blks
    scs_m = evstr(blkname+"(""define"")");
    export_to_hdf5(toolbox_dir + 'macros/h5/'+blkname+".h5", "scs_m");
    clear scs_m;
    blkstyle = struct();
    blkstyle.image = "file:"+toolbox_dir + 'macros/svg_icons/'+blkname+".svg";
    xpal = xcosPalAddBlock(xpal, toolbox_dir + 'macros/h5/'+blkname+".h5", toolbox_dir + 'macros/gif_icons/'+blkname+".gif", blkstyle);
end
// Save the palette
xcosPalExport(xpal, toolbox_dir + "macros/RTAI.xpal");


clear toolbox_dir TOOLBOX_NAME TOOLBOX_TITLE;
// ====================================================================
