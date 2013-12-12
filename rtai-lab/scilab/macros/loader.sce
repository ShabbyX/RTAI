mode(-1);
actDIR = pwd();

// specific part
libname='rtai' // name of scilab function library [CUSTOM]
DIR = get_absolute_file_path('loader.sce');
chdir(DIR)

// You should also modify the  ROUTINES/loader.sce with the new 
// Scilab primitive for the path

rtailib = lib(DIR+'/macros/')
exec('SCI/contrib/RTAI/routines/loader.sce');

chdir('macros');
exec('loadmacros.sce');
chdir(actDIR);

disp("Scicos-RTAI Ready");

